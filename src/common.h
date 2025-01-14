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
    float max_line_width(Font font);
    void recalc(Font font, int font_size, wchar_t *text, size_t text_len, float max_line_width);
    Vector2 get_vec_to_pos(Font font, int font_size, size_t row, size_t col);
};

void draw_lines(Font font, size_t font_size, Vector2 pos, Lines lines, Color color);
float measure_wtext(Font font, const wchar_t *text, size_t text_len); // Function like 'MeasureText' but for 'wchar_t *'
}

#endif
