#include "ui/screen.h"

namespace lv::ui {

Screen::Screen() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    timeout(50);
    curs_set(1);
    start_color();
    use_default_colors();
    init_pair(1, COLOR_BLACK, COLOR_CYAN);
    init_pair(2, COLOR_YELLOW, -1);
}

Screen::~Screen() {
    endwin();
}

int Screen::rows() const {
    int y = 0;
    int x = 0;
    getmaxyx(stdscr, y, x);
    (void)x;
    return y;
}

int Screen::cols() const {
    int y = 0;
    int x = 0;
    getmaxyx(stdscr, y, x);
    (void)y;
    return x;
}

} // namespace lv::ui
