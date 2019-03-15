/**
 * worker.cpp
 * @author wherewindblow
 * @date   Jun 08, 2018
 */

#include "worker.h"

#include <atomic>
#include <functional>

#include <Poco/Runnable.h>
#include <Poco/Thread.h>
#include <sys/prctl.h>

#include "log.h"
#include "package.h"
#include "network_message.h"
#include "transaction.h"
#include "monitor.h"


namespace spaceless {

static Logger& logger = get_logger("worker");
static lights::TextWriter error_msg;

namespace details {

class Worker: public Poco::Runnable
{
public:
	SPACELESS_SINGLETON_INSTANCE(Worker)

	virtual void run();

	enum RunState
	{
		STARTING,
		STARTED,
		STOPPING,
		STOPPED,
	};

	std::atomic<int> run_state = ATOMIC_VAR_INIT(STOPPED);
	std::atomic<bool> stop_flag = ATOMIC_VAR_INIT(false);

private:
	void process_message(const NetworkMessage& msg);

	void trigger_transaction(const NetworkMessage& msg);

	bool call_transaction(int conn_id,
						  int trans_id,
						  Package package,
						  TransactionErrorHandler error_handler,
						  std::function<void()> function);

	const std::string& get_name(int cmd);
};


void Worker::run()
{
	run_state = STARTED;
	prctl(PR_SET_NAME, WORKER_THREAD_NAME);

	LIGHTS_INFO(logger, "Running worker.");

	// Package manager instance may be create in network thread. But monitor only can be use in worker thread.
	SPACELESS_REG_MONITOR(PackageManager);
	// Monitor constructor need timer manager. If register in timer constructor will lead to dead lock.
	SPACELESS_REG_MONITOR(TimerManager);
	SPACELESS_REG_MONITOR(MultiplyPhaseTransactionManager);

	int idle_times = 0;
	while (!stop_flag)
	{
		bool have_message = false;
		if (!NetworkMessageQueue::instance()->empty(NetworkMessageQueue::IN_QUEUE))
		{
			have_message = true;
			auto msg = NetworkMessageQueue::instance()->pop(NetworkMessageQueue::IN_QUEUE);
			process_message(msg);
		}

		int expiry_count = TimerManager::instance()->process_expiry_timer();
		bool have_expiry_time = expiry_count != 0;

		if (!have_message && !have_expiry_time)
		{
			long time = WORKER_IDLE_SLEEP_MS;
			++idle_times;
			if (idle_times > WORKER_LONG_IDLE_TIMES)
			{
				time = WORKER_LONG_IDLE_SLEEP_MS;
				idle_times = 0;
			}

			Poco::Thread::current()->sleep(time);
		}
		else
		{
			idle_times = 0;
		}
	}

	LIGHTS_INFO(logger, "Stopped worker.");
	run_state = STOPPED;
}


void Worker::process_message(const NetworkMessage& msg)
{
	if (msg.conn_id != 0 || msg.service_id != 0)
	{
		trigger_transaction(msg);
	}

	if (msg.delegate != nullptr)
	{
		if (!safe_call(msg.delegate, error_msg))
		{
			LIGHTS_ERROR(logger, "Delegation {}: {}.", msg.caller, error_msg.c_str());
		}
	}
}


void Worker::trigger_transaction(const NetworkMessage& msg)
{
	int conn_id = msg.conn_id;
	int package_id = msg.package_id;

	Package package = PackageManager::instance()->find_package(package_id);
	if (!package.is_valid())
	{
		LIGHTS_ERROR(logger, "Connection {}: Package already removed. package_id={}.", conn_id, package_id);
		return;
	}

	int command = package.header().base.command;
	int trigger_package_id = package.header().extend.trigger_package_id;

	MultiplyPhaseTransaction* waiting_trans = nullptr;
	if (trigger_package_id != 0)
	{
		waiting_trans = MultiplyPhaseTransactionManager::instance()->find_bound_transaction(trigger_package_id);
	}

	if (waiting_trans == nullptr) // Create new transaction.
	{
		auto trans = TransactionManager::instance()->find_transaction(command);
		if (trans != nullptr)
		{
			switch (trans->trans_type)
			{
				case TransactionType::ONE_PHASE_TRANSACTION:
				{
					LIGHTS_DEBUG(logger, "Connection {}: Receive package. cmd={}, name={}.",
								 conn_id, command, get_name(command));
					auto trans_handler = std::any_cast<OnePhaseTransaction>(trans->trans_handler);

					call_transaction(conn_id, 0, package, trans->error_handler, [&]()
					{
						trans_handler(conn_id, package);
					});

					break;
				}
				case TransactionType::MULTIPLY_PHASE_TRANSACTION:
				{
					auto trans_factory = std::any_cast<TransactionFatory>(trans->trans_handler);
					auto& trans_handler = MultiplyPhaseTransactionManager::instance()->register_transaction(trans_factory);
					int trans_id = trans_handler.transaction_id();

					LIGHTS_DEBUG(logger, "Connection {}: Receive package. cmd={}, name={}. Transaction start. trans_id={}.",
								 conn_id,
								 command,
								 get_name(command),
								 trans_id);
					trans_handler.pre_on_init(conn_id, package);

					auto error_handler = [&](int conn_id, const PackageTriggerSource& trigger_source, int error_code)
					{
						trans_handler.on_error(conn_id, error_code);
					};

					call_transaction(conn_id, trans_id, package, error_handler, [&]()
					{
						trans_handler.on_init(conn_id, package);
					});

					if (!trans_handler.is_waiting())
					{
						LIGHTS_DEBUG(logger, "Connection {}: Transaction end. trans_id={}.", conn_id, trans_id);
						MultiplyPhaseTransactionManager::instance()->remove_transaction(trans_id);
					}
					break;
				}
				default:
					LIGHTS_ASSERT(false && "Invalid TransactionType");
					break;
			}
		}
		else
		{
			LIGHTS_ERROR(logger, "Connection {}: Unknown command. cmd={}.", conn_id, command);
		}
	}
	else // Active waiting transaction.
	{
		// Check the send response connection is same as send request connection.
		// Don't give chance to other to interrupt not self transaction.

		bool is_fit_network;
		if (waiting_trans->waiting_connection_id() != 0)
		{
			is_fit_network = conn_id == waiting_trans->waiting_connection_id();
		}
		else
		{
			is_fit_network = msg.service_id == waiting_trans->waiting_service_id();
		}

		if (is_fit_network && command == waiting_trans->waiting_command())
		{
			int trans_id = waiting_trans->transaction_id();
			LIGHTS_DEBUG(logger, "Connection {}: Receive package. cmd={}, name={}. Transaction active. trans_id={}, phase={}.",
						 conn_id,
						 command,
						 get_name(command),
						 trans_id,
						 waiting_trans->current_phase());

			// If connection close will not lead to leak.
			// 1. If after on_active is not at waiting, transaction will be remove.
			// 2. If after on_active is at waiting, transaction will be remove on timeout
			//    that cannot receive message by connection close.

			waiting_trans->clear_waiting_state();

			auto error_handler = [&](int conn_id, const PackageTriggerSource& trigger_source, int error_code)
			{
				waiting_trans->on_error(conn_id, error_code);
			};

			call_transaction(conn_id, trans_id, package, error_handler, [&]()
			{
				auto callback = waiting_trans->on_active().get_callback();
				(waiting_trans->*callback)(conn_id, package);
			});

			if (!waiting_trans->is_waiting())
			{
				LIGHTS_DEBUG(logger, "Connection {}: Transaction end. trans_id={}.", conn_id, trans_id);
				MultiplyPhaseTransactionManager::instance()->remove_transaction(trans_id);
			}
		}
		else
		{
			LIGHTS_ERROR(logger, "Connection {}: Not fit with waiting info. "
				"service_id={}, cmd={}, waiting_conn_id={}, waiting_service_id={}, waiting_cmd={}.",
						 conn_id,
						 msg.service_id,
						 command,
						 waiting_trans->waiting_connection_id(),
						 waiting_trans->waiting_service_id(),
						 waiting_trans->waiting_command());
		}

		MultiplyPhaseTransactionManager::instance()->remove_bound_transaction(trigger_package_id);
	}

	PackageManager::instance()->remove_package(package_id);
}


bool Worker::call_transaction(int conn_id,
							  int trans_id,
							  Package package,
							  TransactionErrorHandler error_handler,
							  std::function<void()> function)
{
	int error_code = 0;
	if (!safe_call(function, error_msg, &error_code))
	{
		LIGHTS_ERROR(logger, "Connection {}: Transaction error. trans_id={}. {}.", conn_id, trans_id, error_msg.c_str());

		if (error_handler != nullptr)
		{
			safe_call([&]() {
				error_handler(conn_id, package.get_trigger_source(), error_code);
			}, error_msg);

			LIGHTS_ERROR(logger, "Connection {}: Transaction error handler error. trans_id={}. {}.",
						 conn_id, trans_id, error_msg.c_str());
		}
		return false;
	}
	return true;
}


const std::string& Worker::get_name(int cmd)
{
	auto name = protocol::find_message_name(cmd);
	if (name == nullptr)
	{
		static const std::string INVALID;
		return INVALID;
	}
	return *name;
}

} // namespace details


void WorkerScheduler::start()
{
	using namespace details;
	auto worker = Worker::instance();
	if (worker->run_state == Worker::STOPPED)
	{
		worker->run_state = Worker::STARTING;
		LIGHTS_INFO(logger, "Starting worker scheduler.");

		// Use static to avoid destroy thread and throw NullPointerException (Poco internal bug).
		// Because new thread will use this thread object data. When it destroy before get it's internal data
		// will get a null data and throw NullPointerException when use it.
		static Poco::Thread thread;
		thread.start(*worker);
	}
	else if (worker->run_state == Worker::STARTED)
	{
		LIGHTS_ASSERT(false && "Worker already started");
	}
}


void WorkerScheduler::stop()
{
	using namespace details;
	auto worker = Worker::instance();
	if (worker->run_state == Worker::STARTED)
	{
		worker->stop_flag = true;
		worker->run_state = Worker::STOPPING;
		LIGHTS_INFO(logger, "Stopping worker scheduler.");
	}
}


bool WorkerScheduler::is_worker_running()
{
	using namespace details;
	return Worker::instance()->run_state != Worker::STOPPED;
}


int TimerManager::register_timer(lights::StringView caller,
								 lights::PreciseTime interval,
								 std::function<void()> expiry_action,
								 TimerCallPolicy call_policy,
								 lights::PreciseTime delay)
{
	if (delay.seconds == 0)
	{
		delay = interval;
	}

	lights::PreciseTime expiry_time = lights::current_precise_time() + delay;

	Timer timer = {
		m_next_id,
		interval,
		expiry_time,
		expiry_action,
		call_policy,
		caller,
	};

	++m_next_id;
	m_timer_queue.push_back(timer);
	std::push_heap(m_timer_queue.begin(), m_timer_queue.end(), TimerCompare());
	return timer.timer_id;
}


int TimerManager::register_frequent_timer(lights::StringView caller,
										  lights::PreciseTime interval,
										  std::function<void()> expiry_action,
										  lights::PreciseTime delay)
{
	return register_timer(caller, interval, expiry_action, TimerCallPolicy::CALL_FREQUENTLY, delay);
}


void TimerManager::remove_timer(int time_id)
{
	auto itr = std::remove_if(m_timer_queue.begin(), m_timer_queue.end(), [&](const Timer& timer)
	{
		return timer.timer_id == time_id;
	});

	if (itr == m_timer_queue.end())
	{
		return;
	}

	m_timer_queue.erase(itr);
	std::make_heap(m_timer_queue.begin(), m_timer_queue.end(), TimerCompare());
}


int TimerManager::process_expiry_timer()
{
	if (m_timer_queue.empty())
	{
		return 0;
	}

	// Need to copy.
	Timer top = m_timer_queue.front();
	if (lights::current_precise_time() < top.expiry_time)
	{
		return 0;
	}

	if (!safe_call(top.expiry_action, error_msg))
	{
		LIGHTS_ERROR(logger, "Timer {}: {}", top.caller, error_msg.c_str());
	}

	std::pop_heap(m_timer_queue.begin(), m_timer_queue.end(), TimerCompare());
	m_timer_queue.pop_back();

	if (top.call_policy == TimerCallPolicy::CALL_FREQUENTLY)
	{
		top.expiry_time = lights::current_precise_time() + top.interval;
		m_timer_queue.push_back(top);
		std::push_heap(m_timer_queue.begin(), m_timer_queue.end(), TimerCompare());
	}
	return 1;
}


std::size_t TimerManager::size()
{
	return m_timer_queue.size();
}

} // namespace spaceless