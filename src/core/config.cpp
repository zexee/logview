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

// Read all key-value pairs. Keys currently recognised: font_size, font_path.
static std::pair<int, std::string> load_config() {
    const auto path = config_path();
    if (path.empty()) return {0, {}};
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return {0, {}};

    std::ifstream in(path);
    if (!in) return {0, {}};

    int font_size = 0;
    std::string font_path;
    std::string line;
    while (std::getline(in, line)) {
        if (line.find("font_size:") == 0) {
            const auto val = line.substr(10);
            const auto start = val.find_first_not_of(" \t");
            if (start != std::string::npos) {
                font_size = std::atoi(val.c_str() + start);
            }
        } else if (line.find("font_path:") == 0) {
            auto val = line.substr(10);
            const auto start = val.find_first_not_of(" \t");
            if (start != std::string::npos) {
                font_path = val.substr(start);
                // Trim trailing whitespace / newline
                while (!font_path.empty() && std::isspace(
                    static_cast<unsigned char>(font_path.back()))) {
                    font_path.pop_back();
                }
                // Remove surrounding quotes if present.
                if (font_path.size() >= 2) {
                    const char f = font_path.front();
                    if ((f == '"' || f == '\'') && font_path.back() == f) {
                        font_path = font_path.substr(1, font_path.size() - 2);
                    }
                }
            }
        }
    }
    return {font_size, font_path};
}

int load_font_size() {
    return load_config().first;
}

std::string load_font_path() {
    return load_config().second;
}

void save_config(int font_size, const std::string& font_path) {
    const auto path = config_path();
    if (path.empty()) return;
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    // Preserve any existing keys we don't manage.
    std::string existing;
    {
        std::ifstream in(path);
        if (in) {
            std::string line;
            while (std::getline(in, line)) {
                if (line.find("font_size:") != 0 && line.find("font_path:") != 0) {
                    existing += line + "\n";
                }
            }
        }
    }

    std::ofstream out(path);
    if (!out) return;
    out << "font_size: " << font_size << '\n';
    if (!font_path.empty()) {
        out << "font_path: " << font_path << '\n';
    }
    if (!existing.empty()) {
        out << existing;
    }
}

} // namespace lv
