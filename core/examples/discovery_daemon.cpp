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
    std::vector<int> waiting_fds;
};

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

    int epoll_fd_discovery = epoll_create1(0);
    struct epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    epoll_ctl(epoll_fd_discovery, EPOLL_CTL_ADD, server_fd, &ev);

    struct epoll_event events[64];
    while (true) {
        int event_num = epoll_wait(epoll_fd_discovery, events, 64, -1);
        for (int i = 0; i < event_num; ++i) {
            int fd = events[i].data.fd;
            // 新连接
            if (fd == server_fd) {
                int client_fd = accept(server_fd, nullptr, nullptr);
                struct epoll_event client_ev{};
                client_ev.events = EPOLLIN;
                client_ev.data.fd = client_fd;
                epoll_ctl(epoll_fd_discovery, EPOLL_CTL_ADD, client_fd, &client_ev);
                continue;
            }

            // 已有连接有数据
            char buffer[256] = {};
            ssize_t read_bytes = read(fd, buffer, sizeof(buffer));
            if (read_bytes < 0) {
                // 断开连接，subscriber
                for (auto& [topic, entry] : topic_registry) {
                    auto& waiting_fds = entry.waiting_fds;
                    waiting_fds.erase(std::remove(waiting_fds.begin(), waiting_fds.end(), fd), waiting_fds.end());
                }
                epoll_ctl(epoll_fd_discovery, EPOLL_CTL_DEL, fd, nullptr);
                close(fd);
                continue;
            }
        }

        int client_fd;
        if ((client_fd = accept(server_fd, (sockaddr*)&server_addr, &server_addr_len)) == -1) {
            perror("accept");
            break;
        }

        char buffer[256] = {}; // NOLINT

        if (read(client_fd, buffer, sizeof(buffer)) == -1) {
            perror("read");
            close(client_fd);
            break;
        }

        std::string msg(buffer);

        if (msg.substr(0, 3) == "PUB") {
            // "PUB <topic> <port>"
            size_t space1 = msg.find(' ', 4);
            std::string topic = msg.substr(4, space1 - 4);
            std::string port = msg.substr(space1 + 1);

            topic_registry[topic].port = port;
            std::cout << "[Discovery] Registered: " << topic << " at port " << port << "\n";
        } else if (msg.substr(0, 3) == "SUB") {
            // "SUB <topic>"
            std::string topic = msg.substr(4);
            std::string port =
                topic_registry.count(topic) ? topic_registry[topic].port : "NOT_FOUND";

            if (write(client_fd, port.c_str(), port.length()) == -1) {
                perror("write");
                close(client_fd);
                break;
            }

            std::cout << "[Discovery] Queried: " << topic << " -> " << port << "\n";
        }

        close(client_fd);
    }

    return 0;
}