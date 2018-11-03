/**
 * transaction.cpp
 * @author wherewindblow
 * @date   Jan 09, 2018
 */

#include "transaction.h"

#include <cmath>
#include <foundation/transaction.h>
#include <protocol/all.h>

#include "core.h"


namespace spaceless {
namespace storage_node {
namespace transaction {

void on_put_file_session(int conn_id, Package package)
{
	protocol::ReqNodePutFileSession request;
	package.parse_to_protocol(request);

	FileSession* session = FileSessionManager::instance()->find_session(request.file_path());
	if (session != nullptr)
	{
		if (request.max_fragment() != session->max_fragment)
		{
			LIGHTS_THROW_EXCEPTION(Exception, ERR_FILE_SESSION_CANNOT_CHANGE_MAX_FRAGMENT);
		}
	}
	else
	{
		session = &FileSessionManager::instance()->register_put_session(request.file_path(), request.max_fragment());
	}

	protocol::RspNodePutFileSession response;
	response.set_session_id(session->session_id);
	Network::send_back_protobuf(conn_id, response, package);
}


void on_put_file(int conn_id, Package package)
{
	protocol::ReqPutFile request;
	package.parse_to_protocol(request);

	FileSession& session = FileSessionManager::instance()->get_session(request.session_id());

	if (request.fragment_index() >= session.max_fragment)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_FILE_SESSION_INVALID_FRAGMENT);
	}

	lights::SequenceView file_content(request.fragment_content());
	int start_pos = request.fragment_index() * protocol::MAX_FRAGMENT_CONTENT_LEN;
	bool is_flush = request.fragment_index() + 1 == session.max_fragment;
	SharingFileManager::instance()->put_file(session.filename, file_content, start_pos, is_flush);

	if (request.fragment_index() + 1 == session.max_fragment)
	{
		FileSessionManager::instance()->remove_session(session.session_id);
	}

	protocol::RspPutFile response;
	response.set_session_id(request.session_id());
	response.set_fragment_index(request.fragment_index());
	Network::send_back_protobuf(conn_id, response, package);
}


void on_get_file_session(int conn_id, Package package)
{
	protocol::ReqNodeGetFileSession request;
	package.parse_to_protocol(request);

	FileSession* session = FileSessionManager::instance()->find_session(request.file_path());
	if (session == nullptr)
	{
		session = &FileSessionManager::instance()->register_get_session(request.file_path(),
																		protocol::MAX_FRAGMENT_CONTENT_LEN);
	}

	protocol::RspNodeGetFileSession response;
	response.set_session_id(session->session_id);
	response.set_max_fragment(session->max_fragment);
	Network::send_back_protobuf(conn_id, response, package);
}


void on_get_file(int conn_id, Package package)
{
	protocol::ReqGetFile request;
	package.parse_to_protocol(request);

	FileSession& session = FileSessionManager::instance()->get_session(request.session_id());
	if (request.fragment_index() >= session.max_fragment)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_FILE_SESSION_INVALID_FRAGMENT);
	}

	protocol::RspGetFile response;
	response.set_fragment_index(request.fragment_index());

	char file_content[protocol::MAX_FRAGMENT_CONTENT_LEN];
	int start_pos = request.fragment_index() * protocol::MAX_FRAGMENT_CONTENT_LEN;
	std::size_t content_len = SharingFileManager::instance()->get_file(session.filename,
																	   {file_content, protocol::MAX_FRAGMENT_CONTENT_LEN},
																	   start_pos);
	response.set_fragment_content(file_content, content_len);

	if (request.fragment_index() + 1 == session.max_fragment)
	{
		FileSessionManager::instance()->remove_session(session.session_id);
	}

	Network::send_back_protobuf(conn_id, response, package);
}

} // namespace transaction
} // namespace storage_node
} // namespace spaceless
