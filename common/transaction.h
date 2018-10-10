/**
 * transaction.h
 * @author wherewindblow
 * @date   May 24, 2018
 */

#pragma once

#include <map>
#include <protocol/message_declare.h>
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
	 * @param conn_id         Network connection id.
	 * @param cmd             Identifies package content type.
	 * @param msg             Message of protobuff type.
	 * @param bind_trans_id   Specific transaction that trigger by response.
	 * @param is_send_back    Is need to return last request associate transaction.
	 */
	static void send_protocol(int conn_id,
							  const protocol::Message& msg,
							  int bind_trans_id = 0,
							  int trigger_trans_id = 0,
							  int trigger_cmd = 0);

	/**
	 * Parses protobuf as package buffer and send to remote on asynchronization and send back request transaction id.
	 * @param conn_id         Network connection id.
	 * @param cmd             Identifies package content type.
	 * @param msg             Message of protobuff type.
	 * @param trigger_package The package that trigger current transaction.
	 * @param bind_trans_id   Specific transaction that trigger by response.
	 */
	static void send_back_protobuf(int conn_id,
								   const protocol::Message& msg,
								   const PackageBuffer& trigger_package,
								   int bind_trans_id = 0);


	/**
	 * Parses protobuf as package buffer and send to remote on asynchronization and send back request transaction id.
	 * @param conn_id         Network connection id.
	 * @param cmd             Identifies package content type.
	 * @param msg             Message of protobuff type.
	 * @param trigger_source  Package trigger source of trigger current transaction package.
	 * @param bind_trans_id   Specific transaction that trigger by response.
	 */
	static void send_back_protobuf(int conn_id,
								   const protocol::Message& msg,
								   const PackageTriggerSource& trigger_source,
								   int bind_trans_id = 0);

	/**
	 * Sends package to remote on asynchronization.
	 */
	static void service_send_package(int service_id, const PackageBuffer& package);

	/**
	 * Parses protobuf as package buffer and send to remote on asynchronization.
	 * @param service_id      Network service id.
	 * @param cmd             Identifies package content type.
	 * @param msg             Message of protobuff type.
	 * @param bind_trans_id   Specific transaction that trigger by response.
	 * @param is_send_back    Is need to return last request associate transaction.
	 */
	static void service_send_protobuf(int service_id,
									  const protocol::Message& msg,
									  int bind_trans_id = 0,
									  int trigger_trans_id = 0,
									  int trigger_cmd = 0);
	
	/**
	 * Service only can use to send package, but cannot send back message.
	 * Because send back message must use network connection id and receive package must from that connection id.
	 */

private:
	static Logger& logger;
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

	/**
	 * Processes exception.
	 */
	virtual void on_error(int conn_id, const Exception& ex);

	/**
	 * Sets wait package info.
	 * @param conn_id       Network connection that send indicate package.
	 * @param cmd           Waits command.
	 * @param current_phase Current phase.
	 * @param timeout       Time out of waiting next package.
	 */
	void wait_next_phase(int conn_id, int cmd, int current_phase, int timeout = DEFAULT_TIME_OUT);

	/**
	 * Sets wait package info.
	 * @param conn_id       Network connection that send indicate package.
	 * @param msg           Waits protobuf type.
	 * @param current_phase Current phase.
	 * @param timeout       Time out of waiting next package.
	 */
	void wait_next_phase(int conn_id, const protocol::Message& msg, int current_phase, int timeout = DEFAULT_TIME_OUT);

	/**
	 * Sets wait package info.
	 * @param service_id    Network service that send indicate package.
	 * @param cmd           Waits command.
	 * @param current_phase Current phase.
	 * @param timeout       Time out of waiting next package.
	 */
	void service_wait_next_phase(int service_id, int cmd, int current_phase, int timeout = DEFAULT_TIME_OUT);

	/**
	 * Sets wait package info.
	 * @param service_id    Network service that send indicate package.
	 * @param cmd           Waits protobuf type.
	 * @param current_phase Current phase.
	 * @param timeout       Time out of waiting next package.
	 */
	void service_wait_next_phase(int service_id, const protocol::Message& msg, int current_phase, int timeout = DEFAULT_TIME_OUT);

	/**
	 * Sends back message to first connection.
	 */
	void send_back_message(const protocol::Message& msg);

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
	MultiplyPhaseTransaction& register_transaction(TransactionFatory trans_factory);

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
 * Transaction type.
 */
enum class TransactionType
{
	ONE_PHASE_TRANSACTION,
	MULTIPLY_PHASE_TRANSACTION,
};


using TransactionErrorHandler = void (*)(int conn_id, const PackageTriggerSource& trigger_source, const Exception& ex);

void on_transaction_error(int conn_id, const PackageTriggerSource& trigger_source, const Exception& ex);


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
										TransactionErrorHandler error_handler = on_transaction_error);

	void register_one_phase_transaction(const protocol::Message& msg,
										OnePhaseTrancation trancation,
										TransactionErrorHandler error_handler = on_transaction_error);

	/**
	 * Registers association of command with multiply phase transaction.
 	 * @param trans_factory  Fatory of multiply phase transaction.
     * @note A command only can associate one transaction.
     * @throw Throws exception if register failure.
	 */
	void register_multiply_phase_transaction(int cmd, TransactionFatory trans_factory);

	void register_multiply_phase_transaction(const protocol::Message& msg, TransactionFatory trans_factory);

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


#define SPACELESS_REG_ONE_TRANS(protobuf_type, ...) \
		TransactionManager::instance()->register_one_phase_transaction(protobuf_type(), __VA_ARGS__);

#define SPACELESS_REG_MULTIPLE_TRANS(protobuf_type, ...) \
		TransactionManager::instance()->register_multiply_phase_transaction(protobuf_type(), __VA_ARGS__);

} // namespace spaceless
