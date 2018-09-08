#include <common/network.h>
#include <common/transaction.h>
#include <common/log.h>
#include <protocol/all.h>

#include <Poco/Util/JSONConfiguration.h>

#include "core.h"
#include "transaction.h"


namespace spaceless {
namespace resource_server {

static Logger& logger = get_logger("resource_server");

const std::string CONFIGURATION_PATH = "../configuration/resource_server_conf.json";

int main(int argc, const char* argv[])
{
	try
	{
		LoggerManager::instance()->for_each([](const std::string& name, Logger* logger) {
			logger->set_level(lights::LogLevel::DEBUG);
		});

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
					LIGHTS_ERROR(logger, "Have not storage node in here");
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

		SPACE_REGISTER_ONE_PHASE_TRANSACTION(protocol::ReqRegisterUser, transaction::on_register_user)
		SPACE_REGISTER_ONE_PHASE_TRANSACTION(protocol::ReqLoginUser, transaction::on_login_user)
		SPACE_REGISTER_ONE_PHASE_TRANSACTION(protocol::ReqRemoveUser, transaction::on_remove_user)
		SPACE_REGISTER_ONE_PHASE_TRANSACTION(protocol::ReqFindUser, transaction::on_find_user)
		SPACE_REGISTER_ONE_PHASE_TRANSACTION(protocol::ReqRegisterGroup, transaction::on_register_group)
		SPACE_REGISTER_ONE_PHASE_TRANSACTION(protocol::ReqFindGroup, transaction::on_find_group)
		SPACE_REGISTER_ONE_PHASE_TRANSACTION(protocol::ReqJoinGroup, transaction::on_join_group)
		SPACE_REGISTER_ONE_PHASE_TRANSACTION(protocol::ReqAssignAsManager, transaction::on_assign_as_manager)
		SPACE_REGISTER_ONE_PHASE_TRANSACTION(protocol::ReqAssignAsMemeber, transaction::on_assign_as_memeber)
		SPACE_REGISTER_ONE_PHASE_TRANSACTION(protocol::ReqKickOutUser, transaction::on_kick_out_user)
		SPACE_REGISTER_ONE_PHASE_TRANSACTION(protocol::ReqCreatePath, transaction::on_create_path)

		SPACE_REGISTER_MULTIPLE_PHASE_TRANSACTION(protocol::ReqPutFile, transaction::PutFileTrans::register_transaction);
		SPACE_REGISTER_MULTIPLE_PHASE_TRANSACTION(protocol::ReqGetFile, transaction::GetFileTrans::register_transaction);
		SPACE_REGISTER_MULTIPLE_PHASE_TRANSACTION(protocol::ReqRemovePath, transaction::RemovePathTrans::register_transaction);

		NetworkConnectionManager::instance()->run();
	}
	catch (Exception& ex)
	{
		LIGHTS_ERROR(logger, ex);
	}
	catch (Poco::Exception& ex)
	{
		LIGHTS_ERROR(logger, "{}:{}", ex.name(), ex.message());
	}
	catch (std::exception& ex)
	{
		LIGHTS_ERROR(logger, "{}:{}", typeid(ex).name(), ex.what());
	}
	return 0;
}

} // namespace resource_server
} // namespace spaceless


int main(int argc, const char* argv[])
{
	return spaceless::resource_server::main(argc, argv);
}
