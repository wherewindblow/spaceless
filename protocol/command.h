/**
 * command.h
 * @author wherewindblow
 * @date   Jun 12, 2018
 */

#pragma once

#include <string>
#include "message_declare.h"

namespace spaceless {
namespace protocol {

/**
 * Gets message name.
 */
const std::string& get_message_name(const Message& msg);

/**
 * Finds message name.
 * @note Returns nullptr if cannot it.
 */
const std::string* find_message_name(int cmd);

/**
 * Gets message name.
 * @throw Throws exception if cannot find it.
 */
const std::string& get_message_name(int cmd);

/**
 * Finds command.
 * @note Returns nullptr if cannot it.
 */
const int* find_command(const std::string& msg_name);

/**
 * Finds command.
 * @note Returns nullptr if cannot it.
 */
inline const int* find_command(const Message& msg)
{
	return find_command(get_message_name(msg));
}

/**
 * Gets command.
 * @throw Throws exception if cannot find it.
 */
int get_command(const std::string& msg_name);

/**
 * Gets command.
 * @throw Throws exception if cannot find it.
 */
inline int get_command(const Message& msg)
{
	return get_command(get_message_name(msg));
}

} // namespace protocol
} // namespace spaceless
