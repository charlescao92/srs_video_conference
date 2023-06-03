#ifndef KRTCSDK_KRTC_BASE_KRTC_CLIENT_H_
#define KRTCSDK_KRTC_BASE_KRTC_CLIENT_H_

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <future>

#include "krtc/base/krtc_websocket.h"

namespace krtc {

enum MSG_ACTION {
    JOIN,
    PUBLISH,
    LEAVE,
};

class KRTCClient {
public:
    static KRTCClient* Instance();

    bool JoinRoom(const std::string& server_addr,
        const std::string& room_id,
        const std::string& uid);

    void LeaveRoom();

    void HandleWebsocketMessage(const std::string& json_msg);

    void HandleWebsocketConnectStatus(const WEBSOCKET_STATUS& status);

    void SendPublishMsg();

private:
    void SendAction(const MSG_ACTION& action);

private:
    std::unique_ptr<WebsocketClient> websocket_client_;
    std::string room_id_;
    std::string uid_;

    std::atomic<WEBSOCKET_STATUS> current_status_ = WEBSOCKET_STATUS::NO_STATUS;
    bool is_joined_ = false;
};

}

#endif // KRTCSDK_KRTC_BASE_KRTC_CLIENT_H_
