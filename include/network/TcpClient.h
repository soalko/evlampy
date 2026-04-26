#pragma once

#include "network/TcpServer.h"

#include <functional>
#include <string>

class TcpClient {
public:
	using ReceiveHandler = std::function<void(const std::string& fromUser, const std::string& payload)>;

	explicit TcpClient(TcpServer& server);

	void setOnMessage(ReceiveHandler handler);
	bool connect(const std::string& username);
	void disconnect();

	bool send(const std::string& toUser, const std::string& payload) const;
	[[nodiscard]] bool isConnected() const;
	[[nodiscard]] const std::string& username() const;

private:
	TcpServer& server_;
	std::string username_;
	bool connected_ = false;
	ReceiveHandler onMessage_;
};
