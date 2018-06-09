/**
 * transaction.cpp
 * @author wherewindblow
 * @date   May 24, 2018
 */

#include "transaction.h"

#include <cassert>

#include "network.h"
#include "schedule.h"


namespace spaceless {

void Network::send_package(int conn_id, const PackageBuffer& package)
{
	NetworkMessageQueue::Message msg = {conn_id, package.package_id()};
	NetworkMessageQueue::instance()->push(NetworkMessageQueue::OUT_QUEUE, msg);
}


MultiplyPhaseTransaction::MultiplyPhaseTransaction(int trans_id) :
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

	int trans_id = m_id; // Cannot capture this. It maybe remove on timeout.
	TimerManager::instance()->start_timer(PreciseTime(timeout, 0), [trans_id]()
	{
		auto trans = MultiplyPhaseTransactionManager::instance()->find_transaction(trans_id);
		if (!trans)
		{
			return;
		}

		SPACELESS_DEBUG(MODULE_NETWORK, "Network connction {}: Timeout trans_id {}, phase {}.",
						trans->waiting_connection_id(),
						trans_id,
						trans->current_phase());
		trans->on_timeout();

		SPACELESS_DEBUG(MODULE_NETWORK, "Network connction {}: End trans_id {}.", trans->waiting_connection_id(), trans_id);
		MultiplyPhaseTransactionManager::instance()->remove_transaction(trans_id);
	});
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




} // namespace spaceless