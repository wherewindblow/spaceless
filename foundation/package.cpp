/**
 * package.cpp
 * @author wherewindblow
 * @date   May 26, 2018
 */

#include "package.h"

#include <string>
#include <protocol/message.h>
#include <crypto/aes.h>


namespace spaceless {

bool PackageBuffer::expect_content_length(std::size_t content_length)
{
	if (m_length - HEADER_LEN >= content_length)
	{
		return true;
	}

	std::size_t old_length = (m_length == STACK_BUFFER_LEN) ? FIRST_HEAP_BUFFER_LEN : m_length;
	std::size_t new_length = old_length;

	// Align buffer with FIRST_HEAP_BUFFER_LEN.
	while (new_length - HEADER_LEN < content_length)
	{
		new_length = old_length * 2;
		old_length = new_length;
	}

	if (new_length > MAX_BUFFER_LEN)
	{
		return false;
	}

	char* old_data = m_data;
	m_data = new char[new_length];
	lights::copy_array(m_data, old_data, sizeof(PackageHeader));
	m_length = new_length;

	if (old_data != m_buffer)
	{
		delete[] old_data;
	}
	return true;
}


void Package::parse_to_protocol(protocol::Message& msg) const
{
	bool ok = protocol::parse_to_message(content(), msg);
	if (!ok)
	{
		LIGHTS_THROW(Exception, ERR_NETWORK_PACKAGE_CANNOT_PARSE_TO_PROTOCOL);
	}
}


Package PackageManager::register_package(int content_len)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	// To support crypto in-place operation.
	std::size_t cipher_content_len = crypto::aes_cipher_length(static_cast<size_t>(content_len));
	
	std::size_t len = Package::HEADER_LEN + cipher_content_len;
	char* data = new char[len];
	Package::Entry entry(m_next_id, len, data);
	auto value = std::make_pair(m_next_id, entry);
	++m_next_id;

	auto result = m_package_list.insert(value);
	if (!result.second)
	{
		LIGHTS_THROW(Exception, ERR_NETWORK_PACKAGE_ALREADY_EXIST);
	}

	Package package(&result.first->second);
	package.header().reset();
	return package;
}


void PackageManager::remove_package(int package_id)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	auto itr = m_package_list.find(package_id);
	if (itr != m_package_list.end())
	{
		delete[] itr->second.data;
		m_package_list.erase(itr);
	}
}


Package PackageManager::find_package(int package_id)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	auto itr = m_package_list.find(package_id);
	if (itr == m_package_list.end())
	{
		return Package();
	}

	return Package(&itr->second);
}


Package PackageManager::get_package(int package_id)
{
	Package package = find_package(package_id);
	if (!package.is_valid())
	{
		LIGHTS_THROW(Exception, ERR_NETWORK_PACKAGE_NOT_EXIST);
	}

	return package;
}


std::size_t PackageManager::size()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_package_list.size();
}

} // namespace spaceless
