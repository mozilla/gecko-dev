/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SECURITY_CSPVIOLATION_H_
#define DOM_SECURITY_CSPVIOLATION_H_

#include "nsCOMPtr.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIURI.h"
#include "nsString.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Variant.h"

#include <cstdint>

class nsIURI;

namespace mozilla::dom {
class Element;

// Represents parts of <https://w3c.github.io/webappsec-csp/#violation>.
// The remaining parts can be deduced from the corresponding nsCSPContext.
struct CSPViolationData {
  enum class BlockedContentSource {
    Unknown,
    Inline,
    Eval,
    Self,
    WasmEval,
    TrustedTypesPolicy,
  };

  using Resource = mozilla::Variant<nsCOMPtr<nsIURI>, BlockedContentSource>;

  // According to https://github.com/w3c/webappsec-csp/issues/442 column- and
  // line-numbers are expected to be 1-origin.
  //
  // @param aSample Will be truncated if necessary.
  CSPViolationData(uint32_t aViolatedPolicyIndex, Resource&& aResource,
                   const CSPDirective aEffectiveDirective,
                   const nsACString& aSourceFile, uint32_t aLineNumber,
                   uint32_t aColumnNumber, Element* aElement,
                   const nsAString& aSample);

  ~CSPViolationData();

  BlockedContentSource BlockedContentSourceOrUnknown() const;

  const uint32_t mViolatedPolicyIndex;
  const Resource mResource;
  const CSPDirective mEffectiveDirective;
  // String representation of the URL. The empty string represents a null-URL.
  const nsCString mSourceFile;
  const uint32_t mLineNumber;
  const uint32_t mColumnNumber;
  RefPtr<Element> mElement;
  const nsString mSample;
};
}  // namespace mozilla::dom

#endif  // DOM_SECURITY_CSPVIOLATION_H_
