#include "chat/ChatServer.h"

#include <algorithm>
#include <set>

namespace {
template <typename SetT>
std::vector<std::string> sortedFromSet(const SetT& input) {
	std::vector<std::string> out(input.begin(), input.end());
	std::sort(out.begin(), out.end());
	return out;
}
}

bool ChatServer::registerUser(const std::string& username,
							  const std::string& passwordHash,
							  const std::string& publicKeyPem) {
	std::lock_guard<std::mutex> lock(mutex_);
	if (users_.find(username) != users_.end()) {
		return false;
	}

	users_.emplace(username, UserRecord{passwordHash, publicKeyPem, {}});
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

std::vector<std::string> ChatServer::listUsers() const {
	std::lock_guard<std::mutex> lock(mutex_);
	std::vector<std::string> out;
	out.reserve(users_.size());
	for (const auto& [username, _] : users_) {
		out.push_back(username);
	}
	std::sort(out.begin(), out.end());
	return out;
}

bool ChatServer::addContact(const std::string& owner, const std::string& contact) {
	std::lock_guard<std::mutex> lock(mutex_);
	const auto ownerIt = users_.find(owner);
	const auto contactIt = users_.find(contact);
	if (ownerIt == users_.end() || contactIt == users_.end()) {
		return false;
	}
	return ownerIt->second.contacts.insert(contact).second;
}

bool ChatServer::removeContact(const std::string& owner, const std::string& contact) {
	std::lock_guard<std::mutex> lock(mutex_);
	const auto ownerIt = users_.find(owner);
	if (ownerIt == users_.end()) {
		return false;
	}
	return ownerIt->second.contacts.erase(contact) > 0;
}

std::vector<std::string> ChatServer::listContacts(const std::string& owner) const {
	std::lock_guard<std::mutex> lock(mutex_);
	const auto it = users_.find(owner);
	if (it == users_.end()) {
		return {};
	}
	return sortedFromSet(it->second.contacts);
}

std::optional<std::string> ChatServer::createGroup(const std::string& owner,
								 const std::string& groupName,
								 const std::vector<std::string>& members) {
	std::lock_guard<std::mutex> lock(mutex_);
	if (users_.find(owner) == users_.end()) {
		return std::nullopt;
	}

	GroupRecord record;
	record.groupName = groupName;
	record.owner = owner;
	record.members.insert(owner);
	for (const auto& member : members) {
		if (users_.find(member) != users_.end()) {
			record.members.insert(member);
		}
	}

	const std::string groupId = "group-" + std::to_string(nextGroupId_++) + "-" + groupName;
	groups_.emplace(groupId, std::move(record));
	return groupId;
}

std::vector<std::string> ChatServer::listGroups(const std::string& username) const {
	std::lock_guard<std::mutex> lock(mutex_);
	std::vector<std::string> out;
	for (const auto& [groupId, group] : groups_) {
		if (group.members.find(username) != group.members.end()) {
			out.push_back(groupId);
		}
	}
	std::sort(out.begin(), out.end());
	return out;
}

std::vector<std::string> ChatServer::getGroupMembers(const std::string& groupId) const {
	std::lock_guard<std::mutex> lock(mutex_);
	const auto it = groups_.find(groupId);
	if (it == groups_.end()) {
		return {};
	}
	return sortedFromSet(it->second.members);
}

std::string ChatServer::getGroupName(const std::string& groupId) const {
	std::lock_guard<std::mutex> lock(mutex_);
	const auto it = groups_.find(groupId);
	if (it == groups_.end()) {
		return {};
	}
	return it->second.groupName;
}

void ChatServer::storeMessage(const Message& message) {
	std::lock_guard<std::mutex> lock(mutex_);
	if (message.isGroupMessage) {
		groupHistory_[message.chatId].push_back(message);
		return;
	}
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

std::vector<Message> ChatServer::getGroupConversation(const std::string& groupId) const {
	std::lock_guard<std::mutex> lock(mutex_);
	const auto it = groupHistory_.find(groupId);
	if (it == groupHistory_.end()) {
		return {};
	}
	return it->second;
}

bool ChatServer::deleteMessageForAll(const std::string& requester, const std::string& messageId) {
	std::lock_guard<std::mutex> lock(mutex_);
	bool removed = false;
	for (auto& [_, messages] : history_) {
		const auto before = messages.size();
		messages.erase(std::remove_if(messages.begin(), messages.end(), [&](const Message& m) {
			return m.messageId == messageId && m.sender == requester;
		}), messages.end());
		removed = removed || messages.size() != before;
	}
	for (auto& [_, messages] : groupHistory_) {
		const auto before = messages.size();
		messages.erase(std::remove_if(messages.begin(), messages.end(), [&](const Message& m) {
			return m.messageId == messageId && m.sender == requester;
		}), messages.end());
		removed = removed || messages.size() != before;
	}
	return removed;
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

