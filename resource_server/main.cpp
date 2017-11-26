#include <iostream>

#include <lights/ostream.h>
#include <lights/file.h>
#include <protocol/all.h>

#include "resource_server.h"
#include "common/network.h"
#include "common/log.h"
#include "protocol/protocol.pb.h"

#include <Poco/Environment.h>


namespace spaceless {

const int MODULE_RESOURCE_SERVER = 1000;

namespace resource_server {


void on_register_user(Connection& conn, const PackageBuffer& package)
{
	protocol::ReqRegisterUser request;
	package.parse_as_protobuf(request);
	try
	{
		User& user = UserManager::instance()->register_user(request.username(), request.password());
		protocol::RspRegisterUser rsponse;
		rsponse.mutable_user()->set_uid(user.uid);
		rsponse.mutable_user()->set_name(user.username);
		for (auto& group_id : user.group_list)
		{
			*rsponse.mutable_user()->mutable_group_list()->Add() = group_id;
		}
		conn.write_protobuf(RSP_REGISTER_USER, rsponse);
	}
	catch (const Exception& ex)
	{
		protocol::RspRegisterUser rsponse;
		rsponse.set_result(ex.code());
		conn.write_protobuf(RSP_REGISTER_USER, rsponse);
		SPACELESS_ERROR(MODULE_RESOURCE_SERVER, "Rmote endpoint {}. {}", conn.remote_endpoint(), ex);
	}
}

void on_login_user(Connection& conn, const PackageBuffer& package)
{
	protocol::ReqLoginUser request;
	package.parse_as_protobuf(request);
	bool pass = UserManager::instance()->login_user(request.uid(), request.password(), conn);
	protocol::RspLoginUser rsponse;
	rsponse.set_result(pass);
	conn.write_protobuf(RSP_LOGIN_USER, rsponse);
}

void on_remove_user(Connection& conn, const PackageBuffer& package)
{
	protocol::ReqRemoveUser request;
	package.parse_as_protobuf(request);
	UserManager::instance()->remove_user(request.uid());
	protocol::RspRemoveUser rsponse;
	rsponse.set_result(true);
	conn.write_protobuf(RSP_LOGIN_USER, rsponse);
}

int main(int argc, const char* argv[])
{
	try
	{
		ConnectionManager::instance()->register_listener("127.0.0.1", 10240);

		std::pair<int, CommandHandlerManager::CommandHandler> handlers[] = {
			{REQ_REGISTER_USER, on_register_user},
			{REQ_LOGIN_USER, on_login_user},
			{REQ_REMOVE_USER, on_remove_user},
		};

		for (std::size_t i = 0; i < lights::size_of_array(handlers); ++i)
		{
			CommandHandlerManager::instance()->register_command(handlers[i].first, handlers[i].second);
		}
		service.run();
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
