/**
 * rng.cpp
 * @author wherewindblow
 * @date   Nov 18, 2018
 */


#include "rng.h"
#include <cryptopp/osrng.h>

namespace spaceless {
namespace crypto {

RandomNumberGenerator& get_default_rng()
{
	static CryptoPP::AutoSeededRandomPool rng;
	return rng;
}

} // namespace crypto
} // namespace spaceless