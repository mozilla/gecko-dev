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

namespace mozilla::dom::quota::test {

class DOM_Quota_DirectoryLock : public QuotaManagerDependencyFixture {
 public:
  static void SetUpTestCase() { ASSERT_NO_FATAL_FAILURE(InitializeFixture()); }

  static void TearDownTestCase() { ASSERT_NO_FATAL_FAILURE(ShutdownFixture()); }
};

// Test that Drop unregisters directory lock synchronously.
TEST_F(DOM_Quota_DirectoryLock, Drop_Timing) {
  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    RefPtr<UniversalDirectoryLock> exclusiveDirectoryLock =
        DirectoryLockImpl::CreateInternal(
            WrapNotNullUnchecked(quotaManager), Nullable<PersistenceType>(),
            OriginScope::FromNull(), Nullable<Client::Type>(),
            /* aExclusive */ true, DirectoryLockCategory::None);

    bool done = false;

    exclusiveDirectoryLock->Acquire()->Then(
        GetCurrentSerialEventTarget(), __func__,
        [&done](const BoolPromise::ResolveOrRejectValue& aValue) {
          done = true;
        });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });

    exclusiveDirectoryLock->Drop();
    exclusiveDirectoryLock = nullptr;

    RefPtr<UniversalDirectoryLock> sharedDirectoryLock =
        DirectoryLockImpl::CreateInternal(
            WrapNotNullUnchecked(quotaManager), Nullable<PersistenceType>(),
            OriginScope::FromNull(), Nullable<Client::Type>(),
            /* aExclusive */ false, DirectoryLockCategory::None);

    ASSERT_FALSE(sharedDirectoryLock->MustWait());

    sharedDirectoryLock = nullptr;
  });
}

}  // namespace mozilla::dom::quota::test
