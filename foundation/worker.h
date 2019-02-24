/**
 * worker.h
 * @author wherewindblow
 * @date   Jun 08, 2018
 */

#pragma once

#include <vector>
#include <functional>

#include <lights/precise_time.h>

#include "basics.h"


/**
 * @note All function that use in framework cannot throw exception. Because don't know who can catch it.
 */
namespace spaceless {

/**
 * Schedule worker execution.
 */
class WorkerScheduler
{
public:
	SPACELESS_SINGLETON_INSTANCE(WorkerScheduler);

	/**
	 * Starts to schedule worker. Worker is running in other thread.
	 */
	void start();

	/**
	 * Stops of schedule worker. Sets stop flag to let worker to stop running.
	 * @note After this function return, the worker may be still running in other thread.
	 */
	void stop();

	/**
	 * Check worker is running.
	 */
	bool is_worker_running();
};


enum class TimerCallPolicy : bool
{
	CALL_ONCE,
	CALL_FREQUENTLY,
};


/**
 * Manager of all timer. And provide basic scheduling function.
 * @note TimerManager are scheduling by worker.
 */
class TimerManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(TimerManager);

	/**
	 * Registers timer and call @c expiry_action at time expiry.
	 * @param delay Default is using value of @c interval.
	 * @return Returns time id.
	 */
	int register_timer(lights::PreciseTime interval,
					   std::function<void()> expiry_action,
					   TimerCallPolicy call_policy = TimerCallPolicy::CALL_ONCE,
					   lights::PreciseTime delay = lights::PreciseTime(0, 0));

	/**
	 * Removes timer.
	 */
	void remove_timer(int time_id);

	/**
	 * Process expiry timer if have.
	 * @return Count of timer expiry.
	 * @note Only use in internal.
	 */
	int process_expiry_timer();

	/**
	 * Returns number of timer.
	 */
	std::size_t size();

private:
	struct Timer
	{
		int timer_id;
		lights::PreciseTime interval;
		lights::PreciseTime expiry_time;
		std::function<void()> expiry_action;
		TimerCallPolicy call_policy;
	};

	struct TimerCompare
	{
		bool operator()(const Timer& left, const Timer& right)
		{
			return left.expiry_time > right.expiry_time;
		}
	};

	std::vector<Timer> m_timer_queue;
	int m_next_id = 1;
};

} // namespace spaceless
