/**
 * package.cpp
 * @author wherewindblow
 * @date   May 26, 2018
 */

#include "package.h"
#include <protocol/message.h>


namespace spaceless {

void PackageBuffer::parse_as_protocol(protocol::Message& msg) const
{
	lights::SequenceView storage = content();
	bool ok = msg.ParseFromArray(storage.data(), static_cast<int>(storage.length()));
	if (!ok)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_NETWORK_PACKAGE_CANNOT_PARSE_AS_PROTOCOL);
	}
}


PackageBuffer& PackageBufferManager::register_package()
{
	std::lock_guard<std::mutex> lock(m_mutex);

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
	std::lock_guard<std::mutex> lock(m_mutex);
	m_package_list.erase(package_id);
}


PackageBuffer* PackageBufferManager::find_package(int package_id)
{
	std::lock_guard<std::mutex> lock(m_mutex);

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


} // namespace spaceless
