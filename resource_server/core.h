/**
 * core.h
 * @author wherewindblow
 * @date   Oct 26, 2017
 */

#pragma once

#include <vector>
#include <set>
#include <map>
#include <lights/sequence.h>
#include <lights/exception.h>
#include <common/network.h>
#include <common/basics.h>


namespace spaceless {
namespace resource_server {

const int MODULE_RESOURCE_SERVER = 1000;

enum
{
	ERR_USER_ALREADY_EXIST = 1000,
	ERR_USER_NOT_EXIST = 1001,

	ERR_GROUP_ALREADY_EXIST = 1100,
	ERR_GROUP_NOT_EXIST = 1101,
	ERR_GROUP_NOT_PERMIT_NEED_OWNER = 1102,
	ERR_GROUP_NOT_PERMIT_NEED_MANAGER = 1103,
	ERR_GROUP_NOT_PERMIT_NEED_MEMBER = 1104,
	ERR_GROUP_CREATE_DIR_MUST_UNDER_DIR = 1105,
	ERR_GROUP_REMOVE_DIR_MUST_UNDER_DIR = 1106,
	ERR_GROUP_CANNOT_REMOVE_ROOT_DIR = 1107,
	ERR_GROUP_CANNOT_KICK_OUT_OWNER = 1110,

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


struct User
{
	int user_id;
	std::string username;
	std::string password;
	std::vector<int> group_list;
	int conn_id = 0;
};


inline bool operator== (const User& lhs, const User& rhs)
{
	return lhs.user_id == rhs.user_id;
}


class UserManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(UserManager);

	User& register_user(const std::string& username, const std::string& password);

	bool login_user(int user_id, const std::string& password, NetworkConnection& conn);

	void remove_user(int user_id);

	User* find_user(int user_id);

	User* find_user(const std::string& username);

	User& get_user(int user_id);

	User& get_user(const std::string& username);

private:
	using UserList = std::map<int, User>;
	UserList m_user_list;
	int m_next_id = 1;
};


class SharingGroup
{
public:
	using UserList = std::vector<int>;

	SharingGroup(int group_id, const std::string& group_name, int ower_id, int root_dir_id);

	int group_id() const;

	const std::string& group_name() const;

	int owner_id() const;

	void put_file(int user_id, const std::string& filename, lights::SequenceView file_content, bool is_append = false);

	std::size_t get_file(int user_id, const std::string& filename, lights::Sequence file_content, int start_pos = 0);

	/**
	 * Create all directory by directory path. If a parent directory is not create will automatically create.
	 * @param user_id  User id.
	 * @param full_dir_path Full path of directory. Like "this/is/a/path".
	 */
	void create_directory(int user_id, const std::string& full_dir_path);

	/**
	 * Remove last directory of directory path.
	 * @param user_id User id.
	 * @param full_dir_path Full path of directory. Like "this/is/a/path".
	 */
	void remove_directory(int user_id, const std::string& full_dir_path);

	void join_group(int user_id);

	void kick_out_user(int user_id);

	const UserList& manager_list() const;

	const UserList& member_list() const;

	bool is_manager(int user_id);

	bool is_member(int user_id);

private:
	int m_group_id;
	std::string m_group_name;
	int m_owner_id;
	int m_root_dir_id;
	UserList m_manager_list; // Manager list include owner and general managers.
	UserList m_member_list; // Member list include all members in group.
};


inline bool operator== (const SharingGroup& lhs, const SharingGroup& rhs)
{
	return lhs.group_id() == rhs.group_id();
}

extern const char* group_file_path;

class SharingGroupManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(SharingGroupManager);

	SharingGroup& register_group(int user_id, const std::string& group_name);

	void remove_group(int user_id, int group_id);

	SharingGroup* find_group(int group_id);

	SharingGroup* find_group(const std::string& group_name);

	SharingGroup& get_group(int group_id);

	SharingGroup& get_group(const std::string& group_name);

private:
	using GroupList = std::map<int, SharingGroup>;
	GroupList m_group_list;
	int m_next_id = 1;
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

	FileTransferSession* find_session(int group_id, const std::string& filename);

private:
	using SessionList = std::map<int, FileTransferSession>;
	SessionList m_session_list;
	int m_next_id = 1;
};


struct SharingFile
{
	enum FileType
	{
		GENERAL_FILE,
		DIRECTORY,
	};

	int file_id;
	FileType file_type;
	std::string file_name;
	int node_id; // Only available when file_type is GENERAL_FILE.
	std::string node_file_name; // Only available when file_type is GENERAL_FILE.
	std::vector<int> inside_file_list; // Only available when file_type is DIRECTORY.
};


class SharingFileManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(SharingFileManager);

	SharingFile& register_file(SharingFile::FileType file_type,
							   const std::string& file_name,
							   int node_id = 0,
							   const std::string& node_file_name = std::string());

	void remove_file(int file_id);

	SharingFile* find_file(int file_id);

	SharingFile* find_file(int node_id, const std::string& node_file_name);

	SharingFile& get_file(int file_id);

	SharingFile& get_file(int node_id, const std::string& node_file_name);

private:
	using SharingFileList = std::map<int, SharingFile>;
	using StorageFile = std::pair<int, std::string>;
	using StorageFileList = std::map<StorageFile, int>;
	SharingFileList m_sharing_file_list;
	StorageFileList m_storage_file_list;
	int m_next_id = 1;
};


struct StorageNode
{
	int node_id;
	std::string node_ip;
	short node_port;
};


class StorageNodeManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(StorageNodeManager);

	StorageNode& register_node(const std::string& node_ip, short node_port);

	void remove_node(int node_id);

	StorageNode* find_node(int node_id);

	StorageNode* find_node(const std::string& node_ip, short node_port);

	StorageNode& get_node(int node_id);

	StorageNode& get_node(const std::string& node_ip, short node_port);

private:
	using StorageNodeList = std::map<int, StorageNode>;
	StorageNodeList m_node_list;
	int m_next_id = 1;
};

extern NetworkConnection* storage_node_conn;

} // namespace resource_server
} // namespace spaceless
