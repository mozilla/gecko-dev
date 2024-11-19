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
#include "mozilla/dom/TrustedScript.h"
#include "mozilla/dom/TrustedScriptURL.h"
#include "mozilla/dom/TrustedTypePolicy.h"
#include "mozilla/dom/TrustedTypePolicyFactory.h"
#include "nsGlobalWindowInner.h"
#include "nsLiteralString.h"
#include "nsTArray.h"
#include "xpcpublic.h"

#include "mozilla/dom/CSPViolationData.h"
#include "mozilla/dom/ElementBinding.h"
#include "mozilla/dom/nsCSPUtils.h"

#include "nsContentUtils.h"
#include "nsIContentSecurityPolicy.h"

namespace mozilla::dom::TrustedTypeUtils {

template <typename T>
nsString GetTrustedTypeName() {
  if constexpr (std::is_same_v<T, TrustedHTML>) {
    return u"TrustedHTML"_ns;
  }
  if constexpr (std::is_same_v<T, TrustedScript>) {
    return u"TrustedScript"_ns;
  }
  MOZ_ASSERT((std::is_same_v<T, TrustedScriptURL>));
  return u"TrustedScriptURL"_ns;
}

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

template <typename ExpectedType>
void ProcessValueWithADefaultPolicy(const Document& aDocument,
                                    const nsAString& aInput,
                                    const nsAString& aSink,
                                    ExpectedType** aResult,
                                    ErrorResult& aError) {
  *aResult = nullptr;

  // TODO(bug 1928929): We should also be able to get the policy factory from
  // a worker's global scope.
  nsPIDOMWindowInner* piDOMWindowInner =
      GetScopeObjectAsInnerWindow(aDocument, aError);
  if (aError.Failed()) {
    return;
  }
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

  JS::Rooted<JS::Value> trustedTypeName{cx};
  using ExpectedTypeArg =
      std::remove_const_t<std::remove_reference_t<decltype(**aResult)>>;
  if (!xpc::NonVoidStringToJsval(cx, GetTrustedTypeName<ExpectedTypeArg>(),
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
  if constexpr (std::is_same_v<ExpectedTypeArg, TrustedHTML>) {
    RefPtr<CreateHTMLCallback> callbackObject =
        defaultPolicy->GetOptions().mCreateHTMLCallback;
    defaultPolicy->DetermineTrustedPolicyValue(
        callbackObject, aInput, arguments,
        /* aThrowIfMissing */ false, aError, policyValue);
  } else if constexpr (std::is_same_v<ExpectedTypeArg, TrustedScript>) {
    RefPtr<CreateScriptCallback> callbackObject =
        defaultPolicy->GetOptions().mCreateScriptCallback;
    defaultPolicy->DetermineTrustedPolicyValue(
        callbackObject, aInput, arguments,
        /* aThrowIfMissing */ false, aError, policyValue);
  } else {
    MOZ_ASSERT((std::is_same_v<ExpectedTypeArg, TrustedScriptURL>));
    RefPtr<CreateScriptURLCallback> callbackObject =
        defaultPolicy->GetOptions().mCreateScriptURLCallback;
    defaultPolicy->DetermineTrustedPolicyValue(
        callbackObject, aInput, arguments,
        /* aThrowIfMissing */ false, aError, policyValue);
  }

  if (aError.Failed()) {
    return;
  }

  if (policyValue.IsVoid()) {
    return;
  }

  MakeRefPtr<ExpectedType>(policyValue).forget(aResult);
}

template <typename ExpectedType, typename TrustedTypeOrString>
MOZ_CAN_RUN_SCRIPT inline const nsAString* GetTrustedTypesCompliantString(
    const TrustedTypeOrString& aInput, const nsAString& aSink,
    const nsAString& aSinkGroup, const nsINode& aNode,
    Maybe<nsAutoString>& aResultHolder, ErrorResult& aError) {
  using TrustedTypeOrStringArg =
      std::remove_const_t<std::remove_reference_t<decltype(aInput)>>;
  auto isString = [&aInput] {
    if constexpr (std::is_same_v<TrustedTypeOrStringArg, TrustedHTMLOrString>) {
      return aInput.IsString();
    }
    if constexpr (std::is_same_v<TrustedTypeOrStringArg,
                                 TrustedHTMLOrNullIsEmptyString>) {
      return aInput.IsNullIsEmptyString();
    }
    MOZ_ASSERT_UNREACHABLE();
    return false;
  };
  auto getAsString = [&aInput] {
    if constexpr (std::is_same_v<TrustedTypeOrStringArg, TrustedHTMLOrString>) {
      return &aInput.GetAsString();
    }
    if constexpr (std::is_same_v<TrustedTypeOrStringArg,
                                 TrustedHTMLOrNullIsEmptyString>) {
      return &aInput.GetAsNullIsEmptyString();
    }
    MOZ_ASSERT_UNREACHABLE();
    return static_cast<const nsAString*>(&EmptyString());
  };
  auto isTrustedType = [&aInput] {
    if constexpr (std::is_same_v<TrustedTypeOrStringArg, TrustedHTMLOrString> ||
                  std::is_same_v<TrustedTypeOrStringArg,
                                 TrustedHTMLOrNullIsEmptyString>) {
      return aInput.IsTrustedHTML();
    }
    MOZ_ASSERT_UNREACHABLE();
    return false;
  };
  auto getAsTrustedType = [&aInput] {
    if constexpr (std::is_same_v<TrustedTypeOrStringArg, TrustedHTMLOrString> ||
                  std::is_same_v<TrustedTypeOrStringArg,
                                 TrustedHTMLOrNullIsEmptyString>) {
      return &aInput.GetAsTrustedHTML().mData;
    }
    MOZ_ASSERT_UNREACHABLE();
    return &EmptyString();
  };

  if (!StaticPrefs::dom_security_trusted_types_enabled()) {
    // A trusted type might've been created before the pref was set to `false`.
    return isString() ? getAsString() : getAsTrustedType();
  }

  if (isTrustedType()) {
    return getAsTrustedType();
  }

  // Below, we use fast paths when there are no require-trusted-types-for
  // directives. Note that the global object's CSP may differ from the
  // owner-document's one. E.g. when aDocument was created by
  // `document.implementation.createHTMLDocument` and it's not connected to a
  // browsing context.
  Document* ownerDoc = aNode.OwnerDoc();
  const bool ownerDocLoadedAsData = ownerDoc->IsLoadedAsData();
  if (!ownerDoc->HasPolicyWithRequireTrustedTypesForDirective() &&
      !ownerDocLoadedAsData) {
    return getAsString();
  }
  nsPIDOMWindowInner* piDOMWindowInner =
      GetScopeObjectAsInnerWindow(*ownerDoc, aError);
  if (aError.Failed()) {
    return nullptr;
  }
  if (ownerDocLoadedAsData && piDOMWindowInner->GetExtantDoc() &&
      !piDOMWindowInner->GetExtantDoc()
           ->HasPolicyWithRequireTrustedTypesForDirective()) {
    return getAsString();
  }
  RefPtr<nsIContentSecurityPolicy> csp = piDOMWindowInner->GetCsp();

  if (!DoesSinkTypeRequireTrustedTypes(csp, aSinkGroup)) {
    return getAsString();
  }

  RefPtr<ExpectedType> convertedInput;
  RefPtr<const Document> pinnedDoc{ownerDoc};
  ProcessValueWithADefaultPolicy<ExpectedType>(
      *pinnedDoc, *getAsString(), aSink, getter_AddRefs(convertedInput),
      aError);

  if (aError.Failed()) {
    return nullptr;
  }

  if (!convertedInput) {
    if (ShouldSinkTypeMismatchViolationBeBlockedByCSP(csp, aSink, aSinkGroup,
                                                      *getAsString()) ==
        SinkTypeMismatch::Value::Allowed) {
      return getAsString();
    }

    aError.ThrowTypeError("Sink type mismatch violation blocked by CSP"_ns);
    return nullptr;
  }

  aResultHolder = Some(convertedInput->mData);
  return aResultHolder.ptr();
}

#define IMPL_GET_TRUSTED_TYPES_COMPLIANT_STRING(_trustedTypeOrString, \
                                                _expectedType)        \
  const nsAString* GetTrustedTypesCompliantString(                    \
      const _trustedTypeOrString& aInput, const nsAString& aSink,     \
      const nsAString& aSinkGroup, const nsINode& aNode,              \
      Maybe<nsAutoString>& aResultHolder, ErrorResult& aError) {      \
    return GetTrustedTypesCompliantString<_expectedType>(             \
        aInput, aSink, aSinkGroup, aNode, aResultHolder, aError);     \
  }

IMPL_GET_TRUSTED_TYPES_COMPLIANT_STRING(TrustedHTMLOrString, TrustedHTML);
IMPL_GET_TRUSTED_TYPES_COMPLIANT_STRING(TrustedHTMLOrNullIsEmptyString,
                                        TrustedHTML);

}  // namespace mozilla::dom::TrustedTypeUtils
