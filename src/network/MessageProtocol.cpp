#include "network/MessageProtocol.h"

#include <openssl/evp.h>

#include <sstream>
#include <stdexcept>

namespace {
std::vector<std::string> split(const std::string& text, char delimiter) {
	std::vector<std::string> parts;
	std::string current;
	for (char ch : text) {
		if (ch == delimiter) {
			parts.push_back(current);
			current.clear();
		} else {
			current.push_back(ch);
		}
	}
	parts.push_back(current);
	return parts;
}
}

std::string MessageProtocol::serialize(const Message& message) {
	std::ostringstream os;
	os << message.messageId << '|'
	   << message.sender << '|'
	   << message.receiver << '|'
	   << message.timestamp << '|'
	   << static_cast<int>(message.status) << '|'
	   << base64Encode(message.encryptedSessionKey) << '|'
	   << base64Encode(message.encryptedPayload);
	return os.str();
}

Message MessageProtocol::deserialize(const std::string& wireData) {
	const auto parts = split(wireData, '|');
	if (parts.size() != 7) {
		throw std::invalid_argument("Wire message has invalid field count");
	}

	Message message;
	message.messageId = parts[0];
	message.sender = parts[1];
	message.receiver = parts[2];
	message.timestamp = std::stoll(parts[3]);
	message.status = static_cast<MessageStatus>(std::stoi(parts[4]));
	message.encryptedSessionKey = base64Decode(parts[5]);
	message.encryptedPayload = base64Decode(parts[6]);
	return message;
}

std::string MessageProtocol::base64Encode(const std::vector<uint8_t>& data) {
	if (data.empty()) {
		return {};
	}

	const int outLen = 4 * ((static_cast<int>(data.size()) + 2) / 3);
	std::string out(static_cast<std::size_t>(outLen), '\0');
	const int written = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(&out[0]),
										data.data(),
										static_cast<int>(data.size()));
	if (written <= 0) {
		throw std::runtime_error("Base64 encode failed");
	}
	out.resize(static_cast<std::size_t>(written));
	return out;
}

std::vector<uint8_t> MessageProtocol::base64Decode(const std::string& base64) {
	if (base64.empty()) {
		return {};
	}

	std::vector<uint8_t> out((base64.size() * 3) / 4 + 2);
	const int written = EVP_DecodeBlock(out.data(),
										reinterpret_cast<const unsigned char*>(base64.data()),
										static_cast<int>(base64.size()));
	if (written < 0) {
		throw std::runtime_error("Base64 decode failed");
	}

	std::size_t pad = 0;
	if (!base64.empty() && base64.back() == '=') {
		pad++;
		if (base64.size() > 1 && base64[base64.size() - 2] == '=') {
			pad++;
		}
	}

	out.resize(static_cast<std::size_t>(written) - pad);
	return out;
}

