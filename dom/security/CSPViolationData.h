/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SECURITY_CSPVIOLATION_H_
#define DOM_SECURITY_CSPVIOLATION_H_

#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "nsString.h"
#include "mozilla/Variant.h"

#include <cstdint>

class nsIURI;

namespace mozilla::dom {
// Represents parts of <https://w3c.github.io/webappsec-csp/#violation>.
struct CSPViolationData {
  enum class BlockedContentSource {
    Unknown,
    Inline,
    Eval,
    Self,
    WasmEval,
  };

  using Resource = mozilla::Variant<nsCOMPtr<nsIURI>, BlockedContentSource>;

  // @param aSample Will be truncated if necessary.
  CSPViolationData(uint32_t aViolatedPolicyIndex, Resource&& aResource,
                   uint32_t aLineNumber, uint32_t aColumnNumber,
                   const nsAString& aSample);

  BlockedContentSource BlockedContentSourceOrUnknown() const;

  const uint32_t mViolatedPolicyIndex;
  const Resource mResource;
  const uint32_t mLineNumber;
  const uint32_t mColumnNumber;
  const nsString mSample;
};
}  // namespace mozilla::dom

#endif  // DOM_SECURITY_CSPVIOLATION_H_
