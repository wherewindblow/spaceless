/*
 * client.cpp
 * @author wherewindblow
 * @date   Nov 20, 2017
 */

#include "client.h"
#include "protocol/all.h"


namespace spaceless {
namespace client {

NetworkConnection* conn = nullptr;


void UserManager::register_user(lights::StringView username, lights::StringView password)
{
	protocol::ReqRegisterUser request;
	request.set_username(username.to_std_string());
	request.set_password(password.to_std_string());
	conn->send_protobuf(REQ_REGISTER_USER, request);
}


void UserManager::login_user(int uid, lights::StringView password)
{
	protocol::ReqLoginUser request;
	request.set_uid(uid);
	request.set_password(password.to_std_string());
	conn->send_protobuf(REQ_LOGIN_USER, request);
}


void UserManager::remove_user(int uid)
{
	protocol::ReqRemoveUser request;
	request.set_uid(uid);
	conn->send_protobuf(REQ_REMOVE_USER, request);
}



} // namespace client
} // namespace spaceless
