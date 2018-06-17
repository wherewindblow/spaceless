/**
 * command.h
 * @author wherewindblow
 * @date   Jun 12, 2018
 */

#pragma once

#include <string>


namespace spaceless {
namespace protocol {

template <typename ProtobufType>
inline const std::string& get_protobuf_name(const ProtobufType& msg)
{
	return msg.GetMetadata().descriptor->name();
}

const std::string* find_protobuf_name(int cmd);

const int* find_command(const std::string& protobuf_name);

template <typename ProtobufType>
const int* find_command(const ProtobufType& msg)
{
	return find_command(get_protobuf_name(msg));
}

int get_command(const std::string& protobuf_name);

const std::string& get_protobuf_name(int cmd);

template <typename ProtobufType>
int get_command(const ProtobufType& msg)
{
	return get_command(get_protobuf_name(msg));
}

} // namespace protocol
} // namespace spaceless
