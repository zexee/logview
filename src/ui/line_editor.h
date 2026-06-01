#pragma once

#include <ncurses.h>

#include <string>

namespace lv::ui {

enum class LineEditorEvent {
    None,
    Submitted,
    Canceled,
};

class LineEditor {
public:
    void start(std::string prompt, std::string text = {});
    void cancel();
    LineEditorEvent handle_key(int key);
    void render(WINDOW* window, int width) const;

    bool active() const { return active_; }
    const std::string& prompt() const { return prompt_; }
    const std::string& text() const { return text_; }

private:
    void insert_char(char ch);
    void backspace();
    void delete_char();

    bool active_ = false;
    std::string prompt_;
    std::string text_;
    std::size_t cursor_ = 0;
};

} // namespace lv::ui
