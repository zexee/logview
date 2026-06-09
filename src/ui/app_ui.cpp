#include "ui/app_ui.h"

#include "core/filter_engine.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>
#include <string_view>
#include <utility>

namespace lv::ui {

AppUi::AppUi(MMapFile file, LineIndex index, RuleSet rules, std::string rule_path)
    : file_(std::move(file)), index_(std::move(index)), rules_(std::move(rules)), rule_path_(std::move(rule_path)) {}

AppUi::~AppUi() {
    join_filter_jobs();
}

int AppUi::run() {
    recreate_windows();
    start_filter_job();
    while (running_) {
        if (poll_filter_jobs()) {
            dirty_ = true;
        }
        if (dirty_) {
            render();
            dirty_ = false;
        }
        int key = getch();
        if (key != ERR) {
            key = normalize_key(key);
            handle_key(key);
            dirty_ = true;
        }
    }
    destroy_windows();
    return 0;
}

void AppUi::recreate_windows() {
    destroy_windows();

    const int rows = std::max(3, screen_.rows());
    const int cols = std::max(20, screen_.cols());
    const int editor_height = 1;
    const int content_height = rows - editor_height;
    const int separator_height = content_height > 3 ? 1 : 0;
    const int split_content_height = std::max(1, content_height - separator_height);
    const int rules_height = std::clamp(split_content_height / 3, 2, std::max(2, split_content_height - 2));
    const int log_height = std::max(1, split_content_height - rules_height);

    log_rect_ = Rect{0, 0, log_height, cols};
    rules_rect_ = Rect{log_height + separator_height, 0, rules_height, cols};
    editor_rect_ = Rect{rows - 1, 0, editor_height, cols};

    log_window_ = newwin(log_rect_.height, log_rect_.width, log_rect_.y, log_rect_.x);
    rules_window_ = newwin(rules_rect_.height, rules_rect_.width, rules_rect_.y, rules_rect_.x);
    editor_window_ = newwin(editor_rect_.height, editor_rect_.width, editor_rect_.y, editor_rect_.x);

    keypad(log_window_, TRUE);
    keypad(rules_window_, TRUE);
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
}

void AppUi::render() {
    render_log();
    render_rules();
    render_editor();
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
        mvwaddnstr(log_window_, 0, 0, path.c_str(), title_width);
    }
    if (focus_ == Focus::Log && !editor_.active()) {
        wattroff(log_window_, COLOR_PAIR(1));
    }
    if (rules_rect_.y > log_rect_.y + log_rect_.height && log_rect_.width > 0) {
        mvwhline(stdscr, log_rect_.height, 0, ACS_HLINE, log_rect_.width);
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
            const std::size_t offset = static_cast<std::size_t>(wrap * content_width);
            const std::size_t remaining = offset < line.size() ? line.size() - offset : 0;
            const int chunk_len = static_cast<int>(std::min<std::size_t>(remaining, content_width));
            const std::string_view chunk(line.data() + offset, static_cast<std::size_t>(chunk_len));

            if (selected) {
                wattron(log_window_, A_REVERSE);
            }
            if (wrap == 0) {
                mvwprintw(log_window_, row, 0, "%*zu ", number_width - 1, line_number + 1);
            } else {
                mvwprintw(log_window_, row, 0, "%*s ", number_width - 1, "");
            }

            if (chunk_len > 0) {
                render_log_chunk(row, number_width, chunk, highlight, selected);
            }
            const int used = number_width + chunk_len;
            if (used < log_rect_.width) {
                mvwprintw(log_window_, row, used, "%*s", log_rect_.width - used, "");
            }
            if (selected) {
                wattroff(log_window_, A_REVERSE);
            }

            if (search_regex_ && !search_matches.empty()) {
                for (const HighlightMatch& m : search_matches) {
                    if (m.start < offset + chunk_len && m.start + m.length > offset) {
                        const std::size_t chunk_start =
                            m.start > offset ? m.start - offset : 0;
                        const std::size_t chunk_end =
                            std::min(m.start + m.length, offset + chunk_len) - offset;
                        const std::size_t match_chunk_len = chunk_end - chunk_start;
                        const int draw_col = number_width + static_cast<int>(chunk_start);
                        wattron(log_window_, COLOR_PAIR(3) | A_BOLD);
                        wmove(log_window_, row, draw_col);
                        for (std::size_t i = 0; i < match_chunk_len; ++i) {
                            const std::size_t chunk_idx = chunk_start + i;
                            if (chunk_idx < chunk.size()) {
                                waddch(log_window_, printable_char(chunk[chunk_idx]));
                            }
                        }
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
    if (focus_ == Focus::Rules && !editor_.active()) {
        wattron(rules_window_, COLOR_PAIR(1));
    }
    mvwprintw(rules_window_, 0, 0, " rules ");
    if (focus_ == Focus::Rules && !editor_.active()) {
        wattroff(rules_window_, COLOR_PAIR(1));
    }

    const int inner_height = std::max(0, rules_rect_.height - 1);
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
        mvwprintw(rules_window_, row + 1, 0, "%2zu  %-*s", index + 1, rules_rect_.width - 6, text.c_str());
        if (index == rule_cursor_) {
            wattroff(rules_window_, A_REVERSE);
        }
    }
    if (rules_.empty()) {
        mvwprintw(rules_window_, 1, 0, "no rules");
    }
    wnoutrefresh(rules_window_);
}

void AppUi::render_editor() {
    editor_.render(editor_window_, editor_rect_.width);
    if (!editor_.active() && !status_.empty()) {
        const int width = std::max(1, editor_rect_.width);
        const int max_status_width = std::max(0, width - 8);
        const int status_width = std::min<int>(max_status_width, static_cast<int>(status_.size()));
        const int start = std::max(0, width - status_width);
        const std::string_view visible_status(status_.data() + status_.size() - static_cast<std::size_t>(status_width),
                                              static_cast<std::size_t>(status_width));
        mvwaddnstr(editor_window_, 0, start, visible_status.data(), static_cast<int>(visible_status.size()));
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
    if (editor_.active()) {
        const LineEditorEvent event = editor_.handle_key(key);
        if (event == LineEditorEvent::Submitted) {
            handle_editor_submit();
        } else if (event == LineEditorEvent::Canceled) {
            editing_rule_ = false;
            adding_rule_ = false;
            if (search_active_) {
                search_active_ = false;
                status_ = "search canceled";
            } else {
                status_ = "canceled";
            }
        }
        return;
    }

    switch (key) {
    case KEY_RESIZE:
        recreate_windows();
        break;
    case '\t':
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
    case 'q':
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
        rule_cursor_ = insert_index;
    } else {
        rules_.replace(rule_cursor_, std::move(parsed.rule));
    }
    status_ = "rule updated; filtering";
    editing_rule_ = false;
    adding_rule_ = false;
    start_filter_job();
}

void AppUi::handle_command(const std::string& command) {
    std::istringstream input(command);
    std::string name;
    input >> name;

    if (name == "q" || name == "quit") {
        running_ = false;
        return;
    }
    if (name == "rules") {
        std::string path;
        input >> path;
        if (path.empty()) {
            status_ = "usage: :rules <rule_file>";
            return;
        }
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
        start_filter_job();
        return;
    }
    if (name == "write-rules") {
        std::string path;
        input >> path;
        if (path.empty()) {
            path = rule_path_;
        }
        if (path.empty()) {
            status_ = "usage: :write-rules <rule_file>";
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
    if (name == "save-filtered") {
        std::string path;
        input >> path;
        if (path.empty()) {
            status_ = "usage: :save-filtered <output_file>";
            return;
        }
        save_filtered_file(path);
        return;
    }
    if (name == "open") {
        std::string path;
        input >> path;
        if (path.empty()) {
            status_ = "usage: :open <log_file>";
            return;
        }
        open_log_file(path);
        return;
    }
    status_ = "unknown command: " + command;
}

void AppUi::open_log_file(const std::string& path) {
    join_filter_jobs();

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
    search_matches_ = BitArray();
    log_cursor_ = 0;
    log_top_line_ = 0;
    status_ = "opened; filtering: " + path;
    start_filter_job();
}

void AppUi::save_filtered_file(const std::string& path) {
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
    editor_.start("", "ss ");
}

void AppUi::delete_selected_rule() {
    if (rules_.empty()) {
        status_ = "no rule to delete";
        return;
    }
    rules_.remove(rule_cursor_);
    if (rule_cursor_ >= rules_.size() && rule_cursor_ > 0) {
        --rule_cursor_;
    }
    if (rule_top_ > rule_cursor_) {
        rule_top_ = rule_cursor_;
    }
    status_ = "rule deleted; filtering";
    start_filter_job();
}

void AppUi::move_selected_rule_up() {
    if (rules_.move_up(rule_cursor_)) {
        --rule_cursor_;
        status_ = "rule moved; filtering";
        start_filter_job();
    }
}

void AppUi::move_selected_rule_down() {
    if (rules_.move_down(rule_cursor_)) {
        ++rule_cursor_;
        status_ = "rule moved; filtering";
        start_filter_job();
    }
}

std::string AppUi::selected_rule_text() const {
    if (rules_.empty()) {
        return "ss ";
    }
    return rules_[rule_cursor_].serialize();
}

void AppUi::keep_rule_cursor_visible() {
    const std::size_t visible_rows = rules_rect_.height > 1 ? static_cast<std::size_t>(rules_rect_.height - 1) : 0;
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

void AppUi::start_filter_job() {
    auto state = std::make_shared<FilterJobState>();
    state->generation = next_filter_generation_++;
    RuleSet rules = rules_;
    LineIndex index = index_;

    std::thread thread([state, rules = std::move(rules), index = std::move(index)]() mutable {
        FilterEngine engine;
        FilterResult result = engine.run(index, rules);
        std::lock_guard<std::mutex> lock(state->mutex);
        state->result = std::move(result);
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
                log_top_line_ = log_cursor_;
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
    for (int i = 0; i < steps; ++i) {
        const LineNumber next = direction > 0 ? next_visible_line(log_cursor_) : previous_visible_line(log_cursor_);
        if (next == log_cursor_) {
            break;
        }
        log_cursor_ = next;
    }
}

int AppUi::line_wrap_rows(LineNumber line, int content_width) const {
    if (content_width <= 0 || line >= index_.line_count()) {
        return 1;
    }
    const std::size_t size = index_.line(line).size();
    return std::max(1, static_cast<int>((size + static_cast<std::size_t>(content_width) - 1) /
                                        static_cast<std::size_t>(content_width)));
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
        if (rules_[index].type() == RuleMatchType::Literal) {
            return rules_[index].pattern();
        }
    }
    return {};
}

void AppUi::begin_search() {
    search_active_ = true;
    editing_rule_ = false;
    adding_rule_ = false;
    editor_.start("/", "");
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
        // empty pattern: clear search
        search_active_ = false;
        search_pattern_.clear();
        search_regex_.reset();
        search_matches_ = BitArray();
        status_.clear();
        return;
    }

    try {
        search_regex_ = std::make_unique<std::regex>(text);
        search_pattern_ = text;
    } catch (const std::regex_error& e) {
        status_ = "invalid regex: " + text;
        search_active_ = false;
        search_regex_.reset();
        search_pattern_.clear();
        search_matches_ = BitArray();
        return;
    }

    build_search_bitmap();
    search_active_ = false;

    const std::size_t match_count = search_matches_.count_ones();
    status_ = "search: /" + text + "/ matched " + std::to_string(match_count) + " lines";

    if (match_count > 0 && !search_matches_.get(log_cursor_)) {
        jump_to_next_match();
    }
}

void AppUi::build_search_bitmap() {
    if (!search_regex_) {
        search_matches_ = BitArray();
        return;
    }

    search_matches_ = BitArray(index_.line_count(), false);
    for (LineNumber line = 0; line < index_.line_count(); ++line) {
        const std::string_view line_view = index_.line(line);
        std::cmatch m;
        if (std::regex_search(line_view.data(), line_view.data() + line_view.size(), m, *search_regex_)) {
            search_matches_.set(line, true);
        }
    }
}

void AppUi::jump_to_next_match() {
    if (!search_regex_ || index_.line_count() == 0) {
        return;
    }
    const LineNumber next = next_search_match(log_cursor_);
    if (next != log_cursor_) {
        log_cursor_ = next;
        log_top_line_ = log_cursor_;
    }
}

void AppUi::jump_to_previous_match() {
    if (!search_regex_ || index_.line_count() == 0) {
        return;
    }
    const LineNumber prev = previous_search_match(log_cursor_);
    if (prev != log_cursor_) {
        log_cursor_ = prev;
        log_top_line_ = log_cursor_;
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
    std::cregex_iterator it(line_view.data(), line_view.data() + line_view.size(), *search_regex_);
    std::cregex_iterator end;
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
    int x = col;
    std::size_t pos = 0;
    while (pos < chunk.size()) {
        const std::size_t match = highlight.empty() || selected ? std::string_view::npos : chunk.find(highlight, pos);
        if (match == std::string_view::npos) {
            wmove(log_window_, row, x);
            while (pos < chunk.size()) {
                waddch(log_window_, printable_char(chunk[pos]));
                ++pos;
            }
            return;
        }
        if (match > pos) {
            wmove(log_window_, row, x);
            while (pos < match) {
                waddch(log_window_, printable_char(chunk[pos]));
                ++x;
                ++pos;
            }
        }
        wattron(log_window_, COLOR_PAIR(2) | A_BOLD);
        wmove(log_window_, row, x);
        for (std::size_t i = 0; i < highlight.size() && match + i < chunk.size(); ++i) {
            waddch(log_window_, printable_char(chunk[match + i]));
        }
        wattroff(log_window_, COLOR_PAIR(2) | A_BOLD);
        x += static_cast<int>(highlight.size());
        pos = match + highlight.size();
    }
}

char AppUi::printable_char(char ch) {
    const unsigned char value = static_cast<unsigned char>(ch);
    if (value == '\t') {
        return ' ';
    }
    if (value < 32 || value == 127) {
        return '.';
    }
    return ch;
}

} // namespace lv::ui
