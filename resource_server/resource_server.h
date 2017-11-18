/**
 * resource_server.h
 * @author wherewindblow
 * @date   Oct 26, 2017
 */

#pragma once

#include <vector>
#include <set>
#include <map>
#include <lights/sequence.h>
#include <lights/exception.h>

#include "common/basics.h"


namespace spaceless {

enum ErrorTpye
{
	ERR_USER_ALREADY_EXIST = 1000,
	ERR_USER_CANNOT_REGISTER = 1001,
	ERR_USER_NOT_EXIST = 1002,
	ERR_GROUP_ALREADY_EXIST = 1100,
	ERR_GROUP_CANNOT_REGISTER = 1101,
	ERR_GROUP_NOT_EXIST = 1102,
	ERR_GROUP_ONLY_ONWER_CAN_REMOVE = 1103,
	ERR_GROUP_ONLY_MANAGER_CAN_CREATE_DIR = 1104,
	ERR_GROUP_CREATE_DIR_MUST_UNDER_DIR = 1105,
	ERR_GROUP_ONLY_MANAGER_CAN_REMOVE_DIR = 1106,
	ERR_GROUP_REMOVE_DIR_MUST_UNDER_DIR = 1107,
	ERR_GROUP_CANNOT_REMOVE_ROOT_DIR = 1108,
	ERR_GROUP_CANNOT_KICK_OUT_OWNER = 1110,

	ERR_FILE_ALREADY_EXIST = 1200,
	ERR_FILE_CANNOT_CREATE = 1201,
	ERR_FILE_NOT_EXIST = 1202,
	ERR_NODE_ALREADY_EXIST = 1300,
	ERR_NODE_CANNOT_CREATE = 1301,
	ERR_NODE_NOT_EXIST = 1302,
};


class Exception: public lights::Exception
{
public:
	Exception(const lights::SourceLocation& occur_location, ErrorTpye error_tpye):
		lights::Exception(occur_location, error_tpye)
	{}
};


struct User
{
	int uid;
	std::string name;
	std::string password;
	std::vector<int> group_list;
};


inline bool operator== (const User& lhs, const User& rhs)
{
	return lhs.uid == rhs.uid;
}


class UserManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(UserManager);

	User& register_user(lights::StringView username, lights::StringView password);

	void remove_user(int uid);

	User* find_user(int uid);

	User* find_user(lights::StringView username);

	User& get_user(int uid);

	User& get_user(lights::StringView username);

private:
	using UserList = std::map<int, User>;
	UserList m_user_list;
	int m_next_id = 1;
};


class SharingGroup
{
public:
	SharingGroup(int group_id, lights::StringView group_name, int ower_id, int root_dir_id);

	int group_id() const;

	lights::StringView group_name() const;

	int owner_id() const;

	void put_file(int uid, lights::StringView target_name, lights::SequenceView file_content);

	void get_file(int uid, lights::StringView target_name, lights::Sequence file_content);

	/**
	 * Create all directory by directory path. If a parent directory is not create will automatically create.
	 * @param uid  User id.
	 * @param full_dir_path Full path of directory. Like "this/is/a/path".
	 */
	void create_directory(int uid, lights::StringView full_dir_path);

	/**
	 * Remove last directory of directory path.
	 * @param uid User id.
	 * @param full_dir_path Full path of directory. Like "this/is/a/path".
	 */
	void remove_directory(int uid, lights::StringView full_dir_path);

	void join_group(int uid);

	void kick_out(int uid);

private:
	int m_group_id;
	std::string m_group_name;
	int m_owner_id;
	int m_root_dir_id;
	std::vector<int> m_manager_list;
	std::vector<int> m_member_list;
};


inline bool operator== (const SharingGroup& lhs, const SharingGroup& rhs)
{
	return lhs.group_id() == rhs.group_id();
}


class SharingGroupManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(SharingGroupManager);

	SharingGroup& register_group(int uid, lights::StringView group_name);

	void remove_group(int uid, int group_id);

	SharingGroup* find_group(int group_id);

	SharingGroup* find_group(lights::StringView group_name);

	SharingGroup& get_group(int group_id);

	SharingGroup& get_group(lights::StringView group_name);

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
							   lights::StringView file_name,
							   int node_id = 0,
							   lights::StringView node_file_name = lights::invalid_string_view());

	void remove_file(int file_id);

	SharingFile* find_file(int file_id);

	SharingFile* find_file(int node_id, lights::StringView node_file_name);

	SharingFile& get_file(int file_id);

	SharingFile& get_file(int node_id, lights::StringView node_file_name);

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

	StorageNode& register_node(lights::StringView node_ip, short node_port);

	void remove_node(int node_id);

	StorageNode* find_node(int node_id);

	StorageNode* find_node(lights::StringView node_ip, short node_port);

	StorageNode& get_node(int node_id);

	StorageNode& get_node(lights::StringView node_ip, short node_port);

private:
	using StorageNodeList = std::map<int, StorageNode>;
	StorageNodeList m_node_list;
	int m_next_id = 1;
};

} // namespace spaceless
