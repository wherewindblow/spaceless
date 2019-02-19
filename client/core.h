/**
 * core.h
 * @author wherewindblow
 * @date   Nov 20, 2017
 */

#pragma once

#include <ctime>
#include <vector>
#include <map>

#include <lights/sequence.h>
#include <lights/precise_time.h>
#include <foundation/basics.h>


namespace spaceless {
namespace client {

extern int conn_id;

struct User
{
	User(int user_id, const std::string& username) :
		user_id(user_id),
		username(username),
		group_list()
	{}

	int user_id;
	std::string username;
	std::vector<int> group_list;
};


class UserManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(UserManager);

	void register_user(const std::string& username, const std::string& password);

	void login_user(int user_id, const std::string& password);

	void remove_user(int user_id);

	void find_user(int user_id);

	void find_user(const std::string& username);

private:
	using UserList = std::map<int, User>;
	UserList m_user_list;
	int m_next_id = 1;
};


struct SharingGroup
{
	SharingGroup() = delete; // TODO: How to uses it?

	int m_group_id;
	std::string m_group_name;
	int m_owner_id;
	int m_root_dir_id;
	std::vector<int> m_manager_list;
	std::vector<int> m_member_list;
};


struct FileSession
{
	FileSession() :
		session_id(0),
		local_path(),
		group_id(0),
		remote_path(),
		max_fragment(0),
		fragment_index(0),
		start_time(),
		fragment_state()
	{}

	int session_id;
	std::string local_path;
	int group_id;
	std::string remote_path;
	int max_fragment;
	int fragment_index;
	lights::PreciseTime start_time;
	std::map<int, bool> fragment_state;
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

	void assign_as_manager(int group_id, int user_id);

	void assign_as_member(int group_id, int user_id);

	void kick_out_user(int group_id, int user_id);

private:
	using GroupList = std::map<int, SharingGroup>;
	GroupList m_group_list;
	int m_next_id = 1;
	FileSession m_put_session;
	FileSession m_get_session;
};


class SharingFileManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(SharingFileManager);

	void list_file(int group_id, const std::string& file_path);

	void put_file(int group_id, const std::string& local_path, const std::string& remote_path);

	void start_put_file(int next_fragment);

	void get_file(int group_id, const std::string& remote_path, const std::string& local_path);

	void start_get_file();

	int get_next_fragment(const std::string& local_path);

	void set_next_fragment(const std::string& local_path, int next_fragment);

	void create_path(int group_id, const std::string& directory_path);

	void remove_path(int group_id, const std::string& directory_path, bool force_remove_all);

	FileSession& put_file_session();

	FileSession& get_file_session();

private:
	FileSession m_put_session;
	FileSession m_get_session;
};


class DelayTesting
{
public:
	SPACELESS_SINGLETON_INSTANCE(DelayTesting);

	void start_testing();

	void on_receive_response(int second, int microsecond);

	lights::PreciseTime last_delay_time();

	lights::PreciseTime average_delay_time();

private:
	lights::PreciseTime m_last_delay_time;
	lights::PreciseTime m_total_delay_time;
	int m_test_times;
};


} // namespace client
} // namespace spaceless

