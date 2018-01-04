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


namespace spaceless {

using Poco::Net::SocketAddress;

PackageBuffer& PackageBufferManager::register_package()
{
	auto pair = m_package_list.emplace(m_next_id, PackageBuffer(m_next_id));
	++m_next_id;
	if (pair.second == false)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_NETWORK_PACKAGE_CANNOT_REGISTER);
	}
	return pair.first->second;
}


void PackageBufferManager::remove_package(int package_id)
{
	m_package_list.erase(package_id);
}


PackageBuffer* PackageBufferManager::find_package(int package_id)
{
	auto itr = m_package_list.find(package_id);
	if (itr == m_package_list.end())
	{
		return nullptr;
	}

	return &itr->second;
}


PackageBuffer& PackageBufferManager::get_package(int package_id)
{
	PackageBuffer* package = find_package(package_id);
	if (package == nullptr)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_NETWORK_PACKAGE_NOT_EXIST);
	}

	return *package;
}


void CommandHandlerManager::register_command(int cmd, CommandHandlerManager::CommandHandler callback)
{
	m_handler_list.insert(std::make_pair(cmd, callback));
}


void CommandHandlerManager::remove_command(int cmd)
{
	m_handler_list.erase(cmd);
}


CommandHandlerManager::CommandHandler CommandHandlerManager::find_handler(int cmd)
{
	auto itr = m_handler_list.find(cmd);
	if (itr == m_handler_list.end())
	{
		return nullptr;
	}
	return itr->second;
}


NetworkConnection::NetworkConnection(StreamSocket& socket, SocketReactor& reactor) :
	m_socket(socket),
	m_reactor(reactor),
	m_read_buffer(0),
	m_readed_len(0),
	m_read_state(ReadState::READ_HEADER),
	m_sended_len(0),
	m_attachment(nullptr)
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

	NetworkConnectionManager::instance()->m_conn_list.emplace_back(this);
	m_id = NetworkConnectionManager::instance()->m_next_id;
	++NetworkConnectionManager::instance()->m_next_id;

	try
	{
		std::string address = m_socket.address().toString();
		std::string peer_address = m_socket.peerAddress().toString();
		SPACELESS_DEBUG(MODULE_NETWORK, "Creates a network connection: id:{}, address:{}/{}.",
						m_id, address, peer_address);
	}
	catch (Poco::Exception& ex)
	{
		SPACELESS_DEBUG(MODULE_NETWORK, "Creates a network connection: id:{}, address:unkown/unkown.", m_id);
	}
}


NetworkConnection::~NetworkConnection()
{
	try
	{
		SPACELESS_DEBUG(MODULE_NETWORK, "Destroys a network connection: id:{}.", m_id);
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
		SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: {}", m_id, ex.what());
	}
	catch (...)
	{
		SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: Unkown error", m_id);
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
	}

	int len = static_cast<int>(package.total_length());
	m_sended_len = m_socket.sendBytes(package.data(), len);
	if (m_sended_len == len)
	{
		m_sended_len = 0;
	}
	else if (m_sended_len < len)
	{
		m_send_list.push(package.package_id());
		Poco::Observer<NetworkConnection, WritableNotification> writable_observer(*this, &NetworkConnection::on_writable);
		m_reactor.addEventHandler(m_socket, writable_observer);
	}
}


void NetworkConnection::close()
{
	delete this;
}


void NetworkConnection::set_attachment(void* attachment)
{
	m_attachment = attachment;
}


void* NetworkConnection::get_attachment()
{
	return m_attachment;
}


StreamSocket& NetworkConnection::stream_socket()
{
	return m_socket;
}


const StreamSocket& NetworkConnection::stream_socket() const
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

			if (len == -1)
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
			int len = m_socket.receiveBytes(m_read_buffer.data() + PackageBuffer::MAX_HEADER_LEN + m_readed_len,
											m_read_buffer.header().content_length - m_readed_len);

			if (len == -1)
			{
				return;
			}

			m_readed_len += len;
			if (m_readed_len == m_read_buffer.header().content_length)
			{
				m_readed_len = 0;
				m_read_state = ReadState::READ_HEADER;

				auto handler = CommandHandlerManager::instance()->find_handler(m_read_buffer.header().command);
				if (handler)
				{
					SPACELESS_DEBUG(MODULE_NETWORK, "Network connction {}: Trigger cmd {}.",
									m_id, m_read_buffer.header().command);
					try
					{
						handler(*this, m_read_buffer);
					}
					catch (const Exception& ex)
					{
						SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: {}/{}.", m_id, ex.code(), ex);
					}
					catch (const std::exception& ex)
					{
						SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: {}.", m_id, ex.what());
					}
					catch (...)
					{
						SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: Unkown error.", m_id);
					}
				}
				else
				{
					SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: Unkown command {}.",
									m_id, m_read_buffer.header().command);
				}
			}
			break;
		}
	}

	read_for_state(++deep);
}


NetworkConnectionManager::~NetworkConnectionManager()
{
	try
	{
		stop_all();
	}
	catch (std::exception& ex)
	{
		SPACELESS_ERROR(MODULE_NETWORK, ex.what());
	}
	catch (...)
	{
		SPACELESS_ERROR(MODULE_NETWORK, "Unkown error");
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
	ServerSocket serve_socket(SocketAddress(host, port));
	m_acceptor_list.emplace_back(serve_socket, m_reactor);
	SPACELESS_DEBUG(MODULE_NETWORK, "Creates a network listener: {}.", serve_socket.address());
}


void NetworkConnectionManager::remove_connection(int conn_id)
{
	m_conn_list.remove_if([conn_id](const NetworkConnection* connection) {
		return connection->connection_id() == conn_id;
	});
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
	m_reactor.run();
}

} // namespace spaceless
