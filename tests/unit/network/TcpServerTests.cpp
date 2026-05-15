#include "network/TcpServer.h"

#include <gtest/gtest.h>

#include <algorithm>

TEST(TcpServerTests, ConnectClientSucceeds) {
    TcpServer server;
    EXPECT_TRUE(server.connectClient("alice", [](const std::string&, const std::string&) {}));
}

TEST(TcpServerTests, ConnectClientDuplicateFails) {
    TcpServer server;
    EXPECT_TRUE(server.connectClient("alice", [](const std::string&, const std::string&) {}));
    EXPECT_FALSE(server.connectClient("alice", [](const std::string&, const std::string&) {}));
}

TEST(TcpServerTests, DisconnectRemovesFromOnline) {
    TcpServer server;
    server.connectClient("alice", [](const std::string&, const std::string&) {});
    server.disconnectClient("alice");
    EXPECT_FALSE(server.isOnline("alice"));
}

TEST(TcpServerTests, SendToOnlineClientDeliversPayload) {
    TcpServer server;
    std::string from;
    std::string payload;
    server.connectClient("bob", [&](const std::string& f, const std::string& p) {
        from = f;
        payload = p;
    });

    EXPECT_TRUE(server.sendTo("alice", "bob", "hello"));
    EXPECT_EQ(from, "alice");
    EXPECT_EQ(payload, "hello");
}

TEST(TcpServerTests, SendToOfflineClientFails) {
    TcpServer server;
    EXPECT_FALSE(server.sendTo("alice", "bob", "hello"));
}

TEST(TcpServerTests, ListOnlineUsersContainsAllConnected) {
    TcpServer server;
    server.connectClient("alice", [](const std::string&, const std::string&) {});
    server.connectClient("bob", [](const std::string&, const std::string&) {});

    const auto users = server.listOnlineUsers();
    EXPECT_EQ(users.size(), 2u);
    EXPECT_TRUE(std::find(users.begin(), users.end(), "alice") != users.end());
    EXPECT_TRUE(std::find(users.begin(), users.end(), "bob") != users.end());
}

TEST(TcpServerTests, SendAfterDisconnectFails) {
    TcpServer server;
    server.connectClient("bob", [](const std::string&, const std::string&) {});
    server.disconnectClient("bob");

    EXPECT_FALSE(server.sendTo("alice", "bob", "hello"));
}

