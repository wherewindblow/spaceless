/**
 * transaction.cpp
 * @author wherewindblow
 * @date   Jan 09, 2018
 */

#include "transaction.h"

#include <cmath>
#include <common/transaction.h>
#include <protocol/all.h>

#include "core.h"


namespace spaceless {
namespace storage_node {
namespace transaction {

void on_put_file_session(int conn_id, const PackageBuffer& package)
{
	protocol::ReqNodePutFileSession request;
	package.parse_as_protobuf(request);

	auto& session = FileSessionManager::instance()->register_put_session(request.file_path(), request.max_fragment());

	protocol::RspNodePutFileSession response;
	response.set_session_id(session.session_id);
	Network::send_back_protobuf(conn_id, response, package);
}


void on_put_file(int conn_id, const PackageBuffer& package)
{
	protocol::ReqPutFile request;
	package.parse_as_protobuf(request);

	FileSession* session = FileSessionManager::instance()->find_session(request.session_id());
	if (session == nullptr)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_FILE_SESSION_NOT_EXIST);
	}

	if (request.fragment_index() != session->fragment_index + 1)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_FILE_SESSION_INVALID_INDEX);
	}
	++session->fragment_index;

	lights::SequenceView file_content(request.fragment_content());
	bool is_append = request.fragment_index() != 0;
	bool is_flush = request.fragment_index() + 1 == session->max_fragment;
	SharingFileManager::instance()->put_file(session->filename, file_content, is_append, is_flush);

	if (request.fragment_index() + 1 == session->max_fragment)
	{
		FileSessionManager::instance()->remove_session(session->session_id);
	}

	protocol::RspPutFile response;
	response.set_session_id(request.session_id());
	response.set_fragment_index(request.fragment_index());
	Network::send_back_protobuf(conn_id, response, package);
}


void on_get_file_session(int conn_id, const PackageBuffer& package)
{
	protocol::ReqNodeGetFileSession request;
	package.parse_as_protobuf(request);

	auto& session = FileSessionManager::instance()->register_get_session(request.file_path(),
																		 protocol::MAX_FRAGMENT_CONTENT_LEN);

	protocol::RspNodeGetFileSession response;
	response.set_session_id(session.session_id);
	response.set_max_fragment(session.max_fragment);
	Network::send_back_protobuf(conn_id, response, package);
}


void on_get_file(int conn_id, const PackageBuffer& package)
{
	protocol::ReqGetFile request;
	package.parse_as_protobuf(request);

	FileSession* session = FileSessionManager::instance()->find_session(request.session_id());
	if (session == nullptr)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_FILE_SESSION_NOT_EXIST);
	}

	if (request.fragment_index() != session->fragment_index + 1)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_FILE_SESSION_INVALID_INDEX);
	}
	++session->fragment_index;

	protocol::RspGetFile response;
	response.set_fragment_index(request.fragment_index());

	char file_content[protocol::MAX_FRAGMENT_CONTENT_LEN];
	int start_pos = request.fragment_index() * protocol::MAX_FRAGMENT_CONTENT_LEN;
	std::size_t content_len = SharingFileManager::instance()->get_file(session->filename,
																	   {file_content, protocol::MAX_FRAGMENT_CONTENT_LEN},
																	   start_pos);
	response.set_fragment_content(file_content, content_len);

	if (request.fragment_index() + 1 == session->max_fragment)
	{
		FileSessionManager::instance()->remove_session(session->session_id);
	}

	Network::send_back_protobuf(conn_id, response, package);
}

} // namespace transaction
} // namespace storage_node
} // namespace spaceless
