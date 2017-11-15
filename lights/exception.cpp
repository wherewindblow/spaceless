/**
 * exception.cpp
 * @author wherewindblow
 * @date   Jul 23, 2017
 */

#include "exception.h"

#include "format.h"
#include "format/binary_format.h"


namespace lights {

StringView LightsErrorCodeCategory::name() const
{
	return "LightsErrorCodeCategory";
}


const ErrorCodeDescriptions& LightsErrorCodeCategory::descriptions(int code) const
{
	static ErrorCodeDescriptions map[] = {
		{"Success"},
		{"Assertion error", "Assertion error: {}"},
		{"Invalid argument", "Invalid argument: {}"},
		{"Open file failure", "Open file \"{}\" failure: {}"}
	};

	if (is_safe_index(code, map))
	{
		return map[static_cast<std::size_t>(code)];
	}
	else
	{
		static ErrorCodeDescriptions unknow = {"Unknow error"};
		return unknow;
	}
}


const char* Exception::what() const noexcept
{
	return get_description(ErrorCodeDescriptions::TYPE_WITHOUT_ARGS).data();
}


void Exception::dump_message(SinkAdapter& out, ErrorCodeDescriptions::DescriptionType /* description_type */) const
{
	StringView str = get_description(ErrorCodeDescriptions::TYPE_WITHOUT_ARGS);
	out.write(str);
}

void AssertionError::dump_message(SinkAdapter& out, ErrorCodeDescriptions::DescriptionType description_type) const
{
	write(make_format_sink_adapter(out),
		  get_description(description_type),
		  m_description);
}

void InvalidArgument::dump_message(SinkAdapter& out, ErrorCodeDescriptions::DescriptionType description_type) const
{
	write(make_format_sink_adapter(out),
		  get_description(description_type),
		  m_description);
}

void OpenFileError::dump_message(SinkAdapter& out, ErrorCodeDescriptions::DescriptionType description_type) const
{
	write(make_format_sink_adapter(out),
		  get_description(description_type),
		  m_filename,
		  current_error());
}


void dump(const Exception& ex, SinkAdapter& out)
{
	ex.dump_message(out);
	out << " <-- ";
	auto& loc = ex.occur_location();
	out << loc.file() << ":";
	to_string(make_format_sink_adapter(out), loc.line());
	out << "##" << loc.function();
}


void to_string(FormatSinkAdapter<BinaryStoreWriter> out, const Exception& ex)
{
	details::FormatSelfSinkAdapter<BinaryStoreWriter> adapter(out);
	ex.dump_message(adapter);
	out << " <-- ";
	auto& loc = ex.occur_location();
	out.get_internal_sink().append(loc.file(), true);
	out << ":" << loc.line() << "##";
	out.get_internal_sink().append(loc.function(), true);
}

} // namespace lights
