#include "mini_ipc/param_manager.hpp"

#include <cstring>
#include <unistd.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <string>
#include <iostream>
#include <unordered_map>

struct TopicEntry {
    std::string port;
    bool port_avilable = false;
    int publisher_fd;
    std::vector<int> waiting_fds;
};

std::pair<std::string, std::string> parse_pub(const std::string& msg) {
    size_t space1 = msg.find(' ', 4);
    std::string topic = msg.substr(4, space1 - 4);
    std::string port = msg.substr(space1 + 1);

    return std::pair<std::string, std::string>{topic, port};
}

int main(int argc, char const* argv[]) {
    // 参数管理器
    auto& params = mini_ipc::ParamManager::instance();
    if (!params.load("core/config/comm.yaml")) {
        std::cerr << "Failed to load config, using defaults.\n";
    }

    int server_fd;
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 地址复用(快速重启)
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    int port = params.get<int>("discovery_daemon.port", 8888);

    struct sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    socklen_t server_addr_len = sizeof(server_addr);

    int backlog = params.get<int>("discovery_daemon.backlog", 128);

    bind(server_fd, (sockaddr*)&server_addr, server_addr_len);
    listen(server_fd, backlog);

    std::cout << "[Discovery] Daemon running on port " << port << "..." << "\n";

    // topic --> port
    std::unordered_map<std::string, TopicEntry> topic_registry;
    // fd --> publisher fd
    std::unordered_map<int, std::string> publisher_fds;

    int epoll_fd_discovery = epoll_create1(0);
    struct epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    epoll_ctl(epoll_fd_discovery, EPOLL_CTL_ADD, server_fd, &ev);

    struct epoll_event events[64];
    while (true) {
        int event_num = epoll_wait(epoll_fd_discovery, events, 64, -1);
        for (int i = 0; i < event_num; ++i) {
            int active_fd = events[i].data.fd;

            // 新连接
            if (active_fd == server_fd) {
                int client_fd = accept(server_fd, nullptr, nullptr);
                struct epoll_event client_ev{};
                client_ev.events = EPOLLIN;
                client_ev.data.fd = client_fd;
                epoll_ctl(epoll_fd_discovery, EPOLL_CTL_ADD, client_fd, &client_ev);
                continue;
            }

            // 已有连接有数据
            char buffer[256] = {};
            ssize_t read_bytes = read(active_fd, buffer, sizeof(buffer));
            // epoll管理的fd存在错误或掉线, TODO:当前仅考虑掉线情况
            if (read_bytes <= 0) {
                // 首先判断是不是publisher
                auto it_fd = publisher_fds.find(active_fd);
                if (it_fd != publisher_fds.end()) {
                    topic_registry[it_fd->second].port_avilable = false;
                    topic_registry[it_fd->second].publisher_fd = -1;
                    epoll_ctl(epoll_fd_discovery, EPOLL_CTL_DEL, it_fd->first, nullptr);
                    close(it_fd->first);
                    publisher_fds.erase(it_fd);

                    continue;
                }
                // 判断是否为waitting_fds, 所有遍历完不是waitting_fd那就subscriber
                for (auto& [topic, entry] : topic_registry) {
                    auto& waiting_fds = entry.waiting_fds;
                    auto it_fd = std::find(waiting_fds.begin(), waiting_fds.end(), active_fd);
                    if (it_fd != waiting_fds.end()) {
                        waiting_fds.erase(it_fd);
                        break;
                    }
                }
                epoll_ctl(epoll_fd_discovery, EPOLL_CTL_DEL, active_fd, nullptr);
                close(active_fd);
                continue;
            }

            // 消息分发, 注册或查询
            std::string msg(buffer, read_bytes);
            // 注册
            if (std::strcmp(msg.substr(0, 3).c_str(), "PUB") == 0) {
                auto [topic, port] = parse_pub(msg);
                auto& entry = topic_registry[topic];

                // 已有正常运行的publisher
                // TODO: publisher做对应处理
                if (entry.port_avilable) {
                    write(active_fd, "REJECT", 6);
                    epoll_ctl(epoll_fd_discovery, EPOLL_CTL_DEL, active_fd, nullptr);
                    close(active_fd);

                    continue;
                }

                // 存在publisher但是状态异常, 崩溃遗留
                if (entry.publisher_fd > 0) {
                    epoll_ctl(epoll_fd_discovery, EPOLL_CTL_DEL, entry.publisher_fd, nullptr);
                    close(entry.publisher_fd);
                    publisher_fds.erase(entry.publisher_fd);
                }

                entry.port = port;
                entry.port_avilable = true;
                entry.publisher_fd = active_fd;
                publisher_fds[active_fd] = topic;

                // 通知在等待该话题的订阅者, 话题的发布者已上线
                std::string notify_msg = "PORT " + port;
                for (const auto& w_fd : entry.waiting_fds) {
                    write(w_fd, notify_msg.c_str(), notify_msg.size());
                    epoll_ctl(epoll_fd_discovery, EPOLL_CTL_DEL, w_fd, nullptr);
                    close(w_fd);
                }
                entry.waiting_fds.clear();

                // 向publisher回复ok, 表示注册完成
                write(active_fd, "OK", 2);

                struct epoll_event ev{};
                ev.data.fd = active_fd;
                ev.events = EPOLLIN;
                epoll_ctl(epoll_fd_discovery, EPOLL_CTL_ADD, active_fd, &ev);
            } else if (std::strcmp(msg.substr(0, 3).c_str(), "SUB") == 0) {
                std::string topic = msg.substr(4);
                auto it = topic_registry.find(topic);
                // 如果topic的发布者已经上线，则直接回复对应端口号
                if (it != topic_registry.end() && !it->second.port.empty() && it->second.port_avilable) {
                    write(active_fd, it->second.port.c_str(), it->second.port.size());
                    epoll_ctl(epoll_fd_discovery, EPOLL_CTL_DEL, active_fd, nullptr);
                    close(active_fd);
                } else {
                    // 向subscriber回复WAITING, 发布者未上线, 加入等待
                    topic_registry[topic].waiting_fds.push_back(active_fd);
                    write(active_fd, "WAITING", 7);
                }
            }
        }
    }

    return 0;
}
