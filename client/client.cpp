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
	network_conn->send_protobuf(REQ_REGISTER_USER, request);
}


void UserManager::login_user(int uid, const std::string& password)
{
	protocol::ReqLoginUser request;
	request.set_uid(uid);
	request.set_password(password);
	network_conn->send_protobuf(REQ_LOGIN_USER, request);
}


void UserManager::remove_user(int uid)
{
	protocol::ReqRemoveUser request;
	request.set_uid(uid);
	network_conn->send_protobuf(REQ_REMOVE_USER, request);
}



} // namespace client
} // namespace spaceless
