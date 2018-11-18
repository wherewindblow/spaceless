/**
 * aes.h
 * @author wherewindblow
 * @date   Sep 04, 2016
 * @note Depend on cryptopp library.
 */

#pragma once

#include <array>
#include <string>
#include <iosfwd>

#include <cryptopp/aes.h>
#include <lights/sequence.h>

#include "basic.h"


namespace spaceless {
namespace crypto {

/**
 * AES key bits.
 */
enum class AesKeyBits
{
	BITS_128 = 16,
	BITS_192 = 24,
	BITS_256 = 32
};


/**
 * AES valid key.
 */
class AesKey
{
public:
	/**
	 * Creates AES key by given value.
	 * @note value will resize according to bits. If value.length() * 8 is longer
	 *     than specified bits, value will be trim to fix.
	 */
	AesKey(lights::StringView value, AesKeyBits bits);

	/**
	 * Creates random AES key.
	 */
	AesKey(AesKeyBits bits);

	/**
	 * Resets to given AES Key.
	 */
	void reset(lights::StringView value, AesKeyBits bits);

	/**
	 * Resets to random AES key.
	 */
	void reset(AesKeyBits bits);

	/**
	 * Gets key value.
	 */
	const std::string& get_value() const
	{
		return m_value;
	}

	/**
	 * Gets key bits.
	 */
	AesKeyBits get_bits() const
	{
		return m_bits;
	}

private:
	std::string m_value;
	AesKeyBits m_bits;
};


constexpr std::size_t AES_BLOCK_SIZE = static_cast<std::size_t>(CryptoPP::AES::BLOCKSIZE);

using AesBlock = std::array<byte, AES_BLOCK_SIZE>;


/**
 * Encrypt data with a fixed block with AES.
 */
class AesBlockEncryptor
{
public:
	/**
	 * Sets encryption key.
	 */
	void set_key(const AesKey& key)
	{
		auto& key_value = key.get_value();
		m_encryption.SetKey(reinterpret_cast<const byte*>(key_value.data()), key_value.size());
	}

	/**
	 * Encrypt plain content.
	 */
	AesBlock encrypt(const AesBlock& plain) const
	{
		AesBlock cipher;
		m_encryption.ProcessBlock(plain.data(), cipher.data());
		return cipher;
	}

	/**
	 * Encrypt data from @c in_block to @c out_block
	 * @note The size of @a in_block and @a out_block is @c AES_BLOCK_SIZE.
	 */
	void encrypt(const byte* in_block, byte* out_block) const
	{
		m_encryption.ProcessBlock(in_block, out_block);
	}

	/**
	 * Encrypt data from @c in_block to @c out_block
	 * @note The size of @a in_block and @a out_block is @c AES_BLOCK_SIZE.
	 */
	void encrypt(const char* in_block, char* out_block) const
	{
		this->encrypt(reinterpret_cast<const byte*>(in_block), reinterpret_cast<byte*>(out_block));
	}

	/**
	 * Encrypt data in place.
	 * @note The size of @a in_out_block is @c AES_BLOCK_SIZE.
	 */
	void encrypt(char* in_out_block)
	{
		m_encryption.ProcessBlock(reinterpret_cast<byte*>(in_out_block));
	}

private:
	CryptoPP::AES::Encryption m_encryption;
};


/**
 * Decrypt data with a fixed block.
 */
class AesBlockDecryptor
{
public:
	/**
	 * Sets decryption key.
	 */
	void set_key(const AesKey& key)
	{
		auto& key_value = key.get_value();
		m_decryption.SetKey(reinterpret_cast<const byte*>(key_value.data()), key_value.size());
	}

	/**
	 * Decrypt cipher content.
	 */
	AesBlock decrypt(const AesBlock& cipher) const
	{
		AesBlock plain;
		m_decryption.ProcessBlock(cipher.data(), plain.data());
		return plain;
	}

	/**
	 * Decrypt data from @c in_block to @c out_block
	 * @note The size of @a in_block and @a out_block is @c AES_BLOCK_SIZE.
	 */
	void decrypt(const byte* in, byte* out) const
	{
		m_decryption.ProcessBlock(in, out);
	}

	/**
	 * Decrypt data from @c in_block to @c out_block
	 * @note The size of @a in_block and @a out_block is @c AES_BLOCK_SIZE.
	 */
	void decrypt(const char* in, char* out) const
	{
		this->decrypt(reinterpret_cast<const byte*>(in), reinterpret_cast<byte*>(out));
	}

	/**
	 * Decrypt data in place.
	 * @note The size of @a in_out_block is @c AES_BLOCK_SIZE.
	 */
	void decrypt(char* in_out_block)
	{
		m_decryption.ProcessBlock(reinterpret_cast<byte*>(in_out_block));
	}

private:
	CryptoPP::AES::Decryption m_decryption;
};


/**
 * Returns AES cipher length.
 */
std::size_t aes_cipher_length(std::size_t plain_length);

/**
 * Uses AES method to encrypt plain to cipher.
 */
void aes_encrypt(lights::SequenceView plain, lights::Sequence cipher, const AesKey& key);

/**
 * Uses AES method to decrypt cipher to plain.
 */
void aes_decrypt(lights::SequenceView cipher, lights::Sequence plain, const AesKey& key);

/**
 * Uses AES method to encrypt string.
 * @note plain string cannot include null character.
 */
std::string aes_encrypt(const std::string& plain, const AesKey& key);

/**
 * Uses AES method to decrypt string.
 * @note plain string cannot include null character.
 */
std::string aes_decrypt(const std::string& cipher, const AesKey& key, bool shrink = false);

/**
 * Uses AES method to encrypt data from @c in to @c out.
 */
void aes_encrypt(std::istream& in, std::ostream& out, const AesKey& key);

/**
 * Uses AES method to decrypt data from @c in to @c out.
 * @throw Throw exception if input data is not complete,
 *        data must to be multiple of @c AES_BLOCK_SIZE.
 */
void aes_decrypt(std::istream& in, std::ostream& out, const AesKey& key, bool shrink = false);

/**
 * Uses AES method to encrypt data from @c in_filename to @c out_filename.
 * @throw Throw exception if open file failure.
 */
void aes_encrypt_file(const std::string& in_filename, const std::string& out_filename, const AesKey& key);

/**
 * Uses AES method to decrypt data from @c in_filename to @c out_filename.
 * @throw Throw exception condition.
 *        1) If open file failure.
 *        2) If input data is not complete, data must to be multiple of @c AES_BLOCK_SIZE.
 */
void aes_decrypt_file(const std::string& in_filename, const std::string& out_filename, const AesKey& key);

} // namespace crypto
} // namespace spaceless