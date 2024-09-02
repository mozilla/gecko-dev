/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TrustedTypePolicyFactory.h"

#include <utility>

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/CSPViolationData.h"
#include "mozilla/dom/TrustedTypePolicy.h"
#include "mozilla/dom/TrustedTypeUtils.h"
#include "mozilla/dom/nsCSPUtils.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TrustedTypePolicyFactory, mGlobalObject)

JSObject* TrustedTypePolicyFactory::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return TrustedTypePolicyFactory_Binding::Wrap(aCx, this, aGivenProto);
}

constexpr size_t kCreatePolicyCSPViolationMaxSampleLength = 40;

static CSPViolationData CreateCSPViolationData(JSContext* aJSContext,
                                               uint32_t aPolicyIndex,
                                               const nsAString& aPolicyName) {
  auto caller = JSCallingLocation::Get(aJSContext);
  const nsAString& sample =
      Substring(aPolicyName, /* aStartPos */ 0,
                /* aLength */ kCreatePolicyCSPViolationMaxSampleLength);

  return {aPolicyIndex,
          CSPViolationData::Resource{
              CSPViolationData::BlockedContentSource::TrustedTypesPolicy},
          nsIContentSecurityPolicy::TRUSTED_TYPES_DIRECTIVE,
          caller.FileName(),
          caller.mLine,
          caller.mColumn,
          /* aElement */ nullptr,
          sample};
}

auto TrustedTypePolicyFactory::ShouldTrustedTypePolicyCreationBeBlockedByCSP(
    JSContext* aJSContext,
    const nsAString& aPolicyName) const -> PolicyCreation {
  // CSP-support for Workers will be added in
  // <https://bugzilla.mozilla.org/show_bug.cgi?id=1901492>.
  // That is, currently only Windows are supported.
  nsIContentSecurityPolicy* csp =
      mGlobalObject->GetAsInnerWindow()
          ? mGlobalObject->GetAsInnerWindow()->GetCsp()
          : nullptr;

  auto result = PolicyCreation::Allowed;

  if (csp) {
    uint32_t numPolicies = 0;
    csp->GetPolicyCount(&numPolicies);

    for (uint64_t i = 0; i < numPolicies; ++i) {
      const nsCSPPolicy* policy = csp->GetPolicy(i);
      if (policy->hasDirective(
              nsIContentSecurityPolicy::TRUSTED_TYPES_DIRECTIVE)) {
        if (policy->ShouldCreateViolationForNewTrustedTypesPolicy(
                aPolicyName, mCreatedPolicyNames)) {
          // Only required for Workers;
          // https://bugzilla.mozilla.org/show_bug.cgi?id=1901492.
          nsICSPEventListener* cspEventListener{nullptr};

          CSPViolationData cspViolationData{
              CreateCSPViolationData(aJSContext, i, aPolicyName)};

          csp->LogTrustedTypesViolationDetailsUnchecked(
              std::move(cspViolationData), cspEventListener);

          if (policy->getDisposition() == nsCSPPolicy::Disposition::Enforce) {
            result = PolicyCreation::Blocked;
          }
        }
      }
    }
  }

  return result;
}

already_AddRefed<TrustedTypePolicy> TrustedTypePolicyFactory::CreatePolicy(
    JSContext* aJSContext, const nsAString& aPolicyName,
    const TrustedTypePolicyOptions& aPolicyOptions, ErrorResult& aRv) {
  if (PolicyCreation::Blocked ==
      ShouldTrustedTypePolicyCreationBeBlockedByCSP(aJSContext, aPolicyName)) {
    nsCString errorMessage =
        "Content-Security-Policy blocked creating policy named '"_ns +
        NS_ConvertUTF16toUTF8(aPolicyName) + "'"_ns;

    // TODO: perhaps throw different TypeError messages,
    //       https://github.com/w3c/trusted-types/issues/511.
    aRv.ThrowTypeError(errorMessage);
    return nullptr;
  }

  // TODO: add default policy support; this requires accessing the default
  //       policy on the C++ side, hence already now ref-counting policy
  //       objects.

  TrustedTypePolicy::Options options;

  if (aPolicyOptions.mCreateHTML.WasPassed()) {
    options.mCreateHTMLCallback = &aPolicyOptions.mCreateHTML.Value();
  }

  if (aPolicyOptions.mCreateScript.WasPassed()) {
    options.mCreateScriptCallback = &aPolicyOptions.mCreateScript.Value();
  }

  if (aPolicyOptions.mCreateScriptURL.WasPassed()) {
    options.mCreateScriptURLCallback = &aPolicyOptions.mCreateScriptURL.Value();
  }

  RefPtr<TrustedTypePolicy> policy =
      MakeRefPtr<TrustedTypePolicy>(this, aPolicyName, std::move(options));

  mCreatedPolicyNames.AppendElement(aPolicyName);

  return policy.forget();
}

#define IS_TRUSTED_TYPE_IMPL(_trustedTypeSuffix)                                                                                                                         \
  bool TrustedTypePolicyFactory::Is##_trustedTypeSuffix(                                                                                                                 \
      JSContext*, const JS::Handle<JS::Value>& aValue) const {                                                                                                           \
    /**                                                                                                                                                                  \
     * No need to check the internal slot.                                                                                                                               \
     * Ensured by the corresponding test:                                                                                                                                \
     * <https://searchfox.org/mozilla-central/rev/b60cb73160843adb5a5a3ec8058e75a69b46acf7/testing/web-platform/tests/trusted-types/TrustedTypePolicyFactory-isXXX.html> \
     */                                                                                                                                                                  \
    return aValue.isObject() &&                                                                                                                                          \
           IS_INSTANCE_OF(Trusted##_trustedTypeSuffix, &aValue.toObject());                                                                                              \
  }

IS_TRUSTED_TYPE_IMPL(HTML);
IS_TRUSTED_TYPE_IMPL(Script);
IS_TRUSTED_TYPE_IMPL(ScriptURL);

already_AddRefed<TrustedHTML> TrustedTypePolicyFactory::EmptyHTML() {
  // Preserving the wrapper ensures:
  // ```
  //  const e = trustedTypes.emptyHTML;
  //  e === trustedTypes.emptyHTML;
  // ```
  // which comes with the cost of keeping the factory, one per global, alive.
  // An additional benefit is it saves the cost of re-instantiating potentially
  // multiple emptyHML objects. Both, the JS- and the C++-objects.
  dom::PreserveWrapper(this);

  RefPtr<TrustedHTML> result = new TrustedHTML(EmptyString());
  return result.forget();
}

already_AddRefed<TrustedScript> TrustedTypePolicyFactory::EmptyScript() {
  // See the explanation in `EmptyHTML()`.
  dom::PreserveWrapper(this);

  RefPtr<TrustedScript> result = new TrustedScript(EmptyString());
  return result.forget();
}

}  // namespace mozilla::dom
