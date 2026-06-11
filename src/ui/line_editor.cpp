#include "ui/line_editor.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace lv::ui {

LineEditor::~LineEditor() {
    destroy_form();
}

void LineEditor::start(std::string prompt, std::string text) {
    active_ = true;
    prompt_ = std::move(prompt);
    text_ = std::move(text);
    cursor_ = text_.size();
    history_index_ = history_.size();
    if (form_ != nullptr) {
        set_field_text(text_);
    }
}

void LineEditor::cancel() {
    active_ = false;
    prompt_.clear();
    text_.clear();
    cursor_ = 0;
    history_index_ = history_.size();
    destroy_form();
}

LineEditorEvent LineEditor::handle_key(int key) {
    if (!active_) {
        return LineEditorEvent::None;
    }

    if (form_ == nullptr) {
        switch (key) {
        case 10:
        case KEY_ENTER:
            commit_history();
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
            fallback_backspace();
            break;
        case KEY_DC:
            fallback_delete();
            break;
        case KEY_UP:
            history_previous();
            break;
        case KEY_DOWN:
            history_next();
            break;
        default:
            if (key >= 32 && key <= 126) {
                fallback_insert(static_cast<char>(key));
            }
            break;
        }
        return LineEditorEvent::None;
    }

    switch (key) {
    case 10:
    case KEY_ENTER:
        form_driver(form_, REQ_VALIDATION);
        sync_text_from_field();
        commit_history();
        active_ = false;
        destroy_form();
        return LineEditorEvent::Submitted;
    case 27:
        cancel();
        return LineEditorEvent::Canceled;
    case KEY_LEFT:
        if (cursor_ > 0) {
            --cursor_;
            sync_form_cursor();
        }
        break;
    case KEY_RIGHT:
        sync_text_from_field();
        if (cursor_ < text_.size()) {
            ++cursor_;
            sync_form_cursor();
        }
        break;
    case KEY_HOME:
        cursor_ = 0;
        sync_form_cursor();
        break;
    case KEY_END:
        sync_text_from_field();
        cursor_ = text_.size();
        sync_form_cursor();
        break;
    case KEY_BACKSPACE:
    case 127:
    case 8:
        if (cursor_ > 0) {
            form_driver(form_, REQ_DEL_PREV);
            --cursor_;
        }
        break;
    case KEY_DC:
        if (cursor_ < text_.size()) {
            form_driver(form_, REQ_DEL_CHAR);
        }
        break;
    case KEY_IC:
        form_driver(form_, REQ_INS_MODE);
        break;
    case KEY_UP:
        history_previous();
        break;
    case KEY_DOWN:
        history_next();
        break;
    case 21:
        form_driver(form_, REQ_BEG_LINE);
        form_driver(form_, REQ_CLR_EOL);
        cursor_ = 0;
        break;
    default:
        if (key >= 32 && key <= 126) {
            form_driver(form_, key);
            ++cursor_;
        }
        break;
    }

    sync_text_from_field();
    cursor_ = std::min(cursor_, text_.size());
    return LineEditorEvent::None;
}

void LineEditor::render(WINDOW* window, int width) {
    if (active_) {
        const bool needs_rebuild = form_ == nullptr || bound_window_ != window || bound_width_ != width ||
                                   bound_prompt_width_ != static_cast<int>(prompt_.size());
        if (needs_rebuild) {
            werase(window);
        }
        ensure_form(window, width);
        wattron(window, A_BOLD);
        mvwaddstr(window, 0, 0, prompt_.c_str());
        wattroff(window, A_BOLD);
        pos_form_cursor(form_);
    } else {
        destroy_form();
        werase(window);
        wclrtoeol(window);
    }
    wnoutrefresh(window);
}

void LineEditor::ensure_form(WINDOW* window, int width) {
    const int prompt_width = static_cast<int>(prompt_.size());
    const int next_field_width = std::max(1, width - prompt_width);
    if (form_ != nullptr && bound_window_ == window && bound_width_ == width && bound_prompt_width_ == prompt_width &&
        field_width_ == next_field_width) {
        return;
    }

    destroy_form();
    bound_window_ = window;
    bound_width_ = width;
    bound_prompt_width_ = prompt_width;
    field_width_ = next_field_width;

    field_window_ = derwin(window, 1, field_width_, 0, prompt_width);
    fields_[0] = new_field(1, field_width_, 0, 0, 0, 0);
    fields_[1] = nullptr;
    set_field_opts(fields_[0], O_VISIBLE | O_PUBLIC | O_EDIT | O_ACTIVE);
    set_max_field(fields_[0], 4096);
    set_field_back(fields_[0], A_NORMAL);

    form_ = new_form(fields_);
    set_form_win(form_, window);
    set_form_sub(form_, field_window_);
    post_form(form_);
    set_field_text(text_);
}

void LineEditor::destroy_form() {
    if (form_ != nullptr) {
        unpost_form(form_);
        free_form(form_);
        form_ = nullptr;
    }
    if (fields_[0] != nullptr) {
        free_field(fields_[0]);
        fields_[0] = nullptr;
    }
    if (field_window_ != nullptr) {
        delwin(field_window_);
        field_window_ = nullptr;
    }
    fields_[1] = nullptr;
    bound_window_ = nullptr;
    bound_width_ = 0;
    bound_prompt_width_ = 0;
    field_width_ = 0;
}

void LineEditor::set_field_text(const std::string& text) {
    text_ = text;
    cursor_ = text_.size();
    if (fields_[0] == nullptr) {
        return;
    }
    set_field_buffer(fields_[0], 0, text_.c_str());
    sync_form_cursor();
}

void LineEditor::sync_text_from_field() {
    if (fields_[0] == nullptr) {
        return;
    }
    form_driver(form_, REQ_VALIDATION);
    text_ = trim_field_buffer(field_buffer(fields_[0], 0));
    cursor_ = std::min(cursor_, text_.size());
}

void LineEditor::sync_form_cursor() {
    if (form_ == nullptr) {
        return;
    }
    form_driver(form_, REQ_BEG_LINE);
    const std::size_t moves = std::min(cursor_, text_.size());
    for (std::size_t i = 0; i < moves; ++i) {
        form_driver(form_, REQ_RIGHT_CHAR);
    }
}

void LineEditor::fallback_insert(char ch) {
    text_.insert(text_.begin() + static_cast<std::ptrdiff_t>(cursor_), ch);
    ++cursor_;
}

void LineEditor::fallback_backspace() {
    if (cursor_ == 0) {
        return;
    }
    text_.erase(text_.begin() + static_cast<std::ptrdiff_t>(cursor_ - 1));
    --cursor_;
}

void LineEditor::fallback_delete() {
    if (cursor_ >= text_.size()) {
        return;
    }
    text_.erase(text_.begin() + static_cast<std::ptrdiff_t>(cursor_));
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
    set_field_text(history_[history_index_]);
}

void LineEditor::history_next() {
    if (history_.empty()) {
        return;
    }
    if (history_index_ + 1 < history_.size()) {
        ++history_index_;
        set_field_text(history_[history_index_]);
    } else {
        history_index_ = history_.size();
        set_field_text("");
    }
}

std::string LineEditor::trim_field_buffer(const char* buffer) {
    if (buffer == nullptr) {
        return {};
    }
    std::string text(buffer);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.pop_back();
    }
    return text;
}

} // namespace lv::ui
