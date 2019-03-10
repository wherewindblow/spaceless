/**
 * transaction.cpp
 * @author wherewindblow
 * @date   May 24, 2018
 */

#include "transaction.h"

#include <cassert>
#include <protocol/all.h>

#include "log.h"
#include "network_message.h"
#include "worker.h"


namespace spaceless {

static Logger& logger = get_logger("worker");


void Network::send_package(int conn_id, Package package, int service_id)
{
	NetworkMessage msg;
	msg.conn_id = conn_id;
	msg.package_id = package.package_id();
	msg.service_id = service_id;
	NetworkMessageQueue::instance()->push(NetworkMessageQueue::OUT_QUEUE, msg);
}


void Network::send_protocol(int conn_id,
							const protocol::Message& msg,
							int bind_trans_id,
							int trigger_package_id,
							int trigger_cmd,
							int service_id)
{
	const char* target_type = conn_id ? "Connection" : "Service";
	int target_id = conn_id ? conn_id : service_id;

	int size = protocol::get_message_size(msg);
	if (static_cast<std::size_t>(size) > PackageBuffer::MAX_CONTENT_LEN)
	{
		LIGHTS_ERROR(logger, "{} {}: Content length is too large. length={}.", target_type, target_id, size);
		return;
	}

	Package package = PackageManager::instance()->register_package(size);
	PackageHeader& header = package.header();

	if (protocol::get_message_name(msg) == "RspError" && trigger_cmd != 0)
	{
		// Convert RspError to RspXXX that associate trigger cmd. So dependent on protocol message name.
		auto msg_name = protocol::get_message_name(trigger_cmd);
		msg_name.replace(0, 3, "Rsp");
		header.base.command = protocol::get_command(msg_name);
		LIGHTS_DEBUG(logger, "{} {}: Send package. cmd={}, name={}.", target_type, target_id, header.base.command, msg_name);
	}
	else
	{
		auto& msg_name = protocol::get_message_name(msg);
		header.base.command = protocol::get_command(msg_name);
		LIGHTS_DEBUG(logger, "{} {}: Send package. cmd={}, name={}.", target_type, target_id, header.base.command, msg_name);
	}

	header.base.content_length = size;
	header.extend.self_package_id = package.package_id();
	header.extend.trigger_package_id = trigger_package_id;

	bool ok = protocol::parse_to_sequence(msg, package.content_buffer());
	if (!ok)
	{
		LIGHTS_ERROR(logger, "{} {}: Parse to sequence failure. cmd={}.", target_type, target_id, header.base.command);
		PackageManager::instance()->remove_package(package.package_id());
		return;
	}

	if (bind_trans_id != 0)
	{
		MultiplyPhaseTransactionManager::instance()->bind_transaction(bind_trans_id, package.package_id());
	}

	send_package(conn_id, package, service_id);
}


MultiplyPhaseTransaction::MultiplyPhaseTransaction(int trans_id) :
	m_id(trans_id),
	m_current_phase(0),
	m_first_conn_id(0),
	m_first_trigger_source(),
	m_wait_conn_id(0),
	m_wait_service_id (0),
	m_wait_cmd(0),
	m_is_waiting(false)
{
}


MultiplyPhaseTransaction* MultiplyPhaseTransaction::register_transaction(int trans_id)
{
	return nullptr;
}


void MultiplyPhaseTransaction::pre_on_init(int conn_id, Package package)
{
	m_first_conn_id = conn_id;
	m_first_trigger_source = package.get_trigger_source();
}


void MultiplyPhaseTransaction::on_timeout()
{
}


void MultiplyPhaseTransaction::on_error(int conn_id, int error_code)
{
	on_transaction_error(m_first_conn_id, m_first_trigger_source, error_code);
}


void MultiplyPhaseTransaction::wait_next_phase(int conn_id, int cmd, int current_phase, int timeout, int service_id)
{
	m_wait_conn_id = conn_id;
	m_wait_service_id = service_id;
	m_wait_cmd = cmd;
	m_current_phase = current_phase;
	m_is_waiting = true;

	int trans_id = m_id; // Cannot capture this. It maybe remove on timeout.
	TimerManager::instance()->register_timer("wait_next_phase", lights::PreciseTime(timeout), [trans_id]()
	{
		auto trans = MultiplyPhaseTransactionManager::instance()->find_transaction(trans_id);
		if (!trans)
		{
			return;
		}

		LIGHTS_DEBUG(logger, "Connection {}: Transaction timeout. trans_id={}, phase={}.",
					 trans->waiting_connection_id(),
					 trans_id,
					 trans->current_phase());

		trans->clear_waiting_state();
		trans->on_timeout();

		if (!trans->is_waiting())
		{
			LIGHTS_DEBUG(logger, "Connection {}: Transaction end. trans_id={}.", trans->waiting_connection_id(), trans_id);
			MultiplyPhaseTransactionManager::instance()->remove_transaction(trans_id);
		}
	});
}


void MultiplyPhaseTransaction::send_back_error(int code)
{
	LIGHTS_ERROR(logger, "Connection {}: Transaction error. code={}.", first_connection_id(), code);
	protocol::RspError error;
	error.set_result(code);
	Network::send_back_protocol(first_connection_id(), error, m_first_trigger_source);
}


MultiplyPhaseTransaction& MultiplyPhaseTransactionManager::register_transaction(TransactionFatory trans_factory)
{
	MultiplyPhaseTransaction* trans = trans_factory(m_next_id);
	++m_next_id;

	auto value = std::make_pair(trans->transaction_id(), trans);
	auto result = m_trans_list.insert(value);
	if (!result.second)
	{
		LIGHTS_THROW(Exception, ERR_MULTIPLY_PHASE_TRANSACTION_ALREADY_EXIST);
	}

	return *trans;
}


void MultiplyPhaseTransactionManager::remove_transaction(int trans_id)
{
	MultiplyPhaseTransaction* trans = find_transaction(trans_id);
	if (trans != nullptr)
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


std::size_t MultiplyPhaseTransactionManager::size()
{
	return m_trans_list.size();
}


void MultiplyPhaseTransactionManager::bind_transaction(int trans_id, int package_id)
{
	auto value = std::make_pair(package_id, trans_id);
	auto result = m_bind_list.insert(value);
	if (!result.second)
	{
		LIGHTS_THROW(Exception, ERR_BOUND_TRANSACTION_ALREADY_EXIST);
	}
}


void MultiplyPhaseTransactionManager::remove_bound_transaction(int package_id)
{
	m_bind_list.erase(package_id);
}


MultiplyPhaseTransaction* MultiplyPhaseTransactionManager::find_bound_transaction(int package_id)
{
	auto itr = m_bind_list.find(package_id);
	if (itr == m_bind_list.end())
	{
		return nullptr;
	}

	return find_transaction(itr->second);
}


void on_transaction_error(int conn_id, const PackageTriggerSource& trigger_source, int error_code)
{
	protocol::RspError error;
	error.set_result(error_code);
	Network::send_back_protocol(conn_id, error, trigger_source);
}


void TransactionManager::register_transaction(int cmd,
											  TransactionType trans_type,
											  std::any handler,
											  TransactionErrorHandler error_handler)
{
	Transaction trans(trans_type, handler, error_handler);
	auto value = std::make_pair(cmd, trans);
	auto result = m_trans_list.insert(value);
	if (!result.second)
	{
		LIGHTS_THROW(Exception, ERR_TRANSACTION_ALREADY_EXIST);
	}
}


void TransactionManager::register_one_phase_transaction(int cmd,
														OnePhaseTransaction transaction,
														TransactionErrorHandler error_handler)
{
	register_transaction(cmd, TransactionType::ONE_PHASE_TRANSACTION, transaction, error_handler);
}


void TransactionManager::register_one_phase_transaction(const protocol::Message& msg,
														OnePhaseTransaction transaction,
														TransactionErrorHandler error_handler)
{
	auto cmd = protocol::get_command(msg);
	register_one_phase_transaction(cmd, transaction, error_handler);
}


void TransactionManager::register_multiply_phase_transaction(int cmd, TransactionFatory trans_factory)
{
	register_transaction(cmd, TransactionType::MULTIPLY_PHASE_TRANSACTION, trans_factory);
}


void TransactionManager::register_multiply_phase_transaction(const protocol::Message& msg, TransactionFatory trans_factory)
{
	auto cmd = protocol::get_command(msg);
	register_multiply_phase_transaction(cmd, trans_factory);
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