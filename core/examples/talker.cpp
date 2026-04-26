#include "mini_ipc/node.hpp"
#include "mini_ipc/param_manager.hpp"

#include <iostream>
#include <thread>
#include <chrono>

using namespace std::chrono_literals;

int main(int argc, char const* argv[]) {
    auto& params = mini_ipc::ParamManager::instance();

    if (!params.load("/home/hizaml/mini-ipc/core/config/comm.yaml")) {
        std::cerr << "Failed to load config, using defaults.\n";
    }


    auto node =
        std::make_shared<mini_ipc::Node>("test_node");

    auto pub = node->create_publisher("test_topic");

    std::thread spin_thread([&node]() {
        node->spin();
    });

    int count = 0;
    while (true) {
        std::string msg = "test_msg_" + std::to_string(count++);
        pub.publish(msg);
        std::cout << "Published: " << msg << "\n";
        std::this_thread::sleep_for(1s);
    }

    spin_thread.join();

    return 0;
}