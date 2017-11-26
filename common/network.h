/**
 * network.h
 * @author wherewindblow
 * @date   Nov 01, 2017
 */

#pragma once

#include <map>
#include <memory>
#include <list>
#include <boost/asio.hpp>
#include <lights/sequence.h>

#include "basics.h"
#include "log.h"
#include "exception.h"


namespace lights {

template <typename Sink>
FormatSinkAdapter<Sink> operator<< (FormatSinkAdapter<Sink> out, const boost::asio::ip::tcp::endpoint& endpoint)
{
	out << endpoint.address().to_string() << ':' << endpoint.port();
	return out;
}

} // namespace lights


/**
 * @note All function that use in framework cannot throw exception. Because don't know who can catch it.
 */
namespace spaceless {

const int MODULE_NETWORK = 1;

enum
{
	ERR_NETWORK_PACKAGE_CANNOT_REGISTER = 100,
	ERR_NETWORK_PACKAGE_CANNOT_PARSE_AS_PROTOBUF = 101,
};


namespace asio = boost::asio;
using tcp = asio::ip::tcp;

extern asio::io_service service;


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

	PackageBuffer(int id):
		m_id(id)
	{
		header().version = PACKAGE_VERSION;
	}

	int package_id() const
	{
		return m_id;
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
	 * Return buffer according to header content length.
	 * @note Must ensure header is valid.
	 */
	lights::Sequence content()
	{
		return { m_buffer + MAX_HEADER_LEN, static_cast<std::size_t>(header().content_length) };
	}

	/**
	 * Return buffer according to header content length.
	 * @note Must ensure header is valid.
	 */
	lights::SequenceView content() const
	{
		return { m_buffer + MAX_HEADER_LEN, static_cast<std::size_t>(header().content_length) };
	}

	/**
	 * Return all content buffer.
	 */
	lights::Sequence content_buffer()
	{
		return { m_buffer + MAX_HEADER_LEN, MAX_CONTENT_LEN };
	}

	/**
	 * Return all content buffer.
	 */
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

	template <typename T>
	void parse_as_protobuf(T& value) const;

private:
	int m_id;
	char m_buffer[MAX_BUFFER_LEN];
};


template <typename T>
void PackageBuffer::parse_as_protobuf(T& value) const
{
	lights::SequenceView storage = content();
	bool ok = value.ParseFromArray(storage.data(), static_cast<int>(storage.length()));
	if (!ok)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_NETWORK_PACKAGE_CANNOT_PARSE_AS_PROTOBUF);
	}
}


/**
 * Manage package buffer and guarantee they are valid when connetion underlying write is call.
 */
class PackageBufferManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(PackageBufferManager);

	PackageBuffer& register_package_buffer();

	void remove_package_buffer(int buffer_id);

private:
	int m_next_id = 1;
	std::map<int, PackageBuffer> m_buffer_list;
};


class Connection;

/**
 * Manage all command and handler. When recieve a command will trigger associated handler.
 * @note A command only can associate one handler.
 */
class CommandHandlerManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(CommandHandlerManager);

	using CommandHandler = std::function<void(Connection&, const PackageBuffer&)>;

	void register_command(int cmd, CommandHandler callback);

	void remove_command(int cmd);

	CommandHandler find_handler(int cmd);

private:
	std::map<int, CommandHandler> m_handler_list;
};


class ConnectionManager;

class Connection
{
public:
	Connection(int id);

	~Connection();

	int connection_id() const;

	void connect(lights::StringView address, unsigned short port);

	void start_reading();

	/**
	 * Stop reading and close connection.
	 */
	void close();

	/**
	 * @note package must be create from PackageBufferManager::instance()->register_package_buffer().
	 */
	void write_package_buffer(const PackageBuffer& package);

	template <typename T>
	void write_protobuf(int cmd, const T& msg);

	void set_attachment(void* attachment);

	void* attachment();

	tcp::endpoint remote_endpoint();

private:
	friend ConnectionManager;

	void read_header();

	void read_content();

	int m_id;
	tcp::socket m_socket;
	tcp::endpoint m_remote_endpoint;
	PackageBuffer m_read_buffer;
	void* m_attachment;
};


class ConnectionManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(ConnectionManager);

	~ConnectionManager();

	Connection& register_connection(lights::StringView address, unsigned short port);

	void register_listener(lights::StringView address, unsigned short port);

	void stop_all();

private:
	void accept_connection(tcp::acceptor& acceptor);

	friend class Connection;

	int m_next_id;
	std::list<Connection> m_conn_list;
	std::list<tcp::acceptor> m_acceptor_list;
};


template <typename T>
void Connection::write_protobuf(int cmd, const T& msg)
{
	int size = msg.ByteSize();
	if (static_cast<std::size_t>(size) > PackageBuffer::MAX_CONTENT_LEN)
	{
		SPACELESS_ERROR(MODULE_NETWORK, "Remote endpoint {}. Content length is too large. Length:{}",
						remote_endpoint(), size)
		return;
	}

	PackageBuffer& package = PackageBufferManager::instance()->register_package_buffer();
	package.header().command = cmd;
	package.header().content_length = size;
	lights::Sequence storage = package.content_buffer();
	msg.SerializeToArray(storage.data(), static_cast<int>(storage.length()));

	write_package_buffer(package);
}


} // namespace spaceless
