/**
 * transcation.h
 * @author wherewindblow
 * @date   Dec 24, 2017
 */

#pragma once

#include <common/network.h>
#include <protocol/all.h>


namespace spaceless {
namespace resource_server {
namespace transcation {

void on_register_user(NetworkConnection& conn, const PackageBuffer& package);

void on_login_user(NetworkConnection& conn, const PackageBuffer& package);

void on_remove_user(NetworkConnection& conn, const PackageBuffer& package);

void on_find_user(NetworkConnection& conn, const PackageBuffer& package);

void on_register_group(NetworkConnection& conn, const PackageBuffer& package);

void on_remove_group(NetworkConnection& conn, const PackageBuffer& package);

void on_find_group(NetworkConnection& conn, const PackageBuffer& package);

void on_join_group(NetworkConnection& conn, const PackageBuffer& package);

void on_assign_as_manager(NetworkConnection& conn, const PackageBuffer& package);

void on_assign_as_memeber(NetworkConnection& conn, const PackageBuffer& package);

void on_kick_out_user(NetworkConnection& conn, const PackageBuffer& package);

void on_create_path(NetworkConnection& conn, const PackageBuffer& package);


class PutFileTranscation: public MultiplyPhaseTranscation
{
public:
	enum
	{
		WAIT_STORAGE_NODE_PUT_FILE,
	};

	static MultiplyPhaseTranscation* register_transcation(int trans_id);

	PutFileTranscation(int trans_id);

	PhaseResult on_init(NetworkConnection& conn, const PackageBuffer& package) override;

	PhaseResult on_active(NetworkConnection& conn, const PackageBuffer& package) override;

	PhaseResult send_back_error(int error_code);

private:
	protocol::ReqPutFile m_request;
	protocol::RspPutFile m_rsponse;
};


class GetFileTranscation: public MultiplyPhaseTranscation
{
public:
	enum
	{
		WAIT_STORAGE_NODE_GET_FILE,
	};

	static MultiplyPhaseTranscation* register_transcation(int trans_id);

	GetFileTranscation(int trans_id);

	PhaseResult on_init(NetworkConnection& conn, const PackageBuffer& package) override;

	PhaseResult on_active(NetworkConnection& conn, const PackageBuffer& package) override;

	PhaseResult send_back_error(int error_code);

private:
	protocol::ReqGetFile m_request;
	protocol::RspGetFile m_rsponse;
};


class RemovePathTranscation: public MultiplyPhaseTranscation
{
public:
	enum
	{
		WAIT_STORAGE_NODE_GET_FILE,
	};

	static MultiplyPhaseTranscation* register_transcation(int trans_id);

	RemovePathTranscation(int trans_id);

	PhaseResult on_init(NetworkConnection& conn, const PackageBuffer& package) override;

	PhaseResult on_active(NetworkConnection& conn, const PackageBuffer& package) override;

	PhaseResult send_back_error(int error_code);

private:
	protocol::ReqRemovePath m_request;
	protocol::RspRemovePath m_rsponse;
};

} // namespace transcation
} // namespace resource_server
} // namespace spaceless
