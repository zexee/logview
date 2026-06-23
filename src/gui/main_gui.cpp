// Entry point for the lv-gui binary: hosts the lv TUI inside an SDL2 window
// with an OpenGL context. PDCursesMod's GL backend draws the TUI directly.
//
// Architecture:
//   - SDL2 creates the window with an OpenGL 3.3 context.
//   - PDCursesMod's gl backend attaches to the same window via pdc_window
//     and renders the TUI through the OpenGL context.
//   - An SDL event filter intercepts Ctrl+=/- before they reach PDCursesMod's
//     internal SDL_PollEvent, adjusting the font size.
//   - AppUi::tick() is driven once per frame; its getch() pumps SDL events
//     through PDCursesMod's internal event handler.

#include "ui/app_ui.h"
#include "core/config.h"

#include <SDL.h>
#include <SDL_ttf.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

extern "C" {
extern SDL_Window* pdc_window;
extern SDL_GLContext pdc_gl_context;
extern int pdc_font_size;
extern TTF_Font* pdc_ttffont;
extern int pdc_fheight, pdc_fwidth;
}

namespace {

void usage(const char* program) {
    std::fprintf(stderr, "Usage: %s <log_file> [rule_file]\n", program);
}

struct GuiState {
    lv::ui::AppUi* app_ui = nullptr;
    bool quit = false;
};

void adjust_font_now(lv::ui::AppUi& app_ui, int delta) {
    if (pdc_font_size <= 0) pdc_font_size = 18;
    pdc_font_size = std::max(6, std::min(72, pdc_font_size + delta));

    if (pdc_ttffont) {
        TTF_CloseFont(pdc_ttffont);
        pdc_ttffont = nullptr;
    }

    const char* fname = std::getenv("PDC_FONT");
    if (fname) pdc_ttffont = TTF_OpenFont(fname, pdc_font_size);
    if (!pdc_ttffont) {
#ifdef _WIN32
        fname = "C:/Windows/Fonts/consola.ttf";
#elif __APPLE__
        fname = "/System/Library/Fonts/Menlo.ttc";
#else
        fname = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf";
#endif
        pdc_ttffont = TTF_OpenFont(fname, pdc_font_size);
    }
    if (pdc_ttffont) {
        TTF_SetFontKerning(pdc_ttffont, 0);
        TTF_SetFontHinting(pdc_ttffont, TTF_HINTING_MONO);
        TTF_SizeText(pdc_ttffont, "W", &pdc_fwidth, &pdc_fheight);
    }

    lv::save_font_size(pdc_font_size);
    app_ui.rebuild_layout();
}

// SDL event filter — runs before SDL_PollEvent returns to any caller.
// Intercept Ctrl+=/- here so PDCursesMod's internal SDL_PollEvent never
// sees them.
static int SDLCALL event_filter(void* userdata, SDL_Event* event) {
    auto* state = static_cast<GuiState*>(userdata);
    if (event->type == SDL_QUIT) {
        state->quit = true;
    }
    if (event->type == SDL_KEYDOWN && (event->key.keysym.mod & KMOD_CTRL)) {
        if (event->key.keysym.sym == SDLK_EQUALS ||
            event->key.keysym.sym == SDLK_PLUS) {
            if (state->app_ui) adjust_font_now(*state->app_ui, +1);
            return 0;  // drop
        }
        if (event->key.keysym.sym == SDLK_MINUS) {
            if (state->app_ui) adjust_font_now(*state->app_ui, -1);
            return 0;  // drop
        }
    }
    return 1;  // keep
}

int run(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        usage(argv[0]);
        return 1;
    }

    const std::string log_path = argv[1];
    const std::string rules_path = (argc == 3) ? argv[2] : std::string{};

    // ---- Open the log + rule files before touching SDL so we can fail fast.
    lv::MMapFile file;
    if (!file.open(log_path)) {
        std::fprintf(stderr, "lv-gui: cannot open log file: %s\n", argv[1]);
        return 1;
    }
    lv::LineIndex index;
    if (!index.build(file)) {
        std::fprintf(stderr, "lv-gui: cannot index log file: %s\n", argv[1]);
        return 1;
    }
    lv::RuleSet rules;
    if (!rules_path.empty()) {
        std::string error;
        if (!rules.load(rules_path, &error)) {
            std::fprintf(stderr, "lv-gui: %s\n", error.c_str());
            return 1;
        }
    }

    // ---- SDL2 init with OpenGL context --------------------------------------
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::fprintf(stderr, "lv-gui: SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    constexpr int kInitialWindowWidth = 1280;
    constexpr int kInitialWindowHeight = 800;
    SDL_Window* window = SDL_CreateWindow(
        "lv", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        kInitialWindowWidth, kInitialWindowHeight,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (window == nullptr) {
        std::fprintf(stderr, "lv-gui: SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (gl_context == nullptr) {
        std::fprintf(stderr, "lv-gui: SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (SDL_GL_SetSwapInterval(1) != 0) {
        SDL_ClearError();
    }

    // ---- Hand the SDL window + GL context to PDCursesMod BEFORE initscr(). ---
    pdc_window = window;
    pdc_gl_context = gl_context;

    // Restore the saved font size.
    {
        const int saved = lv::load_font_size();
        if (saved > 0) pdc_font_size = saved;
    }

    // Push a synthetic resize event for first-frame sizing.
    {
        SDL_Event ev;
        SDL_zero(ev);
        ev.type = SDL_WINDOWEVENT;
        ev.window.event = SDL_WINDOWEVENT_SIZE_CHANGED;
        int ww, wh;
        SDL_GetWindowSize(window, &ww, &wh);
        ev.window.data1 = ww;
        ev.window.data2 = wh;
        ev.window.windowID = SDL_GetWindowID(window);
        SDL_PushEvent(&ev);
    }

    // ---- AppUi --------------------------------------------------------------
    {
        lv::ui::AppUi app_ui(std::move(file), std::move(index), std::move(rules),
                             rules_path.empty() ? "" : rules_path);
        app_ui.start();

        GuiState gui_state;
        gui_state.app_ui = &app_ui;
        SDL_SetEventFilter(event_filter, &gui_state);

        // ---- Main loop ------------------------------------------------------
        while (!gui_state.quit && app_ui.tick()) {
            // tick() handles its own SDL event processing via getch().
        }
    }

    // ---- Cleanup ------------------------------------------------------------
    SDL_SetEventFilter(nullptr, nullptr);
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    return run(argc, argv);
}
