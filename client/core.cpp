/*
 * core.cpp
 * @author wherewindblow
 * @date   Nov 20, 2017
 */


#include "core.h"

#include <cmath>
#include <fstream>

#include <lights/file.h>

#include <foundation/transaction.h>
#include <foundation/worker.h>
#include <protocol/all.h>


namespace spaceless {
namespace client {

int conn_id = 0;

const int DELAY_TESTING_PER_SEC = 60;

const std::string META_FILE_PREFIX = ".meta";


void UserManager::register_user(const std::string& username, const std::string& password)
{
	protocol::ReqRegisterUser request;
	request.set_username(username);
	request.set_password(password);
	Network::send_protocol(conn_id, request);
}


void UserManager::login_user(int user_id, const std::string& password)
{
	protocol::ReqLoginUser request;
	request.set_user_id(user_id);
	request.set_password(password);
	Network::send_protocol(conn_id, request);
}


void UserManager::remove_user(int user_id)
{
	protocol::ReqRemoveUser request;
	request.set_user_id(user_id);
	Network::send_protocol(conn_id, request);
}


void UserManager::find_user(int user_id)
{
	protocol::ReqFindUser request;
	request.set_user_id(user_id);
	Network::send_protocol(conn_id, request);
}


void UserManager::find_user(const std::string& username)
{
	protocol::ReqFindUser request;
	request.set_username(username);
	Network::send_protocol(conn_id, request);
}


void SharingGroupManager::register_group(const std::string& group_name)
{
	protocol::ReqRegisterGroup request;
	request.set_group_name(group_name);
	Network::send_protocol(conn_id, request);
}


void SharingGroupManager::remove_group(int group_id)
{
	protocol::ReqRemoveGroup request;
	request.set_group_id(group_id);
	Network::send_protocol(conn_id, request);
}


void SharingGroupManager::find_group(int group_id)
{
	protocol::ReqFindGroup request;
	request.set_group_id(group_id);
	Network::send_protocol(conn_id, request);
}


void SharingGroupManager::find_group(const std::string& group_name)
{
	protocol::ReqFindGroup request;
	request.set_group_name(group_name);
	Network::send_protocol(conn_id, request);
}


void SharingGroupManager::join_group(int group_id)
{
	protocol::ReqJoinGroup request;
	request.set_group_id(group_id);
	Network::send_protocol(conn_id, request);
}


void SharingGroupManager::assign_as_manager(int group_id, int user_id)
{
	protocol::ReqAssignAsManager request;
	request.set_group_id(group_id);
	request.set_user_id(user_id);
	Network::send_protocol(conn_id, request);
}


void SharingGroupManager::assign_as_member(int group_id, int user_id)
{
	protocol::ReqAssignAsMember request;
	request.set_group_id(group_id);
	request.set_user_id(user_id);
	Network::send_protocol(conn_id, request);
}


void SharingGroupManager::kick_out_user(int group_id, int user_id)
{
	protocol::ReqKickOutUser request;
	request.set_group_id(group_id);
	request.set_user_id(user_id);
	Network::send_protocol(conn_id, request);
}


void SharingFileManager::list_file(int group_id, const std::string& file_path)
{
	protocol::ReqListFile request;
	request.set_group_id(group_id);
	request.set_file_path(file_path);
	Network::send_protocol(conn_id, request);
}


void SharingFileManager::put_file(int group_id, const std::string& local_path, const std::string& remote_path)
{
	m_put_session.local_path = local_path;
	m_put_session.group_id = group_id;
	m_put_session.remote_path = remote_path;
	m_put_session.start_time = lights::current_precise_time();

	lights::FileStream file(local_path, "r");
	float file_size = static_cast<float>(file.size());
	int max_fragment = static_cast<int>(std::ceil(file_size / protocol::MAX_FRAGMENT_CONTENT_LEN));
	m_put_session.max_fragment = max_fragment;

	protocol::ReqPutFileSession request;
	request.set_group_id(group_id);
	request.set_file_path(remote_path);
	request.set_max_fragment(m_put_session.max_fragment);
	Network::send_protocol(conn_id, request);
}


void SharingFileManager::start_put_file(int next_fragment)
{
	lights::FileStream file(m_put_session.local_path, "r");
	for (int fragment_index = next_fragment; fragment_index < m_put_session.max_fragment; ++fragment_index)
	{
		protocol::ReqPutFile request;
		request.set_session_id(m_put_session.session_id);
		request.set_fragment_index(fragment_index);

		char content[protocol::MAX_FRAGMENT_CONTENT_LEN];
		file.seek(fragment_index * protocol::MAX_FRAGMENT_CONTENT_LEN, lights::FileSeekWhence::BEGIN);
		std::size_t content_len = file.read({content, protocol::MAX_FRAGMENT_CONTENT_LEN});
		request.set_fragment_content(content, content_len);
		Network::send_protocol(conn_id, request);
		m_put_session.fragment_state[fragment_index] = true;
	}
}


void SharingFileManager::get_file(int group_id, const std::string& remote_path, const std::string& local_path)
{
	m_get_session.local_path = local_path;
	m_get_session.group_id = group_id;
	m_get_session.remote_path = remote_path;
	m_get_session.start_time = lights::current_precise_time();

	protocol::ReqGetFileSession request;
	request.set_group_id(group_id);
	request.set_file_path(remote_path);
	Network::send_protocol(conn_id, request);
}


void SharingFileManager::start_get_file()
{
	int next_fragment = get_next_fragment(m_get_session.local_path);
	for (int fragment_index = next_fragment; fragment_index < m_get_session.max_fragment; ++fragment_index)
	{
		protocol::ReqGetFile request;
		request.set_session_id(m_get_session.session_id);
		request.set_fragment_index(fragment_index);
		Network::send_protocol(conn_id, request);
	}
}


int SharingFileManager::get_next_fragment(const std::string& local_path)
{
	std::string meta_filename = m_get_session.local_path + META_FILE_PREFIX;
	int next_fragment = 0;
	std::ifstream meta_file(meta_filename);
	while (meta_file)
	{
		meta_file >> next_fragment;
	}
	return next_fragment;
}


void SharingFileManager::set_next_fragment(const std::string& local_path, int next_fragment)
{
	std::string meta_filename = m_get_session.local_path + META_FILE_PREFIX;
	lights::FileStream meta_file(meta_filename, "a");
	meta_file << lights::format("{}\n", next_fragment);
}


void SharingFileManager::create_path(int group_id, const std::string& path)
{
	protocol::ReqCreatePath request;
	request.set_group_id(group_id);
	request.set_path(path);
	Network::send_protocol(conn_id, request);
}


void SharingFileManager::remove_path(int group_id, const std::string& path, bool force_remove_all)
{
	protocol::ReqRemovePath request;
	request.set_group_id(group_id);
	request.set_path(path);
	request.set_force_remove_all(force_remove_all);
	Network::send_protocol(conn_id, request);
}


FileSession& SharingFileManager::put_file_session()
{
	return m_put_session;
}


FileSession& SharingFileManager::get_file_session()
{
	return m_get_session;
}


void DelayTesting::start_testing()
{
	TimerManager::instance()->register_frequent_timer("start_testing", lights::PreciseTime(DELAY_TESTING_PER_SEC), []()
	{
		protocol::ReqPing request;
		lights::PreciseTime time = lights::current_precise_time();
		request.set_second(static_cast<std::int32_t>(time.seconds));
		std::int64_t microsecond = lights::nanosecond_to_microsecond(time.nanoseconds);
		request.set_microsecond(static_cast<std::int32_t>(microsecond));
		Network::send_protocol(conn_id, request);
	});
}


void DelayTesting::on_receive_response(int second, int microsecond)
{
	lights::PreciseTime send_time(second, lights::microsecond_to_nanosecond(microsecond));
	lights::PreciseTime rtt = lights::current_precise_time() - send_time;
	m_last_delay_time = rtt / 2;
	m_total_delay_time = m_total_delay_time + m_last_delay_time;
	++m_test_times;
}


lights::PreciseTime DelayTesting::last_delay_time()
{
	return m_last_delay_time;
}


lights::PreciseTime DelayTesting::average_delay_time()
{
	if (m_test_times == 0)
	{
		return lights::PreciseTime();
	}
	else
	{
		return m_total_delay_time / m_test_times;
	}
}

} // namespace client
} // namespace spaceless
