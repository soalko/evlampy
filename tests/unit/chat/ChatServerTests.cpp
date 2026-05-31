#include "chat/ChatServer.h"
#include "chat/Message.h"

#include "test_utils/TestEnvironment.h"

#include <gtest/gtest.h>

class ChatServerTests : public ::testing::Test {
protected:
    ScopedTestHome home;
    ChatServer server;
};

TEST_F(ChatServerTests, RegisterUserSuccess) {
    EXPECT_TRUE(server.registerUser("alice", "hash1", "pub1"));
}

TEST_F(ChatServerTests, RegisterDuplicateFails) {
    ASSERT_TRUE(server.registerUser("alice", "hash1", "pub1"));
    EXPECT_FALSE(server.registerUser("alice", "hash2", "pub2"));
}

TEST_F(ChatServerTests, AuthenticateSuccessAndFail) {
    ASSERT_TRUE(server.registerUser("alice", "hash1", "pub1"));
    EXPECT_TRUE(server.authenticate("alice", "hash1"));
    EXPECT_FALSE(server.authenticate("alice", "bad"));
}

TEST_F(ChatServerTests, PublicKeyLookupWorks) {
    ASSERT_TRUE(server.registerUser("alice", "hash1", "pubkey-alice"));
    EXPECT_EQ(server.getPublicKey("alice"), "pubkey-alice");
    EXPECT_TRUE(server.getPublicKey("unknown").empty());
}

TEST_F(ChatServerTests, StoreAndReadConversation) {
    const Message msg = Message::build("alice", "bob", {1}, {2, 3});
    server.storeMessage(msg);

    const auto conv = server.getConversation("alice", "bob");
    ASSERT_EQ(conv.size(), 1u);
    EXPECT_EQ(conv[0].messageId, msg.messageId);
}

TEST_F(ChatServerTests, ConversationLookupSymmetricByParticipants) {
    server.storeMessage(Message::build("alice", "bob", {1}, {2}));

    const auto convAB = server.getConversation("alice", "bob");
    const auto convBA = server.getConversation("bob", "alice");
    ASSERT_EQ(convAB.size(), 1u);
    ASSERT_EQ(convBA.size(), 1u);
    EXPECT_EQ(convAB[0].messageId, convBA[0].messageId);
}

TEST_F(ChatServerTests, DeleteMessageForAllOnlyBySender) {
    const Message msg = Message::build("alice", "bob", {1}, {2});
    server.storeMessage(msg);

    EXPECT_FALSE(server.deleteMessageForAll("bob", msg.messageId));
    EXPECT_TRUE(server.deleteMessageForAll("alice", msg.messageId));

    const auto conv = server.getConversation("alice", "bob");
    EXPECT_TRUE(conv.empty());
}

TEST_F(ChatServerTests, DeleteUnknownMessageFails) {
    EXPECT_FALSE(server.deleteMessageForAll("alice", "no-such-id"));
}
