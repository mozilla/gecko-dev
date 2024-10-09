/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/quota/MozPromiseUtils.h"
#include "QuotaManagerDependencyFixture.h"

namespace mozilla::dom::quota::test {

TEST(DOM_Quota_MozPromiseUtils, BoolPromiseToBoolPromise)
{
  auto value = QuotaManagerDependencyFixture::Await(Map<BoolPromise>(
      BoolPromise::CreateAndResolve(true, __func__),
      [](const BoolPromise::ResolveOrRejectValue& aValue) { return false; }));

  ASSERT_TRUE(value.IsResolve());
  ASSERT_FALSE(value.ResolveValue());
}

TEST(DOM_Quota_MozPromiseUtils, ExclusiveBoolPromiseToBoolPromise)
{
  auto value = QuotaManagerDependencyFixture::Await(
      Map<BoolPromise>(ExclusiveBoolPromise::CreateAndResolve(true, __func__),
                       [](ExclusiveBoolPromise::ResolveOrRejectValue&& aValue) {
                         return false;
                       }));

  ASSERT_TRUE(value.IsResolve());
  ASSERT_FALSE(value.ResolveValue());
}

}  // namespace mozilla::dom::quota::test
