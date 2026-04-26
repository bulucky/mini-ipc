#include "mini_ipc/param_manager.hpp"

#include <sstream>

namespace mini_ipc {

ParamManager& ParamManager::instance() {
    static ParamManager manager;
    return manager;
}

bool ParamManager::load(const std::string& config_path) {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        root_ = YAML::LoadFile(config_path);
        return true;
    } catch (const YAML::Exception&) {
        root_ = YAML::Node{};
        return false;
    }
}

bool ParamManager::has(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    YAML::Node node = find_node(key);
    return node && !node.IsNull();
}

YAML::Node ParamManager::find_node(const std::string& key) const {
    YAML::Node current = root_;

    std::stringstream ss{key};
    std::string part;

    while (std::getline(ss, part, '.')) {
        if (!current || !current[part]) {
            return YAML::Node{};
        }
        current = current[part];
    }

    return current;
}
} // namespace mini_ipc
