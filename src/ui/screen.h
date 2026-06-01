#pragma once

#include <ncurses.h>

namespace lv::ui {

struct Rect {
    int y = 0;
    int x = 0;
    int height = 0;
    int width = 0;
};

class Screen {
public:
    Screen();
    ~Screen();

    Screen(const Screen&) = delete;
    Screen& operator=(const Screen&) = delete;

    int rows() const;
    int cols() const;
};

} // namespace lv::ui
