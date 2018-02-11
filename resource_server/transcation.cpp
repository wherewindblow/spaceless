/**
 * transcation.cpp
 * @author wherewindblow
 * @date   Dec 24, 2017
 */

#include "transcation.h"

#include <cmath>
#include <protocol/all.h>

#include "core.h"


namespace spaceless {
namespace resource_server {
namespace transcation {

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
	conn.send_back_protobuf(rsponse_cmd, rsponse); \
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
		SPACELESS_ERROR(MODULE_RESOURCE_SERVER, "Connection {}, user {}: {}", conn.connection_id(), user->user_id, ex); \
		log_error = false; \
	} \
	\
	send_back_msg: \
	conn.send_back_protobuf(rsponse_cmd, rsponse); \
	if (rsponse.result() && log_error) \
	{ \
		if (user == nullptr) \
		{ \
			SPACELESS_ERROR(MODULE_RESOURCE_SERVER, "Connection {}: {}", conn.connection_id(), rsponse.result()); \
		} \
		else \
		{ \
			SPACELESS_ERROR(MODULE_RESOURCE_SERVER, "Connection {}, user {}: {}", \
				conn.connection_id(), user->user_id, rsponse.result()); \
		} \
	} \


void convert_user(const User& server_user, protocol::User& proto_user)
{
	proto_user.set_user_id(server_user.user_id);
	proto_user.set_username(server_user.username);
	for (auto& group_id : server_user.group_list)
	{
		*proto_user.mutable_group_list()->Add() = group_id;
	}
}


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
		bool pass = UserManager::instance()->login_user(request.user_id(), request.password(), conn);
		rsponse.set_result(pass ? 0 : -1);
	SPACELESS_COMMAND_HANDLER_END(protocol::RSP_LOGIN_USER);
}


void on_remove_user(NetworkConnection& conn, const PackageBuffer& package)
{
	SPACELESS_COMMAND_HANDLER_BEGIN(protocol::ReqRemoveUser, protocol::RspRemoveUser);
		UserManager::instance()->remove_user(request.user_id());
		rsponse.set_result(0);
	SPACELESS_COMMAND_HANDLER_END(protocol::RSP_REMOVE_USER);
}


void on_find_user(NetworkConnection& conn, const PackageBuffer& package)
{
	SPACELESS_COMMAND_HANDLER_BEGIN(protocol::ReqFindUser, protocol::RspFindUser);
		User* user = nullptr;
		if (request.user_id())
		{
			user = UserManager::instance()->find_user(request.user_id());
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
		SharingGroup& group = SharingGroupManager::instance()->register_group(user->user_id, request.group_name());
		rsponse.set_group_id(group.group_id());
	SPACELESS_COMMAND_HANDLER_USER_END(protocol::RSP_REGISTER_GROUP);
}


void on_remove_group(NetworkConnection& conn, const PackageBuffer& package)
{
	SPACELESS_COMMAND_HANDLER_USER_BEGIN(protocol::ReqRemoveGroup, protocol::RspRemoveGroup);
		SharingGroupManager::instance()->remove_group(user->user_id, request.group_id());
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
		group.join_group(user->user_id);
	SPACELESS_COMMAND_HANDLER_USER_END(protocol::RSP_JOIN_GROUP);
}


void on_kick_out_user(NetworkConnection& conn, const PackageBuffer& package)
{
	SPACELESS_COMMAND_HANDLER_USER_BEGIN(protocol::ReqKickOutUser, protocol::RspKickOutUser);
		SharingGroup& group = SharingGroupManager::instance()->get_group(request.group_id());
		if (request.user_id() != user->user_id) // Only manager can kick out other user.
		{
			if (!group.is_manager(user->user_id))
			{
				rsponse.set_result(-1);
				goto send_back_msg;
			}
		}
		group.kick_out_user(request.user_id());
	SPACELESS_COMMAND_HANDLER_USER_END(protocol::RSP_KICK_OUT_USER);
}


MultiplyPhaseTranscation* PutFileTranscation::register_transcation(int trans_id)
{
	return new PutFileTranscation(trans_id);
}


PutFileTranscation::PutFileTranscation(int trans_id) :
	MultiplyPhaseTranscation(trans_id) {}


MultiplyPhaseTranscation::PhaseResult PutFileTranscation::on_init(NetworkConnection& conn, const PackageBuffer& package)
{
	User* user = static_cast<User*>(first_connection()->get_attachment());
	if (user == nullptr)
	{
		return send_back_error(-1);
	}

	try
    {
		package.parse_as_protobuf(m_request);
		SharingGroup& group = SharingGroupManager::instance()->get_group(m_request.group_id());
		if (!group.is_manager(user->user_id))
		{
			return send_back_error(-1);
		}

		storage_node_conn->send_protobuf(protocol::REQ_PUT_FILE, m_request, transcation_id());
		wait_next_phase(*storage_node_conn, protocol::RSP_PUT_FILE, WAIT_STORAGE_NODE_PUT_FILE, 1);
		return WAIT_NEXT_PHASE;
	}
	catch (Exception& ex)
	{
		SPACELESS_ERROR(MODULE_RESOURCE_SERVER, "Connection {}, user {}: {}",
						first_connection()->connection_id(), user->user_id, ex);
		return send_back_error(ex.code());
	}
}


MultiplyPhaseTranscation::PhaseResult PutFileTranscation::on_active(NetworkConnection& conn, const PackageBuffer& package)
{
	User* user = static_cast<User*>(first_connection()->get_attachment());
	if (user == nullptr)
	{
		return send_back_error(-1);
	}

	try
	{
		package.parse_as_protobuf(m_rsponse);
		if (m_rsponse.result())
		{
			SPACELESS_ERROR(MODULE_RESOURCE_SERVER, "Connection {}, user {}: {}",
							first_connection()->connection_id(), user->user_id, m_rsponse.result());
		}
		send_back_message(protocol::RSP_PUT_FILE, m_rsponse);
		return EXIT_TRANCATION;
	}
	catch (Exception& ex)
	{
		SPACELESS_ERROR(MODULE_RESOURCE_SERVER, "Connection {}, user {}: {}",
						first_connection()->connection_id(), user->user_id, ex);
		return send_back_error(ex.code());
	}
}


MultiplyPhaseTranscation::PhaseResult PutFileTranscation::send_back_error(int error_code)
{
	m_rsponse.set_result(error_code);
	send_back_message(protocol::RSP_PUT_FILE, m_rsponse);
	return EXIT_TRANCATION;
}


MultiplyPhaseTranscation* GetFileTranscation::register_transcation(int trans_id)
{
	return new GetFileTranscation(trans_id);
}


GetFileTranscation::GetFileTranscation(int trans_id) :
	MultiplyPhaseTranscation(trans_id) {}


MultiplyPhaseTranscation::PhaseResult GetFileTranscation::on_init(NetworkConnection& conn, const PackageBuffer& package)
{
	User* user = static_cast<User*>(first_connection()->get_attachment());
	if (user == nullptr)
	{
		return send_back_error(-1);
	}

	try
	{
		package.parse_as_protobuf(m_request);
		SharingGroup& group = SharingGroupManager::instance()->get_group(m_request.group_id());
		if (!group.is_member(user->user_id))
		{
			return send_back_error(-1);
		}

		storage_node_conn->send_protobuf(protocol::REQ_GET_FILE, m_request, transcation_id());
		wait_next_phase(*storage_node_conn, protocol::RSP_GET_FILE, WAIT_STORAGE_NODE_GET_FILE, 1);
		return WAIT_NEXT_PHASE;
	}
	catch (Exception& ex)
	{
		SPACELESS_ERROR(MODULE_RESOURCE_SERVER, "Connection {}, user {}: {}",
						first_connection()->connection_id(), user->user_id, ex);
		return send_back_error(ex.code());
	}
}


MultiplyPhaseTranscation::PhaseResult GetFileTranscation::on_active(NetworkConnection& conn, const PackageBuffer& package)
{
	User* user = static_cast<User*>(first_connection()->get_attachment());
	if (user == nullptr)
	{
		return send_back_error(-1);
	}

	try
	{
		package.parse_as_protobuf(m_rsponse);
		if (m_rsponse.result())
		{
			SPACELESS_ERROR(MODULE_RESOURCE_SERVER, "Connection {}, user {}: {}",
							first_connection()->connection_id(), user->user_id, m_rsponse.result());
		}
		send_back_message(protocol::RSP_GET_FILE, m_rsponse);
		return EXIT_TRANCATION;
	}
	catch (Exception& ex)
	{
		SPACELESS_ERROR(MODULE_RESOURCE_SERVER, "Connection {}, user {}: {}",
						first_connection()->connection_id(), user->user_id, ex);
		return send_back_error(ex.code());
	}
}


MultiplyPhaseTranscation::PhaseResult GetFileTranscation::send_back_error(int error_code)
{
	m_rsponse.set_result(error_code);
	send_back_message(protocol::RSP_GET_FILE, m_rsponse);
	return EXIT_TRANCATION;
}

} // namespace transcation
} // namespace resource_server
} // namespace spaceless
