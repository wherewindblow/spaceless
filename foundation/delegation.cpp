/**
 * delegation.cpp
 * @author wherewindblow
 * @date   Feb 02, 2019
 */

#include "delegation.h"

#include "network.h"


namespace spaceless {

void Delegation::delegate(ThreadTarget target, std::function<void()> func)
{
	NetworkMessage msg;
	msg.delegate = func;
	NetworkMessageQueue::QueueType queue_type = NetworkMessageQueue::OUT_QUEUE;
	if (target == ThreadTarget::WORKER)
	{
		queue_type = NetworkMessageQueue::IN_QUEUE;
	}
	NetworkMessageQueue::instance()->push(queue_type, msg);
}

} // namespace spaceless
