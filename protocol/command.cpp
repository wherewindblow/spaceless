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

	/**
	 * Creates command table.
	 */
	CommandTableImpl()
	{
		m_name_list = default_command_name_map;
		for (auto& pair: m_name_list)
		{
			m_cmd_list.insert(std::make_pair(pair.second, pair.first));
		}
	}

	/**
	 * Finds name.
	 * @note Returns nullptr if cannot it.
	 */
	const std::string* find_name(int cmd)
	{
		auto itr = m_name_list.find(cmd);
		if (itr == m_name_list.end())
		{
			return nullptr;
		}
		return &itr->second;
	}

	/**
	 * Gets name.
	 * @throw Throws exception if cannot find it.
	 */
	const std::string& get_name(int cmd)
	{
		auto name = find_name(cmd);
		if (!name)
		{
			LIGHTS_THROW_EXCEPTION(Exception, ERR_PROTOCOL_PROTOBUF_NAME_NOT_EXIST);
		}
		return *name;
	}

	/**
	 * Finds command.
	 * @note Returns nullptr if cannot it.
	 */
	const int* find_command(const std::string& name)
	{
		auto itr = m_cmd_list.find(name);
		if (itr == m_cmd_list.end())
		{
			return nullptr;
		}
		return &itr->second;
	}

	/**
	 * Gets command.
	 * @throw Throws exception if cannot find it.
	 */
	int get_command(const std::string& protobuf_name)
	{
		auto cmd = find_command(protobuf_name);
		if (!cmd)
		{
			LIGHTS_THROW_EXCEPTION(Exception, ERR_PROTOCOL_COMMAND_NOT_EXIST);
		}
		return *cmd;
	}

private:
	std::map<int, std::string> m_name_list;
	std::map<std::string, int> m_cmd_list;
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
	return details::CommandTableImpl::instance()->get_name(cmd);
}

} // namespace protocol
} // namespace spaceless
