#ifndef KRTC_BASE_KRTC_WEBSOCKET_H_
#define KRTC_BASE_KRTC_WEBSOCKET_H_

#include <string>
#include <functional>
#include <memory>
#include <thread>

#include <libwebsockets.h>

namespace krtc {

enum class WEBSOCKET_STATUS {
    NO_STATUS,
    CONNECTING,
    CONNECT_SUCCESS,
    CONNECT_FAILED,
    CONNECT_CLOSE,
    MESSAGE_SEND,
    MESSAGE_SEND_SUCCESS,
    MESSAGE_SEND_FAILED,
    MESSAGE_RECEIVED,
};

class WebsocketClient {
public:
    WebsocketClient(const std::string& server_addr, 
        const uint16_t& server_port, 
        const std::string& path);
    ~WebsocketClient();

    void OnCallback(struct lws* wsi, enum lws_callback_reasons reason, 
        void* user, void* in, size_t len);
    void OnSendMessage();

    void connect();
    void disconnect();
    void send(const std::string& message);

    typedef std::function<void(const std::string&)> MessageCallback;
    void setMessageCallback(MessageCallback callback) {
        messageCallback_ = callback;
    }

    typedef std::function<void(const WEBSOCKET_STATUS&)> StatusCallback;
    void setStatusCallback(StatusCallback callback) {
        statusCallback_ = callback;
    }

    void callbackMessage(const char* msg, int len);

private:
    std::string server_addr_;
    uint16_t server_port_;
    std::string url_path_;
    struct lws_context* lws_context_ = nullptr;
    struct lws* lws_websocket_ = nullptr;
    MessageCallback messageCallback_ = nullptr;
    StatusCallback statusCallback_ = nullptr;

    std::unique_ptr<std::thread> websocket_thread_;
    std::atomic_bool interrupted_ = false;

    std::string wait_send_message_;
};

} // namespace krtc

#endif // KRTC_BASE_KRTC_WEBSOCKET_H_