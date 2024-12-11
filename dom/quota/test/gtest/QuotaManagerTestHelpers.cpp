/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaManagerTestHelpers.h"

#include "mozilla/dom/quota/CommonMetadata.h"
#include "nsString.h"

namespace mozilla::dom::quota::test {

PrincipalMetadata GetPrincipalMetadata(const nsCString& aGroup,
                                       const nsCString& aOriginNoSuffix) {
  return PrincipalMetadata{""_ns, aGroup, aOriginNoSuffix, aOriginNoSuffix,
                           /* aIsPrivate */ false};
}

PrincipalMetadata GetPrincipalMetadata(const nsCString& aSuffix,
                                       const nsCString& aGroupNoSuffix,
                                       const nsCString& aOriginNoSuffix) {
  nsCString group = aGroupNoSuffix + aSuffix;
  nsCString origin = aOriginNoSuffix + aSuffix;

  return PrincipalMetadata{aSuffix, group, origin, origin,
                           /* aIsPrivate */ false};
}

}  // namespace mozilla::dom::quota::test
