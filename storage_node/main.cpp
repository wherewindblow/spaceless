#include <iostream>

#include <lights/ostream.h>
#include <lights/file.h>
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

		NetworkConnectionManager::instance()->register_listener("127.0.0.1", 10241);

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
