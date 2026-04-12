#include "mini_ipc/node.hpp"

#include <iostream>

int main(int argc, char const* argv[]) {
    auto node =
        std::make_shared<mini_ipc::Node>("test_node");

    mini_ipc::Subscriber::CallbackType sub_callback =
        [](const std::string& msg) {
            std::cout << "Receive:" << msg << "\n";
        };
    auto sub = node->create_subscriber("test_topic", sub_callback);

    node->spin();

    // while (true) {
    //     pub.publish("test_msg");
    // }

    return 0;
}