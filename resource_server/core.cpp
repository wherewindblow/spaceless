/*
 * core.cpp
 * @author wherewindblow
 * @date   Oct 26, 2017
 */

#include "core.h"

#include <algorithm>
#include <utility>

#include <lights/file.h>
#include <foundation/log.h>
#include <foundation/exception.h>
#include <foundation/delegation.h>
#include <foundation/network.h>

#include <Poco/String.h>
#include <Poco/StringTokenizer.h>


namespace spaceless {
namespace resource_server {

static Logger& logger = get_logger("core");

User& UserManager::register_user(const std::string& username, const std::string& password)
{
	User* user = find_user(username);
	if (user != nullptr)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_USER_ALREADY_EXIST);
	}

	User new_user(m_next_id, username, password);
	++m_next_id;

	auto value = std::make_pair(new_user.user_id, new_user);
	auto result = m_user_list.insert(value);
	if (!result.second)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_USER_ALREADY_EXIST);
	}

	return result.first->second;
}


void UserManager::remove_user(int user_id)
{
	m_user_list.erase(user_id);
}


User* UserManager::find_user(int user_id)
{
	auto itr = m_user_list.find(user_id);
	if (itr == m_user_list.end())
	{
		return nullptr;
	}

	return &(itr->second);
}


User* UserManager::find_user(const std::string& user_name)
{
	auto itr = std::find_if(m_user_list.begin(), m_user_list.end(), [&user_name](const UserList::value_type& value_type)
	{
		return value_type.second.user_name == user_name;
	});

	if (itr == m_user_list.end())
	{
		return nullptr;
	}

	return &(itr->second);
}


User& UserManager::get_user(int user_id)
{
	User* user = find_user(user_id);
	if (user == nullptr)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_USER_NOT_EXIST);
	}
	return *user;
}


User& UserManager::get_user(const std::string& username)
{
	User* user = find_user(username);
	if (user == nullptr)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_USER_NOT_EXIST);
	}
	return *user;
}


bool UserManager::login_user(int user_id, const std::string& password, int conn_id)
{
	User* user = find_user(user_id);
	if (user == nullptr)
	{
		return false;
	}

	if (user->password != password)
	{
		return false;
	}

	user->conn_id = conn_id;
	m_login_user_list.insert(std::make_pair(conn_id, user_id));
	return true;
}


User* UserManager::find_login_user(int conn_id)
{
	auto itr = m_login_user_list.find(conn_id);
	if (itr == m_login_user_list.end())
	{
		return nullptr;
	}

	return find_user(itr->second);
}

User& UserManager::get_login_user(int conn_id)
{
	User* user = find_login_user(conn_id);
	if (user == nullptr)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_USER_NOT_LOGIN);
	}
	return *user;
}


const char FilePath::SEPARATOR[] = "/";

const char FilePath::ROOT[] = "";


FilePath::FilePath(const std::string& path):
	m_path(path)
{
	Poco::trimInPlace(m_path);
	Poco::replaceInPlace(m_path, "\\", SEPARATOR);
	Poco::replaceInPlace(m_path, "//", SEPARATOR);

	if (m_path.front() == SEPARATOR[0])
	{
		m_path.erase(0, 1);
	}

	if (m_path.back() == SEPARATOR[0])
	{
		m_path.resize(m_path.size() - 1);
	}
}


FilePath::SplitReult FilePath::split() const
{
	return SplitReult(new Poco::StringTokenizer(m_path, SEPARATOR));
}


std::string FilePath::directory_path() const
{
	std::size_t pos = m_path.rfind(SEPARATOR);
	if (pos != std::string::npos)
	{
		return m_path.substr(0, pos);
	}
	else
	{
		return ROOT;
	}
}


std::string FilePath::filename() const
{
	std::size_t pos = m_path.rfind(SEPARATOR);
	if (pos != std::string::npos)
	{
		return m_path.substr(pos + 1, m_path.size() - pos - 1);
	}
	else
	{
		return m_path;
	}
}


SharingGroup::SharingGroup(int group_id, const std::string& group_name, int owner_id, int root_dir_id, int node_id):
	m_group_id(group_id),
	m_group_name(group_name),
	m_owner_id(owner_id),
	m_root_dir_id(root_dir_id),
	m_node_id(node_id),
	m_manager_list(),
	m_member_list()
{
	m_manager_list.push_back(owner_id);
	m_member_list.push_back(owner_id);
}


int SharingGroup::group_id() const
{
	return m_group_id;
}


const std::string& SharingGroup::group_name() const
{
	return m_group_name;
}


int SharingGroup::owner_id() const
{
	return m_owner_id;
}


int SharingGroup::root_dir_id() const
{
	return m_root_dir_id;
}


int SharingGroup::node_id() const
{
	return m_node_id;
}


const SharingGroup::UserList& SharingGroup::manager_list() const
{
	return m_manager_list;
}


const SharingGroup::UserList& SharingGroup::member_list() const
{
	return m_member_list;
}


bool SharingGroup::is_manager(int user_id)
{
	auto itr = std::find(m_manager_list.begin(), m_manager_list.end(), user_id);
	return itr != m_manager_list.end();
}


bool SharingGroup::is_member(int user_id)
{
	auto itr = std::find(m_member_list.begin(), m_member_list.end(), user_id);
	return itr != m_member_list.end();
}


void SharingGroup::join_group(int user_id)
{
	auto itr = std::find(m_member_list.begin(), m_member_list.end(), user_id);
	if (itr != m_member_list.end())
	{
		return;
	}

	m_member_list.push_back(user_id);
}


void SharingGroup::assign_as_manager(int user_id)
{
	if (is_member(user_id))
	{
		kick_out_user(user_id);
		m_manager_list.push_back(user_id);
	}
	else if (is_manager(user_id))
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_GROUP_ALREADY_IS_MANAGER);
	}
	else
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_GROUP_USER_NOT_JOIN);
	}
}


void SharingGroup::assign_as_member(int user_id)
{
	if (is_manager(user_id))
	{
		kick_out_user(user_id);
		m_member_list.push_back(user_id);
	}
	else if (is_member(user_id))
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_GROUP_ALREADY_IS_MEMBER);
	}
	else
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_GROUP_USER_NOT_JOIN);
	}
}


void SharingGroup::kick_out_user(int user_id)
{
	if (user_id == owner_id())
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_GROUP_CANNOT_KICK_OUT_OWNER);
	}

	auto itr = std::find(m_member_list.begin(), m_member_list.end(), user_id);
	if (itr != m_member_list.end())
	{
		m_member_list.erase(itr);
	}

	itr = std::find(m_manager_list.begin(), m_manager_list.end(), user_id);
	if (itr != m_manager_list.end())
	{
		m_manager_list.erase(itr);
	}
}


int SharingGroup::get_file_id(const FilePath& path)
{
	int parent_dir_id = m_root_dir_id;
	auto result = path.split();
	for (auto& dir_name: *result)
	{
		SharingFile& parent = SharingFileManager::instance()->get_file(parent_dir_id);
		if (parent.file_type != SharingFile::DIRECTORY)
		{
			return INVALID_ID;
		}

		SharingDirectory& parent_dir = dynamic_cast<SharingDirectory&>(parent);
		int file_id = parent_dir.find_file(dir_name);
		if (file_id == INVALID_ID)
		{
			return file_id;
		}

		parent_dir_id = file_id;
	}

	return parent_dir_id;
}


bool SharingGroup::exist_path(const FilePath& path)
{
	return get_file_id(path) != INVALID_ID;
}


void SharingGroup::add_file(const FilePath& dir_path, int file_id)
{
	int dir_id = get_file_id(dir_path);
	SharingFile& dir_file = SharingFileManager::instance()->get_file(dir_id);
	if (dir_file.file_type != SharingFile::DIRECTORY)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_GROUP_NOT_DIR);
	}

	auto& dir = dynamic_cast<SharingDirectory&>(dir_file);
	dir.file_list.push_back(file_id);
}


void SharingGroup::create_path(const FilePath& path)
{
	int parent_id = m_root_dir_id;
	auto result = path.split();
	for (auto& dir_name: *result)
	{
		SharingFile& parent = SharingFileManager::instance()->get_file(parent_id);
		if (parent.file_type != SharingFile::DIRECTORY)
		{
			LIGHTS_THROW_EXCEPTION(Exception, ERR_GROUP_CREATE_DIR_MUST_UNDER_DIR);
		}

		SharingDirectory& parent_dir = dynamic_cast<SharingDirectory&>(parent);
		int file_id = parent_dir.find_file(dir_name);
		if (file_id == INVALID_ID)
		{
			SharingFile& new_file = SharingFileManager::instance()->register_file(SharingFile::DIRECTORY, dir_name);
			parent_dir.file_list.push_back(new_file.file_id);
			parent_id = new_file.file_id;
		}
		else
		{
			parent_id = file_id;
		}
	}
}


void SharingGroup::remove_path(const FilePath& path)
{
	int previous_parent_id = m_root_dir_id;
	int parent_id = m_root_dir_id;
	auto result = path.split();
	for (auto& dir_name: *result)
	{
		SharingFile& parent = SharingFileManager::instance()->get_file(parent_id);
		if (parent.file_type != SharingFile::DIRECTORY)
		{
			LIGHTS_THROW_EXCEPTION(Exception, ERR_GROUP_INVALID_PATH);
		}

		SharingDirectory& parent_dir = dynamic_cast<SharingDirectory&>(parent);
		int file_id = parent_dir.find_file(dir_name);

		previous_parent_id = parent_id;
		if (file_id == INVALID_ID)
		{
			LIGHTS_THROW_EXCEPTION(Exception, ERR_FILE_NOT_EXIST);
		}
		else
		{
			parent_id = file_id;
		}
	}

	if (parent_id == m_root_dir_id)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_GROUP_CANNOT_REMOVE_ROOT_DIR);
	}

	SharingFileManager::instance()->remove_file(parent_id);
	SharingFile& previous_parent = SharingFileManager::instance()->get_file(previous_parent_id);
	LIGHTS_ASSERT(previous_parent.file_type == SharingFile::DIRECTORY);
	SharingDirectory& previous_parent_dir = dynamic_cast<SharingDirectory&>(previous_parent);
	previous_parent_dir.remove_file(parent_id);
}


SharingGroup& SharingGroupManager::register_group(int user_id, const std::string& group_name)
{
	SharingGroup* old_group = find_group(group_name);
	if (old_group != nullptr)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_GROUP_ALREADY_EXIST);
	}

	SharingFile& root_dir = SharingFileManager::instance()->register_file(SharingFile::DIRECTORY, group_name);
	StorageNode& fit_node = StorageNodeManager::instance()->get_fit_node();
	SharingGroup new_group(m_next_id, group_name, user_id, root_dir.file_id, fit_node.node_id);
	++m_next_id;
	++fit_node.use_counting;

	auto value = std::make_pair(new_group.group_id(), new_group);
	auto result = m_group_list.insert(value);
	if (!result.second)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_GROUP_ALREADY_EXIST);
	}

	return result.first->second;
}


void SharingGroupManager::remove_group(int user_id, int group_id)
{
	SharingGroup& group = get_group(group_id);
	if (group.owner_id() != user_id)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_GROUP_NOT_PERMIT_NEED_OWNER);
	}

	StorageNode& node = StorageNodeManager::instance()->get_node(group.node_id());
	--node.use_counting;
	m_group_list.erase(group_id);
}


SharingGroup* SharingGroupManager::find_group(int group_id)
{
	auto itr = m_group_list.find(group_id);
	if (itr == m_group_list.end())
	{
		return nullptr;
	}

	return &(itr->second);
}


SharingGroup* SharingGroupManager::find_group(const std::string& group_name)
{
	auto itr = std::find_if(m_group_list.begin(), m_group_list.end(), [&group_name](const GroupList::value_type& value_type)
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


SharingGroup& SharingGroupManager::get_group(const std::string& group_name)
{
	SharingGroup* group = find_group(group_name);
	if (group == nullptr)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_GROUP_NOT_EXIST);
	}
	return *group;
}


int SharingDirectory::find_file(const std::string& filename)
{
	auto itr = std::find_if(file_list.begin(), file_list.end(), [&filename](int file_id)
	{
		SharingFile* file = SharingFileManager::instance()->find_file(file_id);
		if (file != nullptr && file->file_name == filename)
		{
			return true;
		}
		else
		{
			return false;
		}
	});

	if (itr == file_list.end())
	{
		return INVALID_ID;
	}

	return *itr;
}


void SharingDirectory::remove_file(int file_id)
{
	auto itr = std::remove(file_list.begin(),
						   file_list.end(),
						   file_id);
	file_list.erase(itr, file_list.end());
}


SharingFile& SharingFileManager::register_file(SharingFile::FileType file_type, const std::string& file_name, int arg)
{
	SharingFile* sharing_file = nullptr;
	switch (file_type)
	{
		case SharingFile::DIRECTORY:
		{
			sharing_file = new SharingDirectory();
			break;
		}
		case SharingFile::GENERAL_FILE:
		{
			auto file = new SharingGeneralFile;
			file->storage_file_id = arg;
			sharing_file = file;

			auto& storage_file = dynamic_cast<SharingStorageFile&>(get_file(file->storage_file_id));
			++storage_file.use_counting;
			break;
		}
		case SharingFile::STORAGE_FILE:
		{
			auto file = new SharingStorageFile;
			file->node_id = arg;
			sharing_file = file;
			break;
		}
	}

	sharing_file->file_id = m_next_id;
	++m_next_id;
	sharing_file->file_type = file_type;
	sharing_file->file_name = file_name;

	auto value = std::make_pair(sharing_file->file_id, sharing_file);
	auto result = m_file_list.insert(value);
	if (!result.second)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_FILE_ALREADY_EXIST);
	}

	return *result.first->second;
}


void SharingFileManager::remove_file(int file_id)
{
	SharingFile* sharing_file = find_file(file_id);
	m_file_list.erase(file_id);

	if (sharing_file != nullptr)
	{
		if (sharing_file->file_type == SharingFile::GENERAL_FILE)
		{
			auto* general_file = dynamic_cast<SharingGeneralFile*>(sharing_file);
			SharingFile& storage_file = get_file(general_file->storage_file_id);
			LIGHTS_ASSERT(storage_file.file_type == SharingFile::STORAGE_FILE);
			auto& real_storage_file = dynamic_cast<SharingStorageFile&>(storage_file);
			--real_storage_file.use_counting;
			remove_file(real_storage_file.file_id);
		}

		delete sharing_file;
	}
}


SharingFile* SharingFileManager::find_file(int file_id)
{
	auto itr = m_file_list.find(file_id);
	if (itr == m_file_list.end())
	{
		return nullptr;
	}
	return itr->second;
}


SharingFile* SharingFileManager::find_file(int node_id, const std::string& node_file_name)
{
	for (auto& pair : m_file_list)
	{
		if (pair.second->file_type == SharingFile::STORAGE_FILE)
		{
			auto storage_file = dynamic_cast<SharingStorageFile*>(pair.second);
			if (storage_file->node_id == node_id &&
				storage_file->file_name == node_file_name)
			{
				return storage_file;
			}
		}
	}

	return nullptr;
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


SharingFile& SharingFileManager::get_file(int node_id, const std::string& node_file_name)
{
	SharingFile* file = find_file(node_id, node_file_name);
	if (file == nullptr)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_FILE_NOT_EXIST);
	}
	return *file;
}


void StorageNodeManager::register_node(const std::string& ip, unsigned short port, RegisterCallback callback)
{
	Delegation::delegate("register_node", Delegation::NETWORK, [ip, port, callback](){
		NetworkService& service = NetworkServiceManager::instance()->register_service(ip, port);
		int service_id = service.service_id;

		Delegation::delegate("register_node", Delegation::WORKER, [ip, port, service_id, callback](){
			auto inst = StorageNodeManager::instance();
			StorageNode node(inst->m_next_id, ip, port, service_id);
			++inst->m_next_id;

			auto value = std::make_pair(node.node_id, node);
			auto result = inst->m_node_list.insert(value);
			if (!result.second)
			{
				LIGHTS_ERROR(logger, "Cannot register node. address={}:{}", ip, port);
				return;
			}

			if (callback != nullptr)
			{
				callback(result.first->second);
			}
		});
	});
}


void StorageNodeManager::remove_node(int node_id)
{
	StorageNode* node = find_node(node_id);
	if (node != nullptr)
	{
		int service_id = node->service_id;
		Delegation::delegate("remove_node", Delegation::NETWORK, [service_id]() {
			NetworkServiceManager::instance()->remove_service(service_id);
		});
		m_node_list.erase(node_id);
	}
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


StorageNode* StorageNodeManager::find_node(const std::string& ip, unsigned short port)
{
	auto itr = std::find_if(m_node_list.begin(), m_node_list.end(), [&](const StorageNodeList::value_type& value_type)
	{
		return value_type.second.ip == ip && value_type.second.port == port;
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


StorageNode& StorageNodeManager::get_node(const std::string& ip, unsigned short port)
{
	StorageNode* node = find_node(ip, port);
	if (node == nullptr)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_NODE_NOT_EXIST);
	}
	return *node;
}

StorageNode& StorageNodeManager::get_fit_node()
{
	// Find free node.
	auto itr = std::find_if(m_node_list.begin(), m_node_list.end(), [](const StorageNodeList::value_type& pair) {
		return pair.second.use_counting == 0;
	});

	if (itr != m_node_list.end())
	{
		return itr->second;
	}

	// Find the lowest load node.
	StorageNode* low_load_node = &(m_node_list.begin()->second);
	for (auto& pair: m_node_list)
	{
		if (pair.second.use_counting < low_load_node->use_counting)
		{
			low_load_node = &pair.second;
		}
	}

	return *low_load_node;
}


PutFileSession& FileSessionManager::register_put_session(int user_id,
														 int group_id,
														 const std::string& file_path,
														 int max_fragment)
{
	PutFileSession* session_entry = new PutFileSession(m_next_id,
													   user_id,
													   group_id,
													   file_path,
													   max_fragment);
	++m_next_id;

	Session session(SessionType::PUT_SESSION, session_entry);

	auto value = std::make_pair(session_entry->session_id, session);
	auto result = m_session_list.insert(value);
	if (!result.second)
	{
		delete session_entry;
		LIGHTS_THROW_EXCEPTION(Exception, ERR_FILE_SESSION_ALREADY_EXIST);
	}

	on_register_session(group_id, file_path, session_entry->session_id);
	return *session_entry;
}


GetFileSession& FileSessionManager::register_get_session(int user_id, int group_id, const std::string& file_path)
{
	GetFileSession* session_entry = new GetFileSession(m_next_id, user_id, group_id, file_path);
	++m_next_id;

	Session session(SessionType::GET_SESSION, session_entry);

	auto value = std::make_pair(session_entry->session_id, session);
	auto result = m_session_list.insert(value);
	if (!result.second)
	{
		delete session_entry;
		LIGHTS_THROW_EXCEPTION(Exception, ERR_FILE_SESSION_ALREADY_EXIST);
	}

	on_register_session(group_id, file_path, session_entry->session_id);
	return *session_entry;
}


void FileSessionManager::remove_session(int session_id)
{
	auto itr = m_session_list.find(session_id);
	if (itr == m_session_list.end())
	{
		return;
	}

	if (itr->second.type == SessionType::PUT_SESSION)
	{
		delete reinterpret_cast<PutFileSession*>(itr->second.entry);
	}
	else if (itr->second.type == SessionType::GET_SESSION)
	{
		delete reinterpret_cast<GetFileSession*>(itr->second.entry);
	}

	m_session_list.erase(session_id);
}


PutFileSession* FileSessionManager::find_put_session(int session_id)
{
	auto itr = m_session_list.find(session_id);
	if (itr == m_session_list.end())
	{
		return nullptr;
	}

	if (itr->second.type != SessionType::PUT_SESSION)
	{
		return nullptr;
	}

	return reinterpret_cast<PutFileSession*>(itr->second.entry);
}


GetFileSession* FileSessionManager::find_get_session(int session_id)
{
	auto itr = m_session_list.find(session_id);
	if (itr == m_session_list.end())
	{
		return nullptr;
	}

	if (itr->second.type != SessionType::GET_SESSION)
	{
		return nullptr;
	}

	return reinterpret_cast<GetFileSession*>(itr->second.entry);
}


PutFileSession* FileSessionManager::find_put_session(int user_id, int group_id, const std::string& file_path)
{
	Session* session = find_session(group_id, file_path);
	if (session == nullptr)
	{
		return nullptr;
	}

	if (session->type != SessionType::PUT_SESSION)
	{
		return nullptr;
	}

	auto put_session = reinterpret_cast<PutFileSession*>(session->entry);
	if (put_session->user_id != user_id)
	{
		return nullptr;
	}

	return put_session;
}


GetFileSession* FileSessionManager::find_get_session(int user_id, int group_id, const std::string& file_path)
{
	Session* session = find_session(group_id, file_path);
	if (session == nullptr)
	{
		return nullptr;
	}

	if (session->type != SessionType::GET_SESSION)
	{
		return nullptr;
	}

	auto get_session = reinterpret_cast<GetFileSession*>(session->entry);
	if (get_session->user_id != user_id)
	{
		return nullptr;
	}

	return get_session;
}


PutFileSession& FileSessionManager::get_put_session(int session_id)
{
	PutFileSession* session = find_put_session(session_id);
	if (session == nullptr)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_FILE_SESSION_NOT_EXIST);
	}

	return *session;
}


GetFileSession& FileSessionManager::get_get_session(int session_id)
{
	GetFileSession* session = find_get_session(session_id);
	if (session == nullptr)
	{
		LIGHTS_THROW_EXCEPTION(Exception, ERR_FILE_SESSION_NOT_EXIST);
	}

	return *session;
}


void FileSessionManager::on_register_session(int group_id, const std::string& file_path, int session_id)
{
	auto& group_session = m_group_session_list[group_id];
	group_session[file_path] = session_id;
}


FileSessionManager::Session* FileSessionManager::find_session(int group_id, const std::string& file_path)
{
	auto group_session_itr = m_group_session_list.find(group_id);
	if (group_session_itr == m_group_session_list.end())
	{
		return nullptr;
	}

	auto& group_session = group_session_itr->second;
	auto file_session_itr = group_session.find(file_path);
	if (file_session_itr == group_session.end())
	{
		return nullptr;
	}

	auto session_itr = m_session_list.find(file_session_itr->second);
	if (session_itr == m_session_list.end())
	{
		group_session.erase(file_path); // Delay removing.
		return nullptr;
	}

	return &session_itr->second;
}

} // namespace resource_server
} // namespace spaceless
