#include <cassert>
#include <iostream>
#include <string>
#include <cstdio>
#include <map>

#include <td/telegram/Client.h>

#include "chat.h"
#include "ted.h"
#include "tgclient.h"

#define UPDATE_HANDLERS \
    H(updateAuthorizationState) \
    H(authorizationStateWaitTdlibParameters) \
    H(authorizationStateReady) \
    H(authorizationStateWaitPhoneNumber) \
    H(authorizationStateWaitCode) \
    /*H(updateNewChat)*/

#define REQ_ANSWER_HANDLERS \
    H(Object) // when you don't need an answer

#define HANDLER_IMPL(type, param_name) static void type##_handler(td_api::object_ptr<td_api::type> param_name)

#define TG_CLIENT_WAIT_TIME 0.0

namespace td_api = td::td_api;

typedef void (*Handler)(td_api::object_ptr<td_api::Object>);


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

// Client
static td::ClientManager manager;
static std::int32_t client_id;
static State state;
static const char *global_api_id;
static const char *global_api_hash;

static std::map<std::int64_t, Handler> update_handler_map = {
#define H(type) { td_api::type::ID, (Handler) type##_handler },
    UPDATE_HANDLERS
#undef H
};
static Handler req_answer_handler_map[] = {
    (Handler) nullptr, // handler ids starts with 1 => skip first
#define H(type) (Handler) type##_handler
    REQ_ANSWER_HANDLERS
#undef H
};

static void process_update(td_api::object_ptr<td_api::Object> obj)
{
    auto handler = update_handler_map.find(obj->get_id());
    if (handler == update_handler_map.end()) {
        /*fprintf(stderr, "[!] Handler not found for:\n");*/
        /*std::cout << td_api::to_string(obj) << "\n";*/
    } else {
        handler->second(std::move(obj));
    }
}

void tgclient::init(const char *api_id, const char *api_hash)
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

void tgclient::update()
{
    auto resp = manager.receive(TG_CLIENT_WAIT_TIME);
    if (resp.object == nullptr) return;
    if (resp.request_id == 0) {
        process_update(std::move(resp.object));
    } else {
        (req_answer_handler_map[resp.request_id])(std::move(resp.object));
    }
}

void tgclient::process_input()
{
    std::wstring text = ted::get_text();
    if (text.length() == 0) return;
    std::string text_as_str(text.begin(), text.end());
    switch (state) {
        case NONE:
            break;

        case FREETIME:
            if (text[0] == ':' && text[1] == 'l') {
                manager.send(
                        client_id,
                        Object_handler_id,
                        td_api::make_object<td_api::logOut>());
            } else {
                chat::push_msg((int *)text.c_str(), text.length(), (int*)L"You", 3);
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
    process_update(std::move(auth_update->authorization_state_));
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
    state = State::FREETIME;
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
