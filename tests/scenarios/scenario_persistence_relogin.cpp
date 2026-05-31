#include "chat/ChatClient.h"
#include "test_utils/TestEnvironment.h"

#include <iostream>

int main() {
    ScopedTestHome home;
    ChatServer server;
    OpenSSLKeyFactory keyFactory;
    AES256GCMStrategy encryption;

    {
        ChatClient user("persist_user", server, keyFactory, encryption);
        if (!user.registerOnServer("persist_pass")) {
            std::cerr << "register failed\n";
            return 1;
        }
        if (!user.login("persist_pass")) {
            std::cerr << "first login failed\n";
            return 2;
        }
        user.logout();
    }

    {
        ChatClient restored("persist_user", server, keyFactory, encryption);
        if (!restored.login("persist_pass")) {
            std::cerr << "login with restored client failed\n";
            return 3;
        }
    }

    std::cout << "scenario_persistence_relogin: PASS\n";
    return 0;
}
