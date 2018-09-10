/*
 * core.cpp
 * @author wherewindblow
 * @date   Nov 20, 2017
 */


#include "core.h"

#include <cmath>
#include <common/transaction.h>
#include <protocol/all.h>


namespace spaceless {
namespace client {

int conn_id = 0;


void UserManager::register_user(const std::string& username, const std::string& password)
{
	protocol::ReqRegisterUser request;
	request.set_username(username);
	request.set_password(password);
	Network::send_protobuf(conn_id, request);
}


void UserManager::login_user(int user_id, const std::string& password)
{
	protocol::ReqLoginUser request;
	request.set_user_id(user_id);
	request.set_password(password);
	Network::send_protobuf(conn_id, request);
}


void UserManager::remove_user(int user_id)
{
	protocol::ReqRemoveUser request;
	request.set_user_id(user_id);
	Network::send_protobuf(conn_id, request);
}


void UserManager::find_user(int user_id)
{
	protocol::ReqFindUser request;
	request.set_user_id(user_id);
	Network::send_protobuf(conn_id, request);
}


void UserManager::find_user(const std::string& username)
{
	protocol::ReqFindUser request;
	request.set_username(username);
	Network::send_protobuf(conn_id, request);
}


void SharingGroupManager::register_group(const std::string& group_name)
{
	protocol::ReqRegisterGroup request;
	request.set_group_name(group_name);
	Network::send_protobuf(conn_id, request);
}


void SharingGroupManager::remove_group(int group_id)
{
	protocol::ReqRemoveGroup request;
	request.set_group_id(group_id);
	Network::send_protobuf(conn_id, request);
}


void SharingGroupManager::find_group(int group_id)
{
	protocol::ReqFindGroup request;
	request.set_group_id(group_id);
	Network::send_protobuf(conn_id, request);
}


void SharingGroupManager::find_group(const std::string& group_name)
{
	protocol::ReqFindGroup request;
	request.set_group_name(group_name);
	Network::send_protobuf(conn_id, request);
}


void SharingGroupManager::join_group(int group_id)
{
	protocol::ReqJoinGroup request;
	request.set_group_id(group_id);
	Network::send_protobuf(conn_id, request);
}


void SharingGroupManager::assign_as_manager(int group_id, int user_id)
{
	protocol::ReqAssignAsManager request;
	request.set_group_id(group_id);
	request.set_user_id(user_id);
	Network::send_protobuf(conn_id, request);
}


void SharingGroupManager::assign_as_memeber(int group_id, int user_id)
{
	protocol::ReqAssignAsMemeber request;
	request.set_group_id(group_id);
	request.set_user_id(user_id);
	Network::send_protobuf(conn_id, request);
}


void SharingGroupManager::kick_out_user(int group_id, int user_id)
{
	protocol::ReqKickOutUser request;
	request.set_group_id(group_id);
	request.set_user_id(user_id);
	Network::send_protobuf(conn_id, request);
}


void SharingGroupManager::put_file(int group_id,
								   const std::string& local_file_path,
								   const std::string& remote_file_path,
								   int /*fragment_index*/)
{
	m_putting_file_session.local_file_path = local_file_path;
	m_putting_file_session.group_id = group_id;
	m_putting_file_session.remote_file_path = remote_file_path;
	m_putting_file_session.start_time = lights::current_precise_time();

	lights::FileStream file(local_file_path, "r");
	float file_size = static_cast<float>(file.size());
	int max_fragment = static_cast<int>(std::ceil(file_size / protocol::MAX_FRAGMENT_CONTENT_LEN));
	m_putting_file_session.max_fragment = max_fragment;

	for (int fragment_index = 0; fragment_index < m_putting_file_session.max_fragment; ++fragment_index)
	{
		protocol::ReqPutFile request;
		request.set_group_id(group_id);
		request.set_file_path(remote_file_path);
		request.mutable_fragment()->set_fragment_index(fragment_index);
		request.mutable_fragment()->set_max_fragment(m_putting_file_session.max_fragment);

		char content[protocol::MAX_FRAGMENT_CONTENT_LEN];
		file.seek(fragment_index * protocol::MAX_FRAGMENT_CONTENT_LEN, lights::FileSeekWhence::BEGIN);
		std::size_t content_len = file.read({content, protocol::MAX_FRAGMENT_CONTENT_LEN});
		request.mutable_fragment()->set_fragment_content(content, content_len);
		Network::send_protobuf(conn_id, request);
		m_putting_file_session.fragment_state[fragment_index] = true;
	}
}


void SharingGroupManager::get_file(int group_id, const std::string& remote_file_path, const std::string& local_file_path)
{
	m_getting_file_session.local_file_path = local_file_path;
	m_getting_file_session.group_id = group_id;
	m_getting_file_session.remote_file_path = remote_file_path;
	protocol::ReqGetFile request;
	request.set_group_id(group_id);
	request.set_file_path(remote_file_path);
	Network::send_protobuf(conn_id, request);
}


void SharingGroupManager::create_path(int group_id, const std::string& path)
{
	protocol::ReqCreatePath request;
	request.set_group_id(group_id);
	request.set_path(path);
	Network::send_protobuf(conn_id, request);
}


void SharingGroupManager::remove_path(int group_id, const std::string& path, bool force_remove_all)
{
	protocol::ReqRemovePath request;
	request.set_group_id(group_id);
	request.set_path(path);
	request.set_force_remove_all(force_remove_all);
	Network::send_protobuf(conn_id, request);
}


FileTransferSession& SharingGroupManager::putting_file_session()
{
	return m_putting_file_session;
}

FileTransferSession& SharingGroupManager::getting_file_session()
{
	return m_getting_file_session;
}

} // namespace client
} // namespace spaceless
