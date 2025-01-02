#include <cmath>
#include <cassert>
#include <cstring>
#include <cwchar>

#include "ted.h"
#include "common.h"
#include "config.h"

ted::State ted::state;

void ted::init()
{
    state.lines = common::Lines{ .text = state.buffer };
    state.lines.grow_one();
    state.lines.items[0].text = state.buffer;
    state.lines.items[0].len = 0;
    state.cursor_pos = Pos{};
    state.max_line_len = floor(DEFAULT_WIDTH/common::state.glyph_width);
}

void ted::clear()
{
    state.lines.text_len = 0;
    state.lines.len = 0;
    state.cursor_pos = Pos{};
    state.lines.grow_one();
}

void ted::render()
{
    // Render text editor background
    Vector2 pos = { .y = ((float)(DEFAULT_HEIGHT - state.lines.len*FONT_SIZE)) };
    DrawRectangle(0, pos.y, DEFAULT_WIDTH, DEFAULT_HEIGHT, TED_BG_COLOR);

    // Render text
    common::draw_lines(pos, state.lines, TED_FG_COLOR);

    // Render cursor
    pos.x = state.cursor_pos.col*common::state.glyph_width + 1;
    pos.y = pos.y + state.cursor_pos.row*FONT_SIZE;
    DrawTextCodepoint(
            common::state.font, TED_CURSOR_CODE,
            pos, FONT_SIZE,
            TED_CURSOR_COLOR);
}

void ted::try_cursor_motion(Motion m)
{
    Pos p = state.cursor_pos;
    int *curr_text_ptr, *ptr;
    switch (m) {
        case MOTION_BACKWARD:
            if (p.col == 0) {
                if (p.row > 0) {
                    p.row -= 1;
                    p.col = state.lines.items[p.row].len;
                }
            } else {
                p.col -= 1;
            }
            break;

        case MOTION_FORWARD:
            if (p.col == state.lines.items[p.row].len) {
                if (p.row+1 < state.lines.len) {
                    p.row += 1;
                    p.col = 0;
                }
            } else {
                p.col += 1;
            }
            break;

        case MOTION_FORWARD_WORD:;
            curr_text_ptr = &state.lines.items[p.row].text[p.col];
            ptr = &state.lines.text[state.lines.text_len-1]; // set to the end text ptr
            while (curr_text_ptr != ptr && *curr_text_ptr == ' ') curr_text_ptr++;  // move to the word begin
            while (curr_text_ptr != ptr && *curr_text_ptr != ' ') curr_text_ptr++;  // move to the word end
            ted::move_cursor_to_ptr(curr_text_ptr+1);
            return;

        case MOTION_BACKWARD_WORD:;
            curr_text_ptr = &state.lines.items[p.row].text[p.col];
            ptr = state.lines.text-1; // set to the begin text ptr
            if (curr_text_ptr[-1] == ' ') curr_text_ptr--;
            while (curr_text_ptr != ptr && *curr_text_ptr == ' ') curr_text_ptr--; // move to the word end
            while (curr_text_ptr != ptr && *curr_text_ptr != ' ') curr_text_ptr--; // move to the word begin
            ted::move_cursor_to_ptr(curr_text_ptr+1);
            return;

        case MOTION_UP:
            if (p.row > 0) {
                p.row -= 1;
                if (p.col >= state.lines.items[p.row].len) {
                    p.col = state.lines.items[p.row].len;
                }
            }
            break;

        case MOTION_DOWN:
            if (p.row+1 < state.lines.len) {
                p.row += 1;
                if (p.col >= state.lines.items[p.row].len) {
                    p.col = state.lines.items[p.row].len;
                }
            }
            break;

        case MOTION_BEGIN:
            p.col = 0;
            break;

        case MOTION_END:
            p.col = state.lines.items[p.row].len;
            break;

        default: assert(0 && "not yet implemented");
    }

    state.cursor_pos = p;
}

void ted::move_cursor_to_ptr(int *text_ptr)
{
    Pos pos = { state.cursor_pos.row > 0 ? state.cursor_pos.row-1 : 0, 0 };
    common::Line line = state.lines.items[pos.row];
    size_t real_line_len = line.len + line.trim_whitespace_count;
    while (&line.text[pos.col] != text_ptr) {
        if (pos.col >= real_line_len) {
            if (++pos.row >= state.lines.len) return;
            line = state.lines.items[pos.row];
            real_line_len = line.len + line.trim_whitespace_count;
            pos.col = 0;
        } else {
            pos.col += 1;
        }
    }

    state.cursor_pos = pos;
}

void ted::insert_symbol(int s)
{
    if (state.lines.text_len+1 >= MAX_MSG_LEN) return;

    // move text after cursor
    int *text_curr_ptr = &state.lines.items[state.cursor_pos.row].text[state.cursor_pos.col];
    int *text_end_ptr = &state.lines.text[state.lines.text_len];
    size_t size = (text_end_ptr - text_curr_ptr) * sizeof(int);
    memmove(text_curr_ptr+1, text_curr_ptr, size);

    *text_curr_ptr = s;
    state.lines.text_len += 1;

    state.lines.recalc(state.max_line_len);
    ted::move_cursor_to_ptr(text_curr_ptr+1);
}

void ted::delete_symbols(size_t count)
{
    if (state.lines.text_len < count) return;

    int *text_curr_ptr = &state.lines.items[state.cursor_pos.row].text[state.cursor_pos.col];
    int *text_end_ptr = &state.lines.text[state.lines.text_len];
    size_t size = (text_end_ptr - text_curr_ptr) * sizeof(int);

    int *new_curr_text_ptr = text_curr_ptr - count;
    memmove(new_curr_text_ptr, text_curr_ptr, size);
    state.lines.text_len -= text_curr_ptr - new_curr_text_ptr;

    state.lines.recalc(state.max_line_len);
    ted::move_cursor_to_ptr(new_curr_text_ptr);
}

void ted::delete_word()
{
    int *begin_text_ptr = state.lines.text;
    int *curr_text_ptr = &state.lines.items[state.cursor_pos.row].text[state.cursor_pos.col];
    int *new_curr_text_ptr = curr_text_ptr;
    if (new_curr_text_ptr[-1] == ' ') new_curr_text_ptr--;
    while (new_curr_text_ptr != begin_text_ptr && *new_curr_text_ptr == ' ') new_curr_text_ptr--; // move to the word end
    while (new_curr_text_ptr != begin_text_ptr) {
        if (*new_curr_text_ptr == ' ') {
            new_curr_text_ptr += 1;
            break;
        } else {
            new_curr_text_ptr--;
        }
    }

    ted::delete_symbols(curr_text_ptr - new_curr_text_ptr);
}

void ted::delete_line()
{
    int *curr_text_ptr = &state.lines.items[state.cursor_pos.row].text[state.cursor_pos.col];
    int *new_text_ptr = state.lines.items[state.cursor_pos.row].text;
    ted::delete_symbols(curr_text_ptr - new_text_ptr);
}
