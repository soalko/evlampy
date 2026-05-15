# Матрица unit-тестов

| Класс | Файл тестов | Количество тестов | Покрытые функции |
|---|---|---:|---|
| `Message` | `tests/unit/chat/MessageTests.cpp` | 7 | `build(...)` |
| `MessageProtocol` | `tests/unit/network/MessageProtocolTests.cpp` | 7 | `serialize`, `deserialize`, `base64Encode`, `base64Decode` |
| `TcpServer` | `tests/unit/network/TcpServerTests.cpp` | 7 | `connectClient`, `disconnectClient`, `sendTo`, `isOnline`, `listOnlineUsers` |
| `TcpClient` | `tests/unit/network/TcpClientTests.cpp` | 7 | `setOnMessage`, `connect`, `disconnect`, `send`, `isConnected`, `username` |
| `AES256GCMStrategy` | `tests/unit/encryption/AES256GCMStrategyTests.cpp` | 7 | `encrypt`, `decrypt` |
| `OpenSSLKeyFactory` | `tests/unit/encryption/OpenSSLKeyFactoryTests.cpp` | 7 | `generateRsaKeyPair`, `generateSymmetricKey`, `encryptWithPublicKey`, `decryptWithPrivateKey` |
| `ChatServer` | `tests/unit/chat/ChatServerTests.cpp` | 12 | регистрация/аутентификация, контакты, сообщения, группы, аудит, admin-функции |
| `ChatClient` | `tests/unit/chat/ChatClientTests.cpp` | 11 | `registerOnServer`, `login`, `logout`, `sendMessage`, `sendGroupMessage`, `deleteMessageForAll`, контакты, группы, списки |
| `Observer`/`Subject` | `tests/unit/patterns/ObserverTests.cpp` | 7 | `attach`, `detach`, `notify` |
| `SendMessageCommand` + `Logger` | `tests/unit/patterns/CommandLoggerTests.cpp` | 7 | `execute`, `Logger::instance`, `info`, `error` |

Итого: 79 unit-тестов.

