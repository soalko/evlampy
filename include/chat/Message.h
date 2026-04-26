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
	std::vector<uint8_t> encryptedSessionKey;
	std::vector<uint8_t> encryptedPayload;
	std::int64_t timestamp;
	MessageStatus status;

	static Message build(const std::string& sender,
						 const std::string& receiver,
						 std::vector<uint8_t> encryptedSessionKey,
						 std::vector<uint8_t> encryptedPayload);
};
