/**
 * transcation.cpp
 * @author wherewindblow
 * @date   Jan 09, 2018
 */

#include "transcation.h"

#include <cmath>
#include <protocol/all.h>

#include "core.h"


namespace spaceless {
namespace storage_node {
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
		SPACELESS_ERROR(MODULE_STORAGE_NODE, "Connection {}: {}", conn.connection_id(), ex); \
		log_error = false; \
	} \
	\
	send_back_msg: \
	conn.send_back_protobuf(rsponse_cmd, rsponse); \
	if (rsponse.result() && log_error) \
	{ \
		SPACELESS_ERROR(MODULE_STORAGE_NODE, "Connection {}: {}", conn.connection_id(), rsponse.result()); \
	} \



void on_put_file(NetworkConnection& conn, const PackageBuffer& package)
{
	SPACELESS_COMMAND_HANDLER_BEGIN(protocol::ReqPutFile, protocol::RspPutFile);
		FileTransferSession* session = nullptr;
		if (request.fragment().fragment_index() == 0)
		{
			session =
				&FileTransferSessionManager::instance()->register_put_session(request.group_id(),
																			  request.filename(),
																			  request.fragment().max_fragment());
		}
		else
		{
			session = FileTransferSessionManager::instance()->find_session(request.group_id(), request.filename());
			if (session == nullptr)
			{
				rsponse.set_result(-1);
				goto send_back_msg;
			}

			if (request.fragment().max_fragment() != session->max_fragment ||
				request.fragment().fragment_index() != session->fragment_index + 1)
			{
				rsponse.set_result(-1);
				goto send_back_msg;
			}
			++session->fragment_index;
		}

		lights::SequenceView file_content(request.fragment().fragment_content());
		bool is_append = request.fragment().fragment_index() != 0;
		SharingFileManager::instance()->put_file(request.filename(), file_content, is_append);

		if (request.fragment().fragment_index() + 1 == request.fragment().max_fragment())
		{
			FileTransferSessionManager::instance()->remove_session(session->session_id);
		}
	SPACELESS_COMMAND_HANDLER_END(protocol::RSP_PUT_FILE);
}


void on_get_file(NetworkConnection& conn, const PackageBuffer& package)
{
	SPACELESS_COMMAND_HANDLER_BEGIN(protocol::ReqGetFile, protocol::RspGetFile);
		FileTransferSession* session =
			FileTransferSessionManager::instance()->find_session(request.group_id(), request.filename());
		if (session == nullptr)
		{
			session = &FileTransferSessionManager::instance()->register_get_session(request.group_id(),
																					request.filename(),
																					protocol::MAX_FRAGMENT_CONTENT_LEN);
		}

		rsponse.mutable_fragment()->set_max_fragment(session->max_fragment);
		rsponse.mutable_fragment()->set_fragment_index(session->fragment_index);

		char file_content[protocol::MAX_FRAGMENT_CONTENT_LEN];
		std::size_t content_len = SharingFileManager::instance()->get_file(request.filename(),
						{file_content, protocol::MAX_FRAGMENT_CONTENT_LEN},
						session->fragment_index * protocol::MAX_FRAGMENT_CONTENT_LEN);
		rsponse.mutable_fragment()->set_fragment_content(file_content, content_len);

		++session->fragment_index;
		if (session->fragment_index >= session->max_fragment)
		{
			FileTransferSessionManager::instance()->remove_session(session->session_id);
		}
	SPACELESS_COMMAND_HANDLER_END(protocol::RSP_GET_FILE);
}

} // namespace transcation
} // namespace storage_node
} // namespace spaceless