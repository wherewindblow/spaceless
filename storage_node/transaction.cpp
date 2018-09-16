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

void on_put_file(int conn_id, const PackageBuffer& package)
{
	protocol::ReqPutFile request;
	protocol::RspPutFile response;
	package.parse_as_protobuf(request);

	FileTransferSession* session = nullptr;
	if (request.fragment().fragment_index() == 0) // First put request.
	{
		session =
			&FileTransferSessionManager::instance()->register_put_session(request.group_id(),
																		  request.file_path(),
																		  request.fragment().max_fragment());
	}
	else
	{
		session = FileTransferSessionManager::instance()->find_session(request.group_id(), request.file_path());
		if (session == nullptr)
		{
			LIGHTS_THROW_EXCEPTION(Exception, -1);
		}

		if (request.fragment().max_fragment() != session->max_fragment ||
			request.fragment().fragment_index() != session->fragment_index + 1)
		{
			LIGHTS_THROW_EXCEPTION(Exception, -1);
		}
		++session->fragment_index;
	}

	lights::SequenceView file_content(request.fragment().fragment_content());
	bool is_append = request.fragment().fragment_index() != 0;
	bool is_flush = request.fragment().fragment_index() + 1 == session->max_fragment;
	SharingFileManager::instance()->put_file(request.file_path(), file_content, is_append, is_flush);

	if (request.fragment().fragment_index() + 1 == request.fragment().max_fragment())
	{
		FileTransferSessionManager::instance()->remove_session(session->session_id);
	}

	response.set_fragment_index(request.fragment().fragment_index());
	Network::send_back_protobuf(conn_id, response, package);
}


void on_get_file(int conn_id, const PackageBuffer& package)
{
	protocol::ReqGetFile request;
	protocol::RspGetFile response;
	package.parse_as_protobuf(request);

	auto session = FileTransferSessionManager::instance()->find_session(request.group_id(), request.file_path());
	if (session == nullptr)
	{
		session = &FileTransferSessionManager::instance()->register_get_session(request.group_id(),
																				request.file_path(),
																				protocol::MAX_FRAGMENT_CONTENT_LEN);
	}

	response.mutable_fragment()->set_max_fragment(session->max_fragment);
	response.mutable_fragment()->set_fragment_index(session->fragment_index);

	char file_content[protocol::MAX_FRAGMENT_CONTENT_LEN];
	int start_pos = session->fragment_index * protocol::MAX_FRAGMENT_CONTENT_LEN;
	std::size_t content_len = SharingFileManager::instance()->get_file(request.file_path(),
																	   {file_content, protocol::MAX_FRAGMENT_CONTENT_LEN},
																	   start_pos);
	response.mutable_fragment()->set_fragment_content(file_content, content_len);

	++session->fragment_index;
	if (session->fragment_index >= session->max_fragment)
	{
		FileTransferSessionManager::instance()->remove_session(session->session_id);
	}

	Network::send_back_protobuf(conn_id, response, package);
}

} // namespace transaction
} // namespace storage_node
} // namespace spaceless
