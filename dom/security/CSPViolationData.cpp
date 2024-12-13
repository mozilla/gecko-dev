/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSPViolationData.h"

#include "nsCharTraits.h"
#include "nsContentUtils.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/nsCSPContext.h"

#include <utility>

namespace mozilla::dom {

/* static */
const nsDependentSubstring CSPViolationData::MaybeTruncateSample(
    const nsAString& aSample) {
  uint32_t length = aSample.Length();
  uint32_t maybeTruncatedLength = nsCSPContext::ScriptSampleMaxLength();
  if (length > maybeTruncatedLength) {
    // Don't cut off right before a low surrogate. Just include it.
    // TODO(bug 1935996): Should we also count previous surrogate pairs as
    // single characters?
    if (NS_IS_LOW_SURROGATE(aSample[maybeTruncatedLength])) {
      maybeTruncatedLength++;
    }
  }
  return Substring(aSample, 0, maybeTruncatedLength);
}

static const nsString MaybeTruncateSampleWithEllipsis(
    const nsAString& aSample) {
  const nsDependentSubstring sample =
      CSPViolationData::MaybeTruncateSample(aSample);
  return sample.Length() < aSample.Length()
             ? sample + nsContentUtils::GetLocalizedEllipsis()
             : nsString(aSample);
}

CSPViolationData::CSPViolationData(uint32_t aViolatedPolicyIndex,
                                   Resource&& aResource,
                                   const CSPDirective aEffectiveDirective,
                                   const nsACString& aSourceFile,
                                   uint32_t aLineNumber, uint32_t aColumnNumber,
                                   Element* aElement, const nsAString& aSample)
    : mViolatedPolicyIndex{aViolatedPolicyIndex},
      mResource{std::move(aResource)},
      mEffectiveDirective{aEffectiveDirective},
      mSourceFile{aSourceFile},
      mLineNumber{aLineNumber},
      mColumnNumber{aColumnNumber},
      mElement{aElement},
      // For TrustedTypesSink, sample is already truncated and formatted in
      // ShouldSinkTypeMismatchViolationBeBlockedByCSP.
      // TODO(bug 1935996): The specifications do not mention adding an
      // ellipsis.
      mSample{BlockedContentSourceOrUnknown() ==
                      BlockedContentSource::TrustedTypesSink
                  ? nsString(aSample)
                  : MaybeTruncateSampleWithEllipsis(aSample)} {}

// Required for `mElement`, since its destructor requires a definition of
// `Element`.
CSPViolationData::~CSPViolationData() = default;

auto CSPViolationData::BlockedContentSourceOrUnknown() const
    -> BlockedContentSource {
  return mResource.is<CSPViolationData::BlockedContentSource>()
             ? mResource.as<CSPViolationData::BlockedContentSource>()
             : CSPViolationData::BlockedContentSource::Unknown;
}
}  // namespace mozilla::dom
