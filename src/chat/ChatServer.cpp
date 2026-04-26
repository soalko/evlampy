#include "chat/ChatServer.h"

#include <algorithm>

bool ChatServer::registerUser(const std::string& username,
							  const std::string& passwordHash,
							  const std::string& publicKeyPem) {
	std::lock_guard<std::mutex> lock(mutex_);
	if (users_.find(username) != users_.end()) {
		return false;
	}

	users_.emplace(username, UserRecord{passwordHash, publicKeyPem});
	return true;
}

bool ChatServer::authenticate(const std::string& username, const std::string& passwordHash) const {
	std::lock_guard<std::mutex> lock(mutex_);
	const auto it = users_.find(username);
	return it != users_.end() && it->second.passwordHash == passwordHash;
}

std::string ChatServer::getPublicKey(const std::string& username) const {
	std::lock_guard<std::mutex> lock(mutex_);
	const auto it = users_.find(username);
	if (it == users_.end()) {
		return {};
	}
	return it->second.publicKeyPem;
}

void ChatServer::storeMessage(const Message& message) {
	std::lock_guard<std::mutex> lock(mutex_);
	history_[makeConversationKey(message.sender, message.receiver)].push_back(message);
}

std::vector<Message> ChatServer::getConversation(const std::string& userA, const std::string& userB) const {
	std::lock_guard<std::mutex> lock(mutex_);
	const auto key = makeConversationKey(userA, userB);
	const auto it = history_.find(key);
	if (it == history_.end()) {
		return {};
	}
	return it->second;
}

TcpServer& ChatServer::transport() {
	return transport_;
}

const TcpServer& ChatServer::transport() const {
	return transport_;
}

std::string ChatServer::makeConversationKey(const std::string& a, const std::string& b) {
	if (a <= b) {
		return a + "::" + b;
	}
	return b + "::" + a;
}

