#pragma once

#include "chat/ChatClient.h"

#include <QWidget>

#include <memory>
#include <string>
#include <unordered_map>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTextEdit;

class MessengerWindow final : public QWidget, public Observer<DecryptedMessageEvent> {
public:
	explicit MessengerWindow(QWidget* parent = nullptr);
	~MessengerWindow() override;

	void onNotify(const DecryptedMessageEvent& event) override;

private:
	void buildUi();
	void bindUi();
	void setStatus(const std::string& statusText, bool error = false);
	ChatClient* currentClient() const;
	ChatClient& ensureClient(const std::string& username);
	void switchCurrentUser(const std::string& username);
	void refreshUsers();
	void refreshContacts();
	void refreshGroups();
	void refreshCurrentChat();

	ChatServer server_;
	OpenSSLKeyFactory keyFactory_;
	AES256GCMStrategy encryption_;
	std::unordered_map<std::string, std::unique_ptr<ChatClient>> clients_;
	ChatClient* currentClient_ = nullptr;

	QLineEdit* usernameInput_ = nullptr;
	QLineEdit* emailInput_ = nullptr;
	QLineEdit* passwordInput_ = nullptr;
	QPushButton* registerButton_ = nullptr;
	QPushButton* loginButton_ = nullptr;
	QPushButton* logoutButton_ = nullptr;

	QListWidget* usersList_ = nullptr;
	QListWidget* contactsList_ = nullptr;
	QPushButton* addContactButton_ = nullptr;
	QPushButton* removeContactButton_ = nullptr;

	QListWidget* groupsList_ = nullptr;
	QLineEdit* groupNameInput_ = nullptr;
	QLineEdit* groupMembersInput_ = nullptr;
	QPushButton* createGroupButton_ = nullptr;

	QTextEdit* chatView_ = nullptr;
	QLineEdit* messageInput_ = nullptr;
	QPushButton* sendButton_ = nullptr;
	QPushButton* sendGroupButton_ = nullptr;
	QLineEdit* deleteMessageIdInput_ = nullptr;
	QPushButton* deleteMessageButton_ = nullptr;

	QLabel* statusLabel_ = nullptr;

	std::string selectedUser_;
	std::string selectedGroup_;
};

