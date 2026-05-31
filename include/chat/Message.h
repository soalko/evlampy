#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class MessageStatus {
	Sent,
	Delivered,
	Read
};

struct Message {
	std::string messageId;
	std::string sender;
	std::string receiver;
	std::string chatId;
	std::vector<uint8_t> encryptedSessionKey;
	std::vector<uint8_t> encryptedSelfSessionKey;
	std::vector<uint8_t> encryptedPayload;
	std::int64_t timestamp;
	MessageStatus status;
	bool isGroupMessage = false;
	bool deletedForAll = false;

	static Message build(const std::string& sender,
						 const std::string& receiver,
						 std::vector<uint8_t> encryptedSessionKey,
						 std::vector<uint8_t> encryptedPayload,
						 std::vector<uint8_t> encryptedSelfSessionKey = {},
						 std::string chatId = {},
						 bool isGroupMessage = false);
};
