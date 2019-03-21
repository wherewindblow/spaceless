/**
 * actor_message.h
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
 * ActorMessage uses to operation difference actor(thread).
 */
struct ActorMessage
{
	enum Type
	{
		NETWORK_TYPE = 1,
		DELEGATE_TYPE = 2,
	};

	struct NetworkMsg
	{
		NetworkMsg() :
			conn_id(0),
			service_id(0),
			package_id(0)
		{}

		int conn_id;
		int service_id;
		int package_id;
	};

	struct DelegateMsg
	{
		DelegateMsg() :
			function(),
			caller(lights::invalid_string_view())
		{}

		std::function<void()> function;
		lights::StringView caller;
	};

	Type type;
	NetworkMsg network_msg;
	DelegateMsg delegate_msg;
};


/**
 * Actor message queue include input queue and output queue. It's use to separate network thread and worker thread.
 * So all operation of this class is thread safe.
 */
class ActorMessageQueue
{
public:
	SPACELESS_SINGLETON_INSTANCE(ActorMessageQueue);

	enum QueueType
	{
		IN_QUEUE,
		OUT_QUEUE,
		MAX,
	};

	/**
	 * Pushes message to indicate queue.
	 */
	void push(QueueType queue_type, const ActorMessage& msg);

	/**
	 * Pops message of indicate queue.
	 */
	ActorMessage pop(QueueType queue_type);

	/**
	 * Checks indicate queue is empty.
	 */
	bool empty(QueueType queue_type);

	/**
	 * Gets size of indicate queue.
	 */
	std::size_t size(QueueType queue_type);

private:
	std::queue<ActorMessage> m_queue[QueueType::MAX];
	std::mutex m_mutex[QueueType::MAX];
};


} // namespace spaceless
