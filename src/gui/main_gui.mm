#import <Cocoa/Cocoa.h>

#include "chat/ChatClient.h"

#include <functional>
#include <cctype>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

enum class ChatKind {
	Direct = 0,
	Group = 1,
};

struct ChatEntry {
	ChatKind kind = ChatKind::Direct;
	std::string id;
	std::string title;
};

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

NSString* statusText(MessageStatus status) {
	switch (status) {
	case MessageStatus::Sent:
		return @"sent";
	case MessageStatus::Delivered:
		return @"delivered";
	case MessageStatus::Read:
		return @"read";
	}
	return @"unknown";
}

std::vector<std::string> splitMembers(const std::string& raw) {
	std::vector<std::string> out;
	std::string current;
	for (char ch : raw) {
		if (ch == ',') {
			if (!current.empty()) {
				out.push_back(current);
				current.clear();
			}
		} else if (!std::isspace(static_cast<unsigned char>(ch))) {
			current.push_back(ch);
		}
	}
	if (!current.empty()) {
		out.push_back(current);
	}
	return out;
}

NSString* formatMessage(const MessageView& message) {
	const auto dt = [NSDate dateWithTimeIntervalSince1970:static_cast<double>(message.timestamp) / 1000.0];
	NSDateFormatter* formatter = [[NSDateFormatter alloc] init];
	[formatter setDateFormat:@"HH:mm:ss"];
	const auto time = [formatter stringFromDate:dt];
	const auto target = message.isGroupMessage ? message.chatId : message.toUser;
	return [NSString stringWithFormat:@"[%@] %s -> %s | %@ | %s",
			time,
			message.fromUser.c_str(),
			target.c_str(),
			statusText(message.status),
			message.text.c_str()];
}
}

@interface MessengerController : NSObject <NSApplicationDelegate, NSTableViewDelegate, NSTableViewDataSource>
@end

@implementation MessengerController {
	NSWindow* _window;
	NSView* _authView;
	NSView* _mainView;

	NSTextField* _usernameField;
	NSSecureTextField* _passwordField;
	NSButton* _registerButton;
	NSButton* _loginButton;
	NSTextField* _authStatusLabel;

	NSTextField* _currentUserLabel;
	NSTableView* _chatTableView;
	NSTableView* _messageTableView;
	NSTextField* _messageField;
	NSTextField* _statusField;
	NSTextField* _activeChatLabel;
	NSButton* _newChatButton;
	NSButton* _createGroupButton;
	NSButton* _logoutButton;
	NSButton* _refreshButton;
	NSButton* _sendButton;
	NSButton* _deleteButton;

	ChatServer _server;
	OpenSSLKeyFactory _keyFactory;
	AES256GCMStrategy _encryption;
	std::unordered_map<std::string, std::unique_ptr<ChatClient>> _clients;
	std::unique_ptr<GuiObserver> _observer;
	ChatClient* _activeClient;
	std::vector<ChatEntry> _chats;
	std::vector<MessageView> _messages;
	ChatKind _selectedChatKind;
	std::string _selectedChatId;
}

- (instancetype)init {
	self = [super init];
	if (self) {
		_activeClient = nullptr;
		_selectedChatKind = ChatKind::Direct;
		MessengerController* controller = self;
		_observer = std::make_unique<GuiObserver>([controller](const DecryptedMessageEvent& event) {
			[controller handleIncomingEvent:event];
		});
	}
	return self;
}

- (void)handleIncomingEvent:(const DecryptedMessageEvent&)event {
	if (event.isGroupMessage) {
		if (_selectedChatKind == ChatKind::Group && _selectedChatId == event.chatId) {
			[self refreshMessages];
		}
	} else if (_selectedChatKind == ChatKind::Direct && _selectedChatId == event.fromUser) {
		[self refreshMessages];
	}
}

- (void)appendMessage:(NSString*)text {
	NSAlert* alert = [[NSAlert alloc] init];
	[alert setMessageText:text];
	[alert runModal];
}

- (void)setStatus:(NSString*)text error:(BOOL)error {
	[_statusField setStringValue:text ? text : @""];
	[_statusField setTextColor:error ? [NSColor systemRedColor] : [NSColor systemGreenColor]];
}

- (std::string)usernameValue {
	return _usernameField.stringValue.UTF8String ? _usernameField.stringValue.UTF8String : "";
}

- (std::string)passwordValue {
	return _passwordField.stringValue.UTF8String ? _passwordField.stringValue.UTF8String : "";
}

- (ChatClient*)currentClient {
	return _activeClient;
}

- (ChatClient&)ensureClient:(const std::string&)username {
	auto it = _clients.find(username);
	if (it == _clients.end()) {
		auto inserted = _clients.emplace(username, std::make_unique<ChatClient>(username, _server, _keyFactory, _encryption));
		it = inserted.first;
	}
	return *it->second;
}

- (void)showAuthView {
	[_authView setHidden:NO];
	[_mainView setHidden:YES];
	[_usernameField setStringValue:@""];
	[_passwordField setStringValue:@""];
	if (_window != nil) {
		[_window makeKeyAndOrderFront:nil];
		[_window makeFirstResponder:_usernameField];
	}
}

- (void)showMainView {
	[_authView setHidden:YES];
	[_mainView setHidden:NO];
	if (_activeClient != nullptr) {
		[_currentUserLabel setStringValue:[NSString stringWithFormat:@"Logged in as %s", _activeClient->username().c_str()]];
	}
	if (_window != nil) {
		[_window makeKeyAndOrderFront:nil];
		[_window makeFirstResponder:_messageField];
	}
}

- (void)refreshChats {
	_chats.clear();

	auto* client = [self currentClient];
	if (client == nullptr) {
		[_chatTableView reloadData];
		return;
	}

	for (const auto& contact : client->listContacts()) {
		_chats.push_back(ChatEntry{ChatKind::Direct, contact, contact});
	}
	for (const auto& groupId : client->listGroups()) {
		const auto groupName = _server.getGroupName(groupId);
		const auto title = groupName.empty() ? groupId : groupName + " (" + groupId + ")";
		_chats.push_back(ChatEntry{ChatKind::Group, groupId, title});
	}

	[_chatTableView reloadData];
	if (_chatTableView.numberOfRows > 0 && _chatTableView.selectedRow < 0) {
		[_chatTableView selectRowIndexes:[NSIndexSet indexSetWithIndex:0] byExtendingSelection:NO];
	}
}

- (void)refreshMessages {
	_messages.clear();
	[_messageTableView reloadData];

	auto* client = [self currentClient];
	if (client == nullptr) {
		[_activeChatLabel setStringValue:@"Login to start chatting"];
		[_messageTableView reloadData];
		return;
	}

	if (_selectedChatId.empty()) {
		[_activeChatLabel setStringValue:@"Select a chat or create a new one"];
		return;
	}

	if (_selectedChatKind == ChatKind::Direct) {
		_messages = client->getConversation(_selectedChatId);
		[_activeChatLabel setStringValue:[NSString stringWithFormat:@"Chat with %s", _selectedChatId.c_str()]];
	} else {
		_messages = client->getGroupConversation(_selectedChatId);
		const auto groupName = _server.getGroupName(_selectedChatId);
		if (groupName.empty()) {
			[_activeChatLabel setStringValue:[NSString stringWithFormat:@"Group %s", _selectedChatId.c_str()]];
		} else {
			[_activeChatLabel setStringValue:[NSString stringWithFormat:@"%s (%s)", groupName.c_str(), _selectedChatId.c_str()]];
		}
	}

	[_messageTableView reloadData];
}

- (void)buildUi {
	_window = [[NSWindow alloc] initWithContentRect:NSMakeRect(180, 120, 1120, 720)
		styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable)
		backing:NSBackingStoreBuffered
		defer:NO];
	[_window setTitle:@"Evlampy Messenger"];

	NSView* content = [_window contentView];

	_authView = [[NSView alloc] initWithFrame:[content bounds]];
	[_authView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
	[content addSubview:_authView];

	_mainView = [[NSView alloc] initWithFrame:[content bounds]];
	[_mainView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
	[content addSubview:_mainView];

	// Auth page
	{
		NSTextField* title = [[NSTextField alloc] initWithFrame:NSMakeRect(300, 530, 520, 32)];
		[title setStringValue:@"Evlampy Messenger"];
		[title setEditable:NO];
		[title setBezeled:NO];
		[title setDrawsBackground:NO];
		[title setAlignment:NSTextAlignmentCenter];
		[title setFont:[NSFont boldSystemFontOfSize:26]];
		[_authView addSubview:title];

		NSSearchField* spacer = nil;
		(void)spacer;

		NSTextField* usernameLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(320, 455, 120, 24)];
		[usernameLabel setStringValue:@"Username"];
		[usernameLabel setEditable:NO];
		[usernameLabel setBezeled:NO];
		[usernameLabel setDrawsBackground:NO];
		[_authView addSubview:usernameLabel];

		_usernameField = [[NSTextField alloc] initWithFrame:NSMakeRect(445, 452, 240, 24)];
		[_usernameField setPlaceholderString:@"username"];
		[_authView addSubview:_usernameField];

		NSTextField* passwordLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(320, 410, 120, 24)];
		[passwordLabel setStringValue:@"Password"];
		[passwordLabel setEditable:NO];
		[passwordLabel setBezeled:NO];
		[passwordLabel setDrawsBackground:NO];
		[_authView addSubview:passwordLabel];

		_passwordField = [[NSSecureTextField alloc] initWithFrame:NSMakeRect(445, 407, 240, 24)];
		[_passwordField setPlaceholderString:@"password"];
		[_authView addSubview:_passwordField];

		_registerButton = [[NSButton alloc] initWithFrame:NSMakeRect(445, 360, 110, 28)];
		[_registerButton setTitle:@"Register"];
		[_registerButton setBezelStyle:NSBezelStyleRounded];
		[_authView addSubview:_registerButton];

		_loginButton = [[NSButton alloc] initWithFrame:NSMakeRect(575, 360, 110, 28)];
		[_loginButton setTitle:@"Login"];
		[_loginButton setBezelStyle:NSBezelStyleRounded];
		[_authView addSubview:_loginButton];

		_authStatusLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(300, 300, 520, 24)];
		[_authStatusLabel setEditable:NO];
		[_authStatusLabel setBezeled:NO];
		[_authStatusLabel setDrawsBackground:NO];
		[_authStatusLabel setAlignment:NSTextAlignmentCenter];
		[_authView addSubview:_authStatusLabel];
	}

	// Main page
	{
		_currentUserLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 672, 400, 24)];
		[_currentUserLabel setEditable:NO];
		[_currentUserLabel setBezeled:NO];
		[_currentUserLabel setDrawsBackground:NO];
		[_currentUserLabel setStringValue:@"Not logged in"];
		[_mainView addSubview:_currentUserLabel];

		_logoutButton = [[NSButton alloc] initWithFrame:NSMakeRect(980, 670, 120, 28)];
		[_logoutButton setTitle:@"Logout"];
		[_logoutButton setBezelStyle:NSBezelStyleRounded];
		[_mainView addSubview:_logoutButton];

		_refreshButton = [[NSButton alloc] initWithFrame:NSMakeRect(850, 670, 120, 28)];
		[_refreshButton setTitle:@"Refresh"];
		[_refreshButton setBezelStyle:NSBezelStyleRounded];
		[_mainView addSubview:_refreshButton];

		_newChatButton = [[NSButton alloc] initWithFrame:NSMakeRect(20, 630, 130, 28)];
		[_newChatButton setTitle:@"New chat"];
		[_newChatButton setBezelStyle:NSBezelStyleRounded];
		[_mainView addSubview:_newChatButton];

		_createGroupButton = [[NSButton alloc] initWithFrame:NSMakeRect(160, 630, 130, 28)];
		[_createGroupButton setTitle:@"Create group"];
		[_createGroupButton setBezelStyle:NSBezelStyleRounded];
		[_mainView addSubview:_createGroupButton];

		NSScrollView* chatScroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(20, 150, 270, 470)];
		[chatScroll setHasVerticalScroller:YES];
		_chatTableView = [[NSTableView alloc] initWithFrame:chatScroll.bounds];
		NSTableColumn* chatColumn = [[NSTableColumn alloc] initWithIdentifier:@"chat"];
		[chatColumn setWidth:250];
		[_chatTableView addTableColumn:chatColumn];
		[_chatTableView setHeaderView:nil];
		[_chatTableView setDelegate:self];
		[_chatTableView setDataSource:self];
		[_chatTableView setSelectionHighlightStyle:NSTableViewSelectionHighlightStyleRegular];
		[chatScroll setDocumentView:_chatTableView];
		[_mainView addSubview:chatScroll];

		_activeChatLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(320, 630, 580, 28)];
		[_activeChatLabel setEditable:NO];
		[_activeChatLabel setBezeled:NO];
		[_activeChatLabel setDrawsBackground:NO];
		[_activeChatLabel setStringValue:@"Select a chat or create a new one"];
		[_mainView addSubview:_activeChatLabel];

		NSScrollView* messageScroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(320, 150, 780, 470)];
		[messageScroll setHasVerticalScroller:YES];
		_messageTableView = [[NSTableView alloc] initWithFrame:messageScroll.bounds];
		NSTableColumn* messageColumn = [[NSTableColumn alloc] initWithIdentifier:@"message"];
		[messageColumn setWidth:760];
		[_messageTableView addTableColumn:messageColumn];
		[_messageTableView setHeaderView:nil];
		[_messageTableView setDelegate:self];
		[_messageTableView setDataSource:self];
		[_messageTableView setSelectionHighlightStyle:NSTableViewSelectionHighlightStyleRegular];
		[messageScroll setDocumentView:_messageTableView];
		[_mainView addSubview:messageScroll];

		_messageField = [[NSTextField alloc] initWithFrame:NSMakeRect(320, 95, 560, 28)];
		[_messageField setPlaceholderString:@"Write a message..."];
		[_mainView addSubview:_messageField];

		_sendButton = [[NSButton alloc] initWithFrame:NSMakeRect(890, 93, 95, 30)];
		[_sendButton setTitle:@"Send"];
		[_sendButton setBezelStyle:NSBezelStyleRounded];
		[_mainView addSubview:_sendButton];

		_deleteButton = [[NSButton alloc] initWithFrame:NSMakeRect(995, 93, 105, 30)];
		[_deleteButton setTitle:@"Delete"];
		[_deleteButton setBezelStyle:NSBezelStyleRounded];
		[_mainView addSubview:_deleteButton];

		_statusField = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 20, 1080, 24)];
		[_statusField setEditable:NO];
		[_statusField setBezeled:NO];
		[_statusField setDrawsBackground:NO];
		[_statusField setStringValue:@"Ready"];
		[_mainView addSubview:_statusField];
	}
}

- (void)bindUi {
	[_registerButton setTarget:self];
	[_registerButton setAction:@selector(onRegister:)];
	[_loginButton setTarget:self];
	[_loginButton setAction:@selector(onLogin:)];
	[_logoutButton setTarget:self];
	[_logoutButton setAction:@selector(onLogout:)];
	[_refreshButton setTarget:self];
	[_refreshButton setAction:@selector(onRefresh:)];
	[_newChatButton setTarget:self];
	[_newChatButton setAction:@selector(onNewChat:)];
	[_createGroupButton setTarget:self];
	[_createGroupButton setAction:@selector(onCreateGroup:)];
	[_sendButton setTarget:self];
	[_sendButton setAction:@selector(onSend:)];
	[_deleteButton setTarget:self];
	[_deleteButton setAction:@selector(onDelete:)];

	[_chatTableView setTarget:self];
	[_chatTableView setDoubleAction:@selector(onChatDoubleClick:)];
}

- (void)onChatDoubleClick:(id)sender {
	(void)sender;
	[self refreshMessages];
}

- (void)onRegister:(id)sender {
	(void)sender;
	const auto username = [self usernameValue];
	const auto password = [self passwordValue];
	if (username.empty() || password.empty()) {
		[self setStatus:@"Username and password are required" error:YES];
		return;
	}
	ChatClient* client = [self currentClient];
	if (client == nullptr) {
		client = &([self ensureClient:username]);
	}
	if (!client->registerOnServer(password)) {
		[self setStatus:@"Registration failed" error:YES];
		return;
	}
	[_authStatusLabel setStringValue:@"Registration successful. You can now log in."];
	[self setStatus:[NSString stringWithFormat:@"Registered: %s", username.c_str()] error:NO];
}

- (void)onLogin:(id)sender {
	(void)sender;
	const auto username = [self usernameValue];
	const auto password = [self passwordValue];
	if (username.empty() || password.empty()) {
		[self setStatus:@"Username and password are required" error:YES];
		return;
	}
	ChatClient& client = [self ensureClient:username];
	if (!client.login(password)) {
		[self setStatus:@"Login failed. Register first or check password." error:YES];
		return;
	}
	if (_activeClient != nullptr && _activeClient != &client) {
		_activeClient->detach(_observer.get());
	}
	_activeClient = &client;
	_activeClient->attach(_observer.get());
	[_currentUserLabel setStringValue:[NSString stringWithFormat:@"Logged in as %s", username.c_str()]];
	[self refreshChats];
	[self refreshMessages];
	[self showMainView];
	[self setStatus:[NSString stringWithFormat:@"Logged in as %s", username.c_str()] error:NO];
}

- (void)onLogout:(id)sender {
	(void)sender;
	if (_activeClient != nullptr) {
		_activeClient->detach(_observer.get());
		_activeClient->logout();
		_activeClient = nullptr;
	}
	_selectedChatId.clear();
	_chats.clear();
	_messages.clear();
	[_chatTableView reloadData];
	[_messageTableView reloadData];
	[self showAuthView];
	[self setStatus:@"Logged out" error:NO];
}

- (void)onRefresh:(id)sender {
	(void)sender;
	[self refreshChats];
	[self refreshMessages];
}

- (void)onNewChat:(id)sender {
	(void)sender;
	if (_activeClient == nullptr) {
		[self setStatus:@"Login first" error:YES];
		return;
	}
	NSAlert* alert = [[NSAlert alloc] init];
	[alert setMessageText:@"New chat"];
	[alert setInformativeText:@"Enter username to start a direct chat"];
	NSTextField* field = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 260, 24)];
	[alert setAccessoryView:field];
	[alert addButtonWithTitle:@"Open"];
	[alert addButtonWithTitle:@"Cancel"];
	if ([alert runModal] != NSAlertFirstButtonReturn) {
		return;
	}
	const std::string peer = field.stringValue.UTF8String ? field.stringValue.UTF8String : "";
	if (peer.empty()) {
		[self setStatus:@"Username is required" error:YES];
		return;
	}
	if (!_activeClient->addContact(peer)) {
		[self setStatus:@"Could not add contact (user may not exist)" error:YES];
		return;
	}
	[self refreshChats];
	[self selectChatWithKind:ChatKind::Direct chatId:peer];
	[self setStatus:[NSString stringWithFormat:@"Chat opened with %s", peer.c_str()] error:NO];
}

- (void)onCreateGroup:(id)sender {
	(void)sender;
	if (_activeClient == nullptr) {
		[self setStatus:@"Login first" error:YES];
		return;
	}
	NSAlert* alert = [[NSAlert alloc] init];
	[alert setMessageText:@"Create group"];
	[alert setInformativeText:@"Enter group name and members separated by commas"];
	NSView* accessory = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 320, 64)];
	NSTextField* nameField = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 36, 320, 24)];
	[nameField setPlaceholderString:@"Group name"];
	NSTextField* membersField = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 4, 320, 24)];
	[membersField setPlaceholderString:@"Members: alice,bob"];
	[accessory addSubview:nameField];
	[accessory addSubview:membersField];
	[alert setAccessoryView:accessory];
	[alert addButtonWithTitle:@"Create"];
	[alert addButtonWithTitle:@"Cancel"];
	if ([alert runModal] != NSAlertFirstButtonReturn) {
		return;
	}
	const std::string groupName = nameField.stringValue.UTF8String ? nameField.stringValue.UTF8String : "";
	if (groupName.empty()) {
		[self setStatus:@"Group name is required" error:YES];
		return;
	}
	const auto members = splitMembers(membersField.stringValue.UTF8String ? membersField.stringValue.UTF8String : "");
	const auto groupId = _activeClient->createGroup(groupName, members);
	if (!groupId.has_value()) {
		[self setStatus:@"Failed to create group" error:YES];
		return;
	}
	[self refreshChats];
	[self selectChatWithKind:ChatKind::Group chatId:*groupId];
	[self setStatus:[NSString stringWithFormat:@"Group created: %s", groupName.c_str()] error:NO];
}

- (void)onSend:(id)sender {
	(void)sender;
	if (_activeClient == nullptr) {
		[self setStatus:@"Login first" error:YES];
		return;
	}
	const std::string text = _messageField.stringValue.UTF8String ? _messageField.stringValue.UTF8String : "";
	if (text.empty()) {
		[self setStatus:@"Message is empty" error:YES];
		return;
	}
	if (_selectedChatId.empty()) {
		[self setStatus:@"Select a chat first" error:YES];
		return;
	}
	bool ok = false;
	if (_selectedChatKind == ChatKind::Direct) {
		ok = _activeClient->sendMessage(_selectedChatId, text);
	} else {
		ok = _activeClient->sendGroupMessage(_selectedChatId, text);
	}
	if (!ok) {
		[self setStatus:@"Send failed" error:YES];
		return;
	}
	[_messageField setStringValue:@""];
	[self refreshMessages];
	[self setStatus:@"Message sent" error:NO];
}

- (void)onDelete:(id)sender {
	(void)sender;
	if (_activeClient == nullptr) {
		[self setStatus:@"Login first" error:YES];
		return;
	}
	const NSInteger row = _messageTableView.selectedRow;
	if (row < 0 || row >= static_cast<NSInteger>(_messages.size())) {
		[self setStatus:@"Select a message first" error:YES];
		return;
	}
	const auto& message = _messages[static_cast<std::size_t>(row)];
	if (!message.messageId.empty() && _activeClient->deleteMessageForAll(message.messageId)) {
		[self refreshMessages];
		[self setStatus:@"Message deleted for all" error:NO];
	} else {
		[self setStatus:@"Delete failed" error:YES];
	}
}

- (void)selectChatWithKind:(ChatKind)kind chatId:(const std::string&)chatId {
	_selectedChatKind = kind;
	_selectedChatId = chatId;
	for (NSInteger i = 0; i < _chatTableView.numberOfRows; ++i) {
		if (i < static_cast<NSInteger>(_chats.size()) && _chats[static_cast<std::size_t>(i)].kind == kind && _chats[static_cast<std::size_t>(i)].id == chatId) {
			[_chatTableView selectRowIndexes:[NSIndexSet indexSetWithIndex:i] byExtendingSelection:NO];
			break;
		}
	}
	[self refreshMessages];
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
	(void)notification;
	[self buildUi];
	[self bindUi];
	[self setStatus:@"Ready" error:NO];
	[_window makeKeyAndOrderFront:nil];
	[NSApp activateIgnoringOtherApps:YES];
	[self showAuthView];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
	(void)sender;
	return YES;
}

- (NSInteger)numberOfRowsInTableView:(NSTableView*)tableView {
	if (tableView == _chatTableView) {
		return static_cast<NSInteger>(_chats.size());
	}
	if (tableView == _messageTableView) {
		return static_cast<NSInteger>(_messages.size());
	}
	return 0;
}

- (nullable NSView*)tableView:(NSTableView*)tableView viewForTableColumn:(NSTableColumn*)tableColumn row:(NSInteger)row {
	NSString* identifier = tableColumn.identifier;
	if (identifier == nil) {
		identifier = @"cell";
	}
	NSTableCellView* cell = [tableView makeViewWithIdentifier:identifier owner:self];
	if (cell == nil) {
		cell = [[NSTableCellView alloc] initWithFrame:NSMakeRect(0, 0, tableColumn.width, 24)];
		NSTextField* textField = [[NSTextField alloc] initWithFrame:cell.bounds];
		[textField setBezeled:NO];
		[textField setDrawsBackground:NO];
		[textField setEditable:NO];
		[textField setSelectable:NO];
		[textField setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
		[cell setTextField:textField];
		[cell addSubview:textField];
		[cell setIdentifier:identifier];
	}

	if (tableView == _chatTableView && row >= 0 && row < static_cast<NSInteger>(_chats.size())) {
		const auto& chat = _chats[static_cast<std::size_t>(row)];
		cell.textField.stringValue = [NSString stringWithUTF8String:chat.title.c_str()];
	} else if (tableView == _messageTableView && row >= 0 && row < static_cast<NSInteger>(_messages.size())) {
		cell.textField.stringValue = formatMessage(_messages[static_cast<std::size_t>(row)]);
	}
	return cell;
}

- (void)tableViewSelectionDidChange:(NSNotification*)notification {
	(void)notification;
	const NSInteger row = _chatTableView.selectedRow;
	if (row < 0 || row >= static_cast<NSInteger>(_chats.size())) {
		return;
	}
	const auto& chat = _chats[static_cast<std::size_t>(row)];
	_selectedChatKind = chat.kind;
	_selectedChatId = chat.id;
	[self refreshMessages];
}

@end

int main(int argc, const char* argv[]) {
	(void)argc;
	(void)argv;
	@autoreleasepool {
		NSApplication* app = [NSApplication sharedApplication];
		[app setActivationPolicy:NSApplicationActivationPolicyRegular];
		MessengerController* delegate = [[MessengerController alloc] init];
		[app setDelegate:delegate];
		[app run];
	}
	return 0;
}

