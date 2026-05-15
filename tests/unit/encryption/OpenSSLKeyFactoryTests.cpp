#include "encryption/OpenSSLKeyFactory.h"

#include <gtest/gtest.h>

TEST(OpenSSLKeyFactoryTests, GenerateRsaPairHasPemHeaders) {
    OpenSSLKeyFactory factory;
    const KeyPair pair = factory.generateRsaKeyPair();

    EXPECT_NE(pair.publicKeyPem.find("BEGIN PUBLIC KEY"), std::string::npos);
    EXPECT_NE(pair.privateKeyPem.find("BEGIN PRIVATE KEY"), std::string::npos);
}

TEST(OpenSSLKeyFactoryTests, GenerateSymmetricKeyDefaultSizeIs32) {
    OpenSSLKeyFactory factory;
    const auto key = factory.generateSymmetricKey();
    EXPECT_EQ(key.size(), 32u);
}

TEST(OpenSSLKeyFactoryTests, GenerateSymmetricKeyCustomSize) {
    OpenSSLKeyFactory factory;
    const auto key = factory.generateSymmetricKey(64);
    EXPECT_EQ(key.size(), 64u);
}

TEST(OpenSSLKeyFactoryTests, EncryptDecryptRoundtrip) {
    OpenSSLKeyFactory factory;
    const auto pair = factory.generateRsaKeyPair();
    const std::vector<uint8_t> payload = {1, 2, 3, 4, 5, 6, 7, 8};

    const auto encrypted = factory.encryptWithPublicKey(pair.publicKeyPem, payload);
    const auto decrypted = factory.decryptWithPrivateKey(pair.privateKeyPem, encrypted);

    EXPECT_EQ(decrypted, payload);
}

TEST(OpenSSLKeyFactoryTests, EncryptWithInvalidPublicKeyThrows) {
    OpenSSLKeyFactory factory;
    const std::vector<uint8_t> payload = {1, 2, 3};

    EXPECT_THROW((void)factory.encryptWithPublicKey("invalid-pem", payload), std::runtime_error);
}

TEST(OpenSSLKeyFactoryTests, DecryptWithInvalidPrivateKeyThrows) {
    OpenSSLKeyFactory factory;
    const auto pair = factory.generateRsaKeyPair();
    const std::vector<uint8_t> payload = {9, 8, 7};
    const auto encrypted = factory.encryptWithPublicKey(pair.publicKeyPem, payload);

    EXPECT_THROW((void)factory.decryptWithPrivateKey("invalid-private-key", encrypted), std::runtime_error);
}

TEST(OpenSSLKeyFactoryTests, OaepProducesDifferentCiphertextForSameInput) {
    OpenSSLKeyFactory factory;
    const auto pair = factory.generateRsaKeyPair();
    const std::vector<uint8_t> payload(16, 0xAB);

    const auto c1 = factory.encryptWithPublicKey(pair.publicKeyPem, payload);
    const auto c2 = factory.encryptWithPublicKey(pair.publicKeyPem, payload);

    EXPECT_NE(c1, c2);
}

