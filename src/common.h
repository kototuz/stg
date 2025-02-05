#ifndef COMMON_H
#define COMMON_H

#include <cstddef>

#include "config.h"

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

        void    grow_one();
        float   max_line_width(FontId font_id);
        void    recalc(FontId font_id, wchar_t *text, size_t text_len, float max_line_width);
        Vector2 get_vec_to_pos(FontId font_id, size_t row, size_t col);
    };

    void  init();
    void  draw_text_in_width(FontId font_id, Vector2 pos, const wchar_t *text, size_t text_len, Color color, float in_width);
    void  draw_lines(FontId font_id, Vector2 pos, Lines lines, Color color);
    void  draw_wtext(FontId font_id, Vector2 pos, const wchar_t *wtext, size_t wtext_len, Color color);
    float font_size(FontId font_id);
    float measure_wtext(FontId font_id, const wchar_t *text, size_t text_len); // Function like 'MeasureText' but for 'wchar_t *'
}

#endif
