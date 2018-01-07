/**
 * transcation.h
 * @author wherewindblow
 * @date   Dec 24, 2017
 */

#pragma once

#include <common/network.h>

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

void on_kick_out_user(NetworkConnection& conn, const PackageBuffer& package);

void on_put_file(NetworkConnection& conn, const PackageBuffer& package);

void on_get_file(NetworkConnection& conn, const PackageBuffer& package);

} // namespace transcation
} // namespace resource_server
} // namespace spaceless
