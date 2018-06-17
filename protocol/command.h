/**
 * command.h
 * @author wherewindblow
 * @date   Jun 12, 2018
 */

#pragma once

#include <string>


namespace spaceless {
namespace protocol {

/**
 * Gets protobuf name.
 */
template <typename ProtobufType>
inline const std::string& get_protobuf_name(const ProtobufType& msg)
{
	return msg.GetMetadata().descriptor->name();
}

/**
 * Finds protobuf name.
 * @note Returns nullptr if cannot it.
 */
const std::string* find_protobuf_name(int cmd);

/**
 * Gets protobuf name.
 * @throw Throws exception if cannot find it.
 */
const std::string& get_protobuf_name(int cmd);

/**
 * Finds command.
 * @note Returns nullptr if cannot it.
 */
const int* find_command(const std::string& protobuf_name);

/**
 * Finds command.
 * @note Returns nullptr if cannot it.
 */
template <typename ProtobufType>
const int* find_command(const ProtobufType& msg)
{
	return find_command(get_protobuf_name(msg));
}

/**
 * Gets command.
 * @throw Throws exception if cannot find it.
 */
int get_command(const std::string& protobuf_name);

/**
 * Gets command.
 * @throw Throws exception if cannot find it.
 */
template <typename ProtobufType>
int get_command(const ProtobufType& msg)
{
	return get_command(get_protobuf_name(msg));
}

} // namespace protocol
} // namespace spaceless
