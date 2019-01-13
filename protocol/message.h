/**
 * message.h
 * Uses to separate implementation.
 * @author wherewindblow
 * @date   Oct 10, 2018
 */

#pragma once

#include <google/protobuf/message.h>


namespace spaceless {
namespace protocol {

using Message = google::protobuf::Message;

/**
 * Gets message name.
 */
inline const std::string& get_message_name(const Message& msg)
{
	return msg.GetDescriptor()->name();
}

/**
 * Get message memory size.
 */
inline int get_message_size(const Message& msg)
{
	return msg.ByteSize();
}

/**
 * Parses sequence to message.
 * @return Return is success or failure.
 */
inline bool parse_to_message(lights::SequenceView sequence, Message& msg)
{
	return msg.ParseFromArray(sequence.data(), static_cast<int>(sequence.length()));
}

/**
 * Parses message to sequence.
 * @return Return is success or failure.
 */
inline bool parse_to_sequence(const Message& msg, lights::Sequence sequence)
{
	return msg.SerializeToArray(sequence.data(), static_cast<int>(sequence.length()));
}

} // namespace protocol
} // namespace spaceless
