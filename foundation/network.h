/**
 * network.h
 * @author wherewindblow
 * @date   Nov 01, 2017
 */

#pragma once

#include <memory>
#include <list>
#include <queue>
#include <map>

#include <lights/sequence.h>
#include <Poco/Net/SocketAcceptor.h>
#include <Poco/Net/SocketReactor.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/StreamSocket.h>
#include <crypto/aes.h>
#include <crypto/rsa.h>

#include "basics.h"
#include "exception.h"
#include "package.h"


/**
 * @note 1. All function that use in framework cannot throw exception. Because don't know who can catch it.
 *       2. All NetworkXXX cannot be use in worker thread except NetworkMessageQueue.
 */
namespace spaceless {

using Poco::Net::SocketAcceptor;
using Poco::Net::SocketReactor;
using Poco::Net::ServerSocket;
using Poco::Net::StreamSocket;
using Poco::Net::ReadableNotification;
using Poco::Net::WritableNotification;
using Poco::Net::ShutdownNotification;
using Poco::Net::TimeoutNotification;
using Poco::Net::ErrorNotification;


/**
 * NetworkConnection handler socket notification and cache receive message.
 * @note Only operate this class in the thread that run @ NetworkConnectionManager::run.
 */
class NetworkConnection
{
public:
	enum OpenType
	{
		ACTIVE_OPEN,
		PASSIVE_OPEN,
	};

	/**
	 * Creates the NetworkConnection and add event handler.
	 * @note Do not create in stack.
	 */
	NetworkConnection(StreamSocket& socket, SocketReactor& reactor, OpenType open_type = PASSIVE_OPEN);

	/**
	 * Destroys the NetworkConnection and remove event handler.
	 */
	~NetworkConnection();

	/**
	 * Returns connection id.
	 */
	int connection_id() const;

	/**
	 * Sends package to remote on asynchronization.
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

	enum class CryptoState
	{
		STARTING,
		STARTED,
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
	 * Read input according to read state.
	 */
	void read_for_state();

	void on_read_complete_package(int read_content_len);

	void close_without_waiting();

	void send_raw_package(Package package);

	void send_not_crypto_package();

	int m_id;
	StreamSocket m_socket;
	SocketReactor& m_reactor;
	PackageBuffer m_receive_buffer;
	int m_receive_len;
	ReadState m_read_state;
	std::queue<int> m_send_list;
	int m_send_len;
	OpenType m_open_type;
	CryptoState m_crypto_state;
	crypto::RsaPrivateKey m_private_key;
	crypto::AesKey m_key;
	std::queue<int> m_not_crypto_list;
	bool m_is_closing;
};


struct NetworkMessage
{
	NetworkMessage():
		conn_id(0),
		service_id(0),
		package_id(0),
		delegate()
	{}

	int conn_id;
	int service_id;
	int package_id;
	std::function<void()> delegate;
};


/**
 * Network message queue include input queue and output queue. It's use to separate network thread and worker thread.
 * So all operation of this class is thread safe.
 */
class NetworkMessageQueue
{
public:
	SPACELESS_SINGLETON_INSTANCE(NetworkMessageQueue)

	enum QueueType
	{
		IN_QUEUE,
		OUT_QUEUE,
		MAX,
	};

	/**
	 * Pushes message to indicate queue.
	 */
	void push(QueueType queue_type, const NetworkMessage& msg);

	/**
	 * Pops message of indicate queue.
	 */
	NetworkMessage pop(QueueType queue_type);

	/**
	 * Checks indicate queue is empty.
	 */
	bool empty(QueueType queue_type);

	/**
	 * Gets size of indicate queue.
	 */
	std::size_t size(QueueType queue_type);

private:
	std::queue<NetworkMessage> m_queue[QueueType::MAX];
	std::mutex m_mutex[QueueType::MAX];
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
	void process_out_message();

	void send_package(const NetworkMessage& msg);
};


/**
 * Manager of all network connection and listener. And schedule network event.
 */
class NetworkManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(NetworkManager);

	/**
	 * Destroys the network connection manager.
	 */
	~NetworkManager();

	/**
	 * Registers network connection.
	 */
	NetworkConnection& register_connection(const std::string& host, unsigned short port);

	/**
	 * Registers network listener.
	 */
	void register_listener(const std::string& host, unsigned short port);

	/**
	 * Removes network connection.
	 */
	void remove_connection(int conn_id);

	/**
	 * Finds network connection.
	 * @note Returns nullptr if cannot find connection.
	 */
	NetworkConnection* find_connection(int conn_id);

	/**
	 * Gets network connection.
	 * @throw Throws exception if cannot find connection.
	 */
	NetworkConnection& get_connection(int conn_id);

	/**
	 * Finds network connection.
	 * @note Returns nullptr if cannot find connection.
	 */
	NetworkConnection* find_open_connection(int conn_id);

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
	friend class NetworkConnection;

	int on_create_connection(NetworkConnection* conn);

	void on_destroy_connection(int conn_id);

	int m_next_id = 1;
	std::map<int, NetworkConnection*> m_conn_list;
	std::list<SocketAcceptor<NetworkConnection>> m_acceptor_list;
	NetworkReactor m_reactor;
};


/**
 * NetworkService use to identify service and delay registration of network connection.
 */
struct NetworkService
{
	int service_id;
	std::string ip;
	unsigned short port;
};


/**
 * Manager of network service.
 */
class NetworkServiceManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(NetworkServiceManager);

	/**
	 * Registers network service and delay registration of network connection.
	 */
	NetworkService& register_service(const std::string& ip, unsigned short port);

	/**
	 * Removes network service.
	 */
	void remove_service(int service_id);

	/**
	 * Finds network service.
	 * @note Returns nullptr if cannot find service.
	 */
	NetworkService* find_service(int service_id);

	/**
	 * Finds network service.
	 * @note Returns nullptr if cannot find service.
	 */
	NetworkService* find_service(const std::string& ip, unsigned short port);

	/**
	 * Gets network connection id by service. When cannot find connection, will register network connection automatically.
	 * @throw Throws exception if cannot find service or cannot register network connection.
	 * @note  Cannot own it as member, because connection id may be change by some reason.
	 */
	int get_connection_id(int service_id);

	/**
	 * Finds network service by connection.
	 */
	NetworkService* find_service_by_connection(int conn_id);

private:
	using ServiceList = std::map<int, NetworkService>;
	ServiceList m_service_list;
	std::map<int, int> m_conn_list;
	std::map<int, int> m_conn_service_list;
	int m_next_id = 1;
};

} // namespace spaceless
