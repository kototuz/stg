#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cassert>

#include "common.h"
#include "config.h"

static float get_glyph_width(Font font, wchar_t codepoint)
{
    int cp_idx = GetGlyphIndex(font, codepoint);
    return (font.glyphs[cp_idx].advanceX == 0) ?
           font.recs[cp_idx].width :
           font.glyphs[cp_idx].advanceX;
}

void common::draw_lines(Font font, size_t font_size, Vector2 pos, common::Lines lines, Color color)
{
    size_t begin_x = pos.x;
    for (size_t i = 0; i < lines.len; i++) {
        DrawTextCodepoints(
                font,
                (const int *)lines.items[i].text,
                lines.items[i].len,
                pos, font_size, 0, color);
        pos.y += font_size;
        pos.x = begin_x;
    }
}

float common::Lines::max_line_width(Font font)
{
    float result = 0;
    for (size_t i = 0; i < this->len; i++) {
        float line_len = measure_wtext(font, this->items[i].text, this->items[i].len);
        if (line_len > result) result = line_len;
    }

    return result;
}

void common::Lines::grow_one()
{
    if (this->len < this->cap) {
        this->items[this->len].len = 0;
        this->len += 1;
        return;
    }

    Line *new_buf = (Line *) malloc((this->cap+1) * sizeof(Line));
    if (new_buf == NULL) {
        fprintf(stderr, "ERROR: Could not grow one line: no memory\n");
        exit(1);
    }

    memcpy(new_buf, this->items, this->len * sizeof(Line));
    new_buf[this->len] = (Line){0};

    this->cap += 1;
    this->len += 1;
    if (this->items != NULL) free(this->items);
    this->items = new_buf;
}

Vector2 common::Lines::get_vec_to_pos(Font font, int font_size, size_t row, size_t col)
{
    Vector2 ret = { .y = (float) row*font_size };
    Line line = this->items[row];
    for (size_t i = 0; i < col; i++) {
        ret.x += get_glyph_width(font, line.text[i]);
    }

    return ret;
}

void common::Lines::recalc(Font font, int font_size, wchar_t *text, size_t text_len, float max_line_width)
{
    // clear all lines
    this->len = 0;
    memset(this->items, 0x0, this->cap*sizeof(common::Line));
    this->grow_one();
    this->items[0].text = text;

    float curr_line_width = 0;
    float last_word_begin_on_width = 0;
    size_t last_word_begin_idx = 0;
    Line *curr_line = &this->items[0];
    for (size_t i = 0; i < text_len; i++) {
        if (text[i] == '\n') {
            this->grow_one();
            curr_line = &this->items[this->len-1];
            curr_line->len = 0;
            curr_line->text = &text[i+1];
            curr_line_width = 0;
            last_word_begin_on_width = 0;
            continue;
        }

        float glyph_width = get_glyph_width(font, text[i]);
        if (curr_line_width+glyph_width > max_line_width) {
            if (text[i] == ' ') {
                do {
                    curr_line->trim_whitespace_count += 1;
                    if (++i >= text_len) return;
                } while (text[i] == ' ');
                this->grow_one();
                curr_line = &this->items[this->len-1];
                curr_line->len = 1;
                curr_line->text = &text[i];
                curr_line_width = glyph_width;
            } else if (curr_line_width - last_word_begin_on_width + glyph_width >= max_line_width) {
                this->grow_one();
                curr_line = &this->items[this->len-1];
                curr_line->len = 1;
                curr_line->text = &text[i];
                curr_line_width = glyph_width;
                last_word_begin_on_width = 0;
            } else {
                curr_line->len -= i - last_word_begin_idx + 1;
                this->grow_one();
                curr_line = &this->items[this->len-1];
                curr_line->text = &text[last_word_begin_idx];
                curr_line->len = i - last_word_begin_idx + 1;
                curr_line_width = curr_line_width - last_word_begin_on_width + glyph_width;
                last_word_begin_on_width = 0;
            }
        } else {
            curr_line_width += glyph_width;
            curr_line->len += 1;
            if (text[i] == ' ') {
                last_word_begin_on_width = curr_line_width;
                last_word_begin_idx = i+1;
            }
        }
        /*puts("========================");*/
        /*printf("Last word begin on: %f\n", last_word_begin_on_width);*/
        /*printf("Glyph width:        %f\n", glyph_width);*/
        /*printf("Max line width:     %f\n", max_line_width);*/
        /*printf("Current line width: %f\n", curr_line_width);*/
        /*puts("========================");*/
    }
}

float common::measure_wtext(Font font, const wchar_t *text, size_t text_len)
{
    float result = 0;
    for (size_t i = 0; i < text_len; i++) {
        result += get_glyph_width(font, text[i]);
    }

    return result;
}
