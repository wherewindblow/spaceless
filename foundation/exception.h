/**
 * exception.h
 * @author wherewindblow
 * @date   Nov 18, 2017
 */

#pragma once

#include "basics.h"

#include <functional>
#include <lights/exception.h>


namespace lights {

class TextWriter;

}


namespace spaceless {

namespace details {

/**
 * ErrorCodeCategory is a implementation of error category that use in spaceless project.
 */
class ErrorCodeCategory: public lights::ErrorCodeCategory
{
public:
	SPACELESS_SINGLETON_INSTANCE(ErrorCodeCategory);

	/**
	 * Returns name of category.
	 */
	lights::StringView name() const override;

	/**
	 * Returns error descriptions of error code.
	 */
	const lights::ErrorCodeDescriptions& descriptions(int code) const override;
};

} // namespace details


/**
 * Exception that use in spaceless project.
 */
class Exception: public lights::Exception
{
public:
	/**
	 * Creates exception.
	 */
	Exception(const lights::SourceLocation& occur_location, int code):
		lights::Exception(occur_location, code, *details::ErrorCodeCategory::instance())
	{
	}
};


/**
 * Error info describe a unique error message.
 */
struct ErrorInfo
{
	ErrorInfo(ErrorCategory category, int code) :
		category(category),
		code(code)
	{}

	ErrorInfo() :
		ErrorInfo(ErrorCategory::INVALID, 0)
	{}

	ErrorCategory category;
	int code;
};


/**
 * Gets error info.
 */
inline ErrorInfo get_error_info(const lights::Exception& ex)
{
	if (typeid(ex) == typeid(lights::Exception))
	{
		return ErrorInfo(ErrorCategory::LIGHTS, ex.code());
	}

	if (typeid(ex) == typeid(Exception))
	{
		return ErrorInfo(ErrorCategory::SPACELESS, ex.code());
	}

	LIGHTS_ASSERT(false && "Invalid Exception.");
}


inline ErrorInfo to_error_info(int code)
{
	return ErrorInfo(ErrorCategory::SPACELESS, code);
}


#define SPACELESS_THROW(code) LIGHTS_THROW(Exception, code)

/**
 * Call function without throw exception.
 */
bool safe_call(std::function<void()> function, lights::TextWriter& error_msg, ErrorInfo* error_info = nullptr);

} // namespace spaceless
