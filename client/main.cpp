#include <iostream>
#include <thread>

#include <lights/ostream.h>
#include <protocol/all.h>
#include <common/network.h>
#include <common/log.h>

#include "core.h"


namespace spaceless {

const int MODULE_CLIENT = 1100;

namespace client {

void report_error(int result)
{
	if (result)
	{
		std::cout << lights::format("Failure {}.", result) << std::endl;
	}
}


void read_handler(NetworkConnection& conn, const PackageBuffer& package);

using ConnectionList = std::vector<int>;

void cmd_ui_interface(ConnectionList& conn_list);


int main(int argc, const char* argv[])
{
	try
	{
		logger.set_level(lights::LogLevel::DEBUG);

		std::pair<int, OnePhaseTrancation> handlers[] = {
			{protocol::RSP_REGISTER_USER, read_handler},
			{protocol::RSP_LOGIN_USER, read_handler},
			{protocol::RSP_REMOVE_USER, read_handler},
			{protocol::RSP_FIND_USER, read_handler},
			{protocol::RSP_REGISTER_GROUP, read_handler},
			{protocol::RSP_REMOVE_GROUP, read_handler},
			{protocol::RSP_FIND_GROUP, read_handler},
			{protocol::RSP_JOIN_GROUP, read_handler},
			{protocol::RSP_KICK_OUT_USER, read_handler},
			{protocol::RSP_PUT_FILE, read_handler},
			{protocol::RSP_GET_FILE, read_handler},
		};

		for (std::size_t i = 0; i < lights::size_of_array(handlers); ++i)
		{
			TranscationManager::instance()->register_one_phase_transcation(handlers[i].first, handlers[i].second);
		}

		network_conn = &NetworkConnectionManager::instance()->register_connection("127.0.0.1", 10240);
		ConnectionList conn_list;
		conn_list.push_back(network_conn->connection_id());

		// Must run after have a event in loop. Just after reading.
		std::thread thread([]() {
			NetworkConnectionManager::instance()->run();
		});
		thread.detach();

		cmd_ui_interface(conn_list);
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


void cmd_ui_interface(ConnectionList& conn_list)
{
	while (true)
	{
		std::string func;
		std::cin.clear();
		std::cin >> func;
		if (func == "register_user")
		{
			std::string username;
			std::string password;
			std::cout << "Please input username and password" << std::endl;
			std::cin >> username >> password;
			UserManager::instance()->register_user(username, password);
		}
		else if (func == "login_user")
		{
			int user_id;
			std::string password;
			std::cout << "Please input user id and password." << std::endl;
			std::cin >> user_id >> password;
			UserManager::instance()->login_user(user_id, password);
		}
		else if (func == "remove_user")
		{
			int user_id;
			std::cout << "Please input user_id." << std::endl;
			std::cin >> user_id;
			UserManager::instance()->remove_user(user_id);
		}
		else if (func == "find_user")
		{
			std::string input;
			std::cout << "Please input user id or username." << std::endl;
			std::cin >> input;
			try
			{
				int user_id = stoi(input);
				UserManager::instance()->find_user(user_id);
			}
			catch (const std::exception&)
			{
				UserManager::instance()->find_user(input); // Make input as username.
			}
		}
		else if (func == "register_group")
		{
			std::string group_name;
			std::cout << "Please input group name." << std::endl;
			std::cin >> group_name;
			SharingGroupManager::instance()->register_group(group_name);
		}
		else if (func == "remove_group")
		{
			int group_id;
			std::cout << "Please input group id." << std::endl;
			std::cin >> group_id;
			SharingGroupManager::instance()->remove_group(group_id);
		}
		else if (func == "find_group")
		{
			std::string input;
			std::cout << "Please input group id or group name." << std::endl;
			std::cin >> input;
			try
			{
				int group_id = stoi(input);
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
			std::cout << "Please input group id." << std::endl;
			std::cin >> group_id;
			SharingGroupManager::instance()->join_group(group_id);
		}
		else if (func == "kick_out_user")
		{
			int group_id, user_id;
			std::cout << "Please input group id and user id." << std::endl;
			std::cin >> group_id >> user_id;
			SharingGroupManager::instance()->kick_out_user(group_id, user_id);
		}
		else if (func == "put_file")
		{
			std::cout << "Please input group id, local filename and remote filename." << std::endl;
			int group_id;
			std::string local_filename, remote_filename;
			std::cin >> group_id >> local_filename >> remote_filename;
			SharingGroupManager::instance()->put_file(group_id, local_filename, remote_filename);
		}
		else if (func == "get_file")
		{
			std::cout << "Please input group id, local filename and remote filename." << std::endl;
			int group_id;
			std::string local_filename, remote_filename;
			std::cin >> group_id >> remote_filename >> local_filename;
			SharingGroupManager::instance()->get_file(group_id, remote_filename, local_filename);
		}
		else if (func == "register_connection")
		{
			std::cout << "Please input host and port." << std::endl;
			std::string host;
			unsigned short port;
			std::cin >> host >> port;
			NetworkConnection& conn = NetworkConnectionManager::instance()->register_connection(host, port);
			conn_list.push_back(conn.connection_id());
			std::cout << lights::format("New connection index is {}.", conn_list.size() - 1) << std::endl;
		}
		else if (func == "switch_connection")
		{
			std::cout << "Please input connection index." << std::endl;
			std::size_t index;
			std::cin >> index;
			try
			{
				int id = conn_list.at(index);
				NetworkConnection* conn = NetworkConnectionManager::instance()->find_connection(id);
				if (conn == nullptr)
				{
					std::cout << "Invalid network connection." << std::endl;
				}
				else
				{
					network_conn = conn;
				}
			}
			catch (std::out_of_range& e)
			{
				std::cout << e.what() << std::endl;
			}
		}
		else if (func == "quit")
		{
			break;
		}
		else
		{
			std::cout << "Unkown command, please input again." << std::endl;
		}
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
				report_error(rsponse.result());
			}
			else
			{
				std::cout << lights::format("Your user id is {}.", rsponse.user().user_id()) << std::endl;
			}
			break;
		}
		case protocol::RSP_LOGIN_USER:
		{
			protocol::RspLoginUser rsponse;
			package.parse_as_protobuf(rsponse);
			if (rsponse.result())
			{
				report_error(rsponse.result());
			}
			break;
		}
		case protocol::RSP_REMOVE_USER:
		{
			protocol::RspRemoveUser rsponse;
			package.parse_as_protobuf(rsponse);
			if (rsponse.result())
			{
				report_error(rsponse.result());
			}
			break;
		}
		case protocol::RSP_FIND_USER:
		{
			protocol::RspFindUser rsponse;
			package.parse_as_protobuf(rsponse);
			if (rsponse.result())
			{
				report_error(rsponse.result());
			}
			else
			{
				std::cout << lights::format("Your user id is {} and username is {}.",
											rsponse.user().user_id(), rsponse.user().username())
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
				report_error(rsponse.result());
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
				report_error(rsponse.result());
			}
			break;
		}
		case protocol::RSP_FIND_GROUP:
		{
			protocol::RspFindGroup rsponse;
			package.parse_as_protobuf(rsponse);
			if (rsponse.result())
			{
				report_error(rsponse.result());
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
				report_error(rsponse.result());
			}
			break;
		}
		case protocol::RSP_KICK_OUT_USER:
		{
			protocol::RspKickOutUser rsponse;
			package.parse_as_protobuf(rsponse);
			if (rsponse.result())
			{
				report_error(rsponse.result());
			}
			break;
		}
		case protocol::RSP_PUT_FILE:
		{
			protocol::RspPutFile rsponse;
			package.parse_as_protobuf(rsponse);
			if (rsponse.result())
			{
				report_error(rsponse.result());
			}
			else
			{
				FileTransferSession& session = SharingGroupManager::instance()->putting_file_session();
				if (session.fragment_index + 1 >= session.max_fragment)
				{
					std::cout << lights::format("Put file {} finish.", session.remote_filename) << std::endl;
				}
				else
				{
					++session.fragment_index;
					SharingGroupManager::instance()->put_file(session.group_id,
															  session.local_filename,
															  session.remote_filename,
															  session.fragment_index);
				}
			}
			break;
		}
		case protocol::RSP_GET_FILE:
		{
			protocol::RspGetFile rsponse;
			package.parse_as_protobuf(rsponse);
			if (rsponse.result())
			{
				report_error(rsponse.result());
			}
			else
			{
				FileTransferSession& session = SharingGroupManager::instance()->getting_file_session();
				lights::FileStream file(session.local_filename, "a");
				int offset = rsponse.fragment().fragment_index() * protocol::MAX_FRAGMENT_CONTENT_LEN;
				file.seek(offset, lights::FileSeekWhence::BEGIN);
				file.write({rsponse.fragment().fragment_content()});

				if (rsponse.fragment().fragment_index() + 1 < rsponse.fragment().max_fragment())
				{
					SharingGroupManager::instance()->get_file(session.group_id,
															  session.remote_filename,
															  session.local_filename);
				}
				else
				{
					std::cout << lights::format("Get file {} finish.", session.remote_filename) << std::endl;
				}
			}
			break;
		}
		default:
			break;
	}
}

} // namespace client
} // namespace spaceless


int main(int argc, const char* argv[])
{
	return spaceless::client::main(argc, argv);
}
