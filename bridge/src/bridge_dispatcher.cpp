#include "mini_ipc/bridge_dispatcher.hpp"

#include <iostream>

namespace mini_ipc {
BridgeDispatcher::BridgeDispatcher(Node& node)
    : node_(node) {}

std::string BridgeDispatcher::handle_message(const std::string& text) {
    const std::string type = extract_string_field(text, "type");

    if (type == "publish") {
        const std::string topic = extract_string_field(text, "topic");
        const std::string payload = extract_string_field(text, "payload");

        return handle_publish(topic, payload);
    } else if (type == "subscribe") {
        const std::string topic = extract_string_field(text, "topic");

        return handle_subscribe(topic);
    }

    return make_status("error", "Unsupported command type: " + type);
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

std::string BridgeDispatcher::handle_subscribe(const std::string& topic) {
    if (topic.empty()) {
        return make_status("error", "Subscribe failed: topic is empty");
    }

    return make_status("info", "Subscribe command received for " + topic);
}

std::string BridgeDispatcher::extract_string_field(const std::string& json,
                                                   const std::string& key) {
    const std::string pattern = "\"" + key + "\"";
    const std::size_t key_pos = json.find(pattern);
    if (key_pos == std::string::npos) {
        return "";
    }

    const std::size_t colon_pos = json.find(':', key_pos + pattern.size());
    if (colon_pos == std::string::npos) {
        return "";
    }

    const std::size_t first_quote = json.find('"', colon_pos + 1);
    if (first_quote == std::string::npos) {
        return "";
    }

    const std::size_t second_quote = json.find('"', first_quote + 1);
    if (second_quote == std::string::npos) {
        return "";
    }

    return json.substr(first_quote + 1, second_quote - first_quote - 1);
}

std::string BridgeDispatcher::make_status(const std::string& level,
                                          const std::string& message) {
    return std::string{R"({"type":"status","level":")"} +
           escape_json(level) +
           R"(","message":")" +
           escape_json(message) +
           "\"}";
}

std::string BridgeDispatcher::make_message(const std::string& topic,
                                           const std::string& payload) {
    return std::string{R"({"type":"message","topic":")"} +
           escape_json(topic) +
           R"(","payload":")" +
           escape_json(payload) +
           "\"}";
}

std::string
BridgeDispatcher::escape_json(const std::string& text) {
    std::string escaped;
    escaped.reserve(text.size());

    for (const auto ch : text) {
        switch (ch) {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped += ch;
                break;
        }
    }

    return escaped;
}
} // namespace mini_ipc