#include <cmath>
#include <cwchar>
#include <cassert>
#include <cstring>
#include <cstdio>

#include "chat.h"
#include "common.h"
#include "ted.h"
#include "config.h"

struct Message {
    wchar_t *text;
    size_t text_len;
    wchar_t *author_name;
    size_t author_name_len;
    bool is_mine;
    common::Lines lines;
    void free();
    static Message create(
            const wchar_t *text, size_t text_len,
            const wchar_t *author_name, size_t author_name_len, Color author_name_color);
};

static Message chat_messages[MAX_MSG_COUNT];
static size_t chat_message_count;
static size_t chat_max_msg_line_len;

void chat::init()
{
}

static size_t calc_max_line(common::Lines lines)
{
    size_t result = 0;
    for (size_t i = 0; i < lines.len; i++) {
        if (lines.items[i].len > result)
            result = lines.items[i].len;
    }

    return result;
}

void chat::render()
{
    float height = GetScreenHeight();
    float width  = GetScreenWidth();

    // Calculate max line
    chat_max_msg_line_len =
        floor((width-2*MSG_TEXT_PADDING-2*MSG_TEXT_MARGIN_LEFT_RIGHT) / common::state.glyph_width);

    Color author_name_color;
    float msg_block_width, msg_block_height;
    Vector2 msg_pos = (Vector2){ .y = height - ted::get_height() };
    for (int i = chat_message_count-1; i >= 0; i--) {
        msg_block_height = chat_messages[i].lines.len*FONT_SIZE;
        msg_block_height += FONT_SIZE; // reserve place for 'username'
        msg_block_height += 2*MSG_TEXT_PADDING;

        // calculate width
        msg_block_width = calc_max_line(chat_messages[i].lines)*common::state.glyph_width;
        size_t author_name_width = chat_messages[i].author_name_len*common::state.glyph_width;
        if (msg_block_width < author_name_width) msg_block_width = author_name_width;
        msg_block_width += 2*MSG_TEXT_PADDING;

        msg_pos.y -= MSG_DISTANCE + msg_block_height;

        if (msg_pos.y+msg_block_height < 0) return;

        // If it is an our message we align it to the right and change the author name color
        if (chat_messages[i].is_mine) {
            author_name_color = MY_MSG_COLOR;
            msg_pos.x = width - msg_block_width - MSG_TEXT_MARGIN_LEFT_RIGHT;
        } else {
            author_name_color = NOT_MY_MSG_COLOR;
            msg_pos.x = MSG_TEXT_MARGIN_LEFT_RIGHT;
        }

        // draw message background
        DrawRectangleRounded(
                (Rectangle){msg_pos.x, msg_pos.y, msg_block_width, msg_block_height},
                MSG_REC_ROUNDNESS/msg_block_height, MSG_REC_SEGMENT_COUNT,
                MSG_BG_COLOR);

        // draw username
        DrawTextCodepoints(
                common::state.font,
                (const int *)chat_messages[i].author_name,
                chat_messages[i].author_name_len,
                (Vector2){msg_pos.x+MSG_TEXT_PADDING, msg_pos.y+MSG_TEXT_PADDING},
                FONT_SIZE, 0, author_name_color);

        // draw message
        common::draw_lines(
                (Vector2){ msg_pos.x+MSG_TEXT_PADDING, msg_pos.y+MSG_TEXT_PADDING+FONT_SIZE },
                chat_messages[i].lines,
                MSG_FG_COLOR);
    }
}

void chat::push_msg(std::wstring_view text, std::wstring_view author_name, bool is_mine)
{
    assert(text.length() > 0);

    if (chat_message_count >= MAX_MSG_COUNT) {
        MemFree(chat_messages[0].text);
        MemFree(chat_messages[0].author_name);
        memmove(&chat_messages[0],
                &chat_messages[1],
                (chat_message_count-1) * sizeof(Message));
        chat_message_count -= 1;
    }

    // Create new message

    Message new_msg = {};

    // Copy text
    size_t text_size = text.length() * sizeof(int);
    new_msg.text = (wchar_t *) MemAlloc(text_size);
    new_msg.text_len = text.length();
    memcpy(new_msg.text, &text[0], text_size);
    new_msg.lines.recalc(new_msg.text, new_msg.text_len, chat_max_msg_line_len);

    // Copy author name
    size_t author_name_size = author_name.length() * sizeof(int);
    new_msg.author_name = (wchar_t *) MemAlloc(author_name_size);
    new_msg.author_name_len = author_name.length();
    memcpy(new_msg.author_name, &author_name[0], author_name_size);

    new_msg.is_mine = is_mine;

    chat_messages[chat_message_count++] = new_msg;
}
