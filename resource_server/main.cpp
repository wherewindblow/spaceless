#include <protocol/all.h>
#include <common/network.h>
#include <common/log.h>

#include <Poco/Util/JSONConfiguration.h>

#include "core.h"
#include "transcation.h"


namespace spaceless {
namespace resource_server {

const std::string CONFIGURATION_PATH = "../configuration/resource_server_conf.json";

int main(int argc, const char* argv[])
{
	try
	{
		logger.set_level(lights::LogLevel::DEBUG);

		Poco::Util::JSONConfiguration configuration(CONFIGURATION_PATH);
		for (int i = 0;; ++i)
		{
			std::string key_prefix = "storage_nodes[" + std::to_string(i) + "]";
			try
			{
				std::string ip = configuration.getString(key_prefix + ".ip");
				unsigned int port = configuration.getUInt(key_prefix + ".port");
				StorageNodeManager::instance()->register_node(ip, static_cast<unsigned short>(port));
			}
			catch (Poco::NotFoundException& e)
			{
				if (i == 0)
				{
					SPACELESS_ERROR(MODULE_RESOURCE_SERVER, "Have not storage node in here");
				}
				else
				{
					break;  // Into array end.
				}
			}
		}

		std::string ip = configuration.getString("resource_server.ip");
		unsigned int port = configuration.getUInt("resource_server.port");
		NetworkConnectionManager::instance()->register_listener(ip, static_cast<unsigned short>(port));

		std::string root_user_name = configuration.getString("root_user.name");
		std::string root_user_pwd = configuration.getString("root_user.password");
		User& root = UserManager::instance()->register_user(root_user_name, root_user_pwd);
		std::string root_group_name = configuration.getString("root_user.group");
		SharingGroupManager::instance()->register_group(root.user_id, root_group_name);

		std::pair<int, OnePhaseTrancation> handlers[] = {
			{protocol::REQ_REGISTER_USER, transcation::on_register_user},
			{protocol::REQ_LOGIN_USER, transcation::on_login_user},
			{protocol::REQ_REMOVE_USER, transcation::on_remove_user},
			{protocol::REQ_FIND_USER, transcation::on_find_user},
			{protocol::REQ_REGISTER_GROUP, transcation::on_register_group},
			{protocol::REQ_REMOVE_GROUP, transcation::on_remove_group},
			{protocol::REQ_FIND_GROUP, transcation::on_find_group},
			{protocol::REQ_JOIN_GROUP, transcation::on_join_group},
			{protocol::REQ_ASSIGN_AS_MANAGER, transcation::on_assign_as_manager},
			{protocol::REQ_ASSIGN_AS_MEMEBER, transcation::on_assign_as_memeber},
			{protocol::REQ_KICK_OUT_USER, transcation::on_kick_out_user},
			{protocol::REQ_CREATE_PATH, transcation::on_create_path},
		};

		for (std::size_t i = 0; i < lights::size_of_array(handlers); ++i)
		{
			TranscationManager::instance()->register_one_phase_transcation(handlers[i].first, handlers[i].second);
		}

		std::pair<int, TranscationFatory> fatories[] = {
			{protocol::REQ_PUT_FILE, transcation::PutFileTranscation::register_transcation},
			{protocol::REQ_GET_FILE, transcation::GetFileTranscation::register_transcation},
			{protocol::REQ_REMOVE_PATH, transcation::RemovePathTranscation::register_transcation},
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
