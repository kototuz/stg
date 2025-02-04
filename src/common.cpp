#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <cwchar>
#include <dirent.h>

#include "common.h"
#include "config.h"

#define EMOJI_DIR_PATH "resources/emoji"
// TODO: Emoji size should be adapted to font size
#define EMOJI_SIZE     28.0f
#define EMOJI_COUNT    1241

struct Emoji {
    wchar_t code;
    Texture2D texture;
};

Font common::fonts[];

static Emoji g_emoji[EMOJI_COUNT];

static float get_glyph_width(Font font, wchar_t codepoint);
static bool is_emoji(wchar_t codepoint);
static Texture get_emoji_texture(wchar_t emoji_codepoint);
static void draw_wtext(Font font, size_t font_size, Vector2 pos, const wchar_t *wtext, size_t wtext_len, Color color);

void common::init()
{
#define X(name, path, size) \
    fonts[FontId::name] = LoadFontEx(path, size, nullptr, FONT_GLYPH_COUNT);
    LIST_OF_FONTS
#undef X

    // Load emoji textures /////

    DIR *emoji_dir;
    if ((emoji_dir = opendir(EMOJI_DIR_PATH)) == nullptr) {
        fprintf(stderr, "ERROR: Could not open '%s'\n", EMOJI_DIR_PATH);
        exit(1);
    }

    struct dirent *dp;
    size_t emoji_i = 0;
    char path[] = EMOJI_DIR_PATH"/00000.png";
    while ((dp = readdir(emoji_dir)) != nullptr) {
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) continue;
        strcpy(&path[sizeof(EMOJI_DIR_PATH)], dp->d_name);
        Image emoji_img = LoadImage(path);
        ImageResize(&emoji_img, EMOJI_SIZE, EMOJI_SIZE);
        assert(emoji_i+1 < EMOJI_COUNT && "Increase 'EMOJI_COUNT'");
        g_emoji[emoji_i++] = {
            (wchar_t) strtol(dp->d_name, NULL, 16),
            LoadTextureFromImage(emoji_img),
        };

        UnloadImage(emoji_img);
    }

    closedir(emoji_dir);
}

float common::get_chat_view_x()
{
    return (GetScreenWidth()/2) - (CHAT_VIEW_WIDTH/2);
}

// TODO: replace '\n' with ' '
void common::draw_text_in_width(
        Font font, int font_size,
        Vector2 pos,
        const wchar_t *text, size_t text_len,
        Color color, float in_width)
{
    in_width -= 3*get_glyph_width(font, L'.'); // to reserve place for '...'

    float width = 0;
    for (size_t i = 0; i < text_len; i++) {
        float glyph_width = get_glyph_width(font, text[i]);
        if (width+glyph_width > in_width) {
            draw_wtext(font, font_size, pos, text, i, color);
            pos.x += width;
            DrawTextCodepoints(font, (const int*)L"...", 3, pos, font_size, 0, color);
            return;
        }

        width += glyph_width;
    }

    // If text fits in length draw it
    draw_wtext(font, font_size, pos, text, text_len, color);
}

void common::draw_lines(Font font, size_t font_size, Vector2 pos, common::Lines lines, Color color)
{
    size_t begin_x = pos.x;
    for (size_t i = 0; i < lines.len; i++) {
        draw_wtext(font, font_size, pos, lines.items[i].text, lines.items[i].len, color);
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
    new_buf[this->len] = {};

    this->cap += 1;
    this->len += 1;
    if (this->items != NULL) free(this->items);
    this->items = new_buf;
}

Vector2 common::Lines::get_vec_to_pos(Font font, int font_size, size_t row, size_t col)
{
    Vector2 ret = { 0, (float) row*font_size };
    Line line = this->items[row];
    for (size_t i = 0; i < col; i++) {
        ret.x += get_glyph_width(font, line.text[i]);
    }

    return ret;
}

void common::Lines::recalc(Font font, wchar_t *text, size_t text_len, float max_line_width)
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

// PRIVATE FUNCTION IMPLEMENTATIONS //////////////////////////////

static float get_glyph_width(Font font, wchar_t codepoint)
{
    if (!is_emoji(codepoint)) {
        int cp_idx = GetGlyphIndex(font, codepoint);
        return (font.glyphs[cp_idx].advanceX == 0) ?
            font.recs[cp_idx].width :
            font.glyphs[cp_idx].advanceX;
    } else {
        return EMOJI_SIZE;
    }
}

static bool is_emoji(wchar_t codepoint)
{
    for (size_t i = 0; i < EMOJI_COUNT; i++) {
        if (codepoint == g_emoji[i].code) return true;
    }

    return false;
}

static Texture get_emoji_texture(wchar_t emoji_codepoint)
{
    for (size_t i = 0; i < EMOJI_COUNT; i++) {
        if (g_emoji[i].code == emoji_codepoint) {
            return g_emoji[i].texture;
        }
    }

    assert(0 && "Unknown emoji");
}

static void draw_wtext(Font font, size_t font_size, Vector2 pos, const wchar_t *wtext, size_t wtext_len, Color color)
{
    for (size_t i = 0; i < wtext_len; i++) {
        wchar_t codepoint = wtext[i];
        if (is_emoji(codepoint)) {
            DrawTextureV(get_emoji_texture(codepoint), pos, WHITE);
        } else {
            DrawTextCodepoint(font, codepoint, pos, font_size, color);
        }
        pos.x += get_glyph_width(font, codepoint);
    }
}
