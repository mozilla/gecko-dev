/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/dom/quota/CommonMetadata.h"
#include "nsString.h"
#include "QuotaManagerTestHelpers.h"

namespace mozilla::dom::quota::test {

// Tests that OriginMetadata::GetCompositeKey returns the expected
// "<persistence>*<origin>" string.
TEST(DOM_Quota_CommonMetadata, OriginMetadata_GetCompositeKey)
{
  auto originMetadata =
      GetOriginMetadata(""_ns, "mozilla.org"_ns, "http://www.mozilla.org"_ns);

  auto compositeKey = originMetadata.GetCompositeKey();

  EXPECT_STREQ(compositeKey.get(), "2*http://www.mozilla.org");
}

}  //  namespace mozilla::dom::quota::test
