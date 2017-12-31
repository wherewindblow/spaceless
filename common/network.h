/**
 * network.h
 * @author wherewindblow
 * @date   Nov 01, 2017
 */

#pragma once

#include <map>
#include <memory>
#include <list>
#include <queue>
#include <lights/sequence.h>

#include <Poco/Net/SocketAcceptor.h>
#include <Poco/Net/SocketNotification.h>
#include <Poco/Net/SocketReactor.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/StreamSocket.h>

#include "basics.h"
#include "log.h"
#include "exception.h"


namespace lights {

template <typename Sink>
FormatSinkAdapter<Sink> operator<< (FormatSinkAdapter<Sink> out, const Poco::Net::SocketAddress& address)
{
	out << address.toString();
	return out;
}

} // namespace lights


/**
 * @note All function that use in framework cannot throw exception. Because don't know who can catch it.
 */
namespace spaceless {

using Poco::Net::SocketAcceptor;
using Poco::Net::SocketNotification;
using Poco::Net::SocketReactor;
using Poco::Net::ServerSocket;
using Poco::Net::StreamSocket;
using Poco::Net::ReadableNotification;
using Poco::Net::WritableNotification;
using Poco::Net::ShutdownNotification;
using Poco::Net::TimeoutNotification;
using Poco::Net::ErrorNotification;


const int MODULE_NETWORK = 1;

enum
{
	ERR_NETWORK_PACKAGE_CANNOT_REGISTER = 100,
	ERR_NETWORK_PACKAGE_CANNOT_PARSE_AS_PROTOBUF = 101,
	ERR_NETWORK_PACKAGE_NOT_EXIST = 102,
};


const int PROTOCOL_PACKAGE_VERSION = 1;

struct ProtocolHeader
{
	short version;
	int command;
	int content_length;
} SPACELESS_POD_PACKED_ATTRIBUTE;


/**
 * Network data package to seperate message.
 */
class PackageBuffer
{
public:
	static const std::size_t MAX_BUFFER_LEN = 65536;
	static const std::size_t MAX_HEADER_LEN = sizeof(ProtocolHeader);
	static const std::size_t MAX_CONTENT_LEN = MAX_BUFFER_LEN - MAX_HEADER_LEN;

	/**
	 * Creates the PackageBuffer.
	 */
	PackageBuffer(int package_id):
		m_id(package_id)
	{
		header().version = PROTOCOL_PACKAGE_VERSION;
	}

	int package_id() const
	{
		return m_id;
	}

	/**
	 * Returns package header of buffer.
	 */
	ProtocolHeader& header()
	{
		return *reinterpret_cast<ProtocolHeader*>(m_buffer);
	}

	/**
	 * Returns package header of buffer.
	 */
	const ProtocolHeader& header() const
	{
		return *reinterpret_cast<const ProtocolHeader*>(m_buffer);
	}

	/**
	 * Returns buffer according to header content length.
	 * @note Must ensure header is valid.
	 */
	lights::Sequence content()
	{
		return { m_buffer + MAX_HEADER_LEN, static_cast<std::size_t>(header().content_length) };
	}

	/**
	 * Returns buffer according to header content length.
	 * @note Must ensure header is valid.
	 */
	lights::SequenceView content() const
	{
		return { m_buffer + MAX_HEADER_LEN, static_cast<std::size_t>(header().content_length) };
	}

	/**
	 * Returns all content buffer.
	 */
	lights::Sequence content_buffer()
	{
		return { m_buffer + MAX_HEADER_LEN, MAX_CONTENT_LEN };
	}

	/**
	 * Returns all content buffer.
	 */
	lights::SequenceView content_buffer() const
	{
		return { m_buffer + MAX_HEADER_LEN, MAX_CONTENT_LEN };
	}

	/**
	 * Returns underlying storage buffer data.
	 */
	char* data()
	{
		return m_buffer;
	}

	/**
	 * Returns underlying storage buffer data.
	 */
	const char* data() const
	{
		return m_buffer;
	}

	/**
	 * Returns header and content length.
	 */
	std::size_t total_length() const
	{
		return PackageBuffer::MAX_HEADER_LEN + header().content_length;
	}

	/**
	 * Parses package content as a protobuf.
	 */
	template <typename T>
	void parse_as_protobuf(T& msg) const;

private:
	int m_id;
	char m_buffer[MAX_BUFFER_LEN];
};


template <typename T>
void PackageBuffer::parse_as_protobuf(T& msg) const
{
	lights::SequenceView storage = content();
	bool ok = msg.ParseFromArray(storage.data(), static_cast<int>(storage.length()));
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

	PackageBuffer& register_package();

	void remove_package(int package_id);

	PackageBuffer* find_package(int package_id);

	PackageBuffer& get_package(int package_id);

private:
	int m_next_id = 1;
	std::map<int, PackageBuffer> m_package_list;
};



class NetworkConnection;

/**
 * CommandHandlerManager manages all command and handler. When recieve a command will trigger associated handler.
 * @note A command only can associate one handler.
 */
class CommandHandlerManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(CommandHandlerManager);

	using CommandHandler = std::function<void(NetworkConnection&, const PackageBuffer&)>;

	/**
	 * Registers associate a command with handler.
	 */
	void register_command(int cmd, CommandHandler callback);

	/**
	 * Removes associate a command with handler.
	 */
	void remove_command(int cmd);

	/**
	 * Finds the associate handler of command.
	 * @note If cannot found command will return nullptr.
	 */
	CommandHandler find_handler(int cmd);

private:
	std::map<int, CommandHandler> m_handler_list;
};


/**
 * NetworkConnection handler socket notification and cache receive message.
 */
class NetworkConnection
{
public:
	enum class ReadState
	{
		READ_HEADER,
		READ_CONTENT,
	};

	/**
	 * Creates the NetworkConnection and add event handler.
	 * @note Do not create in stack.
	 */
	NetworkConnection(StreamSocket& socket, SocketReactor& reactor);

	/**
	 * Destroys the NetworkConnection and remove event handler。
	 */
	~NetworkConnection();

	/**
	 * Returns connection id.
	 */
	int connection_id() const;

	/**
	 * Handlers readable notification.
	 */
	void on_readable(ReadableNotification* notification);

	/**
	 * Handlers writable notification (send buffer is not full).
	 */
	void on_writable(WritableNotification* notification);

	/**
	 * Handlers shutdown notification.
	 */
	void on_shutdown(ShutdownNotification* notification);

	/**
	 * Handlers timeout notification.
	 */
	void on_timeout(TimeoutNotification* notification);

	/**
	 * Handlers error notification.
	 */
	void on_error(ErrorNotification* notification);

	/**
	 * Sends a package to remote.
	 */
	void send_package(const PackageBuffer& package);

	/**
	 * Parses a protobuf as package buffer and send to remote.
	 */
	template <typename ProtobufType>
	void send_protobuf(int cmd, const ProtobufType& msg);

	/**
	 * Destroys connection.
	 * @note Cannot use connection after close it.
	 */
	void close();

	/**
	 * Sets associate attachment data with this connection.
	 */
	void set_attachment(void* attachment);

	/**
	 * Gets attachment and the real type of attachement is according to real type that call @c set_attachment.
	 */
	void* get_attachment();

	/**
	 * Returns underlying stream socket.
	 */
	StreamSocket& stream_socket();

private:
	void read_for_state(int deep = 0);

	int m_id;
	StreamSocket m_socket;
	SocketReactor& m_reactor;
	PackageBuffer m_read_buffer;
	int m_readed_len;
	ReadState m_read_state;
	std::queue<int> m_send_list;
	int m_sended_len;
	void* m_attachment;
};


template <typename ProtobufType>
void NetworkConnection::send_protobuf(int cmd, const ProtobufType& msg)
{
	int size = msg.ByteSize();
	if (static_cast<std::size_t>(size) > PackageBuffer::MAX_CONTENT_LEN)
	{
		SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: Content length {} is too large.", m_id, size)
		return;
	}

	PackageBuffer& package = PackageBufferManager::instance()->register_package();
	package.header().command = cmd;
	package.header().content_length = size;
	lights::Sequence storage = package.content_buffer();
	msg.SerializeToArray(storage.data(), static_cast<int>(storage.length()));

	send_package(package);
}


/**
 * NetworkConnectionManager manages all network connection and listener.
 */
class NetworkConnectionManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(NetworkConnectionManager);

	/**
	 * Destroys the connection manager.
	 */
	~NetworkConnectionManager();

	/**
	 * Registers a connection with host and port.
	 */
	NetworkConnection& register_connection(const std::string& address, unsigned short port);

	/**
	 * Registers a listener with host and port.
	 */
	void register_listener(const std::string& address, unsigned short port);

	/**
	 * Stop all connection and listener.
	 */
	void stop_all();

	/**
	 * To run underlying reactor.
	 */
	void run();

private:
	friend class NetworkConnection;

	int m_next_id = 1;
	std::list<NetworkConnection*> m_conn_list;
	std::list<SocketAcceptor<NetworkConnection>> m_acceptor_list;
	SocketReactor m_reactor;
};


} // namespace spaceless
