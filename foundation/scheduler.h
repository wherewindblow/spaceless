/**
 * scheduler.h
 * @author wherewindblow
 * @date   Oct 18, 2018
 */

#pragma once

#include "basics.h"


namespace spaceless {

/**
 * Schedule network and worker.
 */
class Scheduler
{
public:
	SPACELESS_SINGLETON_INSTANCE(Scheduler);

	/**
	 * Starts to schedule. By default, it'll return when have signal SIGINT, SIGTERM or SIGUSR1.
	 * @note It'll block until it's stopped.
	 */
	void start();

	/**
	 * Stops of schedule.
	 * @note It'll block until finish stop.
	 */
	void stop();
};

} // namespace spaceless