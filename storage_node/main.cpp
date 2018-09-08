#include <common/network.h>
#include <common/transaction.h>
#include <common/log.h>
#include <protocol/all.h>

#include "core.h"
#include "transaction.h"


namespace spaceless {
namespace storage_node {

static Logger& logger = LoggerManager::instance()->register_logger("storage_node");

int main(int argc, const char* argv[])
{
	try
	{
		LoggerManager::instance()->for_each([](const std::string& name, Logger* logger) {
			logger->set_level(lights::LogLevel::DEBUG);
		});

		if (argc < 4)
		{
			LIGHTS_ERROR(logger, "Not enought argumets to start up");
			return -1;
		}

		std::string sharing_path = argv[1];
		std::string ip = argv[2];
		unsigned short port = static_cast<unsigned short>(std::stoi(argv[3]));

		SharingFileManager::instance()->set_sharing_path(sharing_path);
		NetworkConnectionManager::instance()->register_listener(ip, port);

		SPACE_REGISTER_ONE_PHASE_TRANSACTION(protocol::ReqPutFile, transaction::on_put_file)
		SPACE_REGISTER_ONE_PHASE_TRANSACTION(protocol::ReqGetFile, transaction::on_get_file)

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
