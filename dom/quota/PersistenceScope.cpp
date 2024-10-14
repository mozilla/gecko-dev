/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PersistenceScope.h"

namespace mozilla::dom::quota {

bool MatchesPersistentPersistenceScope(
    const PersistenceScope& aPersistenceScope) {
  static PersistenceScope scope(
      PersistenceScope::CreateFromValue(PERSISTENCE_TYPE_PERSISTENT));

  return aPersistenceScope.Matches(scope);
}

bool MatchesBestEffortPersistenceScope(
    const PersistenceScope& aPersistenceScope) {
  static PersistenceScope scope(PersistenceScope::CreateFromSet(
      PERSISTENCE_TYPE_TEMPORARY, PERSISTENCE_TYPE_DEFAULT,
      PERSISTENCE_TYPE_PRIVATE));

  return aPersistenceScope.Matches(scope);
}

}  // namespace mozilla::dom::quota
