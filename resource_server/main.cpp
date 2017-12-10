#include <iostream>

#include <lights/ostream.h>
#include <lights/file.h>
#include <protocol/all.h>

#include "resource_server.h"
#include "common/network.h"
#include "common/log.h"
#include "protocol/protocol.pb.h"


namespace spaceless {

const int MODULE_RESOURCE_SERVER = 1000;

namespace resource_server {
namespace transcation {

void convert_user(User& server_user, protocol::User& proto_user)
{
	proto_user.set_uid(server_user.uid);
	proto_user.set_username(server_user.username);
	for (auto& group_id : server_user.group_list)
	{
		*proto_user.mutable_group_list()->Add() = group_id;
	}
}

void on_register_user(NetworkConnection& conn, const ProtocolBuffer& package)
{
	protocol::ReqRegisterUser request;
	package.parse_as_protobuf(request);
	try
	{
		User& user = UserManager::instance()->register_user(request.username(), request.password());
		protocol::RspRegisterUser rsponse;
		convert_user(user, *rsponse.mutable_user());
		conn.send_protobuf(protocol::RSP_REGISTER_USER, rsponse);
	}
	catch (const Exception& ex)
	{
		protocol::RspRegisterUser rsponse;
		rsponse.set_result(ex.code());
		conn.send_protobuf(protocol::RSP_REGISTER_USER, rsponse);
		SPACELESS_ERROR(MODULE_RESOURCE_SERVER, "Rmote address {}. {}",
						conn.stream_socket().peerAddress().toString(), ex);
	}
}


void on_login_user(NetworkConnection& conn, const ProtocolBuffer& package)
{
	protocol::ReqLoginUser request;
	package.parse_as_protobuf(request);
	bool pass = UserManager::instance()->login_user(request.uid(), request.password(), conn);
	protocol::RspLoginUser rsponse;
	rsponse.set_result(pass);
	conn.send_protobuf(protocol::RSP_LOGIN_USER, rsponse);
}


void on_remove_user(NetworkConnection& conn, const ProtocolBuffer& package)
{
	protocol::ReqRemoveUser request;
	package.parse_as_protobuf(request);
	UserManager::instance()->remove_user(request.uid());
	protocol::RspRemoveUser rsponse;
	rsponse.set_result(true);
	conn.send_protobuf(protocol::RSP_LOGIN_USER, rsponse);
}


void on_find_user(NetworkConnection& conn, const ProtocolBuffer& package)
{
	protocol::ReqFindUser request;
	package.parse_as_protobuf(request);

	User* user = nullptr;
	if (request.uid())
	{
		user = UserManager::instance()->find_user(request.uid());
	}
	else
	{
		user = UserManager::instance()->find_user(request.username());
	}

	protocol::RspFindUser rsponse;
	if (user)
	{
		convert_user(*user, *rsponse.mutable_user());
	}
	else
	{
		rsponse.set_result(-1);
	}
	conn.send_protobuf(protocol::RSP_FIND_USER, rsponse);
}

} // namespace transcation


int main(int argc, const char* argv[])
{
	for (protocol::CommandType type = protocol::CommandType_MIN;
		 type <= protocol::CommandType_MAX;
		 type = static_cast<protocol::CommandType>(static_cast<int>(type) + 1))
	{
		std::cout << protocol::CommandType_Name(type) << ":" << type << std::endl;
	}
	try
	{
		NetworkConnectionManager::instance()->register_listener("127.0.0.1", 10240);

		std::pair<int, CommandHandlerManager::CommandHandler> handlers[] = {
			{protocol::REQ_REGISTER_USER, transcation::on_register_user},
			{protocol::REQ_LOGIN_USER, transcation::on_login_user},
			{protocol::REQ_REMOVE_USER, transcation::on_remove_user},
			{protocol::REQ_FIND_USER, transcation::on_find_user},
		};

		for (std::size_t i = 0; i < lights::size_of_array(handlers); ++i)
		{
			CommandHandlerManager::instance()->register_command(handlers[i].first, handlers[i].second);
		}

		NetworkConnectionManager::instance()->run();
	}
	catch (const Exception& ex)
	{
		SPACELESS_ERROR(MODULE_RESOURCE_SERVER, ex);
	}
	catch (const std::exception& ex)
	{
		SPACELESS_ERROR(MODULE_RESOURCE_SERVER, ex.what());
	}

	return 0;
}

} // namespace resource_server
} // namespace spaceless


int main(int argc, const char* argv[])
{
	return spaceless::resource_server::main(argc, argv);
}
