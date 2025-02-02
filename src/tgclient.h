#ifndef TGCLIENT_H_
#define TGCLIENT_H_

#include <td/telegram/Client.h>
namespace td_api = td::td_api;

namespace tgclient {
    enum State {
        STATE_NONE,
        STATE_WAIT_PHONE_NUMBER,
        STATE_WAIT_CODE,
        STATE_FREETIME,
    };

    typedef void (*Handler)(td_api::object_ptr<td_api::Object>);

    extern State state;

    void init();
    void update();
    void process_update(td_api::object_ptr<td_api::Object> obj);
    void request(td_api::object_ptr<td_api::Function> req, Handler handler);
    void request(td_api::object_ptr<td_api::Function> req);
    std::wstring_view username(std::int64_t user_id);
    std::wstring_view chat_title(std::int64_t chat_id);

    template<typename T>
    void request(td_api::object_ptr<td_api::Function> req, void (*handler)(td_api::object_ptr<T>)) {
        request(std::move(req), (Handler)(void *)handler);
    }
};

#endif
