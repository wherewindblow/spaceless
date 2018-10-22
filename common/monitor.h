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
 * Monite manager state.
 */
class ManagerMonitor
{
public:
	SPACELESS_SINGLETON_INSTANCE(ManagerMonitor);

	using GetSizeFunction = std::function<std::size_t()>;

	/**
	 * Creates manager monitor.
	 */
	ManagerMonitor();

	/**
	 * Register manager and monitor manager state.
	 * @throw Throws exception if register failure.
	 */
	void register_manager(const std::string& name, GetSizeFunction get_size_func);

	/**
	 * Remove manager and not monitor manager state no longer.
	 */
	void remove_manager(const std::string& name);

private:
	std::map<std::string, GetSizeFunction> m_manager_list;
};


/**
 * Register manager to monitor.
 * @note Manager must have function that signature is `std::sizt size()`.
 */
#define SPACELESS_REG_MANAGER(class_name)                                     \
{                                                                             \
	auto get_size_func = []() { return class_name::instance()->size(); };     \
	ManagerMonitor::instance()->register_manager(#class_name, get_size_func); \
}

/**
 * Creates manager constructor and register manager to monitor.
 * @note Manager must have function that signature is `std::sizt size()`.
 */
#define SPACELESS_AUTO_REG_MANAGER(class_name) \
	class_name() {SPACELESS_REG_MANAGER(class_name);}

} // namespace spaceless
