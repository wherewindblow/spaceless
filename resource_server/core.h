/**
 * core.h
 * @author wherewindblow
 * @date   Oct 26, 2017
 */

#pragma once

#include <vector>
#include <set>
#include <map>
#include <memory>
#include <functional>

#include <lights/sequence.h>
#include <foundation/basics.h>
#include <Poco/StringTokenizer.h>
#include <Poco/JSON/Object.h>


namespace spaceless {
namespace resource_server {

enum
{
	ERR_USER_ALREADY_EXIST = 1000,
	ERR_USER_NOT_EXIST = 1001,
	ERR_USER_NOT_LOGIN = 1010,

	ERR_GROUP_ALREADY_EXIST = 1100,
	ERR_GROUP_NOT_EXIST = 1101,
	ERR_GROUP_NOT_PERMIT_NEED_OWNER = 1102,
	ERR_GROUP_NOT_PERMIT_NEED_MANAGER = 1103,
	ERR_GROUP_NOT_PERMIT_NEED_MEMBER = 1104,
	ERR_GROUP_CANNOT_KICK_OUT_OWNER = 1106,
	ERR_GROUP_USER_NOT_JOIN = 1107,
	ERR_GROUP_ALREADY_IS_MANAGER = 1108,
	ERR_GROUP_ALREADY_IS_MEMBER = 1109,

	ERR_GROUP_CANNOT_REMOVE_ROOT_DIR = 1110,
	ERR_GROUP_NOT_DIRECTORY = 1112,

	ERR_FILE_ALREADY_EXIST = 1200,
	ERR_FILE_CANNOT_CREATE = 1201,
	ERR_FILE_NOT_EXIST = 1202,

	ERR_NODE_ALREADY_EXIST = 1300,
	ERR_NODE_CANNOT_CREATE = 1301,
	ERR_NODE_NOT_EXIST = 1302,

	ERR_FILE_SESSION_ALREADY_EXIST = 1400,
	ERR_FILE_SESSION_CANNOT_CREATE = 1401,
	ERR_FILE_SESSION_NOT_EXIST = 1402,
	ERR_FILE_SESSION_NOT_REGISTER_USER = 1403,
	ERR_FILE_SESSION_INVALID_FRAGMENT = 1404,
	ERR_FILE_SESSION_CANNOT_CHANGE_MAX_FRAGMENT = 1405,
};


using Poco::JSON::Object;


struct User
{
	User() = default;

	User(int user_id, const std::string& user_name, const std::string& password) :
		user_id(user_id),
		user_name(user_name),
		password(password),
		group_list(),
		conn_id(0),
		last_hearbeat(0)
	{}

	Object::Ptr serialize();

	void deserialize(Object::Ptr object);

	int user_id;
	std::string user_name;
	std::string password;
	std::vector<int> group_list;
	int conn_id;
	std::time_t last_hearbeat;
};


inline bool operator== (const User& lhs, const User& rhs)
{
	return lhs.user_id == rhs.user_id;
}


class UserManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(UserManager);

	UserManager();

	User& register_user(const std::string& username, const std::string& password, bool is_root_user = false);

	void remove_user(int user_id);

	User* find_user(int user_id);

	User* find_user(const std::string& user_name);

	User& get_user(int user_id);

	User& get_user(const std::string& username);

	bool login_user(int user_id, const std::string& password, int conn_id);

	User* find_login_user(int conn_id);

	User& get_login_user(int conn_id);

	void kick_out_user(int conn_id);

	void heartbeat(int user_id);
	
	bool is_root_user(int user_id);

	Object::Ptr serialize();

	void deserialize(Object::Ptr object);

private:
	void kick_out_offline_users();

	using UserList = std::map<int, User>;
	UserList m_user_list;
	std::set<int> m_root_user_list;
	std::map<int, int> m_login_user_list;
	int m_next_id = 1;
};


class FilePath
{
public:
	static const char SEPARATOR[];

	static const char ROOT[];

	using SplitReult = std::unique_ptr<Poco::StringTokenizer>;

	FilePath(const std::string& path);

	SplitReult split() const;

	std::string directory_path() const;

	std::string filename() const;

private:
	std::string m_path;
};


class SharingGroup
{
public:
	using UserList = std::vector<int>;

	SharingGroup() = default;

	SharingGroup(int group_id, const std::string& group_name, int owner_id, int root_dir_id, int node_id);

	int group_id() const;

	const std::string& group_name() const;

	int owner_id() const;

	int root_dir_id() const;

	int node_id() const;

	const UserList& manager_list() const;

	const UserList& member_list() const;

	bool is_manager(int user_id);

	bool is_member(int user_id);

	void join_group(int user_id);

	void assign_as_manager(int user_id);

	void assign_as_member(int user_id);

	void kick_out_user(int user_id);

	/**
	 * Get file id of @c path filename.
	 */
	int get_file_id(const FilePath& path);

	bool exist_path(const FilePath& path);

	void add_file(const FilePath& dir_path, int file_id);

	/**
	 * Creates all directory by path. If a parent directory is not create will automatically create.
	 */
	void create_path(const FilePath& path);

	/**
	 * Removes last file of path. Can remove directory and general file.
	 */
	void remove_path(const FilePath& path);

	Object::Ptr serialize();

	void deserialize(Object::Ptr object);

private:
	int m_group_id;
	std::string m_group_name;
	int m_owner_id;
	int m_root_dir_id;
	int m_node_id;
	UserList m_manager_list; // Manager list include owner and general managers.
	UserList m_member_list; // Member list include all members in group.
};


inline bool operator== (const SharingGroup& lhs, const SharingGroup& rhs)
{
	return lhs.group_id() == rhs.group_id();
}


class SharingGroupManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(SharingGroupManager);

	SharingGroup& register_group(int user_id, const std::string& group_name);

	void remove_group(int user_id, int group_id);

	SharingGroup* find_group(int group_id);

	SharingGroup* find_group(const std::string& group_name);

	SharingGroup& get_group(int group_id);

	SharingGroup& get_group(const std::string& group_name);

	Object::Ptr serialize();

	void deserialize(Object::Ptr object);

private:
	using GroupList = std::map<int, SharingGroup>;
	GroupList m_group_list;
	int m_next_id = 1;
};


class SharingFile
{
public:
	enum FileType
	{
		DIRECTORY,
		GENERAL_FILE,
		STORAGE_FILE,
	};

	SharingFile() :
		file_id(0),
		file_type(GENERAL_FILE),
		file_name()
	{}

	virtual ~SharingFile() = default;

	virtual Object::Ptr serialize();

	virtual void deserialize(Object::Ptr object);

	int file_id;
	FileType file_type;
	std::string file_name;
};


class SharingGeneralFile: public SharingFile
{
public:
	SharingGeneralFile() :
		storage_file_id(0)
	{}

	Object::Ptr serialize() override;

	void deserialize(Object::Ptr object) override;

	int storage_file_id; // The underlying storage file id.
};


class SharingStorageFile: public SharingFile
{
public:
	SharingStorageFile() :
		node_id(0),
		use_counting(0)
	{}

	Object::Ptr serialize() override;

	void deserialize(Object::Ptr object) override;

	int node_id;
	int use_counting;
};


class SharingDirectory: public SharingFile
{
public:
	SharingDirectory() :
		file_list()
	{}

	Object::Ptr serialize() override;

	void deserialize(Object::Ptr object) override;

	int find_file(const std::string& filename);

	void remove_file(int file_id);

	std::vector<int> file_list;
};


class SharingFileManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(SharingFileManager);

	SharingFile& register_file(SharingFile::FileType file_type, const std::string& file_name, int arg = 0);

	void remove_file(int file_id);

	SharingFile* find_file(int file_id);

	SharingFile* find_file(int node_id, const std::string& node_file_name);

	SharingFile& get_file(int file_id);

	SharingFile& get_file(int node_id, const std::string& node_file_name);

	Object::Ptr serialize();

	void deserialize(Object::Ptr object);

private:
	using SharingFileList = std::map<int, SharingFile*>;
	SharingFileList m_file_list;
	int m_next_id = 1;
};


struct StorageNode
{
	StorageNode() = default;

	StorageNode(int node_id, const std::string& ip, unsigned short port, int service_id) :
		node_id(node_id),
		ip(ip),
		port(port),
		service_id(service_id),
		use_counting(0)
	{}

	Object::Ptr serialize();

	void deserialize(Object::Ptr object);

	int node_id;
	std::string ip;
	unsigned short port;
	int service_id;
	int use_counting;
};


class StorageNodeManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(StorageNodeManager);

	using RegisterCallback = std::function<void(StorageNode&)>;

	void register_node(const std::string& ip, unsigned short port, RegisterCallback callback = nullptr);

	void remove_node(int node_id);

	StorageNode* find_node(int node_id);

	StorageNode* find_node(const std::string& ip, unsigned short port);

	StorageNode& get_node(int node_id);

	StorageNode& get_node(const std::string& ip, unsigned short port);

	StorageNode& get_fit_node();

	Object::Ptr serialize();

	void deserialize(Object::Ptr object);

private:
	using StorageNodeList = std::map<int, StorageNode>;
	StorageNodeList m_node_list;
	int m_next_id = 1;
};


struct PutFileSession
{
	PutFileSession(int session_id, int user_id, int group_id, const std::string file_path, int max_fragment) :
		session_id(session_id),
		user_id(user_id),
		group_id(group_id),
		file_path(file_path),
		max_fragment(max_fragment),
		next_fragment(0),
		node_session_id(0)
	{}

	int session_id;
	int user_id;
	int group_id;
	std::string file_path;
	int max_fragment;
	int next_fragment;
	int node_session_id;
};


struct GetFileSession
{
	GetFileSession(int session_id, int user_id, int group_id, const std::string file_path) :
		session_id(session_id),
		user_id(user_id),
		group_id(group_id),
		file_path(file_path),
		node_session_id(0)
	{}

	int session_id;
	int user_id;
	int group_id;
	std::string file_path;
	int node_session_id;
};


class FileSessionManager
{
public:
	enum class SessionType
	{
		PUT_SESSION,
		GET_SESSION,
	};

	struct Session
	{
		Session(SessionType type, void* entry) :
			type(type),
			entry(entry)
		{}

		SessionType type;
		void* entry;
	};

	SPACELESS_SINGLETON_INSTANCE(FileSessionManager);

	PutFileSession& register_put_session(int user_id, int group_id, const std::string& file_path, int max_fragment);

	GetFileSession& register_get_session(int user_id, int group_id, const std::string& file_path);

	void remove_session(int session_id);

	PutFileSession* find_put_session(int session_id);

	GetFileSession* find_get_session(int session_id);

	PutFileSession* find_put_session(int user_id, int group_id, const std::string& file_path);

	GetFileSession* find_get_session(int user_id, int group_id, const std::string& file_path);

	PutFileSession& get_put_session(int session_id);

	GetFileSession& get_get_session(int session_id);

private:
	void on_register_session(int group_id, const std::string& file_path, int session_id);

	Session* find_session(int group_id, const std::string& file_path);

	std::map<int, Session> m_session_list;
	std::map<int, std::map<std::string, int>> m_group_session_list;
	int m_next_id = 1;
};


class SerializationManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(SerializationManager);

	using Serialize = std::function<Object::Ptr()>;
	using Deserialize = std::function<void(Object::Ptr)>;

	SerializationManager();

	void register_serialization(const std::string& name, Serialize serialize, Deserialize deserialize);

	void remove_serialization(const std::string& name);

	void serialize();

	void deserialize();

private:
	struct Operations
	{
		Serialize serialize;
		Deserialize deserialize;
	};

	std::map<std::string, Operations> m_operation_list;
};


#define SPACELESS_REG_SERIALIZATION(class_name) \
do \
{ \
    auto serialize = []() { return class_name::instance()->serialize(); }; \
    auto deserialize = [](Object::Ptr object) { class_name::instance()->deserialize(object); }; \
    SerializationManager::instance()->register_serialization(#class_name, serialize, deserialize);  \
} while (false)

} // namespace resource_server
} // namespace spaceless
