#pragma once

#if defined(_WIN32)
#include <curses.h>
#else
#include <ncurses.h>
#endif

#include <cstddef>
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
    LineEditor() = default;
    ~LineEditor() = default;
    LineEditor(const LineEditor&) = delete;
    LineEditor& operator=(const LineEditor&) = delete;

    void start(std::string prompt, std::string text = {});
    void cancel();
    LineEditorEvent handle_key(int key);
    void render(WINDOW* window, int width);

    bool active() const { return active_; }
    const std::string& prompt() const { return prompt_; }
    const std::string& text() const { return text_; }
    std::size_t history_size() const { return history_.size(); }

private:
    void insert_bytes(const char* data, std::size_t n);
    void backspace_char();
    void delete_char();
    void cursor_left();
    void cursor_right();
    void clear_line();
    void commit_history();
    void history_previous();
    void history_next();
    void reset_pending_utf8();
    bool feed_pending_utf8(int key);
    static std::size_t utf8_char_width_at(std::string_view text, std::size_t byte_offset);

    bool active_ = false;
    std::string prompt_;
    std::string text_;
    std::size_t cursor_ = 0;
    std::vector<std::string> history_;
    std::size_t history_index_ = 0;

    // Multi-byte UTF-8 sequence being assembled from successive getch() bytes.
    std::string pending_utf8_;
    int pending_utf8_expected_ = 0;
};

} // namespace lv::ui
