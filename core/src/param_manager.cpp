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
    std::vector<YAML::Node> nodes;
    nodes.push_back(root_);

    std::stringstream ss{key};
    std::string part;

    while (std::getline(ss, part, '.')) {
        const YAML::Node& current = nodes.back();

        if (!current.IsDefined() || current.IsNull() || !current.IsMap()) {
            return YAML::Node{};
        }

        YAML::Node next = current[part];

        if (!next.IsDefined() || next.IsNull()) {
            return YAML::Node{};
        }

        nodes.push_back(next);
    }

    return nodes.back();
}

} // namespace mini_ipc
