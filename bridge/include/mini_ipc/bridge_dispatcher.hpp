#pragma once

#include <string>

namespace mini_ipc {

class BridgeDispatcher {
public:
    /**
     *@brief: 处理请求消息
     */
    std::string handle_message(const std::string& text);

private:
    /**
     *@brief: 转义json
     */
    static std::string escape_json(const std::string& test);
};
} // namespace mini_ipc