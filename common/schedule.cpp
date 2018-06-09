/**
 * scheduler.cpp
 * @author wherewindblow
 * @date   Jun 08, 2018
 */

#include "schedule.h"

#include <atomic>
#include <functional>

#include <Poco/Runnable.h>
#include <sys/prctl.h>

#include "network.h"
#include "transaction.h"


namespace spaceless {

namespace details {

class SchedulerImpl: public Poco::Runnable
{
public:
	SPACELESS_SINGLETON_INSTANCE(SchedulerImpl)

	virtual void run();

	enum RunState
	{
		STARTING,
		STARTED,
		STOPING,
		STOPED,
	};

	std::atomic<int> run_state = ATOMIC_VAR_INIT(STOPED);
	std::atomic<bool> stop_flag = ATOMIC_VAR_INIT(false);

private:
	void trigger_transaction(const NetworkMessageQueue::Message& msg);

	int safe_excute(int conn_id, std::function<void()> function);
};


void SchedulerImpl::run()
{
	run_state = STARTED;
	prctl(PR_SET_NAME, WORKER_THREAD_NAME);

	while (!stop_flag)
	{
		bool need_sleep = false;
		if (!NetworkMessageQueue::instance()->empty(NetworkMessageQueue::IN_QUEUE))
		{
			auto msg = NetworkMessageQueue::instance()->pop(NetworkMessageQueue::IN_QUEUE);
			trigger_transaction(msg);
		}
		else
		{
			need_sleep = true;
		}

		int expiry_count = 0;
		do
		{
			expiry_count = TimerManager::instance()->process_expiry_timer();
		}
		while (expiry_count != 0);

		if (need_sleep)
		{
			Poco::Thread::current()->sleep(TRANSACTION_IDLE_SLEEP_MS);
		}
	}

	run_state = STOPED;
}


void SchedulerImpl::trigger_transaction(const NetworkMessageQueue::Message& msg)
{
	int conn_id = msg.conn_id;
	int package_id = msg.package_id;
	bool need_remove_package = false;

	PackageBuffer* package = PackageBufferManager::instance()->find_package(package_id);
	if (!package)
	{
		SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: Package {} already remove", conn_id, package_id);
		return;
	}

	int trans_id = package->header().trigger_trans_id;
	int command = package->header().command;

	if (trans_id == 0) // Create new transaction.
	{
		auto trans = TransactionManager::instance()->find_transaction(command);
		if (trans)
		{
			switch (trans->trans_type)
			{
				case TransactionType::ONE_PHASE_TRANSACTION:
				{
					SPACELESS_DEBUG(MODULE_NETWORK, "Network connction {}: Trigger cmd {}.", conn_id, command);
					auto trans_handler = reinterpret_cast<OnePhaseTrancation>(trans->trans_handler);

					safe_excute(conn_id, [&]()
					{
						trans_handler(conn_id, *package);
					});

					need_remove_package = true;
					break;
				}
				case TransactionType::MULTIPLY_PHASE_TRANSACTION:
				{
					auto trans_factory = reinterpret_cast<TransactionFatory>(trans->trans_handler);
					auto& trans_handler =
						MultiplyPhaseTransactionManager::instance()->register_transaction(trans_factory);

					SPACELESS_DEBUG(MODULE_NETWORK, "Network connction {}: Trigger cmd {}, Start trans_id {}.",
									conn_id, command, trans_handler.transaction_id());
					trans_handler.pre_on_init(conn_id, *package);

					auto result = MultiplyPhaseTransaction::EXIT_TRANCATION;
					int err = safe_excute(conn_id, [&]()
					{
						result = trans_handler.on_init(conn_id, *package);
					});

					if (err)
					{
						need_remove_package = true;
					}

					if (result == MultiplyPhaseTransaction::EXIT_TRANCATION)
					{
						SPACELESS_DEBUG(MODULE_NETWORK, "Network connction {}: End trans_id {}.",
										conn_id, trans_handler.transaction_id());
						need_remove_package = true;
						MultiplyPhaseTransactionManager::instance()->remove_transaction(trans_handler.transaction_id());
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
			SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: Unkown command {}.", conn_id, command);
			need_remove_package = true;
		}
	}
	else // Active sleep transaction.
	{
		auto trans_handler = MultiplyPhaseTransactionManager::instance()->find_transaction(trans_id);
		if (trans_handler)
		{
			// Check the send rsponse connection is same as send request connection. Don't give chance to
			// other to interrupt not self transaction.
			if (conn_id == trans_handler->waiting_connection_id() && command == trans_handler->waiting_command())
			{
				SPACELESS_DEBUG(MODULE_NETWORK, "Network connction {}: Trigger cmd {}, Active trans_id {}, phase {}.",
								conn_id,
								command,
								trans_id,
								trans_handler->current_phase());

				// TODO: Clean transaction if connection close.
				//	if (/*trans_handler->first_connection_id() == nullptr*/1)
				//	{
				//		MultiplyPhaseTransactionManager::instance()->remove_transaction(trans_id);
				//		SPACELESS_DEBUG(MODULE_NETWORK, "Network connction {}: End trans_id {} by first connection close.", conn_id, trans_id);
				//	}
				//	else
				{
					auto result = MultiplyPhaseTransaction::EXIT_TRANCATION;
					int err = safe_excute(conn_id, [&]()
					{
						result = trans_handler->on_active(conn_id, *package);

					});

					if (err)
					{
						need_remove_package = true;
					}

					if (result == MultiplyPhaseTransaction::EXIT_TRANCATION)
					{
						SPACELESS_DEBUG(MODULE_NETWORK, "Network connction {}: End trans_id {}.", conn_id, trans_id);
						need_remove_package = true;
						PackageBufferManager::instance()->remove_package(trans_handler->first_package_id());
						MultiplyPhaseTransactionManager::instance()->remove_transaction(trans_id);
					}
				}
			}
			else
			{
				SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: cmd {} not fit with conn_id {}, cmd {}.",
								conn_id,
								command,
								trans_handler->waiting_connection_id(),
								trans_handler->waiting_command());
				need_remove_package = true;
			}
		}
		else
		{
			SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: Unkown trans_id {}.", conn_id, trans_id);
			need_remove_package = true;
		}
	}

	if (need_remove_package)
	{
		PackageBufferManager::instance()->remove_package(package_id);
	}
}


int SchedulerImpl::safe_excute(int conn_id, std::function<void()> function)
{
	try
	{
		function();
		return 0;
	}
	catch (Exception& ex)
	{
		SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: Transaction error {}:{}.", conn_id, ex.code(), ex);
	}
	catch (Poco::Exception& ex)
	{
		SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: Transaction Poco error {}:{}.",
						conn_id,
						ex.name(),
						ex.message());
	}
	catch (std::exception& ex)
	{
		SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: Transaction std error {}:{}.",
						conn_id,
						typeid(ex).name(),
						ex.what());
	}
	catch (...)
	{
		SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: Transaction unkown error.", conn_id);
	}
	return -1;
}

} // namespace details


void WorkScheduler::start()
{
	using namespace details;
	auto scheduler = SchedulerImpl::instance();
	if (scheduler->run_state == SchedulerImpl::STOPED)
	{
		scheduler->run_state = SchedulerImpl::STARTING;

		Poco::Thread thread;
		thread.start(*scheduler);
	}
	else if (scheduler->run_state == SchedulerImpl::STARTED)
	{
		assert(false && "scheduler already started");
	}
}


void WorkScheduler::stop()
{
	using namespace details;
	auto scheduler = SchedulerImpl::instance();
	if (scheduler->run_state == SchedulerImpl::STARTED)
	{
		scheduler->stop_flag = true;
		scheduler->run_state = SchedulerImpl::STOPING;
	}
}


int TimerManager::start_timer(lights::PreciseTime interval, std::function<void()> expiry_action)
{
	lights::PreciseTime expiry_time = lights::current_precise_time() + interval;
	Timer timer = { m_next_id, interval,  expiry_time, expiry_action };
	++m_next_id;
	m_timer_queue.push_back(timer);
	std::push_heap(m_timer_queue.begin(), m_timer_queue.end(), TimerCompare());
	return timer.timer_id;
}


void TimerManager::stop_timer(int time_id)
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

	Timer top = m_timer_queue.front();
	if (lights::current_precise_time() < top.expiry_time)
	{
		return 0;
	}

	top.expiry_action();
	std::pop_heap(m_timer_queue.begin(), m_timer_queue.end(), TimerCompare());
	m_timer_queue.pop_back();
	return 1;
}



} // namespace spaceless