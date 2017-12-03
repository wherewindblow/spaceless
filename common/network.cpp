/*
 * network.cpp
 * @author wherewindblow
 * @date   Nov 01, 2017
 */

#include "network.h"

#include <Poco/Net/SocketAddress.h>
#include <Poco/Observer.h>


namespace spaceless {

using Poco::Net::SocketAddress;


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


Connection::Connection(StreamSocket& socket, SocketReactor& reactor) :
	m_socket(socket),
	m_reactor(reactor),
	m_readed_len(0),
	m_read_state(ReadState::READ_HEADER)
{
	SPACELESS_DEBUG(MODULE_NETWORK, "Creates a connection: {}/{}", m_socket.address(), m_socket.peerAddress())
	ConnectionManager::instance()->m_conn_list.emplace_back(this);
	m_id = ConnectionManager::instance()->m_next_id;
	++ConnectionManager::instance()->m_next_id;

	Poco::Observer<Connection, ReadableNotification> readable_observer(*this, &Connection::on_readable);
	m_reactor.addEventHandler(m_socket, readable_observer);
	Poco::Observer<Connection, ShutdownNotification> shutdown_observer(*this, &Connection::on_shutdown);
	m_reactor.addEventHandler(m_socket, shutdown_observer);
	Poco::Observer<Connection, TimeoutNotification> timeout_observer(*this, &Connection::on_timeout);
	m_reactor.addEventHandler(m_socket, timeout_observer);
	Poco::Observer<Connection, ErrorNotification> error_observer(*this, &Connection::on_error);
	m_reactor.addEventHandler(m_socket, error_observer);
}


Connection::~Connection()
{
	SPACELESS_DEBUG(MODULE_NETWORK, "Destroys a connection: {}/{}", m_socket.address(), m_socket.peerAddress())
	try
	{
		auto& conn_list = ConnectionManager::instance()->m_conn_list;
		auto need_delete = std::find_if(conn_list.begin(), conn_list.end(), [this](const Connection* conn)
		{
			return conn->connection_id() == this->connection_id();
		});
		conn_list.erase(need_delete);

		Poco::Observer<Connection, ReadableNotification> readable_observer(*this, &Connection::on_readable);
		m_reactor.removeEventHandler(m_socket, readable_observer);
		Poco::Observer<Connection, ShutdownNotification> shutdown_observer(*this, &Connection::on_shutdown);
		m_reactor.removeEventHandler(m_socket, shutdown_observer);
		Poco::Observer<Connection, TimeoutNotification> timeout_observer(*this, &Connection::on_timeout);
		m_reactor.removeEventHandler(m_socket, timeout_observer);
		Poco::Observer<Connection, ErrorNotification> error_observer(*this, &Connection::on_error);
		m_reactor.removeEventHandler(m_socket, error_observer);

		m_socket.shutdown();
		m_socket.close();
	}
	catch (const std::exception& ex)
	{
		SPACELESS_ERROR(MODULE_NETWORK, "Remote address {}: {}", m_socket.peerAddress(), ex.what());
	}
	catch (...)
	{
		SPACELESS_ERROR(MODULE_NETWORK, "Remote address {}: Unkown error", m_socket.peerAddress());
	}
}


int Connection::connection_id() const
{
	return m_id;
}


void Connection::on_readable(ReadableNotification* notification)
{
	notification->release();
	read_for_state();
}


void Connection::on_shutdown(ShutdownNotification* /*notification*/)
{
	close();
}


void Connection::on_timeout(TimeoutNotification* /*notification*/)
{
	SPACELESS_ERROR(MODULE_NETWORK, "Remote address {}: On time out.", m_socket.peerAddress());
}


void Connection::on_error(ErrorNotification* /*notification*/)
{
	SPACELESS_ERROR(MODULE_NETWORK, "Remote address {}: On error.", m_socket.peerAddress());
}


void Connection::send_package_buffer(const PackageBuffer& package)
{
	int len = static_cast<int>(PackageBuffer::MAX_HEADER_LEN + package.header().content_length);
	m_socket.sendBytes(package.data(), len);
}


void Connection::close()
{
	delete this;
}


void Connection::set_attachment(void* attachment)
{
	m_attachment = attachment;
}


void* Connection::get_attachment()
{
	return m_attachment;
}


StreamSocket& Connection::stream_socket()
{
	return m_socket;
}


void Connection::read_for_state(int deep)
{
	if (deep > 5) // Reduce call recursive too deeply.
	{
		return;
	}

	switch (m_read_state)
	{
		case ReadState::READ_HEADER:
		{
			int len = m_socket.receiveBytes(m_buffer.data() + m_readed_len,
											static_cast<int>(PackageBuffer::MAX_HEADER_LEN) - m_readed_len);
			if (len == 0 && deep == 0)
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
			int len = m_socket.receiveBytes(m_buffer.data() + PackageBuffer::MAX_HEADER_LEN + m_readed_len,
											m_buffer.header().content_length - m_readed_len);
			if (len == 0 && deep == 0)
			{
				close();
				return;
			}

			m_readed_len += len;
			if (m_readed_len == m_buffer.header().content_length)
			{
				m_readed_len = 0;
				m_read_state = ReadState::READ_HEADER;

				auto handler = CommandHandlerManager::instance()->find_handler(m_buffer.header().command);
				if (handler)
				{
					try
					{
						handler(*this, m_buffer);
					}
					catch (const Exception& ex)
					{
						SPACELESS_ERROR(MODULE_NETWORK, "Remote address {}: {}.", m_socket.peerAddress(), ex);
					}
					catch (const std::exception& ex)
					{
						SPACELESS_ERROR(MODULE_NETWORK, "Remote address {}: {}.", m_socket.peerAddress(), ex.what());
					}
					catch (...)
					{
						SPACELESS_ERROR(MODULE_NETWORK, "Remote address {}: Unkown error.", m_socket.peerAddress());
					}
				}
				else
				{
					SPACELESS_ERROR(MODULE_NETWORK, "Remote address {}: Unkown command {}.",
									m_socket.peerAddress(), m_buffer.header().command);
				}
			}
			break;
		}
	}

	read_for_state(++deep);
}


ConnectionManager::~ConnectionManager()
{
	try
	{
		stop_all();
	}
	catch (const std::exception& ex)
	{
		SPACELESS_ERROR(MODULE_NETWORK, ex.what());
	}
	catch (...)
	{
		SPACELESS_ERROR(MODULE_NETWORK, "Unkown error");
	}
}


Connection& ConnectionManager::register_connection(const std::string& host, unsigned short port)
{
	StreamSocket stream_socket(SocketAddress(host, port));
	Connection* conn = new Connection(stream_socket, m_reactor);
	return *conn;
}


void ConnectionManager::register_listener(const std::string& host, unsigned short port)
{
	ServerSocket serve_socket(SocketAddress(host, port));
	m_acceptor_list.emplace_back(serve_socket, m_reactor);
	SPACELESS_DEBUG(MODULE_NETWORK, "Creates a listener: {}", serve_socket.address());
}


void ConnectionManager::stop_all()
{
	for (Connection* conn: m_conn_list)
	{
		conn->close();
	}
	m_conn_list.clear();
	m_acceptor_list.clear();
}


void ConnectionManager::run()
{
	m_reactor.run();
}

} // namespace spaceless
