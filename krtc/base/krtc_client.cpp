#include "krtc/base/krtc_client.h"

#include <rtc_base/strings/json.h>
#include <rtc_base/logging.h>
#include <rtc_base/helpers.h>

#include "krtc/base/krtc_global.h"
#include "krtc/base/krtc_json.h"

namespace krtc {

constexpr uint16_t DEFAULT_WS_PORT = 1989;

KRTCClient* KRTCClient::Instance() {
	static KRTCClient* const instance = new KRTCClient();
	return instance;
}

bool KRTCClient::JoinRoom(const std::string& server_addr, const std::string& room_id, 
	const std::string& uid)
{
	RTC_LOG(LS_INFO) << "client join room! ";

	if (room_id.empty() || uid.empty()) {
		return false;
	}

	room_id_ = room_id;
	uid_ = uid;

	std::string server_ip, server_port;
	if (!rtc::tokenize_first(server_addr, ':', &server_ip, &server_port)) {
		server_ip = server_addr;
	}

	uint16_t ws_port = DEFAULT_WS_PORT;
	if (!server_port.empty()) {
		ws_port = std::atoi(server_port.c_str());
	}

	std::string path = "/sig/v1/rtc";
	path += "?room=" + room_id_;
	path += "&display=" + uid_;

	websocket_client_.reset(new WebsocketClient(server_ip, ws_port, path));
	websocket_client_->setMessageCallback([this](const std::string& message) {
		// assert(); 是否是当前线程
		HandleWebsocketMessage(message);
	});
	websocket_client_->setStatusCallback([this](const WEBSOCKET_STATUS& status) {
		HandleWebsocketConnectStatus(status);
	});

	current_status_ = WEBSOCKET_STATUS::CONNECTING;
	bool success = false;

	std::promise<bool> connect_promise;
	std::thread connect_thread([this, &connect_promise] {
		websocket_client_->connect();
		while (1) {
			if (current_status_ == WEBSOCKET_STATUS::CONNECT_SUCCESS) {
				connect_promise.set_value(true);
				return;
			}
			else if (current_status_ == WEBSOCKET_STATUS::CONNECT_FAILED 
					|| current_status_ == WEBSOCKET_STATUS::CONNECT_CLOSE) {
				connect_promise.set_value(false);
				return;
			}
			else {
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		}
	});
	connect_thread.join();
	success = connect_promise.get_future().get();
	if (!success) {
		RTC_LOG(LS_INFO) << "websocket connect failed! ";
		if (KRTCGlobal::Instance()->engine_observer()) {
			KRTCGlobal::Instance()->engine_observer()->OnPushFailed(krtc::KRTCError::kConnectWebsocketErr);
		}
		return false;
	}

	success = false;
	std::promise<bool> msg_promise;
	std::thread msg_thread([this, &msg_promise] {
		current_status_ = WEBSOCKET_STATUS::MESSAGE_SEND;
		SendAction(MSG_ACTION::JOIN);
		while (1) {
			if (current_status_ == WEBSOCKET_STATUS::MESSAGE_SEND_SUCCESS) {
				msg_promise.set_value(true);
				return;
			}
			else if (current_status_ == WEBSOCKET_STATUS::MESSAGE_SEND_FAILED) {
				msg_promise.set_value(false);
				return;
			}
			else {
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		}
	});
	msg_thread.join();
	success = msg_promise.get_future().get();
	if (!success) {
		RTC_LOG(LS_ERROR) << "websocket send join msg failed! ";
		return false;
	}

	is_joined_ = true;

	return true;
}

void KRTCClient::LeaveRoom()
{
	RTC_LOG(LS_INFO) << "client leave room! ";

	if (!is_joined_) {
		return;
	}

	SendAction(MSG_ACTION::LEAVE);
	if (websocket_client_) {
		websocket_client_->disconnect();
	}

	is_joined_ = false;
}

/*
* 大致格式，如果是
{"tid":"jRxAiWF","msg":{"action":"join","room":"55555","self":{"display":"111","publishing":false},"participants":[{"display":"8a38581","publishing":true},{"display":"111","publishing":false}]}}
{"msg":{"action":"notify","event":"publish","room":"5dd66a5","self":{"display":"11111","publishing":true},"peer":{"display":"226e9a4","publishing":true},"participants":[{"display":"11111","publishing":true},{"display":"226e9a4","publishing":true}]}}
*/
void KRTCClient::HandleWebsocketMessage(const std::string& json_msg)
{
	RTC_LOG(LS_INFO) << "websocket recv server msg: " << json_msg;

	if (current_status_ == WEBSOCKET_STATUS::MESSAGE_SEND) {
		current_status_ = WEBSOCKET_STATUS::MESSAGE_SEND_SUCCESS;
	}

	JsonValue root;
	bool success = root.FromJson(json_msg);
	if (!success) {
		RTC_LOG(WARNING) << "Received unknown websocket message: " << json_msg;
		return;
	}

	JsonObject jobject = root.ToObject();
	JsonObject msgObject = jobject["msg"].ToObject();
	std::string action = msgObject["action"].ToString();

	if (action == "join") {
		std::map<std::string, bool> parcicipantsInfo;
		JsonArray participantsArray = msgObject["participants"].ToArray();
		for (int i = 0; i < participantsArray.Size(); ++i) {
			JsonObject oneObject = participantsArray[i].ToObject();
			std::string display = oneObject["display"].ToString();
			bool publishing = oneObject["publishing"].ToBool();
			if (uid_ == display) {
				continue;
			}
			parcicipantsInfo[display] = publishing;
		}

		if (parcicipantsInfo.size() == 0) {
			return;
		}

		if (KRTCGlobal::Instance()->msg_observer()) {
			KRTCGlobal::Instance()->msg_observer()->OnParcicipantsInfo(parcicipantsInfo);
		}

		return;
	}
	
	if (action == "notify") { // 通知
		std::string event = msgObject["event"].ToString();
		JsonObject peerObject = msgObject["peer"].ToObject();

		std::string display = peerObject["display"].ToString();
		bool publishing = peerObject["publishing"].ToBool();

		if (event == "join") {
			if (KRTCGlobal::Instance()->msg_observer()) {
				KRTCGlobal::Instance()->msg_observer()->OnParcicipantJoin(display, publishing);
			}
		}
		else if (event == "publish") {
			if (KRTCGlobal::Instance()->msg_observer()) {
				KRTCGlobal::Instance()->msg_observer()->OnParcicipantJoin(display, publishing);
			}
		}
		else if (event == "leave") {
			if (KRTCGlobal::Instance()->msg_observer()) {
				KRTCGlobal::Instance()->msg_observer()->OnParcicipantLeave(display, publishing);
			}
		}
		return;
	}

}

void KRTCClient::HandleWebsocketConnectStatus(const WEBSOCKET_STATUS& status)
{
	current_status_ = status;
}

void KRTCClient::SendPublishMsg()
{
	bool success = false;
	std::promise<bool> msg_promise;
	std::thread msg_thread([this, &msg_promise] {
		current_status_ = WEBSOCKET_STATUS::MESSAGE_SEND;
		SendAction(MSG_ACTION::PUBLISH);
		while (1) {
			if (current_status_ == WEBSOCKET_STATUS::MESSAGE_SEND_SUCCESS) {
				msg_promise.set_value(true);
				return;
			}
			else if (current_status_ == WEBSOCKET_STATUS::MESSAGE_SEND_FAILED) {
				msg_promise.set_value(false);
				return;
			}
			else {
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		}
	});
	msg_thread.join();
	success = msg_promise.get_future().get();

	if (!success) {
		RTC_LOG(LS_ERROR) << "websocket send publish msg failed! ";
	}

}

static std::string GetActionString(const MSG_ACTION& action) {
	std::string strAction;
	switch (action) {
	case MSG_ACTION::JOIN:
		strAction = "join";
		break;
	case MSG_ACTION::PUBLISH:
		strAction = "publish";
		break;
	case MSG_ACTION::LEAVE:
		strAction = "leave";
		break;
	default:
		RTC_LOG(LS_ERROR) << "no such action: " << action;
	}
	return std::move(strAction);
}

void KRTCClient::SendAction(const MSG_ACTION& action)
{
	// {"tid":"398d320","msg":{"action":"join","room":"5dd66a5","display":"50b8507"}}
	std::string strAction = GetActionString(action);
	if (strAction.empty()) {
		return;
	}
	
	Json::Value rootMsg;
	Json::Value reqMsg;
	reqMsg["action"] = strAction;
	reqMsg["room"] = room_id_;
	reqMsg["display"] = uid_;
	rootMsg["tid"] = rtc::CreateRandomString(7);
	rootMsg["msg"] = reqMsg;
	Json::StreamWriterBuilder write_builder;
	write_builder.settings_["indentation"] = "";
	std::string json_data = Json::writeString(write_builder, rootMsg);
	RTC_LOG(LS_INFO) << "websocket client send msg: " << json_data;
	websocket_client_->send(json_data);
}

}
