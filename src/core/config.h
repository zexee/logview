#pragma once

namespace lv {

// Read font_size from ~/.config/lv/config.yaml. Returns 0 if the file
// doesn't exist or the key is missing, signalling "use the default".
int load_font_size();

// Write font_size to ~/.config/lv/config.yaml. Creates the directory
// if it doesn't exist.
void save_font_size(int size);

} // namespace lv
