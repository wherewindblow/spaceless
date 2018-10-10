/**
 * message_declare.h
 * @author wherewindblow
 * @date   Oct 10, 2018
 */

#pragma once


namespace google {
namespace protobuf {

class Message;

} // namespace protobuf
} // namespace google


namespace spaceless {
namespace protocol {

using Message = google::protobuf::Message;

} // namespace protocol
} // namespace spaceless
