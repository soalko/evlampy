#include "chat/ChatClient.h"
#include "test_utils/TestEnvironment.h"

#include <iostream>

int main() {
    ScopedTestHome home;
    ChatServer server;
    OpenSSLKeyFactory keyFactory;
    AES256GCMStrategy encryption;

    ChatClient alice("dm_alice", server, keyFactory, encryption);
    ChatClient bob("dm_bob", server, keyFactory, encryption);

    if (!alice.registerOnServer("alice_pass") || !bob.registerOnServer("bob_pass")) {
        std::cerr << "register failed\n";
        return 1;
    }
    if (!alice.login("alice_pass") || !bob.login("bob_pass")) {
        std::cerr << "login failed\n";
        return 2;
    }
    if (!alice.sendMessage("dm_bob", "hello direct")) {
        std::cerr << "send failed\n";
        return 3;
    }

    const auto conv = server.getConversation("dm_alice", "dm_bob");
    if (conv.size() != 1 || conv[0].sender != "dm_alice" || conv[0].receiver != "dm_bob") {
        std::cerr << "conversation validation failed\n";
        return 4;
    }

    std::cout << "scenario_direct_message: PASS\n";
    return 0;
}
