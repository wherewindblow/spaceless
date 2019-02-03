/**
 * network.h
 * @author wherewindblow
 * @date   Nov 01, 2017
 */

#pragma once

#include <queue>
#include <map>
#include <functional>

#include "basics.h"
#include "package.h"


/**
 * @note 1. All function that use in framework cannot throw exception. Because don't know who can catch it.
 *       2. All NetworkXXX cannot be use in worker thread except NetworkMessageQueue.
 */
namespace spaceless {

namespace details {

class NetworkConnectionImpl;
class NetworkManagerImpl;

} // namespace details


/**
 * NetworkConnection handler socket notification and cache receive message.
 * @note Only operate this class in the thread that run @ NetworkConnectionManager::run.
 */
class NetworkConnection
{
public:
	/**
	 * Creates connection.
	 */
	explicit NetworkConnection(details::NetworkConnectionImpl* impl);

	/**
	 * Checks connection is valid.
	 */
	int is_valid() const;

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
	 * @note Cannot uses connection after close it.
	 */
	void close();

private:
	details::NetworkConnectionImpl* p_impl;
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


/**
 * Manager of all network connection and listener. And schedule network event.
 */
class NetworkManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(NetworkManager);

	/**
	 * Creates network manager.
	 */
	NetworkManager();

	/**
	 * Registers network connection.
	 */
	NetworkConnection register_connection(const std::string& host, unsigned short port);

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
	NetworkConnection find_connection(int conn_id);

	/**
	 * Gets network connection.
	 * @throw Throws exception if cannot find connection.
	 */
	NetworkConnection get_connection(int conn_id);

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
	details::NetworkManagerImpl* p_impl;
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
