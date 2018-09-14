/**
 * transaction.cpp
 * @author wherewindblow
 * @date   May 24, 2018
 */

#include "transaction.h"

#include <cassert>
#include <protocol/all.h>

#include "network.h"
#include "work_schedule.h"


namespace spaceless {

static Logger& logger = get_logger("worker");
Logger& Network::logger = get_logger("worker");

void Network::send_package(int conn_id, const PackageBuffer& package)
{
	NetworkMessageQueue::Message msg = {conn_id, package.package_id()};
	NetworkMessageQueue::instance()->push(NetworkMessageQueue::OUT_QUEUE, msg);
}


MultiplyPhaseTransaction::MultiplyPhaseTransaction(int trans_id) :
	m_id(trans_id)
{
}

void on_error(int conn_id, const PackageTriggerSource& trigger_source, const Exception& ex)
{
	protocol::RspError error;
	error.set_result(ex.code());
	Network::send_back_protobuf(conn_id, error, trigger_source);
}


MultiplyPhaseTransaction* MultiplyPhaseTransaction::register_transaction(int trans_id)
{
	return nullptr;
}


void MultiplyPhaseTransaction::pre_on_init(int conn_id, const PackageBuffer& package)
{
	m_first_conn_id = conn_id;
	m_first_trigger_source = package.get_trigger_source();
}


void MultiplyPhaseTransaction::on_timeout()
{
}


void MultiplyPhaseTransaction::on_error(int conn_id, const Exception& ex)
{
	spaceless::on_error(m_first_conn_id, m_first_trigger_source, ex);
}


void MultiplyPhaseTransaction::send_back_error(int code)
{
	protocol::RspError error;
	error.set_result(code);
	Network::send_back_protobuf(first_connection_id(), error, m_first_trigger_source);
}


void MultiplyPhaseTransaction::wait_next_phase(int conn_id, int cmd, int current_phase, int timeout)
{
	m_wait_conn_id = conn_id;
	m_wait_cmd = cmd;
	m_current_phase = current_phase;
	m_is_waiting = true;

	int trans_id = m_id; // Cannot capture this. It maybe remove on timeout.
	TimerManager::instance()->start_timer(PreciseTime(timeout, 0), [trans_id]()
	{
		auto trans = MultiplyPhaseTransactionManager::instance()->find_transaction(trans_id);
		if (!trans)
		{
			return;
		}

		LIGHTS_DEBUG(logger, "Connction {}: Timeout trans_id {}, phase {}.",
						trans->waiting_connection_id(),
						trans_id,
						trans->current_phase());

		trans->clear_waiting_state();
		trans->on_timeout();

		if (!trans->is_waiting())
		{
			LIGHTS_DEBUG(logger, "Connction {}: End trans_id {}.",
							trans->waiting_connection_id(),
							trans_id);
			MultiplyPhaseTransactionManager::instance()->remove_transaction(trans_id);
		}
	});
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


const PackageTriggerSource& MultiplyPhaseTransaction::first_trigger_source() const
{
	return m_first_trigger_source;
}


int MultiplyPhaseTransaction::waiting_connection_id() const
{
	return m_wait_conn_id;
}


int MultiplyPhaseTransaction::waiting_command() const
{
	return m_wait_cmd;
}


bool MultiplyPhaseTransaction::is_waiting() const
{
	return m_is_waiting;
}


void MultiplyPhaseTransaction::clear_waiting_state()
{
	m_is_waiting = false;
}


MultiplyPhaseTransaction& MultiplyPhaseTransactionManager::register_transaction(TransactionFatory trans_fatory)
{
	MultiplyPhaseTransaction* trans = trans_fatory(m_next_id);
	++m_next_id;

	auto value = std::make_pair(trans->transaction_id(), trans);
	auto result = m_trans_list.insert(value);
	if (!result.second)
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


void TransactionManager::register_transaction(int cmd,
											  TransactionType trans_type,
											  void* handler,
											  TransactionErrorHandler error_handler)
{
	auto value = std::make_pair(cmd, Transaction {trans_type, handler, error_handler});
	auto result = m_trans_list.insert(value);
	if (!result.second)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_TRANSACTION_ALREADY_EXIST);
	}
}


void TransactionManager::register_one_phase_transaction(int cmd,
														OnePhaseTrancation trancation,
														TransactionErrorHandler error_handler)
{
	void* handler = reinterpret_cast<void*>(trancation);
	register_transaction(cmd, TransactionType::ONE_PHASE_TRANSACTION, handler, error_handler);
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