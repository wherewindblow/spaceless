/*
 * network.cpp
 * @author wherewindblow
 * @date   Nov 01, 2017
 */

#include "network.h"
#include "log.h"


namespace spaceless {

asio::io_service service;


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


Connection::Connection():
	m_socket(service)
{
}


Connection::~Connection()
{
	stop();
}


void Connection::connect(lights::StringView address, unsigned short port)
{
	tcp::endpoint endpoint(asio::ip::address::from_string(address.data()), port);
	m_socket.connect(endpoint);
}


void Connection::start()
{
	read_header();
}


void Connection::stop()
{
	if (m_socket.is_open())
	{
		m_socket.shutdown(tcp::socket::shutdown_both);
		m_socket.close();
	}
}


void Connection::read_header()
{
	auto buffer = asio::buffer(m_read_buffer.data(), PackageBuffer::MAX_HEADER_LEN);
	asio::async_read(m_socket, buffer,
					 [this](const boost::system::error_code& error, std::size_t bytes_transferred)
	{
		if (error.value() == asio::error::eof && bytes_transferred == 0)
		{
			return;
		}

		if (error && error.value() != asio::error::eof)
		{
			SPACELESS_ERROR(MODULE_NETWORK, error.message());
			return;
		}

		if (bytes_transferred < PackageBuffer::MAX_HEADER_LEN)
		{
			SPACELESS_ERROR(MODULE_NETWORK, "Not enought bytes for header");
			return;
		}

		if (m_read_buffer.header().content_length > static_cast<int>(PackageBuffer::MAX_CONTENT_LEN))
		{
			SPACELESS_ERROR(MODULE_NETWORK, "Content length is too large");
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
			SPACELESS_ERROR(MODULE_NETWORK, error.message());
			return;
		}

		auto handler = CommandHandlerManager::instance()->find_handler(m_read_buffer.header().command);
		if (handler)
		{
			try
			{
				handler(m_read_buffer);
			}
			catch (const lights::Exception& ex)
			{
				SPACELESS_ERROR(MODULE_NETWORK, ex);
			}
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


Connection& ConnectionManager::create_connection(lights::StringView address, unsigned short port)
{
	Connection& conn = m_conn_list.emplace_back();
	conn.connect(address, port);
	return conn;
}


void ConnectionManager::listen(lights::StringView address, unsigned short port)
{
	tcp::endpoint endpoint(asio::ip::address::from_string(address.data()), port);
	tcp::acceptor& acceptor = m_acceptor_list.emplace_back(service, endpoint);

	Connection& conn = m_conn_list.emplace_back();
	acceptor.async_accept(conn.m_socket, [&conn](const boost::system::error_code& error)
	{
		conn.start();
	});
}


void ConnectionManager::stop_all()
{
	for (auto& conn: m_conn_list)
	{
		conn.stop();
	}
	m_conn_list.clear();

	for (auto& acceptor: m_acceptor_list)
	{
		if (acceptor.is_open())
		{
			acceptor.close();
		}
	}
	m_acceptor_list.clear();
}


} // namespace spaceless
