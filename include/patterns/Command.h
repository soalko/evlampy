#pragma once

#include <string>

class ChatClient;

class Command {
public:
	virtual ~Command() = default;
	virtual bool execute() = 0;
};

class SendMessageCommand final : public Command {
public:
	SendMessageCommand(ChatClient& client, std::string toUser, std::string text);
	bool execute() override;

private:
	ChatClient& client_;
	std::string toUser_;
	std::string text_;
};
