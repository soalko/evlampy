#include "chat/ChatClient.h"

#include "test_utils/TestEnvironment.h"

#include <gtest/gtest.h>

namespace {
class CaptureObserver final : public Observer<DecryptedMessageEvent> {
public:
    void onNotify(const DecryptedMessageEvent& event) override {
        events.push_back(event);
    }

    std::vector<DecryptedMessageEvent> events;
};
}

class ChatClientTests : public ::testing::Test {
protected:
    ScopedTestHome home;
    ChatServer server;
    OpenSSLKeyFactory keyFactory;
    AES256GCMStrategy encryption;
};

TEST_F(ChatClientTests, RegisterAndLoginSuccess) {
    ChatClient alice("alice", server, keyFactory, encryption);

    ASSERT_TRUE(alice.registerOnServer("alice_pass"));
    EXPECT_TRUE(alice.login("alice_pass"));
    EXPECT_TRUE(alice.isLoggedIn());
}

TEST_F(ChatClientTests, LoginFailsWithWrongPassword) {
    ChatClient alice("alice", server, keyFactory, encryption);

    ASSERT_TRUE(alice.registerOnServer("alice_pass"));
    EXPECT_FALSE(alice.login("wrong"));
}

TEST_F(ChatClientTests, LoginFailsForUnknownUser) {
    ChatClient ghost("ghost", server, keyFactory, encryption);
    EXPECT_FALSE(ghost.login("pass"));
}

TEST_F(ChatClientTests, ReloginAfterLogoutSucceeds) {
    ChatClient alice("alice", server, keyFactory, encryption);
    ASSERT_TRUE(alice.registerOnServer("alice_pass"));
    ASSERT_TRUE(alice.login("alice_pass"));
    alice.logout();

    EXPECT_TRUE(alice.login("alice_pass"));
}

TEST_F(ChatClientTests, SendMessageStoresConversationOnServer) {
    ChatClient alice("alice", server, keyFactory, encryption);
    ChatClient bob("bob", server, keyFactory, encryption);

    ASSERT_TRUE(alice.registerOnServer("alice_pass"));
    ASSERT_TRUE(bob.registerOnServer("bob_pass"));
    ASSERT_TRUE(alice.login("alice_pass"));
    ASSERT_TRUE(bob.login("bob_pass"));

    ASSERT_TRUE(alice.sendMessage("bob", "hello bob"));

    const auto conv = server.getConversation("alice", "bob");
    ASSERT_EQ(conv.size(), 1u);
    EXPECT_EQ(conv[0].sender, "alice");
    EXPECT_EQ(conv[0].receiver, "bob");
}

TEST_F(ChatClientTests, ObserverGetsDecryptedIncomingMessage) {
    ChatClient alice("alice", server, keyFactory, encryption);
    ChatClient bob("bob", server, keyFactory, encryption);
    CaptureObserver observer;

    bob.attach(&observer);

    ASSERT_TRUE(alice.registerOnServer("alice_pass"));
    ASSERT_TRUE(bob.registerOnServer("bob_pass"));
    ASSERT_TRUE(alice.login("alice_pass"));
    ASSERT_TRUE(bob.login("bob_pass"));
    ASSERT_TRUE(alice.sendMessage("bob", "plaintext"));

    ASSERT_EQ(observer.events.size(), 1u);
    EXPECT_EQ(observer.events.front().fromUser, "alice");
    EXPECT_EQ(observer.events.front().text, "plaintext");
}

TEST_F(ChatClientTests, SendMessageToUnknownUserFails) {
    ChatClient alice("alice", server, keyFactory, encryption);

    ASSERT_TRUE(alice.registerOnServer("alice_pass"));
    ASSERT_TRUE(alice.login("alice_pass"));
    EXPECT_FALSE(alice.sendMessage("nobody", "test"));
}

TEST_F(ChatClientTests, DeleteMessageForAllRemovesMessageFromHistory) {
    ChatClient alice("alice", server, keyFactory, encryption);
    ChatClient bob("bob", server, keyFactory, encryption);

    ASSERT_TRUE(alice.registerOnServer("alice_pass"));
    ASSERT_TRUE(bob.registerOnServer("bob_pass"));
    ASSERT_TRUE(alice.login("alice_pass"));
    ASSERT_TRUE(bob.login("bob_pass"));
    ASSERT_TRUE(alice.sendMessage("bob", "temporary"));

    const auto conv = server.getConversation("alice", "bob");
    ASSERT_EQ(conv.size(), 1u);
    ASSERT_TRUE(alice.deleteMessageForAll(conv.front().messageId));

    EXPECT_TRUE(server.getConversation("alice", "bob").empty());
}

TEST_F(ChatClientTests, DeleteMessageFailsWhenNotLoggedIn) {
    ChatClient alice("alice", server, keyFactory, encryption);
    ASSERT_TRUE(alice.registerOnServer("alice_pass"));
    EXPECT_FALSE(alice.deleteMessageForAll("id"));
}
