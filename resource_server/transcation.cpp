/**
 * transcation.cpp
 * @author wherewindblow
 * @date   Dec 24, 2017
 */

#include "transcation.h"
#include "resource_server.h"

#include <protocol/all.h>


namespace spaceless {
namespace resource_server {
namespace transcation {

void convert_user(const User& server_user, protocol::User& proto_user)
{
	proto_user.set_uid(server_user.uid);
	proto_user.set_username(server_user.username);
	for (auto& group_id : server_user.group_list)
	{
		*proto_user.mutable_group_list()->Add() = group_id;
	}
}


void on_register_user(NetworkConnection& conn, const PackageBuffer& package)
{
	protocol::ReqRegisterUser request;
	package.parse_as_protobuf(request);
	protocol::RspRegisterUser rsponse;

	try
	{
		User& user = UserManager::instance()->register_user(request.username(), request.password());
		convert_user(user, *rsponse.mutable_user());
	}
	catch (const Exception& ex)
	{
		rsponse.set_result(ex.code());
		SPACELESS_ERROR(MODULE_RESOURCE_SERVER, "Connection {}: {}", conn.connection_id(), ex);
	}
	conn.send_protobuf(protocol::RSP_REGISTER_USER, rsponse);
}


void on_login_user(NetworkConnection& conn, const PackageBuffer& package)
{
	protocol::ReqLoginUser request;
	package.parse_as_protobuf(request);
	protocol::RspLoginUser rsponse;
	bool pass = UserManager::instance()->login_user(request.uid(), request.password(), conn);
	rsponse.set_result(pass ? 0 : -1);
	conn.send_protobuf(protocol::RSP_LOGIN_USER, rsponse);
}


void on_remove_user(NetworkConnection& conn, const PackageBuffer& package)
{
	protocol::ReqRemoveUser request;
	package.parse_as_protobuf(request);
	protocol::RspRemoveUser rsponse;
	UserManager::instance()->remove_user(request.uid());
	rsponse.set_result(0);
	conn.send_protobuf(protocol::RSP_REMOVE_USER, rsponse);
}


void on_find_user(NetworkConnection& conn, const PackageBuffer& package)
{
	protocol::ReqFindUser request;
	package.parse_as_protobuf(request);
	protocol::RspFindUser rsponse;

	User* user = nullptr;
	if (request.uid())
	{
		user = UserManager::instance()->find_user(request.uid());
	}
	else
	{
		user = UserManager::instance()->find_user(request.username());
	}

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


void convert_group(const SharingGroup& server_group, protocol::SharingGroup& rsponse_group)
{
	rsponse_group.set_group_id(server_group.group_id());
	rsponse_group.set_group_name(server_group.group_name());
	rsponse_group.set_owner_id(server_group.owner_id());
	for (std::size_t i = 0; i < server_group.manager_list().size(); ++i)
	{
		*rsponse_group.mutable_manager_list()->Add() = server_group.manager_list()[i];
	}
	for (std::size_t i = 0; i < server_group.member_list().size(); ++i)
	{
		*rsponse_group.mutable_member_list()->Add() = server_group.member_list()[i];
	}
}


void on_register_group(NetworkConnection& conn, const PackageBuffer& package)
{
	protocol::ReqRegisterGroup request;
	package.parse_as_protobuf(request);
	protocol::RspRegisterGroup rsponse;

	User* user = static_cast<User*>(conn.get_attachment());
	if (user == nullptr)
	{
		rsponse.set_result(-1);
		goto send_back_msg;
	}

	try
	{
		SharingGroup& group = SharingGroupManager::instance()->register_group(user->uid, request.group_name());
		rsponse.set_group_id(group.group_id());
	}
	catch (Exception& e)
	{
		rsponse.set_result(e.code());
	}

	send_back_msg:
	conn.send_protobuf(protocol::RSP_REGISTER_GROUP, rsponse);
}


void on_remove_group(NetworkConnection& conn, const PackageBuffer& package)
{
	protocol::ReqRemoveGroup request;
	package.parse_as_protobuf(request);
	protocol::RspRemoveGroup rsponse;

	User* user = static_cast<User*>(conn.get_attachment());
	if (user == nullptr)
	{
		rsponse.set_result(-1);
		goto send_back_msg;
	}

	SharingGroupManager::instance()->remove_group(user->uid, request.group_id());

	send_back_msg:
	conn.send_protobuf(protocol::RSP_REMOVE_GROUP, rsponse);
}


void on_find_group(NetworkConnection& conn, const PackageBuffer& package)
{
	protocol::ReqFindGroup request;
	package.parse_as_protobuf(request);
	protocol::RspFindGroup rsponse;

	{
		User* user = static_cast<User*>(conn.get_attachment());
		if (user == nullptr)
		{
			rsponse.set_result(-1);
			goto send_back_msg;
		}

		SharingGroup* group = nullptr;
		if (request.group_id())
		{
			group = SharingGroupManager::instance()->find_group(request.group_id());
		}
		else
		{
			group = SharingGroupManager::instance()->find_group(request.group_name());
		}

		if (group)
		{
			convert_group(*group, *rsponse.mutable_group());
		}
		else
		{
			rsponse.set_result(-1);
		}
	}

	send_back_msg:
	conn.send_protobuf(protocol::RSP_FIND_GROUP, rsponse);
}


void on_join_group(NetworkConnection& conn, const PackageBuffer& package)
{
	protocol::ReqJoinGroup request;
	package.parse_as_protobuf(request);
	protocol::RspJoinGroup rsponse;

	User* user = static_cast<User*>(conn.get_attachment());
	if (user == nullptr)
	{
		rsponse.set_result(-1);
		goto send_back_msg;
	}

	try
	{
		SharingGroup& group = SharingGroupManager::instance()->get_group(request.group_id());
		group.join_group(user->uid);
	}
	catch (Exception& ex)
	{
		rsponse.set_result(ex.code());
	}

	send_back_msg:
	conn.send_protobuf(protocol::RSP_JOIN_GROUP, rsponse);
}


void on_kick_out_user(NetworkConnection& conn, const PackageBuffer& package)
{
	protocol::ReqKickOutUser request;
	package.parse_as_protobuf(request);
	protocol::RspKickOutUser rsponse;

	User* user = static_cast<User*>(conn.get_attachment());
	if (user == nullptr)
	{
		rsponse.set_result(-1);
		goto send_back_msg;
	}

	try
	{
		SharingGroup& group = SharingGroupManager::instance()->get_group(request.group_id());
		if (request.uid() != user->uid) // Only manager can kick out other user.
		{
			auto itr = std::find(group.manager_list().begin(), group.manager_list().end(), user->uid);
			if (itr == group.manager_list().end())
			{
				rsponse.set_result(-1);
				goto send_back_msg;
			}
		}
		group.kick_out(request.uid());
	}
	catch (Exception& ex)
	{
		rsponse.set_result(ex.code());
	}

	send_back_msg:
	conn.send_protobuf(protocol::RSP_KICK_OUT_USER, rsponse);
}

} // namespace transcation
} // namespace resource_server
} // namespace spaceless
