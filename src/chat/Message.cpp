#include "chat/Message.h"

#include <chrono>
#include <random>
#include <sstream>

namespace {
std::string generateId() {
    static std::mt19937_64 generator(std::random_device{}());
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto rnd = generator();

    std::ostringstream os;
    os << std::hex << now << "-" << rnd;
    return os.str();
}

std::int64_t nowUnixMillis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}
}

Message Message::build(const std::string& sender,
                      const std::string& receiver,
                      std::vector<uint8_t> encryptedSessionKey,
                      std::vector<uint8_t> encryptedPayload,
                      std::vector<uint8_t> encryptedSelfSessionKey,
                      std::string chatId,
                      bool isGroupMessage) {
    Message message;
    message.messageId = generateId();
    message.sender = sender;
    message.receiver = receiver;
    message.chatId = std::move(chatId);
    message.encryptedSessionKey = std::move(encryptedSessionKey);
    message.encryptedSelfSessionKey = std::move(encryptedSelfSessionKey);
    message.encryptedPayload = std::move(encryptedPayload);
    message.timestamp = nowUnixMillis();
    message.status = MessageStatus::Sent;
    message.isGroupMessage = isGroupMessage;
    message.deletedForAll = false;
    return message;
}

