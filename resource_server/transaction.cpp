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
	ERR_USER_LOGIN_FAILURE = 2001,
	ERR_USER_NOT_PERMISSION = 2002,
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


void convert_file(const SharingFile& server_file, protocol::File& proto_file)
{
	proto_file.set_filename(server_file.file_name);
	protocol::FileType type = protocol::FileType::GENERAL_FILE;
	if (server_file.file_type == SharingFile::DIRECTORY)
	{
		type = protocol::FileType::DIRECTORY;
	}
	proto_file.set_type(type);
}


void on_register_user(int conn_id, Package package)
{
	protocol::ReqRegisterUser request;
	protocol::RspRegisterUser response;
	package.parse_to_protocol(request);

	User& user = UserManager::instance()->register_user(request.username(), request.password());
	convert_user(user, *response.mutable_user());
	Network::send_back_protocol(conn_id, response, package);
}


void on_login_user(int conn_id, Package package)
{
	protocol::ReqLoginUser request;
	protocol::RspLoginUser response;
	package.parse_to_protocol(request);

	bool pass = UserManager::instance()->login_user(request.user_id(), request.password(), conn_id);
	if (!pass)
	{
		response.set_result(ERR_USER_LOGIN_FAILURE);
	}

	Network::send_back_protocol(conn_id, response, package);
}


void on_remove_user(int conn_id, Package package)
{
	protocol::ReqRemoveUser request;
	protocol::RspRemoveUser response;
	package.parse_to_protocol(request);

	User& user = UserManager::instance()->get_login_user(conn_id);
	if (UserManager::instance()->is_root_user(user.user_id))
	{
		UserManager::instance()->remove_user(request.user_id());
	}
	else
	{
		response.set_result(ERR_USER_NOT_PERMISSION);
	}

	Network::send_back_protocol(conn_id, response, package);
}


void on_find_user(int conn_id, Package package)
{
	protocol::ReqFindUser request;
	protocol::RspFindUser response;
	package.parse_to_protocol(request);

	UserManager::instance()->get_login_user(conn_id);

	User* target_user = nullptr;
	if (request.user_id())
	{
		target_user = UserManager::instance()->find_user(request.user_id());
	}
	else
	{
		target_user = UserManager::instance()->find_user(request.username());
	}

	if (target_user != nullptr)
	{
		convert_user(*target_user, *response.mutable_user());
	}
	else
	{
		response.set_result(ERR_USER_NOT_EXIST);
	}

	Network::send_back_protocol(conn_id, response, package);
}


void on_ping(int conn_id, Package package)
{
	protocol::ReqPing request;
	protocol::RspPing response;
	package.parse_to_protocol(request);

	User& user = UserManager::instance()->get_login_user(conn_id);
	UserManager::instance()->heartbeat(user.user_id);

	response.set_second(request.second());
	response.set_microsecond(request.microsecond());
	Network::send_back_protocol(conn_id, response, package);
}


void convert_group(const SharingGroup& server_group, protocol::SharingGroup& response_group)
{
	response_group.set_group_id(server_group.group_id());
	response_group.set_group_name(server_group.group_name());
	response_group.set_owner_id(server_group.owner_id());
	for (std::size_t i = 0; i < server_group.manager_list().size(); ++i)
	{
		*response_group.mutable_manager_list()->Add() = server_group.manager_list()[i];
	}
	for (std::size_t i = 0; i < server_group.member_list().size(); ++i)
	{
		*response_group.mutable_member_list()->Add() = server_group.member_list()[i];
	}
}


void on_register_group(int conn_id, Package package)
{
	protocol::ReqRegisterGroup request;
	protocol::RspRegisterGroup response;
	package.parse_to_protocol(request);

	User& user = UserManager::instance()->get_login_user(conn_id);
	SharingGroup& group = SharingGroupManager::instance()->register_group(user.user_id, request.group_name());
	response.set_group_id(group.group_id());

	Network::send_back_protocol(conn_id, response, package);
}


void on_remove_group(int conn_id, Package package)
{
	protocol::ReqRemoveGroup request;
	protocol::RspRemoveGroup response;
	package.parse_to_protocol(request);

	User& user = UserManager::instance()->get_login_user(conn_id);
	SharingGroupManager::instance()->remove_group(user.user_id, request.group_id());

	Network::send_back_protocol(conn_id, response, package);
}


void on_find_group(int conn_id, Package package)
{
	protocol::ReqFindGroup request;
	protocol::RspFindGroup response;
	package.parse_to_protocol(request);

	UserManager::instance()->get_login_user(conn_id);

	SharingGroup* group = nullptr;
	if (request.group_id())
	{
		group = SharingGroupManager::instance()->find_group(request.group_id());
	}
	else
	{
		group = SharingGroupManager::instance()->find_group(request.group_name());
	}

	if (group != nullptr)
	{
		convert_group(*group, *response.mutable_group());
	}
	else
	{
		response.set_result(ERR_GROUP_NOT_EXIST);
	}

	Network::send_back_protocol(conn_id, response, package);
}


void on_join_group(int conn_id, Package package)
{
	protocol::ReqJoinGroup request;
	protocol::RspJoinGroup response;
	package.parse_to_protocol(request);

	User& user = UserManager::instance()->get_login_user(conn_id);
	SharingGroup& group = SharingGroupManager::instance()->get_group(request.group_id());
	group.join_group(user.user_id);

	Network::send_back_protocol(conn_id, response, package);
}


void on_assign_as_manager(int conn_id, Package package)
{
	protocol::ReqAssignAsManager request;
	protocol::RspAssignAsManager response;
	package.parse_to_protocol(request);

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

	Network::send_back_protocol(conn_id, response, package);
}


void on_assign_as_member(int conn_id, Package package)
{
	protocol::ReqAssignAsMember request;
	protocol::RspAssignAsMember response;
	package.parse_to_protocol(request);

	User& user = UserManager::instance()->get_login_user(conn_id);
	SharingGroup& group = SharingGroupManager::instance()->get_group(request.group_id());
	if (!group.is_manager(user.user_id))
	{
		response.set_result(ERR_GROUP_NOT_PERMIT_NEED_MANAGER);
	}
	else
	{
		group.assign_as_member(request.user_id());
	}

	Network::send_back_protocol(conn_id, response, package);
}


void on_kick_out_user(int conn_id, Package package)
{
	protocol::ReqKickOutUser request;
	protocol::RspKickOutUser response;
	package.parse_to_protocol(request);

	User& user = UserManager::instance()->get_login_user(conn_id);
	SharingGroup& group = SharingGroupManager::instance()->get_group(request.group_id());
	if (request.user_id() != user.user_id) // Only manager can kick out other user.
	{
		if (!group.is_manager(user.user_id))
		{
			LIGHTS_THROW(Exception, ERR_GROUP_NOT_PERMIT_NEED_MANAGER);
		}
	}
	group.kick_out_user(request.user_id());

	Network::send_back_protocol(conn_id, response, package);
}


void on_create_path(int conn_id, Package package)
{
	protocol::ReqCreatePath request;
	protocol::RspCreatePath response;
	package.parse_to_protocol(request);

	User& user = UserManager::instance()->get_login_user(conn_id);
	SharingGroup& group = SharingGroupManager::instance()->get_group(request.group_id());
	if (!group.is_manager(user.user_id))
	{
		LIGHTS_THROW(Exception, ERR_GROUP_NOT_PERMIT_NEED_MANAGER);
	}

	group.create_path(request.path());

	Network::send_back_protocol(conn_id, response, package);
}


void on_list_file(int conn_id, Package package)
{
	protocol::ReqListFile request;
	protocol::RspListFile response;
	package.parse_to_protocol(request);

	User& user = UserManager::instance()->get_login_user(conn_id);
	SharingGroup& group = SharingGroupManager::instance()->get_group(request.group_id());
	int file_id = group.get_file_id(request.file_path());
	SharingFile& file = SharingFileManager::instance()->get_file(file_id);
	if (file.file_type != SharingFile::DIRECTORY)
	{
		LIGHTS_THROW(Exception, ERR_GROUP_NOT_DIRECTORY);
	}

	SharingDirectory& directory = dynamic_cast<SharingDirectory&>(file);
	for (int cur_file_id : directory.file_list)
	{
		SharingFile& sharing_file = SharingFileManager::instance()->get_file(cur_file_id);
		protocol::File* proto_file = response.mutable_file_list()->Add();
		convert_file(sharing_file, *proto_file);
	}
	
	Network::send_back_protocol(conn_id, response, package);
}


MultiplyPhaseTransaction* PutFileSessionTrans::factory(int trans_id)
{
	return new PutFileSessionTrans(trans_id);
}


PutFileSessionTrans::PutFileSessionTrans(int trans_id) :
	MultiplyPhaseTransaction(trans_id),
	m_session_id(0)
{}


void PutFileSessionTrans::on_init(int conn_id, Package package)
{
	User& user = UserManager::instance()->get_login_user(conn_id);
	protocol::ReqPutFileSession request;
	package.parse_to_protocol(request);

	SharingGroup& group = SharingGroupManager::instance()->get_group(request.group_id());
	if (!group.is_manager(user.user_id))
	{
		return send_back_error(ERR_GROUP_NOT_PERMIT_NEED_MANAGER);
	}

	PutFileSession* session = FileSessionManager::instance()->find_put_session(user.user_id,
																			   request.group_id(),
																			   request.file_path());
	if (session != nullptr)
	{
		if (request.max_fragment() != session->max_fragment)
		{
			LIGHTS_THROW(Exception, ERR_FILE_SESSION_CANNOT_CHANGE_MAX_FRAGMENT);
		}
	}
	else
	{
		FilePath path = request.file_path();
		if (!group.exist_path(path.directory_path()))
		{
			return send_back_error(ERR_PATH_NOT_EXIST);
		}

		if (group.exist_path(path))
		{
			return send_back_error(ERR_PATH_ALREADY_EXIST);
		}

		SharingFile& storage_file = SharingFileManager::instance()->register_file(SharingFile::STORAGE_FILE,
																				  path.filename(),
																				  group.node_id());
		SharingFile& general_file = SharingFileManager::instance()->register_file(SharingFile::GENERAL_FILE,
																				  path.filename(),
																				  storage_file.file_id);
		group.add_file(path.directory_path(), general_file.file_id);
		session = &FileSessionManager::instance()->register_put_session(user.user_id,
																		request.group_id(),
																		request.file_path(),
																		request.max_fragment());
	}

	m_session_id = session->session_id;

	protocol::ReqNodePutFileSession storage_request;
	FilePath path = session->file_path;
	storage_request.set_file_path(path.filename());
	storage_request.set_max_fragment(session->max_fragment);

	StorageNode& storage_node = StorageNodeManager::instance()->get_node(group.node_id());
	Network::service_send_protocol(storage_node.service_id, storage_request, transaction_id());
	service_wait_next_phase(storage_node.service_id, protocol::RspNodePutFileSession(), &PutFileSessionTrans::on_active);
}


void PutFileSessionTrans::on_active(int conn_id, Package package)
{
	protocol::RspNodePutFileSession node_response;
	package.parse_to_protocol(node_response);
	if (node_response.result())
	{
		FileSessionManager::instance()->remove_session(m_session_id);
		return send_back_error(node_response.result());
	}

	PutFileSession* session = FileSessionManager::instance()->find_put_session(m_session_id);
	session->node_session_id = node_response.session_id();

	protocol::RspPutFileSession response;
	response.set_session_id(m_session_id);
	response.set_next_fragment(session->next_fragment);
	send_back_message(response);
}


void PutFileSessionTrans::on_timeout()
{
	FileSessionManager::instance()->remove_session(m_session_id);
	MultiplyPhaseTransaction::on_timeout();
}


void PutFileSessionTrans::on_error(int conn_id, int error_code)
{
	FileSessionManager::instance()->remove_session(m_session_id);
	MultiplyPhaseTransaction::on_error(conn_id, error_code);
}


MultiplyPhaseTransaction* PutFileTrans::factory(int trans_id)
{
	return new PutFileTrans(trans_id);
}


PutFileTrans::PutFileTrans(int trans_id) :
	MultiplyPhaseTransaction(trans_id),
	m_session_id(0)
{}


void PutFileTrans::on_init(int conn_id, Package package)
{
	User& user = UserManager::instance()->get_login_user(conn_id);

	protocol::ReqPutFile request;
	package.parse_to_protocol(request);

	PutFileSession& session = FileSessionManager::instance()->get_put_session(request.session_id());
	if (session.user_id != user.user_id)
	{
		return send_back_error(ERR_FILE_SESSION_NOT_REGISTER_USER);
	}

	if (request.fragment_index() != session.next_fragment)
	{
		return send_back_error(ERR_FILE_SESSION_INVALID_FRAGMENT);
	}

	++session.next_fragment;
	m_session_id = session.session_id;

	protocol::ReqPutFile node_request = request;
	node_request.set_session_id(session.node_session_id);
	SharingGroup& group = SharingGroupManager::instance()->get_group(session.group_id);
	StorageNode& storage_node = StorageNodeManager::instance()->get_node(group.node_id());
	Network::service_send_protocol(storage_node.service_id, node_request, transaction_id());
	service_wait_next_phase(storage_node.service_id, protocol::RspPutFile(), &PutFileTrans::on_active);
}


void PutFileTrans::on_active(int conn_id, Package package)
{
	UserManager::instance()->get_login_user(first_connection_id());

	protocol::RspPutFile response;
	package.parse_to_protocol(response);
	response.set_session_id(m_session_id);
	send_back_message(response);
}


MultiplyPhaseTransaction* GetFileSessionTrans::factory(int trans_id)
{
	return new GetFileSessionTrans(trans_id);
}


GetFileSessionTrans::GetFileSessionTrans(int trans_id) :
	MultiplyPhaseTransaction(trans_id),
	m_session_id(0)
{}


void GetFileSessionTrans::on_init(int conn_id, Package package)
{
	User& user = UserManager::instance()->get_login_user(conn_id);

	protocol::ReqGetFileSession request;
	package.parse_to_protocol(request);
	SharingGroup& group = SharingGroupManager::instance()->get_group(request.group_id());
	if (!group.is_member(user.user_id))
	{
		return send_back_error(ERR_GROUP_NOT_PERMIT_NEED_MEMBER);
	}

	FilePath path = request.file_path();
	if (!group.exist_path(path))
	{
		return send_back_error(ERR_PATH_NOT_EXIST);
	}

	int file_id = group.get_file_id(path);
	SharingFile& sharing_file = SharingFileManager::instance()->get_file(file_id);
	if (sharing_file.file_type != SharingFile::GENERAL_FILE)
	{
		return send_back_error(ERR_PATH_NOT_GENERAL_FILE);
	}

	auto& general_file = dynamic_cast<SharingGeneralFile&>(sharing_file);
	SharingFile& storage_sharing_file = SharingFileManager::instance()->get_file(general_file.storage_file_id);
	LIGHTS_ASSERT(storage_sharing_file.file_type == SharingFile::STORAGE_FILE);
	auto& storage_file = dynamic_cast<SharingStorageFile&>(storage_sharing_file);

	GetFileSession* previous_session = FileSessionManager::instance()->find_get_session(user.user_id,
																						request.group_id(),
																						request.file_path());
	if (previous_session != nullptr)
	{
		m_session_id = previous_session->session_id;
	}
	else
	{
		GetFileSession& session = FileSessionManager::instance()->register_get_session(user.user_id,
																					   request.group_id(),
																					   request.file_path());
		m_session_id = session.session_id;
	}

	protocol::ReqNodeGetFileSession node_request;
	node_request.set_file_path(path.filename());
	StorageNode& storage_node = StorageNodeManager::instance()->get_node(storage_file.node_id);
	Network::service_send_protocol(storage_node.service_id, node_request, transaction_id());
	service_wait_next_phase(storage_node.service_id, protocol::RspNodeGetFileSession(), &GetFileSessionTrans::on_active);
}


void GetFileSessionTrans::on_active(int conn_id, Package package)
{
	protocol::RspNodeGetFileSession node_response;
	package.parse_to_protocol(node_response);

	if (node_response.result())
	{
		FileSessionManager::instance()->remove_session(m_session_id);
		send_back_error(node_response.result());
		return;
	}

	GetFileSession* session = FileSessionManager::instance()->find_get_session(m_session_id);
	session->node_session_id = node_response.session_id();

	protocol::RspGetFileSession response;
	response.set_session_id(m_session_id);
	response.set_max_fragment(node_response.max_fragment());
	send_back_message(response);
}


void GetFileSessionTrans::on_timeout()
{
	FileSessionManager::instance()->remove_session(m_session_id);
	MultiplyPhaseTransaction::on_timeout();
}


void GetFileSessionTrans::on_error(int conn_id, int error_code)
{
	FileSessionManager::instance()->remove_session(m_session_id);
	MultiplyPhaseTransaction::on_error(conn_id, error_code);
}


MultiplyPhaseTransaction* GetFileTrans::factory(int trans_id)
{
	return new GetFileTrans(trans_id);
}


GetFileTrans::GetFileTrans(int trans_id) :
	MultiplyPhaseTransaction(trans_id),
	m_session_id(0)
{}


void GetFileTrans::on_init(int conn_id, Package package)
{
	User& user = UserManager::instance()->get_login_user(conn_id);

	protocol::ReqGetFile request;
	package.parse_to_protocol(request);

	GetFileSession& session = FileSessionManager::instance()->get_get_session(request.session_id());
	if (session.user_id != user.user_id)
	{
		return send_back_error(ERR_FILE_SESSION_NOT_REGISTER_USER);
	}

	SharingGroup& group = SharingGroupManager::instance()->get_group(session.group_id);
	FilePath path = session.file_path;
	if (!group.exist_path(path))
	{
		return send_back_error(ERR_PATH_NOT_EXIST);
	}

	int file_id = group.get_file_id(path);
	SharingFile& sharing_file = SharingFileManager::instance()->get_file(file_id);
	if (sharing_file.file_type != SharingFile::GENERAL_FILE)
	{
		return send_back_error(ERR_PATH_NOT_GENERAL_FILE);
	}

	m_session_id = session.session_id;

	auto& general_file = dynamic_cast<SharingGeneralFile&>(sharing_file);
	SharingFile& storage_sharing_file = SharingFileManager::instance()->get_file(general_file.storage_file_id);
	LIGHTS_ASSERT(storage_sharing_file.file_type == SharingFile::STORAGE_FILE);
	auto& storage_file = dynamic_cast<SharingStorageFile&>(storage_sharing_file);

	protocol::ReqGetFile node_request;
	node_request.set_session_id(session.node_session_id);
	node_request.set_fragment_index(request.fragment_index());
	StorageNode& storage_node = StorageNodeManager::instance()->get_node(storage_file.node_id);
	Network::service_send_protocol(storage_node.service_id, node_request, transaction_id());
	service_wait_next_phase(storage_node.service_id, protocol::RspGetFile(), &GetFileTrans::on_active);
}


void GetFileTrans::on_active(int conn_id, Package package)
{
	UserManager::instance()->get_login_user(first_connection_id());

	protocol::RspGetFile response;
	package.parse_to_protocol(response);
	response.set_session_id(m_session_id);
	send_back_message(response);
}


MultiplyPhaseTransaction* RemovePathTrans::factory(int trans_id)
{
	return new RemovePathTrans(trans_id);
}


RemovePathTrans::RemovePathTrans(int trans_id) :
	MultiplyPhaseTransaction(trans_id) {}


void RemovePathTrans::on_init(int conn_id, Package package)
{
	User& user = UserManager::instance()->get_login_user(conn_id);

	protocol::ReqRemovePath request;
	package.parse_to_protocol(request);
	SharingGroup& group = SharingGroupManager::instance()->get_group(request.group_id());
	if (!group.is_manager(user.user_id))
	{
		return send_back_error(ERR_GROUP_NOT_PERMIT_NEED_MANAGER);
	}

	group.remove_path(request.path());
}


void RemovePathTrans::on_active(int conn_id, Package package)
{
}

} // namespace transaction
} // namespace resource_server
} // namespace spaceless
