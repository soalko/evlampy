#include "patterns/Command.h"

#include "chat/ChatClient.h"

SendMessageCommand::SendMessageCommand(ChatClient& client, std::string toUser, std::string text)
	: client_(client),
	  toUser_(std::move(toUser)),
	  text_(std::move(text)) {
}

bool SendMessageCommand::execute() {
	return client_.sendMessage(toUser_, text_);
}

