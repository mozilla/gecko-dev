/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TrustedTypeUtils.h"

#include "mozilla/Maybe.h"
#include "mozilla/StaticPrefs_dom.h"

#include "js/RootingAPI.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/TrustedHTML.h"
#include "mozilla/dom/TrustedTypePolicy.h"
#include "mozilla/dom/TrustedTypePolicyFactory.h"
#include "nsGlobalWindowInner.h"
#include "nsLiteralString.h"
#include "nsTArray.h"
#include "xpcpublic.h"

#include "mozilla/dom/CSPViolationData.h"
#include "mozilla/dom/ElementBinding.h"
#include "mozilla/dom/nsCSPUtils.h"
#include "mozilla/dom/TrustedHTML.h"

#include "nsContentUtils.h"
#include "nsIContentSecurityPolicy.h"

namespace mozilla::dom::TrustedTypeUtils {

// https://w3c.github.io/trusted-types/dist/spec/#abstract-opdef-does-sink-type-require-trusted-types
static bool DoesSinkTypeRequireTrustedTypes(nsIContentSecurityPolicy* aCSP,
                                            const nsAString& aSinkGroup) {
  if (!aCSP || !aCSP->GetHasPolicyWithRequireTrustedTypesForDirective()) {
    return false;
  }
  uint32_t numPolicies = 0;
  aCSP->GetPolicyCount(&numPolicies);
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
  MOZ_ASSERT(aCSP && aCSP->GetHasPolicyWithRequireTrustedTypesForDirective());
  SinkTypeMismatch::Value result = SinkTypeMismatch::Value::Allowed;

  uint32_t numPolicies = 0;
  aCSP->GetPolicyCount(&numPolicies);

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

static nsPIDOMWindowInner* GetScopeObjectAsInnerWindow(
    const Document& aDocument, ErrorResult& aError) {
  nsIGlobalObject* globalObject = aDocument.GetScopeObject();
  if (!globalObject) {
    aError.ThrowTypeError("No global object");
    return nullptr;
  }

  nsPIDOMWindowInner* piDOMWindowInner = globalObject->GetAsInnerWindow();
  if (!piDOMWindowInner) {
    aError.ThrowTypeError("No inner window");
    return nullptr;
  }

  return piDOMWindowInner;
}

constexpr size_t kNumArgumentsForDetermineTrustedTypePolicyValue = 2;

void ProcessValueWithADefaultPolicy(const Document& aDocument,
                                    const nsAString& aInput,
                                    const nsAString& aSink,
                                    TrustedHTML** aResult,
                                    ErrorResult& aError) {
  *aResult = nullptr;

  nsPIDOMWindowInner* piDOMWindowInner =
      GetScopeObjectAsInnerWindow(aDocument, aError);
  if (aError.Failed()) {
    return;
  }

  // Since this function is for `TrustedHTML`, the `TrustedTypePolicyFactory`
  // has to stem from the inner window, not from a Worker.
  nsGlobalWindowInner* globalWindowInner =
      nsGlobalWindowInner::Cast(piDOMWindowInner);
  const TrustedTypePolicyFactory* trustedTypePolicyFactory =
      globalWindowInner->TrustedTypes();
  const RefPtr<TrustedTypePolicy> defaultPolicy =
      trustedTypePolicyFactory->GetDefaultPolicy();
  if (!defaultPolicy) {
    return;
  }

  JSContext* cx = nsContentUtils::GetCurrentJSContext();
  if (!cx) {
    return;
  }

  RefPtr<CreateHTMLCallback> callbackObject =
      defaultPolicy->GetOptions().mCreateHTMLCallback;

  JS::Rooted<JS::Value> trustedTypeName{cx};
  if (!xpc::NonVoidLatin1StringToJsval(cx, "TrustedHTML"_ns,
                                       &trustedTypeName)) {
    aError.StealExceptionFromJSContext(cx);
    return;
  }

  JS::Rooted<JS::Value> sink{cx};
  if (!xpc::NonVoidStringToJsval(cx, aSink, &sink)) {
    aError.StealExceptionFromJSContext(cx);
    return;
  }

  AutoTArray<JS::Value, kNumArgumentsForDetermineTrustedTypePolicyValue>
      arguments = {trustedTypeName, sink};

  nsString policyValue;
  defaultPolicy->DetermineTrustedPolicyValue(callbackObject, aInput, arguments,
                                             /* aThrowIfMissing */ false,
                                             aError, policyValue);

  if (aError.Failed()) {
    return;
  }

  if (policyValue.IsVoid()) {
    return;
  }

  RefPtr<TrustedHTML>{new TrustedHTML(policyValue)}.forget(aResult);
}

#define IMPL_GET_TRUSTED_TYPES_COMPLIANT_STRING_FOR_TRUSTED_HTML(              \
    _class, _stringCheck, _stringGetter)                                       \
  const nsAString* GetTrustedTypesCompliantString(                             \
      const _class& aInput, const nsAString& aSink,                            \
      const nsAString& aSinkGroup, const nsINode& aNode,                       \
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
    /* Below, we use fast paths when there are no require-trusted-types-for */ \
    /* directives. Note that the global object's CSP may differ from the    */ \
    /* owner-document's one.                                                */ \
    /* E.g. when aDocument was created by                                   */ \
    /* `document.implementation.createHTMLDocument` and it's not connected  */ \
    /* to a browsing context.                                               */ \
    Document* ownerDoc = aNode.OwnerDoc();                                     \
    const bool ownerDocLoadedAsData = ownerDoc->IsLoadedAsData();              \
    if (!ownerDoc->HasPolicyWithRequireTrustedTypesForDirective() &&           \
        !ownerDocLoadedAsData) {                                               \
      return &aInput._stringGetter();                                          \
    }                                                                          \
    nsPIDOMWindowInner* piDOMWindowInner =                                     \
        GetScopeObjectAsInnerWindow(*ownerDoc, aError);                        \
    if (aError.Failed()) {                                                     \
      return nullptr;                                                          \
    }                                                                          \
    if (ownerDocLoadedAsData && piDOMWindowInner->GetExtantDoc() &&            \
        !piDOMWindowInner->GetExtantDoc()                                      \
             ->HasPolicyWithRequireTrustedTypesForDirective()) {               \
      return &aInput._stringGetter();                                          \
    }                                                                          \
    RefPtr<nsIContentSecurityPolicy> csp = piDOMWindowInner->GetCsp();         \
                                                                               \
    if (!DoesSinkTypeRequireTrustedTypes(csp, aSinkGroup)) {                   \
      return &aInput._stringGetter();                                          \
    }                                                                          \
                                                                               \
    RefPtr<TrustedHTML> convertedInput;                                        \
    RefPtr<const Document> pinnedDoc{ownerDoc};                                \
    ProcessValueWithADefaultPolicy(*pinnedDoc, aInput._stringGetter(), aSink,  \
                                   getter_AddRefs(convertedInput), aError);    \
                                                                               \
    if (aError.Failed()) {                                                     \
      return nullptr;                                                          \
    }                                                                          \
                                                                               \
    if (!convertedInput) {                                                     \
      if (ShouldSinkTypeMismatchViolationBeBlockedByCSP(                       \
              csp, aSink, aSinkGroup, aInput._stringGetter()) ==               \
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
