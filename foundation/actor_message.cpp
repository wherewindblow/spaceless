/**
 * actor_message.cpp
 * @author wherewindblow
 * @date   Feb 05, 2019
 */

#include "actor_message.h"


namespace spaceless {

void ActorMessageQueue::push(QueueType queue_type, const ActorMessage& msg)
{
	std::lock_guard<std::mutex> lock(m_mutex[queue_type]);
	m_queue[queue_type].push(msg);
}


ActorMessage ActorMessageQueue::pop(QueueType queue_type)
{
	std::lock_guard<std::mutex> lock(m_mutex[queue_type]);
	ActorMessage msg = m_queue[queue_type].front();
	m_queue[queue_type].pop();
	return msg;
}


bool ActorMessageQueue::empty(ActorMessageQueue::QueueType queue_type)
{
	std::lock_guard<std::mutex> lock(m_mutex[queue_type]);
	return m_queue[queue_type].empty();
}


std::size_t ActorMessageQueue::size(ActorMessageQueue::QueueType queue_type)
{
	std::lock_guard<std::mutex> lock(m_mutex[queue_type]);
	return m_queue[queue_type].size();
}

} // namespace spaceless
