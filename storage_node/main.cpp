#include <foundation/network.h>
#include <foundation/transaction.h>
#include <foundation/scheduler.h>
#include <foundation/log.h>
#include <foundation/configuration.h>
#include <protocol/all.h>
#include <lights/log_sinks/file_sink.h>

#include "core.h"
#include "transaction.h"


namespace spaceless {
namespace storage_node {

static Logger& logger = get_logger("storage_node");


int main(int argc, const char* argv[])
{
	try
	{
		Configuration::PathList path_list = {"../configuration/storage_node_conf.json", "../configuration/global_conf.json"};
		Configuration configuration(path_list);

		// Sets global log level of each logger.
		lights::LogLevel log_level = to_log_level(configuration.getString("log_level"));
		LoggerManager::instance()->for_each([&](const std::string& name, Logger& logger) {
			logger.set_level(log_level);
		});

		// Sets special log level of some logger.
		for (int i = 0;; ++i)
		{
			std::string key_prefix = "each_log_level[" + std::to_string(i) + "]";
			try
			{
				std::string logger_name = configuration.getString(key_prefix + ".logger_name");
				std::string level = configuration.getString(key_prefix + ".log_level");
				Logger& cur_logger = get_logger(logger_name);
				cur_logger.set_level(to_log_level(level));
			}
			catch (Poco::NotFoundException& e)
			{
				break;  // Into array end.
			}
		}

		if (argc < 4)
		{
			LIGHTS_ERROR(logger, "Not enought argumets to start up.");
			return -1;
		}

		std::string sharing_path = argv[1];
		std::string ip = argv[2];
		unsigned short port = static_cast<unsigned short>(std::stoi(argv[3]));

		SharingFileManager::instance()->set_sharing_path(sharing_path);
		NetworkManager::instance()->register_listener(ip, port, SecuritySetting::CLOSE_SECURITY);

		SPACELESS_REG_ONE_TRANS(protocol::ReqNodePutFileSession, transaction::on_put_file_session)
		SPACELESS_REG_ONE_TRANS(protocol::ReqPutFile, transaction::on_put_file)
		SPACELESS_REG_ONE_TRANS(protocol::ReqNodeGetFileSession, transaction::on_get_file_session)
		SPACELESS_REG_ONE_TRANS(protocol::ReqGetFile, transaction::on_get_file)

		Scheduler::instance()->start();
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
