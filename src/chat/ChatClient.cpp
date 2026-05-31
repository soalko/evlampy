#include "chat/ChatClient.h"
#include "network/MessageProtocol.h"
#include "patterns/Logger.h"
#include <openssl/evp.h>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstdlib>

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
    if (registered_) return false;
    keyPair_ = keyFactory_.generateRsaKeyPair();
    registered_ = server_.registerUser(username_, hashPassword(rawPassword), keyPair_.publicKeyPem);
    if (registered_) {
        saveKeyPairToDisk();
        Logger::instance().info("User '" + username_ + "' registered");
    }
    return registered_;
}

bool ChatClient::login(const std::string& rawPassword) {
    if (loggedIn_) return true;
    if (!server_.authenticate(username_, hashPassword(rawPassword))) return false;
    if (!loadKeyPairFromDisk()) {
        Logger::instance().error("User '" + username_ + "' private key not found or corrupted");
        return false;
    }
    loggedIn_ = transport_.connect(username_);
    if (loggedIn_) registered_ = true;
    if (loggedIn_) Logger::instance().info("User '" + username_ + "' logged in");
    return loggedIn_;
}

void ChatClient::logout() {
    transport_.disconnect();
    loggedIn_ = false;
    Logger::instance().info("User '" + username_ + "' logged out");
}

bool ChatClient::sendMessage(const std::string& toUser, const std::string& plaintext) {
    if (!loggedIn_) return false;
    const std::string receiverPublicKey = server_.getPublicKey(toUser);
    if (receiverPublicKey.empty()) {
        Logger::instance().error("Receiver '" + toUser + "' not found");
        return false;
    }
    const auto sessionKey = keyFactory_.generateSymmetricKey(32);
    const auto encryptedPayload = encryption_.encrypt(toBytes(plaintext), sessionKey);
    const auto encryptedSessionKey = keyFactory_.encryptWithPublicKey(receiverPublicKey, sessionKey);
    const auto encryptedSelfSessionKey = keyFactory_.encryptWithPublicKey(keyPair_.publicKeyPem, sessionKey);
    Message message = Message::build(username_, toUser, encryptedSessionKey, encryptedPayload,
                                     encryptedSelfSessionKey, {}, false);
    message.status = transport_.send(toUser, MessageProtocol::serialize(message))
                         ? MessageStatus::Delivered : MessageStatus::Sent;
    server_.storeMessage(message);
    return true;
}

bool ChatClient::sendGroupMessage(const std::string& groupId, const std::string& plaintext) {
    if (!loggedIn_) return false;
    const auto members = server_.getGroupMembers(groupId);
    if (members.empty()) return false;
    const auto sessionKey = keyFactory_.generateSymmetricKey(32);
    const auto encryptedPayload = encryption_.encrypt(toBytes(plaintext), sessionKey);
    bool anySent = false;
    for (const auto& member : members) {
        const std::string receiverPublicKey = server_.getPublicKey(member);
        if (receiverPublicKey.empty()) continue;
        const auto encryptedSessionKey = keyFactory_.encryptWithPublicKey(receiverPublicKey, sessionKey);
        // Для группы отправитель входит в список members – получит сообщение через обычный encryptedSessionKey
        Message message = Message::build(username_, member, encryptedSessionKey, encryptedPayload,
                                         {}, groupId, true);
        message.status = transport_.send(member, MessageProtocol::serialize(message))
                             ? MessageStatus::Delivered : MessageStatus::Sent;
        server_.storeMessage(message);
        anySent = true;
    }
    return anySent;
}

bool ChatClient::deleteMessageForAll(const std::string& messageId) {
    if (!loggedIn_ || messageId.empty()) return false;
    bool deleted = server_.deleteMessageForAll(username_, messageId);
    if (deleted) Logger::instance().info("User '" + username_ + "' deleted message '" + messageId + "' for all");
    else Logger::instance().error("User '" + username_ + "' failed to delete message '" + messageId + "'");
    return deleted;
}

std::vector<std::string> ChatClient::listUsers() const {
    return server_.listUsers();
}
std::vector<std::string> ChatClient::listContacts() const {
    return server_.listContacts(username_);
}
bool ChatClient::addContact(const std::string& contact) {
    return server_.addContact(username_, contact);
}
bool ChatClient::removeContact(const std::string& contact) {
    return server_.removeContact(username_, contact);
}
std::optional<std::string> ChatClient::createGroup(const std::string& groupName, const std::vector<std::string>& members) {
    return server_.createGroup(username_, groupName, members);
}
std::vector<std::string> ChatClient::listGroups() const {
    return server_.listGroups(username_);
}

MessageView ChatClient::decryptToView(const Message& message) const {
    // Для отправленных сообщений используем encryptedSelfSessionKey, иначе обычный ключ
    const std::vector<uint8_t>* keyToUse = nullptr;
    if (message.sender == username_ && !message.encryptedSelfSessionKey.empty()) {
        keyToUse = &message.encryptedSelfSessionKey;
    } else {
        keyToUse = &message.encryptedSessionKey;
    }
    const auto sessionKey = keyFactory_.decryptWithPrivateKey(keyPair_.privateKeyPem, *keyToUse);
    const auto plaintext = encryption_.decrypt(message.encryptedPayload, sessionKey);
    return MessageView{
        message.messageId,
        message.sender,
        message.receiver,
        message.chatId,
        fromBytes(plaintext),
        message.timestamp,
        message.status,
        message.isGroupMessage,
        message.deletedForAll
    };
}

std::vector<MessageView> ChatClient::getConversation(const std::string& peer) const {
    std::vector<MessageView> out;
    for (const auto& message : server_.getConversation(username_, peer)) {
        // Показываем и отправленные, и полученные сообщения
        out.push_back(decryptToView(message));
    }
    return out;
}

std::vector<MessageView> ChatClient::getGroupConversation(const std::string& groupId) const {
    std::vector<MessageView> out;
    for (const auto& message : server_.getGroupConversation(groupId)) {
        if (message.receiver == username_) {
            out.push_back(decryptToView(message));
        }
    }
    return out;
}

const std::string& ChatClient::username() const { return username_; }
bool ChatClient::isLoggedIn() const { return loggedIn_; }

std::string ChatClient::hashPassword(const std::string& rawPassword) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLen = 0;
    if (EVP_Digest(rawPassword.data(), rawPassword.size(), digest, &digestLen, EVP_sha256(), nullptr) != 1)
        throw std::runtime_error("Failed to hash password");
    static const char* hex = "0123456789abcdef";
    std::string out; out.reserve(digestLen * 2);
    for (unsigned int i = 0; i < digestLen; ++i) {
        unsigned char b = digest[i];
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
        Message message = MessageProtocol::deserialize(payload);
        const std::vector<uint8_t>* keyToUse = nullptr;
        if (message.sender == username_ && !message.encryptedSelfSessionKey.empty()) {
            keyToUse = &message.encryptedSelfSessionKey;
        } else {
            keyToUse = &message.encryptedSessionKey;
        }
        const auto sessionKey = keyFactory_.decryptWithPrivateKey(keyPair_.privateKeyPem, *keyToUse);
        const auto plaintext = encryption_.decrypt(message.encryptedPayload, sessionKey);
        notify({fromUser, fromBytes(plaintext), message.chatId, message.messageId, message.isGroupMessage});
        Logger::instance().info("User '" + username_ + "' received message from '" + fromUser + "'");
    } catch (const std::exception& ex) {
        Logger::instance().error("Failed to decode message for '" + username_ + "': " + ex.what());
    }
}

void ChatClient::saveKeyPairToDisk() const {
    auto homeDir = std::getenv("HOME");
    if (!homeDir) { Logger::instance().error("Cannot determine HOME directory"); return; }
    std::string keysDir = std::string(homeDir) + "/.evlampy/keys";
    try { std::filesystem::create_directories(keysDir); } catch(...) { return; }
    std::string privKeyPath = keysDir + "/" + username_ + ".priv.pem";
    std::string pubKeyPath = keysDir + "/" + username_ + ".pub.pem";
    try {
        std::ofstream privFile(privKeyPath); privFile << keyPair_.privateKeyPem; privFile.close();
        std::ofstream pubFile(pubKeyPath); pubFile << keyPair_.publicKeyPem; pubFile.close();
        Logger::instance().info("Saved keys for user '" + username_ + "'");
    } catch(...) {}
}

bool ChatClient::loadKeyPairFromDisk() {
    auto homeDir = std::getenv("HOME");
    if (!homeDir) return false;
    std::string keysDir = std::string(homeDir) + "/.evlampy/keys";
    std::string privKeyPath = keysDir + "/" + username_ + ".priv.pem";
    std::string pubKeyPath = keysDir + "/" + username_ + ".pub.pem";
    try {
        std::ifstream privFile(privKeyPath); if (!privFile) return false;
        std::stringstream privBuffer; privBuffer << privFile.rdbuf(); keyPair_.privateKeyPem = privBuffer.str(); privFile.close();
        std::ifstream pubFile(pubKeyPath); if (!pubFile) return false;
        std::stringstream pubBuffer; pubBuffer << pubFile.rdbuf(); keyPair_.publicKeyPem = pubBuffer.str(); pubFile.close();
        return true;
    } catch(...) { return false; }
}