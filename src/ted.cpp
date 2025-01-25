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
static Pos           ted_cursor_pos;
static bool          ted_cursor_has_space = false;
static float         ted_max_line_width;
static wchar_t       ted_buffer[MAX_MSG_LEN];
static size_t        ted_buffer_len = 0;
static wchar_t       ted_placeholder[MAX_PLACEHOLDER_LEN];
static size_t        ted_placeholder_len;
static Font          ted_font;
static size_t        ted_font_glyph_width;

static void move_cursor_to_ptr(wchar_t *text_ptr)
{
    Pos pos = { 0, 0 };
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

    if (pos.col > ted_max_line_width) {
        pos.col = ted_max_line_width;
        ted_cursor_has_space = true;
    } else {
        ted_cursor_has_space = false;
    }

    ted_cursor_pos = pos;
}

void ted::init()
{
    // Load ted font
    ted_font = LoadFontEx(
            TED_FONT_PATH, TED_FONT_SIZE,
            nullptr, FONT_GLYPH_COUNT);
    ted_font_glyph_width =
        TED_FONT_SIZE / ted_font.baseSize * ted_font.glyphs[0].advanceX;

    ted_lines = common::Lines{};
    ted_lines.grow_one();
    ted_lines.items[0].text = ted_buffer;
    ted_lines.items[0].len = 0;
    ted_cursor_pos = Pos{};
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
    // Calculate max line
    ted_max_line_width =
        floor(CHAT_VIEW_WIDTH -
              BoxModel::TED_LM - BoxModel::TED_LP -
              BoxModel::TED_RP - BoxModel::TED_RM);

    // Render text editor rectangle
    Rectangle ted_rec = {};
    ted_rec.width = CHAT_VIEW_WIDTH - BoxModel::TED_LM - BoxModel::TED_RM;
    ted_rec.height = ted_lines.len*TED_FONT_SIZE + BoxModel::TED_TP + BoxModel::TED_BP;
    ted_rec.x = common::get_chat_view_x() + BoxModel::TED_LM;
    ted_rec.y = GetScreenHeight() - ted_rec.height - BoxModel::TED_BM;
    DrawRectangleRounded(ted_rec, TED_REC_ROUNDNESS/ted_rec.height, TED_REC_SEGMENT_COUNT, TED_BG_COLOR);

    // Render placeholder or text if it exists
    Vector2 pos = { ted_rec.x + BoxModel::TED_LP, ted_rec.y + BoxModel::TED_TP };
    if (ted_buffer_len == 0) {
        DrawTextCodepoints(
                ted_font,
                (int*)ted_placeholder, ted_placeholder_len,
                pos, TED_FONT_SIZE, 0, TED_PLACEHOLDER_COLOR);
    } else {
        ted::set_placeholder(L"Message...");
        common::draw_lines(ted_font, TED_FONT_SIZE, pos, ted_lines, TED_FG_COLOR);
    }

    // Render cursor
    Vector2 vec_to_pos = ted_lines.get_vec_to_pos(ted_font, TED_FONT_SIZE, ted_cursor_pos.row, ted_cursor_pos.col);
    pos.x += vec_to_pos.x;
    pos.y += vec_to_pos.y;
    DrawLine(pos.x, pos.y, pos.x, pos.y+TED_FONT_SIZE, TED_CURSOR_COLOR);
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
    wchar_t *text_curr_ptr = &ted_lines.items[ted_cursor_pos.row].text[ted_cursor_pos.col+ted_cursor_has_space];
    wchar_t *text_end_ptr = &ted_buffer[ted_buffer_len];
    size_t size = (text_end_ptr - text_curr_ptr) * sizeof(int);
    memmove(text_curr_ptr+1, text_curr_ptr, size);

    *text_curr_ptr = s;
    ted_buffer_len += 1;

    ted_lines.recalc(ted_font, ted_buffer, ted_buffer_len, ted_max_line_width);
    move_cursor_to_ptr(text_curr_ptr+1);
}

void ted::delete_symbols(size_t count)
{
    if (ted_buffer_len < count) return;
    if (ted_cursor_pos.col == 0 && ted_cursor_pos.row == 0) return;

    wchar_t *text_curr_ptr = &ted_lines.items[ted_cursor_pos.row].text[ted_cursor_pos.col];
    wchar_t *text_end_ptr = &ted_buffer[ted_buffer_len];
    size_t size = (text_end_ptr - text_curr_ptr) * sizeof(int);

    wchar_t *new_curr_text_ptr = text_curr_ptr - count;
    memmove(new_curr_text_ptr, text_curr_ptr, size);
    ted_buffer_len -= text_curr_ptr - new_curr_text_ptr;

    ted_lines.recalc(ted_font, ted_buffer, ted_buffer_len, ted_max_line_width);
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
    return ted_lines.len*TED_FONT_SIZE +
           BoxModel::TED_BM + BoxModel::TED_BP +
           BoxModel::TED_TP + BoxModel::TED_TM;
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
