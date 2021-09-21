/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <boost/variant.hpp>
#include <thread>
#include <chrono>

#include "ametsuchi/setting_query.hpp"
#include "backend/protobuf/query_responses/proto_query_response.hpp"
#include "backend/protobuf/transaction.hpp"
#include "builders/protobuf/queries.hpp"
#include "profiling/memory_profiler.hpp"
#include "builders/protobuf/transaction.hpp"
#include "interfaces/query_responses/transactions_response.hpp"
#include "framework/common_constants.hpp"
#include "framework/integration_framework/integration_test_framework.hpp"
#include "instantiate_test_suite.hpp"
#include "integration/acceptance/acceptance_fixture.hpp"
#include "interfaces/permissions.hpp"
#include "interfaces/query_responses/account_asset_response.hpp"
#include "module/shared_model/builders/protobuf/test_block_builder.hpp"
#include "module/shared_model/cryptography/crypto_defaults.hpp"
#include "utils/query_error_response_visitor.hpp"
#include "validators/field_validator.hpp"

using namespace integration_framework;
using namespace shared_model;
using namespace common_constants;

using shared_model::interface::types::PublicKeyHexStringView;

struct TransferAsset : AcceptanceFixture,
                       ::testing::WithParamInterface<StorageType> {
  /**
   * Creates the transaction with the first user creation commands
   * @param perms are the permissions of the user
   * @return built tx
   */
  auto makeFirstUser(const interface::RolePermissionSet &perms = {
                         interface::permissions::Role::kTransfer}) {
    auto new_perms = perms;
    new_perms.set(interface::permissions::Role::kAddAssetQty);
    return AcceptanceFixture::makeUserWithPerms(new_perms);
  }

  /**
   * Creates the transaction with the second user creation commands
   * @param perms are the permissions of the user
   * @return built tx
   */
  auto makeSecondUser(const interface::RolePermissionSet &perms = {
                          interface::permissions::Role::kReceive}) {
    return createUserWithPerms(
               kUser2,
               PublicKeyHexStringView{kUser2Keypair.publicKey()},
               kRole2,
               perms)
        .build()
        .signAndAddSignature(kAdminKeypair)
        .finish();
  }

  proto::Transaction addAssets() {
    return addAssets(kAmount);
  }

  proto::Transaction addAssets(const std::string &amount) {
    return complete(baseTx().addAssetQuantity(kAssetId, amount));
  }

  proto::Transaction makeTransfer(const std::string &amount) {
    return complete(
        baseTx().transferAsset(kUserId, kUser2Id, kAssetId, kDesc, amount));
  }

  proto::Transaction makeTransfer() {
    return makeTransfer(kAmount);
  }

  static constexpr iroha::StorageType storage_types[] = {
      iroha::StorageType::kPostgres, iroha::StorageType::kRocksDb};

  const std::string kAmount = "1.0";
  const std::string kDesc = "description";
  const std::string kRole2 = "roletwo";
  const std::string kUser2 = "usertwo";
  const std::string kUser2Id = kUser2 + "@" + kDomain;
  const crypto::Keypair kUser2Keypair =
      crypto::DefaultCryptoAlgorithmType::generateKeypair();
};

INSTANTIATE_TEST_SUITE_P_DifferentStorageTypes(TransferAsset);

/**
 * TODO mboldyrev 18.01.2019 IR-228 "Basic" tests should be replaced with a
 * common acceptance test
 * also covered by postgres_executor_test TransferAccountAssetTest.Valid
 *
 * @given pair of users with all required permissions
 * @when execute tx with TransferAsset command
 * @then there is the tx in proposal
 */
TEST_P(TransferAsset, Basic) {
  profiler::initTables();
  {
  std::string store_path = "/home/iceseer/Tmp/test_db/store/";
  std::string wsv_path = "/home/iceseer/Tmp/test_db/wsv/";
  IntegrationTestFramework itf(1,
                               StorageType::kRocksDb,
                               boost::none,
                               iroha::StartupWsvDataPolicy::kDrop,
                               true,
                               false,
                               boost::none,
                               milliseconds(20000),
                               milliseconds(20000),
                               milliseconds(10000),
                               getDefaultItfLogManager(),
                               wsv_path,
                               store_path);

  itf.setInitialState(kAdminKeypair)
      .sendTxAwait(makeFirstUser(), CHECK_TXS_QUANTITY(1))
      .sendTxAwait(makeSecondUser(), CHECK_TXS_QUANTITY(1))
      .sendTxAwait(addAssets("1000000000.0"), CHECK_TXS_QUANTITY(1));

  auto make_query = [this](auto const &tx_hashes) {
    return baseQry()
        .creatorAccountId(kAdminId)
        .getTransactions(tx_hashes)
        .build()
        .signAndAddSignature(kAdminKeypair)
        .finish();
  };

  auto const start = std::chrono::steady_clock::now();
  std::vector<interface::types::HashType> hashes;
  while (std::chrono::duration_cast<std::chrono::hours>(
             std::chrono::steady_clock::now() - start)
         < std::chrono::hours(4)) {
    //    std::shared_ptr<const shared_model::interface::Block> block;
    hashes.clear();
    itf.sendTxAwait(makeTransfer(),
                    [&](const auto &resp) {
                      ASSERT_EQ(resp->transactions().size(), 1);
                      for (auto &tx : resp->transactions())
                        hashes.push_back(tx.hash());
                    })
        .sendQuery(make_query(hashes), [](auto &resp) {
          [[maybe_unused]] auto &txs =
              boost::get<const shared_model::interface::TransactionsResponse &>(
                  resp.get());
          ASSERT_EQ(txs.transactions().size(), 1);
        });
  }

  profiler::printTables();
  itf.printDbStatus();

  itf.dropDB();
  profiler::printTables();

  getchar();
  std::abort();
  }
  getchar();
  profiler::printTables();
}
