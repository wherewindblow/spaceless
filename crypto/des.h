/**
 * des.h
 * @author wherewindblow
 * @date   Oct 08, 2016
 * @note Depend on cryptopp library.
 */

#pragma once

#include <array>
#include <string>
#include <iosfwd>

#include <cryptopp/des.h>

#include "basic.h"


namespace spaceless {
namespace crypto {

enum class DesKeyBits
{
	BITS_64 = 8
};


/**
 * DES valid key.
 */
class DesKey
{
public:
	/**
	 * @note value will resize according to bits. If value.length() * 8 is longer
	 *     than specified bits, value will be trim to fix.
	 */
	DesKey(const std::string& value);

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
	DesKeyBits get_bits() const
	{
		return DesKey::bits;
	}

private:
	std::string m_value;
	static DesKeyBits bits;
};


constexpr std::size_t DES_BLOCK_SIZE = static_cast<std::size_t>(CryptoPP::DES::BLOCKSIZE);

using DesBlock = std::array<byte, DES_BLOCK_SIZE>;


/**
 * Encrypt data with a fixed block with DES.
 */
class DesBlockEncryptor
{
public:
	/**
	 * Sets encryption key.
	 */
	void set_key(const DesKey& key)
	{
		auto& key_value = key.get_value();
		m_encryption.SetKey(reinterpret_cast<const byte*>(key_value.data()), key_value.size());
	}

	/**
	 * Encrypt plain content.
	 */
	DesBlock encrypt(const DesBlock& plain) const
	{
		DesBlock cipher;
		m_encryption.ProcessBlock(plain.data(), cipher.data());
		return cipher;
	}

	/**
	 * Encrypt data from @c in_block to @c out_block
	 * @note The size of @a in_block and @a out_block is @c DES_BLOCK_SIZE.
	 */
	void encrypt(const byte* in_block, byte* out_block) const
	{
		m_encryption.ProcessBlock(in_block, out_block);
	}

	/**
	 * Encrypt data from @c in_block to @c out_block
	 * @note The size of @a in_block and @a out_block is @c DES_BLOCK_SIZE.
	 */
	void encrypt(const char* in_block, char* out_block) const
	{
		this->encrypt(reinterpret_cast<const byte*>(in_block), reinterpret_cast<byte*>(out_block));
	}

private:
	CryptoPP::DES::Encryption m_encryption;
};


/**
 * Decrypt data with a fixed block.
 */
class DesBlockDecryptor
{
public:
	/**
	 * Sets decryption key.
	 */
	void set_key(const DesKey& key)
	{
		auto& key_value = key.get_value();
		m_decryption.SetKey(reinterpret_cast<const byte*>(key_value.data()), key_value.size());
	}

	/**
	 * Decrypt cipher content.
	 */
	DesBlock decrypt(const DesBlock& cipher) const
	{
		DesBlock plain;
		m_decryption.ProcessBlock(cipher.data(), plain.data());
		return plain;
	}

	/**
	 * Decrypt data from @c in_block to @c out_block
	 * @note The size of @a in_block and @a out_block is @c DES_BLOCK_SIZE.
	 */
	void decrypt(const byte* in_block, byte* out_block) const
	{
		m_decryption.ProcessBlock(in_block, out_block);
	}

	/**
	 * Decrypt data from @c in_block to @c out_block
	 * @note The size of @a in_block and @a out_block is @c DES_BLOCK_SIZE.
	 */
	void decrypt(const char* in_block, char* out_block) const
	{
		this->decrypt(reinterpret_cast<const byte*>(in_block), reinterpret_cast<byte*>(out_block));
	}

private:
	CryptoPP::DES::Decryption m_decryption;
};


/**
 * Uses DES method to encrypt string.
 */
std::string des_encrypt(const std::string& plain, const DesKey& key);

/**
 * Uses DES method to decrypt string.
 */
std::string des_decrypt(const std::string& cipher, const DesKey& key, bool shrink);

/**
 * Uses DES method to encrypt data from @c in to @c out.
 */
void des_encrypt(std::istream& in, std::ostream& out, const DesKey& key);

/**
 * Uses DES method to decrypt data from @c in to @c out.
 * @throw Throw exception if input data is not complete,
 *        data must to be multiple of @c DES_BLOCK_SIZE.
 */
void des_decrypt(std::istream& in, std::ostream& out, const DesKey& key);

/**
 * Uses DES method to encrypt data from @c in_filename to @c out_filename.
 * @throw Throw exception if open file failure.
 */
void des_encrypt_file(const std::string& in_filename, const std::string& out_filename, const DesKey& key);

/**
 * Uses DES method to decrypt data from @c in_filename to @c out_filename.
 * @throw Throw exception condition.
 *        1) If open file failure.
 *        2) If input data is not complete, data must to be multiple of @c DES_BLOCK_SIZE.
 */
void des_decrypt_file(const std::string& in_filename, const std::string& out_filename, const DesKey& key);

} // namespace crypto
} // namespace spaceless