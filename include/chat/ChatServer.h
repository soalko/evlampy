#pragma once

#include "chat/Message.h"
#include "network/TcpServer.h"

#include <mutex>
#include <optional>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

class ChatServer {
public:
	bool registerUser(const std::string& username,
					  const std::string& passwordHash,
					  const std::string& publicKeyPem);

	bool authenticate(const std::string& username, const std::string& passwordHash) const;
	std::string getPublicKey(const std::string& username) const;
	std::vector<std::string> listUsers() const;

	bool addContact(const std::string& owner, const std::string& contact);
	bool removeContact(const std::string& owner, const std::string& contact);
	std::vector<std::string> listContacts(const std::string& owner) const;

	std::optional<std::string> createGroup(const std::string& owner,
								 const std::string& groupName,
								 const std::vector<std::string>& members);
	std::vector<std::string> listGroups(const std::string& username) const;
	std::vector<std::string> getGroupMembers(const std::string& groupId) const;
	std::string getGroupName(const std::string& groupId) const;

	void storeMessage(const Message& message);
	std::vector<Message> getConversation(const std::string& userA, const std::string& userB) const;
	std::vector<Message> getGroupConversation(const std::string& groupId) const;
	bool deleteMessageForAll(const std::string& requester, const std::string& messageId);

	TcpServer& transport();
	const TcpServer& transport() const;

private:
	struct UserRecord {
		std::string passwordHash;
		std::string publicKeyPem;
		std::unordered_set<std::string> contacts;
	};

	struct GroupRecord {
		std::string groupName;
		std::string owner;
		std::unordered_set<std::string> members;
	};

	static std::string makeConversationKey(const std::string& a, const std::string& b);

	mutable std::mutex mutex_;
	std::unordered_map<std::string, UserRecord> users_;
	std::unordered_map<std::string, std::vector<Message>> history_;
	std::unordered_map<std::string, std::vector<Message>> groupHistory_;
	std::unordered_map<std::string, GroupRecord> groups_;
	std::uint64_t nextGroupId_ = 1;
	TcpServer transport_;
};
