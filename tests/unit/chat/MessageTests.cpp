#include "chat/Message.h"

#include <gtest/gtest.h>

#include <chrono>

TEST(MessageTests, BuildSetsSenderReceiverAndPayload) {
    const auto msg = Message::build("alice", "bob", "", false, {1}, {2}, {3, 4});

    EXPECT_EQ(msg.sender, "alice");
    EXPECT_EQ(msg.receiver, "bob");
    EXPECT_EQ(msg.encryptedSessionKey, std::vector<uint8_t>({1}));
    EXPECT_EQ(msg.senderEncryptedSessionKey, std::vector<uint8_t>({2}));
    EXPECT_EQ(msg.encryptedPayload, std::vector<uint8_t>({3, 4}));
}

TEST(MessageTests, BuildSetsGroupFields) {
    const auto msg = Message::build("alice", "bob", "group:alice:team", true, {}, {}, {});

    EXPECT_TRUE(msg.isGroupMessage);
    EXPECT_EQ(msg.chatId, "group:alice:team");
}

TEST(MessageTests, BuildGeneratesNonEmptyId) {
    const auto msg = Message::build("alice", "bob", "", false, {}, {}, {});
    EXPECT_FALSE(msg.messageId.empty());
}

TEST(MessageTests, BuildGeneratesUniqueIds) {
    const auto a = Message::build("alice", "bob", "", false, {}, {}, {});
    const auto b = Message::build("alice", "bob", "", false, {}, {}, {});
    EXPECT_NE(a.messageId, b.messageId);
}

TEST(MessageTests, BuildSetsTimestampCloseToNow) {
    using namespace std::chrono;
    const auto before = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    const auto msg = Message::build("alice", "bob", "", false, {}, {}, {});
    const auto after = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

    EXPECT_GE(msg.timestamp, before);
    EXPECT_LE(msg.timestamp, after);
}

TEST(MessageTests, BuildSetsDefaultStatusToSent) {
    const auto msg = Message::build("alice", "bob", "", false, {}, {}, {});
    EXPECT_EQ(msg.status, MessageStatus::Sent);
}

TEST(MessageTests, BuildSetsDeletedForAllFalse) {
    const auto msg = Message::build("alice", "bob", "", false, {}, {}, {});
    EXPECT_FALSE(msg.deletedForAll);
}

