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
#include "monitor.h"


namespace spaceless {

/**
 * Network connection operation set that can be call in worker thread.
 */
class Network
{
public:
	/**
	 * Sends package to remote asynchronously.
	 */
	static void send_package(int conn_id, Package package, int service_id = 0);

	/**
	 * Parses message as package buffer and send to remote asynchronously.
	 * @param conn_id            Network connection id.
	 * @param msg                Protocol message.
	 * @param bind_trans_id      Specific transaction that trigger by response.
	 * @param trigger_package_id Transaction id that trigger process. It's only need for send back message.
	 * @param trigger_cmd        Command that trigger process. It's only need for send back message.
	 * @param service_id         Network service id. Uses to replace conn_id.
	 */
	static void send_protocol(int conn_id,
							  const protocol::Message& msg,
							  int bind_trans_id = 0,
							  int trigger_package_id = 0,
							  int trigger_cmd = 0,
							  int service_id = 0);

	/**
	 * Parses message as package buffer and send to remote asynchronously and send back request transaction id.
	 * @param conn_id         Network connection id.
	 * @param msg             Protocol message.
	 * @param trigger_package The package that trigger current transaction.
	 * @param bind_trans_id   Specific transaction that trigger by response.
	 */
	static void send_back_protocol(int conn_id,
								   const protocol::Message& msg,
								   Package trigger_package,
								   int bind_trans_id = 0);

	/**
	 * Parses message as package buffer and send to remote asynchronously and send back request transaction id.
	 * @param conn_id         Network connection id.
	 * @param msg             Protocol message.
	 * @param trigger_source  Package trigger source of trigger current transaction package.
	 * @param bind_trans_id   Specific transaction that trigger by response.
	 */
	static void send_back_protocol(int conn_id,
								   const protocol::Message& msg,
								   const PackageTriggerSource& trigger_source,
								   int bind_trans_id = 0);

	/**
	 * Sends package to remote asynchronously.
	 */
	static void service_send_package(int service_id, Package package);

	/**
	 * Parses message as package buffer and send to remote asynchronously.
	 * @param service_id         Network service id.
	 * @param msg                Protocol message.
	 * @param bind_trans_id      Specific transaction that trigger by response.
	 */
	static void service_send_protocol(int service_id, const protocol::Message& msg, int bind_trans_id = 0);
	
	/**
	 * Service only can use to send package, but cannot send back message.
	 * Because send back message must use network connection id and receive package must from that connection id.
	 */
};


/**
 * Multiply phase transaction allow to save transaction session in time range.
 */
class MultiplyPhaseTransaction
{
public:
	static const int DEFAULT_TIME_OUT = 60;

	/**
	 * Factory of this type transaction. Just simply call constructor.
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
	 * Initializes this base class internal variables.
	 */
	void pre_on_init(int conn_id, Package package);

	/**
	 * Processes the package that trigger by associate command.
	 * @param conn     Network connection of send package.
	 * @param package  Pakcage of trigger this function.
	 */
	virtual void on_init(int conn_id, Package package) = 0;

	/**
	 * Processes the package of wait phase.
	 * @param conn     Network connection of send package.
	 * @param package  Package of trigger this function.
	 */
	virtual void on_active(int conn_id, Package package) = 0;

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
	 * @param service_id    Network service that send indicate package. Uses to replace conn_id.
	 */
	void wait_next_phase(int conn_id, int cmd, int current_phase, int timeout = DEFAULT_TIME_OUT, int service_id = 0);

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

	/**
	 * Sends back error message to first connection.
	 */
	void send_back_error(int code);

	/**
	 * Returns transaction id.
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
	 * Returns waiting service id that set by @c wait_next_phase
	 */
	int waiting_service_id() const;

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
	int m_current_phase;
	int m_first_conn_id;
	PackageTriggerSource m_first_trigger_source;
	int m_wait_conn_id;
	int m_wait_service_id;
	int m_wait_cmd;
	bool m_is_waiting;
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

	SPACELESS_AUTO_REG_MANAGER(MultiplyPhaseTransactionManager);

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

	/**
	 * Returns number of multiply phase transaction.
	 */
	std::size_t size();

	/**
	 * Binds transaction with package.
	 */
	void bind_transaction(int trans_id, int package_id);

	/**
	 * Removes bound relationship.
	 */
	void remove_bound_transaction(int package_id);

	/**
	 * Finds multiply phase transaction.
	 * @note Returns nullptr if cannot find bound relationship or multiply phase transaction.
	 */
	MultiplyPhaseTransaction* find_bound_transaction(int package_id);

private:
	int m_next_id = 1;
	std::map<int, MultiplyPhaseTransaction*> m_trans_list;
	std::map<int, int> m_bind_list;
};


using OnePhaseTransaction = void (*)(int conn_id, Package);


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
	Transaction(TransactionType trans_type, void* trans_handler, TransactionErrorHandler error_handler) :
		trans_type(trans_type),
		trans_handler(trans_handler),
		error_handler(error_handler)
	{}

	TransactionType trans_type;
	void* trans_handler;
	TransactionErrorHandler error_handler;
};


/**
 * Manager of transaction. When receive a command will trigger associated transaction.
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
										OnePhaseTransaction transaction,
										TransactionErrorHandler error_handler = on_transaction_error);

	void register_one_phase_transaction(const protocol::Message& msg,
										OnePhaseTransaction transaction,
										TransactionErrorHandler error_handler = on_transaction_error);

	/**
	 * Registers association of command with multiply phase transaction.
 	 * @param trans_factory  Factory of multiply phase transaction.
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


#define SPACELESS_REG_ONE_TRANS(ProtocolType, ...) \
		TransactionManager::instance()->register_one_phase_transaction(ProtocolType(), __VA_ARGS__);

#define SPACELESS_REG_MULTIPLE_TRANS(ProtocolType, ...) \
		TransactionManager::instance()->register_multiply_phase_transaction(ProtocolType(), __VA_ARGS__);


// ================================= Inline implement. =================================

inline void Network::send_back_protocol(int conn_id,
										const protocol::Message& msg,
										Package trigger_package,
										int bind_trans_id)
{
	send_protocol(conn_id,
				  msg,
				  bind_trans_id,
				  trigger_package.header().extend.self_package_id,
				  trigger_package.header().base.command);
}

inline void Network::send_back_protocol(int conn_id,
										const protocol::Message& msg,
										const PackageTriggerSource& trigger_source,
										int bind_trans_id)
{
	send_protocol(conn_id, msg, bind_trans_id, trigger_source.package_id, trigger_source.command);
}

inline void Network::service_send_package(int service_id, Package package)
{
	send_package(0, package, service_id);
}

inline void Network::service_send_protocol(int service_id, const protocol::Message& msg, int bind_trans_id)
{
	send_protocol(0, msg, bind_trans_id, 0, 0, service_id);
}


inline void MultiplyPhaseTransaction::wait_next_phase(int conn_id,
													  const protocol::Message& msg,
													  int current_phase,
													  int timeout)
{
	auto cmd = protocol::get_command(msg);
	wait_next_phase(conn_id, cmd, current_phase, timeout);
}

inline void MultiplyPhaseTransaction::service_wait_next_phase(int service_id, int cmd, int current_phase, int timeout)
{
	wait_next_phase(0, cmd, current_phase, timeout, service_id);
}

inline void MultiplyPhaseTransaction::service_wait_next_phase(int service_id,
															  const protocol::Message& msg,
															  int current_phase,
															  int timeout)
{
	auto cmd = protocol::get_command(msg);
	service_wait_next_phase(service_id, cmd, current_phase, timeout);
}

inline void MultiplyPhaseTransaction::send_back_message(const protocol::Message& msg)
{
	Network::send_back_protocol(m_first_conn_id, msg, m_first_trigger_source);
}

inline int MultiplyPhaseTransaction::transaction_id() const
{
	return m_id;
}

inline int MultiplyPhaseTransaction::current_phase() const
{
	return m_current_phase;
}

inline int MultiplyPhaseTransaction::first_connection_id() const
{
	return m_first_conn_id;
}

inline const PackageTriggerSource& MultiplyPhaseTransaction::first_trigger_source() const
{
	return m_first_trigger_source;
}

inline int MultiplyPhaseTransaction::waiting_connection_id() const
{
	return m_wait_conn_id;
}

inline int MultiplyPhaseTransaction::waiting_service_id() const
{
	return m_wait_service_id;
}

inline int MultiplyPhaseTransaction::waiting_command() const
{
	return m_wait_cmd;
}

inline bool MultiplyPhaseTransaction::is_waiting() const
{
	return m_is_waiting;
}

inline void MultiplyPhaseTransaction::clear_waiting_state()
{
	m_is_waiting = false;
}


} // namespace spaceless
