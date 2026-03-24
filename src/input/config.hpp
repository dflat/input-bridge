#pragma once
#include <string>
#include <vector>

namespace config {
    struct AppConfig {
        std::vector<std::string> allowed_devices; // Exact names of devices to grab. If empty, grab all compatible.
    };

    AppConfig load_config(const std::string& path = "/etc/input-bridge.conf");
}