/*
 * network.cpp
 * @author wherewindblow
 * @date   Nov 01, 2017
 */

#include "network.h"

#include <execinfo.h>
#include <Poco/Net/NetException.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Observer.h>

#include "log.h"
#include "transaction.h"


namespace lights {

template <typename Sink>
FormatSinkAdapter<Sink> operator<< (FormatSinkAdapter<Sink> out, const Poco::Net::SocketAddress& address)
{
	out << address.toString();
	return out;
}

} // namespace lights


namespace spaceless {

using Poco::Net::SocketAddress;

NetworkConnection::NetworkConnection(StreamSocket& socket, SocketReactor& reactor) :
	m_socket(socket),
	m_reactor(reactor),
	m_read_buffer(0),
	m_readed_len(0),
	m_read_state(ReadState::READ_HEADER),
	m_sended_len(0)
{
	m_socket.setBlocking(false);

	Poco::Observer<NetworkConnection, ReadableNotification> readable(*this, &NetworkConnection::on_readable);
	m_reactor.addEventHandler(m_socket, readable);
	Poco::Observer<NetworkConnection, ShutdownNotification> shutdown(*this, &NetworkConnection::on_shutdown);
	m_reactor.addEventHandler(m_socket, shutdown);
//	Poco::Observer<NetworkConnection, TimeoutNotification> timeout(*this, &NetworkConnection::on_timeout);
//	m_reactor.addEventHandler(m_socket, timeout);
	Poco::Observer<NetworkConnection, ErrorNotification> error(*this, &NetworkConnection::on_error);
	m_reactor.addEventHandler(m_socket, error);

	NetworkConnectionManager::instance()->m_conn_list.push_back(this);
	m_id = NetworkConnectionManager::instance()->m_next_id;
	++NetworkConnectionManager::instance()->m_next_id;

	try
	{
		std::string address = m_socket.address().toString();
		std::string peer_address = m_socket.peerAddress().toString();
		SPACELESS_INFO(MODULE_NETWORK, "Creates network connection {}: local {} and peer {}.",
						m_id, address, peer_address);
	}
	catch (Poco::Exception& ex)
	{
		SPACELESS_INFO(MODULE_NETWORK, "Creates network connection {}: local unknow and peer unknow.", m_id);
	}
}


NetworkConnection::~NetworkConnection()
{
	try
	{
		SPACELESS_DEBUG(MODULE_NETWORK, "Destroys network connection {}.", m_id);
		auto& conn_list = NetworkConnectionManager::instance()->m_conn_list;
		auto need_delete = std::find_if(conn_list.begin(), conn_list.end(), [this](const NetworkConnection* conn)
		{
			return conn->connection_id() == this->connection_id();
		});
		conn_list.erase(need_delete);

		Poco::Observer<NetworkConnection, ReadableNotification> readable(*this, &NetworkConnection::on_readable);
		m_reactor.removeEventHandler(m_socket, readable);
		Poco::Observer<NetworkConnection, ShutdownNotification> shutdown(*this, &NetworkConnection::on_shutdown);
		m_reactor.removeEventHandler(m_socket, shutdown);
		//	Poco::Observer<NetworkConnection, TimeoutNotification> timeout(*this, &NetworkConnection::on_timeout);
		//	m_reactor.removeEventHandler(m_socket, timeout);
		Poco::Observer<NetworkConnection, ErrorNotification> error(*this, &NetworkConnection::on_error);
		m_reactor.removeEventHandler(m_socket, error);

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
		SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: Destroy error: {}", m_id, ex.what());
	}
	catch (...)
	{
		SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: Destroy unkown error", m_id);
	}
}


int NetworkConnection::connection_id() const
{
	return m_id;
}


void NetworkConnection::on_readable(ReadableNotification* notification)
{
	notification->release();
	read_for_state();
}


void NetworkConnection::on_writable(WritableNotification* notification)
{
	notification->release();
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
		Poco::Observer<NetworkConnection, WritableNotification> observer(*this, &NetworkConnection::on_writable);
		m_reactor.removeEventHandler(m_socket, observer);
	}
}


void NetworkConnection::on_shutdown(ShutdownNotification* notification)
{
	notification->release();
	close();
}


void NetworkConnection::on_timeout(TimeoutNotification* notification)
{
	notification->release();
	SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: On time out.", m_id);
}


void NetworkConnection::on_error(ErrorNotification* notification)
{
	notification->release();
	SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: On error.", m_id);
}


void NetworkConnection::send_package(const PackageBuffer& package)
{
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
		Poco::Observer<NetworkConnection, WritableNotification> observer(*this, &NetworkConnection::on_writable);
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
				m_readed_len += len;
			}

			if (m_readed_len == m_read_buffer.header().content_length)
			{
				m_readed_len = 0;
				m_read_state = ReadState::READ_HEADER;

				PackageBuffer& package = PackageBufferManager::instance()->register_package();
				package = m_read_buffer;
				NetworkMessageQueue::Message msg = { connection_id(), package.package_id() };
				NetworkMessageQueue::instance()->push(NetworkMessageQueue::IN_QUEUE, msg);
			}
			break;
		}
	}

	read_for_state(++deep);
}


void NetworkMessageQueue::push(QueueType queue_type, const Message& msg)
{
	std::lock_guard<std::mutex> lock(m_mutex[queue_type]);
	m_queue[queue_type].push(msg);
}


NetworkMessageQueue::Message NetworkMessageQueue::pop(QueueType queue_type)
{
	std::lock_guard<std::mutex> lock(m_mutex[queue_type]);
	Message msg = m_queue[queue_type].front();
	m_queue[queue_type].pop();
	return msg;
}


bool NetworkMessageQueue::empty(NetworkMessageQueue::QueueType queue_type)
{
	std::lock_guard<std::mutex> lock(m_mutex[queue_type]);
	return m_queue[queue_type].empty();
}


void NetworkReactor::onIdle()
{
	SPACELESS_DEBUG(MODULE_NETWORK, "onIdle");
	process_send_package();
}


void NetworkReactor::onBusy()
{
	SPACELESS_DEBUG(MODULE_NETWORK, "onBusy");
	process_send_package();
}


void NetworkReactor::onTimeout()
{
//	SPACELESS_DEBUG(MODULE_NETWORK, "onTimeout");
	process_send_package();
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
		if (!conn)
		{
			SPACELESS_INFO(MODULE_NETWORK, "Network connction {}: Already close", msg.conn_id);
			break;
		}

		PackageBuffer* package = PackageBufferManager::instance()->find_package(msg.package_id);
		if (!package)
		{
			SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: Package {} already remove", msg.conn_id, msg.package_id);
			break;
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
		SPACELESS_ERROR(MODULE_NETWORK, "Network connection manager destroy error: {}", ex.what());
	}
	catch (...)
	{
		SPACELESS_ERROR(MODULE_NETWORK, "Network connection manager destroy unkown error");
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
	SPACELESS_INFO(MODULE_NETWORK, "Creates network listener {}.", server_socket.address());
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
	auto itr = std::find_if(m_conn_list.begin(), m_conn_list.end(), [conn_id](const NetworkConnection* connection)
	{
		return connection->connection_id() == conn_id;
	});

	if (itr == m_conn_list.end())
	{
		return nullptr;
	}

	return *itr;
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
		(*m_conn_list.begin())->close();
	}
	m_conn_list.clear();
	m_acceptor_list.clear();
}


void NetworkConnectionManager::run()
{
	TransactionScheduler::instance()->start();

	m_reactor.setTimeout(Poco::Timespan(0, REACTOR_TIME_OUT_US));
	m_reactor.run();
}

} // namespace spaceless
