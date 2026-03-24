#include "config.hpp"
#include <fstream>
#include <sstream>

namespace config {

AppConfig load_config(const std::string& path) {
    AppConfig cfg;
    std::ifstream file(path);
    if (!file.is_open()) {
        return cfg; // Return empty config (grab all) if file doesn't exist
    }

    std::string line;
    while (std::getline(file, line)) {
        // Basic trim and comment ignoring
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        
        // Trim leading/trailing whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (!line.empty()) {
            cfg.allowed_devices.push_back(line);
        }
    }

    return cfg;
}

}