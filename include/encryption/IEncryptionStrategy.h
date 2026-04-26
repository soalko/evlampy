#pragma once

#include <cstdint>
#include <vector>

class IEncryptionStrategy {
public:
	virtual ~IEncryptionStrategy() = default;

	virtual std::vector<uint8_t> encrypt(const std::vector<uint8_t>& plaintext,
										 const std::vector<uint8_t>& key) = 0;

	virtual std::vector<uint8_t> decrypt(const std::vector<uint8_t>& ciphertext,
										 const std::vector<uint8_t>& key) = 0;
};

