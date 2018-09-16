/*
 * core.cpp
 * @author wherewindblow
 * @date   Jan 09, 2018
 */

#include "core.h"

#include <cmath>
#include <algorithm>
#include <memory>

#include <lights/file.h>
#include <lights/logger.h>
#include <common/exception.h>
#include <common/log.h>

#include <Poco/DirectoryIterator.h>
#include <Poco/Path.h>


namespace spaceless {
namespace storage_node {

static Logger& logger = get_logger("storage_node");

SharingFileManager::SharingFileManager()
{
}


SharingFileManager::FileList SharingFileManager::list_file(const std::string& path) const
{
	FileList file_list;
	Poco::DirectoryIterator itr(get_absolute_path(path));
	Poco::DirectoryIterator end;
	while (itr != end)
	{
		file_list.push_back(itr.name());
	}
	return file_list;
}


void SharingFileManager::create_diretory(const std::string& path)
{

}


void SharingFileManager::remove_diretory(const std::string& path)
{

}


void SharingFileManager::put_file(const std::string& filename, lights::SequenceView file_content, bool is_append, bool is_flush)
{
	std::string path = get_absolute_path(filename);
	lights::FileStream& file = get_file_stream(path);
	file.clear_error();

	if (!is_append)
	{
		file.seek(0, lights::FileSeekWhence::BEGIN);
	}

	file.write(file_content);
	if (is_flush)
	{
		file.flush();
	}
}


std::size_t SharingFileManager::get_file(const std::string& filename, lights::Sequence file_content, int start_pos)
{
	std::string path = get_absolute_path(filename);
	if (!lights::env::file_exists(path.c_str()))
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_FILE_NOT_EXIST)
	}

	lights::FileStream& file = get_file_stream(path);
	file.clear_error();
	file.seek(start_pos, lights::FileSeekWhence::BEGIN);
	return file.read(file_content);
}


const std::string& SharingFileManager::get_sharing_path() const
{
	return m_sharing_path;
}


void SharingFileManager::set_sharing_path(const std::string& sharing_path)
{
	m_sharing_path = sharing_path;
}


std::string SharingFileManager::get_absolute_path(const std::string path) const
{
	return m_sharing_path + Poco::Path::separator() + path;
}


lights::FileStream& SharingFileManager::get_file_stream(const std::string& path)
{
	auto itr = m_file_cache.find(path);
	if (itr == m_file_cache.end())
	{
		auto file = std::make_shared<lights::FileStream>(path, "wb+");
		m_file_cache.insert(std::make_pair(path, file));
		return *file;
	}
	else
	{
		return *itr->second;
	}
}


FileTransferSession& FileTransferSessionManager::register_session(int group_id, const std::string& filename)
{
	FileTransferSession* session = find_session(group_id, filename);
	if (session)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_TRANSFER_SESSION_ALREADY_EXIST);
	}

	FileTransferSession new_session;
	new_session.session_id = m_next_id;
	new_session.group_id = group_id;
	new_session.filename = filename;
	new_session.max_fragment = 0;
	new_session.fragment_index = 0;
	++m_next_id;

	auto value = std::make_pair(new_session.session_id, new_session);
	auto result = m_session_list.insert(value);
	if (result.second == false)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_TRANSFER_SESSION_ALREADY_EXIST);
	}

	return result.first->second;
}


FileTransferSession& FileTransferSessionManager::register_put_session(int group_id,
																	  const std::string& filename,
																	  int max_fragment)
{
	FileTransferSession& session = register_session(group_id, filename);
	session.max_fragment = max_fragment;
	return session;
}


FileTransferSession& FileTransferSessionManager::register_get_session(int group_id,
																	  const std::string& filename,
																	  int fragment_content_len)
{
	FileTransferSession& session = register_session(group_id, filename);

	std::string local_filename = SharingFileManager::instance()->get_absolute_path(filename);
	lights::FileStream file(local_filename, "r");
	float file_size = static_cast<float>(file.size());
	int max_fragment = static_cast<int>(std::ceil(file_size / fragment_content_len));
	session.max_fragment = max_fragment;
	return session;
}


void FileTransferSessionManager::remove_session(int session_id)
{
	m_session_list.erase(session_id);
}


FileTransferSession* FileTransferSessionManager::find_session(int session_id)
{
	auto itr = m_session_list.find(session_id);
	if (itr == m_session_list.end())
	{
		return nullptr;
	}
	return &itr->second;
}


FileTransferSession* FileTransferSessionManager::find_session(int uid, const std::string& filename)
{
	auto itr = std::find_if(m_session_list.begin(), m_session_list.end(), [&](const SessionList::value_type& value_type)
	{
		return value_type.second.group_id == uid && value_type.second.filename == filename;
	});

	if (itr == m_session_list.end())
	{
		return nullptr;
	}

	return &(itr->second);
}


} // namespace storage_node
} // namespace spaceless
