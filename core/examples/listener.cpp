#include "mini_ipc/node.hpp"

int main(int argc, char const* argv[]) {
    auto node =
        std::make_shared<mini_ipc::Node>("test_node");

    auto pub = node->create_publisher("test_topic");

    // while (true) {
    //     pub.publish("test_msg");
    // }

    return 0;
}