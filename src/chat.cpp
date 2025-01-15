#include <cmath>
#include <cwchar>
#include <cassert>
#include <cstring>
#include <cstdio>

#include <raylib.h>

#include "chat.h"
#include "common.h"
#include "ted.h"
#include "config.h"

static Font      chat_msg_author_name_font;
static size_t    chat_msg_author_name_font_glyph_width;
static Font      chat_msg_text_font;
static size_t    chat_msg_text_font_glyph_width;
static chat::Msg chat_messages[MESSAGES_CAPACITY];
static size_t    chat_message_count = 0;
static size_t    chat_selection_offset = 0;

chat::WStr chat::WStr::from(const char *cstr)
{
    WStr result = {};
    result.data = (wchar_t *) LoadCodepoints(cstr, (int *)&result.len);
    return result;
}

void chat::init()
{
    // Load msg author name font
    chat_msg_author_name_font = LoadFontEx(
            MSG_AUTHOR_NAME_FONT_PATH,
            MSG_AUTHOR_NAME_FONT_SIZE,
            nullptr, FONT_GLYPH_COUNT);
    chat_msg_author_name_font_glyph_width =
        MSG_AUTHOR_NAME_FONT_SIZE /
        chat_msg_author_name_font.baseSize *
        chat_msg_author_name_font.glyphs[0].advanceX;

    // Load msg text font
    chat_msg_text_font = LoadFontEx(
            MSG_TEXT_FONT_PATH,
            MSG_TEXT_FONT_SIZE,
            nullptr, FONT_GLYPH_COUNT);
    chat_msg_text_font_glyph_width =
        MSG_TEXT_FONT_SIZE /
        chat_msg_text_font.baseSize *
        chat_msg_text_font.glyphs[0].advanceX;
}

void chat::render(float bottom_margin)
{
    float height = GetScreenHeight();
    float width  = GetScreenWidth();

    // Calculate max line width
    float max_msg_line_width =
        floor(width - 2*MSG_TEXT_PADDING - 2*MSG_TEXT_MARGIN_LEFT_RIGHT);

    size_t selected_msg_idx = chat_message_count - chat_selection_offset;

    Color author_name_color;
    common::Lines lines = {};
    Rectangle msg_rect = { .y = height - bottom_margin };
    for (int i = chat_message_count-1; i >= 0; i--) {
        lines.recalc(
                chat_msg_text_font,
                MSG_TEXT_FONT_SIZE,
                chat_messages[i].text.data,
                chat_messages[i].text.len,
                max_msg_line_width);

        // Calculate height
        msg_rect.height = lines.len*MSG_TEXT_FONT_SIZE;
        msg_rect.height += MSG_AUTHOR_NAME_FONT_SIZE; // reserve place for 'username'
        msg_rect.height += 2*MSG_TEXT_PADDING;
        if (chat_messages[i].reply_to != nullptr) {
            msg_rect.height += 2*MSG_REPLY_MARGIN + 2*MSG_REPLY_PADDING + 2*MSG_TEXT_FONT_SIZE;
        }

        msg_rect.y -= MSG_DISTANCE + msg_rect.height;
        if (msg_rect.y+msg_rect.height < 0) return;

        { // Calculate width
            msg_rect.width = lines.max_line_width(chat_msg_text_font);

            int author_name_width = common::measure_wtext(
                    chat_msg_author_name_font,
                    &chat_messages[i].author_name[0],
                    chat_messages[i].author_name.length());

            if (msg_rect.width < author_name_width) msg_rect.width = author_name_width;

            if (chat_messages[i].reply_to != nullptr) {
                float reply_text_width = common::measure_wtext(
                        chat_msg_text_font,
                        chat_messages[i].reply_to->text.data,
                        chat_messages[i].reply_to->text.len);

                if (reply_text_width+2*MSG_REPLY_PADDING > msg_rect.width) { 
                    if (reply_text_width+2*MSG_REPLY_PADDING > max_msg_line_width) {
                        msg_rect.width = max_msg_line_width;
                    } else {
                        msg_rect.width = reply_text_width + 2*MSG_REPLY_PADDING;
                    }
                }
            }

            msg_rect.width += 2*MSG_TEXT_PADDING;
        }

        // If it is an our message we align it to the right and change the author name color
        if (chat_messages[i].is_mine) {
            author_name_color = MY_MSG_COLOR;
            msg_rect.x = width - msg_rect.width - MSG_TEXT_MARGIN_LEFT_RIGHT;
        } else {
            author_name_color = NOT_MY_MSG_COLOR;
            msg_rect.x = MSG_TEXT_MARGIN_LEFT_RIGHT;
        }

        // If the message is selected we show it
        if (i == selected_msg_idx) {
            DrawRectangle(0, msg_rect.y, width, msg_rect.height, MSG_SELECTED_COLOR);
        }

        // draw message background
        DrawRectangleRounded(
                msg_rect,
                MSG_REC_ROUNDNESS/msg_rect.height, MSG_REC_SEGMENT_COUNT,
                MSG_BG_COLOR);

        Vector2 pos = { msg_rect.x+MSG_TEXT_PADDING, msg_rect.y+MSG_TEXT_PADDING };

        // draw username
        DrawTextCodepoints(
                chat_msg_author_name_font,
                (const int *)&chat_messages[i].author_name[0],
                chat_messages[i].author_name.length(),
                pos, MSG_AUTHOR_NAME_FONT_SIZE, 0, author_name_color);

        if (chat_messages[i].reply_to != nullptr) {
            // Draw 'reply_to'
            pos.y += MSG_TEXT_FONT_SIZE + MSG_REPLY_MARGIN;
            DrawRectangleRounded(
                    (Rectangle){ pos.x, pos.y, msg_rect.width - 2*MSG_TEXT_PADDING, 2*MSG_REPLY_PADDING + 2*MSG_TEXT_FONT_SIZE },
                    MSG_REC_ROUNDNESS/msg_rect.height, MSG_REC_SEGMENT_COUNT,
                    MSG_REPLY_BG_COLOR);
            DrawTextCodepoints(
                    chat_msg_text_font,
                    (const int *)chat_messages[i].reply_to->text.data,
                    chat_messages[i].reply_to->text.len,
                    (Vector2){ pos.x+MSG_REPLY_PADDING, pos.y+MSG_REPLY_PADDING },
                    MSG_TEXT_FONT_SIZE, 0, RAYWHITE);
            pos.y += MSG_TEXT_FONT_SIZE + 2*MSG_REPLY_PADDING + MSG_REPLY_MARGIN;
        }

        // draw message
        pos.y += MSG_TEXT_FONT_SIZE;
        common::draw_lines(
                chat_msg_text_font, MSG_TEXT_FONT_SIZE,
                pos, lines, MSG_FG_COLOR);
    }
}

void chat::push_msg(Msg msg)
{
    if (chat_message_count >= MESSAGES_CAPACITY) {
        UnloadCodepoints((int*)chat_messages[0].text.data);
        memmove(&chat_messages[0], &chat_messages[1], (chat_message_count-1)*sizeof(chat::Msg));
        chat_message_count -= 1;
    }

    chat_messages[chat_message_count++] = msg;
}


chat::Msg *chat::find_msg(std::int64_t msg_id)
{
    for (int i = chat_message_count-1; i >= 0; i--) {
        if (chat_messages[i].id == msg_id) {
            return &chat_messages[i];
        }
    }

    return nullptr;
}

void chat::select_prev_msg()
{
    if (chat_selection_offset < chat_message_count) {
        chat_selection_offset += 1;
    }
}

void chat::select_next_msg()
{
    if (chat_selection_offset > 0) {
        chat_selection_offset -= 1;
    }
}

chat::Msg *chat::get_selected_msg()
{
    return chat_selection_offset == 0 ? nullptr :
           &chat_messages[chat_message_count - chat_selection_offset];
}

void chat::reset_selection()
{
    chat_selection_offset = 0;
}

chat::Msg *chat::get_msgs(size_t *count)
{
    *count = chat_message_count;
    return chat_messages;
}
