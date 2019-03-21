/**
 * delegation.cpp
 * @author wherewindblow
 * @date   Feb 02, 2019
 */

#include "delegation.h"
#include "actor_message.h"


namespace spaceless {

void Delegation::delegate(lights::StringView caller, ActorTarget actor, std::function<void()> function)
{
	ActorMessage actor_msg;
	actor_msg.type = ActorMessage::DELEGATE_TYPE;
	auto& msg = actor_msg.delegate_msg;
	msg.function = function;
	msg.caller = caller;

	ActorMessageQueue::QueueType queue_type = ActorMessageQueue::OUT_QUEUE;
	if (actor == ActorTarget::WORKER)
	{
		queue_type = ActorMessageQueue::IN_QUEUE;
	}

	ActorMessageQueue::instance()->push(queue_type, actor_msg);
}

} // namespace spaceless
