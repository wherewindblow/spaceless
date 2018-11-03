#include <foundation/network.h>
#include <foundation/transaction.h>
#include <foundation/scheduler.h>
#include <foundation/log.h>
#include <foundation/configuration.h>
#include <protocol/all.h>

#include <lights/precise_time.h>

#include "core.h"
#include "transaction.h"


namespace spaceless {
namespace resource_server {

static Logger& logger = get_logger("resource_server");

int main(int argc, const char* argv[])
{
	try
	{
		Configuration::PathList path_list = {"../configuration/resource_server_conf.json", "../configuration/global_conf.json"};
		Configuration configuration(path_list);

		std::string str_log_level = configuration.getString("log_level");
		lights::LogLevel log_level = to_log_level(str_log_level);
		LoggerManager::instance()->for_each([&](const std::string& name, Logger& logger) {
			logger.set_level(log_level);
		});

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
		NetworkManager::instance()->register_listener(ip, static_cast<unsigned short>(port));

		std::string root_user_name = configuration.getString("root_user.name");
		std::string root_user_pwd = configuration.getString("root_user.password");
		User& root = UserManager::instance()->register_user(root_user_name, root_user_pwd);
		std::string root_group_name = configuration.getString("root_user.group");
		SharingGroupManager::instance()->register_group(root.user_id, root_group_name);

		using namespace transaction;
		SPACELESS_REG_ONE_TRANS(protocol::ReqRegisterUser, on_register_user)
		SPACELESS_REG_ONE_TRANS(protocol::ReqLoginUser, on_login_user)
		SPACELESS_REG_ONE_TRANS(protocol::ReqRemoveUser, on_remove_user)
		SPACELESS_REG_ONE_TRANS(protocol::ReqFindUser, on_find_user)
		SPACELESS_REG_ONE_TRANS(protocol::ReqRegisterGroup, on_register_group)
		SPACELESS_REG_ONE_TRANS(protocol::ReqFindGroup, on_find_group)
		SPACELESS_REG_ONE_TRANS(protocol::ReqJoinGroup, on_join_group)
		SPACELESS_REG_ONE_TRANS(protocol::ReqAssignAsManager, on_assign_as_manager)
		SPACELESS_REG_ONE_TRANS(protocol::ReqAssignAsMemeber, on_assign_as_memeber)
		SPACELESS_REG_ONE_TRANS(protocol::ReqKickOutUser, on_kick_out_user)
		SPACELESS_REG_ONE_TRANS(protocol::ReqCreatePath, on_create_path)

		SPACELESS_REG_MULTIPLE_TRANS(protocol::ReqPutFileSession, PutFileSessionTrans::factory);
		SPACELESS_REG_MULTIPLE_TRANS(protocol::ReqPutFile, PutFileTrans::factory);
		SPACELESS_REG_MULTIPLE_TRANS(protocol::ReqGetFileSession, GetFileSessionTrans::factory);
		SPACELESS_REG_MULTIPLE_TRANS(protocol::ReqGetFile, GetFileTrans::factory);
		SPACELESS_REG_MULTIPLE_TRANS(protocol::ReqRemovePath, RemovePathTrans::factory);

		Scheduler::instance()->start();
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
