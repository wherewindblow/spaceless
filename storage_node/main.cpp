#include <protocol/all.h>
#include <common/network.h>
#include <common/log.h>

#include "core.h"
#include "transcation.h"


namespace spaceless {
namespace storage_node {

int main(int argc, const char* argv[])
{
	try
	{
		logger.set_level(lights::LogLevel::DEBUG);

		if (argc < 4)
		{
			SPACELESS_ERROR(MODULE_STORAGE_NODE, "Not enought argumets to start up");
			return -1;
		}

		std::string sharing_path = argv[1];
		std::string ip = argv[2];
		unsigned short port = static_cast<unsigned short>(std::stoi(argv[3]));

		SharingFileManager::instance()->set_sharing_path(sharing_path);
		NetworkConnectionManager::instance()->register_listener(ip, port);

		std::pair<int, OnePhaseTrancation> handlers[] = {
			{protocol::REQ_PUT_FILE, transcation::on_put_file},
			{protocol::REQ_GET_FILE, transcation::on_get_file},
		};

		for (std::size_t i = 0; i < lights::size_of_array(handlers); ++i)
		{
			TranscationManager::instance()->register_one_phase_transcation(handlers[i].first, handlers[i].second);
		}

		NetworkConnectionManager::instance()->run();
	}
	catch (Exception& ex)
	{
		SPACELESS_ERROR(MODULE_STORAGE_NODE, ex);
	}
	catch (std::exception& ex)
	{
		SPACELESS_ERROR(MODULE_STORAGE_NODE, ex.what());
	}

	return 0;
}

} // namespace storage_node
} // namespace spaceless


int main(int argc, const char* argv[])
{
	return spaceless::storage_node::main(argc, argv);
}
