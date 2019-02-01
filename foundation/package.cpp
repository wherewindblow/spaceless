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

void Package::parse_to_protocol(protocol::Message& msg) const
{
	bool ok = protocol::parse_to_message(content(), msg);
	if (!ok)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_NETWORK_PACKAGE_CANNOT_PARSE_TO_PROTOCOL);
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
	if (result.second == false)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_NETWORK_PACKAGE_ALREADY_EXIST);
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
		LIGHTS_THROW_EXCEPTION(Exception, ERR_NETWORK_PACKAGE_NOT_EXIST);
	}

	return package;
}


std::size_t PackageManager::size()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_package_list.size();
}


} // namespace spaceless
