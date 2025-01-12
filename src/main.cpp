#include <cassert>
#include <iostream>
#include <string>
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

#define REQ_ANSWER_HANDLERS \
    H(Object) /*when you don't need an answer*/ \
    H(chats) \
    H(user)

#define COMMANDS \
    C(l)  /*logout*/ \
    C(c)  /*list chats*/ \
    C(sc) /*select chat*/ \

#define HANDLER_IMPL(type, param_name) static void type##_handler(td_api::object_ptr<td_api::type> param_name)

#define CMD_IMPL(name, param) static void name##_cmd(Args param)

#define TG_CLIENT_WAIT_TIME 0.0

namespace td_api = td::td_api;

struct Args {
    std::wstring_view *items;
    size_t count;
};

typedef void (*Handler)(td_api::object_ptr<td_api::Object>);
typedef void (*Command)(Args);

enum ReqAnswerHandlerId {
    IGNORE, // handler ids must start with 1
#define H(type) type##_handler_id,
    REQ_ANSWER_HANDLERS
#undef H
};

enum State {
    NONE,
    WAIT_PHONE_NUMBER,
    WAIT_CODE,
    FREETIME,
};

// Declare all handlers
#define H(type) static void type##_handler(td_api::object_ptr<td_api::type>);
UPDATE_HANDLERS
REQ_ANSWER_HANDLERS
#undef H

// Declare command functions
#define C(name) static void name##_cmd(Args);
COMMANDS
#undef C

// Global state
static td::ClientManager                                manager;
static std::int32_t                                     client_id;
static State                                            state;
static const char                                       *global_api_id;
static const char                                       *global_api_hash;
static std::map<std::int64_t, std::wstring>             chat_title_map;
static std::int64_t                                     curr_chat_id = 0;
static std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
static std::int64_t                                     user_id;
static std::map<std::int64_t, std::wstring>             user_map;

static std::map<std::int64_t, Handler> update_handler_map = {
#define H(type) { td_api::type::ID, (Handler) type##_handler },
    UPDATE_HANDLERS
#undef H
};
static Handler req_answer_handler_map[] = {
    (Handler) nullptr, // handler ids starts with 1 => skip first
#define H(type) (Handler) type##_handler,
    REQ_ANSWER_HANDLERS
#undef H
};
static std::map<std::wstring_view, Command> command_map = {
#define C(name) { L""#name, name##_cmd },
    COMMANDS
#undef C
};


static void tgclient_init(const char *api_id, const char *api_hash);
static void tgclient_update();
static void tgclient_send_msg();
static void tgclient_process_update();
static void tgclient_request();
static std::wstring_view tgclient_get_username(std::int64_t user_id);
static std::wstring_view tgclient_get_chat_title(std::int64_t chat_id);

// Util functions
static std::vector<std::wstring_view> split(std::wstring_view src);
static std::int64_t to_int64_t(std::wstring_view text);

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s <api_id> <api_hash>\n", argv[0]);
        return 1;
    }

    // Disable 'raylib' logging
    SetTraceLogLevel(LOG_NONE);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(DEFAULT_WIDTH, DEFAULT_HEIGHT, "Simple Telegram");

    ted::init();
    chat::init();
    tgclient_init(argv[1], argv[2]);

    while (!WindowShouldClose()) {
        ClearBackground(CHAT_BG_COLOR);
        if (KEYMAP_MOVE_FORWARD)            ted::try_cursor_motion(ted::MOTION_FORWARD);
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
        else if (KEYMAP_SEND_MESSAGE)       tgclient_send_msg();
        else if (KEYMAP_NEW_LINE)           ted::insert_symbol('\n');
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
        handler->second(std::move(obj));
    }
}

static void tgclient_init(const char *api_id, const char *api_hash)
{
    // Create new client
    td::ClientManager::execute(td_api::make_object<td_api::setLogVerbosityLevel>(1));
    client_id = manager.create_client_id();

    // Start connection
    manager.send(client_id, 1, td_api::make_object<td_api::getOption>("version"));

    global_api_id = api_id;
    global_api_hash = api_hash;

    state = State::NONE;
}

static void tgclient_update()
{
    auto resp = manager.receive(TG_CLIENT_WAIT_TIME);
    if (resp.object == nullptr) return;
    if (resp.request_id == 0) {
        tgclient_process_update(std::move(resp.object));
    } else {
        (req_answer_handler_map[resp.request_id])(std::move(resp.object));
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
                res->second(Args{ .items = &args[1], .count = args.size()-1 });
            } else if (curr_chat_id == 0) {
                ted::set_placeholder(L"Chat is not selected");
            } else {
                auto send_message = td_api::make_object<td_api::sendMessage>();
                send_message->chat_id_ = curr_chat_id;
                auto message_content = td_api::make_object<td_api::inputMessageText>();
                message_content->text_ = td_api::make_object<td_api::formattedText>();
                message_content->text_->text_ = std::move(text_as_str);
                send_message->input_message_content_ = std::move(message_content);
                manager.send(client_id, Object_handler_id, std::move(send_message));
            }
            break;

        case WAIT_CODE:
            manager.send(
                    client_id,
                    Object_handler_id,
                    td_api::make_object<td_api::checkAuthenticationCode>(text_as_str));
            break;

        case WAIT_PHONE_NUMBER:
            manager.send(
                    client_id,
                    Object_handler_id,
                    td_api::make_object<td_api::setAuthenticationPhoneNumber>(text_as_str, nullptr));
            break;
    }

    ted::clear();
}

HANDLER_IMPL(updateAuthorizationState, auth_update)
{
    tgclient_process_update(std::move(auth_update->authorization_state_));
}

HANDLER_IMPL(authorizationStateWaitTdlibParameters, wait_params)
{
    auto params = td_api::make_object<td_api::setTdlibParameters>();
    params->use_test_dc_ = false;
    params->database_directory_ = "data";
    params->use_file_database_ = true;
    params->use_chat_info_database_ = true;
    params->use_message_database_ = true;
    params->use_secret_chats_ = false;
    params->api_id_ = std::atoi(global_api_id);
    params->api_hash_ = global_api_hash;
    params->system_language_code_ = "en";
    params->device_model_ = "Desktop";
    params->system_version_ = "Debian 12";
    params->application_version_ = "0.1";
    ted::set_placeholder(L"sending tdlib parameters...");
    manager.send(client_id, Object_handler_id, td_api::move_object_as<td_api::Function>(params));
}

HANDLER_IMPL(authorizationStateReady, update)
{
    ted::set_placeholder(L"authorized");

    // Get user name
    manager.send(client_id, user_handler_id, td_api::make_object<td_api::getMe>());
}

HANDLER_IMPL(authorizationStateWaitPhoneNumber, auth_state)
{
    ted::set_placeholder(L"phone number");
    state = State::WAIT_PHONE_NUMBER;
}

HANDLER_IMPL(Object, answer)
{
    if (answer->get_id() == td_api::error::ID) {
        auto err = td_api::move_object_as<td_api::error>(answer);
        std::wstring msg_as_wstr = std::wstring(err->message_.begin(), err->message_.end());
        ted::set_placeholder(msg_as_wstr.c_str());
    }
}

HANDLER_IMPL(authorizationStateWaitCode, auth_state)
{
    ted::set_placeholder(L"code");
    state = State::WAIT_CODE;
}

HANDLER_IMPL(updateNewChat, update_new_chat)
{
    chat_title_map.insert({update_new_chat->chat_->id_, converter.from_bytes(update_new_chat->chat_->title_)});
}

HANDLER_IMPL(chats, c)
{
    std::wstring msg;
    for (auto chat_id : c->chat_ids_) {
        msg.append(chat_title_map[chat_id]);
        msg.push_back(' ');
        msg.append(std::to_wstring(chat_id));
        msg.push_back('\n');
    }

    chat::push_msg(msg, L"System");
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

HANDLER_IMPL(updateNewMessage, update_new_msg)
{
    if (update_new_msg->message_->chat_id_ != curr_chat_id) return;

    std::string text = "[NONE]";
    if (update_new_msg->message_->content_->get_id() == td_api::messageText::ID) {
        text = static_cast<td_api::messageText &>(*update_new_msg->message_->content_).text_->text_;
    }

    std::wstring_view sender_name;
    std::wstring wstr = converter.from_bytes(text);
    if (update_new_msg->message_->sender_id_->get_id() == td_api::messageSenderUser::ID) {
        auto id = static_cast<td_api::messageSenderUser &>(*update_new_msg->message_->sender_id_).user_id_;
        sender_name = tgclient_get_username(id);

        // If it is our message we push it as our message :)
        if (user_id == id) {
            chat::push_msg(wstr, sender_name, true);
            return;
        }
    } else {
        sender_name = tgclient_get_chat_title(static_cast<td_api::messageSenderChat &>(*update_new_msg->message_->sender_id_).chat_id_);
    }

    chat::push_msg(wstr, sender_name);
}

HANDLER_IMPL(user, me)
{
    user_id = me->id_;
    state = State::FREETIME;
}

HANDLER_IMPL(updateUser, update_user)
{
    auto user_id = update_user->user_->id_;
    user_map.insert({user_id, converter.from_bytes(update_user->user_->first_name_)});
}

CMD_IMPL(c, args)
{
    manager.send(client_id, chats_handler_id, td_api::make_object<td_api::getChats>(nullptr, 10));
}

CMD_IMPL(l, args)
{
    manager.send(client_id, Object_handler_id, td_api::make_object<td_api::logOut>());
}

CMD_IMPL(sc, args)
{
    curr_chat_id = to_int64_t(args.items[0]);
}
