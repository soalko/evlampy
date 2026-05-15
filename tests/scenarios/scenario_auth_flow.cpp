#include "chat/ChatClient.h"
#include "test_utils/TestEnvironment.h"

#include <iostream>

int main() {
    ScopedTestHome home;
    ChatServer server;
    OpenSSLKeyFactory keyFactory;
    AES256GCMStrategy encryption;

    ChatClient alice("scenario_alice", server, keyFactory, encryption);

    if (!alice.registerOnServer("pass123", "alice@example.com")) {
        std::cerr << "register failed\n";
        return 1;
    }
    if (!alice.login("pass123")) {
        std::cerr << "login failed\n";
        return 2;
    }
    alice.logout();
    if (!alice.login("pass123")) {
        std::cerr << "relogin failed\n";
        return 3;
    }

    std::cout << "scenario_auth_flow: PASS\n";
    return 0;
}

