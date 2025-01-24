#include <cmath>
#include <cwchar>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <iostream>

#include <raylib.h>

#include "chat.h"
#include "common.h"
#include "ted.h"
#include "config.h"

enum WidgetType {
    SENDER_NAME,
    REPLY,
    TEXT,
    COUNT,
};

struct Msg {
    chat::MsgData data;
    WidgetType widgets[WidgetType::COUNT];
    size_t widget_count;
};

// WidgetType::SENDER_NAME
static Vector2 widget_sender_name_size_fn(chat::MsgData *, float);
static void    widget_sender_name_render_fn(chat::MsgData *, Vector2, float, float);

// WidgetType::REPLY
static Vector2 widget_reply_size_fn(chat::MsgData *, float);
static void    widget_reply_render_fn(chat::MsgData *, Vector2, float, float);

// WidgetType::TEXT
static Vector2 widget_text_size_fn(chat::MsgData *, float);
static void    widget_text_render_fn(chat::MsgData *, Vector2, float, float);

// Global state
static Font   chat_msg_author_name_font;
static size_t chat_msg_author_name_font_glyph_width;
static Font   chat_msg_text_font;
static size_t chat_msg_text_font_glyph_width;
static Msg    chat_messages[MESSAGES_CAPACITY];
static size_t chat_message_count = 0;
static size_t chat_selection_offset = 0;
static struct {
    Vector2 (*size_fn)  (chat::MsgData *msg_data, float max_widget_width);
    void    (*render_fn)(chat::MsgData *msg_data, Vector2 pos, float max_widget_width, float width);
} widget_vtable[] = {
    { widget_sender_name_size_fn, widget_sender_name_render_fn },
    { widget_reply_size_fn,       widget_reply_render_fn },
    { widget_text_size_fn,        widget_text_render_fn },
};

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

    // Calculate max line width
    float max_msg_line_width =
        floor(CHAT_VIEW_WIDTH - 2*MSG_TEXT_PADDING - 2*MSG_TEXT_MARGIN_LEFT_RIGHT);

    int selected_msg_idx = chat_message_count - chat_selection_offset;

    float heights[WidgetType::COUNT];
    Rectangle msg_rect = { MSG_TEXT_MARGIN_LEFT_RIGHT, height - bottom_margin, 0, 0 };
    for (int i = chat_message_count-1; i >= 0; i--) {
        WidgetType *widgets = chat_messages[i].widgets;
        size_t widget_count = chat_messages[i].widget_count;

        // Calculate message rectangle size
        msg_rect.width = 0;
        msg_rect.height = 0;
        size_t height_count = 0;
        for (size_t j = 0; j < widget_count; j++) {
            Vector2 size = widget_vtable[widgets[j]].size_fn(
                    &chat_messages[i].data,
                    max_msg_line_width);
            heights[height_count++] = size.y;
            msg_rect.height += size.y;
            if (size.x > msg_rect.width) msg_rect.width = size.x;
        }

        // Calculate position
        msg_rect.width += 2*MSG_TEXT_PADDING;
        msg_rect.height += 2*MSG_TEXT_PADDING;
        msg_rect.y -= msg_rect.height + MSG_DISTANCE;
        if (chat_messages[i].data.is_mine) {
            msg_rect.x = common::get_chat_view_x() + CHAT_VIEW_WIDTH - msg_rect.width - MSG_TEXT_MARGIN_LEFT_RIGHT;
        } else {
            msg_rect.x = common::get_chat_view_x() + MSG_TEXT_MARGIN_LEFT_RIGHT;
        }

        // Render selection if the message is selected
        if (i == selected_msg_idx) {
            DrawRectangle(0, msg_rect.y, GetScreenWidth(), msg_rect.height, MSG_SELECTED_COLOR);
        }

        // Render message rectangle
        DrawRectangleRounded(
                msg_rect, MSG_REC_ROUNDNESS/msg_rect.height, MSG_REC_SEGMENT_COUNT,
                msg_color_palette[chat_messages[i].data.is_mine].bg_color);

        // Render message widgets
        Vector2 pos = { msg_rect.x+MSG_TEXT_PADDING, msg_rect.y+MSG_TEXT_PADDING };
        for (size_t j = 0; j < widget_count; j++) {
            widget_vtable[widgets[j]].render_fn(
                    &chat_messages[i].data,
                    pos, max_msg_line_width,
                    msg_rect.width - 2*MSG_TEXT_PADDING);
            pos.y += heights[j];
        }
    }
}

void chat::push_msg(MsgData msg_data)
{
    Msg new_msg = {};
    new_msg.data = msg_data;

    if (msg_data.is_mine) {
        chat_selection_offset = 0;
    } else {
        new_msg.widgets[new_msg.widget_count++] = WidgetType::SENDER_NAME;
        if (chat_selection_offset != 0) {
            chat_selection_offset += 1;
        }
    }

    if (msg_data.reply_to != nullptr) {
        new_msg.widgets[new_msg.widget_count++] = WidgetType::REPLY;
    }

    new_msg.widgets[new_msg.widget_count++] = WidgetType::TEXT;

    if (chat_message_count >= MESSAGES_CAPACITY) {
        UnloadCodepoints((int*)chat_messages[0].data.text.data);
        memmove(&chat_messages[0], &chat_messages[1], (chat_message_count-1)*sizeof(Msg));
        chat_message_count -= 1;
    }

    chat_messages[chat_message_count++] = new_msg;
}


chat::MsgData *chat::find_msg(std::int64_t msg_id)
{
    for (int i = chat_message_count-1; i >= 0; i--) {
        if (chat_messages[i].data.id == msg_id) {
            return &chat_messages[i].data;
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

chat::MsgData *chat::get_selected_msg()
{
    return chat_selection_offset == 0 ? nullptr :
           &chat_messages[chat_message_count - chat_selection_offset].data;
}

// WIDGET FUNCTIONS IMPLS ///////////////

static Vector2 widget_sender_name_size_fn(chat::MsgData *msg_data, float max_line_len)
{
    float width = common::measure_wtext(
            chat_msg_author_name_font,
            &msg_data->sender_name[0],
            msg_data->sender_name.length());

    return { width > max_line_len ? max_line_len : width, MSG_AUTHOR_NAME_FONT_SIZE };
}

static void widget_sender_name_render_fn(chat::MsgData *msg_data, Vector2 pos, float max_line_len, float)
{
    common::draw_text_in_width(
            chat_msg_author_name_font, MSG_AUTHOR_NAME_FONT_SIZE,
            pos, &msg_data->sender_name[0], msg_data->sender_name.length(),
            msg_color_palette[msg_data->is_mine].sender_name_color, max_line_len);
}

static common::Lines text_lines = {};
static Vector2 widget_text_size_fn(chat::MsgData *msg_data, float max_line_len)
{
    text_lines.recalc(
            chat_msg_text_font,
            msg_data->text.data, msg_data->text.len,
            max_line_len);

    return { text_lines.max_line_width(chat_msg_text_font), text_lines.len*MSG_TEXT_FONT_SIZE };
}

static void widget_text_render_fn(chat::MsgData *msg_data, Vector2 pos, float, float)
{
    common::draw_lines(
            chat_msg_text_font, MSG_TEXT_FONT_SIZE,
            pos, text_lines, msg_color_palette[msg_data->is_mine].fg_color);
}

static Vector2 widget_reply_size_fn(chat::MsgData *msg_data, float max_line_len)
{
    float reply_text_width = common::measure_wtext(
            chat_msg_text_font,
            msg_data->reply_to->text.data,
            msg_data->reply_to->text.len);

    float reply_sender_name_width = common::measure_wtext(
            chat_msg_text_font,
            &msg_data->reply_to->sender_name[0],
            msg_data->reply_to->sender_name.length());

    float width = 
        (reply_text_width > reply_sender_name_width ?
         reply_text_width :
         reply_sender_name_width) + 2*MSG_REPLY_PADDING;

    return { width > max_line_len ? max_line_len : width, 2*MSG_REPLY_PADDING + 2*MSG_TEXT_FONT_SIZE };
}

static void widget_reply_render_fn(chat::MsgData *msg_data, Vector2 pos, float max_widget_width, float width)
{
    Rectangle reply_rect = { pos.x, pos.y, width, 2*MSG_REPLY_PADDING + 2*MSG_TEXT_FONT_SIZE };
    DrawRectangleRounded(
            reply_rect, MSG_REC_ROUNDNESS/reply_rect.height,
            MSG_REC_SEGMENT_COUNT, msg_color_palette[msg_data->is_mine].reply_bg_color);

    pos.x += MSG_REPLY_PADDING;
    pos.y += MSG_REPLY_PADDING;

    common::draw_text_in_width(
            chat_msg_author_name_font, MSG_AUTHOR_NAME_FONT_SIZE,
            pos, &msg_data->reply_to->sender_name[0],
            msg_data->reply_to->sender_name.length(),
            msg_color_palette[msg_data->reply_to->is_mine].sender_name_color, max_widget_width);

    pos.y += MSG_AUTHOR_NAME_FONT_SIZE;

    common::draw_text_in_width(
            chat_msg_text_font, MSG_TEXT_FONT_SIZE,
            pos, msg_data->reply_to->text.data,
            msg_data->reply_to->text.len,
            msg_color_palette[msg_data->is_mine].fg_color, max_widget_width);
}
