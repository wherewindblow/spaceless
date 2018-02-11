/*
 * core.cpp
 * @author wherewindblow
 * @date   Jan 09, 2018
 */

#include "core.h"

#include <cmath>
#include <algorithm>

#include <lights/file.h>
#include <common/exception.h>
#include <Poco/DirectoryIterator.h>


namespace spaceless {
namespace storage_node {

SharingFileManager::SharingFileManager():
	m_file_path("/tmp/")
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
	return spaceless::storage_node::SharingFileManager::FileList();
}


void SharingFileManager::create_diretory(const std::string& path)
{

}


void SharingFileManager::remove_diretory(const std::string& path)
{

}

void SharingFileManager::put_file(const std::string& filename, lights::SequenceView file_content, bool is_append)
{
	const char* mode = is_append ? "a" : "w";
	lights::FileStream file(get_absolute_path(filename), mode);
	file.write(file_content);
	// TODO Need to optimize.
}


std::size_t SharingFileManager::get_file(const std::string& filename, lights::Sequence file_content, int start_pos)
{
	lights::FileStream file(get_absolute_path(filename), "r");
	file.seek(start_pos, lights::FileSeekWhence::BEGIN);
	return file.read(file_content);
	// TODO Need to optimize.
}

const std::string& SharingFileManager::get_sharing_file_path() const
{
	return m_file_path;
}


void SharingFileManager::set_sharing_file_path(const std::string& file_path)
{
	m_file_path = file_path;
}


std::string SharingFileManager::get_absolute_path(const std::string path) const
{
	return m_file_path + "/" + path;
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

	auto itr = m_session_list.insert(std::make_pair(new_session.session_id, new_session));
	if (itr.second == false)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_TRANSFER_SESSION_ALREADY_EXIST);
	}

	return itr.first->second;
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

	std::string local_filename = SharingFileManager::instance()->get_sharing_file_path() + filename;
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
	auto itr = std::find_if(m_session_list.begin(), m_session_list.end(),
							[&](const SessionList::value_type& value_type)
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
