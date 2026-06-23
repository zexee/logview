#pragma once

#include <string>

namespace lv {

// Read font_size from ~/.config/lv/config.yaml. Returns 0 if the file
// doesn't exist or the key is missing, signalling "use the default".
int load_font_size();

// Read font_path from the config file. Returns empty string if not set.
std::string load_font_path();

// Write font_size and optional font_path to the config file.
// font_path may be empty to keep the current value (or erase it).
void save_config(int font_size, const std::string& font_path = {});

} // namespace lv
