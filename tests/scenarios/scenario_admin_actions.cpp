#include "chat/ChatServer.h"
#include "chat/Message.h"
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

    if (!server.authenticate("admin", "hash_admin")) {
        std::cerr << "admin auth failed\n";
        return 3;
    }
    if (server.getPublicKey("user") != "pub_user") {
        std::cerr << "public key lookup failed\n";
        return 4;
    }

    const auto msg = Message::build("admin", "user", {1}, {2, 3});
    server.storeMessage(msg);
    if (server.getConversation("admin", "user").size() != 1) {
        std::cerr << "store message failed\n";
        return 5;
    }

    if (!server.deleteMessageForAll("admin", msg.messageId)) {
        std::cerr << "delete message failed\n";
        return 6;
    }
    if (!server.getConversation("admin", "user").empty()) {
        std::cerr << "message not deleted\n";
        return 7;
    }

    std::cout << "scenario_admin_actions: PASS\n";
    return 0;
}
