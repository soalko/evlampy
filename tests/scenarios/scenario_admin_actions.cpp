#include "chat/ChatServer.h"
#include "test_utils/TestEnvironment.h"

#include <iostream>

int main() {
    ScopedTestHome home;
    ChatServer server;

    if (!server.registerUser("admin", "hash_admin", "pub_admin")) {
        std::cerr << "admin register failed\n";
        return 1;
    }
    if (!server.registerUser("user", "hash_user", "pub_user")) {
        std::cerr << "user register failed\n";
        return 2;
    }

    if (!server.blockUser("admin", "user")) {
        std::cerr << "block failed\n";
        return 3;
    }
    if (server.authenticate("user", "hash_user")) {
        std::cerr << "blocked user should not authenticate\n";
        return 4;
    }

    if (!server.unblockUser("admin", "user")) {
        std::cerr << "unblock failed\n";
        return 5;
    }

    if (!server.resetPasswordHash("admin", "user", "new_hash")) {
        std::cerr << "reset password failed\n";
        return 6;
    }
    if (!server.authenticate("user", "new_hash")) {
        std::cerr << "auth with new hash failed\n";
        return 7;
    }

    std::cout << "scenario_admin_actions: PASS\n";
    return 0;
}

