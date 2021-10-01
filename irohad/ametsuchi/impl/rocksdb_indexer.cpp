/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ametsuchi/impl/rocksdb_indexer.hpp"

#include <fmt/core.h>

#include "ametsuchi/impl/rocksdb_common.hpp"
#include "common/to_lower.hpp"
#include "cryptography/hash.hpp"

using namespace iroha::ametsuchi;
using namespace shared_model::interface::types;

RocksDBIndexer::RocksDBIndexer(std::shared_ptr<RocksDBContext> db_context)
    : db_context_(std::move(db_context)) {}

void RocksDBIndexer::txHashStatus(const TxPosition &position,
                                  TimestampType const ts,
                                  const HashType &tx_hash,
                                  bool is_committed) {
}

void RocksDBIndexer::committedTxHash(
    const TxPosition &position,
    shared_model::interface::types::TimestampType const ts,
    const HashType &committed_tx_hash) {
}

void RocksDBIndexer::rejectedTxHash(
    const TxPosition &position,
    shared_model::interface::types::TimestampType const ts,
    const HashType &rejected_tx_hash) {
}

void RocksDBIndexer::txPositions(
    shared_model::interface::types::AccountIdType const &account,
    HashType const &hash,
    boost::optional<AssetIdType> &&asset_id,
    TimestampType const ts,
    TxPosition const &position) {
}

iroha::expected::Result<void, std::string> RocksDBIndexer::flush() {
  RocksDbCommon common(db_context_);
  return {};
}
