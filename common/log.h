/**
 * log.h
 * @author wherewindblow
 * @date   Nov 05, 2017
 */

#pragma once

#include <map>
#include <lights/logger.h>


namespace spaceless {

/**
 * Logger that support using with LIGHT_INFO and etc.
 */
using Logger = lights::TextLogger;


/**
 * Record all logger to reuse it.
 */
class LoggerManager
{
public:
	LIGHTS_SINGLETON_INSTANCE(LoggerManager);

	/**
	 * Creates logger manager.
	 */
	LoggerManager();

	/**
	 * Register logger.
	 * @throw Throws exception if register failure.
	 */
	Logger& register_logger(const std::string& name);

	/**
	 * Finds logger.
	 * @note Returns nullptr if cannot find logger.
	 */
	Logger* find_logger(const std::string& name);

	/**
	 * For each logger to do something.
	 */
	void for_each(std::function<void(const std::string& logger_name, Logger& logger)> callback);

private:
	lights::LogSinkPtr m_log_sink_ptr;
	std::map<std::string, Logger*> m_logger_list;
};

/**
 * Gets logger by name. Finds existing logger or create new logger.
 */
Logger& get_logger(const std::string& name);

/**
 * Convert string to log level.
 */
lights::LogLevel to_log_level(lights::StringView str);

} // namespace spaceless


