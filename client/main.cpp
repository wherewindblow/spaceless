#include <iostream>
#include <thread>

#include <lights/ostream.h>
#include <protocol/all.h>
#include <common/network.h>
#include <common/log.h>

#include "client.h"


namespace spaceless {

const int MODULE_CLIENT = 1100;

namespace client {

template <typename ProtoBuffer>
void report_error(const ProtoBuffer& proto_buffer)
{
	if (proto_buffer.result())
	{
		std::cout << lights::format("Failure {}.", proto_buffer.result()) << std::endl;
	}
}


void read_handler(NetworkConnection& conn, const PackageBuffer& package)
{
	switch (package.header().command)
	{
		case protocol::RSP_REGISTER_USER:
		{
			protocol::RspRegisterUser rsponse;
			package.parse_as_protobuf(rsponse);
			if (rsponse.result())
			{
				report_error(rsponse);
			}
			else
			{
				std::cout << lights::format("Your uid is {}.", rsponse.user().uid()) << std::endl;
			}
			break;
		}
		case protocol::RSP_LOGIN_USER:
		{
			protocol::RspLoginUser rsponse;
			package.parse_as_protobuf(rsponse);
			if (rsponse.result())
			{
				report_error(rsponse);
			}
			break;
		}
		case protocol::RSP_REMOVE_USER:
		{
			protocol::RspRemoveUser rsponse;
			package.parse_as_protobuf(rsponse);
			if (rsponse.result())
			{
				report_error(rsponse);
			}
			break;
		}
		case protocol::RSP_FIND_USER:
		{
			protocol::RspFindUser rsponse;
			package.parse_as_protobuf(rsponse);
			if (rsponse.result())
			{
				report_error(rsponse);
			}
			else
			{
				std::cout << lights::format("Your uid is {} and username is {}.",
											rsponse.user().uid(), rsponse.user().username())
						  << std::endl;
			}
			break;
		}
		case protocol::RSP_REGISTER_GROUP:
		{
			protocol::RspRegisterGroup rsponse;
			package.parse_as_protobuf(rsponse);
			if (rsponse.result())
			{
				report_error(rsponse);
			}
			else
			{
				std::cout << lights::format("Group id is {}.", rsponse.group_id()) << std::endl;
			}
			break;
		}
		case protocol::RSP_REMOVE_GROUP:
		{
			protocol::RspRemoveGroup rsponse;
			package.parse_as_protobuf(rsponse);
			if (rsponse.result())
			{
				report_error(rsponse);
			}
			break;
		}
		case protocol::RSP_FIND_GROUP:
		{
			protocol::RspFindGroup rsponse;
			package.parse_as_protobuf(rsponse);
			if (rsponse.result())
			{
				report_error(rsponse);
			}
			else
			{
				std::string msg;
				auto sink = lights::make_format_sink_adapter(msg);

				lights::write(sink,
							  "group_id = {};\n"
								  "group_name = {};\n"
								  "owner_id = {};\n",
							  rsponse.group().group_id(),
							  rsponse.group().group_name(),
							  rsponse.group().owner_id()
				);

				lights::write(sink, "manager_list = ");
				for (int i = 0; i < rsponse.group().manager_list().size(); ++i)
				{
					if (i != 0)
					{
						lights::write(sink, ", ");
					}
					lights::write(sink, "{}", rsponse.group().manager_list()[i]);
				}
				lights::write(sink, ";\n");

				lights::write(sink, "member_list = ");
				for (int i = 0; i < rsponse.group().member_list().size(); ++i)
				{
					if (i != 0)
					{
						lights::write(sink, ", ");
					}
					lights::write(sink, "{}", rsponse.group().member_list()[i]);
				}
				lights::write(sink, ";\n");

				std::cout << msg << std::endl;
			}
			break;
		}
		case protocol::RSP_JOIN_GROUP:
		{
			protocol::RspJoinGroup rsponse;
			package.parse_as_protobuf(rsponse);
			if (rsponse.result())
			{
				report_error(rsponse);
			}
			break;
		}
		case protocol::RSP_KICK_OUT_USER:
		{
			protocol::RspKickOutUser rsponse;
			package.parse_as_protobuf(rsponse);
			if (rsponse.result())
			{
				report_error(rsponse);
			}
			break;
		}
		default:
			break;
	}
}


int main(int argc, const char* argv[])
{
	try
	{
		logger.set_level(lights::LogLevel::DEBUG);

		std::pair<int, CommandHandlerManager::CommandHandler> handlers[] = {
			{protocol::RSP_REGISTER_USER, read_handler},
			{protocol::RSP_LOGIN_USER, read_handler},
			{protocol::RSP_REMOVE_USER, read_handler},
			{protocol::RSP_FIND_USER, read_handler},
			{protocol::RSP_REGISTER_GROUP, read_handler},
			{protocol::RSP_REMOVE_GROUP, read_handler},
			{protocol::RSP_FIND_GROUP, read_handler},
			{protocol::RSP_JOIN_GROUP, read_handler},
			{protocol::RSP_KICK_OUT_USER, read_handler},
		};

		for (std::size_t i = 0; i < lights::size_of_array(handlers); ++i)
		{
			CommandHandlerManager::instance()->register_command(handlers[i].first, handlers[i].second);
		}

		network_conn = &NetworkConnectionManager::instance()->register_connection("127.0.0.1", 10240);

		// Must run after have a event in loop. Just after reading.
		std::thread thread([]() {
			NetworkConnectionManager::instance()->run();
		});
		thread.detach();

		while (true)
		{
			std::string func;
			std::cin.clear();
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
				std::cout << lights::format("Please input uid.\n");
				std::cin >> uid;
				UserManager::instance()->remove_user(uid);
			}
			else if (func == "find_user")
			{
				std::string input;
				std::cout << lights::format("Please input uid or username.\n");
				std::cin >> input;
				try
				{
					int uid = std::stoi(input);
					UserManager::instance()->find_user(uid);
				}
				catch (const std::exception&)
				{
					UserManager::instance()->find_user(input); // Make input as username.
				}
			}
			else if (func == "register_group")
			{
				std::string group_name;
				std::cout << lights::format("Please input group name.\n");
				std::cin >> group_name;
				SharingGroupManager::instance()->register_group(group_name);
			}
			else if (func == "remove_group")
			{
				int group_id;
				std::cout << lights::format("Please input group id.\n");
				std::cin >> group_id;
				SharingGroupManager::instance()->remove_group(group_id);
			}
			else if (func == "find_group")
			{
				std::string input;
				std::cout << lights::format("Please input group id or group name.\n");
				std::cin >> input;
				try
				{
					int group_id = std::stoi(input);
					SharingGroupManager::instance()->find_group(group_id);
				}
				catch (const std::exception&)
				{
					SharingGroupManager::instance()->find_group(input); // Make input as name.
				}
			}
			else if (func == "join_group")
			{
				int group_id;
				std::cout << lights::format("Please input group id.\n");
				std::cin >> group_id;
				SharingGroupManager::instance()->join_group(group_id);
			}
			else if (func == "kick_out_user")
			{
				int group_id, uid;
				std::cout << lights::format("Please input group id.\n");
				std::cin >> group_id >> uid;
				SharingGroupManager::instance()->kick_out_user(group_id, uid);
			}
		}
	}
	catch (Exception& ex)
	{
		SPACELESS_ERROR(MODULE_CLIENT, ex);
	}
	catch (std::exception& ex)
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
