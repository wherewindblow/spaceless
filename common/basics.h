/**
 * basics.h
 * @author wherewindblow
 * @date   Nov 15, 2017
 */

#pragma once


#define SPACELESS_SINGLETON_INSTANCE(class_name) \
static class_name* instance() \
{ \
	static class_name inst; \
	return &inst; \
}

#define SPACELESS_POD_PACKED_ATTRIBUTE __attribute__((packed))

namespace spaceless {

const int MODULE_NETWORK = 1;
const int PROTOCOL_PACKAGE_VERSION = 1;
const int INVALID_ID = 0;
const int MAX_PACKAGE_PROCESS_PER_TIMES = 10;
const int REACTOR_TIME_OUT_US = 5000;
const int POLLING_INTERNAL_SLEEP_MS = 2;

enum
{
	ERR_SUCCESS = 0,
	ERR_NETWORK_PACKAGE_ALREADY_EXIST = 100,
	ERR_NETWORK_PACKAGE_CANNOT_PARSE_AS_PROTOBUF = 101,
	ERR_NETWORK_PACKAGE_NOT_EXIST = 102,
	ERR_NETWORK_CONNECTION_NOT_EXIST = 110,
	ERR_TRANSACTION_ALREADY_EXIST = 120,
	ERR_MULTIPLY_PHASE_TRANSACTION_ALREADY_EXIST = 130,
	ERR_EVENT_ALREADY_EXIST = 140,
};

} // namespace spaceless

