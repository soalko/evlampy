#include "network/TcpClient.h"

#include <gtest/gtest.h>

TEST(TcpClientTests, ConnectSetsStateAndUsername) {
    TcpServer server;
    TcpClient client(server);

    EXPECT_TRUE(client.connect("alice"));
    EXPECT_TRUE(client.isConnected());
    EXPECT_EQ(client.username(), "alice");
}

TEST(TcpClientTests, ConnectTwiceFails) {
    TcpServer server;
    TcpClient client(server);

    EXPECT_TRUE(client.connect("alice"));
    EXPECT_FALSE(client.connect("alice"));
}

TEST(TcpClientTests, DisconnectClearsState) {
    TcpServer server;
    TcpClient client(server);

    ASSERT_TRUE(client.connect("alice"));
    client.disconnect();

    EXPECT_FALSE(client.isConnected());
    EXPECT_TRUE(client.username().empty());
}

TEST(TcpClientTests, SendBeforeConnectFails) {
    TcpServer server;
    TcpClient alice(server);

    EXPECT_FALSE(alice.send("bob", "payload"));
}

TEST(TcpClientTests, SendAfterConnectDelivers) {
    TcpServer server;
    TcpClient alice(server);
    TcpClient bob(server);
    std::string received;

    bob.setOnMessage([&](const std::string&, const std::string& payload) { received = payload; });
    ASSERT_TRUE(alice.connect("alice"));
    ASSERT_TRUE(bob.connect("bob"));

    EXPECT_TRUE(alice.send("bob", "hello-bob"));
    EXPECT_EQ(received, "hello-bob");
}

TEST(TcpClientTests, ReceiveCallbackInvokedWithSender) {
    TcpServer server;
    TcpClient alice(server);
    TcpClient bob(server);
    std::string from;

    bob.setOnMessage([&](const std::string& f, const std::string&) { from = f; });
    ASSERT_TRUE(alice.connect("alice"));
    ASSERT_TRUE(bob.connect("bob"));

    ASSERT_TRUE(alice.send("bob", "payload"));
    EXPECT_EQ(from, "alice");
}

TEST(TcpClientTests, SendAfterDisconnectFails) {
    TcpServer server;
    TcpClient alice(server);
    TcpClient bob(server);
    ASSERT_TRUE(alice.connect("alice"));
    ASSERT_TRUE(bob.connect("bob"));
    alice.disconnect();

    EXPECT_FALSE(alice.send("bob", "payload"));
}

