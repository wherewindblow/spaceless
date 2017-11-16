/**
 * network.h
 * @author wherewindblow
 * @date   Nov 01, 2017
 */

#pragma once

#include <map>
#include <memory>
#include <boost/asio.hpp>
#include <lights/sequence.h>

#include "basics.h"


namespace spaceless {

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using TcpSocket = tcp::socket;
using TcpEndpoint = tcp::endpoint;


class PackageBuffer;

class CommandHandlerManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(CommandHandlerManager);

	using CommandHandler = std::function<void(const PackageBuffer&)>;

	void register_command(int cmd, CommandHandler callback);

	void remove_command(int cmd);

	CommandHandler find_handler(int cmd);

private:
	std::map<int, CommandHandler> m_handler_list;
};

const int PACKAGE_VERSION = 1;

struct PackageHeader
{
	short version;
	int command;
	int content_length;
} SPACELESS_POD_PACKED_ATTRIBUTE;


class PackageBuffer
{
public:
	static const std::size_t MAX_BUFFER_LEN = 65536;
	static const std::size_t MAX_HEADER_LEN = sizeof(PackageHeader);
	static const std::size_t MAX_CONTENT_LEN = MAX_BUFFER_LEN - MAX_HEADER_LEN;

	PackageBuffer()
	{
		header().version = PACKAGE_VERSION;
	}

	PackageHeader& header()
	{
		return *reinterpret_cast<PackageHeader*>(m_buffer);
	}

	const PackageHeader& header() const
	{
		return *reinterpret_cast<const PackageHeader*>(m_buffer);
	}

	/**
	 * @note Must ensure header is valid.
	 */
	lights::Sequence content()
	{
		return { m_buffer + MAX_HEADER_LEN, static_cast<std::size_t>(header().content_length) };
	}

	/**
	 * @note Must ensure header is valid.
	 */
	lights::SequenceView content() const
	{
		return { m_buffer + MAX_HEADER_LEN, static_cast<std::size_t>(header().content_length) };
	}

	lights::Sequence content_buffer()
	{
		return { m_buffer + MAX_HEADER_LEN, MAX_CONTENT_LEN };
	}

	lights::SequenceView content_buffer() const
	{
		return { m_buffer + MAX_HEADER_LEN, MAX_CONTENT_LEN };
	}

	char* data()
	{
		return m_buffer;
	}

	const char* data() const
	{
		return m_buffer;
	}

	/**
	 * @note Must ensure header cnd content have set.
	 */
	asio::const_buffers_1 asio_buffer() const
	{
		return asio::buffer(static_cast<const char*>(m_buffer), MAX_HEADER_LEN + header().content_length);
	}

private:
	char m_buffer[MAX_BUFFER_LEN];
};


class ConnectionManager;

class Connection
{
public:
	Connection(TcpSocket tcp_socket);

	~Connection();

	void start();

	void stop();

	void write(const PackageBuffer& package);

private:
	void read_header();

	void read_content();

	TcpSocket m_socket;
	PackageBuffer m_read_buffer;
	PackageBuffer m_write_buffer;
};

using ConnectionPtr = std::shared_ptr<Connection>;


class ConnectionManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(ConnectionManager);

	~ConnectionManager();

	Connection& create(TcpSocket tcp_socket);

	void stop_all();

	void run();

	asio::io_service& io_service();

private:
	friend class Connection;

	std::vector<Connection*> m_conn_list;
	asio::io_service m_io_service;
};

} // namespace spaceless
