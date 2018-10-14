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
 * Gets command.
 * @throw Throws exception if cannot find it.
 */
int get_command(const std::string& msg_name);

/**
 * Finds command.
 * @note Returns nullptr if cannot it.
 */
const int* find_command(const Message& msg);

/**
 * Gets command.
 * @throw Throws exception if cannot find it.
 */
int get_command(const Message& msg);

} // namespace protocol
} // namespace spaceless
