/**
 * log.h
 * @author wherewindblow
 * @date   Nov 05, 2017
 */

#pragma once

#include <map>
#include <lights/logger.h>


namespace spaceless {

using Logger = lights::TextLogger;

class LoggerManager
{
public:
	LIGHTS_SINGLETON_INSTANCE(LoggerManager);

	LoggerManager();

	Logger& register_logger(const std::string& name);

	Logger* find_logger(const std::string& name);

	void for_each(std::function<void(const std::string&, Logger*)> callback);

private:
	lights::LogSinkPtr m_log_sink_ptr;
	std::map<std::string, Logger*> m_logger_map;
};

Logger& get_logger(const std::string& name);

} // namespace spaceless
