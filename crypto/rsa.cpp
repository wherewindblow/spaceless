/**
 * rsa.cpp
 * @author wherewindblow
 * @date   Jul 24, 2016
 * @note This depend on cryptopp library.
 */

#include "rsa.h"
#include "rng.h"

#include <cryptopp/hex.h>
#include <cryptopp/files.h>
#include <cryptopp/rsa.h>
#include <cryptopp/osrng.h>


namespace spaceless {
namespace crypto {

void RsaPrivateKey::save_to_file(const std::string& filename) const
{
	CryptoPP::HexEncoder file(new CryptoPP::FileSink(filename.c_str()));
	p_impl.DEREncode(file);
	file.MessageEnd();
}


void RsaPrivateKey::load_from_file(const std::string& filename)
{
	CryptoPP::FileSource file(filename.c_str(), true, new CryptoPP::HexDecoder);
	p_impl.BERDecode(file);
}


std::string RsaPrivateKey::save_to_string() const
{
	std::string result;
	CryptoPP::HexEncoder string(new CryptoPP::StringSink(result));
	p_impl.DEREncode(string);
	return result;
}


void RsaPrivateKey::load_from_string(const std::string& hex)
{
	CryptoPP::StringSource string(hex, true, new CryptoPP::HexDecoder);
	p_impl.BERDecode(string);
}


void RsaPublicKey::save_to_file(const std::string& filename) const
{
	CryptoPP::HexEncoder file(new CryptoPP::FileSink(filename.c_str()));
	p_impl.DEREncode(file);
	file.MessageEnd();
}


void RsaPublicKey::load_from_file(const std::string& filename)
{
	CryptoPP::FileSource file(filename.c_str(), true, new CryptoPP::HexDecoder);
	p_impl.BERDecode(file);
}


std::string RsaPublicKey::save_to_string() const
{
	std::string result;
	CryptoPP::HexEncoder string(new CryptoPP::StringSink(result));
	p_impl.DEREncode(string);
	return result;
}


void RsaPublicKey::load_from_string(const std::string& hex)
{
	CryptoPP::StringSource string(hex, true, new CryptoPP::HexDecoder);
	p_impl.BERDecode(string);
}


RsaKeyPair::RsaKeyPair(RsaKeyPair&& keys):
		private_key(std::move(keys.private_key)), public_key(std::move(keys.public_key))
{
}


const RsaKeyPair& RsaKeyPair::operator=(RsaKeyPair&& keys)
{
	if (&keys != this)
	{
		private_key = std::move(keys.private_key);
		public_key = std::move(keys.public_key);
	}
	return *this;
}


RsaKeyPair generate_rsa_key_pair(unsigned int key_length)
{
	CryptoPP::InvertibleRSAFunction params;
	params.GenerateRandomWithKeySize(get_default_rng(), key_length);
	RsaKeyPair pair;
	pair.public_key.p_impl = params;
	pair.private_key.p_impl = params;
	return pair;
}


std::string rsa_encrypt(const std::string& plain, const RsaPublicKey& key)
{
	CryptoPP::RSAES_OAEP_SHA_Encryptor encryptor(key.p_impl);
	std::string cipher;
	auto sink = new CryptoPP::StringSink(cipher);
	auto filter = new CryptoPP::PK_EncryptorFilter(get_default_rng(), encryptor, sink);
	CryptoPP::StringSource(plain, true, filter);
	return cipher;
}


std::string rsa_decrypt(const std::string& cipher, const RsaPrivateKey& key)
{
	CryptoPP::RSAES_OAEP_SHA_Decryptor decryptor(key.p_impl);
	std::string plain;
	auto sink = new CryptoPP::StringSink(plain);
	auto filter = new CryptoPP::PK_DecryptorFilter(get_default_rng(), decryptor, sink);
	CryptoPP::StringSource(cipher, true, filter);
	return plain;
}

} // namespace spaceless
} // namespace lights