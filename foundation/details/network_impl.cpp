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


namespace spaceless {
namespace details {

using Poco::Net::SocketAddress;

static Logger& logger = get_logger("network");


// Check network connection is valid to avoid notifying a not exist network connection.
template <class C, class N>
class SafeObserver: public Poco::NObserver<C, N>
{
public:
	typedef Poco::NObserver<C, N> BaseObserver;
	typedef Poco::AutoPtr<N> NotificationPtr;
	typedef void (C::*Callback)(const NotificationPtr&);

	SafeObserver(C& object, Callback method, int conn_id):
		BaseObserver(object, method),
		m_conn_id(conn_id)
	{
	}

	SafeObserver(const SafeObserver& observer):
		BaseObserver(observer),
		m_conn_id(observer.m_conn_id)
	{
	}

	SafeObserver& operator=(const SafeObserver& observer)
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


NetworkConnectionImpl::NetworkConnectionImpl(StreamSocket& socket, SocketReactor& reactor, OpenType open_type) :
	m_socket(socket),
	m_reactor(reactor),
	m_receive_len(0),
	m_read_state(ReadState::READ_HEADER),
	m_send_len(0),
	m_open_type(open_type),
	m_crypto_state(CryptoState::STARTING),
	m_key("", crypto::AesKeyBits::BITS_256),
	m_is_closing(false)
{
	m_socket.setBlocking(false);

	SafeObserver<NetworkConnectionImpl, ReadableNotification> readable(*this, &NetworkConnectionImpl::on_readable, m_id);
	m_reactor.addEventHandler(m_socket, readable);
	SafeObserver<NetworkConnectionImpl, ErrorNotification> error(*this, &NetworkConnectionImpl::on_error, m_id);
	m_reactor.addEventHandler(m_socket, error);

	m_id = NetworkManagerImpl::instance()->on_create_connection(this);

	try
	{
		std::string address = m_socket.address().toString();
		std::string peer_address = m_socket.peerAddress().toString();
		LIGHTS_INFO(logger, "Creates network connection {}: local {} and peer {}.", m_id, address, peer_address);
	}
	catch (Poco::Exception& ex)
	{
		LIGHTS_INFO(logger, "Creates network connection {}: local unknown and peer unknown.", m_id);
	}

	if (open_type == PASSIVE_OPEN)
	{
		// Start crypto connection.
		crypto::RsaKeyPair key_pair = crypto::generate_rsa_key_pair();
		m_private_key = key_pair.private_key;
		std::string public_key = key_pair.public_key.save_to_string();
		int content_len = static_cast<int>(public_key.length());

		Package package = PackageManager::instance()->register_package(content_len);
		PackageHeader::Base& header_base = package.header().base;
		header_base.command = static_cast<int>(BuildinCommand::REQ_START_CRYPTO);
		header_base.content_length = content_len;
		lights::copy_array(reinterpret_cast<char*>(package.content_buffer().data()), public_key.c_str(), public_key.length());
		send_raw_package(package);
	}
}


NetworkConnectionImpl::~NetworkConnectionImpl()
{
	try
	{
		LIGHTS_INFO(logger, "Destroys network connection {}.", m_id);
		NetworkManagerImpl::instance()->on_destroy_connection(m_id);

		SafeObserver<NetworkConnectionImpl, ReadableNotification> readable(*this, &NetworkConnectionImpl::on_readable, m_id);
		m_reactor.removeEventHandler(m_socket, readable);
		SafeObserver<NetworkConnectionImpl, WritableNotification> writable(*this, &NetworkConnectionImpl::on_writable, m_id);
		m_reactor.removeEventHandler(m_socket, writable);
		SafeObserver<NetworkConnectionImpl, ErrorNotification> error(*this, &NetworkConnectionImpl::on_error, m_id);
		m_reactor.removeEventHandler(m_socket, error);

		while (!m_send_list.empty())
		{
			int package_id = m_send_list.front();
			m_send_list.pop();
			PackageManager::instance()->remove_package(package_id);
		}

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
		LIGHTS_ERROR(logger, "Connection {}: Destroy error: {}.", m_id, ex.what());
	}
	catch (...)
	{
		LIGHTS_ERROR(logger, "Connection {}: Destroy unkown error.", m_id);
	}
}


int NetworkConnectionImpl::connection_id() const
{
	return m_id;
}


void NetworkConnectionImpl::send_package(Package package)
{
	if (m_is_closing)
	{
		LIGHTS_ERROR(logger, "Connection {}: Send package on closing connection: cmd {}, trigger_package_id {}.",
					 m_id, package.header().base.command, package.header().extend.trigger_package_id);
		return;
	}

	if (m_crypto_state != CryptoState::STARTED)
	{
		m_not_crypto_list.push(package.package_id());
		return;
	}

	// Encrypt package.
	crypto::AesBlockEncryptor encryptor;
	encryptor.set_key(m_key);

	std::size_t content_len = static_cast<std::size_t>(package.header().base.content_length);
	std::size_t cipher_len = crypto::aes_cipher_length(content_len);

	lights::Sequence content = package.content();
	for (std::size_t i = 0; i + crypto::AES_BLOCK_SIZE <= cipher_len; i += crypto::AES_BLOCK_SIZE)
	{
		encryptor.encrypt(&content.at<char>(i));
	}

	package.set_is_cipher(true);

	send_raw_package(package);
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


bool NetworkConnectionImpl::is_open() const
{
	return !m_is_closing;
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
		SafeObserver<NetworkConnectionImpl, WritableNotification> observer(*this, &NetworkConnectionImpl::on_writable, m_id);
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
				int len = m_socket.receiveBytes(m_receive_buffer.data() + m_receive_len,
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
					auto read_header_base = reinterpret_cast<PackageHeader::Base*>(m_receive_buffer.data());
					if (read_header_base->version != PACKAGE_VERSION)
					{
						if (read_header_base->command != static_cast<int>(BuildinCommand::NTF_INVALID_VERSION))
						{
							// Notify peer and logic layer package version is invalid.
							Package package = PackageManager::instance()->register_package(0);
							PackageHeader::Base& header_base = package.header().base;
							header_base.command = static_cast<int>(BuildinCommand::NTF_INVALID_VERSION);
							header_base.content_length = 0;
							send_raw_package(package);

							LIGHTS_INFO(logger, "Connection {}: Package version invalid.", m_id);
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
				int read_content_len = m_receive_buffer.header().base.content_length;
				if (m_crypto_state == CryptoState::STARTED)
				{
					auto plain_len = static_cast<std::size_t>(read_content_len);
					read_content_len = static_cast<int>(crypto::aes_cipher_length(plain_len));
				}

				if (read_content_len != 0)
				{
					int len = m_socket.receiveBytes(m_receive_buffer.data() + PackageBuffer::HEADER_LEN + m_receive_len,
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

					on_read_complete_package(read_content_len);
				}
				break;
			}
		}
	}
}


void NetworkConnectionImpl::on_read_complete_package(int read_content_len)
{
	PackageHeader& header = m_receive_buffer.header();
	LIGHTS_DEBUG(logger, "Connection {}: Receive package cmd {}, trigger_package_id {}.",
				 m_id, header.base.command, header.extend.trigger_package_id);

	switch (m_crypto_state)
	{
		case CryptoState::STARTING:
		{
			auto cmd = static_cast<BuildinCommand>(header.base.command);
			if (m_open_type == PASSIVE_OPEN && cmd == BuildinCommand::RSP_START_CRYPTO)
			{
				std::string cipher(m_receive_buffer.content_data(), static_cast<std::size_t>(header.base.content_length));
				std::string plain = rsa_decrypt(cipher, m_private_key);
				m_key.reset(plain, crypto::AesKeyBits::BITS_256);
				m_crypto_state = CryptoState::STARTED;

				send_not_crypto_package();
			}
			else if (m_open_type == ACTIVE_OPEN && cmd == BuildinCommand::REQ_START_CRYPTO)
			{
				crypto::RsaPublicKey public_key;
				std::string public_key_str(m_receive_buffer.content_data(),
										   static_cast<std::size_t>(header.base.content_length));
				public_key.load_from_string(public_key_str);

				m_key.reset(crypto::AesKeyBits::BITS_256);

				std::string cipher = rsa_encrypt(m_key.get_value(), public_key);
				int content_len = static_cast<int>(cipher.length());

				Package package = PackageManager::instance()->register_package(content_len);
				PackageHeader::Base& header_base = package.header().base;
				header_base.command = static_cast<int>(BuildinCommand::RSP_START_CRYPTO);
				header_base.content_length = content_len;
				lights::copy_array(reinterpret_cast<char*>(package.content_buffer().data()),
								   cipher.c_str(), cipher.length());
				send_raw_package(package);

				// Must set state after send_package, because it depend on this state.
				m_crypto_state = CryptoState::STARTED;

				send_not_crypto_package();
			}
			else
			{
				// Ignore package when not started crypto.
				LIGHTS_DEBUG(logger, "Connection {}: Ignore package cmd {}, trigger_package_id {}.",
							 m_id, header.base.command, header.extend.trigger_package_id);
			}
			break;
		}

		case CryptoState::STARTED:
		{
			Package package = PackageManager::instance()->register_package(read_content_len);
			package.header() = header;
			lights::SequenceView cipher(m_receive_buffer.content_data(), static_cast<std::size_t>(read_content_len));
			crypto::aes_decrypt(cipher, package.content_buffer(), m_key);

			NetworkMessage msg;
			msg.conn_id = m_id;
			msg.package_id = package.package_id();
			NetworkService* service = NetworkServiceManager::instance()->find_service_by_connection(m_id);
			if (service != nullptr)
			{
				msg.service_id = service->service_id;
			}
			NetworkMessageQueue::instance()->push(NetworkMessageQueue::IN_QUEUE, msg);
			break;
		}
	}
}


void NetworkConnectionImpl::close_without_waiting()
{
	delete this;
}


void NetworkConnectionImpl::send_raw_package(Package package)
{
	LIGHTS_DEBUG(logger, "Connection {}: Send package cmd {}, trigger_package_id {}.",
				 m_id, package.header().base.command, package.header().extend.trigger_package_id);

	if (!m_send_list.empty())
	{
		m_send_list.push(package.package_id());
		return;
	}

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
		SafeObserver<NetworkConnectionImpl, WritableNotification> observer(*this, &NetworkConnectionImpl::on_writable, m_id);
		m_reactor.addEventHandler(m_socket, observer);
	}
}


void NetworkConnectionImpl::send_not_crypto_package()
{
	while (!m_not_crypto_list.empty())
	{
		int package_id = m_not_crypto_list.front();
		m_not_crypto_list.pop();
		Package package = PackageManager::instance()->find_package(package_id);
		if (package.is_valid())
		{
			send_package(package);
		}
	}
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

		if (msg.delegate)
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
			LIGHTS_ERROR(logger, "Connection {}: Package {} already remove.", conn_id, msg.package_id);
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
		LIGHTS_ERROR(logger, "Delegation {}: Exception error {}/{}.", caller, ex.code(), ex);
	}
	catch (Poco::Exception& ex)
	{
		LIGHTS_ERROR(logger, "Delegation {}: Poco error {}:{}.", caller, ex.name(), ex.message());
	}
	catch (std::exception& ex)
	{
		LIGHTS_ERROR(logger, "Delegation {}: std error {}:{}.", caller, typeid(ex).name(), ex.what());
	}
	catch (...)
	{
		LIGHTS_ERROR(logger, "Delegation {}: unknown error.", caller);
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
		LIGHTS_ERROR(logger, "Network connection manager destroy error: {}.", ex.what());
	}
	catch (...)
	{
		LIGHTS_ERROR(logger, "Network connection manager destroy unknown error.");
	}
}


NetworkConnectionImpl& NetworkManagerImpl::register_connection(const std::string& host, unsigned short port)
{
	StreamSocket stream_socket(SocketAddress(host, port));
	auto conn_impl = new NetworkConnectionImpl(stream_socket, m_reactor, NetworkConnectionImpl::ACTIVE_OPEN);
	return *conn_impl;
}


void NetworkManagerImpl::register_listener(const std::string& host, unsigned short port)
{
	ServerSocket server_socket(SocketAddress(host, port));
	m_acceptor_list.emplace_back(server_socket, m_reactor);
	LIGHTS_INFO(logger, "Creates network listener {}.", server_socket.address().toString());
}


void NetworkManagerImpl::remove_connection(int conn_id)
{
	NetworkConnectionImpl* conn = find_connection(conn_id);
	if (conn)
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
		NetworkConnectionImpl* conn =  m_conn_list.begin()->second;
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

} // namespace details
} // namespace spaceless
