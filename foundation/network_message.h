/**
 * network_message.h
 * @author wherewindblow
 * @date   Feb 05, 2019
 */

#pragma once

#include <functional>
#include <queue>
#include <mutex>

#include <lights/sequence.h>

#include "basics.h"


namespace spaceless {

/**
 * NetworkMessage uses to operation difference thread.
 */
struct NetworkMessage
{
	NetworkMessage():
		conn_id(0),
		service_id(0),
		package_id(0),
		delegate(),
		caller(lights::invalid_string_view())
	{}

	int conn_id;
	int service_id;
	int package_id;
	std::function<void()> delegate;
	lights::StringView caller;
};


/**
 * Network message queue include input queue and output queue. It's use to separate network thread and worker thread.
 * So all operation of this class is thread safe.
 */
class NetworkMessageQueue
{
public:
	SPACELESS_SINGLETON_INSTANCE(NetworkMessageQueue);

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


} // namespace spaceless
