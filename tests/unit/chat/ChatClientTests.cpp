#include "chat/ChatClient.h"

#include "test_utils/TestEnvironment.h"

#include <gtest/gtest.h>

class ChatClientTests : public ::testing::Test {
protected:
    ScopedTestHome home;
    ChatServer server;
    OpenSSLKeyFactory keyFactory;
    AES256GCMStrategy encryption;
};

TEST_F(ChatClientTests, RegisterAndLoginSuccess) {
    ChatClient alice("alice", server, keyFactory, encryption);

    ASSERT_TRUE(alice.registerOnServer("alice_pass", "a@a.com"));
    EXPECT_TRUE(alice.login("alice_pass"));
    EXPECT_TRUE(alice.isLoggedIn());
}

TEST_F(ChatClientTests, LoginFailsWithWrongPassword) {
    ChatClient alice("alice", server, keyFactory, encryption);

    ASSERT_TRUE(alice.registerOnServer("alice_pass", "a@a.com"));
    EXPECT_FALSE(alice.login("wrong"));
}

TEST_F(ChatClientTests, LoginFailsForUnknownUser) {
    ChatClient ghost("ghost", server, keyFactory, encryption);
    EXPECT_FALSE(ghost.login("pass"));
}

TEST_F(ChatClientTests, ReloginAfterLogoutSucceeds) {
    ChatClient alice("alice", server, keyFactory, encryption);
    ASSERT_TRUE(alice.registerOnServer("alice_pass", "a@a.com"));
    ASSERT_TRUE(alice.login("alice_pass"));
    alice.logout();

    EXPECT_TRUE(alice.login("alice_pass"));
}

TEST_F(ChatClientTests, LoginNewClientInstanceWithPersistedKeysSucceeds) {
    {
        ChatClient alice("alice", server, keyFactory, encryption);
        ASSERT_TRUE(alice.registerOnServer("alice_pass", "a@a.com"));
        ASSERT_TRUE(alice.login("alice_pass"));
        alice.logout();
    }

    ChatClient aliceNew("alice", server, keyFactory, encryption);
    EXPECT_TRUE(aliceNew.login("alice_pass"));
}

TEST_F(ChatClientTests, SendMessageAndReadConversation) {
    ChatClient alice("alice", server, keyFactory, encryption);
    ChatClient bob("bob", server, keyFactory, encryption);

    ASSERT_TRUE(alice.registerOnServer("alice_pass", "a@a.com"));
    ASSERT_TRUE(bob.registerOnServer("bob_pass", "b@b.com"));
    ASSERT_TRUE(alice.login("alice_pass"));
    ASSERT_TRUE(bob.login("bob_pass"));

    ASSERT_TRUE(alice.sendMessage("bob", "hello bob"));

    const auto bobView = bob.getConversation("alice");
    ASSERT_EQ(bobView.size(), 1u);
    EXPECT_EQ(bobView[0].text, "hello bob");
}

TEST_F(ChatClientTests, SendMessageToUnknownUserFails) {
    ChatClient alice("alice", server, keyFactory, encryption);

    ASSERT_TRUE(alice.registerOnServer("alice_pass", "a@a.com"));
    ASSERT_TRUE(alice.login("alice_pass"));
    EXPECT_FALSE(alice.sendMessage("nobody", "test"));
}

TEST_F(ChatClientTests, ContactsAddAndRemoveFlow) {
    ChatClient alice("alice", server, keyFactory, encryption);
    ChatClient bob("bob", server, keyFactory, encryption);

    ASSERT_TRUE(alice.registerOnServer("alice_pass", "a@a.com"));
    ASSERT_TRUE(bob.registerOnServer("bob_pass", "b@b.com"));
    ASSERT_TRUE(alice.login("alice_pass"));

    ASSERT_TRUE(alice.addContact("bob"));
    auto contacts = alice.listContacts();
    ASSERT_EQ(contacts.size(), 1u);
    EXPECT_EQ(contacts[0], "bob");

    ASSERT_TRUE(alice.removeContact("bob"));
    EXPECT_TRUE(alice.listContacts().empty());
}

TEST_F(ChatClientTests, CreateGroupAndSendGroupMessage) {
    ChatClient alice("alice", server, keyFactory, encryption);
    ChatClient bob("bob", server, keyFactory, encryption);

    ASSERT_TRUE(alice.registerOnServer("alice_pass", "a@a.com"));
    ASSERT_TRUE(bob.registerOnServer("bob_pass", "b@b.com"));
    ASSERT_TRUE(alice.login("alice_pass"));
    ASSERT_TRUE(bob.login("bob_pass"));

    const auto groupId = alice.createGroup("team", {"bob"});
    ASSERT_TRUE(groupId.has_value());
    ASSERT_TRUE(alice.sendGroupMessage(*groupId, "hello team"));

    const auto bobGroup = bob.getGroupConversation(*groupId);
    ASSERT_EQ(bobGroup.size(), 1u);
    EXPECT_EQ(bobGroup.front().text, "hello team");
}

TEST_F(ChatClientTests, DeleteMessageForAllFlow) {
    ChatClient alice("alice", server, keyFactory, encryption);
    ChatClient bob("bob", server, keyFactory, encryption);

    ASSERT_TRUE(alice.registerOnServer("alice_pass", "a@a.com"));
    ASSERT_TRUE(bob.registerOnServer("bob_pass", "b@b.com"));
    ASSERT_TRUE(alice.login("alice_pass"));
    ASSERT_TRUE(bob.login("bob_pass"));
    ASSERT_TRUE(alice.sendMessage("bob", "temporary"));

    auto conv = alice.getConversation("bob");
    ASSERT_EQ(conv.size(), 1u);
    ASSERT_TRUE(alice.deleteMessageForAll(conv.front().messageId));

    auto bobConv = bob.getConversation("alice");
    ASSERT_EQ(bobConv.size(), 1u);
    EXPECT_EQ(bobConv.front().text, "[message deleted]");
}

TEST_F(ChatClientTests, ListOnlineUsersReflectsCurrentState) {
    ChatClient alice("alice", server, keyFactory, encryption);
    ChatClient bob("bob", server, keyFactory, encryption);

    ASSERT_TRUE(alice.registerOnServer("alice_pass", "a@a.com"));
    ASSERT_TRUE(bob.registerOnServer("bob_pass", "b@b.com"));
    ASSERT_TRUE(alice.login("alice_pass"));
    ASSERT_TRUE(bob.login("bob_pass"));

    const auto online = alice.listOnlineUsers();
    EXPECT_EQ(online.size(), 2u);
}

