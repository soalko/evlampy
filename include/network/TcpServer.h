#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

class TcpServer {
public:
	using ReceiveHandler = std::function<void(const std::string& fromUser, const std::string& payload)>;

	bool connectClient(const std::string& username, ReceiveHandler handler);
	void disconnectClient(const std::string& username);

	bool sendTo(const std::string& fromUser, const std::string& toUser, const std::string& payload);
	bool isOnline(const std::string& username) const;

private:
	mutable std::mutex mutex_;
	std::unordered_map<std::string, ReceiveHandler> handlers_;
};

