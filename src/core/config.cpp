#include "core/config.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace lv {

static std::filesystem::path config_path() {
    const char* config_home = std::getenv("XDG_CONFIG_HOME");
    if (config_home != nullptr && *config_home) {
        return std::filesystem::path(config_home) / "lv" / "config.yaml";
    }
    const char* home = std::getenv("HOME");
    if (home != nullptr) {
        return std::filesystem::path(home) / ".config" / "lv" / "config.yaml";
    }
    return {};
}

int load_font_size() {
    const auto path = config_path();
    if (path.empty()) return 0;
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return 0;

    std::ifstream in(path);
    if (!in) return 0;
    std::string line;
    while (std::getline(in, line)) {
        // Minimal YAML: "font_size: 18"
        const auto pos = line.find("font_size:");
        if (pos == 0) {
            const auto val = line.substr(10);  // "font_size:" is 10 chars
            const auto start = val.find_first_not_of(" \t");
            if (start != std::string::npos) {
                return std::atoi(val.c_str() + start);
            }
        }
    }
    return 0;
}

void save_font_size(int size) {
    const auto path = config_path();
    if (path.empty()) return;
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    std::ofstream out(path);
    if (!out) return;
    out << "font_size: " << size << '\n';
}

} // namespace lv
