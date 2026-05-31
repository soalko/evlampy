#include "chat/ChatClient.h"
#include "test_utils/TestEnvironment.h"

#include <iostream>

int main() {
    ScopedTestHome home;
    ChatServer server;
    OpenSSLKeyFactory keyFactory;
    AES256GCMStrategy encryption;

    ChatClient alice("del_alice", server, keyFactory, encryption);
    ChatClient bob("del_bob", server, keyFactory, encryption);

    if (!alice.registerOnServer("alice_pass") || !bob.registerOnServer("bob_pass")) {
        std::cerr << "register failed\n";
        return 1;
    }
    if (!alice.login("alice_pass") || !bob.login("bob_pass")) {
        std::cerr << "login failed\n";
        return 2;
    }
    if (!alice.sendMessage("del_bob", "ephemeral")) {
        std::cerr << "send failed\n";
        return 3;
    }

    const auto beforeDelete = server.getConversation("del_alice", "del_bob");
    if (beforeDelete.empty()) {
        std::cerr << "no message in history\n";
        return 4;
    }

    if (!alice.deleteMessageForAll(beforeDelete.front().messageId)) {
        std::cerr << "delete for all failed\n";
        return 5;
    }

    const auto afterDelete = server.getConversation("del_alice", "del_bob");
    if (!afterDelete.empty()) {
        std::cerr << "delete propagation failed\n";
        return 6;
    }

    std::cout << "scenario_delete_for_all: PASS\n";
    return 0;
}
