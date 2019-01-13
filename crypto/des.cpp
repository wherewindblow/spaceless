/**
 * des.cpp
 * @author wherewindblow
 * @date   Oct 08, 2016
 */

#include "des.h"

#include <fstream>

#include <cryptopp/des.h>
#include <lights/file.h>
#include <foundation/basics.h>
#include <foundation/exception.h>


namespace spaceless {
namespace crypto {

DesKeyBits DesKey::bits = DesKeyBits::BITS_64;

DesKey::DesKey(const std::string& value)
{
	this->m_value.resize(static_cast<std::size_t>(DesKey::bits));
	this->m_value.replace(0, value.length(), value);
}


template <typename Istream>
inline Istream& read(Istream& in, byte* data, std::size_t size)
{
	return in.read(reinterpret_cast<char*>(data), size);
}

template <typename Istream>
inline Istream& read(Istream& in, DesBlock& block)
{
	return read(in, block.data(), block.size());
}

template <typename Ostream>
inline Ostream& write(Ostream& out, const byte* data, std::size_t size)
{
	return out.write(reinterpret_cast<const char*>(data), size);
}

template <typename Ostream>
inline Ostream& write(Ostream& out, const DesBlock& block)
{
	return write(out, block.data(), block.size());
}


std::string des_encrypt(const std::string& plain, const DesKey& key)
{
	std::size_t over = plain.length() % DES_BLOCK_SIZE;
	std::size_t pad_length = (over == 0) ?  0 : DES_BLOCK_SIZE - over;
	std::string cipher;
	cipher.resize(plain.length() + pad_length);

	DesBlockEncryptor encryptor;
	encryptor.set_key(key);

	for (std::size_t i = 0; i + DES_BLOCK_SIZE <= cipher.length() - over; i += DES_BLOCK_SIZE)
	{
		encryptor.encrypt(&plain[i], &cipher[i]);
	}

	if (over != 0)
	{
		DesBlock block;
		block.fill('\0');
		auto start = plain.begin() + (plain.length() - over);
		std::copy(start, plain.end(), block.begin());
		char* out = &cipher[cipher.length() - DES_BLOCK_SIZE];
		encryptor.encrypt(block.data(), reinterpret_cast<byte*>(out));
	}
	return cipher;
}


std::string des_decrypt(const std::string& cipher, const DesKey& key, bool shrink)
{
	std::string plain;
	plain.resize(cipher.length());

	DesBlockDecryptor decryptor;
	decryptor.set_key(key);

	for (std::size_t i = 0; i + DES_BLOCK_SIZE <= plain.length(); i += DES_BLOCK_SIZE)
	{
		decryptor.decrypt(&cipher[i], &plain[i]);
	}

	if (shrink)
	{
		// shrink for useless character.
		std::size_t useless_len = 1;
		for (; useless_len <= DES_BLOCK_SIZE; ++useless_len)
		{
			if (plain[plain.length() - useless_len] != '\0')
			{
				break;
			}
		}
		plain.resize(plain.length() - useless_len + 1);
	}
	return plain;
}


void des_encrypt(std::istream& in, std::ostream& out, const DesKey& key)
{
	DesBlock plain;
	DesBlockEncryptor encryptor;
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
			DesBlock cipher = encryptor.encrypt(plain);
			write(out, cipher);
		}
	} while (in);
}


void des_decrypt(std::istream& in, std::ostream& out, const DesKey& key)
{
	DesBlock cipher;
	DesBlockDecryptor decryptor;
	decryptor.set_key(key);

	DesBlock plain;
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
				// Shrink for unuse charater.
				std::size_t real_len = DES_BLOCK_SIZE;
				for (; real_len > 0; --real_len)
				{
					if (plain[real_len - 1] != '\0')
					{
						break;
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
				LIGHTS_THROW_EXCEPTION(Exception, ERR_CRYPTO_INCOMPLETE_DATA);
			}
			decryptor.decrypt(cipher.data(), plain.data());
		}

	} while (in);
}



void des_encrypt_file(const std::string& in_filename, const std::string& out_filename, const DesKey& key)
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
		des_encrypt(in, out, key);
	}
}


void des_decrypt_file(const std::string& in_filename, const std::string& out_filename, const DesKey& key)
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

		if (size % DES_BLOCK_SIZE != 0)
		{
			LIGHTS_THROW_EXCEPTION(Exception, ERR_CRYPTO_INCOMPLETE_DATA);
		}

		des_decrypt(in, out, key);
	}
}

} // namespace crypto
} // namespace spaceless