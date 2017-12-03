#include <iostream>
#include <thread>

#include <lights/ostream.h>
#include <protocol/all.h>

#include "common/network.h"
#include "common/log.h"
#include "protocol/protocol.pb.h"
#include "client.h"


namespace spaceless {

const int MODULE_CLIENT = 1100;

namespace client {

void read_handler(spaceless::Connection& conn, const spaceless::PackageBuffer& package)
{
	switch (package.header().command)
	{
		case RSP_REGISTER_USER:
		{
			protocol::RspRegisterUser rsponse;
			package.parse_as_protobuf(rsponse);
			if (rsponse.result())
			{
				std::cout << lights::format("Register failure {}.", rsponse.result()) << std::endl;
			}
			else
			{
				std::cout << lights::format("Your uid is {}.", rsponse.user().uid()) << std::endl;
			}
			break;
		}
		case RSP_LOGIN_USER:
		{
			protocol::RspLoginUser rsponse;
			package.parse_as_protobuf(rsponse);
			std::cout << lights::format("Login result:{}.", rsponse.result()) << std::endl;
			break;
		}
		case RSP_REMOVE_USER:
		{
			protocol::RspRemoveUser rsponse;
			package.parse_as_protobuf(rsponse);
			std::cout << lights::format("Remove result:{}.", rsponse.result()) << std::endl;
			break;
		}
		default:break;
	}
}


int main(int argc, const char* argv[])
{
	try
	{
		std::pair<int, CommandHandlerManager::CommandHandler> handlers[] = {
			{RSP_REGISTER_USER, read_handler},
			{RSP_LOGIN_USER, read_handler},
			{RSP_REMOVE_USER, read_handler},
		};

		for (std::size_t i = 0; i < lights::size_of_array(handlers); ++i)
		{
			CommandHandlerManager::instance()->register_command(handlers[i].first, handlers[i].second);
		}

		conn = &ConnectionManager::instance()->register_connection("127.0.0.1", 10240);

		// Must run after have a event in loop. Just after reading.
		std::thread thread([]() {
			ConnectionManager::instance()->run();
		});
		thread.detach();

		while (true)
		{
			std::string func;
			std::cin >> func;
			if (func == "register_user")
			{
				std::string username;
				std::string password;
				std::cout << lights::format("Please input username and password.\n");
				std::cin >> username >> password;
				UserManager::instance()->register_user(username, password);
			}
			else if (func == "login_user")
			{
				int uid;
				std::string password;
				std::cout << lights::format("Please input uid and password.\n");
				std::cin >> uid >> password;
				UserManager::instance()->login_user(uid, password);
			}
			else if (func == "remove_user")
			{
				int uid;
				std::string password;
				std::cout << lights::format("Please input uid.\n");
				std::cin >> uid;
				UserManager::instance()->remove_user(uid);
			}
		}
	}
	catch (const Exception& ex)
	{
		SPACELESS_ERROR(MODULE_CLIENT, ex);
	}
	catch (const std::exception& ex)
	{
		SPACELESS_ERROR(MODULE_CLIENT, ex.what());
	}
	return 0;
}

} // namespace client
} // namespace spaceless


int main(int argc, const char* argv[])
{
	return spaceless::client::main(argc, argv);
}
