#include "ui/screen.h"

#include <clocale>

namespace lv::ui {

Screen::Screen() {
    std::setlocale(LC_ALL, "");
    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    timeout(50);
    set_escdelay(0);
    curs_set(1);
    start_color();
    use_default_colors();
    init_pair(1, COLOR_BLACK, COLOR_CYAN);
    init_pair(2, COLOR_YELLOW, -1);
    init_pair(3, COLOR_GREEN, -1);
    init_pair(4, COLOR_WHITE, -1);
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
