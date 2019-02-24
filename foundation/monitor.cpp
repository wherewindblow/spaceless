/**
 * monitor.cpp
 * @author wherewindblow
 * @date   Oct 21, 2018
 */


#include "monitor.h"

#include "exception.h"
#include "worker.h"
#include "log.h"


namespace spaceless {

static Logger& logger = get_logger("monitor");

ManagerMonitor::ManagerMonitor()
{
	TimerManager::instance()->register_timer(lights::PreciseTime(MONITOR_MANAGER_STATE_PERIOD_SEC), [&]
	{
		for (auto& pair : m_manager_list)
		{
			LIGHTS_INFO(logger, "Manager={}, size={}.", pair.first, pair.second());
		}
	}, TimerCallPolicy::CALL_FREQUENTLY);
}


void ManagerMonitor::register_manager(const std::string& name, GetSizeFunction get_size_func)
{
	auto value = std::make_pair(name, get_size_func);
	auto result = m_manager_list.insert(value);
	if (!result.second)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_MONITOR_MANAGER_ALREADY_EXIST);
	}
}


void ManagerMonitor::remove_manager(const std::string& name)
{
	m_manager_list.erase(name);
}

} // namespace spaceless

