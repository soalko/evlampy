#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct KeyPair {
	std::string publicKeyPem;
	std::string privateKeyPem;
};

class KeyFactory {
public:
	virtual ~KeyFactory() = default;

	virtual KeyPair generateRsaKeyPair(int bits = 2048) = 0;
	virtual std::vector<uint8_t> generateSymmetricKey(std::size_t size = 32) = 0;

	virtual std::vector<uint8_t> encryptWithPublicKey(const std::string& publicKeyPem,
													  const std::vector<uint8_t>& data) = 0;

	virtual std::vector<uint8_t> decryptWithPrivateKey(const std::string& privateKeyPem,
													   const std::vector<uint8_t>& encryptedData) = 0;
};
