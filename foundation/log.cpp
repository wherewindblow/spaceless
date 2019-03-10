/**
 * log.cpp
 * @author wherewindblow
 * @date   Nov 05, 2017
 */

#include "log.h"

#include <lights/sinks/stdout_sink.h>
#include <lights/sinks/file_sink.h>


namespace spaceless {

lights::LogLevel to_log_level(lights::StringView str)
{
	using lights::LogLevel;
	if (str == "debug")
	{
		return LogLevel::DEBUG;
	}
	else if (str == "info")
	{
		return LogLevel::INFO;
	}
	else if (str == "warnning")
	{
		return LogLevel::WARN;
	}
	else if (str == "error")
	{
		return LogLevel::ERROR;
	}
	else if (str == "off")
	{
		return LogLevel::OFF;
	}
	else
	{
		return LogLevel::OFF;
	}
}


LoggerManager::LoggerManager():
	m_sink(&lights::sinks::StdoutSink::instance())
{
}


Logger& LoggerManager::register_logger(const std::string& name)
{
	Logger* logger = new Logger(name, *m_sink);
	auto pair = m_logger_list.insert(std::make_pair(name, logger));
	if (!pair.second)
	{
		delete logger;
//		return nullptr;
		LIGHTS_ASSERT(false && "Repeat logger name");
	}
	return *logger;
}


Logger* LoggerManager::find_logger(const std::string& name)
{
	auto itr = m_logger_list.find(name);
	if (itr == m_logger_list.end())
	{
		return nullptr;
	}

	return itr->second;
}


void LoggerManager::for_each(std::function<void(const std::string&, Logger&)> callback)
{
	for (auto& pair : m_logger_list)
	{
		callback(pair.first, *pair.second);
	}
}


Logger& get_logger(const std::string& name)
{
	Logger* logger = LoggerManager::instance()->find_logger(name);
	if (logger != nullptr)
	{
		return *logger;
	}

	return LoggerManager::instance()->register_logger(name);
}

} // namespace spaceless