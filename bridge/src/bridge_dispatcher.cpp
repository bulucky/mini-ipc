#include "mini_ipc/bridge_dispatcher.hpp"

namespace mini_ipc {
std::string BridgeDispatcher::handle_message(const std::string& text) {
    return std::string{R"({"type":"message","topic":"debug","payload":")"} +
           escape_json(text) +
           "\"}";
}

std::string BridgeDispatcher::escape_json(const std::string& text) {
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