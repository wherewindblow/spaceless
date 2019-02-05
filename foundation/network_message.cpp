/**
 * network_message.cpp
 * @author wherewindblow
 * @date   Feb 05, 2019
 */

#include "network_message.h"


namespace spaceless {

void NetworkMessageQueue::push(QueueType queue_type, const NetworkMessage& msg)
{
	std::lock_guard<std::mutex> lock(m_mutex[queue_type]);
	m_queue[queue_type].push(msg);
}


NetworkMessage NetworkMessageQueue::pop(QueueType queue_type)
{
	std::lock_guard<std::mutex> lock(m_mutex[queue_type]);
	NetworkMessage msg = m_queue[queue_type].front();
	m_queue[queue_type].pop();
	return msg;
}


bool NetworkMessageQueue::empty(NetworkMessageQueue::QueueType queue_type)
{
	std::lock_guard<std::mutex> lock(m_mutex[queue_type]);
	return m_queue[queue_type].empty();
}


std::size_t NetworkMessageQueue::size(NetworkMessageQueue::QueueType queue_type)
{
	std::lock_guard<std::mutex> lock(m_mutex[queue_type]);
	return m_queue[queue_type].size();
}

} // namespace spaceless
