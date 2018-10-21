/**
 * package.cpp
 * @author wherewindblow
 * @date   May 26, 2018
 */

#include "package.h"
#include <protocol/message.h>


namespace spaceless {

void Package::parse_to_protocol(protocol::Message& msg) const
{
	bool ok = protocol::parse_to_message(content(), msg);
	if (!ok)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_NETWORK_PACKAGE_CANNOT_PARSE_TO_PROTOCOL);
	}
}


Package PackageManager::register_package(lights::SequenceView data)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	char* new_data = new char[data.length()];
	lights::copy_array(new_data, static_cast<const char*>(data.data()), data.length());
	lights::Sequence sequence(new_data, data.length());
	auto value = std::make_pair(m_next_id, sequence);
	++m_next_id;

	auto result = m_package_list.insert(value);
	if (result.second == false)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_NETWORK_PACKAGE_ALREADY_EXIST);
	}

	return { result.first->first, result.first->second };
}


Package PackageManager::register_package(int content_len)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	std::size_t len = Package::HEADER_LEN + content_len;
	char* data = new char[len];
	lights::Sequence sequence(data, len);
	auto value = std::make_pair(m_next_id, sequence);
	++m_next_id;

	auto result = m_package_list.insert(value);
	if (result.second == false)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_NETWORK_PACKAGE_ALREADY_EXIST);
	}

	return { result.first->first, result.first->second };
}


void PackageManager::remove_package(int package_id)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	auto itr = m_package_list.find(package_id);
	if (itr != m_package_list.end())
	{
		delete[] static_cast<char*>(itr->second.data());
		m_package_list.erase(itr);
	}
}


Package PackageManager::find_package(int package_id)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	auto itr = m_package_list.find(package_id);
	if (itr == m_package_list.end())
	{
		return {};
	}

	return { itr->first, itr->second };
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
	return m_package_list.size();
}


} // namespace spaceless
