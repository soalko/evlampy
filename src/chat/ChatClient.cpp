#include "chat/ChatClient.h"

#include "network/MessageProtocol.h"
#include "patterns/Logger.h"

#include <openssl/evp.h>

#include <stdexcept>

ChatClient::ChatClient(std::string username,
					   ChatServer& server,
					   OpenSSLKeyFactory& keyFactory,
					   AES256GCMStrategy& encryption)
	: username_(std::move(username)),
	  server_(server),
	  keyFactory_(keyFactory),
	  encryption_(encryption),
	  transport_(server.transport()) {
	transport_.setOnMessage([this](const std::string& fromUser, const std::string& payload) {
		handleIncoming(fromUser, payload);
	});
}

bool ChatClient::registerOnServer(const std::string& rawPassword) {
	if (registered_) {
		return false;
	}

	keyPair_ = keyFactory_.generateRsaKeyPair();
	registered_ = server_.registerUser(username_, hashPassword(rawPassword), keyPair_.publicKeyPem);
	if (registered_) {
		Logger::instance().info("User '" + username_ + "' registered");
	}
	return registered_;
}

bool ChatClient::login(const std::string& rawPassword) {
	if (!registered_) {
		return false;
	}
	if (!server_.authenticate(username_, hashPassword(rawPassword))) {
		return false;
	}

	loggedIn_ = transport_.connect(username_);
	if (loggedIn_) {
		Logger::instance().info("User '" + username_ + "' logged in");
	}
	return loggedIn_;
}

void ChatClient::logout() {
	transport_.disconnect();
	loggedIn_ = false;
	Logger::instance().info("User '" + username_ + "' logged out");
}

bool ChatClient::sendMessage(const std::string& toUser, const std::string& plaintext) {
	if (!loggedIn_) {
		return false;
	}

	const std::string receiverPublicKey = server_.getPublicKey(toUser);
	if (receiverPublicKey.empty()) {
		Logger::instance().error("Receiver '" + toUser + "' not found");
		return false;
	}

	const auto sessionKey = keyFactory_.generateSymmetricKey(32);
	const auto encryptedPayload = encryption_.encrypt(toBytes(plaintext), sessionKey);
	const auto encryptedSessionKey = keyFactory_.encryptWithPublicKey(receiverPublicKey, sessionKey);

	Message message = Message::build(username_, toUser, encryptedSessionKey, encryptedPayload);
	message.status = transport_.send(toUser, MessageProtocol::serialize(message))
						 ? MessageStatus::Delivered
						 : MessageStatus::Sent;

	server_.storeMessage(message);
	return true;
}

const std::string& ChatClient::username() const {
	return username_;
}

bool ChatClient::isLoggedIn() const {
	return loggedIn_;
}

std::string ChatClient::hashPassword(const std::string& rawPassword) {
	unsigned char digest[EVP_MAX_MD_SIZE];
	unsigned int digestLen = 0;

	if (EVP_Digest(rawPassword.data(),
				   rawPassword.size(),
				   digest,
				   &digestLen,
				   EVP_sha256(),
				   nullptr) != 1) {
		throw std::runtime_error("Failed to hash password");
	}

	static const char* hex = "0123456789abcdef";
	std::string out;
	out.reserve(digestLen * 2);
	for (unsigned int i = 0; i < digestLen; ++i) {
		const auto b = static_cast<unsigned char>(digest[i]);
		out.push_back(hex[(b >> 4) & 0x0f]);
		out.push_back(hex[b & 0x0f]);
	}
	return out;
}

std::vector<uint8_t> ChatClient::toBytes(const std::string& value) const {
	return {value.begin(), value.end()};
}

std::string ChatClient::fromBytes(const std::vector<uint8_t>& value) const {
	return {value.begin(), value.end()};
}

void ChatClient::handleIncoming(const std::string& fromUser, const std::string& payload) {
	try {
		const Message message = MessageProtocol::deserialize(payload);
		const auto sessionKey = keyFactory_.decryptWithPrivateKey(keyPair_.privateKeyPem, message.encryptedSessionKey);
		const auto plaintext = encryption_.decrypt(message.encryptedPayload, sessionKey);

		notify({fromUser, fromBytes(plaintext)});
		Logger::instance().info("User '" + username_ + "' received message from '" + fromUser + "'");
	} catch (const std::exception& ex) {
		Logger::instance().error("Failed to decode message for '" + username_ + "': " + ex.what());
	}
}

