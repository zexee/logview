#pragma once

#include <string>

namespace lv { namespace ui { class AppUi; } }

namespace lv::gui {

// Render the main menu bar via Dear ImGui. Call this every frame between
// ImGui::NewFrame() and ImGui::Render(). Menu items trigger callbacks that
// prefill the AppUi editor with the corresponding command; the user still
// types the argument (file path, etc.) and presses Enter to execute.
//
// app_ui is captured as a pointer; pass nullptr to disable all menu items
// (e.g. before AppUi is fully constructed).
void render_main_menu(lv::ui::AppUi* app_ui);

} // namespace lv::gui
