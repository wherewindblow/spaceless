/**
 * transaction.h
 * @author wherewindblow
 * @date   May 24, 2018
 */

#pragma once

#include <map>
#include <protocol/command.h>

#include "basics.h"
#include "package.h"
#include "log.h"


namespace spaceless {

/**
 * Network connection operation set that is thread safe.
 */
class Network
{
public:
	/**
	 * Sends package to remote on asynchronization.
	 */
	static void send_package(int conn_id, const PackageBuffer& package);

	/**
	 * Parses protobuf as package buffer and send to remote on asynchronization.
	 * @param cmd             Identifies package content type.
	 * @param msg             Message of protobuff type.
	 * @param bind_trans_id   Specific transaction that trigger by response.
	 * @param is_send_back    Is need to return last request associate transaction.
	 */
	template <typename ProtobufType>
	static void send_protobuf(int conn_id, const ProtobufType& msg, int bind_trans_id = 0, int trigger_trans_id = 0, int trigger_cmd = 0);

	/**
	 * Parses protobuf as package buffer and send to remote on asynchronization and send back request transaction id.
	 * @param cmd             Identifies package content type.
	 * @param msg             Message of protobuff type.
	 * @param trigger_package The package that trigger current transaction.
	 * @param bind_trans_id   Specific transaction that trigger by response.
	 */
	template <typename ProtobufType>
	static void send_back_protobuf(int conn_id, const ProtobufType& msg, const PackageBuffer& trigger_package, int bind_trans_id = 0);


	template <typename ProtobufType>
	static void send_back_protobuf(int conn_id, const ProtobufType& msg, const PackageTriggerSource& trigger_source, int bind_trans_id = 0);

private:
	static Logger& logger;
};


template <typename ProtobufType>
void Network::send_protobuf(int conn_id, const ProtobufType& msg, int bind_trans_id, int trigger_trans_id, int trigger_cmd)
{
	int size = msg.ByteSize();
	if (static_cast<std::size_t>(size) > PackageBuffer::MAX_CONTENT_LEN)
	{
//		LIGHTS_THROW_EXCEPTION(Exception, ERR_NETWORK_PACKAGE_TOO_LARGE);
		LIGHTS_ERROR(logger, "Connction {}: Content length {} is too large.", conn_id, size)
		return;
	}

	PackageBuffer& package = PackageBufferManager::instance()->register_package();
	PackageHeader& header = package.header();

	if (protocol::get_protobuf_name(msg) == "RspError" && trigger_cmd != 0)
	{
		// Convert RspError to RspXXX that associate trigger cmd. So dependent on protobuf message name.
		auto protobuf_name = protocol::get_protobuf_name(trigger_cmd);
		protobuf_name.replace(0, 3, "Rsp");
		header.command = protocol::get_command(protobuf_name);
		LIGHTS_DEBUG(logger, "Connction {}: Send cmd {}, name {}.", conn_id, header.command, protobuf_name);
	}
	else
	{
		auto& protobuf_name = protocol::get_protobuf_name(msg);
		header.command = protocol::get_command(protobuf_name);
		LIGHTS_DEBUG(logger, "Connction {}: Send cmd {}, name {}.", conn_id, header.command, protobuf_name);
	}
	header.self_trans_id = bind_trans_id;
	header.trigger_trans_id = trigger_trans_id;
	header.content_length = size;
	lights::Sequence storage = package.content_buffer();
	msg.SerializeToArray(storage.data(), static_cast<int>(storage.length()));

	send_package(conn_id, package);
}


template <typename ProtobufType>
void Network::send_back_protobuf(int conn_id, const ProtobufType& msg, const PackageBuffer& trigger_package, int bind_trans_id)
{
	send_protobuf(conn_id, msg, bind_trans_id, trigger_package.header().self_trans_id, trigger_package.header().command);
}


template <typename ProtobufType>
void Network::send_back_protobuf(int conn_id, const ProtobufType& msg, const PackageTriggerSource& trigger_source, int bind_trans_id)
{
	send_protobuf(conn_id, msg, bind_trans_id, trigger_source.self_trans_id, trigger_source.command);
}


/**
 * Transaction type.
 */
enum class TransactionType
{
	ONE_PHASE_TRANSACTION,
	MULTIPLY_PHASE_TRANSACTION,
};

using TransactionErrorHandler = void (*)(int conn_id, const PackageTriggerSource& trigger_source, const Exception& ex);

void on_error(int conn_id, const PackageTriggerSource& trigger_source, const Exception& ex);

/**
 * General transaction type.
 */
struct Transaction
{
	TransactionType trans_type;
	void* trans_handler;
	TransactionErrorHandler error_handler;
};


/**
 * Multiply phase transaction allow to save transaction session in time range.
 */
class MultiplyPhaseTransaction
{
public:
	static const int DEFAULT_TIME_OUT = 60;

	/**
	 * Factory of this type transaction. Just simply call contructor.
	 * @note This function is only a example.
	 */
	static MultiplyPhaseTransaction* register_transaction(int trans_id);

	/**
	 * Creates transaction by trans_id.
	 */
	MultiplyPhaseTransaction(int trans_id);

	/**
	 * Destroys transaction.
	 */
	virtual ~MultiplyPhaseTransaction() = default;

	/**
	 * Initializes this base class interal variables.
	 */
	void pre_on_init(int conn_id, const PackageBuffer& package);

	/**
	 * Processes the package that trigger by associate command.
	 * @param conn     Network connection of send package.
	 * @param package  Pakcage of trigger this function.
	 */
	virtual void on_init(int conn_id, const PackageBuffer& package) = 0;

	/**
	 * Processes the package of wait phase.
	 * @param conn     Network connection of send package.
	 * @param package  Pakcage of trigger this function.
	 */
	virtual void on_active(int conn_id, const PackageBuffer& package) = 0;

	/**
	 * Processes wait time out.
	 */
	virtual void on_timeout();

	virtual void on_error(int conn_id, const Exception& ex);

	/**
	 * Sets wait package info.
	 * @param conn          Network connection that send indicate package.
	 * @param cmd           Waits command.
	 * @param current_phase Current phase.
	 * @param timeout       Time out of waiting next package.
	 */
	void wait_next_phase(int conn_id, int cmd, int current_phase, int timeout = DEFAULT_TIME_OUT);

	template <typename ProtobufType>
	void wait_next_phase(int conn_id, const ProtobufType& msg, int current_phase, int timeout = DEFAULT_TIME_OUT);

	/**
	 * Sends back message to first connection.
	 */
	template <typename T>
	void send_back_message(T& msg);

	void send_back_error(int code);

	/**
	 * Returns tranascation id.
	 */
	int transaction_id() const;

	/**
	 * Returns current phase.
	 */
	int current_phase() const;

	/**
	 * Returns connection id that first start this transaction.
	 */
	int first_connection_id() const;

	/**
	 * Returns package id that first start this transaction.
	 */
	const PackageTriggerSource& first_trigger_source() const;

	/**
	 * Returns waiting connection id that set by @c wait_next_phase
	 */
	int waiting_connection_id() const;

	/**
	 * Returns waiting command that set by @c wait_next_phase.
	 */
	int waiting_command() const;

	/**
	 * Returns is at waiting state.
	 */
	bool is_waiting() const;

	/**
	 * Clears waiting state.
	 */
	void clear_waiting_state();

private:
	int m_id;
	int m_current_phase = 0;
	int m_first_conn_id = 0;
	PackageTriggerSource m_first_trigger_source;
	int m_wait_conn_id = 0;
	int m_wait_cmd = 0;
	bool m_is_waiting = false;
};

template <typename ProtobufType>
void MultiplyPhaseTransaction::wait_next_phase(int conn_id, const ProtobufType& msg, int current_phase, int timeout)
{
	auto cmd = protocol::get_command(msg);
	wait_next_phase(conn_id, cmd, current_phase, timeout);
}

template <typename ProtobufType>
void MultiplyPhaseTransaction::send_back_message(ProtobufType& msg)
{
	Network::send_back_protobuf(m_first_conn_id, msg, m_first_trigger_source);
}


/**
 * Transaction factory function of multiply phase transaction.
 */
using TransactionFatory = MultiplyPhaseTransaction* (*)(int trans_id);


/**
 * Manager of multiply phase transaction.
 */
class MultiplyPhaseTransactionManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(MultiplyPhaseTransactionManager);

	/**
	 * Registers multiply phase transaction.
	 */
	MultiplyPhaseTransaction& register_transaction(TransactionFatory trans_fatory);

	/**
	 * Removes multiply phase transaction.
	 */
	void remove_transaction(int trans_id);

	/**
	 * Finds multiply phase transaction.
	 * @note Returns nullptr if cannot find multiply phase transaction.
	 */
	MultiplyPhaseTransaction* find_transaction(int trans_id);

private:
	int m_next_id = 1;
	std::map<int, MultiplyPhaseTransaction*> m_trans_list;
};


using OnePhaseTrancation = void (*)(int conn_id, const PackageBuffer&);

/**
 * Manager of transaction. When recieve a command will trigger associated transaction.
 */
class TransactionManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(TransactionManager);

	/**
	 * Registers association of command with transaction.
     * @note A command only can associate one transaction.
     * @throw Throws exception if register failure.
	 */
	void register_transaction(int cmd,
							  TransactionType trans_type,
							  void* handler,
							  TransactionErrorHandler error_handler = nullptr);

	/**
	 * Registers association of command with one phase transaction.
	 * @note A command only can associate one transaction.
	 * @throw Throws exception if register failure.
	 */
	void register_one_phase_transaction(int cmd,
										OnePhaseTrancation trancation,
										TransactionErrorHandler error_handler = on_error);

	template <typename ProtoBufType>
	void register_one_phase_transaction(const ProtoBufType& msg,
										OnePhaseTrancation trancation,
										TransactionErrorHandler error_handler = on_error);

	/**
	 * Registers association of command with multiply phase transaction.
 	 * @param trans_fatory  Fatory of multiply phase transaction.
     * @note A command only can associate one transaction.
     * @throw Throws exception if register failure.
	 */
	void register_multiply_phase_transaction(int cmd, TransactionFatory trans_fatory);

	template <typename ProtoBufType>
	void register_multiply_phase_transaction(const ProtoBufType& msg, TransactionFatory trans_fatory);

	/**
	 * Removes association of command with transaction.
	 */
	void remove_transaction(int cmd);

	/**
	 * Finds transaction that associate with command.
	 * @note Return nullptr if cannot find command.
	 */
	Transaction* find_transaction(int cmd);

private:
	std::map<int, Transaction> m_trans_list;
};


template <typename ProtoBufType>
void TransactionManager::register_one_phase_transaction(const ProtoBufType& msg,
														OnePhaseTrancation trancation,
														TransactionErrorHandler error_handler)
{
	auto cmd = protocol::get_command(msg);
	register_one_phase_transaction(cmd, trancation, error_handler);
}


template <typename ProtoBufType>
void TransactionManager::register_multiply_phase_transaction(const ProtoBufType& msg, TransactionFatory trans_fatory)
{
	auto cmd = protocol::get_command(msg);
	register_multiply_phase_transaction(cmd, trans_fatory);
}


#define SPACE_REGISTER_ONE_PHASE_TRANSACTION(protobuf_type, ...) \
		TransactionManager::instance()->register_one_phase_transaction(protobuf_type(), __VA_ARGS__);

#define SPACE_REGISTER_MULTIPLE_PHASE_TRANSACTION(protobuf_type, ...) \
		TransactionManager::instance()->register_multiply_phase_transaction(protobuf_type(), __VA_ARGS__);

} // namespace spaceless
