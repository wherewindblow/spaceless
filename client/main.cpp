#include <iostream>
#include <thread>

#include <foundation/network.h>
#include <foundation/transaction.h>
#include <foundation/scheduler.h>
#include <foundation/log.h>
#include <foundation/configuration.h>
#include <protocol/all.h>

#include <lights/ostream.h>

#include "core.h"


namespace spaceless {

namespace client {

static Logger& logger = get_logger("client");

void read_handler(int conn_id, Package package);

using ConnectionList = std::vector<int>;

void cmd_ui_interface(ConnectionList& conn_list);


int main(int argc, const char* argv[])
{
	try
	{
		Configuration::PathList path_list = {"../configuration/client_conf.json", "../configuration/global_conf.json"};
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

		SPACELESS_REG_ONE_TRANS(protocol::RspPing, read_handler);
		SPACELESS_REG_ONE_TRANS(protocol::RspRegisterUser, read_handler);
		SPACELESS_REG_ONE_TRANS(protocol::RspLoginUser, read_handler);
		SPACELESS_REG_ONE_TRANS(protocol::RspRemoveUser, read_handler);
		SPACELESS_REG_ONE_TRANS(protocol::RspFindUser, read_handler);
		SPACELESS_REG_ONE_TRANS(protocol::RspRegisterGroup, read_handler);
		SPACELESS_REG_ONE_TRANS(protocol::RspRemoveGroup, read_handler);
		SPACELESS_REG_ONE_TRANS(protocol::RspFindGroup, read_handler);
		SPACELESS_REG_ONE_TRANS(protocol::RspJoinGroup, read_handler);
		SPACELESS_REG_ONE_TRANS(protocol::RspAssignAsManager, read_handler);
		SPACELESS_REG_ONE_TRANS(protocol::RspAssignAsMemeber, read_handler);
		SPACELESS_REG_ONE_TRANS(protocol::RspKickOutUser, read_handler);
		SPACELESS_REG_ONE_TRANS(protocol::RspPutFileSession, read_handler);
		SPACELESS_REG_ONE_TRANS(protocol::RspPutFile, read_handler);
		SPACELESS_REG_ONE_TRANS(protocol::RspGetFileSession, read_handler);
		SPACELESS_REG_ONE_TRANS(protocol::RspGetFile, read_handler);
		SPACELESS_REG_ONE_TRANS(protocol::RspCreatePath, read_handler);
		SPACELESS_REG_ONE_TRANS(protocol::RspRemovePath, read_handler);

		NetworkConnection conn = NetworkManager::instance()->register_connection("127.0.0.1", 10240);
		conn_id = conn.connection_id();
		ConnectionList conn_list;
		conn_list.push_back(conn.connection_id());

		// Must run after have a event in loop. Just after reading.
		std::thread thread([]() {
			Scheduler::instance()->start();
		});
		thread.detach();

		UserManager::instance()->login_user(1, "pwd");

		DelayTesting::instance()->start_testing();

		cmd_ui_interface(conn_list);
	}
	catch (Exception& ex)
	{
		LIGHTS_ERROR(logger, ex);
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
		else if (func == "assign_as_manager")
		{
			int group_id, user_id;
			std::cout << "Please input group id and user id." << std::endl;
			std::cin >> group_id >> user_id;
			SharingGroupManager::instance()->assign_as_manager(group_id, user_id);
		}
		else if (func == "assign_as_memeber")
		{
			int group_id, user_id;
			std::cout << "Please input group id and user id." << std::endl;
			std::cin >> group_id >> user_id;
			SharingGroupManager::instance()->assign_as_memeber(group_id, user_id);
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
			std::cout << "Please input group id, local file path and remote file path." << std::endl;
			int group_id;
			std::string local_file_path, remote_file_path;
			std::cin >> group_id >> local_file_path >> remote_file_path;
			SharingGroupManager::instance()->put_file(group_id, local_file_path, remote_file_path);
		}
		else if (func == "get_file")
		{
			std::cout << "Please input group id, remote file path and local file path." << std::endl;
			int group_id;
			std::string local_file_path, remote_file_path;
			std::cin >> group_id >> remote_file_path >> local_file_path;
			SharingGroupManager::instance()->get_file(group_id, remote_file_path, local_file_path);
		}
		else if (func == "create_path")
		{
			std::cout << "Please input group id and path." << std::endl;
			int group_id;
			std::string path;
			std::cin >> group_id >> path;
			SharingGroupManager::instance()->create_path(group_id, path);
		}
		else if (func == "remove_path")
		{
			std::cout << "Please input group id and path." << std::endl;
			int group_id;
			bool force_remove_all;
			std::string path;
			std::cin >> group_id >> path >> force_remove_all;
			SharingGroupManager::instance()->remove_path(group_id, path, force_remove_all);
		}
		else if (func == "register_connection")
		{
			std::cout << "Please input host and port." << std::endl;
			std::string host;
			unsigned short port;
			std::cin >> host >> port;
			NetworkConnection conn = NetworkManager::instance()->register_connection(host, port);
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
				NetworkConnection conn = NetworkManager::instance()->find_connection(id);
				if (!conn.is_valid())
				{
					std::cout << "Invalid network connection." << std::endl;
				}
				else
				{
					conn_id = conn.connection_id();
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

int cmd(const std::string& protocol_name)
{
	return protocol::get_command(protocol_name);
}

void read_handler(int conn_id, Package package)
{
	int command = package.header().base.command;

	protocol::RspError error;
	package.parse_to_protocol(error);
	if (error.result())
	{
		std::cout << lights::format("Failure {} by {}.", error.result(), command) << std::endl;
		return;
	}

	if (command == cmd("RspRegisterUser"))
	{
		protocol::RspRegisterUser response;
		package.parse_to_protocol(response);
		std::cout << lights::format("Your user id is {}.", response.user().user_id()) << std::endl;
	}
	else if (command == cmd("RspFindUser"))
	{
		protocol::RspFindUser response;
		package.parse_to_protocol(response);
		std::cout << lights::format("Your user id is {} and username is {}.",
									response.user().user_id(), response.user().user_name())
				  << std::endl;
	}
	else if (command == cmd("RspRegisterGroup"))
	{
		protocol::RspRegisterGroup response;
		package.parse_to_protocol(response);
		std::cout << lights::format("Group id is {}.", response.group_id()) << std::endl;
	}
	else if (command == cmd("RspFindGroup"))
	{
		protocol::RspFindGroup response;
		package.parse_to_protocol(response);
		std::string msg;
		auto sink = lights::make_format_sink(msg);

		lights::write(sink,
					  "group_id = {};\n"
						  "group_name = {};\n"
						  "owner_id = {};\n",
					  response.group().group_id(),
					  response.group().group_name(),
					  response.group().owner_id()
		);

		lights::write(sink, "manager_list = ");
		for (int i = 0; i < response.group().manager_list().size(); ++i)
		{
			if (i != 0)
			{
				lights::write(sink, ", ");
			}
			lights::write(sink, "{}", response.group().manager_list()[i]);
		}
		lights::write(sink, ";\n");

		lights::write(sink, "member_list = ");
		for (int i = 0; i < response.group().member_list().size(); ++i)
		{
			if (i != 0)
			{
				lights::write(sink, ", ");
			}
			lights::write(sink, "{}", response.group().member_list()[i]);
		}
		lights::write(sink, ";\n");

		std::cout << msg << std::endl;
	}
	else if (command == cmd("RspPutFileSession"))
	{
		protocol::RspPutFileSession response;
		package.parse_to_protocol(response);
		FileSession& session = SharingGroupManager::instance()->put_file_session();
		session.session_id = response.session_id();
		SharingGroupManager::instance()->start_put_file(response.next_fragment());
	}
	else if (command == cmd("RspPutFile"))
	{
		protocol::RspPutFile response;
		package.parse_to_protocol(response);
		FileSession& session = SharingGroupManager::instance()->put_file_session();
		if (response.fragment_index() + 1 >= session.max_fragment)
		{
			lights::PreciseTime use_sec = lights::current_precise_time() - session.start_time;
			std::cout << lights::format("Put file {} finish. use {}", session.remote_path, use_sec) << std::endl;
		}
		else
		{
//			session.fragment_state[]
//			++session.fragment_index;
//			SharingGroupManager::instance()->put_file(session.group_id,
//													  session.local_path,
//													  session.remote_path,
//													  session.fragment_index);
		}
	}
	else if (command == cmd("RspGetFileSession"))
	{
		protocol::RspGetFileSession response;
		package.parse_to_protocol(response);
		FileSession& session = SharingGroupManager::instance()->get_file_session();
		session.session_id = response.session_id();
		session.max_fragment = response.max_fragment();
		SharingGroupManager::instance()->start_get_file();
	}
	else if (command == cmd("RspGetFile"))
	{
		protocol::RspGetFile response;
		package.parse_to_protocol(response);
		FileSession& session = SharingGroupManager::instance()->get_file_session();
		lights::FileStream file(session.local_path, "a");
		int offset = response.fragment_index() * protocol::MAX_FRAGMENT_CONTENT_LEN;
		file.seek(offset, lights::FileSeekWhence::BEGIN);
		file.write({response.fragment_content()});

		if (response.fragment_index() + 1 < session.max_fragment)
		{
			SharingGroupManager::instance()->set_next_fragment(session.local_path, response.fragment_index() + 1);
		}
		else
		{
			lights::PreciseTime use_sec = lights::current_precise_time() - session.start_time;
			std::cout << lights::format("Get file {} finish. use {}", session.remote_path, use_sec) << std::endl;
		}
	}
	else if (command == cmd("RspPing"))
	{
		protocol::RspPing response;
		package.parse_to_protocol(response);
		DelayTesting::instance()->on_receive_response(response.second(), response.microsecond());
		LIGHTS_INFO(logger, "Delay last {}, average {}",
					DelayTesting::instance()->last_delay_time(),
					DelayTesting::instance()->average_delay_time());
	}
}

} // namespace client
} // namespace spaceless


int main(int argc, const char* argv[])
{
	return spaceless::client::main(argc, argv);
}
