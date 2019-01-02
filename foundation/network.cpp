/*
 * network.cpp
 * @author wherewindblow
 * @date   Nov 01, 2017
 */

#include "network.h"

#include <execinfo.h>
#include <Poco/Net/NetException.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/AbstractObserver.h>
#include <Poco/NObserver.h>
#include <crypto/rsa.h>
#include <protocol/all.h>

#include "log.h"
#include "worker.h"


namespace lights {

template <typename Backend>
FormatSink<Backend> operator<< (FormatSink<Backend> sink, const Poco::Net::SocketAddress& address)
{
	sink << address.toString();
	return sink;
}

} // namespace lights


namespace spaceless {

static Logger& logger = get_logger("network");

using Poco::Net::SocketAddress;

namespace details {

// Check network connection is valid to avoid notifying a not exist network connection.
template <class C, class N>
class Observer: public Poco::NObserver<C, N>
{
public:
	typedef Poco::NObserver<C, N> BaseObserver;
	typedef Poco::AutoPtr<N> NotificationPtr;
	typedef void (C::*Callback)(const NotificationPtr&);

	Observer(C& object, Callback method, int conn_id):
		BaseObserver(object, method),
		m_conn_id(conn_id)
	{
	}

	Observer(const Observer& observer):
		BaseObserver(observer),
		m_conn_id(observer.m_conn_id)
	{
	}

	Observer& operator=(const Observer& observer)
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
		// Closing connection need to proccess writeable event.
		NetworkConnection* conn = NetworkManager::instance()->find_connection(m_conn_id);
		if (conn != nullptr)
		{
			BaseObserver::notify(pNf);
		}
	}

private:
	int m_conn_id;
};

} // namespace details


NetworkConnection::NetworkConnection(StreamSocket& socket, SocketReactor& reactor, OpenType open_type) :
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

	details::Observer<NetworkConnection, ReadableNotification> readable(*this, &NetworkConnection::on_readable, m_id);
	m_reactor.addEventHandler(m_socket, readable);
	details::Observer<NetworkConnection, ErrorNotification> error(*this, &NetworkConnection::on_error, m_id);
	m_reactor.addEventHandler(m_socket, error);

	m_id = NetworkManager::instance()->on_create_connection(this);

	try
	{
		std::string address = m_socket.address().toString();
		std::string peer_address = m_socket.peerAddress().toString();
		LIGHTS_INFO(logger, "Creates network connection {}: local {} and peer {}.", m_id, address, peer_address);
	}
	catch (Poco::Exception& ex)
	{
		LIGHTS_INFO(logger, "Creates network connection {}: local unknow and peer unknow.", m_id);
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


NetworkConnection::~NetworkConnection()
{
	try
	{
		LIGHTS_INFO(logger, "Destroys network connection {}.", m_id);
		NetworkManager::instance()->on_destroy_connection(m_id);

		details::Observer<NetworkConnection, ReadableNotification> readable(*this, &NetworkConnection::on_readable, m_id);
		m_reactor.removeEventHandler(m_socket, readable);
		details::Observer<NetworkConnection, WritableNotification> writable(*this, &NetworkConnection::on_writable, m_id);
		m_reactor.removeEventHandler(m_socket, writable);
		details::Observer<NetworkConnection, ErrorNotification> error(*this, &NetworkConnection::on_error, m_id);
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
		LIGHTS_ERROR(logger, "Connction {}: Destroy error: {}.", m_id, ex.what());
	}
	catch (...)
	{
		LIGHTS_ERROR(logger, "Connction {}: Destroy unkown error.", m_id);
	}
}


int NetworkConnection::connection_id() const
{
	return m_id;
}


void NetworkConnection::send_package(Package package)
{
	if (m_is_closing)
	{
		LIGHTS_ERROR(logger, "Connction {}: Send package on closing connection: cmd {}, trans_id {}.",
					 m_id, package.header().base.command, package.header().extend.trigger_trans_id);
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


void NetworkConnection::close()
{
	if (m_send_list.empty())
	{
		delete this;
		return;
	}

	// Delay to delete this after send all message. Ensure all message in m_send_list to be sended.
	m_is_closing = true;
}


bool NetworkConnection::is_open() const
{
	return !m_is_closing;
}


void NetworkConnection::on_readable(const Poco::AutoPtr<ReadableNotification>& notification)
{
	if (m_is_closing)
	{
		return;
	}

	read_for_state();
}


void NetworkConnection::on_writable(const Poco::AutoPtr<WritableNotification>& notification)
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
		details::Observer<NetworkConnection, WritableNotification> observer(*this, &NetworkConnection::on_writable, m_id);
		m_reactor.removeEventHandler(m_socket, observer);

		if (m_is_closing)
		{
			close_without_waiting();
			return;
		}
	}
}


void NetworkConnection::on_error(const Poco::AutoPtr<ErrorNotification>& notification)
{
	// Closes by peer without general notification.
	LIGHTS_ERROR(logger, "Connction {}: On error.", m_id);
	close_without_waiting();
}


void NetworkConnection::read_for_state()
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

							LIGHTS_INFO(logger, "Connction {}: Package version invalid.", m_id);
							close();
							return;
						}
						else
						{
							// Notify logic layer package version is invalid.
							LIGHTS_INFO(logger, "Connction {}: Package version invalid.", m_id);
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


void NetworkConnection::on_read_complete_package(int read_content_len)
{
	PackageHeader& header = m_receive_buffer.header();
	LIGHTS_DEBUG(logger, "Connction {}: Recieve package cmd {}, trans_id {}.",
				 m_id, header.base.command, header.extend.trigger_trans_id);

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
				LIGHTS_DEBUG(logger, "Connction {}: Ignore package cmd {}, trans_id {}.",
							 m_id, header.base.command, header.extend.trigger_trans_id);
			}
			break;
		}

		case CryptoState::STARTED:
		{
			Package package = PackageManager::instance()->register_package(read_content_len);
			package.header() = header;
			lights::SequenceView cipher(m_receive_buffer.content_data(), static_cast<std::size_t>(read_content_len));
			crypto::aes_decrypt(cipher, package.content_buffer(), m_key);

			NetworkMessage msg(m_id, package.package_id());
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


void NetworkConnection::close_without_waiting()
{
	delete this;
}


void NetworkConnection::send_raw_package(Package package)
{
	LIGHTS_DEBUG(logger, "Connction {}: Send package cmd {}, trans_id {}.",
				 m_id, package.header().base.command, package.header().extend.trigger_trans_id);

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
		details::Observer<NetworkConnection, WritableNotification> observer(*this, &NetworkConnection::on_writable, m_id);
		m_reactor.addEventHandler(m_socket, observer);
	}
}


void NetworkConnection::send_not_crypto_package()
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


void NetworkMessageQueue::push(QueueType queue_type, const NetworkMessage& msg)
{
	std::lock_guard<std::mutex> lock(m_mutex[queue_type]);
	m_queue[queue_type].push(msg);
}


NetworkMessage NetworkMessageQueue::pop(QueueType queue_type)
{
	std::lock_guard<std::mutex> lock(m_mutex[queue_type]);
	NetworkMessage msg = m_queue[queue_type].front();
	m_queue[queue_type].pop();
	return msg;
}


bool NetworkMessageQueue::empty(NetworkMessageQueue::QueueType queue_type)
{
	std::lock_guard<std::mutex> lock(m_mutex[queue_type]);
	return m_queue[queue_type].empty();
}


std::size_t NetworkMessageQueue::size(NetworkMessageQueue::QueueType queue_type)
{
	std::lock_guard<std::mutex> lock(m_mutex[queue_type]);
	return m_queue[queue_type].size();
}


void NetworkReactor::onIdle()
{
	process_send_package();
//	SocketReactor::onIdle(); // Avoid sending event to all network connection, because it's not efficient.
}


void NetworkReactor::onBusy()
{
	process_send_package();
//	SocketReactor::onBusy(); // There is nothing in this function.
}


void NetworkReactor::onTimeout()
{
	process_send_package();
//	SocketReactor::onTimeout(); // Avoid sending event to all network connection, because it's not efficient.
}


void NetworkReactor::process_send_package()
{
	for (std::size_t i = 0; i < MAX_SEND_PACKAGE_PROCESS_PER_TIMES; ++i)
	{
		if (NetworkMessageQueue::instance()->empty(NetworkMessageQueue::OUT_QUEUE))
		{
			break;
		}

		auto msg = NetworkMessageQueue::instance()->pop(NetworkMessageQueue::OUT_QUEUE);
		int conn_id = msg.conn_id;
		if (conn_id == 0)
		{
			conn_id = NetworkServiceManager::instance()->get_connection_id(msg.service_id);
		}

		NetworkConnection* conn = NetworkManager::instance()->find_open_connection(conn_id);
		Package package = PackageManager::instance()->find_package(msg.package_id);

		if (conn == nullptr || !package.is_valid())
		{
			if (conn == nullptr)
			{
				LIGHTS_INFO(logger, "Connction {}: Already close.", conn_id);
			}

			if (!package.is_valid())
			{
				LIGHTS_ERROR(logger, "Connction {}: Package {} already remove.", conn_id, msg.package_id);
			}

			if (package.is_valid())
			{
				PackageManager::instance()->remove_package(msg.package_id);
			}

			continue;
		}

		conn->send_package(package);
	}
}


NetworkManager::~NetworkManager()
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
		LIGHTS_ERROR(logger, "Network connection manager destroy unkown error.");
	}
}


NetworkConnection& NetworkManager::register_connection(const std::string& host, unsigned short port)
{
	StreamSocket stream_socket(SocketAddress(host, port));
	NetworkConnection* conn = new NetworkConnection(stream_socket, m_reactor, NetworkConnection::ACTIVE_OPEN);
	return *conn;
}


void NetworkManager::register_listener(const std::string& host, unsigned short port)
{
	ServerSocket server_socket(SocketAddress(host, port));
	m_acceptor_list.emplace_back(server_socket, m_reactor);
	LIGHTS_INFO(logger, "Creates network listener {}.", server_socket.address());
}


void NetworkManager::remove_connection(int conn_id)
{
	NetworkConnection* conn = find_connection(conn_id);
	if (conn)
	{
		conn->close();
	}
}


NetworkConnection* NetworkManager::find_connection(int conn_id)
{
	auto itr = m_conn_list.find(conn_id);
	if (itr == m_conn_list.end())
	{
		return nullptr;
	}

	return itr->second;
}


NetworkConnection& NetworkManager::get_connection(int conn_id)
{
	NetworkConnection* conn = find_connection(conn_id);
	if (conn == nullptr)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_NETWORK_CONNECTION_NOT_EXIST);
	}

	return *conn;
}


NetworkConnection* NetworkManager::find_open_connection(int conn_id)
{
	NetworkConnection* conn = find_connection(conn_id);
	if (conn != nullptr && conn->is_open())
	{
		return conn;
	}

	return nullptr;
}


void NetworkManager::stop_all()
{
	// Cannot use iterator to for each to close, becase close will erase itself on m_conn_list.
	while (!m_conn_list.empty())
	{
		NetworkConnection* conn =  m_conn_list.begin()->second;
		conn->close();
	}
	m_conn_list.clear();
	m_acceptor_list.clear();
}


void NetworkManager::start()
{
	LIGHTS_INFO(logger, "Starting netowk scheduler.");
	m_reactor.setTimeout(Poco::Timespan(0, REACTOR_TIME_OUT_US));
	m_reactor.run();
	LIGHTS_INFO(logger, "Stopped netowk scheduler.");
}


void NetworkManager::stop()
{
	LIGHTS_INFO(logger, "Stopping netowk scheduler.");
	m_reactor.stop();
}


int NetworkManager::on_create_connection(NetworkConnection* conn)
{
	int conn_id = m_next_id;
	++m_next_id;
	m_conn_list.insert(std::make_pair(conn_id, conn));
	return conn_id;
}


void NetworkManager::on_destroy_connection(int conn_id)
{
	m_conn_list.erase(conn_id);
}


NetworkService& NetworkServiceManager::register_service(const std::string& ip, unsigned short port)
{
	NetworkService* old_service = find_service(ip, port);
	if (old_service)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_NETWORK_SERVICE_ALREADY_EXIST);
	}

	NetworkService new_service{ m_next_id, ip, port };
	auto value = std::make_pair(m_next_id, new_service);
	++m_next_id;

	auto result = m_service_list.insert(value);
	if (result.second == false)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_NETWORK_SERVICE_ALREADY_EXIST);
	}

	return result.first->second;
}


void NetworkServiceManager::remove_service(int service_id)
{
	auto itr = m_conn_list.find(service_id);
	if (itr != m_conn_list.end())
	{
		NetworkManager::instance()->remove_connection(itr->second);
	}
	m_service_list.erase(service_id);
}


NetworkService* NetworkServiceManager::find_service(int service_id)
{
	auto itr = m_service_list.find(service_id);
	if (itr == m_service_list.end())
	{
		return nullptr;
	}

	return &(itr->second);
}

NetworkService* NetworkServiceManager::find_service(const std::string& ip, unsigned short port)
{
	auto itr = std::find_if(m_service_list.begin(), m_service_list.end(), [&](const ServiceList::value_type& value_type)
	{
		return value_type.second.ip == ip && value_type.second.port == port;
	});

	if (itr == m_service_list.end())
	{
		return nullptr;
	}

	return &(itr->second);
}


int NetworkServiceManager::get_connection_id(int service_id)
{
	NetworkService* service = find_service(service_id);
	if (service == nullptr)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_NETWORK_SERVICE_NOT_EXIST);
	}

	auto itr = m_conn_list.find(service_id);
	if (itr == m_conn_list.end())
	{
		NetworkConnection& conn = NetworkManager::instance()->register_connection(service->ip, service->port);
		m_conn_list.insert(std::make_pair(service_id, conn.connection_id()));
		m_conn_service_list.insert(std::make_pair(conn.connection_id(), service_id));
		return conn.connection_id();
	}

	int conn_id = itr->second;
	NetworkConnection* conn = NetworkManager::instance()->find_open_connection(conn_id);
	if (conn == nullptr)
	{
		NetworkConnection& new_conn = NetworkManager::instance()->register_connection(service->ip, service->port);
		m_conn_list[service_id] = new_conn.connection_id();
		m_conn_service_list.erase(conn_id);
		m_conn_service_list.insert(std::make_pair(new_conn.connection_id(), service_id));
		return new_conn.connection_id();
	}

	return conn->connection_id();
}


NetworkService* NetworkServiceManager::find_service_by_connection(int conn_id)
{
	auto itr = m_conn_service_list.find(conn_id);
	if (itr == m_conn_service_list.end())
	{
		return nullptr;
	}

	return find_service(itr->second);
}


} // namespace spaceless
