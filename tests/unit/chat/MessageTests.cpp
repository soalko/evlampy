#include "chat/Message.h"

#include <gtest/gtest.h>

#include <chrono>

TEST(MessageTests, BuildSetsSenderReceiverAndPayload) {
    const auto msg = Message::build("alice", "bob", {1}, {3, 4});

    EXPECT_EQ(msg.sender, "alice");
    EXPECT_EQ(msg.receiver, "bob");
    EXPECT_EQ(msg.encryptedSessionKey, std::vector<uint8_t>({1}));
    EXPECT_EQ(msg.encryptedPayload, std::vector<uint8_t>({3, 4}));
}

TEST(MessageTests, BuildGeneratesNonEmptyId) {
    const auto msg = Message::build("alice", "bob", {}, {});
    EXPECT_FALSE(msg.messageId.empty());
}

TEST(MessageTests, BuildGeneratesUniqueIds) {
    const auto a = Message::build("alice", "bob", {}, {});
    const auto b = Message::build("alice", "bob", {}, {});
    EXPECT_NE(a.messageId, b.messageId);
}

TEST(MessageTests, BuildSetsTimestampCloseToNow) {
    using namespace std::chrono;
    const auto before = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    const auto msg = Message::build("alice", "bob", {}, {});
    const auto after = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

    EXPECT_GE(msg.timestamp, before);
    EXPECT_LE(msg.timestamp, after);
}

TEST(MessageTests, BuildSetsDefaultStatusToSent) {
    const auto msg = Message::build("alice", "bob", {}, {});
    EXPECT_EQ(msg.status, MessageStatus::Sent);
}

TEST(MessageTests, BuildSupportsEmptyCipherFields) {
    const auto msg = Message::build("alice", "bob", {}, {});
    EXPECT_TRUE(msg.encryptedSessionKey.empty());
    EXPECT_TRUE(msg.encryptedPayload.empty());
}
