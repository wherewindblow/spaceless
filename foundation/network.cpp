/*
 * network.cpp
 * @author wherewindblow
 * @date   Nov 01, 2017
 */

#include "network.h"

#include "log.h"
#include "worker.h"
#include "details/network_impl.h"


namespace spaceless {

NetworkConnection::NetworkConnection(details::NetworkConnectionImpl* impl) :
	p_impl(impl)
{
}


int NetworkConnection::is_valid() const
{
	return p_impl != nullptr;
}


int NetworkConnection::connection_id() const
{
	return p_impl->connection_id();
}


void NetworkConnection::send_package(Package package)
{
	p_impl->send_package(package);
}


void NetworkConnection::close()
{
	p_impl->close();
	p_impl = nullptr;
}


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


NetworkManager::NetworkManager() :
	p_impl(details::NetworkManagerImpl::instance())
{
}


NetworkConnection NetworkManager::register_connection(const std::string& host, unsigned short port)
{
	details::NetworkConnectionImpl& conn_impl = p_impl->register_connection(host, port);
	return NetworkConnection(&conn_impl);
}


void NetworkManager::register_listener(const std::string& host, unsigned short port)
{
	p_impl->register_listener(host, port);
}


void NetworkManager::remove_connection(int conn_id)
{
	p_impl->remove_connection(conn_id);
}


NetworkConnection NetworkManager::find_connection(int conn_id)
{
	return NetworkConnection(p_impl->find_open_connection(conn_id));
}


NetworkConnection NetworkManager::get_connection(int conn_id)
{
	NetworkConnection conn = find_connection(conn_id);
	if (!conn.is_valid())
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_NETWORK_CONNECTION_NOT_EXIST);
	}

	return conn;
}


void NetworkManager::stop_all()
{
	p_impl->stop_all();
}


void NetworkManager::start()
{
	p_impl->start();
}


void NetworkManager::stop()
{
	p_impl->stop();
}


NetworkService& NetworkServiceManager::register_service(const std::string& ip, unsigned short port)
{
	NetworkService* old_service = find_service(ip, port);
	if (old_service)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_NETWORK_SERVICE_ALREADY_EXIST);
	}

	NetworkService new_service{ m_next_id, ip, port };
	auto value = std::make_pair(m_next_id, new_service);
	++m_next_id;

	auto result = m_service_list.insert(value);
	if (result.second == false)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_NETWORK_SERVICE_ALREADY_EXIST);
	}

	return result.first->second;
}


void NetworkServiceManager::remove_service(int service_id)
{
	auto itr = m_conn_list.find(service_id);
	if (itr != m_conn_list.end())
	{
		NetworkManager::instance()->remove_connection(itr->second);
	}
	m_service_list.erase(service_id);
}


NetworkService* NetworkServiceManager::find_service(int service_id)
{
	auto itr = m_service_list.find(service_id);
	if (itr == m_service_list.end())
	{
		return nullptr;
	}

	return &(itr->second);
}


NetworkService* NetworkServiceManager::find_service(const std::string& ip, unsigned short port)
{
	auto itr = std::find_if(m_service_list.begin(), m_service_list.end(), [&](const ServiceList::value_type& value_type)
	{
		return value_type.second.ip == ip && value_type.second.port == port;
	});

	if (itr == m_service_list.end())
	{
		return nullptr;
	}

	return &(itr->second);
}


int NetworkServiceManager::get_connection_id(int service_id)
{
	NetworkService* service = find_service(service_id);
	if (service == nullptr)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_NETWORK_SERVICE_NOT_EXIST);
	}

	auto itr = m_conn_list.find(service_id);
	if (itr == m_conn_list.end())
	{
		NetworkConnection conn = NetworkManager::instance()->register_connection(service->ip, service->port);
		m_conn_list.insert(std::make_pair(service_id, conn.connection_id()));
		m_conn_service_list.insert(std::make_pair(conn.connection_id(), service_id));
		return conn.connection_id();
	}

	int conn_id = itr->second;
	NetworkConnection conn = NetworkManager::instance()->find_connection(conn_id);
	if (!conn.is_valid())
	{
		NetworkConnection new_conn = NetworkManager::instance()->register_connection(service->ip, service->port);
		m_conn_list[service_id] = new_conn.connection_id();
		m_conn_service_list.erase(conn_id);
		m_conn_service_list.insert(std::make_pair(new_conn.connection_id(), service_id));
		return new_conn.connection_id();
	}

	return conn.connection_id();
}


NetworkService* NetworkServiceManager::find_service_by_connection(int conn_id)
{
	auto itr = m_conn_service_list.find(conn_id);
	if (itr == m_conn_service_list.end())
	{
		return nullptr;
	}

	return find_service(itr->second);
}

} // namespace spaceless
