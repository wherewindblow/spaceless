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

/**
 * Network package header include some common data of each package.
 */
struct PackageHeader
{
	// Version of protocol.
	short version;
	// Indicates how of use content.
	int command;
	// Recieves side will transfer as trigger_trans_id when send back message.
	int self_trans_id;
	// self_trans_id of request.
	int trigger_trans_id;
	// The length of content.
	int content_length;
} SPACELESS_POD_PACKED_ATTRIBUTE;


/**
 * Network package buffer include header and content.
 */
class PackageBuffer
{
public:
	static const std::size_t MAX_BUFFER_LEN = 65536;
	static const std::size_t MAX_HEADER_LEN = sizeof(PackageHeader);
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
	PackageHeader& header()
	{
		return *reinterpret_cast<PackageHeader*>(m_buffer);
	}

	/**
	 * Returns package header of buffer.
	 */
	const PackageHeader& header() const
	{
		return *reinterpret_cast<const PackageHeader*>(m_buffer);
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
 * Manager of package buffer and guarantee package are valid when connetion underlying write is call.
 */
class PackageBufferManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(PackageBufferManager);

	/**
	 * Registers package buffer.
	 */
	PackageBuffer& register_package();

	/**
	 * Removes package buffer.
	 */
	void remove_package(int package_id);

	/**
	 * Finds package buffer.
	 * @note Returns nullptr if cannot find package.
	 */
	PackageBuffer* find_package(int package_id);

	/**
	 * Get package buffer.
	 * @throw Throws exception if cannot find package.
	 */
	PackageBuffer& get_package(int package_id);

private:
	int m_next_id = 1;
	std::map<int, PackageBuffer> m_package_list;
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
	 * Sends package to remote on asynchronization.
	 */
	void send_package(const PackageBuffer& package);

	/**
	 * Parses a protobuf as package buffer and send to remote on asynchronization.
	 * @param cmd            Identifies package content type.
	 * @param msg            Message of protobuff type.
	 * @param bind_trans_id  Specific transcation that trigger by rsponse.
	 * @param is_send_back   Is need to return last request associate transcation.
	 */
	template <typename ProtobufType>
	void send_protobuf(int cmd, const ProtobufType& msg, int bind_trans_id = 0, bool is_send_back = false);

	/**
	 * Parses a protobuf as package buffer and send to remote on asynchronization and send back request transcation id.
	 * @param cmd            Identifies package content type.
	 * @param msg            Message of protobuff type.
	 * @param bind_trans_id  Specific transcation that trigger by rsponse.
	 */
	template <typename ProtobufType>
	void send_back_protobuf(int cmd, const ProtobufType& msg, int bind_trans_id = 0);

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

	/**
	 * Returns underlying stream socket.
	 */
	const StreamSocket& stream_socket() const;

private:
	void read_for_state(int deep = 0);

	void trigger_transcation();

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
void NetworkConnection::send_protobuf(int cmd, const ProtobufType& msg, int bind_trans_id, bool is_send_back)
{
	int size = msg.ByteSize();
	if (static_cast<std::size_t>(size) > PackageBuffer::MAX_CONTENT_LEN)
	{
		SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: Content length {} is too large.", m_id, size)
		return;
	}

	PackageBuffer& package = PackageBufferManager::instance()->register_package();
	PackageHeader& header = package.header();
	header.command = cmd;
	header.self_trans_id  = bind_trans_id;
	header.trigger_trans_id = is_send_back ? m_read_buffer.header().self_trans_id : 0;
	header.content_length = size;
	lights::Sequence storage = package.content_buffer();
	msg.SerializeToArray(storage.data(), static_cast<int>(storage.length()));

	send_package(package);
}


template <typename ProtobufType>
void NetworkConnection::send_back_protobuf(int cmd, const ProtobufType& msg, int bind_trans_id)
{
	send_protobuf(cmd, msg, bind_trans_id, true);
}


/**
 * Manager of all network connection and listener.
 */
class NetworkConnectionManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(NetworkConnectionManager);

	/**
	 * Destroys the network connection manager.
	 */
	~NetworkConnectionManager();

	/**
	 * Registers network connection.
	 */
	NetworkConnection& register_connection(const std::string& host, unsigned short port);

	/**
	 * Registers network listener.
	 */
	void register_listener(const std::string& host, unsigned short port);

	/**
	 * Removes a network connection.
	 */
	void remove_connection(int conn_id);

	/**
	 * Finds a network connection.
	 * @note Returns nullptr if cannot find connection.
	 */
	NetworkConnection* find_connection(int conn_id);

	/**
	 * Stop all network connection and listener.
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


/**
 * Transcation type.
 */
enum class TranscationType
{
	ONE_PHASE_TRANSCATION,
	MULTIPLY_PHASE_TRANSCATION,
};


/**
 * General transcation type.
 */
struct Transcation
{
	TranscationType trans_type;
	void *trans_handler;
};


/**
 * Multiply phase transcation allow to save transcation session in time range.
 */
class MultiplyPhaseTranscation
{
public:
	enum PhaseResult
	{
		EXIT_TRANCATION,
		WAIT_NEXT_PHASE,
	};

	/**
	 * Factory of this type transcation. Just simply call contructor.
	 * @note This function is only a example.
	 */
	static MultiplyPhaseTranscation* register_transcation(int trans_id);

	/**
	 * Creates a transcation by trans_id.
	 */
	MultiplyPhaseTranscation(int trans_id);

	/**
	 * Destroys a transcation.
	 */
	virtual ~MultiplyPhaseTranscation() = default;

	/**
	 * Initializes this base class interal variables.
	 */
	void pre_on_init(NetworkConnection& conn, const PackageBuffer& package);

	/**
	 * Processes the package that trigger by associate command.
	 * @param conn     Network connection of send package.
	 * @param package  Pakcage of trigger this function.
	 */
	virtual PhaseResult on_init(NetworkConnection& conn, const PackageBuffer& package) = 0;

	/**
	 * Processes the package of wait phase.
	 * @param conn     Network connection of send package.
	 * @param package  Pakcage of trigger this function.
	 */
	virtual PhaseResult on_active(NetworkConnection& conn, const PackageBuffer& package) = 0;

	/**
	 * Processes wait time out.
	 */
	virtual PhaseResult on_timeout();

	/**
	 * Sets wait package info.
	 * @param conn          Network connection that send indicate package.
	 * @param cmd           Waits command.
	 * @param current_phase Current phase.
	 * @param timeout       Time out of waiting next package.
	 */
	PhaseResult wait_next_phase(NetworkConnection& conn, int cmd, int current_phase, int timeout);

	template <typename T>
	void send_back_message(int cmd, T& msg);

	/**
	 * Returns tranascation id.
	 */
	int transcation_id() const;

	/**
	 * Returns current phase.
	 */
	int current_phase() const;

	/**
	 * Returns connection that first start this transcation.
	 */
	NetworkConnection* first_connection();

	/**
	 * Returns waiting connection that set by @c wait_next_phase
	 */
	NetworkConnection* waiting_connection();

	/**
	 * Returns waiting command that set by @c wait_next_phase.
	 */
	int waiting_command() const;

private:
	int m_id;
	int m_current_phase = 0;
	NetworkConnection* m_first_conn = nullptr;
	NetworkConnection* m_wait_conn = nullptr;
	int m_wait_cmd = 0;
};


template <typename T>
void MultiplyPhaseTranscation::send_back_message(int cmd, T& msg)
{
	first_connection()->send_back_protobuf(cmd, msg);
}


/**
 * Transcation factory function of multiply phase transcation.
 */
using TranscationFatory = MultiplyPhaseTranscation* (*)(int);


/**
 * Manager of multiply phase transcation.
 */
class MultiplyPhaseTranscationManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(MultiplyPhaseTranscationManager);

	/**
	 * Registers a multiply phase transcation.
	 */
	MultiplyPhaseTranscation& register_transcation(TranscationFatory trans_fatory);

	/**
	 * Removes a multiply phase transcation.
	 */
	void remove_transcation(int trans_id);

	/**
	 * Finds a multiply phase transcation.
	 * @note Returns nullptr if cannot find multiply phase transcation.
	 */
	MultiplyPhaseTranscation* find_transcation(int trans_id);

private:
	int m_next_id = 1;
	std::map<int, MultiplyPhaseTranscation*> m_trans_list;
};


using OnePhaseTrancation = void(*)(NetworkConnection&, const PackageBuffer&);

/**
 * Manager of transcation. When recieve a command will trigger associated transcation.
 */
class TranscationManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(TranscationManager);

	/**
	 * Registers association of command with transcation.
     * @note A command only can associate one transcation.
	 */
	void register_transcation(int cmd, TranscationType trans_type, void *handler);

	/**
	 * Registers association of command with one phase transcation.
	 * @note A command only can associate one transcation.
	 */
	void register_one_phase_transcation(int cmd, OnePhaseTrancation trancation);

	/**
	 * Registers association of command with multiply phase transcation.
 	 * @param trans_fatory  Fatory of multiply phase transcation.
     * @note A command only can associate one transcation.
	 */
	void register_multiply_phase_transcation(int cmd, TranscationFatory trans_fatory);

	/**
	 * Removes association of command with transcation.
	 */
	void remove_transcation(int cmd);

	/**
	 * Finds transcation that associate with command.
	 * @note Return nullptr if cannot find command.
	 */
	Transcation* find_transcation(int cmd);

private:
	std::map<int, Transcation> m_trans_list;
};


} // namespace spaceless
