/**
 * monitor.h
 * @author wherewindblow
 * @date   Oct 21, 2018
 */

#pragma once

#include <string>
#include <functional>
#include <map>

#include "basics.h"


namespace spaceless {

/**
 * Monitor state.
 */
class MonitorManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(MonitorManager);

	using GetSizeFunction = std::function<std::size_t()>;

	/**
	 * Creates monitor manager.
	 */
	MonitorManager();

	/**
	 * Register monitor to monitor state.
	 * @throw Throws exception if register failure.
	 */
	void register_monitor(const std::string& name, GetSizeFunction get_size_func);

	/**
	 * Remove monitor.
	 */
	void remove_monitor(const std::string& name);

private:
	std::map<std::string, GetSizeFunction> m_monitor_list;
};


/**
 * Register monitor to monitor state.
 * @note Manager must have function that signature is `std::size_t size()`.
 */
#define SPACELESS_REG_MONITOR(class_name)                                     \
do                                                                            \
{                                                                             \
	auto get_size_func = []() { return class_name::instance()->size(); };     \
	MonitorManager::instance()->register_monitor(#class_name, get_size_func); \
} while (false)

} // namespace spaceless
