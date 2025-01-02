#ifndef TGCLIENT_H
#define TGCLIENT_H

#include <td/telegram/Client.h>

#define TG_CLIENT_WAIT_TIME 0.0

namespace td_api = td::td_api;

namespace tgclient {
    void init(const char *api_id, const char *api_hash);
    void update();
    void process_input(const int *input, size_t input_len);
}

#endif
