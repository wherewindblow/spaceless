/**
 * log.cpp
 * @author wherewindblow
 * @date   Nov 05, 2017
 */

#include "log.h"

#include <lights/log_sinks/stdout_sink.h>
#include <lights/log_sinks/file_sink.h>


namespace spaceless {

LoggerManager::LoggerManager():
	m_log_sink_ptr(lights::log_sinks::StdoutSink::instance())
//	m_log_sink_ptr(std::make_shared<lights::log_sinks::SimpleFileSink>(("log"))
{
}


Logger& LoggerManager::register_logger(const std::string& name)
{
	Logger* logger = new Logger(name, m_log_sink_ptr);
	auto pair = m_logger_map.insert(std::make_pair(name, logger));
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
	auto itr = m_logger_map.find(name);
	if (itr == m_logger_map.end())
	{
		return nullptr;
	}

	return itr->second;
}


void LoggerManager::for_each(std::function<void(const std::string&, Logger*)> callback)
{
	for (auto& pair :m_logger_map)
	{
		callback(pair.first, pair.second);
	}
}


Logger& get_logger(const std::string& name)
{
	Logger* logger = LoggerManager::instance()->find_logger(name);
	if (logger)
	{
		return *logger;
	}

	return LoggerManager::instance()->register_logger(name);
}

} // namespace spaceless