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


#define SPACELESS_COMMAND_HANDLER_BEGIN(RequestType, RsponseType) \
	RequestType request; \
	RsponseType rsponse; \
	\
	bool log_error = true; \
	try \
    { \
		package.parse_as_protobuf(request); \

#define SPACELESS_COMMAND_HANDLER_END(rsponse_cmd) \
	} \
	catch (Exception& ex) \
	{ \
		rsponse.set_result(ex.code()); \
		SPACELESS_ERROR(MODULE_RESOURCE_SERVER, "Connection {}: {}", conn.connection_id(), ex); \
		log_error = false; \
	} \
	\
	send_back_msg: \
	conn.send_protobuf(rsponse_cmd, rsponse); \
	if (rsponse.result() && log_error) \
	{ \
		SPACELESS_ERROR(MODULE_RESOURCE_SERVER, "Connection {}: {}", conn.connection_id(), rsponse.result()); \
	} \


#define SPACELESS_COMMAND_HANDLER_USER_BEGIN(RequestType, RsponseType) \
	RequestType request; \
	RsponseType rsponse; \
	\
	bool log_error = true; \
	User* user = static_cast<User*>(conn.get_attachment()); \
	if (user == nullptr) \
	{ \
		rsponse.set_result(-1); \
		goto send_back_msg; \
	} \
	\
	try \
    { \
		package.parse_as_protobuf(request); \


#define SPACELESS_COMMAND_HANDLER_USER_END(rsponse_cmd) \
	} \
	catch (Exception& ex) \
	{ \
		rsponse.set_result(ex.code()); \
		SPACELESS_ERROR(MODULE_RESOURCE_SERVER, "Connection {}, user {}: {}", conn.connection_id(), user->uid, ex); \
		log_error = false; \
	} \
	\
	send_back_msg: \
	conn.send_protobuf(rsponse_cmd, rsponse); \
	if (rsponse.result() && log_error) \
	{ \
		if (user == nullptr) \
		{ \
			SPACELESS_ERROR(MODULE_RESOURCE_SERVER, "Connection {}: {}", conn.connection_id(), rsponse.result()); \
		} \
		else \
		{ \
			SPACELESS_ERROR(MODULE_RESOURCE_SERVER, "Connection {}, user {}: {}", \
				conn.connection_id(), user->uid, rsponse.result()); \
		} \
	} \


void on_register_user(NetworkConnection& conn, const PackageBuffer& package)
{
	SPACELESS_COMMAND_HANDLER_BEGIN(protocol::ReqRegisterUser, protocol::RspRegisterUser);
		User& user = UserManager::instance()->register_user(request.username(), request.password());
		convert_user(user, *rsponse.mutable_user());
	SPACELESS_COMMAND_HANDLER_END(protocol::RSP_REGISTER_USER);
}


void on_login_user(NetworkConnection& conn, const PackageBuffer& package)
{
	SPACELESS_COMMAND_HANDLER_BEGIN(protocol::ReqLoginUser, protocol::RspLoginUser);
		bool pass = UserManager::instance()->login_user(request.uid(), request.password(), conn);
		rsponse.set_result(pass ? 0 : -1);
	SPACELESS_COMMAND_HANDLER_END(protocol::RSP_LOGIN_USER);
}


void on_remove_user(NetworkConnection& conn, const PackageBuffer& package)
{
	SPACELESS_COMMAND_HANDLER_BEGIN(protocol::ReqRemoveUser, protocol::RspRemoveUser);
		UserManager::instance()->remove_user(request.uid());
		rsponse.set_result(0);
	SPACELESS_COMMAND_HANDLER_END(protocol::RSP_REMOVE_USER);
}


void on_find_user(NetworkConnection& conn, const PackageBuffer& package)
{
	SPACELESS_COMMAND_HANDLER_BEGIN(protocol::ReqFindUser, protocol::RspFindUser);
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
	SPACELESS_COMMAND_HANDLER_END(protocol::RSP_FIND_USER);
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
	SPACELESS_COMMAND_HANDLER_USER_BEGIN(protocol::ReqRegisterGroup, protocol::RspRegisterGroup);
		SharingGroup& group = SharingGroupManager::instance()->register_group(user->uid, request.group_name());
		rsponse.set_group_id(group.group_id());
	SPACELESS_COMMAND_HANDLER_USER_END(protocol::RSP_REGISTER_GROUP);
}


void on_remove_group(NetworkConnection& conn, const PackageBuffer& package)
{
	SPACELESS_COMMAND_HANDLER_USER_BEGIN(protocol::ReqRemoveGroup, protocol::RspRemoveGroup);
		SharingGroupManager::instance()->remove_group(user->uid, request.group_id());
	SPACELESS_COMMAND_HANDLER_USER_END(protocol::RSP_REMOVE_GROUP);
}


void on_find_group(NetworkConnection& conn, const PackageBuffer& package)
{
	SPACELESS_COMMAND_HANDLER_USER_BEGIN(protocol::ReqFindGroup, protocol::RspFindGroup);
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
	SPACELESS_COMMAND_HANDLER_USER_END(protocol::RSP_FIND_GROUP);
}


void on_join_group(NetworkConnection& conn, const PackageBuffer& package)
{
	SPACELESS_COMMAND_HANDLER_USER_BEGIN(protocol::ReqJoinGroup, protocol::RspJoinGroup);
		SharingGroup& group = SharingGroupManager::instance()->get_group(request.group_id());
		group.join_group(user->uid);
	SPACELESS_COMMAND_HANDLER_USER_END(protocol::RSP_JOIN_GROUP);
}


void on_kick_out_user(NetworkConnection& conn, const PackageBuffer& package)
{
	SPACELESS_COMMAND_HANDLER_USER_BEGIN(protocol::ReqKickOutUser, protocol::RspKickOutUser);
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
		group.kick_out_user(request.uid());
	SPACELESS_COMMAND_HANDLER_USER_END(protocol::RSP_KICK_OUT_USER);
}


void on_put_file(NetworkConnection& conn, const PackageBuffer& package)
{
	SPACELESS_COMMAND_HANDLER_USER_BEGIN(protocol::ReqPutFile, protocol::RspPutFile);
		SharingGroup& group = SharingGroupManager::instance()->get_group(request.group_id());
		FileTransferSession* session = nullptr;
		if (request.fragment().process_fragment_index() == 0)
		{
			session =
				&FileTransferSessionManager::instance()->register_transfer_session(group.group_id(), request.filename());
			session->max_fragment_index = request.fragment().max_fragment_index();
			session->process_fragment_index = request.fragment().process_fragment_index();
		}
		else
		{
			session =
				FileTransferSessionManager::instance()->find_transfer_session(group.group_id(), request.filename());
			if (request.fragment().max_fragment_index() != session->max_fragment_index ||
				request.fragment().process_fragment_index() != session->process_fragment_index + 1)
			{
				rsponse.set_result(-1);
				goto send_back_msg;
			}
			++session->process_fragment_index;
		}

		lights::SequenceView content(request.fragment().fragment_content());
		bool is_append = request.fragment().process_fragment_index() != 0;
		group.put_file(user->uid, request.filename(), content, is_append);

		if (request.fragment().process_fragment_index() + 1 == request.fragment().max_fragment_index())
		{
			FileTransferSessionManager::instance()->remove_transfer_session(session->session_id);
		}
	SPACELESS_COMMAND_HANDLER_USER_END(protocol::RSP_PUT_FILE);
}


void on_get_file(NetworkConnection& conn, const PackageBuffer& package)
{
//	SPACELESS_COMMAND_HANDLER_USER_BEGIN(protocol::ReqJoinGroup, protocol::RspJoinGroup);
//		SharingGroup& group = SharingGroupManager::instance()->get_group(request.group_id());
//		group.join_group(user->uid);
//	SPACELESS_COMMAND_HANDLER_USER_END(protocol::RSP_JOIN_GROUP);
}

} // namespace transcation
} // namespace resource_server
} // namespace spaceless
