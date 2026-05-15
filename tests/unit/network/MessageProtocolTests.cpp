#include "network/MessageProtocol.h"

#include <gtest/gtest.h>

TEST(MessageProtocolTests, SerializeDeserializeRoundtripDirectMessage) {
    Message msg;
    msg.messageId = "m1";
    msg.sender = "alice";
    msg.receiver = "bob";
    msg.chatId = "";
    msg.isGroupMessage = false;
    msg.timestamp = 123;
    msg.status = MessageStatus::Delivered;
    msg.encryptedSessionKey = {1, 2};
    msg.senderEncryptedSessionKey = {3, 4};
    msg.encryptedPayload = {5, 6, 7};
    msg.deletedForAll = false;

    const auto wire = MessageProtocol::serialize(msg);
    const auto decoded = MessageProtocol::deserialize(wire);

    EXPECT_EQ(decoded.messageId, msg.messageId);
    EXPECT_EQ(decoded.sender, msg.sender);
    EXPECT_EQ(decoded.receiver, msg.receiver);
    EXPECT_EQ(decoded.status, msg.status);
    EXPECT_EQ(decoded.encryptedPayload, msg.encryptedPayload);
}

TEST(MessageProtocolTests, SerializeDeserializeRoundtripGroupDeletedMessage) {
    Message msg;
    msg.messageId = "g1";
    msg.sender = "alice";
    msg.receiver = "bob";
    msg.chatId = "group:alice:team";
    msg.isGroupMessage = true;
    msg.timestamp = 456;
    msg.status = MessageStatus::Read;
    msg.deletedForAll = true;

    const auto wire = MessageProtocol::serialize(msg);
    const auto decoded = MessageProtocol::deserialize(wire);

    EXPECT_TRUE(decoded.isGroupMessage);
    EXPECT_TRUE(decoded.deletedForAll);
    EXPECT_EQ(decoded.chatId, msg.chatId);
}

TEST(MessageProtocolTests, DeserializeRejectsInvalidFieldCount) {
    EXPECT_THROW((void)MessageProtocol::deserialize("a|b|c"), std::invalid_argument);
}

TEST(MessageProtocolTests, Base64EncodeDecodeRoundtripBinary) {
    const std::vector<uint8_t> bin = {0, 1, 2, 254, 255};
    const auto encoded = MessageProtocol::base64Encode(bin);
    const auto decoded = MessageProtocol::base64Decode(encoded);

    EXPECT_EQ(decoded, bin);
}

TEST(MessageProtocolTests, Base64DecodeInvalidTextThrows) {
    EXPECT_THROW((void)MessageProtocol::base64Decode("@@@"), std::runtime_error);
}

TEST(MessageProtocolTests, Base64EncodeEmptyReturnsEmptyString) {
    EXPECT_TRUE(MessageProtocol::base64Encode({}).empty());
}

TEST(MessageProtocolTests, Base64DecodeEmptyReturnsEmptyVector) {
    EXPECT_TRUE(MessageProtocol::base64Decode("").empty());
}

