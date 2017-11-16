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

