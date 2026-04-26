#pragma once
#include "IEncryptionStrategy.h"
#include <vector>
#include <cstdint>

class AES256GCMStrategy : public IEncryptionStrategy {
public:
    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& key) override;
    std::vector<uint8_t> decrypt(const std::vector<uint8_t>& ciphertext, const std::vector<uint8_t>& key) override;
};