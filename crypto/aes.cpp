/**
 * aes.cpp
 * @author wherewindblow
 * @date   Sep 04, 2016
 */

#include "aes.h"
#include "rng.h"

#include <algorithm>
#include <fstream>

#include <lights/common.h>
#include <lights/file.h>
#include <foundation/basics.h>
#include <foundation/exception.h>


namespace spaceless {
namespace crypto {

AesKey::AesKey(lights::StringView value, AesKeyBits bits)
{
	reset(value, bits);
}


AesKey::AesKey(AesKeyBits bits)
{
	reset(bits);
}


void AesKey::reset(lights::StringView value, AesKeyBits bits)
{
	m_bits = bits;
	m_value.resize(static_cast<std::size_t>(bits));
	std::size_t len = std::min(m_value.length(), value.length());
	m_value.replace(0, len, value.data(), len);
}


void AesKey::reset(AesKeyBits bits)
{
	m_bits = bits;
	m_value.resize(static_cast<std::size_t>(bits));
	get_default_rng().GenerateBlock(reinterpret_cast<byte*>(m_value.data()), m_value.size());
}


template <typename Istream>
inline Istream& read(Istream& in, byte* data, std::size_t size)
{
	return in.read(reinterpret_cast<char*>(data), size);
}

template <typename Istream>
inline Istream& read(Istream& in, AesBlock& block)
{
	return read(in, block.data(), block.size());
}

template <typename Ostream>
inline Ostream& write(Ostream& out, const byte* data, std::size_t size)
{
	return out.write(reinterpret_cast<const char*>(data), size);
}

template <typename Ostream>
inline Ostream& write(Ostream& out, const AesBlock& block)
{
	return write(out, block.data(), block.size());
}


std::size_t aes_cipher_length(std::size_t plain_length)
{
	std::size_t over = plain_length % AES_BLOCK_SIZE;
	std::size_t pad_length = (over == 0) ? 0 : AES_BLOCK_SIZE - over;
	return plain_length + pad_length;
}


void aes_encrypt(lights::SequenceView plain, lights::Sequence cipher, const AesKey& key)
{
	std::size_t over = plain.length() % AES_BLOCK_SIZE;
	std::size_t pad_length = (over == 0) ? 0 : AES_BLOCK_SIZE - over;
	if (cipher.length() < plain.length() + pad_length)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_CRYPTO_CIPHER_SPACE_NOT_ENOUGHT);
	}

	AesBlockEncryptor encryptor;
	encryptor.set_key(key);

	for (std::size_t i = 0; i + AES_BLOCK_SIZE <= cipher.length() - over; i += AES_BLOCK_SIZE)
	{
		encryptor.encrypt(&plain.at<byte>(i), &cipher.at<byte>(i));
	}

	if (over != 0)
	{
		AesBlock block;
		block.fill('\0');
		auto src = &plain.at<byte>(plain.length() - over);
		lights::copy_array(block.data(), src, over);

		byte* out = reinterpret_cast<byte*>(cipher.data());
		encryptor.encrypt(block.data(), out + cipher.length() - AES_BLOCK_SIZE);
	}
}


void aes_decrypt(lights::SequenceView cipher, lights::Sequence plain, const AesKey& key)
{
	if (plain.length() < cipher.length())
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_CRYPTO_PLAIN_SPACE_NOT_ENOUGHT);
	}

	AesBlockDecryptor decryptor;
	decryptor.set_key(key);

	for (std::size_t i = 0; i + AES_BLOCK_SIZE <= plain.length(); i += AES_BLOCK_SIZE)
	{
		decryptor.decrypt(&cipher.at<byte>(i), &plain.at<byte>(i));
	}
}


std::string aes_encrypt(const std::string& plain, const AesKey& key)
{
	std::string cipher;
	cipher.resize(aes_cipher_length(plain.length()));

	aes_encrypt(lights::SequenceView(plain), lights::Sequence(cipher), key);
	return cipher;
}


std::string aes_decrypt(const std::string& cipher, const AesKey& key, bool shrink)
{
	std::string plain;
	plain.resize(cipher.length());

	aes_decrypt(lights::SequenceView(cipher), lights::Sequence(plain), key);

	if (shrink)
	{
		// Shrink for unuse character.
		std::size_t unuse_len = 1;
		for (; unuse_len <= AES_BLOCK_SIZE; ++unuse_len)
		{
			if (plain[plain.length() - unuse_len] != '\0')
			{
				break;
			}
		}
		plain.resize(plain.length() - unuse_len + 1);

	}
	return plain;
}


void aes_encrypt(std::istream& in, std::ostream& out, const AesKey& key)
{
	AesBlock plain;
	AesBlockEncryptor encryptor;
	encryptor.set_key(key);

	do
	{
		read(in, plain);
		if (in.gcount() > 0)
		{
			if (static_cast<std::size_t>(in.gcount()) < plain.size())
			{
				std::fill(plain.begin() + in.gcount(), plain.end(), '\0');
			}
			AesBlock cipher = encryptor.encrypt(plain);
			write(out, cipher);
		}
	} while (in);
}


void aes_decrypt(std::istream& in, std::ostream& out, const AesKey& key, bool shrink)
{
	AesBlock cipher;
	AesBlockDecryptor decryptor;
	decryptor.set_key(key);

	AesBlock plain;
	std::streamsize previous_gcount = 0;

	do
	{
		read(in, cipher);
		if (previous_gcount > 0) // Write previous decrypt data.
		{
			if (in)
			{
				write(out, plain);
			}
			else
			{
				std::size_t real_len = AES_BLOCK_SIZE;
				if (shrink)
				{
					// Shrink for unuse charater.
					for (; real_len > 0; --real_len)
					{
						if (plain[real_len - 1] != '\0')
						{
							break;
						}
					}
				}
				write(out, plain.data(), real_len);
			}
		}

		previous_gcount = in.gcount();
		if (in.gcount() > 0)
		{
			if (static_cast<std::size_t>(in.gcount()) < cipher.size())
			{
				LIGHTS_THROW_EXCEPTION(Exception, ERR_CRYPTO_IMCOMPELTE_DATA);
			}
			decryptor.decrypt(cipher.data(), plain.data());
		}

	} while (in);
}



void aes_encrypt_file(const std::string& in_filename, const std::string& out_filename, const AesKey& key)
{
	std::ifstream in(in_filename, std::ios::binary);
	std::ofstream out(out_filename, std::ios::binary);

	if (in.fail())
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_CRYPTO_CANNOT_OPEN_FILE);
	}
	else if (out.fail())
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_CRYPTO_CANNOT_OPEN_FILE);
	}
	else
	{
		aes_encrypt(in, out, key);
	}
}


void aes_decrypt_file(const std::string& in_filename, const std::string& out_filename, const AesKey& key)
{
	std::ifstream in(in_filename, std::ios::binary);
	std::ofstream out(out_filename, std::ios::binary);

	if (in.fail())
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_CRYPTO_CANNOT_OPEN_FILE);
	}
	else if (out.fail())
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_CRYPTO_CANNOT_OPEN_FILE);
	}
	else
	{
		lights::FileStream file(in_filename, "r");
		auto size = file.size();
		if (size % AES_BLOCK_SIZE != 0)
		{
			LIGHTS_THROW_EXCEPTION(Exception, ERR_CRYPTO_IMCOMPELTE_DATA);
		}
		aes_decrypt(in, out, key);
	}
}


} // namespace crypto
} // namespace spaceless