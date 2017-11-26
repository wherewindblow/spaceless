/*
 * resource_server.cpp
 * @author wherewindblow
 * @date   Oct 26, 2017
 */

#include "resource_server.h"

#include <algorithm>
#include <utility>

#include <boost/algorithm/string/split.hpp>
#include <lights/exception.h>

#include "common/exception.h"


namespace lights {

inline bool operator==(StringView lhs, StringView rhs)
{
	return lhs.length() == rhs.length() && std::strncmp(lhs.data(), rhs.data(), lhs.length()) == 0;
}

inline bool operator!=(StringView lhs, StringView rhs)
{
	return !(lhs == rhs);
}

} // namespace lights


namespace spaceless {
namespace resource_server {

User& UserManager::register_user(lights::StringView username, lights::StringView password)
{
	User* old_user = find_user(username);
	if (old_user)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_USER_ALREADY_EXIST);
	}

	User new_user = { m_next_id, username.to_std_string(), password.to_std_string() };
	++m_next_id;
	auto itr = m_user_list.insert(std::make_pair(new_user.uid, new_user));
	if (itr.second == false)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_USER_CANNOT_REGISTER);
	}

	return itr.first->second;
}


bool UserManager::login_user(int uid, lights::StringView password, Connection& conn)
{
	User* user = find_user(uid);
	if (!user)
	{
		return false;
	}

	if (user->password != password)
	{
		return false;
	}

	user->conn_id = conn.connection_id();
	conn.set_attachment(&user);
	return true;
}


void UserManager::remove_user(int uid)
{
	m_user_list.erase(uid);
}


User* UserManager::find_user(int uid)
{
	auto itr = m_user_list.find(uid);
	if (itr == m_user_list.end())
	{
		return nullptr;
	}

	return &(itr->second);
}


User* UserManager::find_user(lights::StringView username)
{
	auto itr = std::find_if(m_user_list.begin(), m_user_list.end(),
							[&username](const UserList::value_type& value_type)
	{
		return value_type.second.username == username;
	});

	if (itr == m_user_list.end())
	{
		return nullptr;
	}

	return &(itr->second);
}


User& UserManager::get_user(int uid)
{
	User* user = find_user(uid);
	if (user == nullptr)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_USER_NOT_EXIST);
	}
	return *user;
}


User& UserManager::get_user(lights::StringView username)
{
	User* user = find_user(username);
	if (user == nullptr)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_USER_NOT_EXIST);
	}
	return *user;
}


SharingGroup::SharingGroup(int group_id, lights::StringView group_name, int ower_id, int root_dir_id):
	m_group_id(group_id),
	m_group_name(group_name.to_std_string()),
	m_owner_id(ower_id),
	m_root_dir_id(root_dir_id)
{
	m_manager_list.push_back(ower_id);
}


int SharingGroup::group_id() const
{
	return m_group_id;
}


lights::StringView SharingGroup::group_name() const
{
	return m_group_name;
}


int SharingGroup::owner_id() const
{
	return m_owner_id;
}


void SharingGroup::put_file(int uid, lights::StringView target_name, lights::SequenceView file_content)
{
	// TODO
}


void SharingGroup::get_file(int uid, lights::StringView target_name, lights::Sequence file_content)
{
	// TODO
}


void SharingGroup::create_directory(int uid, lights::StringView full_dir_path)
{
	auto manager_itr = std::find(m_manager_list.begin(), m_manager_list.end(), uid);
	if (manager_itr == m_manager_list.end())
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_GROUP_ONLY_MANAGER_CAN_CREATE_DIR);
	}

	std::vector<std::string> result;
	std::string input = full_dir_path.to_std_string();
	boost::split(result, input, [](char ch)
	{
		return ch == '/';
	});

	int parent_dir_id = m_root_dir_id;
	for (auto& dir_name: result)
	{
		SharingFile* parent_dir = SharingFileManager::instance()->find_file(parent_dir_id);
		if (parent_dir->file_type != SharingFile::DIRECTORY)
		{
			LIGHTS_THROW_EXCEPTION(Exception, ERR_GROUP_CREATE_DIR_MUST_UNDER_DIR);
		}

		auto itr = std::find_if(parent_dir->inside_file_list.begin(), parent_dir->inside_file_list.end(),
					 [&dir_name](int file_id)
		{
			SharingFile* file = SharingFileManager::instance()->find_file(file_id);
			if (file && file->file_name == dir_name)
			{
				if (file->file_type == SharingFile::DIRECTORY)
				{
					return true;
				}
				else
				{
					LIGHTS_THROW_EXCEPTION(Exception, ERR_GROUP_CREATE_DIR_MUST_UNDER_DIR);
				}
			}

			return false;
		});

		if (itr == parent_dir->inside_file_list.end())
		{
			SharingFile& new_file = SharingFileManager::instance()->register_file(SharingFile::DIRECTORY, dir_name);
			parent_dir->inside_file_list.push_back(new_file.file_id);
			parent_dir_id = new_file.file_id;
		}
		else
		{
			parent_dir_id = *itr;
		}
	}
}


void SharingGroup::remove_directory(int uid, lights::StringView full_dir_path)
{
	auto manager_itr = std::find(m_manager_list.begin(), m_manager_list.end(), uid);
	if (manager_itr == m_manager_list.end())
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_GROUP_ONLY_MANAGER_CAN_REMOVE_DIR);
	}

	std::vector<std::string> result;
	std::string input = full_dir_path.to_std_string();
	boost::split(result, input, [](char ch)
	{
		return ch == '/';
	});

	int parent_dir_id = m_root_dir_id;
	for (auto& dir_name: result)
	{
		SharingFile* parent_dir = SharingFileManager::instance()->find_file(parent_dir_id);
		if (parent_dir->file_type != SharingFile::DIRECTORY)
		{
			LIGHTS_THROW_EXCEPTION(Exception, ERR_GROUP_REMOVE_DIR_MUST_UNDER_DIR);
		}

		auto itr = std::find_if(parent_dir->inside_file_list.begin(), parent_dir->inside_file_list.end(),
								[&dir_name](int file_id)
		{
			SharingFile* file = SharingFileManager::instance()->find_file(file_id);
			if (file && file->file_name == dir_name)
			{
				if (file->file_type == SharingFile::DIRECTORY)
				{
					return true;
				}
				else
				{
					LIGHTS_THROW_EXCEPTION(Exception, ERR_GROUP_REMOVE_DIR_MUST_UNDER_DIR);
				}
			}

			return false;
		});

		if (itr == parent_dir->inside_file_list.end())
		{
			LIGHTS_THROW_EXCEPTION(Exception, ERR_FILE_NOT_EXIST);
		}
		else
		{
			parent_dir_id = *itr;
		}
	}

	if (parent_dir_id == m_root_dir_id)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_GROUP_CANNOT_REMOVE_ROOT_DIR);
	}

	SharingFileManager::instance()->remove_file(parent_dir_id);
}


void SharingGroup::join_group(int uid)
{
	auto itr = std::find(m_member_list.begin(), m_member_list.end(), uid);
	if (itr != m_member_list.end())
	{
		return;
	}

	itr = std::find(m_manager_list.begin(), m_manager_list.end(), uid);
	if (itr != m_manager_list.end())
	{
		return;
	}

	m_member_list.push_back(uid);
}


void SharingGroup::kick_out(int uid)
{
	if (uid == owner_id())
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_GROUP_CANNOT_KICK_OUT_OWNER);
	}

	auto itr = std::find(m_member_list.begin(), m_member_list.end(), uid);
	if (itr != m_member_list.end())
	{
		m_member_list.erase(itr);
	}

	itr = std::find(m_manager_list.begin(), m_manager_list.end(), uid);
	if (itr != m_manager_list.end())
	{
		m_manager_list.erase(itr);
	}
}


SharingGroup& SharingGroupManager::register_group(int uid, lights::StringView group_name)
{
	SharingGroup* old_group = find_group(group_name);
	if (old_group)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_GROUP_ALREADY_EXIST);
	}

	SharingFile& root_dir = SharingFileManager::instance()->register_file(SharingFile::DIRECTORY, group_name);
	SharingGroup new_group(m_next_id, group_name, uid, root_dir.file_id);
	++m_next_id;

	auto itr = m_group_list.insert(std::make_pair(new_group.group_id(), new_group));
	if (itr.second)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_GROUP_CANNOT_REGISTER);
	}

	return itr.first->second;
}


void SharingGroupManager::remove_group(int uid, int group_id)
{
	SharingGroup& group = get_group(group_id);
	if (group.owner_id() != uid)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_GROUP_ONLY_ONWER_CAN_REMOVE);
	}

	m_group_list.erase(group_id);
}


SharingGroup* SharingGroupManager::find_group(int group_id)
{
	auto itr = m_group_list.find(group_id);
	if (itr == m_group_list.end())
	{
		return nullptr;
	}
	else
	{
		return &(itr->second);
	}
}


SharingGroup* SharingGroupManager::find_group(lights::StringView group_name)
{
	auto itr = std::find_if(m_group_list.begin(), m_group_list.end(),
							[&group_name](const GroupList::value_type& value_type)
	{
		return value_type.second.group_name() == group_name;
	});

	if (itr == m_group_list.end())
	{
		return nullptr;
	}

	return &(itr->second);
}

SharingGroup& SharingGroupManager::get_group(int group_id)
{
	SharingGroup* group = find_group(group_id);
	if (group == nullptr)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_GROUP_NOT_EXIST);
	}
	return *group;
}

SharingGroup& SharingGroupManager::get_group(lights::StringView group_name)
{
	SharingGroup* group = find_group(group_name);
	if (group == nullptr)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_GROUP_NOT_EXIST);
	}
	return *group;
}


SharingFile& SharingFileManager::register_file(SharingFile::FileType file_type,
											   lights::StringView file_name,
											   int node_id,
											   lights::StringView node_file_name)
{
	StorageFile storage_file = std::make_pair(node_id, node_file_name.to_std_string());
	auto old_itr = m_storage_file_list.find(storage_file);
	if (old_itr != m_storage_file_list.end())
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_FILE_ALREADY_EXIST);
	}

	SharingFile sharing_file = {
		m_next_id,
		file_type,
		file_name.to_std_string(),
		node_id,
		node_file_name.to_std_string()
	};
	++m_next_id;

	auto new_itr = m_sharing_file_list.insert(std::make_pair(sharing_file.file_id, sharing_file));
	if (new_itr.second == false)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_FILE_CANNOT_CREATE);
	}

	m_storage_file_list.insert(std::make_pair(storage_file, sharing_file.file_id));

	return new_itr.first->second;
}


void SharingFileManager::remove_file(int file_id)
{
	SharingFile& sharing_file = get_file(file_id);
	StorageFile storage_file = std::make_pair(sharing_file.node_id, sharing_file.node_file_name);
	m_storage_file_list.erase(storage_file);
	m_sharing_file_list.erase(file_id);
}


SharingFile* SharingFileManager::find_file(int file_id)
{
	auto itr = m_sharing_file_list.find(file_id);
	if (itr == m_sharing_file_list.end())
	{
		return nullptr;
	}
	return &(itr->second);
}


SharingFile* SharingFileManager::find_file(int node_id, lights::StringView node_file_name)
{
	StorageFile storage_file = std::make_pair(node_id, node_file_name.to_std_string());
	auto itr = m_storage_file_list.find(storage_file);
	if (itr == m_storage_file_list.end())
	{
		return nullptr;
	}

	return find_file(itr->second);
}


SharingFile& SharingFileManager::get_file(int file_id)
{
	SharingFile* file = find_file(file_id);
	if (file == nullptr)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_FILE_NOT_EXIST);
	}
	return *file;
}


SharingFile& SharingFileManager::get_file(int node_id, lights::StringView node_file_name)
{
	SharingFile* file = find_file(node_id, node_file_name);
	if (file == nullptr)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_FILE_NOT_EXIST);
	}
	return *file;
}


StorageNode& StorageNodeManager::register_node(lights::StringView node_ip, short node_port)
{
	StorageNode* old_node = find_node(node_ip, node_port);
	if (old_node)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_NODE_ALREADY_EXIST);
	}

	StorageNode new_node = { m_next_id, node_ip.to_std_string(), node_port };
	auto itr = m_node_list.insert(std::make_pair(new_node.node_id, new_node));
	if (itr.second == false)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_NODE_CANNOT_CREATE);
	}

	return itr.first->second;
}


void StorageNodeManager::remove_node(int node_id)
{
	m_node_list.erase(node_id);
}


StorageNode* StorageNodeManager::find_node(int node_id)
{
	auto itr = m_node_list.find(node_id);
	if (itr == m_node_list.end())
	{
		return nullptr;
	}

	return &(itr->second);
}


StorageNode* StorageNodeManager::find_node(lights::StringView node_ip, short node_port)
{
	auto itr = std::find_if(m_node_list.begin(), m_node_list.end(),
							[&](const StorageNodeList::value_type& value_type)
	{
		return value_type.second.node_ip == node_ip && value_type.second.node_port;
	});

	if (itr == m_node_list.end())
	{
		return nullptr;
	}

	return &(itr->second);
}


StorageNode& StorageNodeManager::get_node(int node_id)
{
	StorageNode* node = find_node(node_id);
	if (node == nullptr)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_NODE_NOT_EXIST);
	}
	return *node;
}


StorageNode& StorageNodeManager::get_node(lights::StringView node_ip, short node_port)
{
	StorageNode* node = find_node(node_ip, node_port);
	if (node == nullptr)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_NODE_NOT_EXIST);
	}
	return *node;
}

} // namespace resource_server
} // namespace spaceless
