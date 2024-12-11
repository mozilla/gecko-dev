/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsStringFwd.h"

namespace mozilla::dom::quota {

struct PrincipalMetadata;

namespace test {

/**
 * Creates a PrincipalMetadata object for a principal without an origin suffix.
 *
 * This function takes a group (without suffix) and origin (without suffix) and
 * returns a PrincipalMetadata object with these values. The suffix and private
 * browsing flag are set to their default values: an empty string and false,
 * respectively.
 *
 * @param aGroupNoSuffix The group associated with the principal, without the
 * suffix.
 * @param aOriginNoSuffix The origin without the suffix.
 *
 * @returns A PrincipalMetadata object containing the given group and origin,
 * with an empty origin suffix and a false private browsing flag.
 */
PrincipalMetadata GetPrincipalMetadata(const nsCString& aGroupNoSuffix,
                                       const nsCString& aOriginNoSuffix);

/**
 * Creates a PrincipalMetadata object for a principal with an origin suffix.
 *
 * This function takes an origin suffix, a group (without suffix), and an origin
 * (without suffix), and returns a PrincipalMetadata object with these values,
 * including the origin suffix. The private browsing flag is set to false by
 * default.
 *
 * @param aOriginSuffix The suffix to be added to the group and origin.
 * @param aGroupNoSuffix The group associated with the principal, without the
 * suffix.
 * @param aOriginNoSuffix The origin without the suffix.
 *
 * @returns A PrincipalMetadata object containing the given suffix, group, and
 * origin, with a false private browsing flag.
 */
PrincipalMetadata GetPrincipalMetadata(const nsCString& aOriginSuffix,
                                       const nsCString& aGroupNoSuffix,
                                       const nsCString& aOriginNoSuffix);

}  //  namespace test
}  //  namespace mozilla::dom::quota
