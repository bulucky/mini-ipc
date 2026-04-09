#pragma once

#include <string>
#include <memory>

namespace mini_ipc {

class Publisher {
public:
    Publisher() = default;
    ~Publisher() = default;

    void publish(const std::string& msg);

private:
    friend class Node;

    class Impl;
    std::shared_ptr<Impl> pimpl_;

    explicit Publisher(std::shared_ptr<Impl> impl);
};
} // namespace mini_ipc