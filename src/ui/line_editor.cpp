#include "ui/line_editor.h"

#include <algorithm>
#include <cctype>

namespace lv::ui {

void LineEditor::start(std::string prompt, std::string text) {
    active_ = true;
    prompt_ = std::move(prompt);
    text_ = std::move(text);
    cursor_ = text_.size();
}

void LineEditor::cancel() {
    active_ = false;
    prompt_.clear();
    text_.clear();
    cursor_ = 0;
}

LineEditorEvent LineEditor::handle_key(int key) {
    if (!active_) {
        return LineEditorEvent::None;
    }

    switch (key) {
    case 10:
    case KEY_ENTER:
        active_ = false;
        return LineEditorEvent::Submitted;
    case 27:
        cancel();
        return LineEditorEvent::Canceled;
    case KEY_LEFT:
        if (cursor_ > 0) {
            --cursor_;
        }
        break;
    case KEY_RIGHT:
        if (cursor_ < text_.size()) {
            ++cursor_;
        }
        break;
    case KEY_HOME:
        cursor_ = 0;
        break;
    case KEY_END:
        cursor_ = text_.size();
        break;
    case KEY_BACKSPACE:
    case 127:
    case 8:
        backspace();
        break;
    case KEY_DC:
        delete_char();
        break;
    default:
        if (key >= 32 && key <= 126) {
            insert_char(static_cast<char>(key));
        }
        break;
    }

    return LineEditorEvent::None;
}

void LineEditor::render(WINDOW* window, int width) const {
    werase(window);
    if (active_) {
        wattron(window, A_BOLD);
        waddstr(window, prompt_.c_str());
        wattroff(window, A_BOLD);
        waddnstr(window, text_.c_str(), std::max(0, width - static_cast<int>(prompt_.size()) - 1));
        const int cursor_x = std::min(width - 1, static_cast<int>(prompt_.size() + cursor_));
        wmove(window, 0, cursor_x);
    } else {
        wattron(window, COLOR_PAIR(2));
        waddstr(window, "normal");
        wattroff(window, COLOR_PAIR(2));
    }
    wclrtoeol(window);
    wnoutrefresh(window);
}

void LineEditor::insert_char(char ch) {
    text_.insert(text_.begin() + static_cast<std::ptrdiff_t>(cursor_), ch);
    ++cursor_;
}

void LineEditor::backspace() {
    if (cursor_ == 0) {
        return;
    }
    text_.erase(text_.begin() + static_cast<std::ptrdiff_t>(cursor_ - 1));
    --cursor_;
}

void LineEditor::delete_char() {
    if (cursor_ >= text_.size()) {
        return;
    }
    text_.erase(text_.begin() + static_cast<std::ptrdiff_t>(cursor_));
}

} // namespace lv::ui
