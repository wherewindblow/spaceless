/*
 * network.cpp
 * @author wherewindblow
 * @date   Nov 01, 2017
 */

#include "network.h"
#include "log.h"


namespace spaceless {

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


Connection::Connection(TcpSocket tcp_socket):
	m_socket(std::move(tcp_socket))
{
}

Connection::~Connection()
{
	stop();
}

void Connection::start()
{
	read_header();
}


void Connection::stop()
{
	m_socket.close();
}


void Connection::read_header()
{
	auto buffer = asio::buffer(m_read_buffer.data(), PackageBuffer::MAX_HEADER_LEN);
	asio::async_read(m_socket, buffer,
					 [this](const boost::system::error_code& error, std::size_t bytes_transferred)
	{
		if (error)
		{
			SPACELESS_ERROR(MODULE_NETWORK, "Read package header have error: {}", error.message());
			return;
		}

		if (bytes_transferred < PackageBuffer::MAX_HEADER_LEN)
		{
			SPACELESS_ERROR(MODULE_NETWORK, "Read package header have error: Not enought bytes for header");
			return;
		}

		if (m_read_buffer.header().content_length > static_cast<int>(PackageBuffer::MAX_CONTENT_LEN))
		{
			SPACELESS_ERROR(MODULE_NETWORK, "Read package header have error: Content length is too large");
			return;
		}

		read_content();
	});
}


void Connection::read_content()
{
	auto buffer = asio::buffer(m_read_buffer.data() + PackageBuffer::MAX_HEADER_LEN,
				 static_cast<std::size_t>(m_read_buffer.header().content_length));

	asio::async_read(m_socket, buffer,
					 [this](const boost::system::error_code& error, std::size_t bytes_transferred )
	{
		if (error)
		{
			SPACELESS_ERROR(MODULE_NETWORK, "Read package content have error: {}", error.message());
			return;
		}

		auto callback = CommandHandlerManager::instance()->find_handler(m_read_buffer.header().command);
		if (callback)
		{
			callback(m_read_buffer);
		}

		read_header();
	});
}


void Connection::write(const PackageBuffer& package)
{
	// TODO Consider to use coroutines to serialize asynchronization. (asio::spawn ?)
	m_write_buffer = package; // To ensure buffer is avaliable when really write.
	asio::async_write(m_socket, m_write_buffer.asio_buffer(),
					  [](const boost::system::error_code& error, std::size_t bytes_transferred) {});
}


ConnectionManager::~ConnectionManager()
{
	stop_all();
}


Connection& ConnectionManager::create(TcpSocket tcp_socket)
{
	auto conn = new Connection(std::move(tcp_socket));
	m_conn_list.push_back(conn);
	return *conn;
}


void ConnectionManager::stop_all()
{
	for (std::size_t i = 0; i < m_conn_list.size(); ++i)
	{
		m_conn_list[i]->stop();
		delete m_conn_list[i];
		m_conn_list[i] = nullptr;
	}
}


void ConnectionManager::run()
{
	m_io_service.run();
}


asio::io_service& ConnectionManager::io_service()
{
	return m_io_service;
}


} // namespace spaceless
