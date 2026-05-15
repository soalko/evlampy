#include "chat/ChatServer.h"
#include "chat/Message.h"

#include "test_utils/TestEnvironment.h"

#include <gtest/gtest.h>

#include <algorithm>

class ChatServerTests : public ::testing::Test {
protected:
    ScopedTestHome home;
    ChatServer server;
};

TEST_F(ChatServerTests, RegisterUserSuccess) {
    EXPECT_TRUE(server.registerUser("alice", "hash1", "pub1", "a@a.com"));
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

TEST_F(ChatServerTests, BlockAndUnblockFlow) {
    ASSERT_TRUE(server.registerUser("admin", "h1", "p1"));
    ASSERT_TRUE(server.registerUser("bob", "h2", "p2"));

    ASSERT_TRUE(server.blockUser("admin", "bob"));
    EXPECT_FALSE(server.authenticate("bob", "h2"));

    ASSERT_TRUE(server.unblockUser("admin", "bob"));
    EXPECT_TRUE(server.authenticate("bob", "h2"));
}

TEST_F(ChatServerTests, ResetPasswordHashByAdmin) {
    ASSERT_TRUE(server.registerUser("admin", "h1", "p1"));
    ASSERT_TRUE(server.registerUser("bob", "old", "p2"));

    ASSERT_TRUE(server.resetPasswordHash("admin", "bob", "new"));
    EXPECT_FALSE(server.authenticate("bob", "old"));
    EXPECT_TRUE(server.authenticate("bob", "new"));
}

TEST_F(ChatServerTests, ContactsAddRemoveList) {
    ASSERT_TRUE(server.registerUser("alice", "h1", "p1"));
    ASSERT_TRUE(server.registerUser("bob", "h2", "p2"));

    EXPECT_TRUE(server.addContact("alice", "bob"));
    auto contacts = server.getContacts("alice");
    EXPECT_EQ(contacts.size(), 1u);
    EXPECT_EQ(contacts[0], "bob");

    EXPECT_TRUE(server.removeContact("alice", "bob"));
    EXPECT_TRUE(server.getContacts("alice").empty());
}

TEST_F(ChatServerTests, StoreAndReadConversation) {
    Message msg = Message::build("alice", "bob", "", false, {1}, {2}, {3});
    server.storeMessage(msg);

    const auto conv = server.getConversation("alice", "bob");
    ASSERT_EQ(conv.size(), 1u);
    EXPECT_EQ(conv[0].messageId, msg.messageId);
}

TEST_F(ChatServerTests, MarkDeliveredAndReadFlow) {
    Message msg = Message::build("alice", "bob", "", false, {1}, {2}, {3});
    server.storeMessage(msg);

    EXPECT_TRUE(server.markDelivered(msg.messageId, "bob"));
    EXPECT_TRUE(server.markRead(msg.messageId, "bob"));

    const auto conv = server.getConversation("alice", "bob");
    ASSERT_EQ(conv.size(), 1u);
    EXPECT_EQ(conv[0].status, MessageStatus::Read);
}

TEST_F(ChatServerTests, DeleteMessageForAllOnlyBySender) {
    Message msg = Message::build("alice", "bob", "", false, {1}, {2}, {3});
    server.storeMessage(msg);

    EXPECT_FALSE(server.deleteMessageForAll("bob", msg.messageId));
    EXPECT_TRUE(server.deleteMessageForAll("alice", msg.messageId));

    const auto conv = server.getConversation("alice", "bob");
    ASSERT_EQ(conv.size(), 1u);
    EXPECT_TRUE(conv[0].deletedForAll);
}

TEST_F(ChatServerTests, GroupCreateAndSendConversation) {
    ASSERT_TRUE(server.registerUser("alice", "h1", "p1"));
    ASSERT_TRUE(server.registerUser("bob", "h2", "p2"));
    const auto groupId = server.createGroup("alice", "team", {"bob"});
    ASSERT_TRUE(groupId.has_value());

    Message msg = Message::build("alice", "bob", *groupId, true, {1}, {2}, {3});
    server.storeMessage(msg);

    const auto grp = server.getGroupConversation(*groupId);
    ASSERT_EQ(grp.size(), 1u);
    EXPECT_TRUE(grp.front().isGroupMessage);
}

TEST_F(ChatServerTests, AuditLogHasEntriesAfterOperations) {
    ASSERT_TRUE(server.registerUser("alice", "h1", "p1"));
    ASSERT_TRUE(server.registerUser("bob", "h2", "p2"));
    ASSERT_TRUE(server.addContact("alice", "bob"));

    const auto audit = server.getAuditLog();
    EXPECT_GE(audit.size(), 3u);
}

