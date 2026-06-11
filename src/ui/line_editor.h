#pragma once

#include <form.h>
#include <ncurses.h>

#include <string>
#include <vector>

namespace lv::ui {

enum class LineEditorEvent {
    None,
    Submitted,
    Canceled,
};

class LineEditor {
public:
    ~LineEditor();

    void start(std::string prompt, std::string text = {});
    void cancel();
    LineEditorEvent handle_key(int key);
    void render(WINDOW* window, int width);

    bool active() const { return active_; }
    const std::string& prompt() const { return prompt_; }
    const std::string& text() const { return text_; }
    std::size_t history_size() const { return history_.size(); }

private:
    void ensure_form(WINDOW* window, int width);
    void destroy_form();
    void set_field_text(const std::string& text);
    void sync_text_from_field();
    void sync_form_cursor();
    void fallback_insert(char ch);
    void fallback_backspace();
    void fallback_delete();
    void commit_history();
    void history_previous();
    void history_next();
    static std::string trim_field_buffer(const char* buffer, std::size_t cursor_pos);

    bool active_ = false;
    std::string prompt_;
    std::string text_;
    std::size_t cursor_ = 0;
    std::vector<std::string> history_;
    std::size_t history_index_ = 0;
    FIELD* fields_[2] = {nullptr, nullptr};
    FORM* form_ = nullptr;
    WINDOW* bound_window_ = nullptr;
    WINDOW* field_window_ = nullptr;
    int bound_width_ = 0;
    int bound_prompt_width_ = 0;
    int field_width_ = 0;
};

} // namespace lv::ui
