/*
 * network.cpp
 * @author wherewindblow
 * @date   Nov 01, 2017
 */

#include "network.h"

#include <execinfo.h>
#include <Poco/Net/NetException.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/AbstractObserver.h>
#include <Poco/NObserver.h>

#include "log.h"
#include "worker.h"


namespace lights {

template <typename Backend>
FormatSink<Backend> operator<< (FormatSink<Backend> sink, const Poco::Net::SocketAddress& address)
{
	sink << address.toString();
	return sink;
}

} // namespace lights


namespace spaceless {

static Logger& logger = get_logger("network");

using Poco::Net::SocketAddress;

namespace details {

template <class C, class N>
class Observer: public Poco::NObserver<C, N>
{
public:
	typedef Poco::NObserver<C, N> BaseObserver;
	typedef Poco::AutoPtr<N> NotificationPtr;
	typedef void (C::*Callback)(const NotificationPtr&);

	Observer(C& object, Callback method, int conn_id):
		BaseObserver(object, method),
		m_conn_id(conn_id)
	{
	}

	Observer(const Observer& observer):
		BaseObserver(observer),
		m_conn_id(observer.m_conn_id)
	{
	}

	Observer& operator=(const Observer& observer)
	{
		if (&observer != this)
		{
			static_cast<BaseObserver&>(*this) = observer;
			m_conn_id = observer.m_conn_id;
		}
		return *this;
	}

	void notify(Poco::Notification* pNf) const
	{
		NetworkConnection* conn = NetworkConnectionManager::instance()->find_connection(m_conn_id);
		if (conn != nullptr)
		{
			BaseObserver::notify(pNf);
		}
	}

private:
	int m_conn_id;
};

} // namespace details


NetworkConnection::NetworkConnection(StreamSocket& socket, SocketReactor& reactor) :
	m_socket(socket),
	m_reactor(reactor),
	m_read_buffer(0),
	m_readed_len(0),
	m_read_state(ReadState::READ_HEADER),
	m_sended_len(0)
{
	m_socket.setBlocking(false);

	details::Observer<NetworkConnection, ReadableNotification> readable(*this, &NetworkConnection::on_readable, m_id);
	m_reactor.addEventHandler(m_socket, readable);
	details::Observer<NetworkConnection, ShutdownNotification> shutdown(*this, &NetworkConnection::on_shutdown, m_id);
	m_reactor.addEventHandler(m_socket, shutdown);
	details::Observer<NetworkConnection, ErrorNotification> error(*this, &NetworkConnection::on_error, m_id);
	m_reactor.addEventHandler(m_socket, error);

	m_id = NetworkConnectionManager::instance()->on_create_connection(this);

	try
	{
		std::string address = m_socket.address().toString();
		std::string peer_address = m_socket.peerAddress().toString();
		LIGHTS_INFO(logger, "Creates network connection {}: local {} and peer {}.", m_id, address, peer_address);
	}
	catch (Poco::Exception& ex)
	{
		LIGHTS_INFO(logger, "Creates network connection {}: local unknow and peer unknow.", m_id);
	}
}


NetworkConnection::~NetworkConnection()
{
	try
	{
		LIGHTS_INFO(logger, "Destroys network connection {}.", m_id);
		NetworkConnectionManager::instance()->on_destroy_connection(m_id);

		details::Observer<NetworkConnection, ReadableNotification> readable(*this, &NetworkConnection::on_readable, m_id);
		m_reactor.removeEventHandler(m_socket, readable);
		details::Observer<NetworkConnection, WritableNotification> writable(*this, &NetworkConnection::on_writable, m_id);
		m_reactor.removeEventHandler(m_socket, writable);
		details::Observer<NetworkConnection, ShutdownNotification> shutdown(*this, &NetworkConnection::on_shutdown, m_id);
		m_reactor.removeEventHandler(m_socket, shutdown);
		details::Observer<NetworkConnection, ErrorNotification> error(*this, &NetworkConnection::on_error, m_id);
		m_reactor.removeEventHandler(m_socket, error);

		while (!m_send_list.empty())
		{
			int package_id = m_send_list.front();
			m_send_list.pop();
			PackageBufferManager::instance()->remove_package(package_id);
		}

		try
		{
			m_socket.shutdown();
			m_socket.close();
		}
		catch (Poco::Net::NetException&)
		{
		}
	}
	catch (std::exception& ex)
	{
		LIGHTS_ERROR(logger, "Connction {}: Destroy error: {}.", m_id, ex.what());
	}
	catch (...)
	{
		LIGHTS_ERROR(logger, "Connction {}: Destroy unkown error.", m_id);
	}
}


int NetworkConnection::connection_id() const
{
	return m_id;
}


void NetworkConnection::on_readable(const Poco::AutoPtr<ReadableNotification>& notification)
{
	read_for_state();
}


void NetworkConnection::on_writable(const Poco::AutoPtr<WritableNotification>& notification)
{
	const PackageBuffer* package = nullptr;
	while (!m_send_list.empty())
	{
		package = PackageBufferManager::instance()->find_package(m_send_list.front());
		if (package == nullptr)
		{
			m_send_list.pop();
		}
		else
		{
			break;
		}
	}

	if (package == nullptr)
	{
		return;
	}

	int len = static_cast<int>(package->total_length());
	m_sended_len += m_socket.sendBytes(package->data() + m_sended_len, len - m_sended_len);

	if (m_sended_len == len)
	{
		m_sended_len = 0;
		m_send_list.pop();
		PackageBufferManager::instance()->remove_package(package->package_id());
	}

	if (m_send_list.empty())
	{
		details::Observer<NetworkConnection, WritableNotification> observer(*this, &NetworkConnection::on_writable, m_id);
		m_reactor.removeEventHandler(m_socket, observer);
	}
}


void NetworkConnection::on_shutdown(const Poco::AutoPtr<ShutdownNotification>& notification)
{
	close();
}


void NetworkConnection::on_error(const Poco::AutoPtr<ErrorNotification>& notification)
{
	LIGHTS_ERROR(logger, "Connction {}: On error.", m_id);
}


void NetworkConnection::send_package(const PackageBuffer& package)
{
	LIGHTS_DEBUG(logger, "Connction {}: Send package cmd {}, trans_id {}.",
					m_id, package.header().command, package.header().trigger_trans_id)

	if (!m_send_list.empty())
	{
		m_send_list.push(package.package_id());
		return;
	}

	int len = static_cast<int>(package.total_length());
	m_sended_len = m_socket.sendBytes(package.data(), len);
	if (m_sended_len == len)
	{
		m_sended_len = 0;
		PackageBufferManager::instance()->remove_package(package.package_id());
	}
	else if (m_sended_len < len)
	{
		m_send_list.push(package.package_id());
		details::Observer<NetworkConnection, WritableNotification> observer(*this, &NetworkConnection::on_writable, m_id);
		m_reactor.addEventHandler(m_socket, observer);
	}
}


void NetworkConnection::close()
{
	delete this;
}


StreamSocket& NetworkConnection::stream_socket()
{
	return m_socket;
}


void NetworkConnection::read_for_state(int deep)
{
	if (deep > 5) // Reduce call recursive too deeply.
	{
		return;
	}

	switch (m_read_state)
	{
		case ReadState::READ_HEADER:
		{
			int len = m_socket.receiveBytes(m_read_buffer.data() + m_readed_len,
											static_cast<int>(PackageBuffer::MAX_HEADER_LEN) - m_readed_len);

			if (len == -1) // Not available bytes in buffer.
			{
				return;
			}

			if (len == 0 && deep == 0) // Closes by peer.
			{
				close();
				return;
			}

			m_readed_len += len;
			if (m_readed_len == PackageBuffer::MAX_HEADER_LEN)
			{
				m_readed_len = 0;
				m_read_state = ReadState::READ_CONTENT;
			}
			break;
		}

		case ReadState::READ_CONTENT:
		{
			if (m_read_buffer.header().content_length != 0)
			{
				int len = m_socket.receiveBytes(m_read_buffer.data() + PackageBuffer::MAX_HEADER_LEN + m_readed_len,
												m_read_buffer.header().content_length - m_readed_len);
				if (len == -1) // Not available bytes in buffer.
				{
					return;
				}

				if (len == 0 && deep == 0) // Closes by peer.
				{
					close();
					return;
				}

				m_readed_len += len;
			}

			if (m_readed_len == m_read_buffer.header().content_length)
			{
				m_readed_len = 0;
				m_read_state = ReadState::READ_HEADER;

				LIGHTS_DEBUG(logger, "Connction {}: Recieve package cmd {}, trans_id {}.",
								m_id, m_read_buffer.header().command, m_read_buffer.header().trigger_trans_id)

				PackageBuffer& package = PackageBufferManager::instance()->register_package();
				package = m_read_buffer;
				NetworkMessage msg = { m_id, package.package_id() };
				NetworkMessageQueue::instance()->push(NetworkMessageQueue::IN_QUEUE, msg);
			}
			break;
		}
	}

	read_for_state(++deep);
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


void NetworkReactor::onIdle()
{
	LIGHTS_DEBUG(logger, "onIdle");
	process_send_package();
	SocketReactor::onIdle();
}


void NetworkReactor::onBusy()
{
	LIGHTS_DEBUG(logger, "onBusy");
	process_send_package();
	SocketReactor::onBusy();
}


void NetworkReactor::onTimeout()
{
//	LIGHTS_DEBUG(logger, "onTimeout");
	process_send_package();
	SocketReactor::onTimeout();
}


void NetworkReactor::process_send_package()
{
	for (std::size_t i = 0; i < MAX_SEND_PACKAGE_PROCESS_PER_TIMES; ++i)
	{
		if (NetworkMessageQueue::instance()->empty(NetworkMessageQueue::OUT_QUEUE))
		{
			break;
		}

		auto msg = NetworkMessageQueue::instance()->pop(NetworkMessageQueue::OUT_QUEUE);
		NetworkConnection* conn = NetworkConnectionManager::instance()->find_connection(msg.conn_id);
		PackageBuffer* package = PackageBufferManager::instance()->find_package(msg.package_id);

		if (conn == nullptr || package == nullptr)
		{
			if (conn == nullptr)
			{
				LIGHTS_INFO(logger, "Connction {}: Already close.", msg.conn_id);
			}

			if (package == nullptr)
			{
				LIGHTS_ERROR(logger, "Connction {}: Package {} already remove.", msg.conn_id, msg.package_id);
			}

			if (package)
			{
				PackageBufferManager::instance()->remove_package(msg.package_id);
			}

			continue;
		}

		conn->send_package(*package);
	}
}


NetworkConnectionManager::~NetworkConnectionManager()
{
	try
	{
		stop_all();
	}
	catch (std::exception& ex)
	{
		LIGHTS_ERROR(logger, "Network connection manager destroy error: {}.", ex.what());
	}
	catch (...)
	{
		LIGHTS_ERROR(logger, "Network connection manager destroy unkown error.");
	}
}


NetworkConnection& NetworkConnectionManager::register_connection(const std::string& host, unsigned short port)
{
	StreamSocket stream_socket(SocketAddress(host, port));
	NetworkConnection* conn = new NetworkConnection(stream_socket, m_reactor);
	return *conn;
}


void NetworkConnectionManager::register_listener(const std::string& host, unsigned short port)
{
	ServerSocket server_socket(SocketAddress(host, port));
	m_acceptor_list.emplace_back(server_socket, m_reactor);
	LIGHTS_INFO(logger, "Creates network listener {}.", server_socket.address());
}


void NetworkConnectionManager::remove_connection(int conn_id)
{
	NetworkConnection* conn = find_connection(conn_id);
	if (conn)
	{
		conn->close();
	}
}


NetworkConnection* NetworkConnectionManager::find_connection(int conn_id)
{
	auto itr = m_conn_list.find(conn_id);
	if (itr == m_conn_list.end())
	{
		return nullptr;
	}

	return itr->second;
}


NetworkConnection& NetworkConnectionManager::get_connection(int conn_id)
{
	NetworkConnection* conn = find_connection(conn_id);
	if (conn == nullptr)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_NETWORK_CONNECTION_NOT_EXIST);
	}

	return *conn;
}


void NetworkConnectionManager::stop_all()
{
	// Cannot use iterator to for each to close, becase close will erase itself on m_conn_list.
	while (!m_conn_list.empty())
	{
		NetworkConnection* conn =  m_conn_list.begin()->second;
		conn->close();
	}
	m_conn_list.clear();
	m_acceptor_list.clear();
}


void NetworkConnectionManager::start()
{
	LIGHTS_INFO(logger, "Starting netowk scheduler.");
	m_reactor.setTimeout(Poco::Timespan(0, REACTOR_TIME_OUT_US));
	m_reactor.run();
	LIGHTS_INFO(logger, "Stopped netowk scheduler.");
}


void NetworkConnectionManager::stop()
{
	LIGHTS_INFO(logger, "Stopping netowk scheduler.");
	m_reactor.stop();
}


int NetworkConnectionManager::on_create_connection(NetworkConnection* conn)
{
	int conn_id = m_next_id;
	++m_next_id;
	m_conn_list.insert(std::make_pair(conn_id, conn));
	return conn_id;
}


void NetworkConnectionManager::on_destroy_connection(int conn_id)
{
	m_conn_list.erase(conn_id);
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
		NetworkConnectionManager::instance()->remove_connection(itr->second);
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
		NetworkConnection& conn = NetworkConnectionManager::instance()->register_connection(service->ip, service->port);
		m_conn_list.insert(std::make_pair(service_id, conn.connection_id()));
		return conn.connection_id();
	}

	NetworkConnection* conn = NetworkConnectionManager::instance()->find_connection(itr->second);
	if (conn == nullptr)
	{
		NetworkConnection& new_conn = NetworkConnectionManager::instance()->register_connection(service->ip, service->port);
		m_conn_list[service_id] = new_conn.connection_id();
		return new_conn.connection_id();;
	}

	return conn->connection_id();
}

} // namespace spaceless
