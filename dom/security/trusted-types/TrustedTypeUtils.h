/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SECURITY_TRUSTED_TYPES_TRUSTEDTYPEUTILS_H_
#define DOM_SECURITY_TRUSTED_TYPES_TRUSTEDTYPEUTILS_H_

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/DOMString.h"
#include "mozilla/dom/SessionStoreUtils.h"
#include "mozilla/dom/TrustedTypesBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"
#include "nsString.h"
#include "js/TypeDecls.h"

class nsIContentSecurityPolicy;

namespace mozilla {

class ErrorResult;

template <typename T>
class Maybe;

namespace dom {

class TrustedHTMLOrString;
class TrustedScript;
class TrustedScriptOrString;
class TrustedScriptOrNullIsEmptyString;
class FunctionOrTrustedScriptOrString;
class TrustedScriptURL;
class TrustedScriptURLOrString;
class TrustedScriptURLOrUSVString;
class TrustedHTMLOrTrustedScriptOrTrustedScriptURLOrString;

namespace TrustedTypeUtils {

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

enum class TrustedType : int8_t {
  TrustedHTML,
  TrustedScript,
  TrustedScriptURL,
};
nsString GetTrustedTypeName(TrustedType aTrustedType);

// https://w3c.github.io/trusted-types/dist/spec/#get-trusted-type-compliant-string-algorithm
//
// May only run script if aInput is not a trusted type and if the trusted types
// pref is set to `true`. If this changes, callees might require adjusting.
//
// @param aResultHolder Keeps the compliant string alive when necessary.
// @return The compliant string if aError didn't fail.
MOZ_CAN_RUN_SCRIPT const nsAString* GetTrustedTypesCompliantString(
    const TrustedHTMLOrString& aInput, const nsAString& aSink,
    const nsAString& aSinkGroup, const nsINode& aNode,
    Maybe<nsAutoString>& aResultHolder, ErrorResult& aError);
MOZ_CAN_RUN_SCRIPT const nsAString* GetTrustedTypesCompliantString(
    const TrustedHTMLOrNullIsEmptyString& aInput, const nsAString& aSink,
    const nsAString& aSinkGroup, const nsINode& aNode,
    Maybe<nsAutoString>& aResultHolder, ErrorResult& aError);
MOZ_CAN_RUN_SCRIPT const nsAString* GetTrustedTypesCompliantString(
    const TrustedHTMLOrString& aInput, const nsAString& aSink,
    const nsAString& aSinkGroup, nsIGlobalObject& aGlobalObject,
    Maybe<nsAutoString>& aResultHolder, ErrorResult& aError);
MOZ_CAN_RUN_SCRIPT const nsAString* GetTrustedTypesCompliantString(
    const TrustedScriptOrString& aInput, const nsAString& aSink,
    const nsAString& aSinkGroup, const nsINode& aNode,
    Maybe<nsAutoString>& aResultHolder, ErrorResult& aError);
MOZ_CAN_RUN_SCRIPT const nsAString* GetTrustedTypesCompliantString(
    const TrustedScriptOrNullIsEmptyString& aInput, const nsAString& aSink,
    const nsAString& aSinkGroup, const nsINode& aNode,
    Maybe<nsAutoString>& aResultHolder, ErrorResult& aError);
MOZ_CAN_RUN_SCRIPT const nsAString* GetTrustedTypesCompliantString(
    const FunctionOrTrustedScriptOrString& aInput, const nsAString& aSink,
    const nsAString& aSinkGroup, nsIGlobalObject& aGlobalObject,
    Maybe<nsAutoString>& aResultHolder, ErrorResult& aError);
MOZ_CAN_RUN_SCRIPT const nsAString* GetTrustedTypesCompliantString(
    const TrustedScriptURLOrString& aInput, const nsAString& aSink,
    const nsAString& aSinkGroup, const nsINode& aNode,
    Maybe<nsAutoString>& aResultHolder, ErrorResult& aError);
MOZ_CAN_RUN_SCRIPT const nsAString* GetTrustedTypesCompliantString(
    const TrustedScriptURLOrUSVString& aInput, const nsAString& aSink,
    const nsAString& aSinkGroup, nsIGlobalObject& aGlobalObject,
    Maybe<nsAutoString>& aResultHolder, ErrorResult& aError);
MOZ_CAN_RUN_SCRIPT const nsAString*
GetTrustedTypesCompliantStringForTrustedHTML(const nsAString& aInput,
                                             const nsAString& aSink,
                                             const nsAString& aSinkGroup,
                                             const nsINode& aNode,
                                             Maybe<nsAutoString>& aResultHolder,
                                             ErrorResult& aError);

// https://w3c.github.io/trusted-types/dist/spec/#abstract-opdef-process-value-with-a-default-policy
template <typename ExpectedType>
MOZ_CAN_RUN_SCRIPT void ProcessValueWithADefaultPolicy(
    nsIGlobalObject& aGlobalObject, const nsAString& aInput,
    const nsAString& aSink, ExpectedType** aResult, ErrorResult& aError);

// https://w3c.github.io/trusted-types/dist/spec/#get-trusted-type-data-for-attribute
bool GetTrustedTypeDataForAttribute(const nsAtom* aElementName,
                                    int32_t aElementNamespaceID,
                                    nsAtom* aAttributeName,
                                    int32_t aAttributeNamespaceID,
                                    TrustedType& aTrustedType,
                                    nsAString& aSink);

// https://w3c.github.io/trusted-types/dist/spec/#abstract-opdef-get-trusted-types-compliant-attribute-value
MOZ_CAN_RUN_SCRIPT const nsAString* GetTrustedTypesCompliantAttributeValue(
    const nsINode& aElement, nsAtom* aAttributeName,
    int32_t aAttributeNamespaceID,
    const TrustedHTMLOrTrustedScriptOrTrustedScriptURLOrString& aNewValue,
    Maybe<nsAutoString>& aResultHolder, ErrorResult& aError);

// https://html.spec.whatwg.org/multipage/webappapis.html#hostgetcodeforeval(argument)
bool HostGetCodeForEval(JSContext* aCx, JS::Handle<JSObject*> aCode,
                        JS::MutableHandle<JSString*> aOutCode);

}  // namespace TrustedTypeUtils

}  // namespace dom

}  // namespace mozilla

#define DECL_TRUSTED_TYPE_CLASS(_class)                                     \
  class _class {                                                            \
   public:                                                                  \
    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(_class)              \
    NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(_class)                           \
                                                                            \
    /* Required for Web IDL binding. */                                     \
    bool WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto,      \
                    JS::MutableHandle<JSObject*> aObject);                  \
                                                                            \
    void Stringify(DOMString& aResult) const {                              \
      aResult.SetKnownLiveString(mData);                                    \
    }                                                                       \
                                                                            \
    void ToJSON(DOMString& aResult) const {                                 \
      aResult.SetKnownLiveString(mData);                                    \
    }                                                                       \
                                                                            \
    /* This is always unforged data, because it's only instantiated         \
       from the befriended `TrustedType*` classes and other trusted         \
       functions . */                                                       \
    const nsString mData;                                                   \
                                                                            \
   private:                                                                 \
    template <typename T, typename... Args>                                 \
    friend RefPtr<T> mozilla::MakeRefPtr(Args&&... aArgs);                  \
    friend mozilla::dom::TrustedTypePolicy;                                 \
    friend mozilla::dom::TrustedTypePolicyFactory;                          \
    template <typename ExpectedType>                                        \
    friend void                                                             \
    mozilla::dom::TrustedTypeUtils::ProcessValueWithADefaultPolicy(         \
        nsIGlobalObject& aGlobalObject, const nsAString&, const nsAString&, \
        ExpectedType**, ErrorResult&);                                      \
                                                                            \
    explicit _class(const nsAString& aData) : mData{aData} {                \
      MOZ_ASSERT(!aData.IsVoid());                                          \
    }                                                                       \
                                                                            \
    /* Required because the class is cycle-colleceted. */                   \
    ~_class() = default;                                                    \
  };

#define IMPL_TRUSTED_TYPE_CLASS(_class)                                      \
  NS_IMPL_CYCLE_COLLECTION(_class)                                           \
                                                                             \
  bool _class::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto, \
                          JS::MutableHandle<JSObject*> aObject) {            \
    return _class##_Binding::Wrap(aCx, this, aGivenProto, aObject);          \
  }

#endif  // DOM_SECURITY_TRUSTED_TYPES_TRUSTEDTYPEUTILS_H_
