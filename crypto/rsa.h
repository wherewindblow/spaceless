/**
 * rsa.h
 * @author wherewindblow
 * @date   Jul 24, 2016
 * @note Depend on cryptopp library.
 */

#pragma once

#include <memory>
#include <string>
#include <cryptopp/rsa.h>
#include <cryptopp/osrng.h>


namespace spaceless {
namespace crypto {

struct RsaKeyPair;


/**
 * Private key handle.
 */
class RsaPrivateKey
{
public:
	/**
	 * Saves private key to file.
	 * @note Will save in hex.
	 */
	void save_to_file(const std::string& filename) const;

	/**
	 * Loads private key from file.
	 * @note File content must is hex and save by another PrivateKey.
	 */
	void load_from_file(const std::string& filename);

	/**
	 * Saves private key to hex string.
	 */
	std::string save_to_string() const;

	/**
	 * Loads private key from hex string.
	 * @note String content must is hex and save by another PrivateKey.
	 */
	void load_from_string(const std::string& hex);

private:
	friend RsaKeyPair generate_rsa_key_pair(unsigned int key_length);

	friend std::string rsa_decrypt(const std::string& cipher, const RsaPrivateKey& key);

	CryptoPP::RSA::PrivateKey p_impl;
};


/**
 * Public key handle.
 */
class RsaPublicKey
{
public:
	/**
	 * Saves public key to file.
	 * @note Will save in hex.
	 */
	void save_to_file(const std::string& filename) const;

	/**
	 * Loads public key from file.
	 * @note File content must is hex and save by another PublicKey.
	 */
	void load_from_file(const std::string& filename);

	/**
	 * Saves public key to hex string.
	 */
	std::string save_to_string() const;

	/**
	 * Loads public key from hex string.
	 * @note String content must is hex and save by another PublicKey.
	 */
	void load_from_string(const std::string& hex);

private:
	friend RsaKeyPair generate_rsa_key_pair(unsigned int key_length);

	friend std::string rsa_encrypt(const std::string& plain, const RsaPublicKey& key);

	CryptoPP::RSA::PublicKey p_impl;
};


/**
 * PrivateKey and PublicKey group.
 */
struct RsaKeyPair
{
	RsaKeyPair() = default;
	RsaKeyPair(RsaKeyPair&& keys);
	const RsaKeyPair& operator= (RsaKeyPair&& keys);

	RsaPrivateKey private_key;
	RsaPublicKey public_key;
};


/**
 * Generates a rsa key pair.
 * @param key_length  Mod value bit length [(public key + mod value), (private key + mod value)].
 * @return Public key and private key.
 */
RsaKeyPair generate_rsa_key_pair(unsigned int key_length = 1024);


/**
 * Uses RSA method to encrypt string.
 */
std::string rsa_encrypt(const std::string& plain, const RsaPublicKey& key);


/**
 * Uses RSA method to decrypt string.
 */
std::string rsa_decrypt(const std::string& cipher, const RsaPrivateKey& key);

} // namespace crypto
} // namespace spaceless