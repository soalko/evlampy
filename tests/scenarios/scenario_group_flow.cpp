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

    if (!alice.registerOnServer("alice_pass") || !bob.registerOnServer("bob_pass")) {
        std::cerr << "register failed\n";
        return 1;
    }
    if (!alice.login("alice_pass") || !bob.login("bob_pass")) {
        std::cerr << "login failed\n";
        return 2;
    }

    const auto groupId = alice.createGroup("team", {"group_bob"});
    if (!groupId.has_value()) {
        std::cerr << "group create failed\n";
        return 3;
    }
    if (!alice.sendGroupMessage(*groupId, "hello group")) {
        std::cerr << "group send failed\n";
        return 4;
    }

    const auto groupConv = bob.getGroupConversation(*groupId);
    if (groupConv.size() != 1 || groupConv.front().text != "hello group") {
        std::cerr << "group validation failed\n";
        return 5;
    }

    std::cout << "scenario_group_flow: PASS\n";
    return 0;
}

