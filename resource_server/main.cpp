#include <protocol/all.h>
#include <common/network.h>
#include <common/transaction.h>
#include <common/log.h>

#include <Poco/Util/JSONConfiguration.h>

#include "core.h"
#include "transaction.h"


namespace spaceless {
namespace resource_server {

const std::string CONFIGURATION_PATH = "../configuration/resource_server_conf.json";

int main(int argc, const char* argv[])
{
	try
	{
		logger.set_level(lights::LogLevel::INFO);

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
			{protocol::REQ_REGISTER_USER, transaction::on_register_user},
			{protocol::REQ_LOGIN_USER, transaction::on_login_user},
			{protocol::REQ_REMOVE_USER, transaction::on_remove_user},
			{protocol::REQ_FIND_USER, transaction::on_find_user},
			{protocol::REQ_REGISTER_GROUP, transaction::on_register_group},
			{protocol::REQ_REMOVE_GROUP, transaction::on_remove_group},
			{protocol::REQ_FIND_GROUP, transaction::on_find_group},
			{protocol::REQ_JOIN_GROUP, transaction::on_join_group},
			{protocol::REQ_ASSIGN_AS_MANAGER, transaction::on_assign_as_manager},
			{protocol::REQ_ASSIGN_AS_MEMEBER, transaction::on_assign_as_memeber},
			{protocol::REQ_KICK_OUT_USER, transaction::on_kick_out_user},
			{protocol::REQ_CREATE_PATH, transaction::on_create_path},
		};

		for (std::size_t i = 0; i < lights::size_of_array(handlers); ++i)
		{
			TransactionManager::instance()->register_one_phase_transaction(handlers[i].first, handlers[i].second);
		}

		std::pair<int, TransactionFatory> fatories[] = {
			{protocol::REQ_PUT_FILE, transaction::PutFileTrans::register_transaction},
			{protocol::REQ_GET_FILE, transaction::GetFileTrans::register_transaction},
			{protocol::REQ_REMOVE_PATH, transaction::RemovePathTrans::register_transaction},
		};

		for (std::size_t i = 0; i < lights::size_of_array(fatories); ++i)
		{
			TransactionManager::instance()->register_multiply_phase_transaction(fatories[i].first, fatories[i].second);
		}

		NetworkConnectionManager::instance()->run();
	}
	catch (Exception& ex)
	{
		SPACELESS_ERROR(MODULE_RESOURCE_SERVER, ex);
	}
	catch (Poco::Exception& ex)
	{
		SPACELESS_ERROR(MODULE_RESOURCE_SERVER, "{}:{}", ex.name(), ex.message());
	}
	catch (std::exception& ex)
	{
		SPACELESS_ERROR(MODULE_RESOURCE_SERVER, "{}:{}", typeid(ex).name(), ex.what());
	}
	return 0;
}

} // namespace resource_server
} // namespace spaceless


int main(int argc, const char* argv[])
{
	return spaceless::resource_server::main(argc, argv);
}
