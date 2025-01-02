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
    common::Lines lines;
    int *author_name;
    size_t author_name_len;
    void free();
    static Message create(const int *text, size_t text_len, const int *author_name, size_t author_name_len);
};

static Message chat_messages[MAX_MSG_COUNT];
static size_t chat_message_count;
static size_t chat_max_msg_line_len;

void chat::init()
{
   chat_max_msg_line_len =
        floor((DEFAULT_WIDTH-2*MSG_TEXT_PADDING-2*MSG_TEXT_MARGIN) / common::state.glyph_width);
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
    float width, height;
    Vector2 pos = {
        .x = MSG_TEXT_MARGIN,
        .y = DEFAULT_HEIGHT - ted::get_height()
    };

    for (int i =chat_message_count-1; i >= 0; i--) {
        height = chat_messages[i].lines.len*FONT_SIZE;
        height += FONT_SIZE; // reserve place for 'username'
        height += 2*MSG_TEXT_PADDING;

        // calculate width
        width = calc_max_line(chat_messages[i].lines)*common::state.glyph_width;
        size_t author_name_width = chat_messages[i].author_name_len*common::state.glyph_width;
        if (width < author_name_width) width = author_name_width;
        width += 2*MSG_TEXT_PADDING;

        pos.y -= MSG_TEXT_MARGIN + height;

        // draw message background
        DrawRectangleRounded(
                (Rectangle){pos.x, pos.y, width, height},
                MSG_REC_ROUNDNESS/height, MSG_REC_SEGMENT_COUNT,
                MSG_BG_COLOR);

        // draw username
        DrawTextCodepoints(
                common::state.font,
                chat_messages[i].author_name,
                chat_messages[i].author_name_len,
                (Vector2){ pos.x+MSG_TEXT_PADDING, pos.y+MSG_TEXT_PADDING},
                FONT_SIZE, 0, MSG_AUTHOR_NAME_COLOR);

        // draw message
        common::draw_lines(
                (Vector2){ pos.x+MSG_TEXT_PADDING, pos.y+MSG_TEXT_PADDING+FONT_SIZE },
                chat_messages[i].lines,
                MSG_FG_COLOR);
    }
}

Message Message::create(
        const int *text, size_t text_len,
        const int *author_name, size_t author_name_len)
{
    Message new_msg = {};

    // Copy text
    size_t text_size = text_len * sizeof(int);
    new_msg.lines.text = (int *) MemAlloc(text_size);
    new_msg.lines.text_len = text_len;
    memcpy(new_msg.lines.text, text, text_size);
    new_msg.lines.recalc(chat_max_msg_line_len);

    // Copy author name
    size_t author_name_size = author_name_len * sizeof(int);
    new_msg.author_name = (int *) MemAlloc(author_name_size);
    new_msg.author_name_len = author_name_len;
    memcpy(new_msg.author_name, author_name, author_name_size);

    return new_msg;
}

void Message::free()
{
    MemFree(this->lines.text);
    MemFree(this->author_name);
}

void chat::push_err(const int *err_msg)
{
    chat::push_msg(err_msg, wcslen((wchar_t *)err_msg), (int *)L"Error", 5);
}

void chat::push_msg(
        const int *msg, size_t msg_len,
        const int *author_name, size_t author_name_len)
{
    assert(msg_len > 0);

    if (chat_message_count >= MAX_MSG_COUNT) {
        chat_messages[0].free();
        memmove(&chat_messages[0],
                &chat_messages[1],
                (chat_message_count-1) * sizeof(Message));
        chat_message_count -= 1;
    }

    chat_messages[chat_message_count++] =
        Message::create(msg, msg_len, author_name, author_name_len);
}
