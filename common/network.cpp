/*
 * network.cpp
 * @author wherewindblow
 * @date   Nov 01, 2017
 */

#include "network.h"


namespace spaceless {

asio::io_service service;


PackageBuffer& PackageBufferManager::register_package_buffer()
{
	auto pair = m_buffer_list.emplace(m_next_id, PackageBuffer(m_next_id));
	++m_next_id;
	if (pair.second == false)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_NETWORK_PACKAGE_CANNOT_REGISTER);
	}
	return pair.first->second;
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
	m_id(id),
	m_socket(service),
	m_read_buffer(0),
	m_attachment(nullptr)
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
	m_remote_endpoint.address(asio::ip::address::from_string(address.data()));
	m_remote_endpoint.port(port);
	m_socket.connect(m_remote_endpoint);
}


void Connection::start_reading()
{
	read_header();
}


void Connection::close()
{
	if (m_socket.is_open())
	{
		try
		{
			m_socket.shutdown(tcp::socket::shutdown_both);
			m_socket.close();
		}
		catch (const boost::system::system_error& ex)
		{
			SPACELESS_ERROR(MODULE_NETWORK, ex.what());
		}
	}
}


void Connection::write_package_buffer(const PackageBuffer& package)
{
	// TODO Consider to use coroutines to serialize asynchronization. (asio::spawn ?)
	if (static_cast<std::size_t>(package.header().content_length) > PackageBuffer::MAX_CONTENT_LEN)
	{
		PackageBufferManager::instance()->remove_package_buffer(package.package_id());
		SPACELESS_ERROR(MODULE_NETWORK, "Remote endpoint {}. Content length is too large. Length:{}",
						remote_endpoint(), package.header().content_length)
		return;
	}

	auto buffer = asio::buffer(package.data(),
							   PackageBuffer::MAX_HEADER_LEN + package.header().content_length);
	asio::async_write(m_socket, buffer,
					  [&package](const boost::system::error_code& error, std::size_t bytes_transferred)
	{
		PackageBufferManager::instance()->remove_package_buffer(package.package_id());
	});
}


void Connection::set_attachment(void* attachment)
{
	m_attachment = attachment;
}


void* Connection::attachment()
{
	return m_attachment;
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

		if (error &&
			error.value() != asio::error::eof &&
			error.value() != asio::error::connection_reset)
		{
			SPACELESS_ERROR(MODULE_NETWORK, "Remote endpoint {}. {}", remote_endpoint(), error.message());
			return;
		}

		if (bytes_transferred < PackageBuffer::MAX_HEADER_LEN)
		{
			SPACELESS_ERROR(MODULE_NETWORK, "Remote endpoint {}. Not enought bytes for header. Bytes:{}",
							remote_endpoint(), bytes_transferred);
			return;
		}

		if (static_cast<std::size_t>(m_read_buffer.header().content_length) > PackageBuffer::MAX_CONTENT_LEN)
		{
			SPACELESS_ERROR(MODULE_NETWORK, "Remote endpoint {}. Content length is too large. Length:{}",
							remote_endpoint(), m_read_buffer.header().content_length);
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
			SPACELESS_ERROR(MODULE_NETWORK, "Remote endpoint {}. {}", remote_endpoint(), error.message());
			return;
		}

		auto handler = CommandHandlerManager::instance()->find_handler(m_read_buffer.header().command);
		if (handler)
		{
			try
			{
				handler(*this, m_read_buffer);
			}
			catch (const lights::Exception& ex)
			{
				SPACELESS_ERROR(MODULE_NETWORK, "Remote endpoint {}. {}", remote_endpoint(), ex);
			}
		}
		else
		{
			SPACELESS_ERROR(MODULE_NETWORK, "Remote endpoint {}. Unkown command: {}",
							remote_endpoint(), m_read_buffer.header().command);
		}

		read_header();
	});
}


tcp::endpoint Connection::remote_endpoint()
{
	boost::system::error_code error_code;
	tcp::endpoint remote_endpoint = m_socket.remote_endpoint(error_code);
	if (!error_code)
	{
		m_remote_endpoint = remote_endpoint;
	}
	return m_remote_endpoint;
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
	accept_connection(acceptor);
}


void ConnectionManager::accept_connection(tcp::acceptor& acceptor)
{
	Connection& conn = m_conn_list.emplace_back(m_next_id);
	++m_next_id;
	acceptor.async_accept(conn.m_socket, [&acceptor, &conn](const boost::system::error_code& error)
	{
		conn.start_reading();
		conn.remote_endpoint();
		ConnectionManager::instance()->accept_connection(acceptor);
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
			boost::system::error_code error;
			acceptor.close(error);
			if (error)
			{
				SPACELESS_ERROR(MODULE_NETWORK, "Close accpetor: {}", error.message());
			}
		}
	}
	m_acceptor_list.clear();
}


} // namespace spaceless
