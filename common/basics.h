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

namespace spaceless {

const int PROTOCOL_PACKAGE_VERSION = 1;
const int INVALID_ID = 0;
const int MAX_SEND_PACKAGE_PROCESS_PER_TIMES = 10;
const int REACTOR_TIME_OUT_US = 5000;
const int TRANSACTION_IDLE_SLEEP_MS = 2;

extern const char* WORKER_THREAD_NAME;

enum
{
	ERR_SUCCESS = 0,
	ERR_NETWORK_PACKAGE_ALREADY_EXIST = 100,
	ERR_NETWORK_PACKAGE_CANNOT_PARSE_TO_PROTOCOL = 101,
	ERR_NETWORK_PACKAGE_NOT_EXIST = 102,
	ERR_NETWORK_CONNECTION_NOT_EXIST = 105,
	ERR_NETWORK_SERVICE_ALREADY_EXIST = 110,
	ERR_NETWORK_SERVICE_NOT_EXIST = 111,
	ERR_PROTOCOL_COMMAND_NOT_EXIST = 115,
	ERR_PROTOCOL_NAME_NOT_EXIST = 116,
	ERR_TRANSACTION_ALREADY_EXIST = 120,
	ERR_MULTIPLY_PHASE_TRANSACTION_ALREADY_EXIST = 121,
	ERR_EVENT_ALREADY_EXIST = 140,
};

} // namespace spaceless

