/*
 * network.cpp
 * @author wherewindblow
 * @date   Nov 01, 2017
 */

#include "network.h"

#include <execinfo.h>

#include <Poco/Net/NetException.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Observer.h>


namespace spaceless {

using Poco::Net::SocketAddress;

PackageBuffer& PackageBufferManager::register_package()
{
	auto value = std::make_pair(m_next_id, PackageBuffer(m_next_id));
	++m_next_id;

	auto result = m_package_list.insert(value);
	if (result.second == false)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_NETWORK_PACKAGE_ALREADY_EXIST);
	}

	return result.first->second;
}


void PackageBufferManager::remove_package(int package_id)
{
	m_package_list.erase(package_id);
}


PackageBuffer* PackageBufferManager::find_package(int package_id)
{
	auto itr = m_package_list.find(package_id);
	if (itr == m_package_list.end())
	{
		return nullptr;
	}

	return &itr->second;
}


PackageBuffer& PackageBufferManager::get_package(int package_id)
{
	PackageBuffer* package = find_package(package_id);
	if (package == nullptr)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_NETWORK_PACKAGE_NOT_EXIST);
	}

	return *package;
}


MultiplyPhaseTranscation::MultiplyPhaseTranscation(int trans_id):
	m_id(trans_id)
{
}


MultiplyPhaseTranscation* MultiplyPhaseTranscation::register_transcation(int trans_id)
{
	return nullptr;
}


void MultiplyPhaseTranscation::pre_on_init(NetworkConnection& conn, const PackageBuffer& package)
{
	m_first_conn = &conn;
}


MultiplyPhaseTranscation::PhaseResult MultiplyPhaseTranscation::on_timeout()
{
	return EXIT_TRANCATION;
}


MultiplyPhaseTranscation::PhaseResult MultiplyPhaseTranscation::wait_next_phase(NetworkConnection& conn,
																				int cmd,
																				int current_phase,
																				int timeout)
{
	m_wait_conn = &conn;
	m_wait_cmd = cmd;
	m_current_phase = current_phase;
	// TODO: How to implement timeout of transcation.
	return WAIT_NEXT_PHASE;
}


int MultiplyPhaseTranscation::transcation_id() const
{
	return m_id;
}


int MultiplyPhaseTranscation::current_phase() const
{
	return m_current_phase;
}


NetworkConnection* MultiplyPhaseTranscation::first_connection()
{
	return m_first_conn;
}


NetworkConnection* MultiplyPhaseTranscation::waiting_connection()
{
	return m_wait_conn;
}


int MultiplyPhaseTranscation::waiting_command() const
{
	return m_wait_cmd;
}


MultiplyPhaseTranscation& MultiplyPhaseTranscationManager::register_transcation(TranscationFatory trans_fatory)
{
	MultiplyPhaseTranscation* trans = trans_fatory(m_next_id);
	++m_next_id;

	auto value = std::make_pair(trans->transcation_id(), trans);
	auto result = m_trans_list.insert(value);
	if (result.second == false)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_MULTIPLY_PHASE_TRANSCATION_ALREADY_EXIST);
	}

	return *trans;
}


void MultiplyPhaseTranscationManager::remove_transcation(int trans_id)
{
	MultiplyPhaseTranscation* trans = find_transcation(trans_id);
	if (trans)
	{
		delete trans;
	}

	m_trans_list.erase(trans_id);
}


MultiplyPhaseTranscation* MultiplyPhaseTranscationManager::find_transcation(int trans_id)
{
	auto itr = m_trans_list.find(trans_id);
	if (itr == m_trans_list.end())
	{
		return nullptr;
	}

	return itr->second;
}


void TranscationManager::register_transcation(int cmd, TranscationType trans_type, void* handler)
{
	auto value = std::make_pair(cmd, Transcation {trans_type, handler});
	auto result = m_trans_list.insert(value);
	if (result.second == false)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_TRANSCATION_ALREADY_EXIST);
	}
}


void TranscationManager::register_one_phase_transcation(int cmd, OnePhaseTrancation trancation)
{
	void* handler = reinterpret_cast<void*>(trancation);
	register_transcation(cmd, TranscationType::ONE_PHASE_TRANSCATION, handler);
}


void TranscationManager::register_multiply_phase_transcation(int cmd, TranscationFatory trans_fatory)
{
	void* handler = reinterpret_cast<void*>(trans_fatory);
	register_transcation(cmd, TranscationType::MULTIPLY_PHASE_TRANSCATION, handler);
}


void TranscationManager::remove_transcation(int cmd)
{
	m_trans_list.erase(cmd);
}


Transcation* TranscationManager::find_transcation(int cmd)
{
	auto itr = m_trans_list.find(cmd);
	if (itr == m_trans_list.end())
	{
		return nullptr;
	}
	return &itr->second;
}


NetworkConnection::NetworkConnection(StreamSocket& socket, SocketReactor& reactor) :
	m_socket(socket),
	m_reactor(reactor),
	m_read_buffer(0),
	m_readed_len(0),
	m_read_state(ReadState::READ_HEADER),
	m_sended_len(0),
	m_attachment(nullptr)
{
	m_socket.setBlocking(false);

	Poco::Observer<NetworkConnection, ReadableNotification> readable(*this, &NetworkConnection::on_readable);
	m_reactor.addEventHandler(m_socket, readable);
	Poco::Observer<NetworkConnection, ShutdownNotification> shutdown(*this, &NetworkConnection::on_shutdown);
	m_reactor.addEventHandler(m_socket, shutdown);
//	Poco::Observer<NetworkConnection, TimeoutNotification> timeout(*this, &NetworkConnection::on_timeout);
//	m_reactor.addEventHandler(m_socket, timeout);
	Poco::Observer<NetworkConnection, ErrorNotification> error(*this, &NetworkConnection::on_error);
	m_reactor.addEventHandler(m_socket, error);

	NetworkConnectionManager::instance()->m_conn_list.push_back(this);
	m_id = NetworkConnectionManager::instance()->m_next_id;
	++NetworkConnectionManager::instance()->m_next_id;

	try
	{
		std::string address = m_socket.address().toString();
		std::string peer_address = m_socket.peerAddress().toString();
		SPACELESS_DEBUG(MODULE_NETWORK, "Creates network connection {}: local {} and peer {}.",
						m_id, address, peer_address);
	}
	catch (Poco::Exception& ex)
	{
		SPACELESS_DEBUG(MODULE_NETWORK, "Creates network connection {}: local unkown and peer unkown.", m_id);
	}
}


NetworkConnection::~NetworkConnection()
{
	try
	{
		SPACELESS_DEBUG(MODULE_NETWORK, "Destroys network connection {}.", m_id);
		auto& conn_list = NetworkConnectionManager::instance()->m_conn_list;
		auto need_delete = std::find_if(conn_list.begin(), conn_list.end(), [this](const NetworkConnection* conn)
		{
			return conn->connection_id() == this->connection_id();
		});
		conn_list.erase(need_delete);

		Poco::Observer<NetworkConnection, ReadableNotification> readable(*this, &NetworkConnection::on_readable);
		m_reactor.removeEventHandler(m_socket, readable);
		Poco::Observer<NetworkConnection, ShutdownNotification> shutdown(*this, &NetworkConnection::on_shutdown);
		m_reactor.removeEventHandler(m_socket, shutdown);
		//	Poco::Observer<NetworkConnection, TimeoutNotification> timeout(*this, &NetworkConnection::on_timeout);
		//	m_reactor.removeEventHandler(m_socket, timeout);
		Poco::Observer<NetworkConnection, ErrorNotification> error(*this, &NetworkConnection::on_error);
		m_reactor.removeEventHandler(m_socket, error);

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
		SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: {}", m_id, ex.what());
	}
	catch (...)
	{
		SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: Unkown error", m_id);
	}
}


int NetworkConnection::connection_id() const
{
	return m_id;
}


void NetworkConnection::on_readable(ReadableNotification* notification)
{
	notification->release();
	read_for_state();
}


void NetworkConnection::on_writable(WritableNotification* notification)
{
	notification->release();
	const PackageBuffer* package = nullptr;
	while (!m_send_list.empty())
	{
		package = PackageBufferManager::instance()->find_package(m_send_list.front());
		if (package == nullptr)
		{
			m_send_list.pop();
		}
		else
		{
			break;
		}
	}

	if (package == nullptr)
	{
		return;
	}

	int len = static_cast<int>(package->total_length());
	m_sended_len += m_socket.sendBytes(package->data() + m_sended_len, len - m_sended_len);

	if (m_sended_len == len)
	{
		m_sended_len = 0;
		m_send_list.pop();
		PackageBufferManager::instance()->remove_package(package->package_id());
	}

	if (m_send_list.empty())
	{
		Poco::Observer<NetworkConnection, WritableNotification> observer(*this, &NetworkConnection::on_writable);
		m_reactor.removeEventHandler(m_socket, observer);
	}
}


void NetworkConnection::on_shutdown(ShutdownNotification* notification)
{
	notification->release();
	close();
}


void NetworkConnection::on_timeout(TimeoutNotification* notification)
{
	notification->release();
	SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: On time out.", m_id);
}


void NetworkConnection::on_error(ErrorNotification* notification)
{
	notification->release();
	SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: On error.", m_id);
}


void NetworkConnection::send_package(const PackageBuffer& package)
{
	if (!m_send_list.empty())
	{
		m_send_list.push(package.package_id());
		return;
	}

	int len = static_cast<int>(package.total_length());
	m_sended_len = m_socket.sendBytes(package.data(), len);
	if (m_sended_len == len)
	{
		m_sended_len = 0;
	}
	else if (m_sended_len < len)
	{
		m_send_list.push(package.package_id());
		Poco::Observer<NetworkConnection, WritableNotification> observer(*this, &NetworkConnection::on_writable);
		m_reactor.addEventHandler(m_socket, observer);
	}
}


void NetworkConnection::close()
{
	delete this;
}


void NetworkConnection::set_attachment(void* attachment)
{
	m_attachment = attachment;
}


void* NetworkConnection::get_attachment()
{
	return m_attachment;
}


StreamSocket& NetworkConnection::stream_socket()
{
	return m_socket;
}


const StreamSocket& NetworkConnection::stream_socket() const
{
	return m_socket;
}


void NetworkConnection::read_for_state(int deep)
{
	if (deep > 5) // Reduce call recursive too deeply.
	{
		return;
	}

	switch (m_read_state)
	{
		case ReadState::READ_HEADER:
		{
			int len = m_socket.receiveBytes(m_read_buffer.data() + m_readed_len,
											static_cast<int>(PackageBuffer::MAX_HEADER_LEN) - m_readed_len);

			if (len == -1) // Not available bytes in buffer.
			{
				return;
			}

			if (len == 0 && deep == 0) // Closes by peer.
			{
				close();
				return;
			}

			m_readed_len += len;
			if (m_readed_len == PackageBuffer::MAX_HEADER_LEN)
			{
				m_readed_len = 0;
				m_read_state = ReadState::READ_CONTENT;
			}
			break;
		}

		case ReadState::READ_CONTENT:
		{
			if (m_read_buffer.header().content_length != 0)
			{
				int len = m_socket.receiveBytes(m_read_buffer.data() + PackageBuffer::MAX_HEADER_LEN + m_readed_len,
												m_read_buffer.header().content_length - m_readed_len);
				if (len == -1) // Not available bytes in buffer.
				{
					return;
				}
				m_readed_len += len;
			}

			if (m_readed_len == m_read_buffer.header().content_length)
			{
				m_readed_len = 0;
				m_read_state = ReadState::READ_HEADER;

				trigger_transcation();
			}
			break;
		}
	}

	read_for_state(++deep);
}


void NetworkConnection::trigger_transcation()
{
	try
	{
		int trans_id = m_read_buffer.header().trigger_trans_id;
		int command = m_read_buffer.header().command;

		if (trans_id != 0)
		{
			MultiplyPhaseTranscation* trans_handler =
				MultiplyPhaseTranscationManager::instance()->find_transcation(trans_id);
			if (trans_handler)
			{
				// Check the send rsponse connection is same as send request connection. Don't give chance to
				// other to interrupt not self transcation.
				if (this == trans_handler->waiting_connection() && command == trans_handler->waiting_command())
				{
					SPACELESS_DEBUG(MODULE_NETWORK, "Network connction {}: Trigger cmd {}, Active trans_id {}, phase {}.",
									m_id, command, trans_id, trans_handler->current_phase());
					MultiplyPhaseTranscation::PhaseResult result = trans_handler->on_active(*this, m_read_buffer);
					if (result == MultiplyPhaseTranscation::EXIT_TRANCATION)
					{
						MultiplyPhaseTranscationManager::instance()->remove_transcation(trans_id);
						SPACELESS_DEBUG(MODULE_NETWORK, "Network connction {}: End trans_id {}.", m_id, trans_id);
					}
				}
				else
				{
					SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: cmd {} not fit with conn_id {}, cmd {}.",
									m_id,
									command,
									trans_handler->waiting_connection()->connection_id(),
									trans_handler->waiting_command());
				}
			}
			else
			{
				SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: Unkown trans_id {}.", m_id, trans_id);
			}
		}
		else
		{
			auto transcation = TranscationManager::instance()->find_transcation(command);
			if (transcation)
			{
				switch (transcation->trans_type)
				{
					case TranscationType::ONE_PHASE_TRANSCATION:
					{
						SPACELESS_DEBUG(MODULE_NETWORK, "Network connction {}: Trigger cmd {}.", m_id, command);
						OnePhaseTrancation trans_handler =
							reinterpret_cast<OnePhaseTrancation>(transcation->trans_handler);
						trans_handler(*this, m_read_buffer);
						break;
					}
					case TranscationType::MULTIPLY_PHASE_TRANSCATION:
					{
						TranscationFatory trans_factory = reinterpret_cast<TranscationFatory>(transcation->trans_handler);
						MultiplyPhaseTranscation& trans_handler =
							MultiplyPhaseTranscationManager::instance()->register_transcation(trans_factory);

						SPACELESS_DEBUG(MODULE_NETWORK, "Network connction {}: Trigger cmd {}, Start trans_id {}.",
										m_id, command, trans_handler.transcation_id());
						trans_handler.pre_on_init(*this, m_read_buffer);
						MultiplyPhaseTranscation::PhaseResult result = trans_handler.on_init(*this, m_read_buffer);
						if (result == MultiplyPhaseTranscation::EXIT_TRANCATION)
						{
							MultiplyPhaseTranscationManager::instance()->remove_transcation(trans_handler.transcation_id());
							SPACELESS_DEBUG(MODULE_NETWORK, "Network connction {}: End trans_id {}.",
											m_id, trans_handler.transcation_id());
						}
						break;
					}
					default:
						LIGHTS_ASSERT(false && "Invalid TranscationType");
						break;
				}
			}
			else
			{
				SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: Unkown command {}.", m_id, command);
			}
		}
	}
	catch (const Exception& ex)
	{
		SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: {}/{}.", m_id, ex.code(), ex);
	}
	catch (const std::exception& ex)
	{
		SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: {}.", m_id, ex.what());
	}
	catch (...)
	{
		SPACELESS_ERROR(MODULE_NETWORK, "Network connction {}: Unkown error.", m_id);
	}
}


NetworkConnectionManager::~NetworkConnectionManager()
{
	try
	{
		stop_all();
	}
	catch (std::exception& ex)
	{
		SPACELESS_ERROR(MODULE_NETWORK, ex.what());
	}
	catch (...)
	{
		SPACELESS_ERROR(MODULE_NETWORK, "Unkown error");
	}
}


NetworkConnection& NetworkConnectionManager::register_connection(const std::string& host, unsigned short port)
{
	StreamSocket stream_socket(SocketAddress(host, port));
	NetworkConnection* conn = new NetworkConnection(stream_socket, m_reactor);
	return *conn;
}


void NetworkConnectionManager::register_listener(const std::string& host, unsigned short port)
{
	ServerSocket serve_socket(SocketAddress(host, port));
	m_acceptor_list.emplace_back(serve_socket, m_reactor);
	SPACELESS_DEBUG(MODULE_NETWORK, "Creates network listener {}.", serve_socket.address());
}


void NetworkConnectionManager::remove_connection(int conn_id)
{
	NetworkConnection* conn = find_connection(conn_id);
	if (conn)
	{
		conn->close();
	}
}


NetworkConnection* NetworkConnectionManager::find_connection(int conn_id)
{
	auto itr = std::find_if(m_conn_list.begin(), m_conn_list.end(), [conn_id](const NetworkConnection* connection)
	{
		return connection->connection_id() == conn_id;
	});

	if (itr == m_conn_list.end())
	{
		return nullptr;
	}

	return *itr;
}


void NetworkConnectionManager::stop_all()
{
	// Cannot use iterator to for each to close, becase close will erase itself on m_conn_list.
	while (!m_conn_list.empty())
	{
		(*m_conn_list.begin())->close();
	}
	m_conn_list.clear();
	m_acceptor_list.clear();
}


void NetworkConnectionManager::run()
{
	m_reactor.run();
}

} // namespace spaceless
