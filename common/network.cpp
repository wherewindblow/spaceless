/*
 * network.cpp
 * @author wherewindblow
 * @date   Nov 01, 2017
 */

#include "network.h"
#include "log.h"
#include "exception.h"


namespace spaceless {

asio::io_service service;


PackageBuffer& PackageBufferManager::register_package_buffer()
{
	auto pari = m_buffer_list.emplace(m_next_id, PackageBuffer(m_next_id));
	++m_next_id;
	if (pari.second == false)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_NETWORK_PACKAGE_CANNOT_REGISTER);
	}
	return pari.first->second;
}


void PackageBufferManager::remove_package_buffer(int buffer_id)
{
	m_buffer_list.erase(buffer_id);
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


Connection::Connection(int id):
	m_id(id), m_socket(service), m_read_buffer(0)
{
}


Connection::~Connection()
{
	close();
}


int Connection::connection_id() const
{
	return m_id;
}


void Connection::connect(lights::StringView address, unsigned short port)
{
	tcp::endpoint endpoint(asio::ip::address::from_string(address.data()), port);
	m_socket.connect(endpoint);
}


void Connection::start_reading()
{
	read_header();
}


void Connection::close()
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


void Connection::write_package_buffer(const PackageBuffer& package)
{
	// TODO Consider to use coroutines to serialize asynchronization. (asio::spawn ?)
	asio::async_write(m_socket, package.asio_buffer(),
					  [&package](const boost::system::error_code& error, std::size_t bytes_transferred)
	{
		PackageBufferManager::instance()->remove_package_buffer(package.package_id());
	});
}


ConnectionManager::~ConnectionManager()
{
	stop_all();
}


Connection& ConnectionManager::register_connection(lights::StringView address, unsigned short port)
{
	Connection& conn = m_conn_list.emplace_back(m_next_id);
	++m_next_id;
	conn.connect(address, port);
	return conn;
}


void ConnectionManager::register_listener(lights::StringView address, unsigned short port)
{
	tcp::endpoint endpoint(asio::ip::address::from_string(address.data()), port);
	tcp::acceptor& acceptor = m_acceptor_list.emplace_back(service, endpoint);

	Connection& conn = m_conn_list.emplace_back(m_next_id);
	++m_next_id;
	acceptor.async_accept(conn.m_socket, [&conn](const boost::system::error_code& error)
	{
		conn.start_reading();
	});
}


void ConnectionManager::stop_all()
{
	for (auto& conn: m_conn_list)
	{
		conn.close();
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
