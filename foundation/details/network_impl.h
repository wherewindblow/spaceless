/**
 * network_impl.h
 * @author wherewindblow
 * @date   Feb 02, 2019
 */

#pragma once

#include <queue>
#include <set>

#include <crypto/rsa.h>
#include <Poco/Net/SocketAcceptor.h>
#include <Poco/Net/SocketReactor.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/StreamSocket.h>

#include "../basics.h"
#include "../package.h"


namespace spaceless {

struct NetworkMessage;

namespace details {

using Poco::Net::SocketAcceptor;
using Poco::Net::SocketReactor;
using Poco::Net::ServerSocket;
using Poco::Net::StreamSocket;
using Poco::Net::ReadableNotification;
using Poco::Net::WritableNotification;
using Poco::Net::ShutdownNotification;
using Poco::Net::TimeoutNotification;
using Poco::Net::ErrorNotification;

class SecureConnection;


/**
 * Network connection open type.
 */
enum class ConnectionOpenType
{
	ACTIVE_OPEN,
	PASSIVE_OPEN,
};


/**
 * NetworkConnection handler socket notification and cache receive message.
 * @note Only operate this class in the thread that run @ NetworkConnectionManager::run.
 */
class NetworkConnectionImpl
{
public:
	/**
	 * Creates the NetworkConnection and add event handler.
	 * @note Do not create in stack.
	 */
	NetworkConnectionImpl(StreamSocket& socket,
						  SocketReactor& reactor,
						  ConnectionOpenType open_type = ConnectionOpenType::PASSIVE_OPEN);

	/**
	 * Destroys the NetworkConnection and remove event handler.
	 */
	~NetworkConnectionImpl();

	/**
	 * Returns connection id.
	 */
	int connection_id() const;

	/**
	 * Returns open type.
	 */
	ConnectionOpenType open_type() const;

	/**
	 * Sends raw package to remote asynchronously.
	 */
	void send_raw_package(Package package);

	/**
	 * Sends package that append additional process to remote asynchronously.
	 */
	void send_package(Package package);

	/**
	 * Destroys connection.
	 * @note Cannot use connection after close it.
	 */
	void close();

	/**
	 * Returns connection is open.
	 */
	bool is_open() const;

private:
	enum class ReadState
	{
		READ_HEADER,
		READ_CONTENT,
	};

	/**
	 * Handlers readable notification.
	 */
	void on_readable(const Poco::AutoPtr<ReadableNotification>& notification);

	/**
	 * Handlers writable notification of socket (send buffer is not full).
	 */
	void on_writable(const Poco::AutoPtr<WritableNotification>& notification);

	/**
	 * Handlers error notification of socket.
	 */
	void on_error(const Poco::AutoPtr<ErrorNotification>& notification);

	/**
	 * Reads input according to read state.
	 */
	void read_for_state();

	/**
	 * On read a complete package event.
	 */
	bool on_read_complete_package(int read_content_len);

	/**
	 * Sends all pending package.
	 */
	void send_all_pending_package();

	/**
	 * Closes connection without waiting.
	 */
	void close_without_waiting();

	int m_id;
	StreamSocket m_socket;
	SocketReactor& m_reactor;
	ConnectionOpenType m_open_type;
	PackageBuffer m_receive_buffer;
	int m_receive_len;
	ReadState m_read_state;
	std::queue<int> m_send_list;
	int m_send_len;
	bool m_is_opening;
	bool m_is_closing;
	SecuritySetting security_setting;
	SecureConnection* m_secure_conn;
	std::queue<int> m_pending_list;
};


/**
 * SecureConnection uses to ensure security to transfer data.
 */
class SecureConnection
{
public:
	/**
	 * Creates secure connection.
	 */
	explicit SecureConnection(NetworkConnectionImpl* conn);

	/**
	 * Sends package.
	 */
	void send_package(Package package);

	/**
	 * On read a complete package.
	 */
	void on_read_complete_package(PackageBuffer& receive_buffer, int read_content_len);

	/**
	 * Gets content length that process with security.
	 */
	int get_content_length(int raw_length);

private:
	enum class CryptoState
	{
		STARTING,
		STARTED,
	};

	/**
	 * Sends all pending package.
	 */
	void send_all_pending_package();

	NetworkConnectionImpl* m_conn;
	CryptoState m_crypto_state;
	crypto::RsaPrivateKey m_private_key;
	crypto::AesKey m_key;
	std::queue<int> m_pending_list;
};


class NetworkReactor: public SocketReactor
{
public:
	/**
	 * Called if no sockets are available to call select() on.
	 */
	void onIdle() override;

	/**
	 * Called when the SocketReactor is busy and at least one notification has been dispatched.
	 */
	void onBusy() override;

	/**
	 * Called if the timeout expires and no other events are available.
	 */
	void onTimeout() override;

private:
	/**
	 * Process message that from worker thread.
	 */
	void process_out_message();

	/**
	 * Send package by network message.
	 */
	void send_package(const NetworkMessage& msg);

	/**
	 * Call function safety.
	 */
	bool safe_call_delegate(std::function<void()> function, lights::StringView caller);
};


/**
 * Manager of all network connection and listener. And schedule network event.
 */
class NetworkManagerImpl
{
public:
	SPACELESS_SINGLETON_INSTANCE(NetworkManagerImpl);

	/**
	 * Destroys the network connection manager.
	 */
	~NetworkManagerImpl();

	/**
	 * Registers network connection.
	 */
	NetworkConnectionImpl& register_connection(const std::string& host, unsigned short port);

	/**
	 * Registers network listener.
	 */
	void register_listener(const std::string& host, unsigned short port, SecuritySetting security_setting);

	/**
	 * Removes network connection.
	 */
	void remove_connection(int conn_id);

	/**
	 * Finds network connection.
	 * @note Returns nullptr if cannot find connection.
	 */
	NetworkConnectionImpl* find_connection(int conn_id);

	/**
	 * Finds network connection.
	 * @note Returns nullptr if cannot find connection.
	 */
	NetworkConnectionImpl* find_open_connection(int conn_id);

	/**
	 * Stops all network connection and listener.
	 */
	void stop_all();

	/**
	 * Starts to schedule network event.
	 * @note It'll block until it's stopped.
	 */
	void start();

	/**
	 * Stops of schedule network event. Sets stop flag to let scheduler to stop running.
	 * @note After this function return, the scheduler may be still running.
	 */
	void stop();

private:
	friend class NetworkConnectionImpl;

	/**
	 * On create connection event.
	 */
	int on_create_connection(NetworkConnectionImpl* conn);

	/**
	 * On destroy connection event.
	 */
	void on_destroy_connection(int conn_id);

	SecuritySetting get_security_setting(const std::string& address);

	int m_next_id = 1;
	std::map<int, NetworkConnectionImpl*> m_conn_list;
	std::list<SocketAcceptor<NetworkConnectionImpl>> m_acceptor_list;
	std::set<std::string> m_secure_listener_list;
	NetworkReactor m_reactor;
};

} // namespace details
} // namespace spaceless