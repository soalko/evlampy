#pragma once

#include "chat/Message.h"
#include "network/TcpServer.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class ChatServer {
public:
	bool registerUser(const std::string& username,
					  const std::string& passwordHash,
					  const std::string& publicKeyPem);

	bool authenticate(const std::string& username, const std::string& passwordHash) const;
	std::string getPublicKey(const std::string& username) const;

	void storeMessage(const Message& message);
	std::vector<Message> getConversation(const std::string& userA, const std::string& userB) const;

	TcpServer& transport();
	const TcpServer& transport() const;

private:
	struct UserRecord {
		std::string passwordHash;
		std::string publicKeyPem;
	};

	static std::string makeConversationKey(const std::string& a, const std::string& b);

	mutable std::mutex mutex_;
	std::unordered_map<std::string, UserRecord> users_;
	std::unordered_map<std::string, std::vector<Message>> history_;
	TcpServer transport_;
};
