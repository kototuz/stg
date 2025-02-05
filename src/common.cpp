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
#define EMOJI_COUNT    1252

struct Emoji {
    wchar_t code;
    Texture2D texture;
};

struct FontData {
    Font font;
    EmojiSize emoji_size;
};


static float get_glyph_width(FontId font_id, wchar_t codepoint);
static bool is_emoji(wchar_t codepoint);
static Texture get_emoji_texture(FontId font_id, wchar_t emoji_codepoint);


static FontData g_font_data[FONT_ID_COUNT];
static Emoji    g_emoji_arrays[EMOJI_SIZE_COUNT][EMOJI_COUNT];


void common::init()
{
#define X(name, path, size, emoji_size) \
    g_font_data[FONT_ID_ ## name] = { \
        LoadFontEx(path, size, nullptr, FONT_GLYPH_COUNT), \
        emoji_size, \
    };
    LIST_OF_FONTS
#undef X

    // Open emoji directory
    DIR *emoji_dir;
    if ((emoji_dir = opendir(EMOJI_DIR_PATH)) == nullptr) {
        fprintf(stderr, "ERROR: Could not open '%s'\n", EMOJI_DIR_PATH);
        exit(1);
    }

    // Load emoji images and its codepoints from the directory
    struct dirent *dp;
    size_t emoji_image_i = 0;
    struct { Image img; wchar_t code; } emoji_images[EMOJI_COUNT];
    char path[] = EMOJI_DIR_PATH"/00000.png";
    while ((dp = readdir(emoji_dir)) != nullptr) {
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) continue;
        strcpy(&path[sizeof(EMOJI_DIR_PATH)], dp->d_name);
        assert(emoji_image_i < EMOJI_COUNT && "Increase 'EMOJI_COUNT'");
        emoji_images[emoji_image_i++] = {
            LoadImage(path),
            (wchar_t) strtol(dp->d_name, NULL, 16),
        };
    }
    closedir(emoji_dir);

    // Load emoji arrays
    size_t i;
    Image emoji_img;
#define X(emoji_size) \
    for (i = 0; i < EMOJI_COUNT; i++) { \
        emoji_img = ImageCopy(emoji_images[i].img); \
        ImageResize(&emoji_img, emoji_size, emoji_size); \
        g_emoji_arrays[EMOJI_SIZE_ ## emoji_size][i] = {\
            emoji_images[i].code, \
            LoadTextureFromImage(emoji_img), \
        }; \
        UnloadImage(emoji_img); \
    }
    LIST_OF_EMOJI_SIZES
#undef X

    // Unload emoji images
    for (size_t i = 0; i < EMOJI_COUNT; i++) {
        UnloadImage(emoji_images[i].img);
    }
}

// TODO: replace '\n' with ' '
void common::draw_text_in_width(
        FontId font_id,
        Vector2 pos,
        const wchar_t *text, size_t text_len,
        Color color, float in_width)
{
    in_width -= 3*get_glyph_width(font_id, L'.'); // to reserve place for '...'

    float width = 0;
    for (size_t i = 0; i < text_len; i++) {
        float glyph_width = get_glyph_width(font_id, text[i]);
        if (width+glyph_width > in_width) {
            draw_wtext(font_id, pos, text, i, color);
            pos.x += width;
            draw_wtext(font_id, pos, L"...", 3, color);
            return;
        }

        width += glyph_width;
    }

    // If text fits in length draw it
    draw_wtext(font_id, pos, text, text_len, color);
}

void common::draw_lines(FontId font_id, Vector2 pos, common::Lines lines, Color color)
{
    size_t begin_x = pos.x;
    for (size_t i = 0; i < lines.len; i++) {
        draw_wtext(font_id, pos, lines.items[i].text, lines.items[i].len, color);
        pos.y += g_font_data[font_id].font.baseSize;
        pos.x = begin_x;
    }
}

float common::Lines::max_line_width(FontId font_id)
{
    float result = 0;
    for (size_t i = 0; i < this->len; i++) {
        float line_len = measure_wtext(font_id, this->items[i].text, this->items[i].len);
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

Vector2 common::Lines::get_vec_to_pos(FontId font_id, size_t row, size_t col)
{
    Vector2 ret = { 0, (float) row*g_font_data[font_id].font.baseSize };
    Line line = this->items[row];
    for (size_t i = 0; i < col; i++) {
        ret.x += get_glyph_width(font_id, line.text[i]);
    }

    return ret;
}

void common::Lines::recalc(FontId font_id, wchar_t *text, size_t text_len, float max_line_width)
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

        float glyph_width = get_glyph_width(font_id, text[i]);
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

float common::font_size(FontId font_id)
{
    return g_font_data[font_id].font.baseSize;
}

void common::draw_wtext(FontId font_id, Vector2 pos, const wchar_t *wtext, size_t wtext_len, Color color)
{
    for (size_t i = 0; i < wtext_len; i++) {
        wchar_t codepoint = wtext[i];
        if (is_emoji(codepoint)) {
            DrawTextureV(get_emoji_texture(font_id, codepoint), pos, WHITE);
        } else {
            DrawTextCodepoint(
                    g_font_data[font_id].font, codepoint,
                    pos, g_font_data[font_id].font.baseSize, color);
        }
        pos.x += get_glyph_width(font_id, codepoint);
    }
}

float common::measure_wtext(FontId font_id, const wchar_t *text, size_t text_len)
{
    float result = 0;
    for (size_t i = 0; i < text_len; i++) {
        result += get_glyph_width(font_id, text[i]);
    }

    return result;
}

// PRIVATE FUNCTION IMPLEMENTATIONS //////////////////////////////

static float get_glyph_width(FontId font_id, wchar_t codepoint)
{
    Font font = g_font_data[font_id].font;
    if (!is_emoji(codepoint)) {
        int cp_idx = GetGlyphIndex(font, codepoint);
        return (font.glyphs[cp_idx].advanceX == 0) ?
            font.recs[cp_idx].width :
            font.glyphs[cp_idx].advanceX;
    } else {
        return font.baseSize;
    }
}

static bool is_emoji(wchar_t codepoint)
{
    static_assert(EMOJI_SIZE_COUNT > 0, "'is_emoji' that we have emoji");
    for (size_t i = 0; i < EMOJI_COUNT; i++) {
        if (codepoint == g_emoji_arrays[0][i].code) return true;
    }

    return false;
}

static Texture get_emoji_texture(FontId font_id, wchar_t emoji_codepoint)
{
    Emoji *emoji = g_emoji_arrays[g_font_data[font_id].emoji_size];
    for (size_t i = 0; i < EMOJI_COUNT; i++) {
        if (emoji[i].code == emoji_codepoint) {
            return emoji[i].texture;
        }
    }

    assert(0 && "Unknown emoji");
}
