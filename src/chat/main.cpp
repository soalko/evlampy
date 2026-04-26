#include "chat/ChatClient.h"
#include "patterns/Command.h"

#include <iostream>

class ConsoleMessageObserver final : public Observer<DecryptedMessageEvent> {
public:
	explicit ConsoleMessageObserver(std::string owner)
		: owner_(std::move(owner)) {
	}

	void onNotify(const DecryptedMessageEvent& event) override {
		std::cout << "[" << owner_ << "] new message from " << event.fromUser << ": " << event.text << '\n';
	}

private:
	std::string owner_;
};

int main() {
	try {
		ChatServer server;
		OpenSSLKeyFactory keyFactory;
		AES256GCMStrategy encryption;

		ChatClient alice("alice", server, keyFactory, encryption);
		ChatClient bob("bob", server, keyFactory, encryption);

		ConsoleMessageObserver aliceObserver("alice");
		ConsoleMessageObserver bobObserver("bob");
		alice.attach(&aliceObserver);
		bob.attach(&bobObserver);

		if (!alice.registerOnServer("alice_pass") || !bob.registerOnServer("bob_pass")) {
			std::cerr << "Registration failed\n";
			return 1;
		}
		if (!alice.login("alice_pass") || !bob.login("bob_pass")) {
			std::cerr << "Login failed\n";
			return 1;
		}

		SendMessageCommand send(alice, "bob", "Privet, Bob! Eto Evlampy demo s RSA + AES-GCM.");
		if (!send.execute()) {
			std::cerr << "Send failed\n";
			return 1;
		}

		alice.logout();
		bob.logout();
		return 0;
	} catch (const std::exception& ex) {
		std::cerr << "Fatal error: " << ex.what() << '\n';
		return 2;
	}
}

