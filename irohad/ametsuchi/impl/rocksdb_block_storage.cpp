/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ametsuchi/impl/rocksdb_block_storage.hpp"

#include "ametsuchi/impl/rocksdb_common.hpp"
#include "backend/protobuf/block.hpp"
#include "common/byteutils.hpp"
#include "logger/logger.hpp"
#include "ametsuchi/impl/in_memory_storage.hpp"

using namespace iroha::ametsuchi;

#define CHECK_OPERATION(command, ...)                                          \
  if (auto result = (__VA_ARGS__); expected::hasError(result)) {               \
    log_->error("Error while block {} " command ". Code: {}. Description: {}", \
                block->height(),                                               \
                result.assumeError().code,                                     \
                result.assumeError().description);                             \
    return false;                                                              \
  }

namespace {
  inline iroha::expected::Result<void, DbError> incrementTotalBlocksCount(
      iroha::ametsuchi::RocksDbCommon &common) {
    RDB_TRY_GET_VALUE(
        opt_count,
        forBlocksTotalCount<kDbOperation::kGet, kDbEntry::kCanExist>(common));

    common.encode(opt_count ? *opt_count + 1ull : 1ull);
    RDB_ERROR_CHECK(
        forBlocksTotalCount<kDbOperation::kPut, kDbEntry::kMustExist>(common));

    return {};
  }
}  // namespace

RocksDbBlockStorage::RocksDbBlockStorage(
    std::shared_ptr<RocksDBContext> db_context,
    std::shared_ptr<shared_model::interface::BlockJsonConverter> json_converter,
    logger::LoggerPtr log)
    : db_context_(std::move(db_context)),
      json_converter_(std::move(json_converter)),
      storage_(std::make_shared<StorageType>()),
      log_(std::move(log)) {}

bool RocksDbBlockStorage::insert(
    std::shared_ptr<const shared_model::interface::Block> block) {
  return json_converter_->serialize(*block).match(
      [&](const auto &block_json) {
        return storage_.template exclusiveAccess([&](auto &storage) {
          storage->insert(block->height(), block_json.value);
          return true;
        });
      },
      [this](const auto &error) {
        log_->warn("Error while block serialization: {}", error.error);
        return false;
      });
}

boost::optional<std::unique_ptr<shared_model::interface::Block>>
RocksDbBlockStorage::fetch(
    shared_model::interface::types::HeightType height) const {
  return storage_.sharedAccess(
      [&](auto const &storage)
          -> boost::optional<std::unique_ptr<shared_model::interface::Block>> {
        if (auto data = storage->find(height)) {
          return json_converter_->deserialize((*data).get())
              .match(
                  [&](auto &&block) {
                    return boost::make_optional<
                        std::unique_ptr<shared_model::interface::Block>>(
                        std::move(block.value));
                  },
                  [&](const auto &error)
                      -> boost::optional<
                          std::unique_ptr<shared_model::interface::Block>> {
                    log_->warn("Error while block deserialization: {}",
                               error.error);
                    return boost::none;
                  });
        }
        log_->error("Error while block {} reading.", height);
        return boost::none;
      });
}

size_t RocksDbBlockStorage::size() const {
  return storage_.sharedAccess([](auto const &storage){
    return storage->allTimeValues();
  });
}

void RocksDbBlockStorage::reload() {}

void RocksDbBlockStorage::clear() {
  storage_.exclusiveAccess([](auto &storage) { storage->clear(); });
}

iroha::expected::Result<void, std::string> RocksDbBlockStorage::forEach(
    iroha::ametsuchi::BlockStorage::FunctionType function) const {
  return storage_.sharedAccess(
      [&](auto const &storage) -> iroha::expected::Result<void, std::string> {
        storage->forEach([&](auto const & /*key*/, auto const &value) {
          json_converter_->deserialize(value).match(
              [&](auto &&block) {
                function(std::shared_ptr<const shared_model::interface::Block>(
                    std::move(block.value)));
              },
              [&](const auto &error) {
                log_->warn("Error while block deserialization: {}",
                           error.error);
              });
        });
        return {};
      });
}
