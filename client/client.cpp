/*
 * client.cpp
 * @author wherewindblow
 * @date   Nov 20, 2017
 */

#include "client.h"
#include "protocol/all.h"


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


void UserManager::login_user(int uid, const std::string& password)
{
	protocol::ReqLoginUser request;
	request.set_uid(uid);
	request.set_password(password);
	network_conn->send_protobuf(protocol::REQ_LOGIN_USER, request);
}


void UserManager::remove_user(int uid)
{
	protocol::ReqRemoveUser request;
	request.set_uid(uid);
	network_conn->send_protobuf(protocol::REQ_REMOVE_USER, request);
}


void UserManager::find_user(int uid)
{
	protocol::ReqFindUser request;
	request.set_uid(uid);
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


void SharingGroupManager::kick_out_user(int group_id, int uid)
{
	protocol::ReqKickOutUser request;
	request.set_group_id(group_id);
	request.set_uid(uid);
	network_conn->send_protobuf(protocol::REQ_KICK_OUT_USER, request);
}

} // namespace client
} // namespace spaceless
