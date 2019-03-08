/**
 * exception.cpp
 * @author wherewindblow
 * @date   Mar 06, 2019
 */

#include "exception.h"

#include <functional>
#include <Poco/Exception.h>


namespace spaceless {

bool safe_call(std::function<void()> function, lights::TextWriter& error_msg, int* error_code)
{
	error_msg.clear();
	auto sink = lights::make_format_sink(error_msg);
	try
	{
		function();
		return true;
	}
	catch (Exception& ex)
	{
		lights::write(sink, "Exception. code={}, msg={}", ex.code(), ex);
		if (error_code != nullptr)
		{
			*error_code = ex.code();
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