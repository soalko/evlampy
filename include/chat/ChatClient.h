#pragma once

#include "chat/ChatServer.h"
#include "encryption/AES256GCMStrategy.h"
#include "encryption/OpenSSLKeyFactory.h"
#include "network/TcpClient.h"
#include "patterns/Observer.h"

#include <string>

struct DecryptedMessageEvent {
	std::string fromUser;
	std::string text;
};

class ChatClient : public Subject<DecryptedMessageEvent> {
public:
	ChatClient(std::string username,
			   ChatServer& server,
			   OpenSSLKeyFactory& keyFactory,
			   AES256GCMStrategy& encryption);

	bool registerOnServer(const std::string& rawPassword);
	bool login(const std::string& rawPassword);
	void logout();

	bool sendMessage(const std::string& toUser, const std::string& plaintext);

	[[nodiscard]] const std::string& username() const;
	[[nodiscard]] bool isLoggedIn() const;

private:
	static std::string hashPassword(const std::string& rawPassword);
	std::vector<uint8_t> toBytes(const std::string& value) const;
	std::string fromBytes(const std::vector<uint8_t>& value) const;
	void handleIncoming(const std::string& fromUser, const std::string& payload);

	std::string username_;
	ChatServer& server_;
	OpenSSLKeyFactory& keyFactory_;
	AES256GCMStrategy& encryption_;
	TcpClient transport_;

	KeyPair keyPair_;
	bool registered_ = false;
	bool loggedIn_ = false;
};

