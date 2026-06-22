#include "core/path_util.h"

#include <cstdlib>
#include <string>

namespace lv {

namespace {

std::string home_directory() {
#if defined(_WIN32)
    if (const char* userprofile = std::getenv("USERPROFILE"); userprofile != nullptr) {
        return std::string(userprofile);
    }
    if (const char* homedrive = std::getenv("HOMEDRIVE"); homedrive != nullptr) {
        if (const char* homepath = std::getenv("HOMEPATH"); homepath != nullptr) {
            return std::string(homedrive) + homepath;
        }
    }
    return {};
#else
    if (const char* home = std::getenv("HOME"); home != nullptr) {
        return std::string(home);
    }
    return {};
#endif
}

} // namespace

std::filesystem::path expand_path(const std::string& path) {
    if (path.empty() || path[0] != '~') {
        return std::filesystem::u8path(path);
    }
    if (path.size() == 1) {
        return std::filesystem::u8path(home_directory());
    }
    if (path[1] == '/' || path[1] == '\\') {
        return std::filesystem::u8path(home_directory() + path.substr(1));
    }
    // `~user` form is not supported; treat as literal.
    return std::filesystem::u8path(path);
}

std::string to_utf8(const std::filesystem::path& p) {
    const std::u8string s = p.generic_u8string();
    return std::string(reinterpret_cast<const char*>(s.data()), s.size());
}

} // namespace lv
