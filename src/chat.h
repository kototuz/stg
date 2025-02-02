#ifndef CHAT_H_
#define CHAT_H_

#include <td/telegram/Client.h>
namespace td_api = td::td_api;

namespace chat {
    void init();
    void update();
    void render();
    void ted_set_placeholder(const wchar_t *text);

    // Update handlers
    void update_new_msg(td_api::object_ptr<td_api::updateNewMessage> u);
    void update_msg_send_succeeded(td_api::object_ptr<td_api::updateMessageSendSucceeded> u);
};

#endif
