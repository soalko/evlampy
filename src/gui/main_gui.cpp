#include "chat/ChatClient.h"
#include <QApplication>
#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QDateTime>
#include <QDialog>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QTableWidget>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <memory>
#include <unordered_map>

namespace {
enum class ChatKind { Direct, Group };

QString statusToText(MessageStatus s) {
    switch(s) {
        case MessageStatus::Sent: return "sent";
        case MessageStatus::Delivered: return "delivered";
        case MessageStatus::Read: return "read";
        default: return "unknown";
    }
}

std::vector<std::string> splitMembers(const QString& raw) {
    std::vector<std::string> out;
    for (const auto& part : raw.split(',', Qt::SkipEmptyParts)) {
        QString trimmed = part.trimmed();
        if (!trimmed.isEmpty()) out.push_back(trimmed.toStdString());
    }
    return out;
}

QString formatMessage(const MessageView& msg) {
    QString time = QDateTime::fromMSecsSinceEpoch(msg.timestamp).toString("hh:mm:ss");
    QString chatLabel = msg.isGroupMessage ? QString::fromStdString(msg.chatId) : QString::fromStdString(msg.toUser);
    return QString("[%1] %2 -> %3 | %4 | %5")
        .arg(time, QString::fromStdString(msg.fromUser), chatLabel, statusToText(msg.status), QString::fromStdString(msg.text));
}
} // namespace

class MessengerMainWindow : public QWidget, public Observer<DecryptedMessageEvent> {
public:
    MessengerMainWindow(QWidget* parent = nullptr) : QWidget(parent) {
        buildUi(); bindUi(); showAuthPage(); setStatus("Welcome");
    }
    ~MessengerMainWindow() override {
        for (auto& [_, client] : clients_) client->detach(this);
    }

    void onNotify(const DecryptedMessageEvent& event) override {
        if (!currentClient_) return;
        bool isRelevant = false;
        if (event.isGroupMessage && selectedChatKind_ == ChatKind::Group && selectedChatId_ == event.chatId)
            isRelevant = true;
        else if (!event.isGroupMessage && selectedChatKind_ == ChatKind::Direct && selectedChatId_ == event.fromUser)
            isRelevant = true;
        if (!isRelevant) return;

        // Добавляем новое сообщение в таблицу
        MessageView newMsg;
        newMsg.messageId = event.messageId;
        newMsg.fromUser = event.fromUser;
        newMsg.toUser = event.isGroupMessage ? "" : currentClient_->username();
        newMsg.chatId = event.chatId;
        newMsg.text = event.text;
        newMsg.timestamp = QDateTime::currentMSecsSinceEpoch();
        newMsg.status = MessageStatus::Sent;
        newMsg.isGroupMessage = event.isGroupMessage;
        messages_.push_back(newMsg);
        int row = messageTable_->rowCount();
        messageTable_->insertRow(row);
        QTableWidgetItem* item = new QTableWidgetItem(formatMessage(newMsg));
        item->setData(Qt::UserRole, QString::fromStdString(newMsg.messageId));
        item->setData(Qt::UserRole+1, QString::fromStdString(newMsg.fromUser));
        messageTable_->setItem(row, 0, item);
        messageTable_->scrollToBottom();
    }

private:
    void buildUi() {
        setWindowTitle("Evlampy Messenger"); resize(1180, 720);
        auto* root = new QVBoxLayout(this);
        stack_ = new QStackedWidget(this);
        root->addWidget(stack_, 1);

        // Auth page
        authPage_ = new QWidget();
        auto* authLayout = new QVBoxLayout(authPage_);
        authLayout->addStretch();
        QLabel* title = new QLabel("Evlampy Messenger"); title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet("font-size: 24px; font-weight: 600;");
        authLayout->addWidget(title);
        authLayout->addWidget(new QLabel("Register or log in to start chatting"));
        auto* form = new QWidget(); auto* formLayout = new QFormLayout(form);
        authUsername_ = new QLineEdit(); authUsername_->setPlaceholderText("username");
        authPassword_ = new QLineEdit(); authPassword_->setPlaceholderText("password"); authPassword_->setEchoMode(QLineEdit::Password);
        formLayout->addRow("Username", authUsername_); formLayout->addRow("Password", authPassword_);
        authLayout->addWidget(form, 0, Qt::AlignHCenter);
        auto* btns = new QHBoxLayout();
        registerButton_ = new QPushButton("Register"); loginButton_ = new QPushButton("Login");
        btns->addWidget(registerButton_); btns->addWidget(loginButton_);
        authLayout->addLayout(btns);
        authHint_ = new QLabel(); authHint_->setAlignment(Qt::AlignCenter);
        authLayout->addWidget(authHint_);
        authLayout->addStretch();
        stack_->addWidget(authPage_);

        // Chat page
        chatPage_ = new QWidget();
        auto* chatLayout = new QVBoxLayout(chatPage_);
        auto* top = new QHBoxLayout();
        currentUserLabel_ = new QLabel("Not logged in"); currentUserLabel_->setStyleSheet("font-weight: 600;");
        refreshButton_ = new QPushButton("Refresh"); logoutButton_ = new QPushButton("Logout");
        top->addWidget(currentUserLabel_); top->addStretch(); top->addWidget(refreshButton_); top->addWidget(logoutButton_);
        chatLayout->addLayout(top);

        auto* splitter = new QSplitter(Qt::Horizontal);
        auto* left = new QWidget(); auto* leftLayout = new QVBoxLayout(left);
        auto* leftBtns = new QHBoxLayout();
        newChatButton_ = new QPushButton("New chat"); createGroupButton_ = new QPushButton("Create group");
        leftBtns->addWidget(newChatButton_); leftBtns->addWidget(createGroupButton_);
        leftLayout->addLayout(leftBtns);
        chatList_ = new QListWidget(); chatList_->setSelectionMode(QAbstractItemView::SingleSelection);
        leftLayout->addWidget(chatList_, 1);
        splitter->addWidget(left);

        auto* right = new QWidget(); auto* rightLayout = new QVBoxLayout(right);
        activeChatLabel_ = new QLabel("Select a chat or create a new one"); activeChatLabel_->setStyleSheet("font-weight: 600;");
        rightLayout->addWidget(activeChatLabel_);
        messageTable_ = new QTableWidget();
        messageTable_->setColumnCount(1);
        messageTable_->horizontalHeader()->setVisible(false);
        messageTable_->verticalHeader()->setVisible(false);
        messageTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
        messageTable_->setSelectionMode(QAbstractItemView::SingleSelection);
        messageTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        rightLayout->addWidget(messageTable_, 1);
        auto* compose = new QHBoxLayout();
        messageInput_ = new QLineEdit(); messageInput_->setPlaceholderText("Write a message...");
        sendButton_ = new QPushButton("Send"); deleteButton_ = new QPushButton("Delete selected");
        compose->addWidget(messageInput_, 1); compose->addWidget(sendButton_); compose->addWidget(deleteButton_);
        rightLayout->addLayout(compose);
        splitter->addWidget(right);
        splitter->setStretchFactor(0, 1); splitter->setStretchFactor(1, 2);
        chatLayout->addWidget(splitter, 1);
        stack_->addWidget(chatPage_);

        statusLabel_ = new QLabel("Ready"); statusLabel_->setStyleSheet("color: #2b7a0b;");
        root->addWidget(statusLabel_);
    }

    void bindUi() {
        connect(registerButton_, &QPushButton::clicked, this, [this](){
            std::string u = authUsername_->text().trimmed().toStdString();
            std::string p = authPassword_->text().toStdString();
            if(u.empty()||p.empty()){ setStatus("Username/password required",true); return; }
            auto& client = ensureClient(u);
            if(!client.registerOnServer(p)){ setStatus("Registration failed",true); return; }
            setStatus("Registered: "+u); authHint_->setText("Registration successful. You can now log in.");
        });
        connect(loginButton_, &QPushButton::clicked, this, [this](){
            std::string u = authUsername_->text().trimmed().toStdString();
            std::string p = authPassword_->text().toStdString();
            if(u.empty()||p.empty()){ setStatus("Username/password required",true); return; }
            auto& client = ensureClient(u);
            if(!client.login(p)){ setStatus("Login failed. Check password or register first.",true); return; }
            switchCurrentUser(u);
            showChatPage();
            refreshChatList();
            setStatus("Logged in as "+u);
        });
        connect(logoutButton_, &QPushButton::clicked, this, [this](){
            if(currentClient_){ currentClient_->detach(this); currentClient_->logout(); currentClient_=nullptr; }
            selectedChatId_.clear(); messages_.clear(); messageTable_->clearContents(); messageTable_->setRowCount(0); chatList_->clear();
            showAuthPage(); setStatus("Logged out");
        });
        connect(refreshButton_, &QPushButton::clicked, this, [this](){ refreshChatList(); refreshMessages(); });
        connect(newChatButton_, &QPushButton::clicked, this, [this](){
            if(!currentClient_){ setStatus("Login first",true); return; }
            bool ok; QString peer = QInputDialog::getText(this,"New chat","Enter username:",QLineEdit::Normal,{},&ok);
            if(!ok||peer.isEmpty()) return;
            if(!currentClient_->addContact(peer.toStdString())){ QMessageBox::warning(this,"New chat","User does not exist"); return; }
            refreshChatList(); selectDirectChat(peer.toStdString()); setStatus("Chat opened with "+peer.toStdString());
        });
        connect(createGroupButton_, &QPushButton::clicked, this, [this](){
            if(!currentClient_){ setStatus("Login first",true); return; }
            QDialog d(this); d.setWindowTitle("Create group");
            auto* form = new QFormLayout(&d);
            QLineEdit* nameEdit = new QLineEdit(); QLineEdit* membersEdit = new QLineEdit();
            membersEdit->setPlaceholderText("alice,bob,charlie");
            form->addRow("Group name",nameEdit); form->addRow("Members",membersEdit);
            QDialogButtonBox* btns = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
            form->addWidget(btns); connect(btns,&QDialogButtonBox::accepted,&d,&QDialog::accept);
            connect(btns,&QDialogButtonBox::rejected,&d,&QDialog::reject);
            if(d.exec()!=QDialog::Accepted) return;
            std::string groupName = nameEdit->text().trimmed().toStdString();
            if(groupName.empty()){ QMessageBox::warning(this,"Create group","Group name required"); return; }
            auto members = splitMembers(membersEdit->text());
            auto groupId = currentClient_->createGroup(groupName, members);
            if(!groupId){ QMessageBox::warning(this,"Create group","Failed"); return; }
            refreshChatList(); selectGroupChat(*groupId); setStatus("Group created: "+groupName);
        });
        connect(chatList_, &QListWidget::itemSelectionChanged, this, [this](){
            QListWidgetItem* item = chatList_->currentItem();
            if(!item) return;
            selectedChatKind_ = static_cast<ChatKind>(item->data(Qt::UserRole).toInt());
            selectedChatId_ = item->data(Qt::UserRole+1).toString().toStdString();
            refreshMessages();
        });
        connect(sendButton_, &QPushButton::clicked, this, &MessengerMainWindow::sendCurrentMessage);
        connect(messageInput_, &QLineEdit::returnPressed, this, &MessengerMainWindow::sendCurrentMessage);
        connect(deleteButton_, &QPushButton::clicked, this, &MessengerMainWindow::deleteSelectedMessage);
    }

    void showAuthPage() { stack_->setCurrentWidget(authPage_); authUsername_->setFocus(); }
    void showChatPage() { stack_->setCurrentWidget(chatPage_); messageInput_->setFocus(); if(currentClient_) currentUserLabel_->setText("Logged in as "+QString::fromStdString(currentClient_->username())); }

    ChatClient* currentClient() const { return currentClient_; }
    ChatClient& ensureClient(const std::string& name) {
        auto it = clients_.find(name);
        if(it==clients_.end()) it = clients_.emplace(name, std::make_unique<ChatClient>(name, server_, keyFactory_, encryption_)).first;
        return *it->second;
    }
    void switchCurrentUser(const std::string& name) {
        if(currentClient_) currentClient_->detach(this);
        currentClient_ = &ensureClient(name);
        currentClient_->attach(this);
        selectedChatId_.clear(); messages_.clear(); messageTable_->clearContents(); messageTable_->setRowCount(0); chatList_->clear();
    }

    void refreshChatList() {
        chatList_->clear();
        if(!currentClient_) return;
        for(const auto& contact : currentClient_->listContacts()){
            QListWidgetItem* item = new QListWidgetItem(QString::fromStdString(contact), chatList_);
            item->setData(Qt::UserRole, static_cast<int>(ChatKind::Direct));
            item->setData(Qt::UserRole+1, QString::fromStdString(contact));
        }
        for(const auto& gid : currentClient_->listGroups()){
            QString groupName = QString::fromStdString(server_.getGroupName(gid));
            QString title = groupName.isEmpty() ? QString::fromStdString(gid) : groupName+" ("+QString::fromStdString(gid)+")";
            QListWidgetItem* item = new QListWidgetItem(title, chatList_);
            item->setData(Qt::UserRole, static_cast<int>(ChatKind::Group));
            item->setData(Qt::UserRole+1, QString::fromStdString(gid));
        }
        if(chatList_->count()>0 && !chatList_->currentItem()) chatList_->setCurrentRow(0);
    }

    void selectDirectChat(const std::string& peer) {
        selectedChatKind_ = ChatKind::Direct; selectedChatId_ = peer;
        for(int i=0;i<chatList_->count();++i){
            QListWidgetItem* item = chatList_->item(i);
            if(item->data(Qt::UserRole).toInt()==static_cast<int>(ChatKind::Direct) && item->data(Qt::UserRole+1).toString().toStdString()==peer){
                chatList_->setCurrentItem(item); break;
            }
        }
        refreshMessages();
    }
    void selectGroupChat(const std::string& gid) {
        selectedChatKind_ = ChatKind::Group; selectedChatId_ = gid;
        for(int i=0;i<chatList_->count();++i){
            QListWidgetItem* item = chatList_->item(i);
            if(item->data(Qt::UserRole).toInt()==static_cast<int>(ChatKind::Group) && item->data(Qt::UserRole+1).toString().toStdString()==gid){
                chatList_->setCurrentItem(item); break;
            }
        }
        refreshMessages();
    }

    void refreshMessages() {
        // Запоминаем выделенное сообщение по ID
        QString selectedId;
        int selectedRow = messageTable_->currentRow();
        if(selectedRow >= 0 && selectedRow < messageTable_->rowCount()) {
            QTableWidgetItem* item = messageTable_->item(selectedRow, 0);
            if(item) selectedId = item->data(Qt::UserRole).toString();
        }
        messageTable_->clearContents();
        messageTable_->setRowCount(0);
        messages_.clear();
        if(!currentClient_ || selectedChatId_.empty()){
            activeChatLabel_->setText("Select a chat or create a new one");
            if(!selectedChatId_.empty()) {
                messageTable_->setRowCount(1);
                messageTable_->setItem(0, 0, new QTableWidgetItem("Select a chat to see messages."));
            }
            return;
        }
        std::vector<MessageView> msgs;
        if(selectedChatKind_==ChatKind::Direct) msgs = currentClient_->getConversation(selectedChatId_);
        else msgs = currentClient_->getGroupConversation(selectedChatId_);
        messages_ = std::move(msgs);
        messageTable_->setRowCount(static_cast<int>(messages_.size()));
        int newSelectedRow = -1;
        for(size_t i=0;i<messages_.size();++i){
            QTableWidgetItem* item = new QTableWidgetItem(formatMessage(messages_[i]));
            item->setData(Qt::UserRole, QString::fromStdString(messages_[i].messageId));
            item->setData(Qt::UserRole+1, QString::fromStdString(messages_[i].fromUser));
            messageTable_->setItem(static_cast<int>(i), 0, item);
            if(!selectedId.isEmpty() && messages_[i].messageId == selectedId.toStdString()) newSelectedRow = static_cast<int>(i);
        }
        messageTable_->resizeColumnToContents(0);
        if(newSelectedRow >= 0) {
            messageTable_->selectRow(newSelectedRow);
        } else if(messageTable_->rowCount() > 0) {
            messageTable_->clearSelection();
        }
        messageTable_->scrollToBottom();
        if(selectedChatKind_==ChatKind::Direct) activeChatLabel_->setText(QString("Direct chat with %1").arg(QString::fromStdString(selectedChatId_)));
        else {
            QString groupName = QString::fromStdString(server_.getGroupName(selectedChatId_));
            activeChatLabel_->setText(groupName.isEmpty() ? QString("Group %1").arg(QString::fromStdString(selectedChatId_)) : QString("%1 (%2)").arg(groupName, QString::fromStdString(selectedChatId_)));
        }
    }

    void sendCurrentMessage() {
        if(!currentClient_){ setStatus("Login first",true); return; }
        QString text = messageInput_->text().trimmed();
        if(text.isEmpty()){ setStatus("Message is empty",true); return; }
        if(selectedChatId_.empty()){ setStatus("Select a chat first",true); return; }
        bool ok = false;
        if(selectedChatKind_==ChatKind::Direct) ok = currentClient_->sendMessage(selectedChatId_, text.toStdString());
        else ok = currentClient_->sendGroupMessage(selectedChatId_, text.toStdString());
        if(!ok){ setStatus("Send failed",true); return; }
        // Локально добавляем сообщение
        MessageView newMsg;
        newMsg.messageId = "temp_" + QString::number(QDateTime::currentMSecsSinceEpoch()).toStdString();
        newMsg.fromUser = currentClient_->username();
        newMsg.toUser = selectedChatKind_==ChatKind::Direct ? selectedChatId_ : "";
        newMsg.chatId = selectedChatId_;
        newMsg.text = text.toStdString();
        newMsg.timestamp = QDateTime::currentMSecsSinceEpoch();
        newMsg.status = MessageStatus::Sent;
        newMsg.isGroupMessage = (selectedChatKind_==ChatKind::Group);
        messages_.push_back(newMsg);
        int row = messageTable_->rowCount();
        messageTable_->insertRow(row);
        QTableWidgetItem* item = new QTableWidgetItem(formatMessage(newMsg));
        item->setData(Qt::UserRole, QString::fromStdString(newMsg.messageId));
        item->setData(Qt::UserRole+1, QString::fromStdString(newMsg.fromUser));
        messageTable_->setItem(row, 0, item);
        messageTable_->scrollToBottom();
        messageInput_->clear();
        setStatus("Message sent");
    }

    void deleteSelectedMessage() {
        if(!currentClient_){ setStatus("Login first",true); return; }
        int row = messageTable_->currentRow();
        if(row < 0 || row >= messageTable_->rowCount()){ setStatus("Select a message first",true); return; }
        QTableWidgetItem* item = messageTable_->item(row, 0);
        if(!item) return;
        std::string msgId = item->data(Qt::UserRole).toString().toStdString();
        if(msgId.empty() || msgId.substr(0,5)=="temp_"){ setStatus("Cannot delete temporary message",true); return; }
        if(!currentClient_->deleteMessageForAll(msgId)){ setStatus("Delete failed",true); return; }
        // Удаляем из локального списка
        for(auto it=messages_.begin(); it!=messages_.end(); ++it){
            if(it->messageId == msgId){ messages_.erase(it); break; }
        }
        messageTable_->removeRow(row);
        // Выделяем следующий или предыдущий
        if(row >= messageTable_->rowCount()) row = messageTable_->rowCount() - 1;
        if(row >= 0) messageTable_->selectRow(row);
        setStatus("Message deleted for all");
    }

    void setStatus(const std::string& text, bool error=false){
        statusLabel_->setText(QString::fromStdString(text));
        statusLabel_->setStyleSheet(error ? "color: #b00020;" : "color: #2b7a0b;");
    }

private:
    QStackedWidget* stack_ = nullptr;
    QWidget* authPage_ = nullptr; QWidget* chatPage_ = nullptr;
    QLineEdit* authUsername_ = nullptr; QLineEdit* authPassword_ = nullptr;
    QPushButton* registerButton_ = nullptr; QPushButton* loginButton_ = nullptr;
    QLabel* authHint_ = nullptr;
    QLabel* currentUserLabel_ = nullptr;
    QPushButton* refreshButton_ = nullptr; QPushButton* logoutButton_ = nullptr;
    QPushButton* newChatButton_ = nullptr; QPushButton* createGroupButton_ = nullptr;
    QListWidget* chatList_ = nullptr; QLabel* activeChatLabel_ = nullptr;
    QTableWidget* messageTable_ = nullptr; QLineEdit* messageInput_ = nullptr;
    QPushButton* sendButton_ = nullptr; QPushButton* deleteButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;

    ChatServer server_;
    OpenSSLKeyFactory keyFactory_;
    AES256GCMStrategy encryption_;
    std::unordered_map<std::string, std::unique_ptr<ChatClient>> clients_;
    ChatClient* currentClient_ = nullptr;
    std::string selectedChatId_;
    ChatKind selectedChatKind_ = ChatKind::Direct;
    std::vector<MessageView> messages_;
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    MessengerMainWindow w;
    w.show();
    return app.exec();
}