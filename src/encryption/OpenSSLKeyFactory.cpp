#include "encryption/OpenSSLKeyFactory.h"

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>

#include <memory>
#include <stdexcept>

namespace {
using EvpPkeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using EvpPkeyCtxPtr = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
using BioPtr = std::unique_ptr<BIO, decltype(&BIO_free)>;

std::runtime_error opensslError(const char* message) {
	return std::runtime_error(message);
}
}

KeyPair OpenSSLKeyFactory::generateRsaKeyPair(int bits) {
	EvpPkeyCtxPtr keygenCtx(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr), EVP_PKEY_CTX_free);
	if (!keygenCtx) {
		throw opensslError("Failed to create RSA keygen context");
	}

	if (EVP_PKEY_keygen_init(keygenCtx.get()) <= 0 ||
		EVP_PKEY_CTX_set_rsa_keygen_bits(keygenCtx.get(), bits) <= 0) {
		throw opensslError("Failed to initialize RSA key generation");
	}

	EVP_PKEY* rawKey = nullptr;
	if (EVP_PKEY_keygen(keygenCtx.get(), &rawKey) <= 0) {
		throw opensslError("Failed to generate RSA key pair");
	}
	EvpPkeyPtr pkey(rawKey, EVP_PKEY_free);

	BioPtr pubBio(BIO_new(BIO_s_mem()), BIO_free);
	BioPtr privBio(BIO_new(BIO_s_mem()), BIO_free);
	if (!pubBio || !privBio) {
		throw opensslError("Failed to allocate BIO");
	}

	if (PEM_write_bio_PUBKEY(pubBio.get(), pkey.get()) <= 0 ||
		PEM_write_bio_PrivateKey(privBio.get(), pkey.get(), nullptr, nullptr, 0, nullptr, nullptr) <= 0) {
		throw opensslError("Failed to write PEM key pair");
	}

	const char* pubPtr = nullptr;
	long pubLen = BIO_get_mem_data(pubBio.get(), &pubPtr);
	const char* privPtr = nullptr;
	long privLen = BIO_get_mem_data(privBio.get(), &privPtr);

	if (!pubPtr || !privPtr || pubLen <= 0 || privLen <= 0) {
		throw opensslError("Failed to extract PEM from BIO");
	}

	return {std::string(pubPtr, static_cast<std::size_t>(pubLen)),
			std::string(privPtr, static_cast<std::size_t>(privLen))};
}

std::vector<uint8_t> OpenSSLKeyFactory::generateSymmetricKey(std::size_t size) {
	std::vector<uint8_t> key(size);
	if (RAND_bytes(key.data(), static_cast<int>(size)) != 1) {
		throw opensslError("RAND_bytes failed");
	}
	return key;
}

std::vector<uint8_t> OpenSSLKeyFactory::encryptWithPublicKey(const std::string& publicKeyPem,
															 const std::vector<uint8_t>& data) {
	BioPtr bio(BIO_new_mem_buf(publicKeyPem.data(), static_cast<int>(publicKeyPem.size())), BIO_free);
	if (!bio) {
		throw opensslError("Failed to create BIO for public key");
	}

	EvpPkeyPtr pkey(PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr), EVP_PKEY_free);
	if (!pkey) {
		throw opensslError("Failed to parse public key PEM");
	}

	EvpPkeyCtxPtr ctx(EVP_PKEY_CTX_new(pkey.get(), nullptr), EVP_PKEY_CTX_free);
	if (!ctx || EVP_PKEY_encrypt_init(ctx.get()) <= 0) {
		throw opensslError("Failed to init public key encryption");
	}

	if (EVP_PKEY_CTX_set_rsa_padding(ctx.get(), RSA_PKCS1_OAEP_PADDING) <= 0) {
		throw opensslError("Failed to set RSA OAEP padding");
	}

	std::size_t outLen = 0;
	if (EVP_PKEY_encrypt(ctx.get(), nullptr, &outLen, data.data(), data.size()) <= 0) {
		throw opensslError("Failed to compute encrypted size");
	}

	std::vector<uint8_t> out(outLen);
	if (EVP_PKEY_encrypt(ctx.get(), out.data(), &outLen, data.data(), data.size()) <= 0) {
		throw opensslError("Public key encryption failed");
	}
	out.resize(outLen);
	return out;
}

std::vector<uint8_t> OpenSSLKeyFactory::decryptWithPrivateKey(const std::string& privateKeyPem,
															  const std::vector<uint8_t>& encryptedData) {
	BioPtr bio(BIO_new_mem_buf(privateKeyPem.data(), static_cast<int>(privateKeyPem.size())), BIO_free);
	if (!bio) {
		throw opensslError("Failed to create BIO for private key");
	}

	EvpPkeyPtr pkey(PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr), EVP_PKEY_free);
	if (!pkey) {
		throw opensslError("Failed to parse private key PEM");
	}

	EvpPkeyCtxPtr ctx(EVP_PKEY_CTX_new(pkey.get(), nullptr), EVP_PKEY_CTX_free);
	if (!ctx || EVP_PKEY_decrypt_init(ctx.get()) <= 0) {
		throw opensslError("Failed to init private key decryption");
	}

	if (EVP_PKEY_CTX_set_rsa_padding(ctx.get(), RSA_PKCS1_OAEP_PADDING) <= 0) {
		throw opensslError("Failed to set RSA OAEP padding");
	}

	std::size_t outLen = 0;
	if (EVP_PKEY_decrypt(ctx.get(), nullptr, &outLen, encryptedData.data(), encryptedData.size()) <= 0) {
		throw opensslError("Failed to compute decrypted size");
	}

	std::vector<uint8_t> out(outLen);
	if (EVP_PKEY_decrypt(ctx.get(), out.data(), &outLen, encryptedData.data(), encryptedData.size()) <= 0) {
		throw opensslError("Private key decryption failed");
	}
	out.resize(outLen);
	return out;
}

