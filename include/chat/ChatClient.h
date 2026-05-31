#pragma once

#include "chat/ChatServer.h"
#include "encryption/AES256GCMStrategy.h"
#include "encryption/OpenSSLKeyFactory.h"
#include "network/TcpClient.h"
#include "patterns/Observer.h"

#include <string>
#include <vector>
#include <optional>

struct MessageView {
	std::string messageId;
	std::string fromUser;
	std::string toUser;
	std::string chatId;
	std::string text;
	std::int64_t timestamp = 0;
	MessageStatus status = MessageStatus::Sent;
	bool isGroupMessage = false;
	bool deletedForAll = false;
};

struct DecryptedMessageEvent {
	std::string fromUser;
	std::string text;
	std::string chatId;
	std::string messageId;
	bool isGroupMessage = false;
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
	bool sendGroupMessage(const std::string& groupId, const std::string& plaintext);
	bool deleteMessageForAll(const std::string& messageId);

	std::vector<std::string> listUsers() const;
	std::vector<std::string> listContacts() const;
	bool addContact(const std::string& contact);
	bool removeContact(const std::string& contact);
	std::optional<std::string> createGroup(const std::string& groupName, const std::vector<std::string>& members);
	std::vector<std::string> listGroups() const;
	std::vector<MessageView> getConversation(const std::string& peer) const;
	std::vector<MessageView> getGroupConversation(const std::string& groupId) const;

	[[nodiscard]] const std::string& username() const;
	[[nodiscard]] bool isLoggedIn() const;

private:
	static std::string hashPassword(const std::string& rawPassword);
	std::vector<uint8_t> toBytes(const std::string& value) const;
	std::string fromBytes(const std::vector<uint8_t>& value) const;
	MessageView decryptToView(const Message& message) const;
	void handleIncoming(const std::string& fromUser, const std::string& payload);
	void saveKeyPairToDisk() const;
	bool loadKeyPairFromDisk();

	std::string username_;
	ChatServer& server_;
	OpenSSLKeyFactory& keyFactory_;
	AES256GCMStrategy& encryption_;
	TcpClient transport_;

	KeyPair keyPair_;
	bool registered_ = false;
	bool loggedIn_ = false;
};

