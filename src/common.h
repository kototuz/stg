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

struct State {
    Font font;
    size_t glyph_width;
};

extern State state;

void init();
void draw_lines(Vector2 pos, Lines lines, Color color);
}

#endif
