#include <assert.h>
#include <iostream>
#include <map>
#include <codecvt>
#include <locale>

#include "tgclient.h"
#include "chat.h"

#define TG_CLIENT_WAIT_TIME 0.0f

#define UPDATE_REQUEST_ID 0 // Requests that come from server
#define SILENT_REQUEST_ID 1 // Requests that don't need to be processed

#define REQUEST_ANSWER_HANDLERS_CAPACITY 16

#define LIST_OF_PRIVATE_UPDATE_HANDLERS \
    X(updateAuthorizationState, update_auth_state) \
    X(authorizationStateWaitTdlibParameters, auth_state_wait_tdlib_params) \
    X(authorizationStateReady, auth_state_ready) \
    X(authorizationStateWaitPhoneNumber, auth_state_wait_phone_number) \
    X(authorizationStateWaitCode, auth_state_wait_code) \
    X(updateNewChat, update_new_chat) \
    X(updateUser, update_user) \

#define LIST_OF_PUBLIC_UPDATE_HANDLERS \
    X(updateNewMessage, chat::update_new_msg) \
    X(updateMessageSendSucceeded, chat::update_msg_send_succeeded) \

// Declare private update handlers
#define X(update_type, handler) static void handler(td_api::object_ptr<td_api::update_type>);
LIST_OF_PRIVATE_UPDATE_HANDLERS
#undef X

tgclient::State tgclient::state = tgclient::STATE_NONE;

static td::ClientManager manager;
static std::int32_t      client_id;
static tgclient::Handler request_answer_handlers[REQUEST_ANSWER_HANDLERS_CAPACITY];
static std::map<std::int64_t, std::wstring> users;
static std::map<std::int64_t, std::wstring> chat_titles;
static std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
static std::map<std::int64_t, void*> update_handlers = {
#define X(update_type, handler) { td_api::update_type::ID, (void *) handler },
    LIST_OF_PRIVATE_UPDATE_HANDLERS
    LIST_OF_PUBLIC_UPDATE_HANDLERS
#undef X
};

void tgclient::init()
{
    // Create new client
    td::ClientManager::execute(td_api::make_object<td_api::setLogVerbosityLevel>(1));
    client_id = manager.create_client_id();

    // Start connection
    tgclient::request(td_api::make_object<td_api::getOption>("version"));
}

void tgclient::update()
{
    auto resp = manager.receive(TG_CLIENT_WAIT_TIME);
    if (resp.object == nullptr) return;

    switch (resp.request_id) {
    case UPDATE_REQUEST_ID:
        process_update(std::move(resp.object));
        break;

    // Even we don't need to process that type of requests we must handle errors
    case SILENT_REQUEST_ID:
        if (resp.object->get_id() == td_api::error::ID) {
            std::cout << "ERROR: " <<
                static_cast<td_api::error&>(*resp.object).message_ << "\n";
        }
        break;

    // Requests that need to be processed
    default:
        size_t handler_idx = resp.request_id-2;
        assert(handler_idx < REQUEST_ANSWER_HANDLERS_CAPACITY);
        assert(request_answer_handlers[handler_idx]);
        // TODO: Check if answer is not error
        request_answer_handlers[handler_idx](std::move(resp.object));
        request_answer_handlers[handler_idx] = nullptr;
        break;
    }
}

void tgclient::process_update(td_api::object_ptr<td_api::Object> update)
{
    auto handler = update_handlers.find(update->get_id());
    if (handler == update_handlers.end()) {
        /*fprintf(stderr, "[!] Handler not found for:\n");*/
        /*std::cout << td_api::to_string(update) << "\n";*/
    } else {
        ((tgclient::Handler)handler->second)(std::move(update));
    }
}

void tgclient::request(td_api::object_ptr<td_api::Function> req, Handler handler)
{
    for (size_t i = 0; i < REQUEST_ANSWER_HANDLERS_CAPACITY; i++) {
        if (request_answer_handlers[i] == nullptr) {
            request_answer_handlers[i] = handler;
            manager.send(client_id, i + 2, std::move(req));
            return;
        }
    }

    std::cout << "ERROR: Miss request: handler buffer is overflowed\n";
}

void tgclient::request(td_api::object_ptr<td_api::Function> req)
{
    manager.send(client_id, SILENT_REQUEST_ID, std::move(req));
}

std::wstring_view tgclient::username(std::int64_t user_id)
{
    auto it = users.find(user_id);
    return it == users.end() ?
           std::wstring_view(L"Unknown user") :
           std::wstring_view(it->second);
}

std::wstring_view tgclient::chat_title(std::int64_t chat_id)
{
    auto it = chat_titles.find(chat_id);
    return it == chat_titles.end() ?
           std::wstring_view(L"Unknown chat") :
           std::wstring_view(it->second);
}

// PRIVATE FUNCTION IMPLEMENTATIONS

static void update_auth_state(td_api::object_ptr<td_api::updateAuthorizationState> auth_update)
{
    tgclient::process_update(std::move(auth_update->authorization_state_));
}

static void auth_state_wait_tdlib_params(td_api::object_ptr<td_api::authorizationStateWaitTdlibParameters>)
{
    auto params = td_api::make_object<td_api::setTdlibParameters>();
    params->use_test_dc_ = false;
    params->database_directory_ = "data";
    params->use_file_database_ = true;
    params->use_chat_info_database_ = true;
    params->use_message_database_ = true;
    params->use_secret_chats_ = false;
    params->api_id_ = API_ID;
    params->api_hash_ = API_HASH;
    params->system_language_code_ = "en";
    params->device_model_ = "Desktop";
    params->system_version_ = "Debian 12";
    params->application_version_ = "0.1";
    chat::ted_set_placeholder(L"sending tdlib parameters...");
    tgclient::request(td_api::move_object_as<td_api::Function>(params));
}

static void auth_state_ready(td_api::object_ptr<td_api::authorizationStateReady>)
{
    chat::ted_set_placeholder(L"authorized");
    tgclient::state = tgclient::STATE_FREETIME;
}

static void auth_state_wait_phone_number(td_api::object_ptr<td_api::authorizationStateWaitPhoneNumber>)
{
    chat::ted_set_placeholder(L"phone number");
    tgclient::state = tgclient::STATE_WAIT_PHONE_NUMBER;
}

static void auth_state_wait_code(td_api::object_ptr<td_api::authorizationStateWaitCode>)
{
    chat::ted_set_placeholder(L"code");
    tgclient::state = tgclient::STATE_WAIT_CODE;
}

static void update_user(td_api::object_ptr<td_api::updateUser> update_user)
{
    auto user_id = update_user->user_->id_;
    users.insert({user_id, converter.from_bytes(update_user->user_->first_name_)});
}

static void update_new_chat(td_api::object_ptr<td_api::updateNewChat> update_new_chat)
{
    chat_titles.insert({
        update_new_chat->chat_->id_,
        converter.from_bytes(update_new_chat->chat_->title_)
    });
}
