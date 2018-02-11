#include <iostream>

#include <lights/ostream.h>
#include <lights/file.h>
#include <protocol/all.h>
#include <common/network.h>
#include <common/log.h>

#include "core.h"
#include "transcation.h"


namespace spaceless {
namespace resource_server {

int main(int argc, const char* argv[])
{
	try
	{
		logger.set_level(lights::LogLevel::DEBUG);

		NetworkConnectionManager::instance()->register_listener("127.0.0.1", 10240);
		storage_node_conn = &NetworkConnectionManager::instance()->register_connection("127.0.0.1", 10241);

		std::pair<int, OnePhaseTrancation> handlers[] = {
			{protocol::REQ_REGISTER_USER, transcation::on_register_user},
			{protocol::REQ_LOGIN_USER, transcation::on_login_user},
			{protocol::REQ_REMOVE_USER, transcation::on_remove_user},
			{protocol::REQ_FIND_USER, transcation::on_find_user},
			{protocol::REQ_REGISTER_GROUP, transcation::on_register_group},
			{protocol::REQ_REMOVE_GROUP, transcation::on_remove_group},
			{protocol::REQ_FIND_GROUP, transcation::on_find_group},
			{protocol::REQ_JOIN_GROUP, transcation::on_join_group},
			{protocol::REQ_KICK_OUT_USER, transcation::on_kick_out_user}
		};

		for (std::size_t i = 0; i < lights::size_of_array(handlers); ++i)
		{
			TranscationManager::instance()->register_one_phase_transcation(handlers[i].first, handlers[i].second);
		}

		std::pair<int, TranscationFatory> fatories[] = {
			{protocol::REQ_PUT_FILE, transcation::PutFileTranscation::register_transcation},
			{protocol::REQ_GET_FILE, transcation::GetFileTranscation::register_transcation}
		};

		for (std::size_t i = 0; i < lights::size_of_array(fatories); ++i)
		{
			TranscationManager::instance()->register_multiply_phase_transcation(fatories[i].first, fatories[i].second);
		}

		NetworkConnectionManager::instance()->run();
	}
	catch (Exception& ex)
	{
		SPACELESS_ERROR(MODULE_RESOURCE_SERVER, ex);
	}
	catch (std::exception& ex)
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
