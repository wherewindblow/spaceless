#include <common/network.h>
#include <common/transaction.h>
#include <common/log.h>
#include <protocol/all.h>

#include <Poco/Util/JSONConfiguration.h>

#include "core.h"
#include "transaction.h"


namespace spaceless {
namespace storage_node {

static Logger& logger = get_logger("storage_node");

const std::string CONFIGURATION_PATH = "../configuration/resource_server_conf.json";

int main(int argc, const char* argv[])
{
	try
	{
		Poco::Util::JSONConfiguration configuration(CONFIGURATION_PATH);
		std::string log_level = configuration.getString("log_level");

		LoggerManager::instance()->for_each([&](const std::string& name, Logger* logger) {
			logger->set_level(to_log_level(log_level));
		});

		if (argc < 4)
		{
			LIGHTS_ERROR(logger, "Not enought argumets to start up.");
			return -1;
		}

		std::string sharing_path = argv[1];
		std::string ip = argv[2];
		unsigned short port = static_cast<unsigned short>(std::stoi(argv[3]));

		SharingFileManager::instance()->set_sharing_path(sharing_path);
		NetworkConnectionManager::instance()->register_listener(ip, port);

		SPACELESS_REG_ONE_TRANS(protocol::ReqNodePutFileSession, transaction::on_put_file_session)
		SPACELESS_REG_ONE_TRANS(protocol::ReqPutFile, transaction::on_put_file)
		SPACELESS_REG_ONE_TRANS(protocol::ReqNodeGetFileSession, transaction::on_get_file_session)
		SPACELESS_REG_ONE_TRANS(protocol::ReqGetFile, transaction::on_get_file)

		NetworkConnectionManager::instance()->run();
	}
	catch (Exception& ex)
	{
		LIGHTS_ERROR(logger, ex);
	}
	return 0;
}

} // namespace storage_node
} // namespace spaceless


int main(int argc, const char* argv[])
{
	return spaceless::storage_node::main(argc, argv);
}
