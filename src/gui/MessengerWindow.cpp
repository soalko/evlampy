#include "gui/MessengerWindow.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

#include <algorithm>
#include <sstream>

namespace {
std::vector<std::string> splitCommaSeparated(const QString& raw) {
	std::vector<std::string> out;
	for (const auto& part : raw.split(',', Qt::SkipEmptyParts)) {
		const auto trimmed = part.trimmed();
		if (!trimmed.isEmpty()) {
			out.push_back(trimmed.toStdString());
		}
	}
	return out;
}

std::string statusToString(MessageStatus status) {
	switch (status) {
	case MessageStatus::Sent:
		return "sent";
	case MessageStatus::Delivered:
		return "delivered";
	case MessageStatus::Read:
		return "read";
	}
	return "unknown";
}
}

MessengerWindow::MessengerWindow(QWidget* parent)
	: QWidget(parent) {
	buildUi();
	bindUi();
	setStatus("Ready");
}

MessengerWindow::~MessengerWindow() {
	for (auto& [_, client] : clients_) {
		client->detach(this);
	}
}

void MessengerWindow::onNotify(const DecryptedMessageEvent& event) {
	if (event.isGroupMessage) {
		if (!selectedGroup_.empty() && selectedGroup_ == event.chatId) {
			refreshCurrentChat();
		}
		return;
	}

	if (!selectedUser_.empty() && (selectedUser_ == event.fromUser || selectedUser_ == event.toUser)) {
		refreshCurrentChat();
	}
}

void MessengerWindow::buildUi() {
	setWindowTitle("Evlampy Messenger - Minimal GUI");
	resize(1024, 640);

	auto* root = new QVBoxLayout(this);

	auto* authLayout = new QGridLayout();
	authLayout->addWidget(new QLabel("Username:"), 0, 0);
	usernameInput_ = new QLineEdit();
	authLayout->addWidget(usernameInput_, 0, 1);
	authLayout->addWidget(new QLabel("Email:"), 0, 2);
	emailInput_ = new QLineEdit();
	authLayout->addWidget(emailInput_, 0, 3);
	authLayout->addWidget(new QLabel("Password:"), 0, 4);
	passwordInput_ = new QLineEdit();
	passwordInput_->setEchoMode(QLineEdit::Password);
	authLayout->addWidget(passwordInput_, 0, 5);
	registerButton_ = new QPushButton("Register");
	loginButton_ = new QPushButton("Login");
	logoutButton_ = new QPushButton("Logout");
	authLayout->addWidget(registerButton_, 0, 6);
	authLayout->addWidget(loginButton_, 0, 7);
	authLayout->addWidget(logoutButton_, 0, 8);
	root->addLayout(authLayout);

	auto* body = new QHBoxLayout();

	auto* left = new QVBoxLayout();
	left->addWidget(new QLabel("Users"));
	usersList_ = new QListWidget();
	left->addWidget(usersList_);
	left->addWidget(new QLabel("Contacts"));
	contactsList_ = new QListWidget();
	left->addWidget(contactsList_);
	auto* contactButtons = new QHBoxLayout();
	addContactButton_ = new QPushButton("Add Contact");
	removeContactButton_ = new QPushButton("Remove Contact");
	contactButtons->addWidget(addContactButton_);
	contactButtons->addWidget(removeContactButton_);
	left->addLayout(contactButtons);

	left->addWidget(new QLabel("Groups"));
	groupsList_ = new QListWidget();
	left->addWidget(groupsList_);
	groupNameInput_ = new QLineEdit();
	groupNameInput_->setPlaceholderText("Group name");
	groupMembersInput_ = new QLineEdit();
	groupMembersInput_->setPlaceholderText("Members: alice,bob");
	createGroupButton_ = new QPushButton("Create Group");
	left->addWidget(groupNameInput_);
	left->addWidget(groupMembersInput_);
	left->addWidget(createGroupButton_);

	body->addLayout(left, 1);

	auto* right = new QVBoxLayout();
	chatView_ = new QTextEdit();
	chatView_->setReadOnly(true);
	right->addWidget(chatView_, 1);

	messageInput_ = new QLineEdit();
	messageInput_->setPlaceholderText("Type message...");
	right->addWidget(messageInput_);

	auto* sendButtons = new QHBoxLayout();
	sendButton_ = new QPushButton("Send to user");
	sendGroupButton_ = new QPushButton("Send to group");
	sendButtons->addWidget(sendButton_);
	sendButtons->addWidget(sendGroupButton_);
	right->addLayout(sendButtons);

	auto* deleteLayout = new QHBoxLayout();
	deleteMessageIdInput_ = new QLineEdit();
	deleteMessageIdInput_->setPlaceholderText("Message ID for delete");
	deleteMessageButton_ = new QPushButton("Delete for all");
	deleteLayout->addWidget(deleteMessageIdInput_);
	deleteLayout->addWidget(deleteMessageButton_);
	right->addLayout(deleteLayout);

	body->addLayout(right, 2);
	root->addLayout(body, 1);

	statusLabel_ = new QLabel();
	root->addWidget(statusLabel_);
}

void MessengerWindow::bindUi() {
	connect(registerButton_, &QPushButton::clicked, this, [this]() {
		const auto username = usernameInput_->text().trimmed().toStdString();
		const auto password = passwordInput_->text().toStdString();
		const auto email = emailInput_->text().trimmed().toStdString();
		if (username.empty() || password.empty()) {
			setStatus("Username/password are required", true);
			return;
		}

		auto& client = ensureClient(username);
		if (!client.registerOnServer(password, email)) {
			setStatus("Registration failed (user may already exist)", true);
			return;
		}
		setStatus("Registered: " + username);
		refreshUsers();
	});

	connect(loginButton_, &QPushButton::clicked, this, [this]() {
		const auto username = usernameInput_->text().trimmed().toStdString();
		const auto password = passwordInput_->text().toStdString();
		if (username.empty() || password.empty()) {
			setStatus("Username/password are required", true);
			return;
		}

		auto& client = ensureClient(username);
		if (!client.login(password)) {
			setStatus("Login failed. Check password or local key files.", true);
			return;
		}
		switchCurrentUser(username);
		setStatus("Logged in as " + username);
		refreshUsers();
		refreshContacts();
		refreshGroups();
	});

	connect(logoutButton_, &QPushButton::clicked, this, [this]() {
		if (auto* client = currentClient()) {
			client->logout();
			client->detach(this);
			currentClient_ = nullptr;
			selectedUser_.clear();
			selectedGroup_.clear();
			chatView_->clear();
			setStatus("Logged out");
		}
	});

	connect(usersList_, &QListWidget::itemSelectionChanged, this, [this]() {
		const auto items = usersList_->selectedItems();
		if (!items.empty()) {
			selectedUser_ = items.front()->text().toStdString();
			selectedGroup_.clear();
			refreshCurrentChat();
		}
	});

	connect(contactsList_, &QListWidget::itemSelectionChanged, this, [this]() {
		const auto items = contactsList_->selectedItems();
		if (!items.empty()) {
			selectedUser_ = items.front()->text().toStdString();
			selectedGroup_.clear();
			refreshCurrentChat();
		}
	});

	connect(groupsList_, &QListWidget::itemSelectionChanged, this, [this]() {
		const auto items = groupsList_->selectedItems();
		if (!items.empty()) {
			selectedGroup_ = items.front()->text().toStdString();
			selectedUser_.clear();
			refreshCurrentChat();
		}
	});

	connect(addContactButton_, &QPushButton::clicked, this, [this]() {
		auto* client = currentClient();
		if (!client || selectedUser_.empty()) {
			setStatus("Select user and login first", true);
			return;
		}
		if (!client->addContact(selectedUser_)) {
			setStatus("Failed to add contact", true);
			return;
		}
		setStatus("Contact added: " + selectedUser_);
		refreshContacts();
	});

	connect(removeContactButton_, &QPushButton::clicked, this, [this]() {
		auto* client = currentClient();
		if (!client || selectedUser_.empty()) {
			setStatus("Select contact and login first", true);
			return;
		}
		if (!client->removeContact(selectedUser_)) {
			setStatus("Failed to remove contact", true);
			return;
		}
		setStatus("Contact removed: " + selectedUser_);
		refreshContacts();
	});

	connect(createGroupButton_, &QPushButton::clicked, this, [this]() {
		auto* client = currentClient();
		if (!client) {
			setStatus("Login first", true);
			return;
		}
		const auto groupName = groupNameInput_->text().trimmed().toStdString();
		if (groupName.empty()) {
			setStatus("Group name is required", true);
			return;
		}
		const auto members = splitCommaSeparated(groupMembersInput_->text());
		const auto groupId = client->createGroup(groupName, members);
		if (!groupId.has_value()) {
			setStatus("Failed to create group", true);
			return;
		}
		selectedGroup_ = *groupId;
		selectedUser_.clear();
		setStatus("Group created: " + *groupId);
		refreshGroups();
		refreshCurrentChat();
	});

	connect(sendButton_, &QPushButton::clicked, this, [this]() {
		auto* client = currentClient();
		if (!client || selectedUser_.empty()) {
			setStatus("Select user and login first", true);
			return;
		}
		const auto text = messageInput_->text().toStdString();
		if (text.empty()) {
			setStatus("Message is empty", true);
			return;
		}
		if (!client->sendMessage(selectedUser_, text)) {
			setStatus("Message send failed", true);
			return;
		}
		messageInput_->clear();
		setStatus("Message sent");
		refreshCurrentChat();
	});

	connect(sendGroupButton_, &QPushButton::clicked, this, [this]() {
		auto* client = currentClient();
		if (!client || selectedGroup_.empty()) {
			setStatus("Select group and login first", true);
			return;
		}
		const auto text = messageInput_->text().toStdString();
		if (text.empty()) {
			setStatus("Message is empty", true);
			return;
		}
		if (!client->sendGroupMessage(selectedGroup_, text)) {
			setStatus("Group send failed", true);
			return;
		}
		messageInput_->clear();
		setStatus("Group message sent");
		refreshCurrentChat();
	});

	connect(deleteMessageButton_, &QPushButton::clicked, this, [this]() {
		auto* client = currentClient();
		if (!client) {
			setStatus("Login first", true);
			return;
		}
		const auto messageId = deleteMessageIdInput_->text().trimmed().toStdString();
		if (messageId.empty()) {
			setStatus("Message ID is required", true);
			return;
		}
		if (!client->deleteMessageForAll(messageId)) {
			setStatus("Delete failed (check ownership and ID)", true);
			return;
		}
		setStatus("Message deleted for all");
		refreshCurrentChat();
	});
}

void MessengerWindow::setStatus(const std::string& statusText, bool error) {
	statusLabel_->setText(QString::fromStdString(statusText));
	statusLabel_->setStyleSheet(error ? "color: #b00020;" : "color: #2b7a0b;");
}

ChatClient* MessengerWindow::currentClient() const {
	return currentClient_;
}

ChatClient& MessengerWindow::ensureClient(const std::string& username) {
	auto it = clients_.find(username);
	if (it == clients_.end()) {
		auto inserted = clients_.emplace(username,
			std::make_unique<ChatClient>(username, server_, keyFactory_, encryption_));
		it = inserted.first;
	}
	return *it->second;
}

void MessengerWindow::switchCurrentUser(const std::string& username) {
	if (currentClient_) {
		currentClient_->detach(this);
	}
	currentClient_ = &ensureClient(username);
	currentClient_->attach(this);
}

void MessengerWindow::refreshUsers() {
	usersList_->clear();
	if (auto* client = currentClient()) {
		for (const auto& user : client->listUsers()) {
			usersList_->addItem(QString::fromStdString(user));
		}
	}
}

void MessengerWindow::refreshContacts() {
	contactsList_->clear();
	if (auto* client = currentClient()) {
		for (const auto& contact : client->listContacts()) {
			contactsList_->addItem(QString::fromStdString(contact));
		}
	}
}

void MessengerWindow::refreshGroups() {
	groupsList_->clear();
	if (auto* client = currentClient()) {
		for (const auto& groupId : client->listGroups()) {
			groupsList_->addItem(QString::fromStdString(groupId));
		}
	}
}

void MessengerWindow::refreshCurrentChat() {
	auto* client = currentClient();
	if (!client) {
		chatView_->setPlainText("Login to view chat history.");
		return;
	}

	std::vector<MessageView> messages;
	if (!selectedGroup_.empty()) {
		messages = client->getGroupConversation(selectedGroup_);
	} else if (!selectedUser_.empty()) {
		messages = client->getConversation(selectedUser_);
	} else {
		chatView_->setPlainText("Select user or group.");
		return;
	}

	std::ostringstream os;
	for (const auto& message : messages) {
		os << '[' << message.messageId << "] "
		   << message.fromUser << " -> "
		   << (message.isGroupMessage ? message.chatId : message.toUser)
		   << " | " << statusToString(message.status)
		   << " | " << message.text << '\n';
	}
	chatView_->setPlainText(QString::fromStdString(os.str()));
}

