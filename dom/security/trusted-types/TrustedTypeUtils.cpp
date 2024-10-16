/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TrustedTypeUtils.h"

#include "mozilla/Maybe.h"
#include "mozilla/StaticPrefs_dom.h"

#include "mozilla/dom/CSPViolationData.h"
#include "mozilla/dom/ElementBinding.h"
#include "mozilla/dom/nsCSPUtils.h"
#include "mozilla/dom/TrustedHTML.h"

#include "nsIContentSecurityPolicy.h"

namespace mozilla::dom::TrustedTypeUtils {

// https://w3c.github.io/trusted-types/dist/spec/#abstract-opdef-does-sink-type-require-trusted-types
static bool DoesSinkTypeRequireTrustedTypes(nsIContentSecurityPolicy* aCSP,
                                            const nsAString& aSinkGroup) {
  uint32_t numPolicies = 0;
  if (aCSP) {
    aCSP->GetPolicyCount(&numPolicies);
  }
  for (uint32_t i = 0; i < numPolicies; ++i) {
    const nsCSPPolicy* policy = aCSP->GetPolicy(i);

    if (policy->AreTrustedTypesForSinkGroupRequired(aSinkGroup)) {
      return true;
    }
  }

  return false;
}

namespace SinkTypeMismatch {
enum class Value { Blocked, Allowed };

static constexpr size_t kTrimmedSourceLength = 40;
static constexpr nsLiteralString kSampleSeparator = u"|"_ns;
}  // namespace SinkTypeMismatch

// https://w3c.github.io/trusted-types/dist/spec/#abstract-opdef-should-sink-type-mismatch-violation-be-blocked-by-content-security-policy
static SinkTypeMismatch::Value ShouldSinkTypeMismatchViolationBeBlockedByCSP(
    nsIContentSecurityPolicy* aCSP, const nsAString& aSink,
    const nsAString& aSinkGroup, const nsAString& aSource) {
  SinkTypeMismatch::Value result = SinkTypeMismatch::Value::Allowed;

  uint32_t numPolicies = 0;
  if (aCSP) {
    aCSP->GetPolicyCount(&numPolicies);
  }

  for (uint32_t i = 0; i < numPolicies; ++i) {
    const auto* policy = aCSP->GetPolicy(i);

    if (!policy->AreTrustedTypesForSinkGroupRequired(aSinkGroup)) {
      continue;
    }

    auto caller = JSCallingLocation::Get();

    const nsDependentSubstring trimmedSource = Substring(
        aSource, /* aStartPos */ 0, SinkTypeMismatch::kTrimmedSourceLength);
    const nsString sample =
        aSink + SinkTypeMismatch::kSampleSeparator + trimmedSource;

    CSPViolationData cspViolationData{
        i,
        CSPViolationData::Resource{
            CSPViolationData::BlockedContentSource::TrustedTypesSink},
        nsIContentSecurityPolicy::REQUIRE_TRUSTED_TYPES_FOR_DIRECTIVE,
        caller.FileName(),
        caller.mLine,
        caller.mColumn,
        /* aElement */ nullptr,
        sample};

    // For Workers, a pointer to an object needs to be passed
    // (https://bugzilla.mozilla.org/show_bug.cgi?id=1901492).
    nsICSPEventListener* cspEventListener = nullptr;

    aCSP->LogTrustedTypesViolationDetailsUnchecked(
        std::move(cspViolationData),
        NS_LITERAL_STRING_FROM_CSTRING(
            REQUIRE_TRUSTED_TYPES_FOR_SCRIPT_OBSERVER_TOPIC),
        cspEventListener);

    if (policy->getDisposition() == nsCSPPolicy::Disposition::Enforce) {
      result = SinkTypeMismatch::Value::Blocked;
    }
  }

  return result;
}

// https://w3c.github.io/trusted-types/dist/spec/#abstract-opdef-process-value-with-a-default-policy
// specialized for `TrustedHTML`.
static void ProcessValueWithADefaultPolicy(RefPtr<TrustedHTML>& aResult,
                                           ErrorResult& aError) {
  // TODO: implement default-policy support,
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1903717.

  aResult = nullptr;
}

#define IMPL_GET_TRUSTED_TYPES_COMPLIANT_STRING_FOR_TRUSTED_HTML(              \
    _class, _stringCheck, _stringGetter)                                       \
  const nsAString* GetTrustedTypesCompliantString(                             \
      const _class& aInput, nsIContentSecurityPolicy* aCSP,                    \
      const nsAString& aSink, const nsAString& aSinkGroup,                     \
      Maybe<nsAutoString>& aResultHolder, ErrorResult& aError) {               \
    if (!StaticPrefs::dom_security_trusted_types_enabled()) {                  \
      /* A `TrustedHTML` string might've been created before the pref was set  \
        to `false`. */                                                         \
      return &(aInput._stringCheck() ? aInput._stringGetter()                  \
                                     : aInput.GetAsTrustedHTML().mData);       \
    }                                                                          \
                                                                               \
    if (aInput.IsTrustedHTML()) {                                              \
      return &aInput.GetAsTrustedHTML().mData;                                 \
    }                                                                          \
                                                                               \
    if (!DoesSinkTypeRequireTrustedTypes(aCSP, aSinkGroup)) {                  \
      return &aInput._stringGetter();                                          \
    }                                                                          \
                                                                               \
    RefPtr<TrustedHTML> convertedInput;                                        \
    ProcessValueWithADefaultPolicy(convertedInput, aError);                    \
                                                                               \
    if (aError.Failed()) {                                                     \
      return nullptr;                                                          \
    }                                                                          \
                                                                               \
    if (!convertedInput) {                                                     \
      if (ShouldSinkTypeMismatchViolationBeBlockedByCSP(                       \
              aCSP, aSink, aSinkGroup, aInput._stringGetter()) ==              \
          SinkTypeMismatch::Value::Allowed) {                                  \
        return &aInput._stringGetter();                                        \
      }                                                                        \
                                                                               \
      aError.ThrowTypeError("Sink type mismatch violation blocked by CSP"_ns); \
      return nullptr;                                                          \
    }                                                                          \
                                                                               \
    aResultHolder = Some(convertedInput->mData);                               \
    return aResultHolder.ptr();                                                \
  }

IMPL_GET_TRUSTED_TYPES_COMPLIANT_STRING_FOR_TRUSTED_HTML(TrustedHTMLOrString,
                                                         IsString, GetAsString)
IMPL_GET_TRUSTED_TYPES_COMPLIANT_STRING_FOR_TRUSTED_HTML(
    TrustedHTMLOrNullIsEmptyString, IsNullIsEmptyString, GetAsNullIsEmptyString)

}  // namespace mozilla::dom::TrustedTypeUtils
