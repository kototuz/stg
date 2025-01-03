#include <cmath>
#include <cassert>
#include <cstring>
#include <cwchar>

#include "ted.h"
#include "common.h"
#include "config.h"
#include "common.h"

#define MAX_MSG_LEN 4096
#define MAX_PLACEHOLDER_LEN 32

struct Pos {
    size_t col;
    size_t row;
};

static common::Lines ted_lines;
static Pos ted_cursor_pos;
static size_t ted_max_line_len;
static wchar_t ted_buffer[MAX_MSG_LEN];
static size_t  ted_buffer_len = 0;
static wchar_t ted_placeholder[MAX_PLACEHOLDER_LEN];
static size_t ted_placeholder_len;

static void move_cursor_to_ptr(wchar_t *text_ptr)
{
    Pos pos = { ted_cursor_pos.row > 0 ? ted_cursor_pos.row-1 : 0, 0 };
    common::Line line = ted_lines.items[pos.row];
    size_t real_line_len = line.len + line.trim_whitespace_count;
    while (&line.text[pos.col] != text_ptr) {
        if (pos.col >= real_line_len) {
            if (++pos.row >= ted_lines.len) return;
            line = ted_lines.items[pos.row];
            real_line_len = line.len + line.trim_whitespace_count;
            pos.col = 0;
        } else {
            pos.col += 1;
        }
    }

    ted_cursor_pos = pos;
}

void ted::init()
{
    ted_lines = common::Lines{};
    ted_lines.grow_one();
    ted_lines.items[0].text = ted_buffer;
    ted_lines.items[0].len = 0;
    ted_cursor_pos = Pos{};
    ted_max_line_len = floor(DEFAULT_WIDTH/common::state.glyph_width);
}

void ted::clear()
{
    ted_buffer_len = 0;
    ted_lines.len = 0;
    ted_cursor_pos = Pos{};
    ted_lines.grow_one();
}

void ted::render()
{
    // Render text editor background
    Vector2 pos = { .y = ((float)(DEFAULT_HEIGHT - ted_lines.len*FONT_SIZE)) };
    DrawRectangle(0, pos.y, DEFAULT_WIDTH, DEFAULT_HEIGHT, TED_BG_COLOR);

    // Render placeholder or text if it exists
    if (ted_buffer_len == 0) {
        DrawTextCodepoints(
                common::state.font,
                (int*)ted_placeholder, ted_placeholder_len,
                pos, FONT_SIZE, 0, TED_PLACEHOLDER_COLOR);
    } else {
        common::draw_lines(pos, ted_lines, TED_FG_COLOR);
    }

    // Render cursor
    pos.x = ted_cursor_pos.col*common::state.glyph_width + 1;
    pos.y = pos.y + ted_cursor_pos.row*FONT_SIZE;
    DrawTextCodepoint(
            common::state.font, TED_CURSOR_CODE,
            pos, FONT_SIZE,
            TED_CURSOR_COLOR);
}

void ted::try_cursor_motion(Motion m)
{
    Pos p = ted_cursor_pos;
    wchar_t *curr_text_ptr, *ptr;
    switch (m) {
        case MOTION_BACKWARD:
            if (p.col == 0) {
                if (p.row > 0) {
                    p.row -= 1;
                    p.col = ted_lines.items[p.row].len;
                }
            } else {
                p.col -= 1;
            }
            break;

        case MOTION_FORWARD:
            if (p.col == ted_lines.items[p.row].len) {
                if (p.row+1 < ted_lines.len) {
                    p.row += 1;
                    p.col = 0;
                }
            } else {
                p.col += 1;
            }
            break;

        case MOTION_FORWARD_WORD:;
            curr_text_ptr = &ted_lines.items[p.row].text[p.col];
            ptr = &ted_buffer[ted_buffer_len-1]; // set to the end text ptr
            while (curr_text_ptr != ptr && *curr_text_ptr == ' ') curr_text_ptr++;  // move to the word begin
            while (curr_text_ptr != ptr && *curr_text_ptr != ' ') curr_text_ptr++;  // move to the word end
            move_cursor_to_ptr(curr_text_ptr+1);
            return;

        case MOTION_BACKWARD_WORD:;
            curr_text_ptr = &ted_lines.items[p.row].text[p.col];
            ptr = ted_buffer-1; // set to the begin text ptr
            if (curr_text_ptr[-1] == ' ') curr_text_ptr--;
            while (curr_text_ptr != ptr && *curr_text_ptr == ' ') curr_text_ptr--; // move to the word end
            while (curr_text_ptr != ptr && *curr_text_ptr != ' ') curr_text_ptr--; // move to the word begin
            move_cursor_to_ptr(curr_text_ptr+1);
            return;

        case MOTION_UP:
            if (p.row > 0) {
                p.row -= 1;
                if (p.col >= ted_lines.items[p.row].len) {
                    p.col = ted_lines.items[p.row].len;
                }
            }
            break;

        case MOTION_DOWN:
            if (p.row+1 < ted_lines.len) {
                p.row += 1;
                if (p.col >= ted_lines.items[p.row].len) {
                    p.col = ted_lines.items[p.row].len;
                }
            }
            break;

        case MOTION_BEGIN:
            p.col = 0;
            break;

        case MOTION_END:
            p.col = ted_lines.items[p.row].len;
            break;

        default: assert(0 && "not yet implemented");
    }

    ted_cursor_pos = p;
}

void ted::insert_symbol(int s)
{
    if (ted_buffer_len+1 >= MAX_MSG_LEN) return;

    // move text after cursor
    wchar_t *text_curr_ptr = &ted_lines.items[ted_cursor_pos.row].text[ted_cursor_pos.col];
    wchar_t *text_end_ptr = &ted_buffer[ted_buffer_len];
    size_t size = (text_end_ptr - text_curr_ptr) * sizeof(int);
    memmove(text_curr_ptr+1, text_curr_ptr, size);

    *text_curr_ptr = s;
    ted_buffer_len += 1;

    ted_lines.recalc(ted_buffer, ted_buffer_len, ted_max_line_len);
    move_cursor_to_ptr(text_curr_ptr+1);
}

void ted::delete_symbols(size_t count)
{
    if (ted_buffer_len < count) return;

    wchar_t *text_curr_ptr = &ted_lines.items[ted_cursor_pos.row].text[ted_cursor_pos.col];
    wchar_t *text_end_ptr = &ted_buffer[ted_buffer_len];
    size_t size = (text_end_ptr - text_curr_ptr) * sizeof(int);

    wchar_t *new_curr_text_ptr = text_curr_ptr - count;
    memmove(new_curr_text_ptr, text_curr_ptr, size);
    ted_buffer_len -= text_curr_ptr - new_curr_text_ptr;

    ted_lines.recalc(ted_buffer, ted_buffer_len, ted_max_line_len);
    move_cursor_to_ptr(new_curr_text_ptr);
}

void ted::delete_word()
{
    wchar_t *begin_text_ptr = ted_buffer;
    wchar_t *curr_text_ptr = &ted_lines.items[ted_cursor_pos.row].text[ted_cursor_pos.col];
    wchar_t *new_curr_text_ptr = curr_text_ptr;
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
    wchar_t *curr_text_ptr = &ted_lines.items[ted_cursor_pos.row].text[ted_cursor_pos.col];
    wchar_t *new_text_ptr = ted_lines.items[ted_cursor_pos.row].text;
    ted::delete_symbols(curr_text_ptr - new_text_ptr);
}

float ted::get_height()
{
    return ted_lines.len*FONT_SIZE;
}

std::wstring_view ted::get_text()
{
    return std::wstring_view(ted_buffer, ted_buffer_len);
}

void ted::set_placeholder(const wchar_t *text)
{
    ted_placeholder_len = wcslen(text);
    if (ted_placeholder_len > MAX_PLACEHOLDER_LEN) {
        memcpy(ted_placeholder, text, (MAX_PLACEHOLDER_LEN-1) * sizeof(int));
        ted_placeholder[MAX_PLACEHOLDER_LEN-1] = L'…';
        ted_placeholder_len = MAX_PLACEHOLDER_LEN;
    } else {
        memcpy(ted_placeholder, text, ted_placeholder_len * sizeof(int));
    }
}
