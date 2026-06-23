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
    // Enable mouse wheel events. On ncurses, BUTTON4/BUTTON5 is enough.
    // PDCursesMod's GL backend additionally requires MOUSE_WHEEL_SCROLL
    // or the _mouse_key filter silently drops wheel events.
#if defined(LV_USE_PDCURSES)
    mousemask(BUTTON4_PRESSED | BUTTON5_PRESSED | MOUSE_WHEEL_SCROLL, nullptr);
#else
    mousemask(BUTTON4_PRESSED | BUTTON5_PRESSED, nullptr);
#endif
#if defined(LV_USE_PDCURSES)
    // The PDCursesMod GL backend starts with a compile-time cell grid
    // (25x80) that doesn't match the SDL2 window. resize_term(0, 0)
    // queries the window dimensions and recomputes LINES/COLS. It is
    // RARELY called here (screen ctor): initscr has finished, fonts are
    // loaded, and pdc_window exists. On some window managers the initial
    // size isn't yet available through SDL_GetWindowSize, which is why
    // we call it again from AppUi::start() after the event loop has
    // processed at least one SDL_WINDOWEVENT.
    //
    // NOTE: this is a best-effort early resize; the definitive one
    // happens in AppUi::start().
    resize_term(0, 0);
#endif
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
