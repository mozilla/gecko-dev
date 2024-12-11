/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsStringFwd.h"

namespace mozilla::dom::quota {

struct PrincipalMetadata;

namespace test {

PrincipalMetadata GetPrincipalMetadata(const nsCString& aGroup,
                                       const nsCString& aOriginNoSuffix);

PrincipalMetadata GetPrincipalMetadata(const nsCString& aSuffix,
                                       const nsCString& aGroupNoSuffix,
                                       const nsCString& aOriginNoSuffix);

}  //  namespace test
}  //  namespace mozilla::dom::quota
