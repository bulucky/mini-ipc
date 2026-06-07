#include "mini_ipc/bridge_dispatcher.hpp"
#include "mini_ipc/websocket_session.hpp"

#include <boost/asio/post.hpp>
#include "nlohmann/json.hpp"

#include <string>
#include <iostream>

namespace mini_ipc {
using json = nlohmann::json;

BridgeDispatcher::BridgeDispatcher(Node& node, boost::asio::io_context& ioc)
    : node_(node), ioc_(ioc) {}

std::string BridgeDispatcher::handle_message(const std::string& text,
                                             WebSocketSession* session,
                                             std::weak_ptr<WebSocketSession> session_weak_ptr) { // NOLINT
    try {
        const auto request = json::parse(text);
        const auto type = request.value("type", std::string());

        if (type == "publish") {
            return handle_publish(
                request.value("topic", std::string()),
                request.value("payload", std::string()));
        }

        if (type == "subscribe") {
            return handle_subscribe(request.value("topic", std::string{}), session, session_weak_ptr); // NOLINT
        }

        return make_status("error", "Unsupported command type: " + type);
    } catch (const json::exception& e) {
        return make_status("error", "Invalid JSON request");
    }
}

std::string BridgeDispatcher::handle_publish(const std::string& topic,
                                             const std::string& payload) {
    if (topic.empty()) {
        return make_status("error", "Publish failed: topic is empty");
    }

    Publisher& publisher = get_or_create_publisher(topic);
    publisher.publish(payload);

    std::cout << "[BridgeDispatcher] publish topic=" << topic
              << " payload=" << payload << "\n";

    return make_status("info", "Published to " + topic);
}

Publisher& BridgeDispatcher::get_or_create_publisher(const std::string& topic) {
    auto pub_it = publishers_.find(topic);
    if (pub_it != publishers_.end()) {
        return pub_it->second;
    }

    auto [inserted_it_pub, inserted] =
        publishers_.emplace(topic, node_.create_publisher(topic));

    return inserted_it_pub->second;
}

std::string BridgeDispatcher::handle_subscribe(const std::string& topic,
                                               WebSocketSession* session,
                                               std::weak_ptr<WebSocketSession> session_weak_ptr) { // NOLINT
    if (topic.empty()) {
        return make_status("error", "Subscribe failed: topic is empty");
    }

    if (subscribers_.find(session) != subscribers_.end()) {
        subscribers_.erase(session);
    }

    auto subscriber = node_.create_subscriber(
        topic,
        [this, session_weak_ptr, topic](const std::string& msg) {
            boost::asio::post(ioc_, [topic, session_weak_ptr, msg]() {
                if (auto session_shared_ptr = session_weak_ptr.lock()) {
                    session_shared_ptr->send_message(make_message(topic, msg));
                }
            });
        });

    subscribers_.emplace(session, std::move(subscriber));
    return make_status("info", "Subscribe command received for " + topic);
}

std::string BridgeDispatcher::make_status(const std::string& level,
                                          const std::string& message) {
    return json{
        {"type", "status"},
        {"level", level},
        {"message", message}}
        .dump();
}

std::string BridgeDispatcher::make_message(const std::string& topic,
                                           const std::string& payload) {
    return json{
        {"type", "message"},
        {"topic", topic},
        {"payload", payload}}
        .dump();
}

} // namespace mini_ipc