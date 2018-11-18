/**
 * rng.h
 * @author wherewindblow
 * @date   Nov 18, 2018
 * @note Depend on cryptopp library.
 */

#pragma once

#include <cryptopp/cryptlib.h>


namespace spaceless {
namespace crypto {

using RandomNumberGenerator = CryptoPP::RandomNumberGenerator;

RandomNumberGenerator& get_default_rng();

} // namespace crypto
} // namespace spaceless
