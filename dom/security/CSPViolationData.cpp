/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSPViolationData.h"

#include <utility>

namespace mozilla::dom {
CSPViolationData::CSPViolationData(uint32_t aViolatedPolicyIndex,
                                   Resource&& aResource)
    : mViolatedPolicyIndex{aViolatedPolicyIndex},
      mResource{std::move(aResource)} {}

auto CSPViolationData::BlockedContentSourceOrUnknown() const
    -> BlockedContentSource {
  return mResource.is<CSPViolationData::BlockedContentSource>()
             ? mResource.as<CSPViolationData::BlockedContentSource>()
             : CSPViolationData::BlockedContentSource::Unknown;
}
}  // namespace mozilla::dom
