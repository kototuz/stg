#ifndef COMMON_H
#define COMMON_H

#include <cstddef>

#include <raylib.h>

namespace common {
struct Line {
    wchar_t *text;
    size_t len;
    size_t trim_whitespace_count;
};

struct Lines {
    Line *items;
    size_t len;
    size_t cap;

    void grow_one();
    void recalc(wchar_t *text, size_t text_len, size_t max_line);
};

void draw_lines(Font font, size_t font_size, Vector2 pos, Lines lines, Color color);
}

#endif
