/**
 * transaction.h
 * @author wherewindblow
 * @date   May 24, 2018
 */

#pragma once

#include <map>

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
	static void send_protobuf(int conn_id, int cmd, const ProtobufType& msg, int bind_trans_id = 0, int trigger_trans_id = 0);

	/**
	 * Parses protobuf as package buffer and send to remote on asynchronization and send back request transaction id.
	 * @param cmd             Identifies package content type.
	 * @param msg             Message of protobuff type.
	 * @param trigger_package The package that trigger current transaction.
	 * @param bind_trans_id   Specific transaction that trigger by response.
	 */
	template <typename ProtobufType>
	static void send_back_protobuf(int conn_id, int cmd, const ProtobufType& msg, const PackageBuffer& trigger_package, int bind_trans_id = 0);
};


template <typename ProtobufType>
void Network::send_protobuf(int conn_id, int cmd, const ProtobufType& msg, int bind_trans_id, int trigger_trans_id)
{
	int size = msg.ByteSize();
	if (static_cast<std::size_t>(size) > PackageBuffer::MAX_CONTENT_LEN)
	{
		SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: Content length {} is too large.", conn_id, size)
		return;
	}

	PackageBuffer& package = PackageBufferManager::instance()->register_package();
	PackageHeader& header = package.header();
	header.command = cmd;
	header.self_trans_id = bind_trans_id;
	header.trigger_trans_id = trigger_trans_id;
	header.content_length = size;
	lights::Sequence storage = package.content_buffer();
	msg.SerializeToArray(storage.data(), static_cast<int>(storage.length()));

	send_package(conn_id, package);
}


template <typename ProtobufType>
void Network::send_back_protobuf(int conn_id, int cmd, const ProtobufType& msg, const PackageBuffer& trigger_package, int bind_trans_id)
{
	send_protobuf(conn_id, cmd, msg, bind_trans_id, trigger_package.header().self_trans_id);
}


/**
 * Transaction type.
 */
enum class TransactionType
{
	ONE_PHASE_TRANSACTION,
	MULTIPLY_PHASE_TRANSACTION,
};


/**
 * General transaction type.
 */
struct Transaction
{
	TransactionType trans_type;
	void* trans_handler;
};


/**
 * Multiply phase transaction allow to save transaction session in time range.
 */
class MultiplyPhaseTransaction
{
public:
	enum PhaseResult
	{
		EXIT_TRANCATION,
		WAIT_NEXT_PHASE,
	};

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
	virtual PhaseResult on_init(int conn_id, const PackageBuffer& package) = 0;

	/**
	 * Processes the package of wait phase.
	 * @param conn     Network connection of send package.
	 * @param package  Pakcage of trigger this function.
	 */
	virtual PhaseResult on_active(int conn_id, const PackageBuffer& package) = 0;

	/**
	 * Processes wait time out.
	 */
	virtual PhaseResult on_timeout();

	/**
	 * Sets wait package info.
	 * @param conn          Network connection that send indicate package.
	 * @param cmd           Waits command.
	 * @param current_phase Current phase.
	 * @param timeout       Time out of waiting next package.
	 */
	PhaseResult wait_next_phase(int conn_id, int cmd, int current_phase, int timeout = 1);

	template <typename T>
	void send_back_message(int cmd, T& msg);

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
	int first_package_id() const;

	/**
	 * Returns waiting connection id that set by @c wait_next_phase
	 */
	int waiting_connection_id() const;

	/**
	 * Returns waiting command that set by @c wait_next_phase.
	 */
	int waiting_command() const;

private:
	int m_id;
	int m_current_phase = 0;
	int m_package_id = 0;
	int m_first_conn_id = 0;
	int m_wait_conn_id = 0;
	int m_wait_cmd = 0;
};


template <typename T>
void MultiplyPhaseTransaction::send_back_message(int cmd, T& msg)
{
	PackageBuffer& package = PackageBufferManager::instance()->get_package(m_package_id);
	Network::send_back_protobuf(m_first_conn_id, cmd, msg, package);
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
	void register_transaction(int cmd, TransactionType trans_type, void* handler);

	/**
	 * Registers association of command with one phase transaction.
	 * @note A command only can associate one transaction.
	 * @throw Throws exception if register failure.
	 */
	void register_one_phase_transaction(int cmd, OnePhaseTrancation trancation);

	/**
	 * Registers association of command with multiply phase transaction.
 	 * @param trans_fatory  Fatory of multiply phase transaction.
     * @note A command only can associate one transaction.
     * @throw Throws exception if register failure.
	 */
	void register_multiply_phase_transaction(int cmd, TransactionFatory trans_fatory);

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

} // namespace spaceless
