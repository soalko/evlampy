#include "encryption/AES256GCMStrategy.h"

#include <gtest/gtest.h>

namespace {
std::vector<uint8_t> toBytes(const std::string& s) {
    return {s.begin(), s.end()};
}
}

TEST(AES256GCMStrategyTests, EncryptDecryptRoundtripText) {
    AES256GCMStrategy strategy;
    const std::vector<uint8_t> key(32, 7);
    const auto plaintext = toBytes("hello evlampy");

    const auto cipher = strategy.encrypt(plaintext, key);
    const auto decoded = strategy.decrypt(cipher, key);

    EXPECT_EQ(decoded, plaintext);
}

TEST(AES256GCMStrategyTests, EncryptDecryptRoundtripEmptyPayload) {
    AES256GCMStrategy strategy;
    const std::vector<uint8_t> key(32, 3);
    const std::vector<uint8_t> plaintext;

    const auto cipher = strategy.encrypt(plaintext, key);
    const auto decoded = strategy.decrypt(cipher, key);

    EXPECT_TRUE(decoded.empty());
}

TEST(AES256GCMStrategyTests, EncryptRejectsInvalidKeySize) {
    AES256GCMStrategy strategy;
    const std::vector<uint8_t> badKey(16, 1);

    EXPECT_THROW((void)strategy.encrypt(toBytes("abc"), badKey), std::invalid_argument);
}

TEST(AES256GCMStrategyTests, DecryptRejectsInvalidKeySize) {
    AES256GCMStrategy strategy;
    const std::vector<uint8_t> key(32, 1);
    const auto cipher = strategy.encrypt(toBytes("abc"), key);

    EXPECT_THROW((void)strategy.decrypt(cipher, std::vector<uint8_t>(31, 2)), std::invalid_argument);
}

TEST(AES256GCMStrategyTests, DecryptRejectsTooShortEnvelope) {
    AES256GCMStrategy strategy;
    const std::vector<uint8_t> key(32, 1);

    EXPECT_THROW((void)strategy.decrypt(std::vector<uint8_t>(10, 0), key), std::invalid_argument);
}

TEST(AES256GCMStrategyTests, DecryptRejectsTamperedCiphertext) {
    AES256GCMStrategy strategy;
    const std::vector<uint8_t> key(32, 9);
    auto cipher = strategy.encrypt(toBytes("integrity"), key);
    cipher.back() ^= 0xFF;

    EXPECT_THROW((void)strategy.decrypt(cipher, key), std::runtime_error);
}

TEST(AES256GCMStrategyTests, EncryptUsesRandomIv) {
    AES256GCMStrategy strategy;
    const std::vector<uint8_t> key(32, 4);
    const auto plaintext = toBytes("same-text");

    const auto c1 = strategy.encrypt(plaintext, key);
    const auto c2 = strategy.encrypt(plaintext, key);

    EXPECT_NE(c1, c2);
}

