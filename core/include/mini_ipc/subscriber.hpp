#pragma once
#include <string>
#include <memory>
#include <functional>

namespace mini_ipc {
class Subscriber {
public:
    using CallbackType = std::function<void(const std::string&)>;

    Subscriber() = default;
    ~Subscriber() = default;

private:
    friend class Node;

    class Impl;
    std::shared_ptr<Impl> pimpl_;

    explicit Subscriber(std::shared_ptr<Impl> impl);
};
} // namespace mini_ipc