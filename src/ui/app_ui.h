#pragma once

#include "core/bit_array.h"
#include "core/filter_result.h"
#include "core/line_index.h"
#include "core/mmap_file.h"
#include "core/rule_set.h"
#include "core/types.h"
#include "ui/line_editor.h"
#include "ui/screen.h"

#include <boost/regex.hpp>
#include <memory>
#include <mutex>
#include <cstdint>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace lv::ui {

class AppUi {
public:
    AppUi(MMapFile file, LineIndex index, RuleSet rules, std::string rule_path = {});
    ~AppUi();

    int run();
    void start();          // one-time init (recreate_windows + start_filter_job)
    bool tick();           // advance the event loop once; returns false when run should exit
    void prefill_command(const std::string& cmd);
    void rebuild_layout(); // resize + recreate windows (e.g. after font size change)

private:
    enum class Focus {
        Log,
        Rules,
    };

    void recreate_windows();
    void destroy_windows();
    void render();
    void render_log();
    void render_rules();
    void render_editor();
    int normalize_key(int key);
    void handle_key(int key);
    void handle_editor_submit();
    void handle_command(const std::string& command);
    void open_log_file(const std::string& path);
    void save_filtered_file(const std::string& path);
    void begin_rule_edit();
    void begin_rule_add(std::size_t index);
    void delete_selected_rule();
    void move_selected_rule_up();
    void move_selected_rule_down();
    std::string selected_rule_text() const;
    void keep_rule_cursor_visible();
    void start_filter_job(std::size_t recompute_from = 0);
    static constexpr std::size_t kMergeOnly = static_cast<std::size_t>(-1);
    bool poll_filter_jobs();
    void join_filter_jobs();
    void wait_for_filter_jobs();
    bool line_visible(LineNumber line) const;
    LineNumber first_visible_line() const;
    LineNumber last_visible_line() const;
    LineNumber next_visible_line(LineNumber line) const;
    LineNumber previous_visible_line(LineNumber line) const;
    void move_log_page(int direction);
    void move_log_lines(int direction, int steps);
    void handle_mouse();
    int line_wrap_rows(LineNumber line, int content_width) const;
    void keep_cursor_visible(int content_width, int content_height);
    int line_number_width() const;
    std::string active_literal_highlight() const;

    struct HighlightMatch {
        std::size_t start;
        std::size_t length;
    };

    void begin_search();
    void begin_search_backward();
    void handle_search_submit();
    void incsearch();
    void poll_incsearch();
    void jump_to_next_match();
    void jump_to_previous_match();
    LineNumber next_search_match(LineNumber line) const;
    LineNumber previous_search_match(LineNumber line) const;
    void build_search_bitmap();
    std::vector<HighlightMatch> find_line_matches(LineNumber line) const;
    void poll_search_job();
    void wait_for_search();
    void cancel_search_job();

    void begin_help();
    void close_help();
    void handle_help_key(int key);
    void render_help();
    void render_log_chunk(int row, int col, std::string_view chunk, const std::string& highlight, bool selected);

    Screen screen_;
    MMapFile file_;
    LineIndex index_;
    RuleSet rules_;
    std::string rule_path_;
    const BitArray* filter_bitmap_ = nullptr;
    std::unique_ptr<FilterResult> active_filter_;
    Focus focus_ = Focus::Log;
    bool rules_visible_ = false;
    LineEditor editor_;
    bool running_ = true;
    bool dirty_ = true;
    std::string status_;
    std::string search_status_;
    std::size_t log_cursor_ = 0;
    std::size_t log_top_line_ = 0;
    std::size_t rule_cursor_ = 0;
    std::size_t rule_top_ = 0;
    std::size_t pending_insert_index_ = 0;
    bool editing_rule_ = false;
    bool adding_rule_ = false;
    bool search_active_ = false;
    bool search_backward_ = false;
    std::string search_pattern_;
    std::unique_ptr<boost::regex> search_regex_;
    BitArray search_matches_;
    BitArray mark_bitmap_;
    std::size_t search_orig_cursor_ = 0;
    std::uint64_t next_filter_generation_ = 1;
    std::uint64_t applied_filter_generation_ = 0;

    struct FilterJobState {
        std::mutex mutex;
        bool done = false;
        std::uint64_t generation = 0;
        FilterResult result;
    };

    struct FilterJob {
        std::shared_ptr<FilterJobState> state;
        std::thread thread;
    };

    std::vector<FilterJob> filter_jobs_;

    struct SearchJobState {
        std::mutex mutex;
        bool done = false;
        BitArray matches;
    };

    std::shared_ptr<SearchJobState> search_job_state_;
    std::thread search_thread_;

    struct IncsearchResult {
        std::mutex mutex;
        bool ready = false;
        int generation = 0;
        LineNumber cursor = 0;
        bool found = false;
        std::string status;
    };

    std::shared_ptr<IncsearchResult> incsearch_result_;
    std::thread incsearch_thread_;
    int incsearch_gen_ = 0;

    bool help_active_ = false;
    std::size_t help_scroll_ = 0;

    WINDOW* log_window_ = nullptr;
    WINDOW* rules_window_ = nullptr;
    WINDOW* editor_window_ = nullptr;
    WINDOW* help_window_ = nullptr;
    Rect log_rect_;
    Rect rules_rect_;
    Rect editor_rect_;
};

} // namespace lv::ui
