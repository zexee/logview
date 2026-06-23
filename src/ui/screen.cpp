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
#if !defined(_WIN32) && !defined(LV_USE_PDCURSES)
    set_escdelay(0);
#else
    // PDCursesMod has no set_escdelay; ESC handling differs and doesn't need it.
#endif
    curs_set(1);
    start_color();
    use_default_colors();
    init_pair(1, COLOR_BLACK, COLOR_CYAN);
    init_pair(2, COLOR_YELLOW, -1);
    init_pair(3, COLOR_GREEN, -1);
    init_pair(4, COLOR_WHITE, -1);
    init_pair(5, COLOR_RED, -1);
    // Enable mouse wheel events. BUTTON4 = wheel up, BUTTON5 = wheel down.
    mousemask(BUTTON4_PRESSED | BUTTON5_PRESSED, nullptr);
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
