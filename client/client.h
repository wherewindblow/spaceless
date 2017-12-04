/**
 * client.h
 * @author wherewindblow
 * @date   Nov 20, 2017
 */

#pragma once

#include <vector>
#include <map>
#include <lights/sequence.h>

#include "common/network.h"
#include "common/basics.h"


namespace spaceless {
namespace client {

extern NetworkConnection* conn;

struct User
{
	int uid;
	std::string username;
	std::vector<int> group_list;
};


class UserManager
{
public:
	SPACELESS_SINGLETON_INSTANCE(UserManager);

	void register_user(lights::StringView username, lights::StringView password);

	void login_user(int uid, lights::StringView password);

	void remove_user(int uid);

	User* find_user(int uid);

	User* find_user(lights::StringView username);

	User& get_user(int uid);

	User& get_user(lights::StringView username);

private:
	using UserList = std::map<int, User>;
	UserList m_user_list;
	int m_next_id = 1;
};


} // namespace client
} // namespace spaceless

