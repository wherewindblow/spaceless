/**
 * base64.h
 * @author wherewindblow
 * @date   Aug 26, 2016
 * @note Depend on cryptopp library.
 */

#pragma once

#include <string>


namespace spaceless {
namespace crypto {

/**
 * Encode string to base64 string.
 */
std::string base64_encode(const std::string& origin);


/**
 * Decode base64 string to original string.
 */
std::string base64_decode(const std::string& base64);


/**
 * Encode string to base64 url string.
 */
std::string base64_url_encode(const std::string& origin);


/**
 * decode base64 url string to orginal string.
 */
std::string base64_url_decode(const std::string& base64url);

} // namespace crypto
} // namespace spaceless