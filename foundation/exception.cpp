/**
 * exception.cpp
 * @author wherewindblow
 * @date   Mar 06, 2019
 */

#include "exception.h"

#include <functional>
#include <lights/format.h>
#include <Poco/Exception.h>


namespace spaceless {

namespace details {

lights::StringView ErrorCodeCategory::name() const
{
	return "SpacelessErrorCodeCategory";
}


const lights::ErrorCodeDescriptions& ErrorCodeCategory::descriptions(int code) const
{
	static lights::ErrorCodeDescriptions map[] = {
		{"Success"},
	};

	if (is_safe_index(code, map))
	{
		return map[static_cast<std::size_t>(code)];
	}
	else
	{
		static lights::ErrorCodeDescriptions unknown = {"Unknown error"};
		return unknown;
	}
}

} // namespace details


bool safe_call(std::function<void()> function, lights::TextWriter& error_msg, ErrorInfo* error_info)
{
	error_msg.clear();
	auto sink = lights::make_format_sink(error_msg);
	try
	{
		function();
		return true;
	}
	catch (lights::Exception& ex)
	{
		lights::write(sink, "Exception. code={}, msg={}", ex.code(), ex);
		if (error_info != nullptr)
		{
			*error_info = get_error_info(ex);
		}
	}
	catch (Poco::Exception& ex)
	{
		lights::write(sink, "Poco::Exception. name={}, msg={}", ex.name(), ex.message());
	}
	catch (std::exception& ex)
	{
		lights::write(sink, "std::exception. name={}, msg={}", typeid(ex).name(), ex.what());
	}
	catch (...)
	{
		lights::write(sink, "unknown exception");
	}
	return false;
}

} // namespace spaceless