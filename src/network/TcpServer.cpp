#include "network/TcpServer.h"

bool TcpServer::connectClient(const std::string& username, ReceiveHandler handler) {
	std::lock_guard<std::mutex> lock(mutex_);
	return handlers_.emplace(username, std::move(handler)).second;
}

void TcpServer::disconnectClient(const std::string& username) {
	std::lock_guard<std::mutex> lock(mutex_);
	handlers_.erase(username);
}

bool TcpServer::sendTo(const std::string& fromUser, const std::string& toUser, const std::string& payload) {
	ReceiveHandler handler;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		const auto it = handlers_.find(toUser);
		if (it == handlers_.end()) {
			return false;
		}
		handler = it->second;
	}

	handler(fromUser, payload);
	return true;
}

bool TcpServer::isOnline(const std::string& username) const {
	std::lock_guard<std::mutex> lock(mutex_);
	return handlers_.find(username) != handlers_.end();
}

