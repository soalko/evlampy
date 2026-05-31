#include "chat/ChatClient.h"
#include "patterns/Command.h"

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <stdexcept>

namespace {
struct DemoConfig {
	std::string aliceName = "alice";
	std::string bobName = "bob";
	std::string alicePassword = "alice_pass";
	std::string bobPassword = "bob_pass";
	std::string message = "Privet, Bob! Eto Evlampy demo s RSA + AES-GCM.";
};

void printUsage() {
	std::cout << "Usage: target_exec [--alice-name NAME] [--bob-name NAME] [--alice-pass PASS] [--bob-pass PASS] [--message TEXT]\n";
}

bool applyOption(int& index, int argc, char* argv[], std::string_view option, std::string& destination) {
	const std::string current = argv[index];
	const std::string prefix = std::string(option) + "=";
	if (current == option) {
		if (index + 1 >= argc) {
			throw std::runtime_error(std::string(option) + " requires a value");
		}
		destination = argv[++index];
		return true;
	}
	if (current.rfind(prefix, 0) == 0) {
		destination = current.substr(prefix.size());
		return true;
	}
	return false;
}

DemoConfig parseArguments(int argc, char* argv[]) {
	DemoConfig config;
	for (int i = 1; i < argc; ++i) {
		const std::string arg = argv[i];
		if (arg == "-h" || arg == "--help") {
			printUsage();
			std::exit(0);
		}
		if (applyOption(i, argc, argv, "--alice-name", config.aliceName)) continue;
		if (applyOption(i, argc, argv, "--bob-name", config.bobName)) continue;
		if (applyOption(i, argc, argv, "--alice-pass", config.alicePassword)) continue;
		if (applyOption(i, argc, argv, "--bob-pass", config.bobPassword)) continue;
		if (applyOption(i, argc, argv, "--message", config.message)) continue;
		std::cerr << "Warning: ignoring unknown argument '" << arg << "'\n";
	}
	return config;
}
} // namespace

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

int main(int argc, char* argv[]) {
	try {
		const DemoConfig config = parseArguments(argc, argv);
		std::cout.setf(std::ios::unitbuf);
		ChatServer server;
		OpenSSLKeyFactory keyFactory;
		AES256GCMStrategy encryption;

		ChatClient alice(config.aliceName, server, keyFactory, encryption);
		ChatClient bob(config.bobName, server, keyFactory, encryption);

		ConsoleMessageObserver aliceObserver(config.aliceName);
		ConsoleMessageObserver bobObserver(config.bobName);
		alice.attach(&aliceObserver);
		bob.attach(&bobObserver);

		if (!alice.registerOnServer(config.alicePassword) || !bob.registerOnServer(config.bobPassword)) {
			std::cerr << "Registration failed\n";
			return 1;
		}
		if (!alice.login(config.alicePassword) || !bob.login(config.bobPassword)) {
			std::cerr << "Login failed\n";
			return 1;
		}

		SendMessageCommand send(alice, config.bobName, config.message);
		if (!send.execute()) {
			std::cerr << "Send failed\n";
			return 1;
		}

		std::cout << config.bobName << " conversation with " << config.aliceName << ":\n";
		for (const auto& message : bob.getConversation(config.aliceName)) {
			std::cout << "- " << message.text << '\n';
		}

		alice.logout();
		bob.logout();
		return 0;
	} catch (const std::exception& ex) {
		std::cerr << "Fatal error: " << ex.what() << '\n';
		return 2;
	}
}

