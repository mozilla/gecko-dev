/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/quota/ArtificialFailure.h"

#include "mozilla/Result.h"
#include "mozilla/StaticPrefs_dom.h"

namespace mozilla::dom::quota {

Result<Ok, nsresult> ArtificialFailure(
    nsIQuotaArtificialFailure::Category aCategory) {
  MOZ_ASSERT(aCategory != 0 && ((aCategory & (aCategory - 1)) == 0));

  if ((StaticPrefs::dom_quotaManager_artificialFailure_categories() &
       aCategory) == 0) {
    return Ok{};
  }

  uint32_t probability =
      StaticPrefs::dom_quotaManager_artificialFailure_probability();
  if (probability == 0) {
    return Ok{};
  }

  uint32_t randomValue = std::rand() % 100;
  if (randomValue >= probability) {
    return Ok{};
  }

  uint32_t errorCode =
      StaticPrefs::dom_quotaManager_artificialFailure_errorCode();

  nsresult rv = static_cast<nsresult>(errorCode);
  return Err(rv);
}

}  // namespace mozilla::dom::quota
