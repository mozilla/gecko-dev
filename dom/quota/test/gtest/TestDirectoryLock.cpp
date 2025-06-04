/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DirectoryLockImpl.h"
#include "QuotaManagerDependencyFixture.h"
#include "gtest/gtest.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/dom/quota/OriginScope.h"
#include "mozilla/dom/quota/UniversalDirectoryLock.h"

namespace mozilla::dom::quota::test {

class DOM_Quota_DirectoryLock : public QuotaManagerDependencyFixture {
 public:
  static void SetUpTestCase() { ASSERT_NO_FATAL_FAILURE(InitializeFixture()); }

  static void TearDownTestCase() { ASSERT_NO_FATAL_FAILURE(ShutdownFixture()); }
};

using BoolTripleTestParams = std::tuple<bool, bool, bool>;

class DOM_Quota_DirectoryLockWithBoolTriple
    : public DOM_Quota_DirectoryLock,
      public testing::WithParamInterface<BoolTripleTestParams> {};

TEST_F(DOM_Quota_DirectoryLock, MutableManagerRef) {
  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    RefPtr<ClientDirectoryLock> directoryLock =
        quotaManager->CreateDirectoryLock(GetTestClientMetadata(),
                                          /* aExclusive */ false);

    EXPECT_EQ(&directoryLock->MutableManagerRef(), quotaManager);
  });
}

// Test that Drop unregisters directory lock asynchronously.
TEST_F(DOM_Quota_DirectoryLock, Drop_Timing) {
  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    RefPtr<UniversalDirectoryLock> exclusiveDirectoryLock =
        quotaManager->CreateDirectoryLockInternal(
            PersistenceScope::CreateFromNull(), OriginScope::FromNull(),
            ClientStorageScope::CreateFromNull(),
            /* aExclusive */ true, DirectoryLockCategory::None);

    bool done = false;

    exclusiveDirectoryLock->Acquire()->Then(
        GetCurrentSerialEventTarget(), __func__,
        [&done](const BoolPromise::ResolveOrRejectValue& aValue) {
          done = true;
        });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });

    auto exclusiveDirectoryLockDropPromise = exclusiveDirectoryLock->Drop();
    exclusiveDirectoryLock = nullptr;

    RefPtr<UniversalDirectoryLock> sharedDirectoryLock =
        quotaManager->CreateDirectoryLockInternal(
            PersistenceScope::CreateFromNull(), OriginScope::FromNull(),
            ClientStorageScope::CreateFromNull(),
            /* aExclusive */ false, DirectoryLockCategory::None);

    ASSERT_TRUE(sharedDirectoryLock->MustWait());

    done = false;

    exclusiveDirectoryLockDropPromise->Then(
        GetCurrentSerialEventTarget(), __func__,
        [&done](const BoolPromise::ResolveOrRejectValue& aValue) {
          done = true;
        });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });

    ASSERT_FALSE(sharedDirectoryLock->MustWait());

    sharedDirectoryLock = nullptr;
  });
}

TEST_P(DOM_Quota_DirectoryLockWithBoolTriple, BlockingBehavior) {
  const BoolTripleTestParams& param = GetParam();

  const bool firstExclusive = std::get<0>(param);
  const bool secondExclusive = std::get<1>(param);
  const bool secondMustWait = std::get<2>(param);

  PerformOnBackgroundThread([firstExclusive, secondExclusive,
                             secondMustWait]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    RefPtr<ClientDirectoryLock> directoryLock1 =
        quotaManager->CreateDirectoryLock(GetTestClientMetadata(),
                                          /* aExclusive */ firstExclusive);
    Await(directoryLock1->Acquire());

    RefPtr<ClientDirectoryLock> directoryLock2 =
        quotaManager->CreateDirectoryLock(GetTestClientMetadata(),
                                          /* aExclusive */ secondExclusive);

    EXPECT_EQ(directoryLock2->MustWait(), secondMustWait);
    EXPECT_EQ(directoryLock2->LocksMustWaitFor().IsEmpty(), !secondMustWait);

    DropDirectoryLock(directoryLock1);
  });
}

INSTANTIATE_TEST_SUITE_P(
    , DOM_Quota_DirectoryLockWithBoolTriple,
    testing::Values(std::make_tuple(/* firstExclusive */ false,
                                    /* secondExclusive */ false,
                                    /* secondMustWait */ false),
                    std::make_tuple(/* firstExclusive */ false,
                                    /* secondExclusive */ true,
                                    /* secondMustWait */ true),
                    std::make_tuple(/* firstExclusive */ true,
                                    /* secondExclusive */ false,
                                    /* secondMustWait */ true),
                    std::make_tuple(/* firstExclusive */ true,
                                    /* secondExclusive */ true,
                                    /* secondMustWait */ true)),
    [](const testing::TestParamInfo<BoolTripleTestParams>& aParam)
        -> std::string {
      const BoolTripleTestParams& param = aParam.param;

      const bool firstExclusive = std::get<0>(param);
      const bool secondExclusive = std::get<1>(param);
      const bool secondMustWait = std::get<2>(param);

      std::stringstream ss;

      ss << (firstExclusive ? "FirstExclusive" : "FirstShared") << "_"
         << (secondExclusive ? "SecondExclusive" : "SecondShared") << "_"
         << (secondMustWait ? "SecondMustWait" : "SecondMustNotWait");

      return ss.str();
    });

}  // namespace mozilla::dom::quota::test
