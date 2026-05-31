#include "network/TcpClient.h"

TcpClient::TcpClient(TcpServer& server)
	: server_(server) {
}

void TcpClient::setOnMessage(ReceiveHandler handler) {
	onMessage_ = std::move(handler);
}

bool TcpClient::connect(const std::string& username) {
	if (connected_) {
		return false;
	}

	const bool ok = server_.connectClient(
		username,
		[this](const std::string& fromUser, const std::string& payload) {
			if (onMessage_) {
				onMessage_(fromUser, payload);
			}
		}
	);

	if (ok) {
		connected_ = true;
		username_ = username;
	}
	return ok;
}

void TcpClient::disconnect() {
	if (!connected_) {
		return;
	}
	server_.disconnectClient(username_);
	connected_ = false;
	username_.clear();
}

bool TcpClient::send(const std::string& toUser, const std::string& payload) const {
	if (!connected_) {
		return false;
	}
	return server_.sendTo(username_, toUser, payload);
}

bool TcpClient::isConnected() const {
	return connected_;
}

const std::string& TcpClient::username() const {
	return username_;
}

