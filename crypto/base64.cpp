/**
 * base64.cpp
 * @author wherewindblow
 * @date   Aug 26, 2016
 */

#include "base64.h"
#include <cryptopp/base64.h>


namespace spaceless {
namespace crypto {

#define FITUTIL_BASE64_FUNCTION(name, Algorithm)                               \
std::string name(const std::string& origin)                                    \
{                                                                              \
    Algorithm alogrithm;                                                       \
    alogrithm.Put(reinterpret_cast<const CryptoPP::byte*>(origin.data()), origin.size());\
    alogrithm.MessageEnd();                                                    \
    auto size = alogrithm.MaxRetrievable();                                    \
    std::string result;                                                        \
                                                                               \
    if (size != 0)                                                             \
    {                                                                          \
        result.resize(size);                                                   \
        alogrithm.Get(reinterpret_cast<CryptoPP::byte*>(&result.front()), size);  \
    }                                                                          \
    return result;                                                             \
}


FITUTIL_BASE64_FUNCTION(base64_encode, CryptoPP::Base64Encoder);
FITUTIL_BASE64_FUNCTION(base64_decode, CryptoPP::Base64Decoder);
FITUTIL_BASE64_FUNCTION(base64_url_encode, CryptoPP::Base64URLEncoder);
FITUTIL_BASE64_FUNCTION(base64_url_decode, CryptoPP::Base64URLDecoder);

} // namespace crypto
} // namespace spaceless