/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxFontSrcPrincipal.h"

#include "nsURIHashKey.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/HashFunctions.h"

using mozilla::BasePrincipal;

gfxFontSrcPrincipal::gfxFontSrcPrincipal(nsIPrincipal* aNodePrincipal,
                                         nsIPrincipal* aStoragePrincipal)
    : mNodePrincipal(aNodePrincipal), mStoragePrincipal(aStoragePrincipal) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aNodePrincipal);
  MOZ_ASSERT(aStoragePrincipal);
  mHash = mStoragePrincipal->GetHashValue();
}

gfxFontSrcPrincipal::~gfxFontSrcPrincipal() = default;

bool gfxFontSrcPrincipal::Equals(gfxFontSrcPrincipal* aOther) {
  return BasePrincipal::Cast(mStoragePrincipal)
      ->FastEquals(BasePrincipal::Cast(aOther->mStoragePrincipal));
}
