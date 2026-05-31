#include "encryption/AES256GCMStrategy.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <stdexcept>

static const int GCM_IV_LEN = 12;
static const int GCM_TAG_LEN = 16;

std::vector<uint8_t> AES256GCMStrategy::encrypt(const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& key) {
    if (key.size() != 32) throw std::invalid_argument("AES-256 requires 32-byte key");

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_CIPHER_CTX_new failed");

    std::vector<uint8_t> iv(GCM_IV_LEN);
    if (RAND_bytes(iv.data(), GCM_IV_LEN) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("RAND_bytes failed");
    }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, key.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_EncryptInit_ex failed");
    }

    std::vector<uint8_t> ciphertext(plaintext.size() + GCM_TAG_LEN);
    int len = 0;
    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, plaintext.data(), plaintext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_EncryptUpdate failed");
    }
    int ciphertext_len = len;

    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_EncryptFinal_ex failed");
    }
    ciphertext_len += len;

    std::vector<uint8_t> tag(GCM_TAG_LEN);
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, GCM_TAG_LEN, tag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_CTRL_GCM_GET_TAG failed");
    }
    EVP_CIPHER_CTX_free(ctx);

    // Формат: [IV (12 байт)] [шифротекст] [tag (16 байт)]
    std::vector<uint8_t> result;
    result.insert(result.end(), iv.begin(), iv.end());
    result.insert(result.end(), ciphertext.begin(), ciphertext.begin() + ciphertext_len);
    result.insert(result.end(), tag.begin(), tag.end());
    return result;
}

std::vector<uint8_t> AES256GCMStrategy::decrypt(const std::vector<uint8_t>& ciphertext,
                                                const std::vector<uint8_t>& key) {
    if (key.size() != 32) {
        throw std::invalid_argument("AES-256 requires 32-byte key");
    }
    if (ciphertext.size() < static_cast<std::size_t>(GCM_IV_LEN + GCM_TAG_LEN)) {
        throw std::invalid_argument("Ciphertext is too short for AES-GCM envelope");
    }

    const auto ivBegin = ciphertext.begin();
    const auto ctBegin = ivBegin + GCM_IV_LEN;
    const auto tagBegin = ciphertext.end() - GCM_TAG_LEN;

    std::vector<uint8_t> iv(ivBegin, ctBegin);
    std::vector<uint8_t> tag(tagBegin, ciphertext.end());
    std::vector<uint8_t> encrypted(ctBegin, tagBegin);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw std::runtime_error("EVP_CIPHER_CTX_new failed");
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, key.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_DecryptInit_ex failed");
    }

    std::vector<uint8_t> plaintext(encrypted.size());
    int len = 0;
    if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, encrypted.data(), static_cast<int>(encrypted.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_DecryptUpdate failed");
    }
    int plaintextLen = len;

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, GCM_TAG_LEN, tag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_CTRL_GCM_SET_TAG failed");
    }

    const int finalOk = EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len);
    EVP_CIPHER_CTX_free(ctx);

    if (finalOk != 1) {
        throw std::runtime_error("AES-GCM tag verification failed");
    }
    plaintextLen += len;
    plaintext.resize(static_cast<std::size_t>(plaintextLen));
    return plaintext;
}
