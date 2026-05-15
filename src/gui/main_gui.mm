#import <Cocoa/Cocoa.h>

#include "chat/ChatClient.h"

#include <cctype>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

class GuiObserver final : public Observer<DecryptedMessageEvent> {
public:
	explicit GuiObserver(std::function<void(const DecryptedMessageEvent&)> callback)
		: callback_(std::move(callback)) {
	}

	void onNotify(const DecryptedMessageEvent& event) override {
		if (callback_) {
			callback_(event);
		}
	}

private:
	std::function<void(const DecryptedMessageEvent&)> callback_;
};

struct ClientSession {
	std::unique_ptr<ChatClient> client;
	std::unique_ptr<GuiObserver> observer;
};

@interface MessengerController : NSObject <NSApplicationDelegate>
@end

@implementation MessengerController {
	NSWindow* _window;
	NSTextField* _usernameField;
	NSTextField* _emailField;
	NSSecureTextField* _passwordField;
	NSTextField* _peerField;
	NSTextField* _messageField;
	NSTextField* _groupNameField;
	NSTextField* _groupMembersField;
	NSTextField* _groupIdField;
	NSTextField* _deleteMessageIdField;
	NSTextField* _statusField;
	NSTextView* _chatView;

	ChatServer _server;
	OpenSSLKeyFactory _keyFactory;
	AES256GCMStrategy _encryption;
	std::unordered_map<std::string, ClientSession> _sessions;
	ChatClient* _activeClient;
}

- (void)appendChatLine:(const std::string&)line {
	NSString* prefix = _chatView.string;
	if (prefix == nil) {
		prefix = @"";
	}
	NSString* next = [prefix stringByAppendingFormat:@"%s\n", line.c_str()];
	[_chatView setString:next];
}

- (void)setStatusText:(const std::string&)text ok:(BOOL)ok {
	[_statusField setStringValue:[NSString stringWithUTF8String:text.c_str()]];
	[_statusField setTextColor:ok ? [NSColor systemGreenColor] : [NSColor systemRedColor]];
}

- (ChatClient*)ensureClient:(const std::string&)username {
	auto it = _sessions.find(username);
	if (it == _sessions.end()) {
		ClientSession session;
		session.client = std::make_unique<ChatClient>(username, _server, _keyFactory, _encryption);
		MessengerController* controller = self;
		session.observer = std::make_unique<GuiObserver>([controller](const DecryptedMessageEvent& event) {
			std::string line = event.fromUser + " -> " + event.toUser + " | " + event.text;
			[controller appendChatLine:line];
		});
		session.client->attach(session.observer.get());
		it = _sessions.emplace(username, std::move(session)).first;
	}
	return it->second.client.get();
}

- (std::string)usernameValue {
	return _usernameField.stringValue.UTF8String ? _usernameField.stringValue.UTF8String : "";
}

- (std::string)passwordValue {
	return _passwordField.stringValue.UTF8String ? _passwordField.stringValue.UTF8String : "";
}

- (std::string)emailValue {
	return _emailField.stringValue.UTF8String ? _emailField.stringValue.UTF8String : "";
}

- (std::string)peerValue {
	return _peerField.stringValue.UTF8String ? _peerField.stringValue.UTF8String : "";
}

- (std::string)messageValue {
	return _messageField.stringValue.UTF8String ? _messageField.stringValue.UTF8String : "";
}

- (std::string)groupNameValue {
	return _groupNameField.stringValue.UTF8String ? _groupNameField.stringValue.UTF8String : "";
}

- (std::string)groupMembersValue {
	return _groupMembersField.stringValue.UTF8String ? _groupMembersField.stringValue.UTF8String : "";
}

- (std::string)groupIdValue {
	return _groupIdField.stringValue.UTF8String ? _groupIdField.stringValue.UTF8String : "";
}

- (std::string)deleteMessageIdValue {
	return _deleteMessageIdField.stringValue.UTF8String ? _deleteMessageIdField.stringValue.UTF8String : "";
}

- (void)refreshConversation {
	if (_activeClient == nullptr) {
		return;
	}

	std::vector<MessageView> messages;
	const std::string groupId = [self groupIdValue];
	if (!groupId.empty()) {
		messages = _activeClient->getGroupConversation(groupId);
	} else {
		const std::string peer = [self peerValue];
		if (peer.empty()) {
			return;
		}
		messages = _activeClient->getConversation(peer);
	}

	NSMutableString* combined = [NSMutableString string];
	for (const auto& m : messages) {
		const char* target = m.isGroupMessage ? m.chatId.c_str() : m.toUser.c_str();
		[combined appendFormat:@"[%s] %s -> %s | %s\n",
		 m.messageId.c_str(),
		 m.fromUser.c_str(),
		 target,
		 m.text.c_str()];
	}
	[_chatView setString:combined];
}

- (void)onRegister:(id)sender {
	(void)sender;
	const auto username = [self usernameValue];
	const auto password = [self passwordValue];
	if (username.empty() || password.empty()) {
		[self setStatusText:"Username and password are required" ok:NO];
		return;
	}
	ChatClient* client = [self ensureClient:username];
	if (!client->registerOnServer(password, [self emailValue])) {
		[self setStatusText:"Registration failed" ok:NO];
		return;
	}
	[self setStatusText:"Registered" ok:YES];
}

- (void)onLogin:(id)sender {
	(void)sender;
	const auto username = [self usernameValue];
	const auto password = [self passwordValue];
	if (username.empty() || password.empty()) {
		[self setStatusText:"Username and password are required" ok:NO];
		return;
	}
	ChatClient* client = [self ensureClient:username];
	if (!client->login(password)) {
		[self setStatusText:"Login failed" ok:NO];
		return;
	}
	_activeClient = client;
	[self setStatusText:"Logged in" ok:YES];
}

- (void)onAddContact:(id)sender {
	(void)sender;
	if (_activeClient == nullptr) {
		[self setStatusText:"Login first" ok:NO];
		return;
	}
	const auto peer = [self peerValue];
	if (peer.empty()) {
		[self setStatusText:"Peer is empty" ok:NO];
		return;
	}
	if (!_activeClient->addContact(peer)) {
		[self setStatusText:"Add contact failed" ok:NO];
		return;
	}
	[self setStatusText:"Contact added" ok:YES];
}

- (void)onSend:(id)sender {
	(void)sender;
	if (_activeClient == nullptr) {
		[self setStatusText:"Login first" ok:NO];
		return;
	}
	const auto peer = [self peerValue];
	const auto text = [self messageValue];
	if (peer.empty() || text.empty()) {
		[self setStatusText:"Peer and message are required" ok:NO];
		return;
	}
	if (!_activeClient->sendMessage(peer, text)) {
		[self setStatusText:"Send failed" ok:NO];
		return;
	}
	[_messageField setStringValue:@""];
	[self setStatusText:"Message sent" ok:YES];
	[self refreshConversation];
}

- (void)onCreateGroup:(id)sender {
	(void)sender;
	if (_activeClient == nullptr) {
		[self setStatusText:"Login first" ok:NO];
		return;
	}
	const auto groupName = [self groupNameValue];
	if (groupName.empty()) {
		[self setStatusText:"Group name is required" ok:NO];
		return;
	}

	std::vector<std::string> members;
	const auto membersRaw = [self groupMembersValue];
	std::string current;
	for (char ch : membersRaw) {
		if (ch == ',') {
			if (!current.empty()) {
				members.push_back(current);
				current.clear();
			}
		} else if (!std::isspace(static_cast<unsigned char>(ch))) {
			current.push_back(ch);
		}
	}
	if (!current.empty()) {
		members.push_back(current);
	}

	const auto groupId = _activeClient->createGroup(groupName, members);
	if (!groupId.has_value()) {
		[self setStatusText:"Create group failed" ok:NO];
		return;
	}
	[_groupIdField setStringValue:[NSString stringWithUTF8String:groupId->c_str()]];
	[self setStatusText:"Group created" ok:YES];
	[self refreshConversation];
}

- (void)onSendGroup:(id)sender {
	(void)sender;
	if (_activeClient == nullptr) {
		[self setStatusText:"Login first" ok:NO];
		return;
	}
	const auto groupId = [self groupIdValue];
	const auto text = [self messageValue];
	if (groupId.empty() || text.empty()) {
		[self setStatusText:"Group ID and message are required" ok:NO];
		return;
	}
	if (!_activeClient->sendGroupMessage(groupId, text)) {
		[self setStatusText:"Send group failed" ok:NO];
		return;
	}
	[_messageField setStringValue:@""];
	[self setStatusText:"Group message sent" ok:YES];
	[self refreshConversation];
}

- (void)onDeleteForAll:(id)sender {
	(void)sender;
	if (_activeClient == nullptr) {
		[self setStatusText:"Login first" ok:NO];
		return;
	}
	const auto messageId = [self deleteMessageIdValue];
	if (messageId.empty()) {
		[self setStatusText:"Message ID is required" ok:NO];
		return;
	}
	if (!_activeClient->deleteMessageForAll(messageId)) {
		[self setStatusText:"Delete failed" ok:NO];
		return;
	}
	[self setStatusText:"Message deleted for all" ok:YES];
	[self refreshConversation];
}

- (void)onRefresh:(id)sender {
	(void)sender;
	[self refreshConversation];
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
	(void)notification;
	_activeClient = nullptr;

	// Ensure the app becomes a regular foreground app and receives keyboard events.
	[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

	_window = [[NSWindow alloc] initWithContentRect:NSMakeRect(200, 200, 900, 600)
		styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable)
		backing:NSBackingStoreBuffered
		defer:NO];
	[_window setTitle:@"Evlampy Messenger (Minimal macOS GUI)"];
	[_window makeKeyAndOrderFront:nil];
	[NSApp activateIgnoringOtherApps:YES];

	NSView* content = [_window contentView];

	CGFloat y = 550;
	_usernameField = [[NSTextField alloc] initWithFrame:NSMakeRect(20, y, 140, 24)];
	[_usernameField setPlaceholderString:@"Username"];
	[content addSubview:_usernameField];

	_emailField = [[NSTextField alloc] initWithFrame:NSMakeRect(170, y, 180, 24)];
	[_emailField setPlaceholderString:@"Email"];
	[content addSubview:_emailField];

	_passwordField = [[NSSecureTextField alloc] initWithFrame:NSMakeRect(360, y, 140, 24)];
	[_passwordField setPlaceholderString:@"Password"];
	[content addSubview:_passwordField];

	NSButton* registerButton = [[NSButton alloc] initWithFrame:NSMakeRect(510, y, 90, 24)];
	[registerButton setTitle:@"Register"];
	[registerButton setButtonType:NSButtonTypeMomentaryPushIn];
	[registerButton setBezelStyle:NSBezelStyleRounded];
	[registerButton setTarget:self];
	[registerButton setAction:@selector(onRegister:)];
	[content addSubview:registerButton];

	NSButton* loginButton = [[NSButton alloc] initWithFrame:NSMakeRect(610, y, 90, 24)];
	[loginButton setTitle:@"Login"];
	[loginButton setButtonType:NSButtonTypeMomentaryPushIn];
	[loginButton setBezelStyle:NSBezelStyleRounded];
	[loginButton setTarget:self];
	[loginButton setAction:@selector(onLogin:)];
	[content addSubview:loginButton];

	_peerField = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 510, 180, 24)];
	[_peerField setPlaceholderString:@"Peer username"];
	[content addSubview:_peerField];

	_messageField = [[NSTextField alloc] initWithFrame:NSMakeRect(210, 510, 380, 24)];
	[_messageField setPlaceholderString:@"Message text"];
	[content addSubview:_messageField];

	NSButton* addContactButton = [[NSButton alloc] initWithFrame:NSMakeRect(600, 510, 120, 24)];
	[addContactButton setTitle:@"Add contact"];
	[addContactButton setButtonType:NSButtonTypeMomentaryPushIn];
	[addContactButton setBezelStyle:NSBezelStyleRounded];
	[addContactButton setTarget:self];
	[addContactButton setAction:@selector(onAddContact:)];
	[content addSubview:addContactButton];

	NSButton* sendButton = [[NSButton alloc] initWithFrame:NSMakeRect(730, 510, 70, 24)];
	[sendButton setTitle:@"Send"];
	[sendButton setButtonType:NSButtonTypeMomentaryPushIn];
	[sendButton setBezelStyle:NSBezelStyleRounded];
	[sendButton setTarget:self];
	[sendButton setAction:@selector(onSend:)];
	[content addSubview:sendButton];

	NSButton* refreshButton = [[NSButton alloc] initWithFrame:NSMakeRect(810, 510, 70, 24)];
	[refreshButton setTitle:@"Refresh"];
	[refreshButton setButtonType:NSButtonTypeMomentaryPushIn];
	[refreshButton setBezelStyle:NSBezelStyleRounded];
	[refreshButton setTarget:self];
	[refreshButton setAction:@selector(onRefresh:)];
	[content addSubview:refreshButton];

	_groupNameField = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 475, 160, 24)];
	[_groupNameField setPlaceholderString:@"Group name"];
	[content addSubview:_groupNameField];

	_groupMembersField = [[NSTextField alloc] initWithFrame:NSMakeRect(190, 475, 260, 24)];
	[_groupMembersField setPlaceholderString:@"Members: alice,bob"];
	[content addSubview:_groupMembersField];

	NSButton* createGroupButton = [[NSButton alloc] initWithFrame:NSMakeRect(460, 475, 110, 24)];
	[createGroupButton setTitle:@"Create group"];
	[createGroupButton setButtonType:NSButtonTypeMomentaryPushIn];
	[createGroupButton setBezelStyle:NSBezelStyleRounded];
	[createGroupButton setTarget:self];
	[createGroupButton setAction:@selector(onCreateGroup:)];
	[content addSubview:createGroupButton];

	_groupIdField = [[NSTextField alloc] initWithFrame:NSMakeRect(580, 475, 160, 24)];
	[_groupIdField setPlaceholderString:@"Group ID"];
	[content addSubview:_groupIdField];

	NSButton* sendGroupButton = [[NSButton alloc] initWithFrame:NSMakeRect(750, 475, 130, 24)];
	[sendGroupButton setTitle:@"Send to group"];
	[sendGroupButton setButtonType:NSButtonTypeMomentaryPushIn];
	[sendGroupButton setBezelStyle:NSBezelStyleRounded];
	[sendGroupButton setTarget:self];
	[sendGroupButton setAction:@selector(onSendGroup:)];
	[content addSubview:sendGroupButton];

	_deleteMessageIdField = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 440, 300, 24)];
	[_deleteMessageIdField setPlaceholderString:@"Message ID for delete"];
	[content addSubview:_deleteMessageIdField];

	NSButton* deleteButton = [[NSButton alloc] initWithFrame:NSMakeRect(330, 440, 140, 24)];
	[deleteButton setTitle:@"Delete for all"];
	[deleteButton setButtonType:NSButtonTypeMomentaryPushIn];
	[deleteButton setBezelStyle:NSBezelStyleRounded];
	[deleteButton setTarget:self];
	[deleteButton setAction:@selector(onDeleteForAll:)];
	[content addSubview:deleteButton];

	NSScrollView* scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(20, 60, 860, 370)];
	[scroll setHasVerticalScroller:YES];
	_chatView = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, 860, 370)];
	[_chatView setEditable:NO];
	[scroll setDocumentView:_chatView];
	[content addSubview:scroll];

	_statusField = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 20, 860, 24)];
	[_statusField setEditable:NO];
	[_statusField setBordered:NO];
	[_statusField setDrawsBackground:NO];
	[_statusField setStringValue:@"Ready"];
	[content addSubview:_statusField];

	// Focus username field so text input works immediately after startup.
	[_window makeFirstResponder:_usernameField];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
	(void)sender;
	return YES;
}

@end

int main(int argc, const char* argv[]) {
	(void)argc;
	(void)argv;
	@autoreleasepool {
		NSApplication* app = [NSApplication sharedApplication];
		MessengerController* delegate = [[MessengerController alloc] init];
		[app setDelegate:delegate];
		[app run];
	}
	return 0;
}

