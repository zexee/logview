#pragma once

#include <filesystem>
#include <string>

namespace lv {

// Expand a leading `~` to the user's home directory.
// Returns the path as-is if no expansion applies.
//
// On Linux reads $HOME; on Windows reads %USERPROFILE% (falling back to
// %HOMEDRIVE%%HOMEPATH%). Input and the returned path are both UTF-8.
std::filesystem::path expand_path(const std::string& path);

// Render a filesystem path as a UTF-8 std::string using POSIX-style separators.
// Bridges the C++20 std::u8string returned by path::generic_u8string() back to
// the std::string / std::string_view types the rest of the codebase uses.
std::string to_utf8(const std::filesystem::path& p);

} // namespace lv
