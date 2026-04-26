#pragma once

#include "encryption/KeyFactory.h"

class OpenSSLKeyFactory final : public KeyFactory {
public:
	KeyPair generateRsaKeyPair(int bits = 2048) override;
	std::vector<uint8_t> generateSymmetricKey(std::size_t size = 32) override;

	std::vector<uint8_t> encryptWithPublicKey(const std::string& publicKeyPem,
											  const std::vector<uint8_t>& data) override;

	std::vector<uint8_t> decryptWithPrivateKey(const std::string& privateKeyPem,
											   const std::vector<uint8_t>& encryptedData) override;
};
