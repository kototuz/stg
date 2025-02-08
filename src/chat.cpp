#include <cmath>
#include <cwchar>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <map>
#include <codecvt>
#include <locale>

#include <raylib.h>

#include "chat.h"
#include "common.h"
#include "config.h"
#include "tgclient.h"

#define MESSAGES_CAPACITY 10
#define MSG_COUNT_TO_LOAD_WHEN_OPEN_CHAT MESSAGES_CAPACITY

#define TED_MAX_MSG_LEN         4096
#define TED_MAX_PLACEHOLDER_LEN 32
#define TED_ARGS_CAPACITY       2

#define LIST_OF_WIDGETS \
    X(SENDER_NAME, widget_sender_name_size_fn, widget_sender_name_render_fn) \
    X(REPLY,       widget_reply_size_fn,       widget_reply_render_fn) \
    X(TEXT,        widget_text_size_fn,        widget_text_render_fn) \

#define LIST_OF_COMMANDS \
    X(L"l",  cmd_logout) \
    X(L"c",  cmd_list_chats) \
    X(L"sc", cmd_select_chat) \

struct Pos {
    size_t col;
    size_t row;
};

typedef std::wstring_view Arg;

typedef void (*Command)();

struct WStr {
    wchar_t *data;
    size_t len;

    static WStr from(const char *cstr)
    {
        WStr result = {};
        result.data = (wchar_t *) LoadCodepoints(cstr, (int *)&result.len);
        return result;
    }

    static WStr copy(WStr from)
    {
        WStr result = {};
        result.data = (wchar_t *) calloc(from.len, sizeof(wchar_t));
        memcpy(result.data, from.data, from.len*sizeof(wchar_t));
        result.len = from.len;
        return result;
    }
};

enum WidgetTag {
#define X(tag_name, ...) tag_name,
    LIST_OF_WIDGETS
#undef X
        COUNT,
};

struct Widget {
    WidgetTag tag;
    Vector2 size;
};

struct Msg {
    std::int64_t id;
    WStr text;
    common::Lines text_lines;
    std::wstring_view sender_name;
    bool is_mine;
    Vector2 size;
    size_t widget_count;
    Widget widgets[WidgetTag::COUNT];
    bool has_reply_to;
    struct {
        std::int64_t id;
        WStr text;
        std::wstring_view sender_name;
        bool is_mine;
    } reply_to;
};

enum Motion {
    MOTION_FORWARD_WORD,
    MOTION_BACKWARD_WORD,
    MOTION_FORWARD,
    MOTION_BACKWARD,
    MOTION_UP,
    MOTION_DOWN,
    MOTION_END,
    MOTION_BEGIN,
};

// Declare command functions
#define X(name, func_name) static void func_name();
LIST_OF_COMMANDS
#undef X

// Declare text editor functions
static void ted_send();
static void ted_try_cursor_motion(Motion m);
static void ted_insert_symbol(int s);
static void ted_delete_symbols(size_t count);
static void ted_delete_word();
static void ted_delete_line();
static void ted_move_cursor_to_ptr(wchar_t *ptr);
static void ted_clear();
static void ted_run_command();

// Declare message list functions
static void load_msgs(td_api::object_ptr<td_api::messages> msgs);
static void push_msg(td_api::object_ptr<td_api::message> msg);
static Msg *find_msg(std::int64_t msg_id);

// Declare util functions
static std::int64_t to_int64_t(std::wstring_view text);

// Declare widget functions
#define X(tag_name, size_fn, render_fn) \
    static Vector2 size_fn(Msg*); \
    static void render_fn(Msg*, Vector2, float);
LIST_OF_WIDGETS
#undef X

// GLOBAL STATE //////////////////////////////

// Message list global state
static Msg    chat_messages[MESSAGES_CAPACITY];
static size_t chat_message_count = 0;
static size_t chat_selection_offset = 0;
static std::int64_t chat_id = 0;
static float constexpr max_msg_widget_width = floor(CHAT_VIEW_WIDTH - BoxModel::MSG_LM - BoxModel::MSG_LP - BoxModel::MSG_RP - BoxModel::MSG_RM);
static struct {
    Vector2 (*size_fn)  (Msg *msg_data);
    void    (*render_fn)(Msg *msg_data, Vector2 pos, float width);
} widget_vtable[] = {
#define X(tag_name, size_fn, render_fn) { size_fn, render_fn },
    LIST_OF_WIDGETS
#undef X
};

// Text editor global state
static common::Lines ted_lines;
static Pos           ted_cursor_pos;
static bool          ted_cursor_has_space = false;
static float         ted_max_line_width;
static wchar_t       ted_buffer[TED_MAX_MSG_LEN];
static size_t        ted_buffer_len = 0;
static wchar_t       ted_placeholder[TED_MAX_PLACEHOLDER_LEN];
static size_t        ted_placeholder_len;
static Arg           ted_args[TED_ARGS_CAPACITY];
static size_t        ted_arg_count = 0;
static std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
static std::map<std::wstring_view, Command> command_map = {
#define X(name, func_name) { name, func_name },
    LIST_OF_COMMANDS
#undef X
};

// PUBLIC FUNCTION IMPLEMENTATIONS ///////

void chat::init()
{
    ted_lines = common::Lines{};
    ted_lines.grow_one();
    ted_lines.items[0].text = ted_buffer;
    ted_lines.items[0].len = 0;
    ted_cursor_pos = Pos{};
}

void chat::update()
{
    if      (KEYMAP_SELECT_PREV)        { if (chat_selection_offset < chat_message_count) { chat_selection_offset += 1; } }
    else if (KEYMAP_SELECT_NEXT)        { if (chat_selection_offset > 0) { chat_selection_offset -= 1; } }
    else if (KEYMAP_MOVE_FORWARD)       ted_try_cursor_motion(MOTION_FORWARD);
    else if (KEYMAP_MOVE_BACKWARD)      ted_try_cursor_motion(MOTION_BACKWARD);
    else if (KEYMAP_MOVE_FORWARD_WORD)  ted_try_cursor_motion(MOTION_FORWARD_WORD);
    else if (KEYMAP_MOVE_BACKWARD_WORD) ted_try_cursor_motion(MOTION_BACKWARD_WORD);
    else if (KEYMAP_MOVE_UP)            ted_try_cursor_motion(MOTION_UP);
    else if (KEYMAP_MOVE_DOWN)          ted_try_cursor_motion(MOTION_DOWN);
    else if (KEYMAP_MOVE_END)           ted_try_cursor_motion(MOTION_END);
    else if (KEYMAP_MOVE_BEGIN)         ted_try_cursor_motion(MOTION_BEGIN);
    else if (KEYMAP_DELETE_WORD)        ted_delete_word();
    else if (KEYMAP_DELETE_LINE)        ted_delete_line();
    else if (KEYMAP_DELETE)             ted_delete_symbols(1);
    else if (KEYMAP_NEW_LINE)           ted_insert_symbol('\n');
    else if (KEYMAP_SEND_MESSAGE)       ted_send();
    else { // just insert char
        int symbol = GetCharPressed();
        if (symbol != 0) ted_insert_symbol(symbol);
    }
}

void chat::render()
{
    DrawFPS(0, 0);

    DrawText(TextFormat("Count: %zu\n", chat_message_count), 0, 24, 24, RAYWHITE);

    float ted_font_size = common::font_size(TED_FONT_ID);
    Vector2 chat_view_pos = {
        (GetScreenWidth()/2) - (CHAT_VIEW_WIDTH/2),
        GetScreenHeight() - (ted_lines.len*ted_font_size +
                BoxModel::TED_BM + BoxModel::TED_BP +
                BoxModel::TED_TP + BoxModel::TED_TM)
    };

    { // Render msg list
        Vector2 msg_pos = { 0, chat_view_pos.y };
        Msg *end = &chat_messages[-1];
        Msg *selected_msg = chat_selection_offset > 0 ?
            &chat_messages[chat_message_count-chat_selection_offset] :
            nullptr;
        for (Msg *it = &chat_messages[chat_message_count-1]; it != end; it--) {
            // Calculate message position
            msg_pos.y -= it->size.y + MSG_DISTANCE;
            if (it->is_mine) {
                msg_pos.x = chat_view_pos.x + CHAT_VIEW_WIDTH - it->size.x - BoxModel::MSG_LM - BoxModel::MSG_RM;
            } else {
                msg_pos.x = chat_view_pos.x + BoxModel::MSG_LM;
            }

            if (it == selected_msg) {
                DrawRectangle(0, msg_pos.y, GetScreenWidth(), it->size.y, MSG_SELECTED_COLOR);
            }

            // Render message rectangle
            DrawRectangleRounded(
                    { msg_pos.x, msg_pos.y, it->size.x, it->size.y },
                    MSG_REC_ROUNDNESS/it->size.y, MSG_REC_SEGMENT_COUNT,
                    msg_color_palette[it->is_mine].bg_color);

            // Render message widgets
            float curr_max_msg_widget_width = it->size.x - BoxModel::MSG_LP - BoxModel::MSG_RP;
            Vector2 widget_pos = { msg_pos.x+BoxModel::MSG_LP, msg_pos.y+BoxModel::MSG_TP };
            for (size_t i = 0; i < it->widget_count; i++) {
                widget_vtable[it->widgets[i].tag].render_fn(it, widget_pos,
                        curr_max_msg_widget_width);
                widget_pos.y += it->widgets[i].size.y;
            }
        }
    }

    /* { // Render msg list */
    /*     // Calculate max line width */
    /*     float max_msg_line_width = */
    /*         floor(CHAT_VIEW_WIDTH - */
    /*                 BoxModel::MSG_LM - BoxModel::MSG_LP - */
    /*                 BoxModel::MSG_RP - BoxModel::MSG_RM); */
    /**/
    /*     int selected_msg_idx = chat_message_count - chat_selection_offset; */
    /**/
    /*     float chat_view_bot_pos_y = height -  */
    /*         (ted_lines.len*ted_font_size + */
    /*         BoxModel::TED_BM + BoxModel::TED_BP + */
    /*         BoxModel::TED_TP + BoxModel::TED_TM); */
    /**/
    /*     // TODO: I think it's terrible */
    /*     // ================== */
    /*     // Calculate chat_view's start message idx and start y */
    /*     float msg_list_bot_pos_y = chat_view_bot_pos_y + offset; */
    /*     float y_with_scroll = msg_list_bot_pos_y + scroll; */
    /*     if (msg_list_height >= chat_view_bot_pos_y) { */
    /*         if (y_with_scroll < chat_view_bot_pos_y && msg_begin_idx+1 == chat_message_count) { */
    /*             msg_list_bot_pos_y = chat_view_bot_pos_y; */
    /*         } else if (y_with_scroll < height && msg_begin_idx+1 < chat_message_count) { */
    /*             msg_begin_idx += 1; */
    /*             msg_list_bot_pos_y += scroll; */
    /*             msg_list_bot_pos_y += MSG_DISTANCE + calc_msg_height(msg_begin_idx, max_msg_line_width); */
    /*         } else if (y_with_scroll-msg_list_height > MSG_DISTANCE) { */
    /*             msg_list_bot_pos_y = MSG_DISTANCE + msg_list_height; */
    /*         } else { */
    /*             msg_list_bot_pos_y += scroll; */
    /*             for (; msg_begin_idx >= 0; msg_begin_idx--) { */
    /*                 float msg_height = calc_msg_height(msg_begin_idx, max_msg_line_width); */
    /*                 if (msg_list_bot_pos_y-msg_height-MSG_DISTANCE < height) break; */
    /*                 msg_list_bot_pos_y -= msg_height + MSG_DISTANCE; */
    /*             } */
    /*         } */
    /*     } */
    /**/
    /*     float heights[WidgetTag::COUNT]; */
    /*     Rectangle msg_rect = { BoxModel::MSG_LM, msg_list_bot_pos_y, 0, 0 }; */
    /*     for (int i = msg_begin_idx; i >= 0; i--) { */
    /*         if (msg_rect.y < 0) break; */
    /**/
    /*         WidgetTag *widgets = chat_messages[i].widgets; */
    /*         size_t widget_count = chat_messages[i].widget_count; */
    /**/
    /*         // Calculate message rectangle size */
    /*         msg_rect.width = 0; */
    /*         msg_rect.height = (widget_count-1) * MSG_WIDGET_DISTANCE; */
    /*         size_t height_count = 0; */
    /*         for (size_t j = 0; j < widget_count; j++) { */
    /*             Vector2 size = widget_vtable[widgets[j]].size_fn( */
    /*                     &chat_messages[i], */
    /*                     max_msg_line_width); */
    /*             heights[height_count++] = size.y; */
    /*             msg_rect.height += size.y; */
    /*             if (size.x > msg_rect.width) msg_rect.width = size.x; */
    /*         } */
    /**/
    /*         // Calculate position */
    /*         msg_rect.width += BoxModel::MSG_LP + BoxModel::MSG_RP; */
    /*         msg_rect.height += BoxModel::MSG_TP + BoxModel::MSG_BP; */
    /*         msg_rect.y -= msg_rect.height + MSG_DISTANCE; */
    /*         if (chat_messages[i].is_mine) { */
    /*             msg_rect.x = chat_view_x + CHAT_VIEW_WIDTH - msg_rect.width - BoxModel::MSG_LM - BoxModel::MSG_RM; */
    /*         } else { */
    /*             msg_rect.x = chat_view_x + BoxModel::MSG_LM; */
    /*         } */
    /**/
    /*         // Render selection if the message is selected */
    /*         if (i == selected_msg_idx) { */
    /*             DrawRectangle(0, msg_rect.y, GetScreenWidth(), msg_rect.height, MSG_SELECTED_COLOR); */
    /*         } */
    /**/
    /*         // Render message rectangle */
    /*         DrawRectangleRounded( */
    /*                 msg_rect, MSG_REC_ROUNDNESS/msg_rect.height, MSG_REC_SEGMENT_COUNT, */
    /*                 msg_color_palette[chat_messages[i].is_mine].bg_color); */
    /**/
    /*         // Render message widgets */
    /*         Vector2 pos = { msg_rect.x+BoxModel::MSG_LP, msg_rect.y+BoxModel::MSG_TP }; */
    /*         for (size_t j = 0; j < widget_count; j++) { */
    /*             widget_vtable[widgets[j]].render_fn( */
    /*                     &chat_messages[i], */
    /*                     pos, max_msg_line_width, */
    /*                     msg_rect.width - BoxModel::MSG_LP - BoxModel::MSG_RP); */
    /*             pos.y += heights[j] + MSG_WIDGET_DISTANCE; */
    /*         } */
    /*     } */
    /**/
    /*     offset = msg_list_bot_pos_y - chat_view_bot_pos_y; */
    /*     msg_list_height = msg_list_bot_pos_y - msg_rect.y; */
    /* } */

    { // Render text editor
        // Calculate max line
        ted_max_line_width =
            floor(CHAT_VIEW_WIDTH -
                    BoxModel::TED_LM - BoxModel::TED_LP -
                    BoxModel::TED_RP - BoxModel::TED_RM);

        // Render text editor rectangle
        Rectangle ted_rec = {};
        ted_rec.width = CHAT_VIEW_WIDTH - BoxModel::TED_LM - BoxModel::TED_RM;
        ted_rec.height = ted_lines.len*ted_font_size + BoxModel::TED_TP + BoxModel::TED_BP;
        ted_rec.x = chat_view_pos.x + BoxModel::TED_LM;
        ted_rec.y = GetScreenHeight() - ted_rec.height - BoxModel::TED_BM;
        DrawRectangleRounded(ted_rec, TED_REC_ROUNDNESS/ted_rec.height, TED_REC_SEGMENT_COUNT, TED_BG_COLOR);

        // Render placeholder or text if it exists
        Vector2 pos = { ted_rec.x + BoxModel::TED_LP, ted_rec.y + BoxModel::TED_TP };
        if (ted_buffer_len == 0) {
            common::draw_wtext(TED_FONT_ID, pos, ted_placeholder,
                    ted_placeholder_len, TED_PLACEHOLDER_COLOR);
        } else {
            ted_set_placeholder(L"Message...");
            common::draw_lines(TED_FONT_ID, pos, ted_lines, TED_FG_COLOR);
        }

        // Render cursor
        Vector2 vec_to_pos = ted_lines.get_vec_to_pos(TED_FONT_ID,
                ted_cursor_pos.row, ted_cursor_pos.col);
        pos.x += vec_to_pos.x;
        pos.y += vec_to_pos.y;
        DrawLine(pos.x, pos.y, pos.x, pos.y+ted_font_size, TED_CURSOR_COLOR);
    }
}

void chat::ted_set_placeholder(const wchar_t *text)
{
    ted_placeholder_len = wcslen(text);
    if (ted_placeholder_len > TED_MAX_PLACEHOLDER_LEN) {
        memcpy(ted_placeholder, text, (TED_MAX_PLACEHOLDER_LEN-1) * sizeof(int));
        ted_placeholder[TED_MAX_PLACEHOLDER_LEN-1] = L'…';
        ted_placeholder_len = TED_MAX_PLACEHOLDER_LEN;
    } else {
        memcpy(ted_placeholder, text, ted_placeholder_len * sizeof(int));
    }
}

void chat::update_new_msg(td_api::object_ptr<td_api::updateNewMessage> u)
{
    if (u->message_->chat_id_ != chat_id) return;
    push_msg(std::move(u->message_));
}

void chat::update_msg_send_succeeded(td_api::object_ptr<td_api::updateMessageSendSucceeded> u)
{
    Msg *old_msg = find_msg(u->old_message_id_);
    /* std::wcout << old_msg->id << " -> " << u->message_->id_ << "\n"; */
    if (old_msg != nullptr) old_msg->id = u->message_->id_;
}

// PRIVATE FUNCTION IMPLEMENTATIONS //////////

static void ted_send()
{
    if (ted_buffer_len == 0) return;

    std::string text_utf8 = converter.to_bytes(ted_buffer, &ted_buffer[ted_buffer_len]);
    switch (tgclient::state) {
    case tgclient::STATE_NONE:
        break;

    case tgclient::STATE_WAIT_CODE:
        tgclient::request(
                td_api::make_object<td_api::checkAuthenticationCode>(
                    text_utf8));
        break;

    case tgclient::STATE_WAIT_PHONE_NUMBER:
        tgclient::request(
                td_api::make_object<td_api::setAuthenticationPhoneNumber>(
                    text_utf8, nullptr));
        break;

    case tgclient::STATE_FREETIME:
        if (ted_buffer[0] == COMMAND_START_SYMBOL) {
            ted_run_command();
        } else if (chat_id == 0) {
            chat::ted_set_placeholder(L"Chat is not selected");
        } else {
            auto send_message = td_api::make_object<td_api::sendMessage>();
            send_message->chat_id_ = chat_id;
            auto message_content = td_api::make_object<td_api::inputMessageText>();
            message_content->text_ = td_api::make_object<td_api::formattedText>();
            message_content->text_->text_ = std::move(text_utf8);
            send_message->input_message_content_ = std::move(message_content);

            if (chat_selection_offset > 0) {
                send_message->reply_to_ =
                    td_api::make_object<td_api::inputMessageReplyToMessage>(
                            chat_messages[chat_message_count - chat_selection_offset].id,
                            nullptr);
            }

            tgclient::request(std::move(send_message));
        }
        break;
    }

    ted_clear();
}

static void ted_try_cursor_motion(Motion m)
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
            ted_move_cursor_to_ptr(curr_text_ptr+1);
            return;

        case MOTION_BACKWARD_WORD:;
            curr_text_ptr = &ted_lines.items[p.row].text[p.col];
            ptr = ted_buffer-1; // set to the begin text ptr
            if (curr_text_ptr[-1] == ' ') curr_text_ptr--;
            while (curr_text_ptr != ptr && *curr_text_ptr == ' ') curr_text_ptr--; // move to the word end
            while (curr_text_ptr != ptr && *curr_text_ptr != ' ') curr_text_ptr--; // move to the word begin
            ted_move_cursor_to_ptr(curr_text_ptr+1);
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

static void ted_insert_symbol(int s)
{
    if (ted_buffer_len+1 >= TED_MAX_MSG_LEN) return;

    // move text after cursor
    wchar_t *text_curr_ptr = &ted_lines.items[ted_cursor_pos.row].text[ted_cursor_pos.col+ted_cursor_has_space];
    wchar_t *text_end_ptr = &ted_buffer[ted_buffer_len];
    size_t size = (text_end_ptr - text_curr_ptr) * sizeof(int);
    memmove(text_curr_ptr+1, text_curr_ptr, size);

    *text_curr_ptr = s;
    ted_buffer_len += 1;

    ted_lines.recalc(TED_FONT_ID, ted_buffer, ted_buffer_len, ted_max_line_width);
    ted_move_cursor_to_ptr(text_curr_ptr+1);
}

static void ted_delete_symbols(size_t count)
{
    if (ted_buffer_len < count) return;
    if (ted_cursor_pos.col == 0 && ted_cursor_pos.row == 0) return;

    wchar_t *text_curr_ptr = &ted_lines.items[ted_cursor_pos.row].text[ted_cursor_pos.col];
    wchar_t *text_end_ptr = &ted_buffer[ted_buffer_len];
    size_t size = (text_end_ptr - text_curr_ptr) * sizeof(int);

    wchar_t *new_curr_text_ptr = text_curr_ptr - count;
    memmove(new_curr_text_ptr, text_curr_ptr, size);
    ted_buffer_len -= text_curr_ptr - new_curr_text_ptr;

    ted_lines.recalc(TED_FONT_ID, ted_buffer, ted_buffer_len, ted_max_line_width);
    ted_move_cursor_to_ptr(new_curr_text_ptr);
}

static void ted_delete_word()
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

    ted_delete_symbols(curr_text_ptr - new_curr_text_ptr);
}

static void ted_delete_line()
{
    wchar_t *curr_text_ptr = &ted_lines.items[ted_cursor_pos.row].text[ted_cursor_pos.col];
    wchar_t *new_text_ptr = ted_lines.items[ted_cursor_pos.row].text;
    ted_delete_symbols(curr_text_ptr - new_text_ptr);
}

static void ted_move_cursor_to_ptr(wchar_t *text_ptr)
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

static void ted_clear()
{
    ted_buffer_len = 0;
    ted_lines.len = 0;
    ted_cursor_pos = Pos{};
    ted_lines.grow_one();
}

static void ted_run_command()
{
    ted_arg_count = 0; // clear 'ted_args'

    // Split text into args
    size_t len = 0;
    std::wstring_view src(ted_buffer, ted_buffer_len);
    for (;;) {
        if (src.length() == 0) break;
        if (len == src.length()) {
            if (ted_arg_count >= TED_ARGS_CAPACITY) {
                chat::ted_set_placeholder(L"Arg buffer is overflowed");
                return;
            }
            ted_args[ted_arg_count++] = src;
            break;
        }

        if (src[len] != ' ') { len++; continue; }

        if (ted_arg_count >= TED_ARGS_CAPACITY) {
            chat::ted_set_placeholder(L"Arg buffer is overflowed");
            return;
        }

        ted_args[ted_arg_count++] = std::wstring_view(&src[0], len);
        src.remove_prefix(len);
        src.remove_prefix(std::min(src.find_first_not_of(L" "), src.size()));
        len = 0;
    }

    // Find command
    ted_args[0].remove_prefix(1); // without 'COMMAND_START_SYMBOL'
    auto res = command_map.find(ted_args[0]);
    if (res == command_map.end()) {
        chat::ted_set_placeholder(L"Command not found");
        return;
    }

    res->second();
}

static void push_msg(td_api::object_ptr<td_api::message> tg_msg)
{
    Msg new_msg = {};

    new_msg.id = tg_msg->id_;
    new_msg.is_mine = tg_msg->is_outgoing_;

    // Get message author name
    if (tg_msg->sender_id_->get_id() == td_api::messageSenderUser::ID) {
        auto id = static_cast<td_api::messageSenderUser &>(*tg_msg->sender_id_).user_id_;
        new_msg.sender_name = tgclient::username(id);
    } else {
        new_msg.sender_name = tgclient::chat_title(
                static_cast<td_api::messageSenderChat &>(
                    *tg_msg->sender_id_).chat_id_);
    }

    // Get message text
    std::string text = "[NONE]";
    if (tg_msg->content_->get_id() == td_api::messageText::ID) {
        text = static_cast<td_api::messageText &>(*tg_msg->content_).text_->text_;
    }
    new_msg.text = WStr::from(text.c_str());

    if (new_msg.is_mine) {
        chat_selection_offset = 0;
    } else {
        new_msg.widgets[new_msg.widget_count++].tag = WidgetTag::SENDER_NAME;
        if (chat_selection_offset != 0) {
            chat_selection_offset += 1;
        }
    }

    // Get reply if it exists
    if (tg_msg->reply_to_ != nullptr &&
        tg_msg->reply_to_->get_id() == td_api::messageReplyToMessage::ID) {
        std::int64_t reply_to_id = static_cast<td_api::messageReplyToMessage&>(
                *tg_msg->reply_to_).message_id_;
        Msg *local_reply_to = find_msg(reply_to_id);
        if (local_reply_to == nullptr) {
            std::wcout << "Could not reply to " << reply_to_id << ": not loaded\n";
        } else {
            new_msg.has_reply_to = true;
            new_msg.reply_to.id = local_reply_to->id;
            new_msg.reply_to.text = WStr::copy(local_reply_to->text);
            new_msg.reply_to.sender_name = local_reply_to->sender_name;
            new_msg.reply_to.is_mine = local_reply_to->is_mine;
            new_msg.widgets[new_msg.widget_count++].tag = WidgetTag::REPLY;
        }
    }

    new_msg.widgets[new_msg.widget_count++].tag = WidgetTag::TEXT;

    // Calculate message sizes
    for (size_t i = 0; i < new_msg.widget_count; i++) {
        Vector2 size = widget_vtable[new_msg.widgets[i].tag].size_fn(&new_msg);
        new_msg.widgets[i].size = size;
        new_msg.size.y += size.y;
        if (size.x > new_msg.size.x) new_msg.size.x = size.x;
    }
    new_msg.size.y += BoxModel::MSG_TP + BoxModel::MSG_BP;
    new_msg.size.x += BoxModel::MSG_LP + BoxModel::MSG_RP;

    if (chat_message_count >= MESSAGES_CAPACITY) {
        UnloadCodepoints((int*)chat_messages[0].text.data);
        if (chat_messages[0].has_reply_to) free(chat_messages[0].reply_to.text.data);
        memmove(&chat_messages[0], &chat_messages[1], (chat_message_count-1)*sizeof(Msg));
        chat_message_count -= 1;
    }

    chat_messages[chat_message_count++] = new_msg;
}

/* static float calc_msg_height(size_t msg_idx, float max_msg_line_width) */
/* { */
/*     WidgetTag *widgets = chat_messages[msg_idx].widgets; */
/*     size_t widget_count = chat_messages[msg_idx].widget_count; */
/*     float result = (widget_count-1)*MSG_WIDGET_DISTANCE + BoxModel::MSG_TP + BoxModel::MSG_BP; */
/*     for (size_t j = 0; j < widget_count; j++) { */
/*         Vector2 size = widget_vtable[widgets[j]].size_fn( */
/*                 &chat_messages[msg_idx], */
/*                 max_msg_line_width); */
/*         result += size.y; */
/*     } */
/**/
/*     return result; */
/* } */

static std::int64_t to_int64_t(std::wstring_view text)
{
    size_t i = 0;
    std::int64_t res = 0;
    std::int64_t factor = 1;
    if (text[0] == '-') { factor = -1; i = 1; }

    for (; i < text.length(); i++) {
        assert(isdigit(text[i]));
        res = 10*res + (text[i] - '0');
    }

    return res * factor;
}

static void load_msgs(td_api::object_ptr<td_api::messages> msgs)
{
    if (msgs->total_count_ != MSG_COUNT_TO_LOAD_WHEN_OPEN_CHAT) {
        tgclient::request(
            td_api::make_object<td_api::getChatHistory>(
                chat_id, 0, 0, MSG_COUNT_TO_LOAD_WHEN_OPEN_CHAT, false),
            load_msgs);
        return;
    }

    for (int i = MSG_COUNT_TO_LOAD_WHEN_OPEN_CHAT-1; i >= 0; i--) {
        push_msg(std::move(msgs->messages_[i]));
    }
}

static Msg *find_msg(std::int64_t msg_id)
{
    Msg *begin = &chat_messages[chat_message_count-1];
    Msg *end = &chat_messages[-1];
    for (Msg *it = begin; it != end; it--) {
        if (it->id == msg_id) {
            return it;
        }
    }

    return nullptr;
}

// WIDGET FUNCTIONS IMPLS ///////////////

static Vector2 widget_sender_name_size_fn(Msg *msg_data)
{
    float width = common::measure_wtext(
            MSG_SENDER_NAME_FONT_ID,
            &msg_data->sender_name[0],
            msg_data->sender_name.length());

    return { width > max_msg_widget_width ? max_msg_widget_width : width, common::font_size(MSG_SENDER_NAME_FONT_ID) };
}

static void widget_sender_name_render_fn(Msg *msg_data, Vector2 pos, float)
{
    common::draw_text_in_width(
            MSG_SENDER_NAME_FONT_ID,
            pos, &msg_data->sender_name[0], msg_data->sender_name.length(),
            msg_color_palette[msg_data->is_mine].sender_name_color, max_msg_widget_width);
}

static Vector2 widget_text_size_fn(Msg *msg_data)
{
    msg_data->text_lines.recalc(
            MSG_TEXT_FONT_ID,
            msg_data->text.data, msg_data->text.len,
            max_msg_widget_width);

    return { msg_data->text_lines.max_line_width(MSG_TEXT_FONT_ID), (float)msg_data->text_lines.len*common::font_size(MSG_TEXT_FONT_ID) };
}

static void widget_text_render_fn(Msg *msg_data, Vector2 pos, float)
{
    common::draw_lines(
            MSG_TEXT_FONT_ID, pos, msg_data->text_lines,
            msg_color_palette[msg_data->is_mine].fg_color);
}

static Vector2 widget_reply_size_fn(Msg *msg_data)
{
    float reply_text_width = common::measure_wtext(
            MSG_TEXT_FONT_ID,
            msg_data->reply_to.text.data,
            msg_data->reply_to.text.len);

    float reply_sender_name_width = common::measure_wtext(
            MSG_SENDER_NAME_FONT_ID,
            &msg_data->reply_to.sender_name[0],
            msg_data->reply_to.sender_name.length());

    float width = 
        (reply_text_width > reply_sender_name_width ?
         reply_text_width :
         reply_sender_name_width) + 2*MSG_REPLY_PADDING;

    return {
        width > max_msg_widget_width ? max_msg_widget_width : width,
              2*MSG_REPLY_PADDING +
                  common::font_size(MSG_SENDER_NAME_FONT_ID) +
                  common::font_size(MSG_TEXT_FONT_ID) };
}

static void widget_reply_render_fn(Msg *msg_data, Vector2 pos, float width)
{
    float max_reply_content_width = max_msg_widget_width - 2*MSG_REPLY_PADDING;

    Rectangle reply_rect = { pos.x, pos.y, width, 2*MSG_REPLY_PADDING +
        common::font_size(MSG_REPLY_SENDER_NAME_FONT_ID) +
        common::font_size(MSG_REPLY_TEXT_FONT_ID) };

    Color reply_sender_name_color;
    Color reply_bg_color;
    if (msg_data->is_mine) {
        reply_sender_name_color = msg_color_palette[1].fg_color;
        reply_bg_color = MSG_REPLY_BG_COLOR_IN_MY_MSG;
    } else {
        reply_sender_name_color = msg_color_palette[msg_data->reply_to.is_mine].sender_name_color;
        reply_bg_color = msg_color_palette[msg_data->reply_to.is_mine].reply_bg_color;
    }

    DrawRectangleRounded(
            reply_rect, MSG_REPLY_REC_ROUNDNESS/reply_rect.height,
            MSG_REPLY_REC_SEGMENT_COUNT, reply_bg_color);

    pos.x += MSG_REPLY_PADDING;
    pos.y += MSG_REPLY_PADDING;

    common::draw_text_in_width(
            MSG_REPLY_SENDER_NAME_FONT_ID,
            pos, &msg_data->reply_to.sender_name[0],
            msg_data->reply_to.sender_name.length(),
            reply_sender_name_color, max_reply_content_width);

    pos.y += common::font_size(MSG_REPLY_SENDER_NAME_FONT_ID);

    common::draw_text_in_width(
            MSG_REPLY_TEXT_FONT_ID,
            pos, msg_data->reply_to.text.data,
            msg_data->reply_to.text.len,
            msg_color_palette[msg_data->is_mine].fg_color, max_reply_content_width);
}

// COMMAND FUNCTION IMPLEMENTATIONS //////////

static void cmd_logout()
{
    tgclient::request(td_api::make_object<td_api::logOut>());
}

static void cmd_list_chats()
{
    tgclient::request(
        td_api::make_object<td_api::getChats>(nullptr, 10),
        +[](td_api::object_ptr<td_api::chats> c){
            std::wstring msg;
            for (auto chat_id : c->chat_ids_) {
                msg.append(tgclient::chat_title(chat_id));
                msg.push_back(' ');
                msg.append(std::to_wstring(chat_id));
                msg.push_back('\n');
            }

            std::wcout << msg << "\n";
        });
}

static void cmd_select_chat()
{
    assert(ted_arg_count > 1);
    chat_id = to_int64_t(ted_args[1]);

    tgclient::request(td_api::make_object<td_api::openChat>(chat_id));
    tgclient::request(
        td_api::make_object<td_api::getChatHistory>(
            chat_id, 0, -10, MSG_COUNT_TO_LOAD_WHEN_OPEN_CHAT, false),
        load_msgs);
}
