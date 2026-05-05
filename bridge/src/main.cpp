#include "mini_ipc/node.hpp"
#include "mini_ipc/param_manager.hpp"
#include "mini_ipc/bridge_dispatcher.hpp"
#include "mini_ipc/websocket_server.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>

#include <thread>
#include <iostream>
#include <exception>

int main(int argc, char const* argv[]) {
    try {
        mini_ipc::ParamManager::instance().load("core/config/comm.yaml");
        mini_ipc::Node node("dashboard_bridge");

        std::thread ipc_thread([&node]() {
            node.spin();
        });
        ipc_thread.detach();

        boost::asio::io_context ioc;
        mini_ipc::BridgeDispatcher dispatcher(node);
        mini_ipc::WebSocketServer server(
            ioc,
            boost::asio::ip::tcp::endpoint{
                boost::asio::ip::address::from_string("127.0.0.1"),
                9000},
            dispatcher);

        server.run();
        std::cout << "[Bridge] running on ws://127.0.0.1:9000\n";

        ioc.run();

    } catch (const std::exception& e) {
        std::cerr << "[Bridge] fatal error: "
                  << e.what() << "\n";
        return 1;
    }
}
