#pragma once

#include "chat/Message.h"

#include <string>
#include <vector>

class MessageProtocol {
public:
	static std::string serialize(const Message& message);
	static Message deserialize(const std::string& wireData);

	static std::string base64Encode(const std::vector<uint8_t>& data);
	static std::vector<uint8_t> base64Decode(const std::string& base64);
};
