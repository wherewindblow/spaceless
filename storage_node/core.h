/**
 * core.h
 * @author wherewindblow
 * @date   Jan 09, 2018
 */

#pragma once

#include <string>
#include <vector>
#include <map>

#include <lights/sequence.h>
#include <common/basics.h>


namespace spaceless {
namespace storage_node {

const int MODULE_STORAGE_NODE = 1001;

enum
{
	ERR_FILE_ALREADY_EXIST = 1200,
	ERR_FILE_CANNOT_CREATE = 1201,
	ERR_FILE_NOT_EXIST = 1202,

	ERR_NODE_ALREADY_EXIST = 1300,
	ERR_NODE_CANNOT_CREATE = 1301,
	ERR_NODE_NOT_EXIST = 1302,

	ERR_TRANSFER_SESSION_ALREADY_EXIST = 5000,
	ERR_TRANSFER_SESSION_CANNOT_CREATE = 5001,
	ERR_TRANSFER_SESSION_NOT_EXIST = 5002,
};


struct FileTransferSession
{
	int session_id;
	int group_id;
	std::string filename;
	int max_fragment;
	int fragment_index;
};


class FileTransferSessionManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(FileTransferSessionManager);

	FileTransferSession& register_session(int group_id, const std::string& filename);

	FileTransferSession& register_put_session(int group_id, const std::string& filename, int max_fragment);

	FileTransferSession& register_get_session(int group_id, const std::string& filename, int fragment_content_len);

	void remove_session(int session_id);

	FileTransferSession* find_session(int session_id);

	FileTransferSession* find_session(int uid, const std::string& filename);

private:
	using SessionList = std::map<int, FileTransferSession>;
	SessionList m_session_list;
	int m_next_id = 1;
};



class SharingFileManager
{
public:
	using FileList = std::vector<std::string>;

	SPACELESS_SINGLETON_INSTANCE(SharingFileManager);

	SharingFileManager();

	FileList list_file(const std::string& path) const;

	void create_diretory(const std::string& path);

	void remove_diretory(const std::string& path);

	void put_file(const std::string& filename, lights::SequenceView file_content, bool is_append = false);

	std::size_t get_file(const std::string& filename, lights::Sequence file_content, int start_pos = 0);

	std::string get_absolute_path(const std::string path) const;

private:
	std::string m_file_path;
public:
	const std::string& get_sharing_file_path() const;

	void set_sharing_file_path(const std::string& file_path);
};

} // namespace storage_node
} // namespace spaceless