/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/quota/ClientDirectoryLockHandle.h"
#include "mozilla/dom/quota/ConditionalCompilation.h"
#include "QuotaManagerDependencyFixture.h"

namespace mozilla::dom::quota::test {

class DOM_Quota_ClientDirectoryLockHandle
    : public QuotaManagerDependencyFixture {
 public:
  static void SetUpTestCase() { ASSERT_NO_FATAL_FAILURE(InitializeFixture()); }

  static void TearDownTestCase() { ASSERT_NO_FATAL_FAILURE(ShutdownFixture()); }
};

TEST_F(DOM_Quota_ClientDirectoryLockHandle, DefaultConstruction) {
  PerformClientDirectoryLockTest(
      GetTestClientMetadata(), [](RefPtr<ClientDirectoryLock> aDirectoryLock) {
        ASSERT_TRUE(aDirectoryLock);

        ClientDirectoryLockHandle handle;

        EXPECT_FALSE(handle);

        DIAGNOSTICONLY(EXPECT_TRUE(handle.IsInert()));

        aDirectoryLock->Drop();
      });
}

TEST_F(DOM_Quota_ClientDirectoryLockHandle, ConstructionWithLock) {
  PerformClientDirectoryLockTest(
      GetTestClientMetadata(), [](RefPtr<ClientDirectoryLock> aDirectoryLock) {
        ASSERT_TRUE(aDirectoryLock);

        ClientDirectoryLockHandle handle(std::move(aDirectoryLock));

        EXPECT_TRUE(handle);

        DIAGNOSTICONLY(EXPECT_FALSE(handle.IsInert()));
      });
}

TEST_F(DOM_Quota_ClientDirectoryLockHandle, MoveConstruction) {
  PerformClientDirectoryLockTest(
      GetTestClientMetadata(), [](RefPtr<ClientDirectoryLock> aDirectoryLock) {
        ASSERT_TRUE(aDirectoryLock);

        ClientDirectoryLockHandle handle1(std::move(aDirectoryLock));
        ClientDirectoryLockHandle handle2(std::move(handle1));

        EXPECT_FALSE(handle1);
        EXPECT_TRUE(handle2);

        DIAGNOSTICONLY(EXPECT_TRUE(handle1.IsInert()));
        DIAGNOSTICONLY(EXPECT_FALSE(handle2.IsInert()));
      });
}

TEST_F(DOM_Quota_ClientDirectoryLockHandle, MoveAssignment) {
  PerformClientDirectoryLockTest(
      GetTestClientMetadata(), [](RefPtr<ClientDirectoryLock> aDirectoryLock) {
        ASSERT_TRUE(aDirectoryLock);

        ClientDirectoryLockHandle handle1(std::move(aDirectoryLock));
        ClientDirectoryLockHandle handle2;
        handle2 = std::move(handle1);

        EXPECT_FALSE(handle1);
        EXPECT_TRUE(handle2);

        DIAGNOSTICONLY(EXPECT_TRUE(handle1.IsInert()));
        DIAGNOSTICONLY(EXPECT_FALSE(handle2.IsInert()));
      });
}

}  // namespace mozilla::dom::quota::test
