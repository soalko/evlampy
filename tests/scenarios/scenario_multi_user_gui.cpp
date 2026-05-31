#include "test_utils/TestEnvironment.h"

#include "chat/ChatClient.h"
#include "chat/ChatServer.h"
#include "encryption/AES256GCMStrategy.h"
#include "encryption/OpenSSLKeyFactory.h"
#include "patterns/Observer.h"

#include <iostream>
#include <string>

class SimpleObserver : public Observer<DecryptedMessageEvent> {
public:
	void onNotify(const DecryptedMessageEvent& event) override {
		std::cout << "Event: " << event.fromUser << " -> " << event.text << std::endl;
	}
};

int main() {
	ScopedTestHome home;

	ChatServer server;
	OpenSSLKeyFactory keyFactory;
	AES256GCMStrategy encryption;

	// Scenario: Two users register and login in sequence in the same process
	// This simulates what happens when you open GUI, register alex, then vova

	// 1. Register and login alex
	{
		ChatClient alice("alice", server, keyFactory, encryption);
		if (!alice.registerOnServer("alice_password")) {
			std::cerr << "Failed to register alice" << std::endl;
			return 1;
		}
		if (!alice.login("alice_password")) {
			std::cerr << "Failed to login alice" << std::endl;
			return 1;
		}
		alice.logout();
	}

	// 2. Register and login bob in a NEW ChatClient instance
	// This simulates opening GUI again and logging in as different user
	{
		ChatClient bob("bob", server, keyFactory, encryption);
		if (!bob.registerOnServer("bob_password")) {
			std::cerr << "Failed to register bob" << std::endl;
			return 1;
		}
		if (!bob.login("bob_password")) {
			std::cerr << "Failed to login bob" << std::endl;
			return 1;
		}
		bob.logout();
	}

	// 3. Login alice again (keys loaded from disk)
	{
		ChatClient alice("alice", server, keyFactory, encryption);
		if (!alice.login("alice_password")) {
			std::cerr << "Failed to login alice second time" << std::endl;
			return 1;
		}

		// 4. Login bob
		ChatClient bob("bob", server, keyFactory, encryption);
		if (!bob.login("bob_password")) {
			std::cerr << "Failed to login bob again" << std::endl;
			return 1;
		}

		// 5. alice sends message to bob
		SimpleObserver bobObserver;
		bob.attach(&bobObserver);

		if (!alice.sendMessage("bob", "Hello Bob! This is encrypted!")) {
			std::cerr << "Failed to send message from alice to bob" << std::endl;
			return 1;
		}

		std::cout << "Message sent successfully from alice to bob" << std::endl;

		// 6. Check bob can read message
		auto messages = bob.getConversation("alice");
		if (messages.empty()) {
			std::cerr << "Failed to retrieve message on bob's side" << std::endl;
			return 1;
		}
		if (messages[0].text != "Hello Bob! This is encrypted!") {
			std::cerr << "Message text mismatch: " << messages[0].text << std::endl;
			return 1;
		}

		std::cout << "Message successfully decrypted: " << messages[0].text << std::endl;
		std::cout << "Scenario: Multi-user GUI login/register - PASSED" << std::endl;

		alice.logout();
		bob.logout();
	}

	return 0;
}

