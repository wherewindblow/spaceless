/**
 * transaction.h
 * @author wherewindblow
 * @date   Jan 09, 2018
 */

#pragma once

#include <common/network.h>

namespace spaceless {
namespace storage_node {
namespace transaction {

void on_put_file(NetworkConnection& conn, const PackageBuffer& package);

void on_get_file(NetworkConnection& conn, const PackageBuffer& package);

} // namespace transaction
} // namespace storage_node
} // namespace spaceless
