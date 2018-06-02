/**
 * transaction.h
 * @author wherewindblow
 * @date   Jan 09, 2018
 */

#pragma once

#include <common/package.h>

namespace spaceless {
namespace storage_node {
namespace transaction {

void on_put_file(int conn_id, const PackageBuffer& package);

void on_get_file(int conn_id, const PackageBuffer& package);

} // namespace transaction
} // namespace storage_node
} // namespace spaceless
