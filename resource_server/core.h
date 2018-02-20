/**
 * core.h
 * @author wherewindblow
 * @date   Oct 26, 2017
 */

#pragma once

#include <vector>
#include <set>
#include <map>

#include <Poco/StringTokenizer.h>
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
	ERR_GROUP_USER_NOT_JOIN = 1107,
	ERR_GROUP_ALREADY_IS_MANAGER = 1108,
	ERR_GROUP_ALREADY_IS_MEMEBER = 1109,
	ERR_GROUP_CANNOT_REMOVE_ROOT_DIR = 1110,
	ERR_GROUP_CANNOT_KICK_OUT_OWNER = 1111,

	ERR_FILE_ALREADY_EXIST = 1200,
	ERR_FILE_CANNOT_CREATE = 1201,
	ERR_FILE_NOT_EXIST = 1202,

	ERR_NODE_ALREADY_EXIST = 1300,
	ERR_NODE_CANNOT_CREATE = 1301,
	ERR_NODE_NOT_EXIST = 1302,
};


struct User
{
	int user_id;
	std::string user_name;
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

	User* find_user(const std::string& user_name);

	User& get_user(int user_id);

	User& get_user(const std::string& username);

private:
	using UserList = std::map<int, User>;
	UserList m_user_list;
	int m_next_id = 1;
};


class FilePath
{
public:
	static const char SEPARATOR[];

	static const char ROOT[];

	using SplitReult = std::unique_ptr<Poco::StringTokenizer>;

	FilePath(const std::string& path);

	SplitReult split() const;

	std::string directory_path() const;

	std::string filename() const;

private:
	std::string m_path;
};


class SharingGroup
{
public:
	using UserList = std::vector<int>;

	SharingGroup(int group_id, const std::string& group_name, int ower_id, int root_dir_id);

	int group_id() const;

	const std::string& group_name() const;

	int owner_id() const;

	int root_dir_id() const;

	const UserList& manager_list() const;

	const UserList& member_list() const;

	bool is_manager(int user_id);

	bool is_member(int user_id);

	void join_group(int user_id);

	void assign_as_manager(int user_id);

	void assign_as_memeber(int user_id);

	void kick_out_user(int user_id);

	int get_file_id(const FilePath& path);

	bool exist_path(const FilePath& path);

	void add_file(const FilePath& path, int file_id);

	void put_file(int user_id, const std::string& filename, lights::SequenceView file_content, bool is_append = false);

	std::size_t get_file(int user_id, const std::string& filename, lights::Sequence file_content, int start_pos = 0);

	/**
	 * Creates all directory by path. If a parent directory is not create will automatically create.
	 */
	void create_path(const FilePath& path);

	/**
	 * Removes last file of path. Can remove directory and general file.
	 */
	void remove_path(const FilePath& path);

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
	unsigned short node_port;
	int conn_id;
};


class StorageNodeManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(StorageNodeManager);

	StorageNode& register_node(const std::string& node_ip, unsigned short node_port);

	void remove_node(int node_id);

	StorageNode* find_node(int node_id);

	StorageNode* find_node(const std::string& node_ip, unsigned short node_port);

	StorageNode& get_node(int node_id);

	StorageNode& get_node(const std::string& node_ip, unsigned short node_port);

private:
	using StorageNodeList = std::map<int, StorageNode>;
	StorageNodeList m_node_list;
	int m_next_id = 1;
};


extern StorageNode* default_storage_node;
extern NetworkConnection* default_storage_conn;

} // namespace resource_server
} // namespace spaceless
