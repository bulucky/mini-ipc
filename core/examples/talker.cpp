#include "mini_ipc/node.hpp"


int main(int argc, char const* argv[]) {
    auto node =
        std::make_shared<mini_ipc::Node>("test_node");

    mini_ipc::Subscriber::CallbackType sub_callback =
        [](const std::string& msg) {
            // std::cout << ""
        };
    auto sub = node->create_subscriber("test_topic", sub_callback);

    // while (true) {
    //     pub.publish("test_msg");
    // }

    return 0;
}