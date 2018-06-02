/**
 * transaction.cpp
 * @author wherewindblow
 * @date   May 24, 2018
 */

#include "transaction.h"

#include <cassert>
#include <thread>
#include <atomic>

#include "network.h"


namespace spaceless {

void Network::send_package(int conn_id, const PackageBuffer& package)
{
	NetworkMessageQueue::Message msg = { conn_id, package.package_id() };
	NetworkMessageQueue::instance()->push(NetworkMessageQueue::OUT_QUEUE, msg);
}


MultiplyPhaseTransaction::MultiplyPhaseTransaction(int trans_id):
	m_id(trans_id)
{
}


MultiplyPhaseTransaction* MultiplyPhaseTransaction::register_transaction(int trans_id)
{
	return nullptr;
}


void MultiplyPhaseTransaction::pre_on_init(int conn_id, const PackageBuffer& package)
{
	m_first_conn_id = conn_id;
	m_package_id = package.package_id();
}


MultiplyPhaseTransaction::PhaseResult MultiplyPhaseTransaction::on_timeout()
{
	return EXIT_TRANCATION;
}


MultiplyPhaseTransaction::PhaseResult MultiplyPhaseTransaction::wait_next_phase(int conn_id,
																				int cmd,
																				int current_phase,
																				int timeout)
{
	m_wait_conn_id = conn_id;
	m_wait_cmd = cmd;
	m_current_phase = current_phase;
	// TODO: How to implement timeout of transaction.
	return WAIT_NEXT_PHASE;
}


int MultiplyPhaseTransaction::transaction_id() const
{
	return m_id;
}


int MultiplyPhaseTransaction::current_phase() const
{
	return m_current_phase;
}


int MultiplyPhaseTransaction::first_connection_id() const
{
	return m_first_conn_id;
}


int MultiplyPhaseTransaction::first_package_id() const
{
	return m_package_id;
}


int MultiplyPhaseTransaction::waiting_connection_id() const
{
	return m_wait_conn_id;
}


int MultiplyPhaseTransaction::waiting_command() const
{
	return m_wait_cmd;
}


MultiplyPhaseTransaction& MultiplyPhaseTransactionManager::register_transaction(TransactionFatory trans_fatory)
{
	MultiplyPhaseTransaction* trans = trans_fatory(m_next_id);
	++m_next_id;

	auto value = std::make_pair(trans->transaction_id(), trans);
	auto result = m_trans_list.insert(value);
	if (result.second == false)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_MULTIPLY_PHASE_TRANSACTION_ALREADY_EXIST);
	}

	return *trans;
}


void MultiplyPhaseTransactionManager::remove_transaction(int trans_id)
{
	MultiplyPhaseTransaction* trans = find_transaction(trans_id);
	if (trans)
	{
		delete trans;
	}

	m_trans_list.erase(trans_id);
}


MultiplyPhaseTransaction* MultiplyPhaseTransactionManager::find_transaction(int trans_id)
{
	auto itr = m_trans_list.find(trans_id);
	if (itr == m_trans_list.end())
	{
		return nullptr;
	}

	return itr->second;
}


void TransactionManager::register_transaction(int cmd, TransactionType trans_type, void* handler)
{
	auto value = std::make_pair(cmd, Transaction {trans_type, handler});
	auto result = m_trans_list.insert(value);
	if (result.second == false)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_TRANSACTION_ALREADY_EXIST);
	}
}


void TransactionManager::register_one_phase_transaction(int cmd, OnePhaseTrancation trancation)
{
	void* handler = reinterpret_cast<void*>(trancation);
	register_transaction(cmd, TransactionType::ONE_PHASE_TRANSACTION, handler);
}


void TransactionManager::register_multiply_phase_transaction(int cmd, TransactionFatory trans_fatory)
{
	void* handler = reinterpret_cast<void*>(trans_fatory);
	register_transaction(cmd, TransactionType::MULTIPLY_PHASE_TRANSACTION, handler);
}


void TransactionManager::remove_transaction(int cmd)
{
	m_trans_list.erase(cmd);
}


Transaction* TransactionManager::find_transaction(int cmd)
{
	auto itr = m_trans_list.find(cmd);
	if (itr == m_trans_list.end())
	{
		return nullptr;
	}
	return &itr->second;
}


namespace details {

void trigger_transaction(const NetworkMessageQueue::Message& msg);


struct SchedulerContext
{
	enum RunState
	{
		STARTING,
		STARTED,
		STOPING,
		STOPED,
	};

	std::atomic<int> run_state = ATOMIC_VAR_INIT(STOPED);
	std::atomic<bool> stop_flag = ATOMIC_VAR_INIT(false);
};

SchedulerContext scheduler_context;


void schedule()
{
	scheduler_context.run_state = SchedulerContext::STARTED;

	while (!scheduler_context.stop_flag)
	{
		if (!NetworkMessageQueue::instance()->empty(NetworkMessageQueue::IN_QUEUE))
		{
			auto msg = NetworkMessageQueue::instance()->pop(NetworkMessageQueue::IN_QUEUE);
			trigger_transaction(msg);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(POLLING_INTERNAL_SLEEP_MS));
	}

	scheduler_context.run_state = SchedulerContext::STOPED;
}


int safe_excute(int conn_id, std::function<void()> function)
{
	try
	{
		function();
		return 0;
	}
	catch (Exception& ex)
	{
		SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: Extend exception {}, {}.", conn_id, ex.code(), ex);
	}
	catch (Poco::Exception& ex)
	{
		SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: Type {}, {}.", conn_id, ex.name(), ex.message());
	}
	catch (std::exception& ex)
	{
		SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: Type {}, {}.", conn_id, typeid(ex).name(), ex.what());
	}
	catch (...)
	{
		SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: Unkown error.", conn_id);
	}
	return -1;
}

void trigger_transaction(const NetworkMessageQueue::Message& msg)
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
					auto& trans_handler = MultiplyPhaseTransactionManager::instance()->register_transaction(trans_factory);

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
				SPACELESS_DEBUG(MODULE_NETWORK,
								"Network connction {}: Trigger cmd {}, Active trans_id {}, phase {}.",
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
						SPACELESS_DEBUG(MODULE_NETWORK,
										"Network connction {}: End trans_id {}.",
										conn_id,
										trans_id);
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

} // namespace details


void TransactionScheduler::start()
{
	using namespace details;
	if (scheduler_context.run_state == SchedulerContext::STOPED)
	{
		scheduler_context.run_state = SchedulerContext::STARTING;
		std::thread thread(schedule);
		thread.detach();
	}
	else if (scheduler_context.run_state == SchedulerContext::STARTED)
	{
		assert(false && "scheduler already started");
	}
}


void TransactionScheduler::stop()
{
	using namespace details;
	if (scheduler_context.run_state == SchedulerContext::STARTED)
	{
		scheduler_context.stop_flag = true;
		scheduler_context.run_state = SchedulerContext::STOPING;
	}
}


} // namespace spaceless