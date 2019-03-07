/**
 * transaction.h
 * @author wherewindblow
 * @date   Dec 24, 2017
 */

#pragma once

#include <foundation/package.h>
#include <foundation/transaction.h>
#include <protocol/all.h>


namespace spaceless {
namespace resource_server {
namespace transaction {

void on_ping(int conn_id, Package package);

void on_register_user(int conn_id, Package package);

void on_login_user(int conn_id, Package package);

void on_remove_user(int conn_id, Package package);

void on_find_user(int conn_id, Package package);

void on_register_group(int conn_id, Package package);

void on_remove_group(int conn_id, Package package);

void on_find_group(int conn_id, Package package);

void on_join_group(int conn_id, Package package);

void on_assign_as_manager(int conn_id, Package package);

void on_assign_as_member(int conn_id, Package package);

void on_kick_out_user(int conn_id, Package package);

void on_create_path(int conn_id, Package package);

void on_list_file(int conn_id, Package package);


class PutFileSessionTrans: public MultiplyPhaseTransaction
{
public:
	enum
	{
		WAIT_STORAGE_NODE,
	};

	static MultiplyPhaseTransaction* factory(int trans_id);

	PutFileSessionTrans(int trans_id);

	void on_init(int conn_id, Package package) override;

	void on_active(int conn_id, Package package) override;

	void on_timeout() override;

	void on_error(int conn_id, int error_code) override;

private:
	int m_session_id;
};


class PutFileTrans: public MultiplyPhaseTransaction
{
public:
	enum
	{
		WAIT_STORAGE_NODE,
	};

	static MultiplyPhaseTransaction* factory(int trans_id);

	PutFileTrans(int trans_id);

	void on_init(int conn_id, Package package) override;

	void on_active(int conn_id, Package package) override;

private:
	int m_session_id;
};


class GetFileSessionTrans: public MultiplyPhaseTransaction
{
public:
	enum
	{
		WAIT_STORAGE_NODE,
	};

	static MultiplyPhaseTransaction* factory(int trans_id);

	GetFileSessionTrans(int trans_id);

	void on_init(int conn_id, Package package) override;

	void on_active(int conn_id, Package package) override;

	void on_timeout() override;

	void on_error(int conn_id, int error_code) override;

private:
	int m_session_id;
};


class GetFileTrans: public MultiplyPhaseTransaction
{
public:
	enum
	{
		WAIT_STORAGE_NODE,
	};

	static MultiplyPhaseTransaction* factory(int trans_id);

	GetFileTrans(int trans_id);

	void on_init(int conn_id, Package package) override;

	void on_active(int conn_id, Package package) override;

private:
	int m_session_id;
};


class RemovePathTrans: public MultiplyPhaseTransaction
{
public:
	enum
	{
		WAIT_STORAGE_NODE,
	};

	static MultiplyPhaseTransaction* factory(int trans_id);

	RemovePathTrans(int trans_id);

	void on_init(int conn_id, Package package) override;

	void on_active(int conn_id, Package package) override;
};

} // namespace transaction
} // namespace resource_server
} // namespace spaceless
