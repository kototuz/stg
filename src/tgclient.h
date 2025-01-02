#ifndef TGCLIENT_H
#define TGCLIENT_H

namespace tgclient {
    void init(const char *api_id, const char *api_hash);
    void update();
    void process_input();
}

#endif
