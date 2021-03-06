/**
 * transaction.h
 * @author wherewindblow
 * @date   Jan 09, 2018
 */

#pragma once

#include <foundation/package.h>

namespace spaceless {
namespace storage_node {
namespace transaction {

void on_put_file_session(int conn_id, Package package);

void on_put_file(int conn_id, Package package);

void on_get_file_session(int conn_id, Package package);

void on_get_file(int conn_id, Package package);

} // namespace transaction
} // namespace storage_node
} // namespace spaceless
