#pragma once

#include <mutex>
#include <string>
#include <yaml-cpp/yaml.h>

namespace mini_ipc {

class ParamManager {
public:
    ParamManager() = default;
    ParamManager(const ParamManager&) = delete;
    ParamManager& operator=(const ParamManager&) = delete;

    static ParamManager& instance();

    bool load(const std::string& config_path);

    template <typename T>
    T get(const std::string& key, const T& default_value) const {
        std::lock_guard<std::mutex> lock(mutex_);

        YAML::Node node = find_node(key);
        if (!node.IsDefined() || node.IsNull()) {
            return default_value;
        }

        try {
            return node.as<T>();
        } catch (const YAML::Exception&) {
            return default_value;
        }
    }

    bool has(const std::string& key) const;

private:
    [[nodiscard]] YAML::Node find_node(const std::string& key) const;

    YAML::Node root_;
    mutable std::mutex mutex_;
};

} // namespace mini_ipc