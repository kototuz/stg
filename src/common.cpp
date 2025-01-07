#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "common.h"
#include "config.h"

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

void common::Lines::recalc(wchar_t *text, size_t text_len, size_t max_line)
{
    // clear all lines
    this->len = 0;
    memset(this->items, 0x0, this->cap*sizeof(common::Line));
    this->grow_one();
    this->items[0].text = text;

    size_t last_word_begin = 0;
    common::Line *curr_line = &this->items[0];
    for (size_t i = 0; i < text_len; i++) {
        if (text[i] == '\n') {
            this->grow_one();
            curr_line = &this->items[this->len-1];
            curr_line->len = 0;
            curr_line->text = &text[i+1];
        } else if (curr_line->len+1 > max_line) {
            if (text[i] == ' ') {
                do {
                    curr_line->trim_whitespace_count += 1;
                    if (++i >= text_len) return;
                } while (text[i] == ' ');
                this->grow_one();
                curr_line = &this->items[this->len-1];
                curr_line->len = 1;
                curr_line->text = &text[i];
            } else if (i - last_word_begin >= max_line) {
                this->grow_one();
                curr_line = &this->items[this->len-1];
                curr_line->len = 1;
                curr_line->text = &text[i];
            } else {
                curr_line->len -= i - last_word_begin + 1;
                this->grow_one();
                curr_line = &this->items[this->len-1];
                curr_line->text = &text[last_word_begin];
                curr_line->len = i - last_word_begin + 1;
            }
        } else {
            curr_line->len += 1;
            if (text[i] == ' ') last_word_begin = i+1;
        }
    }
}
