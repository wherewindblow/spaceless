/**
 * delegation.cpp
 * @author wherewindblow
 * @date   Feb 02, 2019
 */

#include "delegation.h"

#include "network.h"


namespace spaceless {

void Delegation::delegate(lights::StringView caller, ThreadTarget thread_target, std::function<void()> function)
{
	NetworkMessage msg;
	msg.delegate = function;
	msg.caller = caller;

	NetworkMessageQueue::QueueType queue_type = NetworkMessageQueue::OUT_QUEUE;
	if (thread_target == ThreadTarget::WORKER)
	{
		queue_type = NetworkMessageQueue::IN_QUEUE;
	}

	NetworkMessageQueue::instance()->push(queue_type, msg);
}

} // namespace spaceless
