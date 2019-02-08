/**
 * network_impl.cpp
 * @author wherewindblow
 * @date   Feb 02, 2019
 */


#include "network_impl.h"

#include <Poco/NObserver.h>
#include <Poco/Net/NetException.h>

#include "../log.h"
#include "../network.h"
#include "../network_message.h"


namespace spaceless {
namespace details {

using Poco::Net::SocketAddress;

static Logger& logger = get_logger("network");


void dump_sequence(lights::Sequence sequence)
{
	lights::TextWriter writer;
	for (std::size_t i = 0; i < sequence.length(); ++i)
	{
		int n = static_cast<int>(sequence.at<unsigned char>(i));
		writer << lights::pad(n, '0', 3) << ' ';
		if (i != 0 && i % 20 == 0)
		{
			puts(writer.c_str());
			writer.clear();
		}
	}
	puts(writer.c_str());
}


void pad_message(NetworkMessage& msg, int conn_id, int package_id)
{
	msg.conn_id = conn_id;
	msg.package_id = package_id;
	NetworkService* service = NetworkServiceManager::instance()->find_service_by_connection(conn_id);
	if (service != nullptr)
	{
		msg.service_id = service->service_id;
	}
}


// Check network connection is valid to avoid notifying a not exist network connection.
template <class Notification>
class ConnectionObserver: public Poco::NObserver<NetworkConnectionImpl, Notification>
{
public:
	typedef Poco::NObserver<NetworkConnectionImpl, Notification> BaseObserver;
	typedef Poco::AutoPtr<Notification> NotificationPtr;

	typedef void (NetworkConnectionImpl::*Callback)(const NotificationPtr&);

	ConnectionObserver(NetworkConnectionImpl& object, Callback method, int conn_id) :
		BaseObserver(object, method),
		m_conn_id(conn_id)
	{
	}

	ConnectionObserver(const ConnectionObserver& observer) :
		BaseObserver(observer),
		m_conn_id(observer.m_conn_id)
	{
	}

	ConnectionObserver& operator=(const ConnectionObserver& observer)
	{
		if (&observer != this)
		{
			static_cast<BaseObserver&>(*this) = observer;
			m_conn_id = observer.m_conn_id;
		}
		return *this;
	}

	void notify(Poco::Notification* pNf) const
	{
		// Closing connection also need to process writeable event.
		NetworkConnectionImpl* conn = NetworkManagerImpl::instance()->find_connection(m_conn_id);
		if (conn != nullptr)
		{
			BaseObserver::notify(pNf);
		}
	}

private:
	int m_conn_id;
};


NetworkConnectionImpl::NetworkConnectionImpl(StreamSocket& socket,
											 SocketReactor& reactor,
											 ConnectionOpenType open_type) :
	m_socket(socket),
	m_reactor(reactor),
	m_open_type(open_type),
	m_receive_len(0),
	m_read_state(ReadState::READ_HEADER),
	m_send_len(0),
	m_is_opening(true),
	m_is_closing(false),
	security_setting(SecuritySetting::OPEN_SECURITY),
	m_secure_conn(nullptr),
	m_pending_list(nullptr)
{
	// Set send and receive operation is non-blocking.
	m_socket.setBlocking(false);

	// Add event handler.
	ConnectionObserver<ReadableNotification> readable(*this, &NetworkConnectionImpl::on_readable, m_id);
	m_reactor.addEventHandler(m_socket, readable);
	ConnectionObserver<ErrorNotification> error(*this, &NetworkConnectionImpl::on_error, m_id);
	m_reactor.addEventHandler(m_socket, error);

	m_id = NetworkManagerImpl::instance()->on_create_connection(this);

	try
	{
		std::string address = m_socket.address().toString();
		std::string peer_address = m_socket.peerAddress().toString();
		LIGHTS_INFO(logger, "Creates connection {}: local={}, peer={}.", m_id, address, peer_address);

		// Send security setting.
		if (open_type == ConnectionOpenType::PASSIVE_OPEN)
		{
			SecuritySetting security_setting = NetworkManagerImpl::instance()->get_security_setting(address);

			int content_len = sizeof(security_setting);
			Package package = PackageManager::instance()->register_package(content_len);
			PackageHeader::Base& header_base = package.header().base;
			header_base.command = static_cast<int>(BuildinCommand::NTF_SECURITY_SETTING);
			header_base.content_length = content_len;
			lights::copy_array(static_cast<char*>(package.content_buffer().data()),
							   reinterpret_cast<const char*>(&security_setting),
							   sizeof(security_setting));
			send_raw_package(package);

			// Start secure connection.
			if (security_setting == SecuritySetting::OPEN_SECURITY)
			{
				m_secure_conn = new SecureConnection(this);
			}

			m_is_opening = false;
		}
	}
	catch (Poco::Exception& ex) // TODO: How to close connection with exception correctly?
	{
		LIGHTS_INFO(logger, "Creates connection {}: local=unknown, peer=unknown.", m_id);
	}
}


NetworkConnectionImpl::~NetworkConnectionImpl()
{
	try
	{
		LIGHTS_INFO(logger, "Destroys connection {}.", m_id);
		NetworkManagerImpl::instance()->on_destroy_connection(m_id);

		// Remove event handler.
		ConnectionObserver<ReadableNotification> readable(*this, &NetworkConnectionImpl::on_readable, m_id);
		m_reactor.removeEventHandler(m_socket, readable);
		ConnectionObserver<WritableNotification> writable(*this, &NetworkConnectionImpl::on_writable, m_id);
		m_reactor.removeEventHandler(m_socket, writable);
		ConnectionObserver<ErrorNotification> error(*this, &NetworkConnectionImpl::on_error, m_id);
		m_reactor.removeEventHandler(m_socket, error);

		// Remove all package that own by this connection.
		while (!m_send_list.empty())
		{
			int package_id = m_send_list.front();
			m_send_list.pop();
			PackageManager::instance()->remove_package(package_id);
		}

		// Close socket.
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
		LIGHTS_ERROR(logger, "Connection {}: Destroy error. msg={}.", m_id, ex.what());
	}
	catch (...)
	{
		LIGHTS_ERROR(logger, "Connection {}: Destroy unknown error.", m_id);
	}


	// Remove pending list.
	if (m_pending_list != nullptr)
	{
		delete m_pending_list;
		m_pending_list = nullptr;
	}

	// Close secure connection.
	if (m_secure_conn != nullptr)
	{
		delete m_secure_conn;
		m_secure_conn = nullptr;
	}
}


void NetworkConnectionImpl::send_raw_package(Package package)
{
	LIGHTS_DEBUG(logger, "Connection {}: Send package. cmd={}, trigger_package_id={}.",
				 m_id, package.header().base.command, package.header().extend.trigger_package_id);

	// Push to send list and delay to send.
	if (!m_send_list.empty())
	{
		m_send_list.push(package.package_id());
		return;
	}

	// Send package.
	int len = static_cast<int>(package.valid_length());
	m_send_len = m_socket.sendBytes(package.data(), len);
	if (m_send_len == len)
	{
		m_send_len = 0;
		PackageManager::instance()->remove_package(package.package_id());
	}
	else if (m_send_len < len)
	{
		m_send_list.push(package.package_id());
		ConnectionObserver<WritableNotification> observer(*this, &NetworkConnectionImpl::on_writable, m_id);
		m_reactor.addEventHandler(m_socket, observer);
	}
}


void NetworkConnectionImpl::send_package(Package package)
{
	if (m_is_closing)
	{
		LIGHTS_ERROR(logger, "Connection {}: Send package while closing. cmd={}, trigger_package_id={}.",
					 m_id, package.header().base.command, package.header().extend.trigger_package_id);
		PackageManager::instance()->remove_package(package.package_id());
		return;
	}

	if (m_secure_conn != nullptr)
	{
		m_secure_conn->send_package(package);
	}
	else
	{
		if (m_is_opening)
		{
			if (m_pending_list == nullptr)
			{
				m_pending_list = new std::queue<int>();
			}
			m_pending_list->push(package.package_id());
		}
		else
		{
			send_raw_package(package);
		}
	}
}


void NetworkConnectionImpl::close()
{
	if (m_send_list.empty())
	{
		delete this;
		return;
	}

	// Delay to delete this after send all message. Ensure all message in m_send_list to be send.
	m_is_closing = true;
}


void NetworkConnectionImpl::on_readable(const Poco::AutoPtr<ReadableNotification>& notification)
{
	if (m_is_closing)
	{
		return;
	}

	read_for_state();
}


void NetworkConnectionImpl::on_writable(const Poco::AutoPtr<WritableNotification>& notification)
{
	// Find package to send.
	Package package;
	while (!m_send_list.empty())
	{
		package = PackageManager::instance()->find_package(m_send_list.front());
		if (!package.is_valid())
		{
			m_send_list.pop();
		}
		else
		{
			break;
		}
	}

	if (!package.is_valid())
	{
		return;
	}

	// Send package.
	int len = static_cast<int>(package.valid_length());
	m_send_len += m_socket.sendBytes(package.data() + m_send_len, len - m_send_len);

	if (m_send_len == len)
	{
		m_send_len = 0;
		m_send_list.pop();
		PackageManager::instance()->remove_package(package.package_id());
	}

	if (m_send_list.empty())
	{
		ConnectionObserver<WritableNotification> observer(*this, &NetworkConnectionImpl::on_writable, m_id);
		m_reactor.removeEventHandler(m_socket, observer);

		if (m_is_closing)
		{
			close_without_waiting();
			return;
		}
	}
}


void NetworkConnectionImpl::on_error(const Poco::AutoPtr<ErrorNotification>& notification)
{
	// Closes by peer without general notification.
	LIGHTS_ERROR(logger, "Connection {}: On error.", m_id);
	close_without_waiting();
}


void NetworkConnectionImpl::read_for_state()
{
	while (true)
	{
		switch (m_read_state)
		{
			case ReadState::READ_HEADER:
			{
				int len = m_socket.receiveBytes(reinterpret_cast<char*>(&m_receive_buffer.header()) + m_receive_len,
												static_cast<int>(PackageBuffer::HEADER_LEN) - m_receive_len);

				if (len == -1) // Not available bytes in buffer.
				{
					return;
				}

				if (len == 0) // Closes by peer.
				{
					close_without_waiting();
					return;
				}

				m_receive_len += len;

				// Check package version.
				if (m_receive_len >= static_cast<int>(sizeof(PackageHeader::Base)))
				{
					PackageHeader::Base& read_header_base = reinterpret_cast<PackageHeader::Base&>(m_receive_buffer.header());
					if (read_header_base.version != PACKAGE_VERSION)
					{
						if (read_header_base.command != static_cast<int>(BuildinCommand::NTF_INVALID_VERSION))
						{
							// Notify peer and logic layer package version is invalid.
							Package package = PackageManager::instance()->register_package(0);
							PackageHeader::Base& header_base = package.header().base;
							header_base.command = static_cast<int>(BuildinCommand::NTF_INVALID_VERSION);
							header_base.content_length = 0;
							send_raw_package(package);

							LIGHTS_INFO(logger, "Connection {}: Package version invalid. cmd={}.",
										m_id, read_header_base.command);
							close();
							return;
						}
						else
						{
							// Notify logic layer package version is invalid.
							LIGHTS_INFO(logger, "Connection {}: Package version invalid.", m_id);
							close();
							return;
						}
					}
				}

				if (m_receive_len == PackageBuffer::HEADER_LEN)
				{
					m_receive_len = 0;
					m_read_state = ReadState::READ_CONTENT;
				}
				break;
			}

			case ReadState::READ_CONTENT:
			{
				int raw_len = m_receive_buffer.header().base.content_length;
				int read_content_len = raw_len;
				if (m_secure_conn != nullptr)
				{
					read_content_len = m_secure_conn->get_content_length(raw_len);
				}

				if (read_content_len != 0)
				{
					auto expect_len = static_cast<std::size_t>(read_content_len);
					lights::Sequence content = m_receive_buffer.content_buffer(expect_len);
					if (content.length() < expect_len)
					{
						PackageHeader& header = m_receive_buffer.header();
						LIGHTS_INFO(logger, "Connection {}: Have not enough space to receive package content. "
							"cmd={}, content_length={}, self_package_id={}.",
									m_id,
									header.base.command,
									header.base.content_length,
									header.extend.self_package_id);
						close();
						return;
					}

					int len = m_socket.receiveBytes(static_cast<char*>(content.data()) + m_receive_len,
													read_content_len - m_receive_len);
					if (len == -1) // Not available bytes in buffer.
					{
						return;
					}

					if (len == 0) // Closes by peer.
					{
						close_without_waiting();
						return;
					}

					m_receive_len += len;
				}

				if (m_receive_len == read_content_len)
				{
					m_receive_len = 0;
					m_read_state = ReadState::READ_HEADER;

					if (!on_read_complete_package(m_receive_buffer))
					{
						return;
					}
				}
				break;
			}
		}
	}
}


bool NetworkConnectionImpl::on_read_complete_package(const PackageBuffer& package_buffer)
{
	const PackageHeader& header = package_buffer.header();
	int cmd = header.base.command;
	LIGHTS_DEBUG(logger, "Connection {}: Receive package. cmd={}, trigger_package_id={}.",
				 m_id, cmd, header.extend.trigger_package_id);

	// Receive security setting.
	if (cmd == static_cast<int>(BuildinCommand::NTF_SECURITY_SETTING))
	{
		if (m_open_type == ConnectionOpenType::PASSIVE_OPEN)
		{
			LIGHTS_ERROR(logger, "Connection {}: Only passive open connection can notify security setting.", m_id);
			close();
			return false;
		}

		if (!m_is_opening)
		{
			LIGHTS_ERROR(logger, "Connection {}: Already open connection cannot change security setting.", m_id);
			close();
			return false;
		}

		lights::SequenceView content = package_buffer.content();
		if (content.length() < sizeof(SecuritySetting))
		{
			LIGHTS_ERROR(logger, "Connection {}: Security setting content not enough. content_length={}.", m_id, content.length());
			close();
			return false;
		}

		m_is_opening = false; // NOTE: send_all_pending_package is dependent on this.
		auto setting = static_cast<const SecuritySetting*>(content.data());
		if (*setting == SecuritySetting::OPEN_SECURITY)
		{
			LIGHTS_ASSERT(m_secure_conn == nullptr && "Cannot create secure connection multiple times");
			m_secure_conn = new SecureConnection(this); // NOTE: because send_all_pending_package is dependent on this.
		}

		send_all_pending_package();
		return true;
	}

	if (m_is_opening)
	{
		LIGHTS_INFO(logger, "Connection {}: Ignore package when connection is opening. cmd={}.", m_id);
		return false;
	}

	// Receive general package.
	if (m_secure_conn != nullptr)
	{
		m_secure_conn->on_read_complete_package(package_buffer);
	}
	else
	{
		int content_len = package_buffer.header().base.content_length;
		Package package = PackageManager::instance()->register_package(content_len);
		lights::copy_array(package.data(), package_buffer.data(), package_buffer.valid_length());

		NetworkMessage msg;
		pad_message(msg, m_id, package.package_id());
		NetworkMessageQueue::instance()->push(NetworkMessageQueue::IN_QUEUE, msg);
	}
	return true;
}


void NetworkConnectionImpl::send_all_pending_package()
{
	if (m_pending_list == nullptr)
	{
		return;
	}

	while (!m_pending_list->empty())
	{
		int package_id = m_pending_list->front();
		m_pending_list->pop();
		Package package = PackageManager::instance()->find_package(package_id);
		if (package.is_valid())
		{
			send_package(package);
		}
	}

	delete m_pending_list;
	m_pending_list = nullptr;
}


SecureConnection::SecureConnection(NetworkConnectionImpl* conn) :
	m_conn(conn),
	m_state(State::STARTING),
	m_private_key(nullptr),
	m_aes_key("", crypto::AesKeyBits::BITS_256),
	m_pending_list(nullptr)
{
	if (m_conn->open_type() == ConnectionOpenType::PASSIVE_OPEN)
	{
		// Generate RSA key pair.
		crypto::RsaKeyPair key_pair = crypto::generate_rsa_key_pair();
		m_private_key = new crypto::RsaPrivateKey(key_pair.private_key);
		std::string public_key = key_pair.public_key.save_to_string();
		int content_len = static_cast<int>(public_key.length());

		// Send public key to peer.
		Package package = PackageManager::instance()->register_package(content_len);
		PackageHeader::Base& header_base = package.header().base;
		header_base.command = static_cast<int>(BuildinCommand::REQ_START_CRYPTO);
		header_base.content_length = content_len;
		lights::copy_array(static_cast<char*>(package.content_buffer().data()),
						   public_key.c_str(),
						   public_key.length());

		m_conn->send_raw_package(package);
	}
}


SecureConnection::~SecureConnection()
{
	if (m_private_key != nullptr)
	{
		delete m_private_key;
		m_private_key = nullptr;
	}

	if (m_pending_list != nullptr)
	{
		delete m_pending_list;
		m_pending_list = nullptr;
	}
}


void SecureConnection::send_package(Package package)
{
	// Delay to send.
	if (m_state != State::STARTED)
	{
		if (m_pending_list == nullptr)
		{
			m_pending_list = new std::queue<int>();
		}
		m_pending_list->push(package.package_id());
		return;
	}

	// Encrypt package.
	crypto::AesBlockEncryptor encryptor;
	encryptor.set_key(m_aes_key);

	int content_len = package.header().base.content_length;
	std::size_t cipher_len = static_cast<std::size_t>(get_content_length(content_len));

	// In-place encryption that must ensure content have enough memory to do this.
	lights::Sequence content = package.content();
	for (std::size_t i = 0; i + crypto::AES_BLOCK_SIZE <= cipher_len; i += crypto::AES_BLOCK_SIZE)
	{
		encryptor.encrypt(&content.at<char>(i));
	}

	package.set_calculate_length(crypto::aes_cipher_length);

	m_conn->send_raw_package(package);
}


void SecureConnection::on_read_complete_package(const PackageBuffer& package_buffer)
{
	const PackageHeader& header = package_buffer.header();
	switch (m_state)
	{
		case State::STARTING:
		{
			ConnectionOpenType open_type = m_conn->open_type();
			auto cmd = static_cast<BuildinCommand>(header.base.command);
			if (open_type== ConnectionOpenType::PASSIVE_OPEN && cmd == BuildinCommand::RSP_START_CRYPTO)
			{
				LIGHTS_ASSERT(m_private_key != nullptr && "Have not store private key");

				// Get AES key.
				lights::SequenceView content = package_buffer.content();
				std::string cipher(static_cast<const char*>(content.data()), content.length());
				std::string plain = rsa_decrypt(cipher, *m_private_key);
				m_aes_key.reset(plain, crypto::AesKeyBits::BITS_256);

				// Remove key, because it's not necessary.
				delete m_private_key;
				m_private_key = nullptr;

				m_state = State::STARTED; // NOTE: send_all_pending_package is dependent on this.

				// Re-send all pending package again.
				send_all_pending_package();
			}
			else if (open_type == ConnectionOpenType::ACTIVE_OPEN && cmd == BuildinCommand::REQ_START_CRYPTO)
			{
				// Get public key.
				crypto::RsaPublicKey public_key;
				lights::SequenceView content = package_buffer.content();
				std::string public_key_str(static_cast<const char*>(content.data()), content.length());
				public_key.load_from_string(public_key_str);

				// Generate random AES key.
				m_aes_key.reset(crypto::AesKeyBits::BITS_256);

				// Encrypt AES key.
				std::string cipher = rsa_encrypt(m_aes_key.get_value(), public_key);
				int content_len = static_cast<int>(cipher.length());

				// Send cipher AES key to peer.
				Package package = PackageManager::instance()->register_package(content_len);
				PackageHeader::Base& header_base = package.header().base;
				header_base.command = static_cast<int>(BuildinCommand::RSP_START_CRYPTO);
				header_base.content_length = content_len;
				lights::copy_array(static_cast<char*>(package.content_buffer().data()),
								   cipher.c_str(), cipher.length());
				m_conn->send_raw_package(package);

				m_state = State::STARTED; // NOTE: send_all_pending_package is dependent on this.

				// Re-send all pending package again.
				send_all_pending_package();
			}
			else
			{
				// Ignore package when not started secure connection.
				LIGHTS_INFO(logger, "Connection {}: Ignore package. cmd={}, trigger_package_id={}.",
							 m_conn->connection_id(), header.base.command, header.extend.trigger_package_id);
			}
			break;
		}

		case State::STARTED:
		{
			// Decrypt package.
			lights::SequenceView content = package_buffer.content();
			int content_len = get_content_length(static_cast<int>(content.length()));
			Package package = PackageManager::instance()->register_package(content_len);
			package.header() = header;
			lights::SequenceView cipher(content.data(), static_cast<std::size_t>(content_len));
			crypto::aes_decrypt(cipher, package.content_buffer(), m_aes_key);

			// Push to in queue.
			NetworkMessage msg;
			pad_message(msg, m_conn->connection_id(), package.package_id());
			NetworkMessageQueue::instance()->push(NetworkMessageQueue::IN_QUEUE, msg);
			break;
		}
	}
}


int SecureConnection::get_content_length(int raw_length)
{
	auto plain_len = static_cast<std::size_t>(raw_length);
	return static_cast<int>(crypto::aes_cipher_length(plain_len));
}


void SecureConnection::send_all_pending_package()
{
	if (m_pending_list == nullptr)
	{
		return;
	}

	while (!m_pending_list->empty())
	{
		int package_id = m_pending_list->front();
		m_pending_list->pop();
		Package package = PackageManager::instance()->find_package(package_id);
		if (package.is_valid())
		{
			send_package(package);
		}
	}

	delete m_pending_list;
	m_pending_list = nullptr;
}


void NetworkReactor::onIdle()
{
	process_out_message();
	// SocketReactor::onIdle(); // Avoid sending event to all network connection, because it's not efficient.
}


void NetworkReactor::onBusy()
{
	process_out_message();
	// SocketReactor::onBusy(); // There is nothing in this function.
}


void NetworkReactor::onTimeout()
{
	process_out_message();
	// SocketReactor::onTimeout(); // Avoid sending event to all network connection, because it's not efficient.
}


void NetworkReactor::process_out_message()
{
	for (std::size_t i = 0; i < MAX_OUT_MSG_PROCESS_PER_TIMES; ++i)
	{
		if (NetworkMessageQueue::instance()->empty(NetworkMessageQueue::OUT_QUEUE))
		{
			break;
		}

		auto msg = NetworkMessageQueue::instance()->pop(NetworkMessageQueue::OUT_QUEUE);
		if (msg.conn_id != 0 || msg.service_id != 0)
		{
			send_package(msg);
		}

		if (msg.delegate != nullptr)
		{
			safe_call_delegate(msg.delegate, msg.caller);
		}
	}
}


void NetworkReactor::send_package(const NetworkMessage& msg)
{
	int conn_id = msg.conn_id;
	if (conn_id == 0)
	{
		conn_id = NetworkServiceManager::instance()->get_connection_id(msg.service_id);
	}

	NetworkConnectionImpl* conn = NetworkManagerImpl::instance()->find_open_connection(conn_id);
	Package package = PackageManager::instance()->find_package(msg.package_id);

	if (conn == nullptr || !package.is_valid())
	{
		if (conn == nullptr)
		{
			LIGHTS_INFO(logger, "Connection {}: Already close.", conn_id);
		}

		if (!package.is_valid())
		{
			LIGHTS_ERROR(logger, "Connection {}: Package already remove. package_id={}.", conn_id, msg.package_id);
		}

		if (package.is_valid())
		{
			PackageManager::instance()->remove_package(msg.package_id);
		}

		return;
	}

	conn->send_package(package);
}


bool NetworkReactor::safe_call_delegate(std::function<void()> function, lights::StringView caller)
{
	try
	{
		function();
		return true;
	}
	catch (Exception& ex)
	{
		LIGHTS_ERROR(logger, "Delegation {}: Exception. code={}, msg={}.", caller, ex.code(), ex);
	}
	catch (Poco::Exception& ex)
	{
		LIGHTS_ERROR(logger, "Delegation {}: Poco::Exception. name={}, msg={}.", caller, ex.name(), ex.message());
	}
	catch (std::exception& ex)
	{
		LIGHTS_ERROR(logger, "Delegation {}: std::exception. name={}, msg={}.", caller, typeid(ex).name(), ex.what());
	}
	catch (...)
	{
		LIGHTS_ERROR(logger, "Delegation {}: unknown exception.", caller);
	}
	return false;
}


NetworkManagerImpl::~NetworkManagerImpl()
{
	try
	{
		stop_all();
	}
	catch (std::exception& ex)
	{
		LIGHTS_ERROR(logger, "Network connection manager destroy error. msg={}.", ex.what());
	}
	catch (...)
	{
		LIGHTS_ERROR(logger, "Network connection manager destroy unknown error.");
	}
}


NetworkConnectionImpl& NetworkManagerImpl::register_connection(const std::string& host, unsigned short port)
{
	StreamSocket stream_socket(SocketAddress(host, port));
	auto conn_impl = new NetworkConnectionImpl(stream_socket, m_reactor, ConnectionOpenType::ACTIVE_OPEN);
	return *conn_impl;
}


void NetworkManagerImpl::register_listener(const std::string& host,
										   unsigned short port,
										   SecuritySetting security_setting)
{
	SocketAddress address(host, port);
	ServerSocket server_socket(address);
	m_acceptor_list.emplace_back(server_socket, m_reactor);

	if (security_setting == SecuritySetting::OPEN_SECURITY)
	{
		m_secure_listener_list.insert(address.toString());
	}

	LIGHTS_INFO(logger, "Creates network listener. address={}.", server_socket.address().toString());
}


void NetworkManagerImpl::remove_connection(int conn_id)
{
	NetworkConnectionImpl* conn = find_connection(conn_id);
	if (conn != nullptr)
	{
		conn->close();
	}
}


NetworkConnectionImpl* NetworkManagerImpl::find_connection(int conn_id)
{
	auto itr = m_conn_list.find(conn_id);
	if (itr == m_conn_list.end())
	{
		return nullptr;
	}

	return itr->second;
}


NetworkConnectionImpl* NetworkManagerImpl::find_open_connection(int conn_id)
{
	NetworkConnectionImpl* conn = find_connection(conn_id);
	if (conn != nullptr && conn->is_open())
	{
		return conn;
	}

	return nullptr;
}


void NetworkManagerImpl::stop_all()
{
	// Cannot use iterator to for each to close, because close will erase itself on m_conn_list.
	while (!m_conn_list.empty())
	{
		NetworkConnectionImpl* conn = m_conn_list.begin()->second;
		conn->close();
	}
	m_conn_list.clear();
	m_acceptor_list.clear();
}


void NetworkManagerImpl::start()
{
	LIGHTS_INFO(logger, "Starting network scheduler.");
	m_reactor.setTimeout(Poco::Timespan(0, REACTOR_TIME_OUT_US));
	m_reactor.run();
	LIGHTS_INFO(logger, "Stopped network scheduler.");
}


void NetworkManagerImpl::stop()
{
	LIGHTS_INFO(logger, "Stopping network scheduler.");
	m_reactor.stop();
}


int NetworkManagerImpl::on_create_connection(NetworkConnectionImpl* conn)
{
	int conn_id = m_next_id;
	++m_next_id;
	m_conn_list.insert(std::make_pair(conn_id, conn));
	return conn_id;
}


void NetworkManagerImpl::on_destroy_connection(int conn_id)
{
	m_conn_list.erase(conn_id);
}


SecuritySetting NetworkManagerImpl::get_security_setting(const std::string& address)
{
	auto itr = m_secure_listener_list.find(address);
	if (itr == m_secure_listener_list.end())
	{
		return SecuritySetting::CLOSE_SECURITY;
	}

	return SecuritySetting::OPEN_SECURITY;
}

} // namespace details
} // namespace spaceless
