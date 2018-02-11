/*
 * core.cpp
 * @author wherewindblow
 * @date   Nov 20, 2017
 */


#include "core.h"

#include <cmath>
#include <protocol/all.h>



namespace spaceless {
namespace client {

NetworkConnection* network_conn = nullptr;


void UserManager::register_user(const std::string& username, const std::string& password)
{
	protocol::ReqRegisterUser request;
	request.set_username(username);
	request.set_password(password);
	network_conn->send_protobuf(protocol::REQ_REGISTER_USER, request);
}


void UserManager::login_user(int user_id, const std::string& password)
{
	protocol::ReqLoginUser request;
	request.set_user_id(user_id);
	request.set_password(password);
	network_conn->send_protobuf(protocol::REQ_LOGIN_USER, request);
}


void UserManager::remove_user(int user_id)
{
	protocol::ReqRemoveUser request;
	request.set_user_id(user_id);
	network_conn->send_protobuf(protocol::REQ_REMOVE_USER, request);
}


void UserManager::find_user(int user_id)
{
	protocol::ReqFindUser request;
	request.set_user_id(user_id);
	network_conn->send_protobuf(protocol::REQ_FIND_USER, request);
}


void UserManager::find_user(const std::string& username)
{
	protocol::ReqFindUser request;
	request.set_username(username);
	network_conn->send_protobuf(protocol::REQ_FIND_USER, request);
}


void SharingGroupManager::register_group(const std::string& group_name)
{
	protocol::ReqRegisterGroup request;
	request.set_group_name(group_name);
	network_conn->send_protobuf(protocol::REQ_REGISTER_GROUP, request);
}


void SharingGroupManager::remove_group(int group_id)
{
	protocol::ReqRemoveGroup request;
	request.set_group_id(group_id);
	network_conn->send_protobuf(protocol::REQ_REMOVE_GROUP, request);
}


void SharingGroupManager::find_group(int group_id)
{
	protocol::ReqFindGroup request;
	request.set_group_id(group_id);
	network_conn->send_protobuf(protocol::REQ_FIND_GROUP, request);
}


void SharingGroupManager::find_group(const std::string& group_name)
{
	protocol::ReqFindGroup request;
	request.set_group_name(group_name);
	network_conn->send_protobuf(protocol::REQ_FIND_GROUP, request);
}


void SharingGroupManager::join_group(int group_id)
{
	protocol::ReqJoinGroup request;
	request.set_group_id(group_id);
	network_conn->send_protobuf(protocol::REQ_JOIN_GROUP, request);
}


void SharingGroupManager::kick_out_user(int group_id, int user_id)
{
	protocol::ReqKickOutUser request;
	request.set_group_id(group_id);
	request.set_user_id(user_id);
	network_conn->send_protobuf(protocol::REQ_KICK_OUT_USER, request);
}


void SharingGroupManager::put_file(int group_id,
							  const std::string& local_filename,
							  const std::string& remote_filename,
							  int fragment_index)
{
	if (fragment_index == 0)
	{
		m_putting_file_session.local_filename = local_filename;
		m_putting_file_session.group_id = group_id;
		m_putting_file_session.remote_filename = remote_filename;
	}

	protocol::ReqPutFile request;
	request.set_group_id(group_id);
	request.set_filename(remote_filename);
	request.mutable_fragment()->set_fragment_index(fragment_index);
	m_putting_file_session.fragment_index = fragment_index;

	lights::FileStream file(local_filename, "r");
	float file_size = static_cast<float>(file.size());
	int max_fragment = static_cast<int>(std::ceil(file_size / protocol::MAX_FRAGMENT_CONTENT_LEN));
	m_putting_file_session.max_fragment = max_fragment;
	request.mutable_fragment()->set_max_fragment(max_fragment);

	char content[protocol::MAX_FRAGMENT_CONTENT_LEN];
	file.seek(fragment_index * protocol::MAX_FRAGMENT_CONTENT_LEN, lights::FileSeekWhence::BEGIN);
	std::size_t content_len = file.read({content, protocol::MAX_FRAGMENT_CONTENT_LEN});
	request.mutable_fragment()->set_fragment_content(content, content_len);
	network_conn->send_protobuf(protocol::REQ_PUT_FILE, request);
}


void SharingGroupManager::get_file(int group_id, const std::string& remote_filename, const std::string& local_filename)
{
	m_getting_file_session.local_filename = local_filename;
	m_getting_file_session.group_id = group_id;
	m_getting_file_session.remote_filename = remote_filename;
	protocol::ReqGetFile request;
	request.set_group_id(group_id);
	request.set_filename(remote_filename);
	network_conn->send_protobuf(protocol::REQ_GET_FILE, request);
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
