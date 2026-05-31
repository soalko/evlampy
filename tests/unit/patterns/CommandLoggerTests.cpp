#include "chat/ChatClient.h"
#include "patterns/Command.h"
#include "patterns/Logger.h"

#include "test_utils/TestEnvironment.h"

#include <gtest/gtest.h>

class CommandLoggerTests : public ::testing::Test {
protected:
    ScopedTestHome home;
    ChatServer server;
    OpenSSLKeyFactory keyFactory;
    AES256GCMStrategy encryption;
};

TEST_F(CommandLoggerTests, SendMessageCommandExecuteSuccess) {
    ChatClient alice("alice", server, keyFactory, encryption);
    ChatClient bob("bob", server, keyFactory, encryption);

    ASSERT_TRUE(alice.registerOnServer("alice_pass"));
    ASSERT_TRUE(bob.registerOnServer("bob_pass"));
    ASSERT_TRUE(alice.login("alice_pass"));
    ASSERT_TRUE(bob.login("bob_pass"));

    SendMessageCommand cmd(alice, "bob", "hello");
    EXPECT_TRUE(cmd.execute());
}

TEST_F(CommandLoggerTests, SendMessageCommandFailsWhenNotLoggedIn) {
    ChatClient alice("alice", server, keyFactory, encryption);
    ChatClient bob("bob", server, keyFactory, encryption);
    ASSERT_TRUE(alice.registerOnServer("alice_pass"));
    ASSERT_TRUE(bob.registerOnServer("bob_pass"));

    SendMessageCommand cmd(alice, "bob", "hello");
    EXPECT_FALSE(cmd.execute());
}

TEST_F(CommandLoggerTests, SendMessageCommandFailsForUnknownReceiver) {
    ChatClient alice("alice", server, keyFactory, encryption);
    ASSERT_TRUE(alice.registerOnServer("alice_pass"));
    ASSERT_TRUE(alice.login("alice_pass"));

    SendMessageCommand cmd(alice, "ghost", "hello");
    EXPECT_FALSE(cmd.execute());
}

TEST(CommandLoggerStandaloneTests, LoggerSingletonReturnsSameInstance) {
    Logger& a = Logger::instance();
    Logger& b = Logger::instance();
    EXPECT_EQ(&a, &b);
}

TEST(CommandLoggerStandaloneTests, LoggerInfoDoesNotThrow) {
    EXPECT_NO_THROW(Logger::instance().info("test info"));
}

TEST(CommandLoggerStandaloneTests, LoggerErrorDoesNotThrow) {
    EXPECT_NO_THROW(Logger::instance().error("test error"));
}

TEST(CommandLoggerStandaloneTests, LoggerSupportsMultipleCalls) {
    EXPECT_NO_THROW(Logger::instance().info("line1"));
    EXPECT_NO_THROW(Logger::instance().info("line2"));
    EXPECT_NO_THROW(Logger::instance().error("line3"));
}
