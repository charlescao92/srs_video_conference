#include <assert.h>

#include <rtc_base/logging.h>

#include "krtc_websocket.h"

namespace krtc {

namespace {
    const char* SSL_CERT_FILE_PATH = "server.crt";
    const char* SSL_PRIVATE_KEY_PATH = "server.key";

    static int wscallback(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len)
    {
        printf("call reason: %d \n", reason);

        const struct lws_protocols* protocols = lws_get_protocol(wsi);
        if (protocols == NULL || protocols->user == NULL) {
            return 0;
        }
        WebsocketClient* wsservice = (WebsocketClient*)user;
        if (!wsservice) {
            wsservice = (WebsocketClient*)protocols->user;
        }

        if (wsservice) {
            wsservice->OnCallback(wsi, reason, user, in, len);
        }

        return 0;
    }

    static struct lws_protocols protocols[] = {
        { NULL, NULL, 0, 0, 0, NULL, 0 }
    };

}

void WebsocketClient::OnCallback(lws* wsi, lws_callback_reasons reason, void* user, 
    void* in, size_t len)
{
    switch (reason) {
    case LWS_CALLBACK_CLIENT_ESTABLISHED: // 3
        if (statusCallback_) {
            statusCallback_(WEBSOCKET_STATUS::CONNECT_SUCCESS);
        }
        break;

    case LWS_CALLBACK_CLIENT_RECEIVE: // 8
        callbackMessage((const char*)in, len);
        break;

    case LWS_CALLBACK_CLIENT_WRITEABLE: // 10 lws_callback_on_writable函数触发来发送数据
        OnSendMessage();
        break;

    case LWS_CALLBACK_CLOSED: // 4
        if (statusCallback_) {
            statusCallback_(WEBSOCKET_STATUS::CONNECT_CLOSE);
        }
        break;
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR: // 1
        disconnect();
        if (statusCallback_) {
            statusCallback_(WEBSOCKET_STATUS::CONNECT_FAILED);
        }
        break;

    case LWS_CALLBACK_CLOSED_CLIENT_HTTP: // 45
    case LWS_CALLBACK_WSI_DESTROY: // 30
        if (statusCallback_) {
            statusCallback_(WEBSOCKET_STATUS::CONNECT_CLOSE);
        }
        break;

    default:
        break;
    }
}

WebsocketClient::WebsocketClient(const std::string& server_addr, const uint16_t& server_port, const std::string& path)
    : server_addr_(server_addr),
    server_port_(server_port),
    url_path_(path)
{
    
}

WebsocketClient::~WebsocketClient()
{
    disconnect();
}

void krtc::WebsocketClient::callbackMessage(const char* msg, int len)
{
    if (messageCallback_) {
        std::string message(msg, len);
        messageCallback_(message);
    }
}

void WebsocketClient::connect()
{
    protocols[0].name = "wss";
    protocols[0].callback = wscallback;
    protocols[0].per_session_data_size = sizeof(WebsocketClient);
    protocols[0].user = this;

    struct lws_context_creation_info context_info = {};
    context_info.port = CONTEXT_PORT_NO_LISTEN;
    context_info.iface = NULL;
    context_info.protocols = protocols;
    context_info.ka_time = 10;     // 心跳包间的时间间隔
    context_info.ka_interval = 10; // 发出心跳包后没有收到ACK确认包时重发心跳包的超时时间
    context_info.ka_probes = 3;   // 心跳探测次数，对于windows操作系统，此设置是无效的，Windows系统时固定为10次，不可修改
    context_info.options =  LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT |
        LWS_SERVER_OPTION_VALIDATE_UTF8 | LWS_SERVER_OPTION_DISABLE_IPV6;
    lws_context_ = lws_create_context(&context_info);
    assert(lws_context_ != nullptr);

    struct lws_client_connect_info connect_info = {};
    connect_info.context = lws_context_;
    connect_info.address = server_addr_.c_str();
    connect_info.port = server_port_;
    connect_info.ssl_connection =  LCCSCF_USE_SSL;
    connect_info.path = url_path_.c_str();
    std::string host = server_addr_ + ":" + std::to_string(server_port_);
    connect_info.host = host.c_str();
    connect_info.origin = host.c_str();
    connect_info.pwsi = &lws_websocket_;
    connect_info.protocol = protocols[0].name;
    connect_info.local_protocol_name = protocols[0].name;
    connect_info.userdata = this;

    lws_client_connect_via_info(&connect_info);

    if (!lws_websocket_) {
        if (statusCallback_) {
            statusCallback_(WEBSOCKET_STATUS::CONNECT_FAILED);
        }
        return;
    }

    interrupted_ = false;
    websocket_thread_ = std::make_unique<std::thread>([this]() {
        int n = 0;
        while (n >= 0 && !interrupted_) {
            n = lws_service(lws_context_, 1000);    
        }
    });

}

void WebsocketClient::disconnect()
{
    interrupted_ = true;

    if (lws_context_) {
        lws_cancel_service(lws_context_);
        lws_context_destroy(lws_context_);
        lws_context_ = nullptr;
        lws_websocket_ = nullptr;
    }

    if (websocket_thread_ && websocket_thread_->joinable()) {
        websocket_thread_->join();
        websocket_thread_ = nullptr;
    }
}

void WebsocketClient::send(const std::string& message)
{
    wait_send_message_ = message;

    OnSendMessage();
}

void WebsocketClient::OnSendMessage()
{
    if (!lws_websocket_ || wait_send_message_.empty()) {
        if (statusCallback_) {
            statusCallback_(WEBSOCKET_STATUS::MESSAGE_SEND_FAILED);
        }
        return;
    }

    size_t totalSize = wait_send_message_.length();
    unsigned char* sendBuf = new unsigned char[LWS_PRE + totalSize];
    memcpy(sendBuf + LWS_PRE, wait_send_message_.data(), wait_send_message_.size());

    int writeSize = lws_write(lws_websocket_, sendBuf + LWS_PRE, wait_send_message_.length(), LWS_WRITE_TEXT);
    delete[] sendBuf;
    if (writeSize == -1) {
        RTC_LOG(LS_ERROR) << "write failed, connection is closed! ";
        if (statusCallback_) {
            statusCallback_(WEBSOCKET_STATUS::MESSAGE_SEND_FAILED);
        }
        return;
    }
    if (writeSize == totalSize) {
        wait_send_message_.clear();
        if (statusCallback_) {
            statusCallback_(WEBSOCKET_STATUS::MESSAGE_SEND_SUCCESS);
        }
    }
    else if (writeSize < totalSize) {
        wait_send_message_ = wait_send_message_.substr(writeSize);
        RTC_LOG(LS_WARNING) << " totalSize:" <<  totalSize <<  ", writeSize:" << writeSize
                        << " remainSize:" << totalSize - writeSize;
    }

    if (!wait_send_message_.empty()) {
        lws_callback_on_writable(lws_websocket_);
    }
}

} // namespace krtc
