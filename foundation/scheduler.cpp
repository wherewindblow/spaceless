/**
 * scheduler.cpp
 * @author wherewindblow
 * @date   Oct 18, 2018
 */


#include "scheduler.h"

#include <signal.h>

#include "worker.h"
#include "network.h"
#include "log.h"


namespace spaceless {

static Logger& logger = get_logger("scheduler");

namespace details {

int catch_signal(int signal_num, void (* signal_handler)(int))
{
	struct sigaction act, old_act;
	act.sa_handler = signal_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	return sigaction(signal_num, &act, &old_act);
}

void safe_exit(int signal_num)
{
	LIGHTS_INFO(logger, "Start exiting by signal {}.", signal_num);
	Scheduler::instance()->stop();
	LIGHTS_INFO(logger, "Finish exiting by signal {}.", signal_num);
}

} // namespace details


void Scheduler::start()
{
	LIGHTS_INFO(logger, "Starting scheduler.");

	using namespace details;
	catch_signal(SIGINT, safe_exit);
	catch_signal(SIGTERM, safe_exit);
	catch_signal(SIGUSR1, safe_exit);

	WorkerScheduler::instance()->start();
	NetworkManager::instance()->start();

	WorkerScheduler* worker_scheduler = WorkerScheduler::instance();
	worker_scheduler->stop();
	while (worker_scheduler->is_worker_running())
	{
		Poco::Thread::current()->sleep(SCHEDULER_WAITTING_STOP_PERIOD_MS);
	}

	LIGHTS_INFO(logger, "Stopped scheduler.");
}


void Scheduler::stop()
{
	LIGHTS_INFO(logger, "Stopping scheduler.")
	NetworkManager::instance()->stop();
	WorkerScheduler* worker_scheduler = WorkerScheduler::instance();
	worker_scheduler->stop();
	while (worker_scheduler->is_worker_running())
	{
		Poco::Thread::current()->sleep(SCHEDULER_WAITTING_STOP_PERIOD_MS);
	}
}

} // namespace spaceless

