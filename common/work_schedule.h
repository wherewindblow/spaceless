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


using lights::PreciseTime;

/**
 * Manager all timer.
 */
class TimerManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(TimerManager)

	struct Timer
	{
		int timer_id;
		PreciseTime interval;
		PreciseTime expiry_time;
		std::function<void()> expiry_action;
	};

	struct TimerCompare
	{
		bool operator()(const Timer& left, const Timer& right)
		{
			return left.expiry_time < right.expiry_time;
		}
	};

	/**
	 * Starts timer and call @c expiry_action at time expriy.
	 * @return Returns time id.
	 */
	int start_timer(PreciseTime interval, std::function<void()> expiry_action);

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
	std::vector<Timer> m_timer_queue;
	int m_next_id = 1;
};

} // namespace spaceless
