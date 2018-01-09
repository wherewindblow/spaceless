/**
 * core.h
 * @author wherewindblow
 * @date   Nov 20, 2017
 */

#pragma once

#include <vector>
#include <map>
#include <lights/sequence.h>

#include "common/network.h"
#include "common/basics.h"


namespace spaceless {
namespace client {

extern NetworkConnection* network_conn;

struct User
{
	int uid;
	std::string username;
	std::vector<int> group_list;
};


class UserManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(UserManager);

	void register_user(const std::string& username, const std::string& password);

	void login_user(int uid, const std::string& password);

	void remove_user(int uid);

	void find_user(int uid);

	void find_user(const std::string& username);

private:
	using UserList = std::map<int, User>;
	UserList m_user_list;
	int m_next_id = 1;
};


class SharingGroup
{
	int m_group_id;
	std::string m_group_name;
	int m_owner_id;
	int m_root_dir_id;
	std::vector<int> m_manager_list;
	std::vector<int> m_member_list;
};

struct FileTransferSession
{
	std::string local_filename;
	int group_id;
	std::string remote_filename;
	int max_fragment_index;
	int process_fragment_index;
};

class SharingGroupManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(SharingGroupManager);

	void register_group(const std::string& group_name);

	void remove_group(int group_id);

	void find_group(int group_id);

	void find_group(const std::string& group_name);

	void join_group(int group_id);

	void kick_out_user(int group_id, int uid);

	void put_file(int group_id,
			 const std::string& local_filename,
			 const std::string& remote_filename,
			 int fragment_index = 0);

	void get_file(int group_id, const std::string& remote_filename, const std::string& local_filename);

	FileTransferSession& putting_file_session();

	FileTransferSession& getting_file_session();

private:
	using GroupList = std::map<int, SharingGroup>;
	GroupList m_group_list;
	int m_next_id = 1;
	FileTransferSession m_putting_file_session;
	FileTransferSession m_getting_file_session;
};


} // namespace client
} // namespace spaceless

