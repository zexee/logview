#include "ui/line_editor.h"

#include <algorithm>
#include <string_view>

namespace lv::ui {

void LineEditor::start(std::string prompt, std::string text) {
    active_ = true;
    prompt_ = std::move(prompt);
    text_ = std::move(text);
    cursor_ = text_.size();
    history_index_ = history_.size();
    reset_pending_utf8();
}

void LineEditor::cancel() {
    active_ = false;
    prompt_.clear();
    text_.clear();
    cursor_ = 0;
    history_index_ = history_.size();
    reset_pending_utf8();
}

LineEditorEvent LineEditor::handle_key(int key) {
    if (!active_) {
        return LineEditorEvent::None;
    }

    if (pending_utf8_expected_ > 0) {
        // Mid-sequence: only trailing bytes are valid input here.
        if (!feed_pending_utf8(key)) {
            // Malformed input: drop pending bytes and reprocess this key.
            reset_pending_utf8();
        } else {
            return LineEditorEvent::None;
        }
    }

    switch (key) {
    case 10:
    case 13:
    case KEY_ENTER:
        commit_history();
        active_ = false;
        return LineEditorEvent::Submitted;
    case 27:  // ESC
    case 3:   // Ctrl-C
        cancel();
        return LineEditorEvent::Canceled;
    case KEY_LEFT:
        cursor_left();
        break;
    case KEY_RIGHT:
        cursor_right();
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
        backspace_char();
        break;
    case KEY_DC:
        delete_char();
        break;
    case KEY_UP:
        history_previous();
        break;
    case KEY_DOWN:
        history_next();
        break;
    case 21:  // Ctrl-U: clear line
        clear_line();
        break;
    default:
        if (key >= 0x80 && key <= 0xFF) {
            // UTF-8 lead byte; assemble trailing bytes via feed_pending_utf8().
            pending_utf8_.clear();
            pending_utf8_.push_back(static_cast<char>(key));
            if ((key & 0xE0) == 0xC0) {
                pending_utf8_expected_ = 2;
            } else if ((key & 0xF0) == 0xE0) {
                pending_utf8_expected_ = 3;
            } else if ((key & 0xF8) == 0xF0) {
                pending_utf8_expected_ = 4;
            } else {
                pending_utf8_expected_ = 0;
            }
            if (pending_utf8_expected_ == static_cast<int>(pending_utf8_.size())) {
                insert_bytes(pending_utf8_.data(), pending_utf8_.size());
                reset_pending_utf8();
            } else if (pending_utf8_expected_ == 0) {
                // Invalid lead byte: insert as a single byte so input makes progress.
                insert_bytes(pending_utf8_.data(), 1);
                reset_pending_utf8();
            }
        } else if (key >= 32 && key <= 126) {
            const char ch = static_cast<char>(key);
            insert_bytes(&ch, 1);
        }
        break;
    }
    return LineEditorEvent::None;
}

bool LineEditor::feed_pending_utf8(int key) {
    if (key < 0x80 || key > 0xFF) {
        return false;
    }
    pending_utf8_.push_back(static_cast<char>(key));
    if (pending_utf8_.size() >= static_cast<std::size_t>(pending_utf8_expected_)) {
        insert_bytes(pending_utf8_.data(), pending_utf8_.size());
        reset_pending_utf8();
    }
    return true;
}

void LineEditor::reset_pending_utf8() {
    pending_utf8_.clear();
    pending_utf8_expected_ = 0;
}

void LineEditor::insert_bytes(const char* data, std::size_t n) {
    text_.insert(cursor_, data, n);
    cursor_ += n;
}

void LineEditor::backspace_char() {
    if (cursor_ == 0) {
        return;
    }
    const std::size_t w = utf8_char_width_at(text_, cursor_ - 1);
    text_.erase(cursor_ - w, w);
    cursor_ -= w;
}

void LineEditor::delete_char() {
    if (cursor_ >= text_.size()) {
        return;
    }
    const std::size_t w = utf8_char_width_at(text_, cursor_);
    text_.erase(cursor_, w);
}

void LineEditor::cursor_left() {
    if (cursor_ > 0) {
        cursor_ -= utf8_char_width_at(text_, cursor_ - 1);
    }
}

void LineEditor::cursor_right() {
    if (cursor_ < text_.size()) {
        cursor_ += utf8_char_width_at(text_, cursor_);
    }
}

void LineEditor::clear_line() {
    text_.clear();
    cursor_ = 0;
}

void LineEditor::commit_history() {
    if (text_.empty()) {
        history_index_ = history_.size();
        return;
    }
    if (history_.empty() || history_.back() != text_) {
        history_.push_back(text_);
    }
    history_index_ = history_.size();
}

void LineEditor::history_previous() {
    if (history_.empty()) {
        return;
    }
    if (history_index_ > 0) {
        --history_index_;
    }
    text_ = history_[history_index_];
    cursor_ = text_.size();
}

void LineEditor::history_next() {
    if (history_.empty()) {
        return;
    }
    if (history_index_ + 1 < history_.size()) {
        ++history_index_;
        text_ = history_[history_index_];
    } else {
        history_index_ = history_.size();
        text_.clear();
    }
    cursor_ = text_.size();
}

std::size_t LineEditor::utf8_char_width_at(std::string_view text, std::size_t byte_offset) {
    if (byte_offset >= text.size()) {
        return 0;
    }
    const unsigned char c = static_cast<unsigned char>(text[byte_offset]);
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    // Invalid lead byte; advance one byte so the cursor makes progress.
    return 1;
}

void LineEditor::render(WINDOW* window, int width) {
    if (window == nullptr) {
        return;
    }

    werase(window);

    if (active_) {
        const int prompt_width = std::min<int>(static_cast<int>(prompt_.size()),
                                               std::max(0, width));
        if (prompt_width > 0) {
            wattron(window, A_BOLD);
            mvwaddnstr(window, 0, 0, prompt_.c_str(), prompt_width);
            wattroff(window, A_BOLD);
        }

        const int field_width = std::max(1, width - prompt_width);

        // Horizontal scroll: keep cursor visible when text exceeds the field.
        std::size_t view_start = 0;
        if (cursor_ > static_cast<std::size_t>(field_width)) {
            view_start = cursor_ - field_width;
        }
        const std::size_t available = text_.size() > view_start
                                          ? text_.size() - view_start
                                          : 0;
        const std::size_t view_len = std::min(available, static_cast<std::size_t>(field_width));
        if (view_len > 0) {
            mvwaddnstr(window, 0, prompt_width,
                       text_.data() + view_start,
                       static_cast<int>(view_len));
        }

        const int cursor_x = prompt_width + static_cast<int>(cursor_ - view_start);
        wmove(window, 0, cursor_x);
    }

    wnoutrefresh(window);
}

} // namespace lv::ui
