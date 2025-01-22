#include <cassert>
#include <iostream>
#include <string>
#include <cstring>
#include <cstdio>
#include <map>
#include <codecvt>
#include <locale>

#include <td/telegram/Client.h>

#include <raylib.h>

#include "config.h"
#include "chat.h"
#include "ted.h"

#define UPDATE_HANDLERS \
    H(updateAuthorizationState) \
    H(authorizationStateWaitTdlibParameters) \
    H(authorizationStateReady) \
    H(authorizationStateWaitPhoneNumber) \
    H(authorizationStateWaitCode) \
    H(updateNewChat) \
    H(updateNewMessage) \
    H(updateUser) \
    H(updateMessageContent) \
    H(updateMessageSendSucceeded) \

#define COMMANDS \
    C(l)  /*logout*/ \
    C(c)  /*list chats*/ \
    C(sc) /*select chat*/ \

#define HANDLER_IMPL(type, param_name) static void type##_handler(td_api::object_ptr<td_api::type> param_name)

#define CMD_IMPL(name, param) static void name##_cmd(Args param)

#define TG_CLIENT_WAIT_TIME 0.0

#define MSG_COUNT_TO_LOAD_WHEN_OPEN_CHAT 16

#define UPDATE_REQUEST_ID 0
#define SILENT_REQUEST_ID 1

#define REQUEST_ANSWER_HANDLERS_CAPACITY 64

namespace td_api = td::td_api;

struct Args {
    std::wstring_view *items;
    size_t count;
};

typedef void (*Handler)(td_api::object_ptr<td_api::Object>);
typedef void (*Command)(Args);

enum State {
    NONE,
    WAIT_PHONE_NUMBER,
    WAIT_CODE,
    FREETIME,
};

// Declare all handlers
#define H(type) static void type##_handler(td_api::object_ptr<td_api::type>);
UPDATE_HANDLERS
#undef H

// Declare command functions
#define C(name) static void name##_cmd(Args);
COMMANDS
#undef C

// Global state
static td::ClientManager                                manager;
static std::int32_t                                     client_id;
static State                                            state;
static std::map<std::int64_t, std::wstring>             chat_title_map;
static std::int64_t                                     curr_chat_id = 0;
static std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
static std::map<std::int64_t, std::wstring>             user_map;
static Handler                                          request_answer_handlers[REQUEST_ANSWER_HANDLERS_CAPACITY];

static std::map<std::int64_t, void*> update_handler_map = {
#define H(type) { td_api::type::ID, (void *) type##_handler },
    UPDATE_HANDLERS
#undef H
};
static std::map<std::wstring_view, Command> command_map = {
#define C(name) { L""#name, name##_cmd },
    COMMANDS
#undef C
};


static void tgclient_init();
static void tgclient_update();
static void tgclient_send_msg();
static void tgclient_process_update(td_api::object_ptr<td_api::Object> obj);
static std::wstring_view tgclient_get_username(std::int64_t user_id);
static std::wstring_view tgclient_get_chat_title(std::int64_t chat_id);
template<typename T> static td_api::object_ptr<T> tgclient_request(td_api::object_ptr<td_api::Function> req); // Deprecated
template<typename T> static void tgclient_request(td_api::object_ptr<td_api::Function> req, void (*f)(td_api::object_ptr<T>));
#define tgclient_silent_request(req) manager.send(client_id, SILENT_REQUEST_ID, req)

// Util functions
static chat::Msg tg_msg_to_chat_msg(td_api::object_ptr<td_api::message> tg_msg);
static std::vector<std::wstring_view> split(std::wstring_view src);
static std::int64_t to_int64_t(std::wstring_view text);

int main()
{
    // Disable 'raylib' logging
    SetTraceLogLevel(LOG_NONE);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(DEFAULT_WIDTH, DEFAULT_HEIGHT, "Simple Telegram");

    ted::init();
    chat::init();
    tgclient_init();

    while (!WindowShouldClose()) {
        ClearBackground(CHAT_BG_COLOR);
        if      (KEYMAP_SELECT_PREV)        chat::select_prev_msg();
        else if (KEYMAP_SELECT_NEXT)        chat::select_next_msg();
        else if (KEYMAP_MOVE_FORWARD)       ted::try_cursor_motion(ted::MOTION_FORWARD);
        else if (KEYMAP_MOVE_BACKWARD)      ted::try_cursor_motion(ted::MOTION_BACKWARD);
        else if (KEYMAP_MOVE_FORWARD_WORD)  ted::try_cursor_motion(ted::MOTION_FORWARD_WORD);
        else if (KEYMAP_MOVE_BACKWARD_WORD) ted::try_cursor_motion(ted::MOTION_BACKWARD_WORD);
        else if (KEYMAP_MOVE_UP)            ted::try_cursor_motion(ted::MOTION_UP);
        else if (KEYMAP_MOVE_DOWN)          ted::try_cursor_motion(ted::MOTION_DOWN);
        else if (KEYMAP_MOVE_END)           ted::try_cursor_motion(ted::MOTION_END);
        else if (KEYMAP_MOVE_BEGIN)         ted::try_cursor_motion(ted::MOTION_BEGIN);
        else if (KEYMAP_DELETE_WORD)        ted::delete_word();
        else if (KEYMAP_DELETE_LINE)        ted::delete_line();
        else if (KEYMAP_DELETE)             ted::delete_symbols(1);
        else if (KEYMAP_NEW_LINE)           ted::insert_symbol('\n');
        else if (KEYMAP_SEND_MESSAGE)       tgclient_send_msg();
        else { // just insert char
            int symbol = GetCharPressed();
            if (symbol != 0) ted::insert_symbol(symbol);
        }

        tgclient_update();

        BeginDrawing();
            ted::render();
            chat::render(ted::get_height());
        EndDrawing();
    }
    CloseWindow();
}

static void tgclient_process_update(td_api::object_ptr<td_api::Object> obj)
{
    auto handler = update_handler_map.find(obj->get_id());
    if (handler == update_handler_map.end()) {
        /*fprintf(stderr, "[!] Handler not found for:\n");*/
        /*std::cout << td_api::to_string(obj) << "\n";*/
    } else {
        ((Handler)handler->second)(std::move(obj));
    }
}

static void tgclient_init()
{
    // Create new client
    td::ClientManager::execute(td_api::make_object<td_api::setLogVerbosityLevel>(1));
    client_id = manager.create_client_id();

    // Start connection
    tgclient_silent_request(td_api::make_object<td_api::getOption>("version"));

    state = State::NONE;
}

static void tgclient_update()
{
    auto resp = manager.receive(TG_CLIENT_WAIT_TIME);
    if (resp.object == nullptr) return;

    switch (resp.request_id) {
    case UPDATE_REQUEST_ID:
        tgclient_process_update(std::move(resp.object));
        break;

    case SILENT_REQUEST_ID:
        if (resp.object->get_id() == td_api::error::ID) {
            std::wstring error_msg = converter.from_bytes(
                    static_cast<td_api::error&>(*resp.object).message_);
            ted::set_placeholder(error_msg.c_str());
        }
        break;

    default: // Other requests
        size_t handler_idx = resp.request_id-2;
        assert(handler_idx < REQUEST_ANSWER_HANDLERS_CAPACITY);
        assert(request_answer_handlers[handler_idx]);
        request_answer_handlers[handler_idx](std::move(resp.object));
        request_answer_handlers[handler_idx] = nullptr;
    }
}

static std::vector<std::wstring_view> split(std::wstring_view src)
{
    size_t len = 0;
    std::vector<std::wstring_view> res;
    for (;;) {
        if (src.length() == 0) break;
        if (len == src.length()) { res.push_back(src); break; }
        if (src[len] != ' ') { len++; continue; }
        res.push_back(std::wstring_view(&src[0], len));
        src.remove_prefix(len);
        src.remove_prefix(std::min(src.find_first_not_of(L" "), src.size()));
        len = 0;
    }

    return res;
}

static void tgclient_send_msg()
{
    std::wstring_view text = ted::get_text();
    if (text.length() == 0) return;
    std::string text_as_str = converter.to_bytes(text.begin(), text.end());

    std::vector<std::wstring_view> args;
    switch (state) {
        case NONE:
            break;

        case FREETIME:
            if (text[0] == COMMAND_START_SYMBOL) {
                args = split(text);
                args[0].remove_prefix(1); // without 'COMMAND_START_SYMBOL'
                auto res = command_map.find(args[0]);
                if (res == command_map.end()) {
                    ted::set_placeholder(L"Command not found");
                    break;
                }
                res->second(Args{ &args[1], args.size()-1 });
            } else if (curr_chat_id == 0) {
                ted::set_placeholder(L"Chat is not selected");
            } else {
                auto send_message = td_api::make_object<td_api::sendMessage>();
                send_message->chat_id_ = curr_chat_id;
                auto message_content = td_api::make_object<td_api::inputMessageText>();
                message_content->text_ = td_api::make_object<td_api::formattedText>();
                message_content->text_->text_ = std::move(text_as_str);
                send_message->input_message_content_ = std::move(message_content);

                chat::Msg *reply_to;
                if ((reply_to = chat::get_selected_msg())) {
                    send_message->reply_to_ =
                        td_api::make_object<td_api::inputMessageReplyToMessage>(
                                reply_to->id, nullptr);
                }

                tgclient_silent_request(std::move(send_message));
            }
            break;

        case WAIT_CODE:
            tgclient_silent_request(
                    td_api::make_object<td_api::checkAuthenticationCode>(text_as_str));
            break;

        case WAIT_PHONE_NUMBER:
            tgclient_silent_request(
                    td_api::make_object<td_api::setAuthenticationPhoneNumber>(text_as_str, nullptr));
            break;
    }

    ted::clear();
}

static std::wstring_view tgclient_get_username(std::int64_t user_id)
{
    auto it = user_map.find(user_id);
    return it == user_map.end() ?
           std::wstring_view(L"Unknown user") :
           std::wstring_view(it->second);
}

static std::wstring_view tgclient_get_chat_title(std::int64_t chat_id)
{
    auto it = chat_title_map.find(chat_id);
    return it == chat_title_map.end() ?
           std::wstring_view(L"Unknown chat") :
           std::wstring_view(it->second);
}

template<typename T>
static td_api::object_ptr<T> tgclient_request(td_api::object_ptr<td_api::Function> req)
{
    (void) req;
    assert(0 && "The function is deprecated");
    return nullptr;
    /*manager.send(client_id, OUR_REQUEST_ID, std::move(req));*/
    /*std::wstring error_msg;*/
    /*for (;;) {*/
    /*    auto resp = manager.receive(TG_CLIENT_WAIT_TIME);*/
    /*    if (resp.object == nullptr) continue;*/
    /*    if (resp.request_id == OUR_REQUEST_ID) {*/
    /*        switch (resp.object->get_id()) {*/
    /*            case T::ID: return td_api::move_object_as<T>(resp.object);*/
    /*            case td_api::error::ID:*/
    /*                error_msg = converter.from_bytes(static_cast<td_api::error&>(*resp.object).message_);*/
    /*                ted::set_placeholder(error_msg.c_str());*/
    /*                return nullptr;*/
    /**/
    /*            default: assert(0 && "Unexpected object id");*/
    /*        }*/
    /*    } else if (resp.request_id == UPDATE_REQUEST_ID) {*/
    /*        tgclient_process_update(std::move(resp.object));*/
    /*    } else if (resp.request_id == SILENT_REQUEST_ID && resp.object->get_id() == td_api::error::ID) {*/
    /*        puts("Get silent request");*/
    /*        error_msg = converter.from_bytes(static_cast<td_api::error&>(*resp.object).message_);*/
    /*        ted::set_placeholder(error_msg.c_str());*/
    /*    }*/
    /*}*/
}

static chat::Msg tg_msg_to_chat_msg(td_api::object_ptr<td_api::message> tg_msg)
{
    chat::Msg new_msg = {};
    new_msg.id = tg_msg->id_;

    std::wcout << "Get message " << tg_msg->id_ << "\n";

    // Get message text
    std::string text = "[NONE]";
    if (tg_msg->content_->get_id() == td_api::messageText::ID) {
        text = static_cast<td_api::messageText &>(*tg_msg->content_).text_->text_;
    }
    new_msg.text = chat::WStr::from(text.c_str());

    // Get reply if it exists
    if (tg_msg->reply_to_ != nullptr &&
        tg_msg->reply_to_->get_id() == td_api::messageReplyToMessage::ID) {
        std::int64_t reply_to_id = static_cast<td_api::messageReplyToMessage&>(
                *tg_msg->reply_to_).message_id_;
        new_msg.reply_to = chat::find_msg(reply_to_id);
        if (new_msg.reply_to == nullptr) {
            std::wcout << "Could not reply to " << reply_to_id << ": not loaded\n";
        }
    }

    // Set color palette
    if (tg_msg->is_outgoing_) {
        new_msg.is_mine = true;
        new_msg.bg_color = MSG_MY_BG_COLOR;
        new_msg.fg_color = MSG_MY_FG_COLOR;
        new_msg.author_name_color = MSG_MY_NAME_COLOR;
    } else {
        new_msg.fg_color = MSG_OTHERS_FG_COLOR;
        new_msg.bg_color = MSG_OTHERS_BG_COLOR;
        new_msg.author_name_color = MSG_OTHERS_NAME_COLOR;
    }

    // Get message author name
    if (tg_msg->sender_id_->get_id() == td_api::messageSenderUser::ID) {
        auto id = static_cast<td_api::messageSenderUser &>(*tg_msg->sender_id_).user_id_;
        new_msg.author_name = tgclient_get_username(id);
    } else {
        new_msg.author_name = tgclient_get_chat_title(
                static_cast<td_api::messageSenderChat &>(
                    *tg_msg->sender_id_).chat_id_);
    }

    return new_msg;
}

template<typename T>
static void tgclient_request(td_api::object_ptr<td_api::Function> req, void (*f)(td_api::object_ptr<T>))
{
    for (size_t i = 0; i < REQUEST_ANSWER_HANDLERS_CAPACITY; i++) {
        if (request_answer_handlers[i] == nullptr) {
            request_answer_handlers[i] = (Handler) ((void *)f); // Thank you C
            manager.send(client_id, i + 2, std::move(req));
            return;
        }
    }

    ted::set_placeholder(L"Miss request: handler buffer is overflowed");
}

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

// HANDLER IMPLS //////////////////

HANDLER_IMPL(updateAuthorizationState, auth_update)
{
    tgclient_process_update(std::move(auth_update->authorization_state_));
}

HANDLER_IMPL(authorizationStateWaitTdlibParameters, wait_params)
{
    (void) wait_params;
    auto params = td_api::make_object<td_api::setTdlibParameters>();
    params->use_test_dc_ = false;
    params->database_directory_ = "data";
    params->use_file_database_ = true;
    params->use_chat_info_database_ = true;
    params->use_message_database_ = true;
    params->use_secret_chats_ = false;
    params->api_id_ = API_ID;
#define stringify(x) #x
    params->api_hash_ = stringify(API_HASH);
#undef stringify
    params->system_language_code_ = "en";
    params->device_model_ = "Desktop";
    params->system_version_ = "Debian 12";
    params->application_version_ = "0.1";
    ted::set_placeholder(L"sending tdlib parameters...");
    tgclient_silent_request(td_api::move_object_as<td_api::Function>(params));
}

HANDLER_IMPL(authorizationStateReady, update)
{
    (void) update;
    ted::set_placeholder(L"authorized");
    state = State::FREETIME;
}

HANDLER_IMPL(authorizationStateWaitPhoneNumber, auth_state)
{
    (void) auth_state;
    ted::set_placeholder(L"phone number");
    state = State::WAIT_PHONE_NUMBER;
}

HANDLER_IMPL(authorizationStateWaitCode, auth_state)
{
    (void) auth_state;
    ted::set_placeholder(L"code");
    state = State::WAIT_CODE;
}

HANDLER_IMPL(updateNewChat, update_new_chat)
{
    chat_title_map.insert({update_new_chat->chat_->id_, converter.from_bytes(update_new_chat->chat_->title_)});
}

HANDLER_IMPL(updateNewMessage, update_new_msg)
{
    if (update_new_msg->message_->chat_id_ != curr_chat_id) return;
    chat::push_msg(tg_msg_to_chat_msg(std::move(update_new_msg->message_)));
}

HANDLER_IMPL(updateUser, update_user)
{
    auto user_id = update_user->user_->id_;
    user_map.insert({user_id, converter.from_bytes(update_user->user_->first_name_)});
}

HANDLER_IMPL(updateMessageContent, content)
{
    if (content->chat_id_ != curr_chat_id) return;
}

HANDLER_IMPL(updateMessageSendSucceeded, suc)
{
    chat::Msg *old_msg = chat::find_msg(suc->old_message_id_);
    std::wcout << old_msg->id << " -> " << suc->message_->id_ << "\n";
    if (old_msg != nullptr) old_msg->id = suc->message_->id_;
}

// CMD IMPLS //////////////////

CMD_IMPL(c, args)
{
    (void) args;
    tgclient_request(
        td_api::make_object<td_api::getChats>(nullptr, 10),
        +[](td_api::object_ptr<td_api::chats> c){
            std::wstring msg;
            for (auto chat_id : c->chat_ids_) {
                msg.append(chat_title_map[chat_id]);
                msg.push_back(' ');
                msg.append(std::to_wstring(chat_id));
                msg.push_back('\n');
            }

            std::wcout << msg << "\n";
        });
}

CMD_IMPL(l, args)
{
    (void) args;
    tgclient_silent_request(td_api::make_object<td_api::logOut>());
}

static void handle_msgs(td_api::object_ptr<td_api::messages> msgs)
{
    if (msgs->total_count_ != MSG_COUNT_TO_LOAD_WHEN_OPEN_CHAT) {
        tgclient_request(
            td_api::make_object<td_api::getChatHistory>(
                curr_chat_id, 0, 0, MSG_COUNT_TO_LOAD_WHEN_OPEN_CHAT, false),
            handle_msgs);
        return;
    }

    for (int i = MSG_COUNT_TO_LOAD_WHEN_OPEN_CHAT-1; i >= 0; i--) {
        chat::push_msg(tg_msg_to_chat_msg(std::move(msgs->messages_[i])));
    }
}

CMD_IMPL(sc, args)
{
    curr_chat_id = to_int64_t(args.items[0]);
    tgclient_silent_request(td_api::make_object<td_api::openChat>(curr_chat_id));
    tgclient_request(
        td_api::make_object<td_api::getChatHistory>(
            curr_chat_id, 0, -10, MSG_COUNT_TO_LOAD_WHEN_OPEN_CHAT, false),
        handle_msgs);
}

// TODO: Smile rendering
// TODO: Chat scrolling
// TODO: Chat panel
// TODO: Font management in the 'config.h'
