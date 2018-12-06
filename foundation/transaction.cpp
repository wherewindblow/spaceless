/**
 * transaction.cpp
 * @author wherewindblow
 * @date   May 24, 2018
 */

#include "transaction.h"

#include <cassert>
#include <protocol/all.h>

#include "network.h"
#include "worker.h"


namespace spaceless {

static Logger& logger = get_logger("worker");
Logger& Network::logger = get_logger("worker");


void Network::send_package(int conn_id, Package package)
{
	NetworkMessage msg = {conn_id, package.package_id()};
	NetworkMessageQueue::instance()->push(NetworkMessageQueue::OUT_QUEUE, msg);
}


void Network::send_protocol(int conn_id,
							const protocol::Message& msg,
							int bind_trans_id,
							int trigger_trans_id,
							int trigger_cmd)
{
	int size = msg.ByteSize();
	if (static_cast<std::size_t>(size) > PackageBuffer::MAX_CONTENT_LEN)
	{
		LIGHTS_ERROR(logger, "Connction {}: Content length {} is too large.", conn_id, size)
		return;
	}

	Package package = PackageManager::instance()->register_package(size);
	PackageHeader& header = package.header();
	header.version = PACKAGE_VERSION;

	if (protocol::get_message_name(msg) == "RspError" && trigger_cmd != 0)
	{
		// Convert RspError to RspXXX that associate trigger cmd. So dependent on protobuf message name.
		auto msg_name = protocol::get_message_name(trigger_cmd);
		msg_name.replace(0, 3, "Rsp");
		header.command = protocol::get_command(msg_name);
		LIGHTS_DEBUG(logger, "Connction {}: Send cmd {}, name {}.", conn_id, header.command, msg_name);
	}
	else
	{
		auto& msg_name = protocol::get_message_name(msg);
		header.command = protocol::get_command(msg_name);
		LIGHTS_DEBUG(logger, "Connction {}: Send cmd {}, name {}.", conn_id, header.command, msg_name);
	}

	header.self_trans_id = bind_trans_id;
	header.trigger_trans_id = trigger_trans_id;
	header.content_length = size;

	bool ok = protocol::parse_to_sequence(msg, package.content_buffer());
	if (!ok)
	{
		LIGHTS_ERROR(logger, "Connction {}: Parse to sequence failure cmd {}.", conn_id, header.command);
		return;
	}

	send_package(conn_id, package);
}


void Network::send_back_protobuf(int conn_id,
								 const protocol::Message& msg,
								 Package trigger_package,
								 int bind_trans_id)
{
	send_protocol(conn_id, msg, bind_trans_id, trigger_package.header().self_trans_id, trigger_package.header().command);
}


void Network::send_back_protobuf(int conn_id,
								 const protocol::Message& msg,
								 const PackageTriggerSource& trigger_source,
								 int bind_trans_id)
{
	send_protocol(conn_id, msg, bind_trans_id, trigger_source.self_trans_id, trigger_source.command);
}


void Network::service_send_protobuf(int service_id,
									const protocol::Message& msg,
									int bind_trans_id,
									int trigger_trans_id,
									int trigger_cmd)
{
	int conn_id = NetworkServiceManager::instance()->get_connection_id(service_id);
	send_protocol(conn_id, msg, bind_trans_id, trigger_trans_id, trigger_cmd);
}


void Network::service_send_package(int service_id, Package package)
{
	int conn_id = NetworkServiceManager::instance()->get_connection_id(service_id);
	send_package(conn_id, package);
}


MultiplyPhaseTransaction::MultiplyPhaseTransaction(int trans_id) :
	m_id(trans_id)
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


void MultiplyPhaseTransaction::on_error(int conn_id, const Exception& ex)
{
	on_transaction_error(m_first_conn_id, m_first_trigger_source, ex);
}


void MultiplyPhaseTransaction::wait_next_phase(int conn_id, int cmd, int current_phase, int timeout)
{
	m_wait_conn_id = conn_id;
	m_wait_cmd = cmd;
	m_current_phase = current_phase;
	m_is_waiting = true;

	int trans_id = m_id; // Cannot capture this. It maybe remove on timeout.
	TimerManager::instance()->start_timer(lights::PreciseTime(timeout), [trans_id]()
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
			LIGHTS_DEBUG(logger, "Connction {}: End trans_id {}.", trans->waiting_connection_id(), trans_id);
			MultiplyPhaseTransactionManager::instance()->remove_transaction(trans_id);
		}
	});
}


void MultiplyPhaseTransaction::wait_next_phase(int conn_id, const protocol::Message& msg, int current_phase, int timeout)
{
	auto cmd = protocol::get_command(msg);
	wait_next_phase(conn_id, cmd, current_phase, timeout);
}


void MultiplyPhaseTransaction::service_wait_next_phase(int service_id, int cmd, int current_phase, int timeout)
{
	int conn_id = NetworkServiceManager::instance()->get_connection_id(service_id);
	wait_next_phase(conn_id, cmd, current_phase, timeout);
}


void MultiplyPhaseTransaction::service_wait_next_phase(int service_id,
													   const protocol::Message& msg,
													   int current_phase,
													   int timeout)
{
	auto cmd = protocol::get_command(msg);
	service_wait_next_phase(service_id, cmd, current_phase, timeout);
}


void MultiplyPhaseTransaction::send_back_message(const protocol::Message& msg)
{
	Network::send_back_protobuf(m_first_conn_id, msg, m_first_trigger_source);
}


void MultiplyPhaseTransaction::send_back_error(int code)
{
	LIGHTS_ERROR(logger, "Connction {}: Error {}.", first_connection_id(), code);
	protocol::RspError error;
	error.set_result(code);
	Network::send_back_protobuf(first_connection_id(), error, m_first_trigger_source);
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


MultiplyPhaseTransaction& MultiplyPhaseTransactionManager::register_transaction(TransactionFatory trans_factory)
{
	MultiplyPhaseTransaction* trans = trans_factory(m_next_id);
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


std::size_t MultiplyPhaseTransactionManager::size()
{
	return m_trans_list.size();
}


void on_transaction_error(int conn_id, const PackageTriggerSource& trigger_source, const Exception& ex)
{
	protocol::RspError error;
	error.set_result(ex.code());
	Network::send_back_protobuf(conn_id, error, trigger_source);
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


void TransactionManager::register_one_phase_transaction(const protocol::Message& msg,
														OnePhaseTrancation trancation,
														TransactionErrorHandler error_handler)
{
	auto cmd = protocol::get_command(msg);
	register_one_phase_transaction(cmd, trancation, error_handler);
}


void TransactionManager::register_multiply_phase_transaction(int cmd, TransactionFatory trans_factory)
{
	void* handler = reinterpret_cast<void*>(trans_factory);
	register_transaction(cmd, TransactionType::MULTIPLY_PHASE_TRANSACTION, handler);
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