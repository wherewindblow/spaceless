/**
 * scheduler.h
 * @author wherewindblow
 * @date   Jun 08, 2018
 */

#pragma once

#include <vector>
#include <functional>

#include <lights/logger.h>

#include "basics.h"

/**
 * @note All function that use in framework cannot throw exception. Because don't know who can catch it.
 */
namespace spaceless {

/**
 * Schedule work execution.
 */
class WorkScheduler
{
public:
	SPACELESS_SINGLETON_INSTANCE(WorkScheduler);

	/**
	 * Starts to schedule.
	 */
	void start();

	/**
	 * Stops of schedule.
	 */
	void stop();
};


enum class TimerCallPolicy : bool
{
	CALL_ONCE,
	CALL_FREQUENTLY,
};


/**
 * Manager all timer.
 */
class TimerManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(TimerManager)

	/**
	 * Starts timer and call @c expiry_action at time expriy.
	 * @param delay Default is using value of @c interval.
	 * @return Returns time id.
	 */
	int start_timer(lights::PreciseTime interval,
					std::function<void()> expiry_action,
					TimerCallPolicy call_policy = TimerCallPolicy::CALL_ONCE,
					lights::PreciseTime delay = lights::PreciseTime(0, 0));

	/**
	 * Stops timer.
	 */
	void stop_timer(int time_id);

	/**
	 * Process expiry timer if have.
	 * @return Count of timer expiry.
	 * @note Only use in internal.
	 */
	int process_expiry_timer();

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
			return left.expiry_time < right.expiry_time;
		}
	};

	std::vector<Timer> m_timer_queue;
	int m_next_id = 1;
};

} // namespace spaceless
