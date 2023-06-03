#include "krtc/media/krtc_media_base.h"

#include <rtc_base/string_encode.h>

namespace krtc {

const std::string DEFALUT_HTTPS_PORT = "1986";

KRTCMediaBase::KRTCMediaBase(const CONTROL_TYPE& type, 
	const std::string& server_addr, 
	const std::string& channel, 
	const int& hwnd) :
	hwnd_(hwnd)
{
	// server_addr  = charlescao92.cn 或者 charlescao92.cn:1989 端口是ws的端口
	// 但是推流端口和ws端口是不一样的

	std::string server_ip, server_port;
	if (!rtc::tokenize_first(server_addr, ':', &server_ip, &server_port)) {
		server_ip = server_addr;
	}

	std::string control;
	if (type == CONTROL_TYPE::PUSH) {
		control = "publish";
	}
	else if (type == CONTROL_TYPE::PULL) {
		control = "play";
	}

	// https://1.14.148.67:443/rtc/v1/publish/
	// https://1.14.148.67:443/rtc/v1/play/
	httpRequestUrl_ = "https://" + server_ip + ":" + DEFALUT_HTTPS_PORT;
	httpRequestUrl_ += "/rtc/v1/" + control + "/";

	if (!channel.empty()) {
		channel_ = channel;
	}

	// "webrtc://1.14.148.67/55555/1c2582f"
	webrtcStreamUrl_ = "webrtc://" + server_ip + "/" + channel_;
}

KRTCMediaBase::~KRTCMediaBase() {}

} // namespace krtc
