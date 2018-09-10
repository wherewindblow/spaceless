/**
 * transaction.cpp
 * @author wherewindblow
 * @date   Dec 24, 2017
 */

#include "transaction.h"

#include <cmath>
#include <protocol/all.h>

#include "core.h"


namespace spaceless {
namespace resource_server {
namespace transaction {

enum
{
	ERR_USER_LOGIN_FALIURE = 2001,
	ERR_PATH_NOT_EXIST = 2100,
	ERR_PATH_ALREADY_EXIST = 2101,
	ERR_PATH_NOT_GENERAL_FILE = 2102,
};


void convert_user(const User& server_user, protocol::User& proto_user)
{
	proto_user.set_user_id(server_user.user_id);
	proto_user.set_user_name(server_user.user_name);
	for (auto& group_id : server_user.group_list)
	{
		*proto_user.mutable_group_list()->Add() = group_id;
	}
}


void on_register_user(int conn_id, const PackageBuffer& package)
{
	protocol::ReqRegisterUser request;
	protocol::RspRegisterUser response;
	package.parse_as_protobuf(request);

	User& user = UserManager::instance()->register_user(request.username(), request.password());
	convert_user(user, *response.mutable_user());
	Network::send_back_protobuf(conn_id, response, package);
}


void on_login_user(int conn_id, const PackageBuffer& package)
{
	protocol::ReqLoginUser request;
	protocol::RspLoginUser response;
	package.parse_as_protobuf(request);

	bool pass = UserManager::instance()->login_user(request.user_id(), request.password(), conn_id);
	if (!pass)
	{
		response.set_result(ERR_USER_LOGIN_FALIURE);
	}

	Network::send_back_protobuf(conn_id, response, package);
}


void on_remove_user(int conn_id, const PackageBuffer& package)
{
	protocol::ReqRemoveUser request;
	protocol::RspRemoveUser response;
	package.parse_as_protobuf(request);

	UserManager::instance()->remove_user(request.user_id());

	Network::send_back_protobuf(conn_id, response, package);
}


void on_find_user(int conn_id, const PackageBuffer& package)
{
	protocol::ReqFindUser request;
	protocol::RspFindUser response;
	package.parse_as_protobuf(request);

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
		convert_user(*user, *response.mutable_user());
	}
	else
	{
		response.set_result(ERR_USER_NOT_EXIST);
	}

	Network::send_back_protobuf(conn_id, response, package);
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


void on_register_group(int conn_id, const PackageBuffer& package)
{
	protocol::ReqRegisterGroup request;
	protocol::RspRegisterGroup response;
	package.parse_as_protobuf(request);

	User& user = UserManager::instance()->get_login_user(conn_id);
	SharingGroup& group = SharingGroupManager::instance()->register_group(user.user_id, request.group_name());
	response.set_group_id(group.group_id());

	Network::send_back_protobuf(conn_id, response, package);
}


void on_remove_group(int conn_id, const PackageBuffer& package)
{
	protocol::ReqRemoveGroup request;
	protocol::RspRemoveGroup response;
	package.parse_as_protobuf(request);

	User& user = UserManager::instance()->get_login_user(conn_id);
	SharingGroupManager::instance()->remove_group(user.user_id, request.group_id());

	Network::send_back_protobuf(conn_id, response, package);
}


void on_find_group(int conn_id, const PackageBuffer& package)
{
	protocol::ReqFindGroup request;
	protocol::RspFindGroup response;
	package.parse_as_protobuf(request);

	User& user = UserManager::instance()->get_login_user(conn_id);

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
		convert_group(*group, *response.mutable_group());
	}
	else
	{
		response.set_result(ERR_GROUP_NOT_EXIST);
	}

	Network::send_back_protobuf(conn_id, response, package);
}


void on_join_group(int conn_id, const PackageBuffer& package)
{
	protocol::ReqJoinGroup request;
	protocol::RspJoinGroup response;
	package.parse_as_protobuf(request);

	User& user = UserManager::instance()->get_login_user(conn_id);
	SharingGroup& group = SharingGroupManager::instance()->get_group(request.group_id());
	group.join_group(user.user_id);

	Network::send_back_protobuf(conn_id, response, package);
}


void on_assign_as_manager(int conn_id, const PackageBuffer& package)
{
	protocol::ReqAssignAsManager request;
	protocol::RspAssignAsManager response;
	package.parse_as_protobuf(request);

	User& user = UserManager::instance()->get_login_user(conn_id);
	SharingGroup& group = SharingGroupManager::instance()->get_group(request.group_id());
	if (!group.is_manager(user.user_id))
	{
		response.set_result(ERR_GROUP_NOT_PERMIT_NEED_MANAGER);
	}
	else
	{
		group.assign_as_manager(request.user_id());
	}

	Network::send_back_protobuf(conn_id, response, package);
}


void on_assign_as_memeber(int conn_id, const PackageBuffer& package)
{
	protocol::ReqAssignAsMemeber request;
	protocol::RspAssignAsMemeber response;
	package.parse_as_protobuf(request);

	User& user = UserManager::instance()->get_login_user(conn_id);
	SharingGroup& group = SharingGroupManager::instance()->get_group(request.group_id());
	if (!group.is_manager(user.user_id))
	{
		response.set_result(ERR_GROUP_NOT_PERMIT_NEED_MANAGER);
	}
	else
	{
		group.assign_as_memeber(request.user_id());
	}

	Network::send_back_protobuf(conn_id, response, package);
}


void on_kick_out_user(int conn_id, const PackageBuffer& package)
{
	protocol::ReqKickOutUser request;
	protocol::RspKickOutUser response;
	package.parse_as_protobuf(request);

	User& user = UserManager::instance()->get_login_user(conn_id);
	SharingGroup& group = SharingGroupManager::instance()->get_group(request.group_id());
	if (request.user_id() != user.user_id) // Only manager can kick out other user.
	{
		if (!group.is_manager(user.user_id))
		{
			LIGHTS_THROW_EXCEPTION(Exception, ERR_GROUP_NOT_PERMIT_NEED_MANAGER);
		}
	}
	group.kick_out_user(request.user_id());

	Network::send_back_protobuf(conn_id, response, package);
}


void on_create_path(int conn_id, const PackageBuffer& package)
{
	protocol::ReqCreatePath request;
	protocol::RspCreatePath response;
	package.parse_as_protobuf(request);

	User& user = UserManager::instance()->get_login_user(conn_id);
	SharingGroup& group = SharingGroupManager::instance()->get_group(request.group_id());
	if (!group.is_manager(user.user_id))
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_GROUP_NOT_PERMIT_NEED_MANAGER);
	}

	group.create_path(request.path());

	Network::send_back_protobuf(conn_id, response, package);
}


MultiplyPhaseTransaction* PutFileTrans::register_transaction(int trans_id)
{
	return new PutFileTrans(trans_id);
}


PutFileTrans::PutFileTrans(int trans_id) :
	MultiplyPhaseTransaction(trans_id) {}


void PutFileTrans::on_init(int conn_id, const PackageBuffer& package)
{
	User& user = UserManager::instance()->get_login_user(conn_id);

	package.parse_as_protobuf(m_request);
	SharingGroup& group = SharingGroupManager::instance()->get_group(m_request.group_id());
	if (!group.is_manager(user.user_id))
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_GROUP_NOT_PERMIT_NEED_MANAGER);
	}

	FilePath path = m_request.file_path();
	if (m_request.fragment().fragment_index() == 0) // First put request.
	{
		if (!group.exist_path(path.directory_path()))
		{
			LIGHTS_THROW_EXCEPTION(Exception, ERR_PATH_NOT_EXIST);
		}

		if (group.exist_path(path))
		{
			LIGHTS_THROW_EXCEPTION(Exception, ERR_PATH_ALREADY_EXIST);
		}

		SharingFile& storage_file = SharingFileManager::instance()->register_file(SharingFile::STORAGE_FILE,
																				  path.filename(),
																				  group.storage_node_id());
		SharingFile& general_file = SharingFileManager::instance()->register_file(SharingFile::GENERAL_FILE,
																				  path.filename(),
																				  storage_file.file_id);
		group.add_file(path.directory_path(), general_file.file_id);
	}

	protocol::ReqPutFile request_to_storage = m_request;
	request_to_storage.set_file_path(path.filename());
	StorageNode& storage_node = StorageNodeManager::instance()->get_node(group.storage_node_id());
	Network::send_protobuf(storage_node.conn_id, request_to_storage, transaction_id());
	wait_next_phase(storage_node.conn_id, protocol::RspPutFile(), WAIT_STORAGE_NODE_PUT_FILE);
}


void PutFileTrans::on_active(int conn_id, const PackageBuffer& package)
{
	User& user = UserManager::instance()->get_login_user(first_connection_id());

	package.parse_as_protobuf(m_rsponse);
	send_back_message(m_rsponse);
}


void PutFileTrans::on_timeout()
{

}


void PutFileTrans::send_back_error(int error_code)
{
	m_rsponse.set_result(error_code);
	send_back_message(m_rsponse);
}


MultiplyPhaseTransaction* GetFileTrans::register_transaction(int trans_id)
{
	return new GetFileTrans(trans_id);
}


GetFileTrans::GetFileTrans(int trans_id) :
	MultiplyPhaseTransaction(trans_id) {}


void GetFileTrans::on_init(int conn_id, const PackageBuffer& package)
{
	User& user = UserManager::instance()->get_login_user(conn_id);

//	protocol::ReqGetFile m_request;
	package.parse_as_protobuf(m_request);
	SharingGroup& group = SharingGroupManager::instance()->get_group(m_request.group_id());
	if (!group.is_member(user.user_id))
	{
		return send_back_error(ERR_GROUP_NOT_PERMIT_NEED_MEMBER);
	}

	FilePath path = m_request.file_path();
	if (!group.exist_path(path))
	{
		return send_back_error(ERR_PATH_NOT_EXIST);
	}

	int file_id = group.get_file_id(path);
	SharingFile& general_file = SharingFileManager::instance()->get_file(file_id);
	if (general_file.file_type != SharingFile::GENERAL_FILE)
	{
		return send_back_error(ERR_PATH_NOT_GENERAL_FILE);
	}

	auto& real_general_file = dynamic_cast<SharingGeneralFile&>(general_file);
	SharingFile& storage_file = SharingFileManager::instance()->get_file(real_general_file.storage_file_id);
	LIGHTS_ASSERT(storage_file.file_type == SharingFile::STORAGE_FILE);
	auto& real_storage_file = dynamic_cast<SharingStorageFile&>(storage_file);

	protocol::ReqGetFile request_to_storage = m_request;
	request_to_storage.set_file_path(path.filename());
	StorageNode& storage_node = StorageNodeManager::instance()->get_node(real_storage_file.node_id);
	Network::send_protobuf(storage_node.conn_id, request_to_storage, transaction_id());
	wait_next_phase(storage_node.conn_id, protocol::RspGetFile(), WAIT_STORAGE_NODE_GET_FILE);
}


void GetFileTrans::on_active(int conn_id, const PackageBuffer& package)
{
	User& user = UserManager::instance()->get_login_user(first_connection_id());

	package.parse_as_protobuf(m_rsponse);
	send_back_message(m_rsponse);
}


void GetFileTrans::send_back_error(int error_code)
{
	m_rsponse.set_result(error_code);
	send_back_message(m_rsponse);
}


MultiplyPhaseTransaction* RemovePathTrans::register_transaction(int trans_id)
{
	return new RemovePathTrans(trans_id);
}


RemovePathTrans::RemovePathTrans(int trans_id) :
	MultiplyPhaseTransaction(trans_id) {}


void RemovePathTrans::on_init(int conn_id, const PackageBuffer& package)
{
	User& user = UserManager::instance()->get_login_user(conn_id);

	package.parse_as_protobuf(m_request);
	SharingGroup& group = SharingGroupManager::instance()->get_group(m_request.group_id());
	if (!group.is_manager(user.user_id))
	{
		return send_back_error(ERR_GROUP_NOT_PERMIT_NEED_MANAGER);
	}

	group.remove_path(m_request.path());
}


void RemovePathTrans::on_active(int conn_id, const PackageBuffer& package)
{
}


void RemovePathTrans::send_back_error(int error_code)
{
	m_rsponse.set_result(error_code);
	send_back_message(m_rsponse);
}

} // namespace transaction
} // namespace resource_server
} // namespace spaceless
