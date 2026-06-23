#include "gui/menu.h"

#include "ui/app_ui.h"

#include <imgui.h>

namespace lv::gui {

namespace {

void prefill(lv::ui::AppUi* app_ui, const std::string& cmd) {
    if (app_ui != nullptr) {
        app_ui->prefill_command(cmd);
    }
}

} // namespace

void render_main_menu(lv::ui::AppUi* app_ui) {
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open...", "Ctrl+O")) {
            prefill(app_ui, "open ");
        }
        if (ImGui::MenuItem("Save Filtered As...", "Ctrl+S")) {
            prefill(app_ui, "write ");
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Quit", "Ctrl+Q")) {
            prefill(app_ui, "quit");
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Rules")) {
        if (ImGui::MenuItem("Load...", "Ctrl+R")) {
            prefill(app_ui, "rules ");
        }
        if (ImGui::MenuItem("Save")) {
            prefill(app_ui, "write-rules");
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Search")) {
        if (ImGui::MenuItem("Find...", "/")) {
            if (app_ui != nullptr) {
                // Begin a forward search; the AppUi editor takes over from there.
                prefill(app_ui, "");
                // Note: '/' would normally trigger begin_search via handle_key.
                // We rely on the user typing the regex pattern directly.
            }
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("Show Help", "?")) {
            // Help is toggled by the '?' key in normal mode; prefilling ':'
            // would activate the command editor instead. Leave a hint.
            ImGui::TextDisabled("Press ? in normal mode");
        }
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

} // namespace lv::gui
