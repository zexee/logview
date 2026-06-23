// Smoke test: verify PDCursesMod's GL backend renders subwindow content
// and handles basic curses API calls.  Requires an X11/Wayland display.
//
// Build: manually, not part of the regular CMake test targets.
//   c++ -DLV_USE_PDCURSES -DPDC_WIDE -DPDC_FORCE_UTF8 -DCHTYPE_32 \
//       -I src -I third_party/PDCursesMod \
//       -I build_gui/third_party/SDL2/include \
//       -I build_gui/third_party/SDL2/include/SDL2 \
//       -I build_gui/third_party/SDL2/include-config-release/SDL2 \
//       -o /tmp/test_pdc tests/test_pdcurses_gui.cpp \
//       build_gui/libpdcurses_sdl2.a \
//       build_gui/third_party/SDL2_ttf/libSDL2_ttf.a \
//       build_gui/third_party/SDL2_ttf/external/freetype/libfreetype.a \
//       build_gui/third_party/SDL2/libSDL2.a \
//       build_gui/third_party/SDL2/libSDL2main.a \
//       -lpthread -ldl -lm

#include <curses.h>
#include <SDL.h>

#include <cstdio>
#include <thread>
#include <chrono>

extern "C" {
extern SDL_Window* pdc_window;
extern SDL_GLContext pdc_gl_context;
}

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* win = SDL_CreateWindow("pdc-test",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        640, 400, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!win) { std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); return 1; }
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    if (!ctx) { std::fprintf(stderr, "SDL_GL_CreateContext: %s\n", SDL_GetError()); return 1; }

    pdc_window = win;
    pdc_gl_context = ctx;

    std::fprintf(stderr, "[pdc-test] about to initscr\n");
    initscr();
    std::fprintf(stderr, "[pdc-test] initscr done, LINES=%d COLS=%d\n", LINES, COLS);
    raw();
    noecho();
    keypad(stdscr, TRUE);
    timeout(50);
    curs_set(1);
    start_color();
    use_default_colors();
    init_pair(1, COLOR_BLACK, COLOR_CYAN);
    init_pair(2, COLOR_YELLOW, -1);
    init_pair(3, COLOR_GREEN, -1);
    init_pair(4, COLOR_WHITE, -1);
    init_pair(5, COLOR_RED, -1);

    // Create a subwindow and write text to test subwindow rendering.
    WINDOW* sub = newwin(10, 60, 5, 5);
    if (!sub) {
        std::fprintf(stderr, "[pdc-test] newwin FAILED\n");
        goto done;
    }
    mvwaddstr(sub, 0, 0, "PDCursesMod GL backend");
    mvwaddstr(sub, 1, 0, "Subwindow rendering OK");
    wattron(sub, COLOR_PAIR(2));
    mvwaddstr(sub, 2, 0, "YELLOW on default bg");
    wattroff(sub, COLOR_PAIR(2));
    wattron(sub, COLOR_PAIR(1));
    mvwaddstr(sub, 3, 0, "BLACK on CYAN");
    wattroff(sub, COLOR_PAIR(1));
    box(sub, 0, 0);
    wnoutrefresh(sub);
    doupdate();
    std::fprintf(stderr, "[pdc-test] doupdate done — should see subwindow\n");

    // Run a few frames.
    for (int i = 0; i < 120; ++i) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) goto done;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        wnoutrefresh(sub);
        doupdate();
    }

done:
    endwin();
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    std::fprintf(stderr, "[pdc-test] clean exit\n");
    return 0;
}
