/**
 * command.cpp
 * @author wherewindblow
 * @date   Jun 12, 2018
 */


#include "command.h"

#include <map>
#include <common/basics.h>
#include <common/exception.h>

#include "command.details"


namespace spaceless {
namespace protocol {
namespace details {

class CommandTableImpl
{
public:
	SPACELESS_SINGLETON_INSTANCE(CommandTableImpl);

	CommandTableImpl()
	{
		m_cmd_name_map = default_command_name_map;
		for (auto& pair: m_cmd_name_map)
		{
			m_name_cmd_map.insert(std::make_pair(pair.second, pair.first));
		}
	}

	const std::string* find_name(int cmd)
	{
		auto itr = m_cmd_name_map.find(cmd);
		if (itr == m_cmd_name_map.end())
		{
			return nullptr;
		}
		return &itr->second;
	}

	const int* find_command(const std::string& name)
	{
		auto itr = m_name_cmd_map.find(name);
		if (itr == m_name_cmd_map.end())
		{
			return nullptr;
		}
		return &itr->second;
	}

	int get_command(const std::string& protobuf_name)
	{
		auto cmd = find_command(protobuf_name);
		if (!cmd)
		{
			LIGHTS_THROW_EXCEPTION(Exception, ERR_COMMAND_NOT_EXIST);
		}
		return *cmd;
	}

	const std::string& get_protobuf_name(int cmd)
	{
		auto name = find_protobuf_name(cmd);
		if (!name)
		{
			LIGHTS_THROW_EXCEPTION(Exception, ERR_PROTOBUF_NAME_NOT_EXIST);
		}
		return *name;
	}

private:
	std::map<int, std::string> m_cmd_name_map;
	std::map<std::string, int> m_name_cmd_map;
};

} // namespace details



const int* find_command(const std::string& protobuf_name)
{
	return details::CommandTableImpl::instance()->find_command(protobuf_name);
}

const std::string* find_protobuf_name(int cmd)
{
	return details::CommandTableImpl::instance()->find_name(cmd);
}

int get_command(const std::string& protobuf_name)
{
	return details::CommandTableImpl::instance()->get_command(protobuf_name);
}

const std::string& get_protobuf_name(int cmd)
{
	return details::CommandTableImpl::instance()->get_protobuf_name(cmd);
}

} // namespace protocol
} // namespace spaceless
