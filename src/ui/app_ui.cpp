#include "ui/app_ui.h"

#include "core/filter_engine.h"
#include "core/path_util.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string_view>
#include <utility>

namespace lv::ui {

namespace {

int utf8_char_width(const char* p) {
    unsigned char c = static_cast<unsigned char>(*p);
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 1;
    if ((c & 0xF0) == 0xE0) return 2;
    if ((c & 0xF8) == 0xF0) return 2;
    return 1;
}

int utf8_byte_len(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

int utf8_display_width(std::string_view sv) {
    int w = 0;
    std::size_t i = 0;
    while (i < sv.size()) {
        unsigned char c = static_cast<unsigned char>(sv[i]);
        int len = utf8_byte_len(c);
        if (len >= 2 && i + len <= sv.size()) {
            w += utf8_char_width(sv.data() + i);
            i += static_cast<std::size_t>(len);
        } else {
            w += 1;
            ++i;
        }
    }
    return w;
}

std::size_t utf8_byte_at_column(std::string_view sv, int col) {
    std::size_t i = 0;
    int current = 0;
    while (i < sv.size() && current < col) {
        unsigned char c = static_cast<unsigned char>(sv[i]);
        int len = utf8_byte_len(c);
        if (len >= 2 && i + len <= sv.size()) {
            current += utf8_char_width(sv.data() + i);
            i += static_cast<std::size_t>(len);
        } else {
            current += 1;
            ++i;
        }
    }
    return i;
}

} // namespace

AppUi::AppUi(MMapFile file, LineIndex index, RuleSet rules, std::string rule_path)
    : file_(std::move(file)), index_(std::move(index)), rules_(std::move(rules)), rule_path_(std::move(rule_path)) {}

AppUi::~AppUi() {
    join_filter_jobs();
    cancel_search_job();
    if (incsearch_thread_.joinable()) {
        incsearch_thread_.detach();
    }
}

int AppUi::run() {
    start();
    while (running_) {
        tick();
    }
    destroy_windows();
    return 0;
}

void AppUi::start() {
#if defined(LV_USE_PDCURSES)
    // On PDCursesMod the compile-time cell grid (80×25 default) doesn't
    // match the SDL2 window. resize_term(0, 0) queries the window for its
    // pixel dimensions and recomputes LINES/COLS. We call it here (after
    // initscr and font loading are fully done) rather than in Screen::Screen
    // because some window managers deliver the final geometry through a
    // SDL_WINDOWEVENT that hasn't arrived yet during initscr.
    resize_term(0, 0);
#endif
    recreate_windows();
    start_filter_job();
}

bool AppUi::tick() {
    if (poll_filter_jobs()) {
        dirty_ = true;
    }
    poll_search_job();
    poll_incsearch();
    if (dirty_) {
        render();
        dirty_ = false;
    }
    int key = ERR;
    if (editor_.active()) {
        key = getch();
        if (key != ERR) {
            key = normalize_key(key);
            handle_key(key);
        }
        if (!editor_.active()) {
            // Editor just closed (submit/cancel/command). Force a redraw so
            // side effects like :open, :rules, :help are visible without an
            // extra keystroke.
            dirty_ = true;
            return running_;
        }
        render_editor();
        doupdate();
        return running_;
    }
    key = getch();
    if (key != ERR) {
        key = normalize_key(key);
        handle_key(key);
        dirty_ = true;
    }
    return running_;
}

void AppUi::prefill_command(const std::string& cmd) {
    editor_.start(":", cmd);
    dirty_ = true;
}

void AppUi::recreate_windows() {
    destroy_windows();

    const int rows = std::max(3, screen_.rows());
    const int cols = std::max(20, screen_.cols());
#if defined(LV_USE_PDCURSES)
    // The ImGui menu bar is ~1 text row tall at default font sizes.
    // Reserve one row so the TUI content starts below the bar instead of
    // being painted over by it.
    const int top_pad = 1;
#else
    const int top_pad = 0;
#endif
    const int editor_height = 1;
    const int content_height = rows - editor_height - top_pad;
    const int separator_height = (content_height > 3 && rules_visible_) ? 1 : 0;
    const int split_content_height = std::max(1, content_height - separator_height);
    const int rules_height = [&]() -> int {
        if (!rules_visible_) {
            return 0;
        }
        const int content = rules_.empty() ? 1 : static_cast<int>(rules_.size()) + 1;
        const int max_height = std::min(10, std::max(1, split_content_height - 1));
        return std::clamp(content, 1, max_height);
    }();
    const int log_height = std::max(1, split_content_height - rules_height);

    log_rect_ = Rect{top_pad, 0, log_height, cols};
    rules_rect_ = Rect{log_height + separator_height + top_pad, 0, rules_height, cols};
    editor_rect_ = Rect{rows - 1, 0, editor_height, cols};

    log_window_ = newwin(log_rect_.height, log_rect_.width, log_rect_.y, log_rect_.x);
    if (rules_visible_) {
        rules_window_ = newwin(rules_rect_.height, rules_rect_.width, rules_rect_.y, rules_rect_.x);
    }
    editor_window_ = newwin(editor_rect_.height, editor_rect_.width, editor_rect_.y, editor_rect_.x);

    keypad(log_window_, TRUE);
    if (rules_visible_) {
        keypad(rules_window_, TRUE);
    }
    keypad(editor_window_, TRUE);
}

void AppUi::destroy_windows() {
    if (log_window_ != nullptr) {
        delwin(log_window_);
        log_window_ = nullptr;
    }
    if (rules_window_ != nullptr) {
        delwin(rules_window_);
        rules_window_ = nullptr;
    }
    if (editor_window_ != nullptr) {
        delwin(editor_window_);
        editor_window_ = nullptr;
    }
    if (help_window_ != nullptr) {
        delwin(help_window_);
        help_window_ = nullptr;
    }
}

void AppUi::render() {
    if (editor_.active()) {
        render_editor();
        doupdate();
        return;
    }
    render_log();
    if (help_active_) {
        render_help();
    } else if (rules_visible_) {
        render_rules();
    }
    render_editor();
#if defined(LV_USE_PDCURSES)
    // PDCursesMod's GL backend does not reliably composite subwindow
    // content via the standard wnoutrefresh -> curscr -> doupdate chain.
    // As a workaround, copy every subwindow's content directly onto
    // stdscr using copywin, then let doupdate render stdscr to the GL
    // framebuffer. The copy is O(ncols * nlines) per frame; at typical
    // terminal sizes (80x25) this is negligible.
    if (log_window_ != nullptr && log_window_ != stdscr) {
        copywin(log_window_, stdscr, 0, 0,
                log_rect_.y, log_rect_.x,
                log_rect_.y + log_rect_.height - 1,
                log_rect_.x + log_rect_.width - 1,
                FALSE);
    }
    if (rules_window_ != nullptr && rules_window_ != stdscr && rules_visible_) {
        copywin(rules_window_, stdscr, 0, 0,
                rules_rect_.y, rules_rect_.x,
                rules_rect_.y + rules_rect_.height - 1,
                rules_rect_.x + rules_rect_.width - 1,
                FALSE);
    }
    if (editor_window_ != nullptr && editor_window_ != stdscr) {
        copywin(editor_window_, stdscr, 0, 0,
                editor_rect_.y, editor_rect_.x,
                editor_rect_.y,  // single-row editor window
                editor_rect_.x + editor_rect_.width - 1,
                FALSE);
    }
    clearok(stdscr, TRUE);
#endif
    doupdate();
}

void AppUi::render_log() {
    werase(log_window_);
    if (focus_ == Focus::Log && !editor_.active()) {
        wattron(log_window_, COLOR_PAIR(1));
    }
    const std::string& path = file_.path();
    const int title_width = std::min<int>(log_rect_.width, static_cast<int>(path.size()));
    if (title_width > 0) {
        const int title_x = std::max(0, (log_rect_.width - title_width) / 2);
        // title_width is a column budget; convert to bytes so UTF-8 paths
        // don't overflow the layout or split a codepoint on Windows.
        const std::size_t title_bytes = utf8_byte_at_column(path, title_width);
        mvwaddnstr(log_window_, 0, title_x, path.c_str(), static_cast<int>(title_bytes));
    }
    if (focus_ == Focus::Log && !editor_.active()) {
        wattroff(log_window_, COLOR_PAIR(1));
    }
    if (rules_rect_.y > log_rect_.y + log_rect_.height && log_rect_.width > 0) {
        const char* label = " Filters ";
        const int label_len = 9;
        const int left_len = (log_rect_.width - label_len) / 2;
        const int line_y = log_rect_.height;
        wattron(stdscr, COLOR_PAIR(4));
        if (left_len > 0) {
            mvwhline(stdscr, line_y, 0, ACS_HLINE, left_len);
        }
        if (focus_ == Focus::Rules) {
            wattron(stdscr, A_REVERSE);
        }
        mvwaddstr(stdscr, line_y, left_len, label);
        if (focus_ == Focus::Rules) {
            wattroff(stdscr, A_REVERSE);
        }
        const int right_start = left_len + label_len;
        if (right_start < log_rect_.width) {
            mvwhline(stdscr, line_y, right_start, ACS_HLINE, log_rect_.width - right_start);
        }
        wattroff(stdscr, COLOR_PAIR(4));
        wnoutrefresh(stdscr);
    }

    const int content_height = std::max(0, log_rect_.height - 1);
    const int number_width = line_number_width();
    const int content_width = std::max(1, log_rect_.width - number_width - 2);
    const std::string highlight = active_literal_highlight();
    keep_cursor_visible(content_width, content_height);

    int row = 1;
    for (LineNumber line_number = log_top_line_; line_number < index_.line_count() && row <= content_height;
         ++line_number) {
        if (!line_visible(line_number)) {
            continue;
        }

        const std::string_view line = index_.line(line_number);
        const int wraps = line_wrap_rows(line_number, content_width);
        const bool selected = line_number == log_cursor_;
        const std::vector<HighlightMatch> search_matches =
            search_regex_ ? find_line_matches(line_number) : std::vector<HighlightMatch>{};
        for (int wrap = 0; wrap < wraps && row <= content_height; ++wrap, ++row) {
            const int wrap_start_col = wrap * content_width;
            const std::size_t offset = utf8_byte_at_column(line, wrap_start_col);
            const std::size_t end_offset = utf8_byte_at_column(line, wrap_start_col + content_width);
            const std::size_t chunk_bytes = std::min(end_offset, line.size()) - offset;
            const std::string_view chunk(line.data() + offset, chunk_bytes);

            if (wrap == 0) {
                wattron(log_window_, COLOR_PAIR(4));
                mvwprintw(log_window_, row, 0, "%*zu", number_width - 1, line_number + 1);
                wattroff(log_window_, COLOR_PAIR(4));
                bool marked = mark_bitmap_.size() > 0 && mark_bitmap_.get(line_number);
                if (marked) {
                    wattron(log_window_, COLOR_PAIR(5) | A_BOLD);
                    waddch(log_window_, 'M');
                    wattroff(log_window_, COLOR_PAIR(5) | A_BOLD);
                } else {
                    waddch(log_window_, ' ');
                }
            } else {
                wattron(log_window_, COLOR_PAIR(4));
                mvwprintw(log_window_, row, 0, "%*s ", number_width - 1, "");
                wattroff(log_window_, COLOR_PAIR(4));
            }

            if (selected) {
                wattron(log_window_, A_REVERSE);
            }

            if (chunk_bytes > 0) {
                render_log_chunk(row, number_width, chunk, highlight, selected);
            }
            const int chunk_display_width = utf8_display_width(chunk);
            const int used = number_width + chunk_display_width;
            if (used < log_rect_.width) {
                mvwprintw(log_window_, row, used, "%*s", log_rect_.width - used, "");
            }
            if (selected) {
                wattroff(log_window_, A_REVERSE);
            }

            if (search_regex_ && !search_matches.empty()) {
                const std::size_t chunk_byte_end = offset + chunk_bytes;
                for (const HighlightMatch& m : search_matches) {
                    if (m.start < chunk_byte_end && m.start + m.length > offset) {
                        const std::size_t chunk_start =
                            m.start > offset ? m.start - offset : 0;
                        const std::size_t chunk_end =
                            std::min(m.start + m.length, chunk_byte_end) - offset;
                        const std::size_t match_chunk_len = chunk_end - chunk_start;
                        const int draw_col = number_width + utf8_display_width(
                            std::string_view(chunk.data(), chunk_start));
                        wattron(log_window_, COLOR_PAIR(3) | A_BOLD);
                        wmove(log_window_, row, draw_col);
                        waddnstr(log_window_, chunk.data() + chunk_start, static_cast<int>(match_chunk_len));
                        wattroff(log_window_, COLOR_PAIR(3) | A_BOLD);
                    }
                }
            }
        }
    }
    wnoutrefresh(log_window_);
}

void AppUi::render_rules() {
    werase(rules_window_);

    const int inner_height = std::max(0, rules_rect_.height);
    keep_rule_cursor_visible();
    for (int row = 0; row < inner_height; ++row) {
        const std::size_t index = rule_top_ + static_cast<std::size_t>(row);
        if (index >= rules_.size()) {
            break;
        }
        if (index == rule_cursor_) {
            wattron(rules_window_, A_REVERSE);
        }
        const std::string text = rules_[index].serialize();
        mvwprintw(rules_window_, row, 0, "%2zu  %-*s", index + 1, rules_rect_.width - 6, text.c_str());
        if (index == rule_cursor_) {
            wattroff(rules_window_, A_REVERSE);
        }
    }
    if (rules_.empty()) {
        mvwprintw(rules_window_, 0, 0, "No filters");
    }
    wnoutrefresh(rules_window_);
}

void AppUi::render_editor() {
    editor_.render(editor_window_, editor_rect_.width);
    if (!editor_.active()) {
        const int width = std::max(1, editor_rect_.width);

        if (!search_status_.empty()) {
            wattron(editor_window_, A_BOLD);
            const std::size_t len = utf8_byte_at_column(search_status_, width);
            mvwaddnstr(editor_window_, 0, 0, search_status_.c_str(), static_cast<int>(len));
            wattroff(editor_window_, A_BOLD);
        }

        if (!status_.empty()) {
            // Status is right-aligned to width; convert display width to bytes
            // for the same UTF-8 safety as above.
            const int status_cols = utf8_display_width(status_);
            const int visible_cols = std::min<int>(status_cols, width);
            const int start_col = std::max(0, width - visible_cols);
            const std::size_t skip_bytes = utf8_byte_at_column(status_, width - visible_cols);
            const std::size_t keep_bytes = utf8_byte_at_column(status_, width) - skip_bytes;
            mvwaddnstr(editor_window_, 0, start_col,
                       status_.c_str() + skip_bytes,
                       static_cast<int>(keep_bytes));
        }
        wnoutrefresh(editor_window_);
    }
}

int AppUi::normalize_key(int key) {
    if (key != 27 || editor_.active()) {
        return key;
    }

    const int first = getch();
    if (first == ERR) {
        return key;
    }
    if (first != '[' && first != 'O') {
        return key;
    }

    const int second = getch();
    switch (second) {
    case 'H':
        return KEY_HOME;
    case 'F':
        return KEY_END;
    case '5':
        if (getch() == '~') {
            return KEY_PPAGE;
        }
        break;
    case '6':
        if (getch() == '~') {
            return KEY_NPAGE;
        }
        break;
    case '1':
    case '7':
        if (getch() == '~') {
            return KEY_HOME;
        }
        break;
    case '4':
    case '8':
        if (getch() == '~') {
            return KEY_END;
        }
        break;
    default:
        break;
    }
    return key;
}

void AppUi::handle_key(int key) {
    if (key == KEY_MOUSE) {
        handle_mouse();
        return;
    }
    if (help_active_) {
        handle_help_key(key);
        return;
    }
    if (editor_.active()) {
        const LineEditorEvent event = editor_.handle_key(key);
        if (event == LineEditorEvent::Submitted) {
            handle_editor_submit();
        } else if (event == LineEditorEvent::Canceled) {
            editing_rule_ = false;
            adding_rule_ = false;
            if (search_active_) {
                search_active_ = false;
                log_cursor_ = search_orig_cursor_;
                incsearch_result_.reset();
            }
            render();
            dirty_ = false;
        } else if (search_active_ && event == LineEditorEvent::None) {
            incsearch();
        }
        return;
    }

    switch (key) {
    case KEY_RESIZE:
#if defined(_WIN32)
        // PDCursesMod does not auto-resize; pull the new size explicitly so
        // getmaxyx() on recreate_windows() sees the updated geometry.
        if (is_termresized()) {
            resize_term(0, 0);
        }
#endif
        recreate_windows();
        break;
    case ' ':
        if (focus_ == Focus::Log) {
            rules_visible_ = !rules_visible_;
            if (rules_visible_) {
                focus_ = Focus::Rules;
            }
        } else if (focus_ == Focus::Rules) {
            rules_visible_ = false;
            focus_ = Focus::Log;
        }
        recreate_windows();
        break;
    case 27:
        if (focus_ == Focus::Rules) {
            rules_visible_ = false;
            focus_ = Focus::Log;
            recreate_windows();
        }
        break;
    case '\t':
        if (!rules_visible_) {
            break;
        }
        focus_ = focus_ == Focus::Log ? Focus::Rules : Focus::Log;
        break;
    case ':':
        editing_rule_ = false;
        adding_rule_ = false;
        search_active_ = false;
        editor_.start(":", "");
        break;
    case '/':
        if (focus_ == Focus::Log) {
            begin_search();
        }
        break;
    case '?':
        if (focus_ == Focus::Log) {
            begin_search_backward();
        }
        break;
    case 4:
        running_ = false;
        break;
    case KEY_UP:
    case 'k':
        if (focus_ == Focus::Rules) {
            if (rule_cursor_ > 0) {
                --rule_cursor_;
            }
        } else if (index_.line_count() > 0) {
            log_cursor_ = previous_visible_line(log_cursor_);
        }
        break;
    case KEY_DOWN:
    case 'j':
        if (focus_ == Focus::Rules) {
            if (rule_cursor_ + 1 < rules_.size()) {
                ++rule_cursor_;
            }
        } else if (index_.line_count() > 0) {
            log_cursor_ = next_visible_line(log_cursor_);
        }
        break;
    case KEY_NPAGE:
    case 6:
        if (focus_ == Focus::Log) {
            move_log_page(1);
        }
        break;
    case KEY_PPAGE:
    case 2:
        if (focus_ == Focus::Log) {
            move_log_page(-1);
        }
        break;
    case 'g':
    case KEY_HOME:
    case KEY_FIND:
    case KEY_A1:
        if (focus_ == Focus::Log) {
            log_cursor_ = first_visible_line();
            log_top_line_ = log_cursor_;
        }
        break;
    case 'G':
    case KEY_END:
    case KEY_SELECT:
    case KEY_C1:
        if (focus_ == Focus::Log) {
            log_cursor_ = last_visible_line();
            log_top_line_ = log_cursor_;
        }
        break;
    case 'n':
        if (focus_ == Focus::Log && search_regex_) {
            jump_to_next_match();
        }
        break;
    case 'N':
        if (focus_ == Focus::Log && search_regex_) {
            jump_to_previous_match();
        }
        break;
    case 'm':
        if (focus_ == Focus::Log && index_.line_count() > 0) {
            if (mark_bitmap_.size() == 0) {
                mark_bitmap_.resize(index_.line_count(), false);
            }
            mark_bitmap_.set(log_cursor_, !mark_bitmap_.get(log_cursor_));
        }
        break;
    case ',':
        if (focus_ == Focus::Log && mark_bitmap_.size() > 0) {
            for (LineNumber c = log_cursor_ > 0 ? log_cursor_ - 1 : index_.line_count() - 1;
                 ; --c) {
                if (c == log_cursor_) break;
                if (mark_bitmap_.get(c)) {
                    log_cursor_ = c;
                    break;
                }
                if (c == 0) c = index_.line_count();
            }
        }
        break;
    case '.':
        if (focus_ == Focus::Log && mark_bitmap_.size() > 0) {
            for (LineNumber c = log_cursor_ + 1; c < index_.line_count(); ++c) {
                if (mark_bitmap_.get(c)) {
                    log_cursor_ = c;
                    break;
                }
            }
        }
        break;
    case '[':
        if (focus_ == Focus::Rules) {
            move_selected_rule_up();
        }
        break;
    case ']':
        if (focus_ == Focus::Rules) {
            move_selected_rule_down();
        }
        break;
    case 'a':
        begin_rule_add(rules_.empty() ? 0 : rule_cursor_ + 1);
        break;
    case 'i':
        begin_rule_add(rules_.empty() ? 0 : rule_cursor_);
        break;
    case 'A':
        begin_rule_add(rules_.size());
        break;
    case 'I':
        begin_rule_add(0);
        break;
    case 'x':
    case 'd':
        if (focus_ == Focus::Rules) {
            delete_selected_rule();
        }
        break;
    case '-':
        if (focus_ == Focus::Rules && !rules_.empty()) {
            Rule& rule = rules_[rule_cursor_];
            rule.set_enabled(!rule.enabled());
            status_ = rule.enabled() ? "rule enabled" : "rule disabled";
            start_filter_job(rule_cursor_);
        }
        break;
    case 10:
    case KEY_ENTER:
        if (focus_ == Focus::Rules) {
            begin_rule_edit();
        }
        break;
    default:
        break;
    }
}

void AppUi::handle_editor_submit() {
    const std::string text = editor_.text();
    if (search_active_) {
        handle_search_submit();
        return;
    }
    if (!editing_rule_) {
        handle_command(text);
        return;
    }

    lv::RuleParseResult parsed = lv::RuleSet::parse_line(text);
    if (!parsed.ok) {
        status_ = "rule error: " + parsed.error;
        editing_rule_ = false;
        adding_rule_ = false;
        return;
    }

    if (adding_rule_ || rules_.empty()) {
        const std::size_t insert_index = std::min(pending_insert_index_, rules_.size());
        rules_.insert(insert_index, std::move(parsed.rule));
        if (active_filter_) {
            active_filter_->insert_layer(insert_index);
        }
        rule_cursor_ = insert_index;
        status_ = "rule updated; filtering";
        editing_rule_ = false;
        adding_rule_ = false;
        start_filter_job(insert_index);
        return;
    } else {
        rules_.replace(rule_cursor_, std::move(parsed.rule));
        status_ = "rule updated; filtering";
        editing_rule_ = false;
        adding_rule_ = false;
        start_filter_job(rule_cursor_);
        return;
    }
}

void AppUi::handle_command(const std::string& command) {
    std::istringstream input(command);
    std::string name;
    input >> name;

    if (name == "q" || name == "quit") {
        running_ = false;
        return;
    }
    if (name == "h" || name == "help") {
        begin_help();
        return;
    }
    if (name == "r" || name == "rules") {
        std::string path;
        input >> path;
        if (path.empty()) {
            status_ = "usage: :r <rule_file>";
            return;
        }
        path = lv::to_utf8(lv::expand_path(path));
        std::string error;
        RuleSet loaded;
        if (!loaded.load(path, &error)) {
            status_ = error;
            return;
        }
        rules_ = std::move(loaded);
        rule_path_ = path;
        rule_cursor_ = 0;
        rule_top_ = 0;
        status_ = "rules loaded; filtering";
        active_filter_.reset();
        filter_bitmap_ = nullptr;
        start_filter_job(0);
        return;
    }
    if (name == "wr" || name == "write-rules") {
        std::string path;
        input >> path;
        if (!path.empty()) {
            path = lv::to_utf8(lv::expand_path(path));
        } else {
            path = rule_path_;
        }
        if (path.empty()) {
            status_ = "usage: :wr <rule_file>";
            return;
        }
        std::string error;
        if (!rules_.save(path, &error)) {
            status_ = error;
            return;
        }
        rule_path_ = path;
        status_ = "rules written: " + path;
        return;
    }
    if (name == "w" || name == "write" || name == "save-filtered") {
        std::string path;
        input >> path;
        if (path.empty()) {
            status_ = "usage: :w <output_file>";
            return;
        }
        path = lv::to_utf8(lv::expand_path(path));
        save_filtered_file(path);
        return;
    }
    if (name == "o" || name == "open" || name == "e") {
        std::string path;
        input >> path;
        if (path.empty()) {
            path = file_.path();
        }
        if (path.empty()) {
            status_ = "no file to reload";
            return;
        }
        path = lv::to_utf8(lv::expand_path(path));
        open_log_file(path);
        return;
    }
    status_ = "unknown command: " + command;
}

void AppUi::open_log_file(const std::string& path) {
    join_filter_jobs();
    cancel_search_job();

    MMapFile file;
    if (!file.open(path)) {
        status_ = "cannot open log file: " + path;
        return;
    }

    LineIndex index;
    if (!index.build(file)) {
        status_ = "cannot index log file: " + path;
        return;
    }

    file_ = std::move(file);
    index_ = std::move(index);
    active_filter_.reset();
    filter_bitmap_ = nullptr;
    search_regex_.reset();
    search_pattern_.clear();
    search_status_.clear();
    search_matches_ = BitArray();
    mark_bitmap_ = BitArray();
    incsearch_result_.reset();
    log_cursor_ = 0;
    log_top_line_ = 0;
    status_ = "opened; filtering: " + path;
    start_filter_job();
}

void AppUi::save_filtered_file(const std::string& path) {
    if (path == file_.path()) {
        status_ = "cannot overwrite source file: " + path;
        return;
    }

    wait_for_filter_jobs();

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        status_ = "cannot write: " + path;
        return;
    }

    std::size_t written = 0;
    for (LineNumber line = 0; line < index_.line_count(); ++line) {
        if (!line_visible(line)) {
            continue;
        }
        const std::string_view text = index_.line(line);
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        out.put('\n');
        ++written;
    }
    status_ = "saved " + std::to_string(written) + " lines: " + path;
}

void AppUi::begin_rule_edit() {
    editing_rule_ = true;
    adding_rule_ = false;
    editor_.start("", selected_rule_text());
}

void AppUi::begin_rule_add(std::size_t index) {
    editing_rule_ = true;
    adding_rule_ = true;
    pending_insert_index_ = std::min(index, rules_.size());
    editor_.start("", "");
}

void AppUi::delete_selected_rule() {
    if (rules_.empty()) {
        status_ = "no rule to delete";
        return;
    }
    const std::size_t deleted_idx = rule_cursor_;
    rules_.remove(deleted_idx);
    if (active_filter_) {
        active_filter_->remove_layer(deleted_idx);
    }
    if (rule_cursor_ >= rules_.size() && rule_cursor_ > 0) {
        --rule_cursor_;
    }
    if (rule_top_ > rule_cursor_) {
        rule_top_ = rule_cursor_;
    }
    status_ = "rule deleted; filtering";
    start_filter_job(kMergeOnly);
}

void AppUi::move_selected_rule_up() {
    const std::size_t old_idx = rule_cursor_;
    if (rules_.move_up(rule_cursor_)) {
        --rule_cursor_;
        if (active_filter_) {
            std::swap(active_filter_->layer(old_idx), active_filter_->layer(rule_cursor_));
        }
        status_ = "rule moved; filtering";
        start_filter_job(rule_cursor_);
    }
}

void AppUi::move_selected_rule_down() {
    const std::size_t old_idx = rule_cursor_;
    if (rules_.move_down(rule_cursor_)) {
        ++rule_cursor_;
        if (active_filter_) {
            std::swap(active_filter_->layer(old_idx), active_filter_->layer(rule_cursor_));
        }
        status_ = "rule moved; filtering";
        start_filter_job(old_idx);
    }
}

std::string AppUi::selected_rule_text() const {
    if (rules_.empty()) {
        return "ss ";
    }
    return rules_[rule_cursor_].serialize();
}

void AppUi::keep_rule_cursor_visible() {
    const std::size_t visible_rows = rules_rect_.height > 0 ? static_cast<std::size_t>(rules_rect_.height) : 0;
    if (visible_rows == 0 || rules_.empty()) {
        rule_top_ = 0;
        return;
    }
    if (rule_cursor_ >= rules_.size()) {
        rule_cursor_ = rules_.size() - 1;
    }
    if (rule_cursor_ < rule_top_) {
        rule_top_ = rule_cursor_;
    } else if (rule_cursor_ >= rule_top_ + visible_rows) {
        rule_top_ = rule_cursor_ - visible_rows + 1;
    }
}

void AppUi::start_filter_job(std::size_t recompute_from) {
    auto state = std::make_shared<FilterJobState>();
    state->generation = next_filter_generation_++;
    RuleSet rules = rules_;
    LineIndex index = index_;

    std::unique_ptr<FilterResult> base;
    if (recompute_from > 0 && recompute_from != kMergeOnly && active_filter_) {
        base = std::make_unique<FilterResult>(*active_filter_);
    } else if (recompute_from == kMergeOnly && active_filter_) {
        base = std::make_unique<FilterResult>(*active_filter_);
    }

    std::thread thread([state, rules = std::move(rules), index = std::move(index),
                        base = std::move(base), recompute_from]() mutable {
        FilterEngine engine;
        if (recompute_from == 0) {
            FilterResult result = engine.run(index, rules);
            std::lock_guard<std::mutex> lock(state->mutex);
            state->result = std::move(result);
        } else if (recompute_from == kMergeOnly) {
            engine.merge_final(*base, rules);
            std::lock_guard<std::mutex> lock(state->mutex);
            state->result = std::move(*base);
        } else {
            engine.recompute_from(*base, index, rules, recompute_from);
            std::lock_guard<std::mutex> lock(state->mutex);
            state->result = std::move(*base);
        }
        state->done = true;
    });

    filter_jobs_.push_back(FilterJob{state, std::move(thread)});
}

bool AppUi::poll_filter_jobs() {
    bool changed = false;
    for (auto it = filter_jobs_.begin(); it != filter_jobs_.end();) {
        bool done = false;
        {
            std::lock_guard<std::mutex> lock(it->state->mutex);
            done = it->state->done;
        }

        if (!done) {
            ++it;
            continue;
        }

        if (it->thread.joinable()) {
            it->thread.join();
        }

        std::unique_ptr<FilterResult> result;
        std::uint64_t generation = 0;
        {
            std::lock_guard<std::mutex> lock(it->state->mutex);
            generation = it->state->generation;
            result = std::make_unique<FilterResult>(std::move(it->state->result));
        }

        if (generation >= applied_filter_generation_) {
            applied_filter_generation_ = generation;
            active_filter_ = std::move(result);
            filter_bitmap_ = &active_filter_->final();
            status_ = "filtered: " + std::to_string(active_filter_->visible_count()) + "/" +
                      std::to_string(active_filter_->line_count());
            if (!line_visible(log_cursor_)) {
                log_cursor_ = next_visible_line(log_cursor_);
                if (!line_visible(log_cursor_)) {
                    log_cursor_ = previous_visible_line(log_cursor_);
                }
            }
            changed = true;
        }

        it = filter_jobs_.erase(it);
    }
    return changed;
}

void AppUi::join_filter_jobs() {
    for (FilterJob& job : filter_jobs_) {
        if (job.thread.joinable()) {
            job.thread.join();
        }
    }
    filter_jobs_.clear();
}

void AppUi::wait_for_filter_jobs() {
    for (FilterJob& job : filter_jobs_) {
        if (job.thread.joinable()) {
            job.thread.join();
        }
    }
    dirty_ = poll_filter_jobs() || dirty_;
}

bool AppUi::line_visible(LineNumber line) const {
    if (line >= index_.line_count()) {
        return false;
    }
    return filter_bitmap_ == nullptr || filter_bitmap_->get(line);
}

LineNumber AppUi::first_visible_line() const {
    for (LineNumber line = 0; line < index_.line_count(); ++line) {
        if (line_visible(line)) {
            return line;
        }
    }
    return 0;
}

LineNumber AppUi::last_visible_line() const {
    if (index_.line_count() == 0) {
        return 0;
    }
    LineNumber line = index_.line_count() - 1;
    while (line > 0) {
        if (line_visible(line)) {
            return line;
        }
        --line;
    }
    return line_visible(0) ? 0 : index_.line_count() - 1;
}

LineNumber AppUi::next_visible_line(LineNumber line) const {
    if (index_.line_count() == 0) {
        return 0;
    }
    for (LineNumber candidate = std::min(line + 1, index_.line_count() - 1); candidate < index_.line_count();
         ++candidate) {
        if (line_visible(candidate)) {
            return candidate;
        }
    }
    return line;
}

LineNumber AppUi::previous_visible_line(LineNumber line) const {
    if (index_.line_count() == 0 || line == 0) {
        return 0;
    }
    LineNumber candidate = line - 1;
    while (candidate > 0) {
        if (line_visible(candidate)) {
            return candidate;
        }
        --candidate;
    }
    return line_visible(0) ? 0 : line;
}

void AppUi::move_log_page(int direction) {
    if (index_.line_count() == 0 || direction == 0) {
        return;
    }

    const int steps = std::max(1, log_rect_.height - 2);
    move_log_lines(direction, steps);
}

void AppUi::move_log_lines(int direction, int steps) {
    if (index_.line_count() == 0 || direction == 0 || steps <= 0) {
        return;
    }
    for (int i = 0; i < steps; ++i) {
        const LineNumber next = direction > 0 ? next_visible_line(log_cursor_) : previous_visible_line(log_cursor_);
        if (next == log_cursor_) {
            break;
        }
        log_cursor_ = next;
    }
}

namespace {
// ncurses exposes getmouse(MEVENT*); PDCursesMod exposes nc_getmouse(MEVENT*)
// alongside its traditional getmouse(void). Wrap so call sites are portable.
#if defined(LV_USE_PDCURSES)
inline int lv_getmouse(MEVENT* e) { return nc_getmouse(e); }
#else
inline int lv_getmouse(MEVENT* e) { return getmouse(e); }
#endif
} // namespace

void AppUi::handle_mouse() {
    MEVENT event;
    if (lv_getmouse(&event) != OK) {
        return;
    }
    constexpr int kWheelStep = 3;
    if (event.bstate & BUTTON4_PRESSED) {
        if (focus_ == Focus::Rules) {
            for (int i = 0; i < kWheelStep && rule_cursor_ > 0; ++i) {
                --rule_cursor_;
            }
        } else {
            move_log_lines(-1, kWheelStep);
        }
    } else if (event.bstate & BUTTON5_PRESSED) {
        if (focus_ == Focus::Rules) {
            for (int i = 0; i < kWheelStep && rule_cursor_ + 1 < rules_.size(); ++i) {
                ++rule_cursor_;
            }
        } else {
            move_log_lines(1, kWheelStep);
        }
    }
}

int AppUi::line_wrap_rows(LineNumber line, int content_width) const {
    if (content_width <= 0 || line >= index_.line_count()) {
        return 1;
    }
    const std::string_view sv = index_.line(line);
    return std::max(1, (utf8_display_width(sv) + content_width - 1) / content_width);
}

void AppUi::keep_cursor_visible(int content_width, int content_height) {
    if (index_.line_count() == 0 || content_height <= 0) {
        log_top_line_ = 0;
        log_cursor_ = 0;
        return;
    }
    if (!line_visible(log_cursor_)) {
        log_cursor_ = next_visible_line(log_cursor_);
        if (!line_visible(log_cursor_)) {
            log_cursor_ = previous_visible_line(log_cursor_);
        }
    }

    if (log_top_line_ > log_cursor_) {
        log_top_line_ = log_cursor_;
    }
    while (log_top_line_ < index_.line_count() && !line_visible(log_top_line_)) {
        log_top_line_ = next_visible_line(log_top_line_);
        if (!line_visible(log_top_line_)) {
            break;
        }
    }

    int rows_to_cursor = 0;
    bool saw_cursor = false;
    for (LineNumber line = log_top_line_; line < index_.line_count() && rows_to_cursor < content_height; ++line) {
        if (!line_visible(line)) {
            continue;
        }
        if (line == log_cursor_) {
            saw_cursor = true;
            rows_to_cursor += line_wrap_rows(line, content_width);
            break;
        }
        rows_to_cursor += line_wrap_rows(line, content_width);
    }

    while ((!saw_cursor || rows_to_cursor > content_height) && log_top_line_ < log_cursor_) {
        log_top_line_ = next_visible_line(log_top_line_);
        rows_to_cursor = 0;
        saw_cursor = false;
        for (LineNumber line = log_top_line_; line < index_.line_count() && rows_to_cursor <= content_height; ++line) {
            if (!line_visible(line)) {
                continue;
            }
            if (line == log_cursor_) {
                saw_cursor = true;
                rows_to_cursor += line_wrap_rows(line, content_width);
                break;
            }
            rows_to_cursor += line_wrap_rows(line, content_width);
        }
    }

    constexpr int kScrollOff = 5;
    const int scrolloff = std::min(kScrollOff, std::max(0, content_height / 2 - 1));

    // Scroll down if cursor is too close to the bottom
    if (rows_to_cursor > content_height - scrolloff) {
        int target = content_height - scrolloff - 1;
        if (target < 0) target = 0;
        while (rows_to_cursor > target && log_top_line_ < log_cursor_) {
            log_top_line_ = next_visible_line(log_top_line_);
            rows_to_cursor = 0;
            for (LineNumber line = log_top_line_; line < index_.line_count() && rows_to_cursor <= content_height; ++line) {
                if (!line_visible(line)) continue;
                if (line == log_cursor_) {
                    rows_to_cursor += line_wrap_rows(line, content_width);
                    break;
                }
                rows_to_cursor += line_wrap_rows(line, content_width);
            }
        }
    }

    // Scroll up if cursor is too close to the top (but not at file start)
    if (rows_to_cursor < scrolloff) {
        int needed = scrolloff - rows_to_cursor;
        for (int i = 0; i < needed; ++i) {
            LineNumber prev = previous_visible_line(log_top_line_);
            if (prev == log_top_line_) break;
            log_top_line_ = prev;
        }
    }
}

int AppUi::line_number_width() const {
    std::size_t value = std::max<std::size_t>(1, index_.line_count());
    int digits = 1;
    while (value >= 10) {
        value /= 10;
        ++digits;
    }
    return digits + 1;
}

std::string AppUi::active_literal_highlight() const {
    if (rules_.empty()) {
        return {};
    }
    const std::size_t start = std::min(rule_cursor_, rules_.size() - 1);
    for (std::size_t offset = 0; offset < rules_.size(); ++offset) {
        const std::size_t index = (start + offset) % rules_.size();
        for (const lv::RuleSegment& seg : rules_[index].segments()) {
            if (seg.type == RuleMatchType::Literal) {
                return seg.pattern;
            }
        }
    }
    return {};
}

void AppUi::begin_search() {
    search_active_ = true;
    search_backward_ = false;
    search_orig_cursor_ = log_cursor_;
    editing_rule_ = false;
    adding_rule_ = false;
    editor_.start("/", "");
}

void AppUi::begin_search_backward() {
    search_active_ = true;
    search_backward_ = true;
    search_orig_cursor_ = log_cursor_;
    editing_rule_ = false;
    adding_rule_ = false;
    editor_.start("?", "");
}

void AppUi::handle_search_submit() {
    std::string text = editor_.text();
    if (text.size() >= 2) {
        const char first = text.front();
        const char last = text.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            text = text.substr(1, text.size() - 2);
        }
    }
    if (text.empty()) {
        cancel_search_job();
        search_active_ = false;
        search_pattern_.clear();
        search_regex_.reset();
        search_matches_ = BitArray();
        search_status_.clear();
        return;
    }

    try {
        search_regex_ = std::make_unique<boost::regex>(text);
        search_pattern_ = text;
    } catch (const boost::regex_error& e) {
        search_status_ = "invalid regex: " + text;
        search_active_ = false;
        search_regex_.reset();
        search_pattern_.clear();
        search_matches_ = BitArray();
        return;
    }

    cancel_search_job();
    search_active_ = false;
    search_matches_ = BitArray(index_.line_count(), false);

    auto state = std::make_shared<SearchJobState>();
    state->done = false;
    state->matches = BitArray(index_.line_count(), false);
    search_job_state_ = state;

    LineIndex index = index_;
    boost::regex regex = *search_regex_;

    search_thread_ = std::thread([state, index = std::move(index), regex = std::move(regex)]() mutable {
        BitArray matches(index.line_count(), false);
        for (LineNumber line = 0; line < index.line_count(); ++line) {
            const std::string_view line_view = index.line(line);
            boost::cmatch m;
            if (boost::regex_search(line_view.data(), line_view.data() + line_view.size(), m, regex)) {
                matches.set(line, true);
            }
        }
        std::lock_guard<std::mutex> lock(state->mutex);
        state->matches = std::move(matches);
        state->done = true;
    });

    search_status_ = "searching...";
}

void AppUi::incsearch() {
    std::string text = editor_.text();
    const int gen = ++incsearch_gen_;

    if (text.empty()) {
        log_cursor_ = search_orig_cursor_;
        search_status_.clear();
        return;
    }

    LineNumber orig_cursor = search_orig_cursor_;
    const char* data = index_.data();
    std::size_t data_size = index_.size();
    LineNumber total = index_.line_count();

    auto result = std::make_shared<IncsearchResult>();
    result->generation = gen;
    incsearch_result_ = result;

    if (incsearch_thread_.joinable()) {
        incsearch_thread_.detach();
    }

    const bool backward = search_backward_;

    incsearch_thread_ = std::thread([result, text = std::move(text), data, data_size, total,
                                      orig_cursor, gen, backward, filter_bitmap = filter_bitmap_,
                                      starts = std::make_shared<std::vector<ByteOffset>>(), 
                                      &starts_ref = index_.starts()]() mutable {
        *starts = starts_ref;

        boost::regex re;
        try {
            re = boost::regex(text);
        } catch (const boost::regex_error&) {
            std::lock_guard<std::mutex> lock(result->mutex);
            result->status = std::string(1, backward ? '?' : '/') + text + " [invalid]";
            result->ready = true;
            return;
        }

        const std::string prefix(1, backward ? '?' : '/');

        if (backward) {
            LineNumber c = orig_cursor;
            while (true) {
                if (!filter_bitmap || filter_bitmap->get(c)) {
                    ByteOffset start = (*starts)[c];
                    ByteOffset end = (c + 1 < starts->size()) ? (*starts)[c + 1] : data_size;
                    std::string_view sv(data + start, end - start);
                    boost::cmatch m;
                    if (boost::regex_search(sv.data(), sv.data() + sv.size(), m, re)) {
                        std::lock_guard<std::mutex> lock(result->mutex);
                        result->cursor = c;
                        result->found = true;
                        result->ready = true;
                        return;
                    }
                }
                if (c == 0) break;
                --c;
            }

            for (LineNumber c = total - 1; c > orig_cursor; --c) {
                if (filter_bitmap && !filter_bitmap->get(c)) continue;
                ByteOffset start = (*starts)[c];
                ByteOffset end = (c + 1 < starts->size()) ? (*starts)[c + 1] : data_size;
                std::string_view sv(data + start, end - start);
                boost::cmatch m;
                if (boost::regex_search(sv.data(), sv.data() + sv.size(), m, re)) {
                    std::lock_guard<std::mutex> lock(result->mutex);
                    result->cursor = c;
                    result->found = true;
                    result->ready = true;
                    return;
                }
            }
        } else {
            for (LineNumber c = orig_cursor; c < total; ++c) {
                if (filter_bitmap && !filter_bitmap->get(c)) continue;
                ByteOffset start = (*starts)[c];
                ByteOffset end = (c + 1 < starts->size()) ? (*starts)[c + 1] : data_size;
                std::string_view sv(data + start, end - start);
                boost::cmatch m;
                if (boost::regex_search(sv.data(), sv.data() + sv.size(), m, re)) {
                    std::lock_guard<std::mutex> lock(result->mutex);
                    result->cursor = c;
                    result->found = true;
                    result->ready = true;
                    return;
                }
            }

            for (LineNumber c = 0; c < orig_cursor; ++c) {
                if (filter_bitmap && !filter_bitmap->get(c)) continue;
                ByteOffset start = (*starts)[c];
                ByteOffset end = (c + 1 < starts->size()) ? (*starts)[c + 1] : data_size;
                std::string_view sv(data + start, end - start);
                boost::cmatch m;
                if (boost::regex_search(sv.data(), sv.data() + sv.size(), m, re)) {
                    std::lock_guard<std::mutex> lock(result->mutex);
                    result->cursor = c;
                    result->found = true;
                    result->ready = true;
                    return;
                }
            }
        }

        std::lock_guard<std::mutex> lock(result->mutex);
        result->status = prefix + text + " [not found]";
        result->ready = true;
    });
}

void AppUi::poll_incsearch() {
    if (!incsearch_result_) return;

    bool ready = false;
    {
        std::lock_guard<std::mutex> lock(incsearch_result_->mutex);
        ready = incsearch_result_->ready;
    }

    if (!ready) return;

    if (incsearch_result_->generation != incsearch_gen_) {
        incsearch_result_.reset();
        return;
    }

    if (incsearch_result_->found) {
        log_cursor_ = incsearch_result_->cursor;
        search_status_.clear();
        if (editor_.active()) {
            render_log();
        }
    } else {
        log_cursor_ = search_orig_cursor_;
        search_status_ = incsearch_result_->status;
    }

    incsearch_result_.reset();
    dirty_ = true;
}

void AppUi::build_search_bitmap() {
    if (!search_regex_) {
        search_matches_ = BitArray();
        return;
    }

    search_matches_ = BitArray(index_.line_count(), false);
    for (LineNumber line = 0; line < index_.line_count(); ++line) {
        const std::string_view line_view = index_.line(line);
        boost::cmatch m;
        if (boost::regex_search(line_view.data(), line_view.data() + line_view.size(), m, *search_regex_)) {
            search_matches_.set(line, true);
        }
    }
}

void AppUi::poll_search_job() {
    if (!search_job_state_) {
        return;
    }

    bool done = false;
    {
        std::lock_guard<std::mutex> lock(search_job_state_->mutex);
        done = search_job_state_->done;
    }

    if (!done) {
        return;
    }

    if (search_thread_.joinable()) {
        search_thread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(search_job_state_->mutex);
        search_matches_ = std::move(search_job_state_->matches);
    }

    search_job_state_.reset();

    const bool backward = search_backward_;
    const std::size_t match_count = search_matches_.count_ones();
    search_status_ = "search: " + std::string(1, backward ? '?' : '/') + search_pattern_
                   + (backward ? "? " : "/ ") + "matched " + std::to_string(match_count) + " lines";

    if (match_count > 0 && !search_matches_.get(log_cursor_)) {
        if (backward) {
            jump_to_previous_match();
        } else {
            jump_to_next_match();
        }
    }
    dirty_ = true;
}

void AppUi::wait_for_search() {
    if (!search_job_state_) {
        return;
    }
    if (search_thread_.joinable()) {
        search_thread_.join();
    }
    poll_search_job();
}

void AppUi::cancel_search_job() {
    if (search_thread_.joinable()) {
        search_thread_.join();
    }
    search_job_state_.reset();
}

void AppUi::begin_help() {
    help_active_ = true;
    help_scroll_ = 0;
    recreate_windows();
}

void AppUi::close_help() {
    help_active_ = false;
    help_scroll_ = 0;
    if (help_window_ != nullptr) {
        delwin(help_window_);
        help_window_ = nullptr;
    }
}

void AppUi::handle_help_key(int key) {
    switch (key) {
    case 27:
        close_help();
        break;
    case 'k':
    case KEY_UP:
        if (help_scroll_ > 0) {
            --help_scroll_;
        }
        break;
    case 'j':
    case KEY_DOWN:
        ++help_scroll_;
        break;
    case 'g':
    case KEY_HOME:
        help_scroll_ = 0;
        break;
    case 'G':
    case KEY_END:
        help_scroll_ = static_cast<std::size_t>(-1);
        break;
    case KEY_NPAGE:
    case 6:
        help_scroll_ += 10;
        break;
    case KEY_PPAGE:
    case 2:
        help_scroll_ = help_scroll_ > 10 ? help_scroll_ - 10 : 0;
        break;
    case 'q':
        close_help();
        break;
    }
    dirty_ = true;
}

void AppUi::render_help() {
    static const std::vector<std::string> lines = {
        " KEYBINDINGS",
        "",
        " Log navigation",
        "   j / Down          next visible line",
        "   k / Up            previous visible line",
        "   g / Home          first visible line",
        "   G / End           last visible line",
        "   PgDn / Ctrl-F     page down",
        "   PgUp / Ctrl-B     page up",
        "   Mouse wheel       scroll 3 lines (focused window)",
        "   Tab               switch focus",
        "   Space             toggle filters window",
        "   /                 search (regex), Enter to submit",
        "   ?                 search backward (regex), Enter to submit",
        "   n / N             next / previous search match",
        "",
        " Rules window (focus with Tab)",
        "   j / k / Up / Down navigate rules",
        "   Enter             edit rule",
        "   a / i / A / I     insert rule (after/before/end/start)",
        "   x / d             delete rule",
        "   [ / ]             move rule up / down",
        "   -                 toggle rule enabled / disabled",
        "   Space / Esc       hide rules window",
        "",
        " Rule syntax",
        "   s/h  PATTERN    show/hide lines matching PATTERN",
        "   s/h  /regex/    show/hide lines matching regex",
        "   si/hi /regex/   case-insensitive regex",
        "   si/hi PATTERN   case-insensitive literal",
        "   s/h  A|B|/C/    OR: mix literal + regex",
        "   sl/hl N [M]      show/hide line N to M (to end)",
        "   sl/hl -N         show/hide last N lines",
        "   sl/hl -N -M      show/hide from Nth to Mth last",
        "   -s /...          disable rule (- prefix)",
        "",
        " Commands (type : to enter)",
        "   :o / :e [file]   open log file (:o / :e alone reloads)",
        "   :w <file>         save filtered lines to file",
        "   :r <file>         load rules from file",
        "   :wr [file]        write rules to file",
        "   :h / :help       show this help",
        "   :q                quit",
    };

    const int screen_rows = std::max(3, screen_.rows());
    const int screen_cols = std::max(20, screen_.cols());
    const int popup_w = std::min(70, screen_cols - 4);
    const int popup_h = std::min(static_cast<int>(lines.size()) + 2, screen_rows - 4);
    const int popup_y = (screen_rows - popup_h) / 2;
    const int popup_x = (screen_cols - popup_w) / 2;

    if (help_window_ == nullptr) {
        help_window_ = newwin(popup_h, popup_w, popup_y, popup_x);
        keypad(help_window_, TRUE);
    }

    const int total_lines = static_cast<int>(lines.size());
    const int visible = popup_h - 2;
    if (help_scroll_ > static_cast<std::size_t>(total_lines - visible)) {
        help_scroll_ = total_lines > visible ? static_cast<std::size_t>(total_lines - visible) : 0;
    }

    werase(help_window_);
    box(help_window_, 0, 0);
    if (focus_ == Focus::Log) {
        wattron(help_window_, COLOR_PAIR(1));
    }
    mvwprintw(help_window_, 0, 1, " HELP ");
    if (focus_ == Focus::Log) {
        wattroff(help_window_, COLOR_PAIR(1));
    }

    for (int i = 0; i < visible; ++i) {
        const std::size_t line_idx = help_scroll_ + static_cast<std::size_t>(i);
        if (line_idx >= lines.size()) {
            break;
        }
        const std::string& text = lines[line_idx];
        const int max_len = popup_w - 3;
        const std::size_t len = utf8_byte_at_column(text, max_len);
        mvwaddnstr(help_window_, i + 1, 1, text.c_str(), len);
    }

    wnoutrefresh(help_window_);
}

void AppUi::jump_to_next_match() {
    if (!search_regex_ || index_.line_count() == 0) {
        return;
    }
    if (search_job_state_) {
        for (LineNumber c = log_cursor_ + 1; c < index_.line_count(); ++c) {
            if (!line_visible(c)) continue;
            boost::cmatch m;
            std::string_view sv = index_.line(c);
            if (boost::regex_search(sv.data(), sv.data() + sv.size(), m, *search_regex_)) {
                log_cursor_ = c;
                return;
            }
        }
        return;
    }
    const LineNumber next = next_search_match(log_cursor_);
    if (next != log_cursor_) {
        log_cursor_ = next;
    }
}

void AppUi::jump_to_previous_match() {
    if (!search_regex_ || index_.line_count() == 0) {
        return;
    }
    if (search_job_state_) {
        LineNumber c = log_cursor_ > 0 ? log_cursor_ - 1 : 0;
        while (true) {
            if (line_visible(c)) {
                boost::cmatch m;
                std::string_view sv = index_.line(c);
                if (boost::regex_search(sv.data(), sv.data() + sv.size(), m, *search_regex_)) {
                    log_cursor_ = c;
                    return;
                }
            }
            if (c == 0) break;
            --c;
        }
        return;
    }
    const LineNumber prev = previous_search_match(log_cursor_);
    if (prev != log_cursor_) {
        log_cursor_ = prev;
    }
}

LineNumber AppUi::next_search_match(LineNumber line) const {
    if (index_.line_count() == 0) {
        return 0;
    }
    for (LineNumber candidate = std::min(line + 1, index_.line_count() - 1); candidate < index_.line_count();
         ++candidate) {
        if (line_visible(candidate) && search_matches_.get(candidate)) {
            return candidate;
        }
    }
    return line;
}

LineNumber AppUi::previous_search_match(LineNumber line) const {
    if (index_.line_count() == 0 || line == 0) {
        return 0;
    }
    LineNumber candidate = line - 1;
    while (candidate > 0) {
        if (line_visible(candidate) && search_matches_.get(candidate)) {
            return candidate;
        }
        --candidate;
    }
    if (line_visible(0) && search_matches_.get(0)) {
        return 0;
    }
    return line;
}

std::vector<AppUi::HighlightMatch> AppUi::find_line_matches(LineNumber line) const {
    std::vector<HighlightMatch> matches;
    if (!search_regex_ || line >= index_.line_count()) {
        return matches;
    }
    const std::string_view line_view = index_.line(line);
    boost::cregex_iterator it(line_view.data(), line_view.data() + line_view.size(), *search_regex_);
    boost::cregex_iterator end;
    for (; it != end; ++it) {
        matches.push_back({static_cast<std::size_t>(it->position()), static_cast<std::size_t>(it->length())});
    }
    return matches;
}

void AppUi::render_log_chunk(int row,
                             int col,
                             std::string_view chunk,
                             const std::string& highlight,
                             bool selected) {
    auto wadd_sanitized = [](WINDOW* win, const char* data, int len) {
        int i = 0;
        while (i < len) {
            unsigned char c = static_cast<unsigned char>(data[i]);
            if (c == '\t') {
                waddch(win, ' ');
                ++i;
            } else if (c < 32 || c == 127) {
                waddch(win, '.');
                ++i;
            } else {
                int start = i;
                while (i < len) {
                    unsigned char ch = static_cast<unsigned char>(data[i]);
                    if (ch < 32 || ch == 127 || ch == '\t') break;
                    ++i;
                }
                if (i > start) {
                    waddnstr(win, data + start, i - start);
                }
            }
        }
    };

    int x = col;
    std::size_t pos = 0;
    while (pos < chunk.size()) {
        const std::size_t match = highlight.empty() || selected ? std::string_view::npos : chunk.find(highlight, pos);
        if (match == std::string_view::npos) {
            wmove(log_window_, row, x);
            wadd_sanitized(log_window_, chunk.data() + pos, static_cast<int>(chunk.size() - pos));
            return;
        }
        if (match > pos) {
            const int seg_len = static_cast<int>(match - pos);
            wmove(log_window_, row, x);
            wadd_sanitized(log_window_, chunk.data() + pos, seg_len);
            x += seg_len;
        }
        wattron(log_window_, COLOR_PAIR(2) | A_BOLD);
        wmove(log_window_, row, x);
        const int hl_len = static_cast<int>(std::min(highlight.size(), chunk.size() - match));
        wadd_sanitized(log_window_, chunk.data() + match, hl_len);
        wattroff(log_window_, COLOR_PAIR(2) | A_BOLD);
        x += static_cast<int>(highlight.size());
        pos = match + highlight.size();
    }
}

} // namespace lv::ui
