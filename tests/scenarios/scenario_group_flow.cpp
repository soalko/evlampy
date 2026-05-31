#include "chat/ChatClient.h"
#include "test_utils/TestEnvironment.h"

#include <iostream>

int main() {
    ScopedTestHome home;
    ChatServer server;
    OpenSSLKeyFactory keyFactory;
    AES256GCMStrategy encryption;

    ChatClient alice("group_alice", server, keyFactory, encryption);
    ChatClient bob("group_bob", server, keyFactory, encryption);
    ChatClient charlie("group_charlie", server, keyFactory, encryption);

    if (!alice.registerOnServer("alice_pass") || !bob.registerOnServer("bob_pass") ||
        !charlie.registerOnServer("charlie_pass")) {
        std::cerr << "register failed\n";
        return 1;
    }
    if (!alice.login("alice_pass") || !bob.login("bob_pass") || !charlie.login("charlie_pass")) {
        std::cerr << "login failed\n";
        return 2;
    }

    if (!alice.sendMessage("group_bob", "hello group/bob")) {
        std::cerr << "send to bob failed\n";
        return 3;
    }
    if (!alice.sendMessage("group_charlie", "hello group/charlie")) {
        std::cerr << "send to charlie failed\n";
        return 4;
    }

    const auto convBob = server.getConversation("group_alice", "group_bob");
    const auto convCharlie = server.getConversation("group_alice", "group_charlie");
    if (convBob.size() != 1 || convCharlie.size() != 1) {
        std::cerr << "fanout validation failed\n";
        return 5;
    }

    std::cout << "scenario_group_flow: PASS\n";
    return 0;
}
