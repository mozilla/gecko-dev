/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SECURITY_TRUSTED_TYPES_TRUSTEDTYPEUTILS_H_
#define DOM_SECURITY_TRUSTED_TYPES_TRUSTEDTYPEUTILS_H_

#include "mozilla/Assertions.h"
#include "mozilla/dom/DOMString.h"
#include "mozilla/dom/TrustedTypesBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"
#include "nsString.h"

#define DECL_TRUSTED_TYPE_CLASS(_class)                                \
  class _class {                                                       \
   public:                                                             \
    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(_class)         \
    NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(_class)                      \
                                                                       \
    /* Required for Web IDL binding. */                                \
    bool WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto, \
                    JS::MutableHandle<JSObject*> aObject);             \
                                                                       \
    void Stringify(DOMString& aResult) const {                         \
      aResult.SetKnownLiveString(mData);                               \
    }                                                                  \
                                                                       \
    void ToJSON(DOMString& aResult) const {                            \
      aResult.SetKnownLiveString(mData);                               \
    }                                                                  \
                                                                       \
    /* This is always unforged data, because it's only instantiated    \
       from the befriended `TrustedType*` classes. */                  \
    const nsString mData;                                              \
                                                                       \
   private:                                                            \
    friend mozilla::dom::TrustedTypePolicy;                            \
    friend mozilla::dom::TrustedTypePolicyFactory;                     \
                                                                       \
    explicit _class(const nsAString& aData) : mData{aData} {           \
      MOZ_ASSERT(!aData.IsVoid());                                     \
    }                                                                  \
                                                                       \
    /* Required because the class is cycle-colleceted. */              \
    ~_class() = default;                                               \
  };

#define IMPL_TRUSTED_TYPE_CLASS(_class)                                      \
  NS_IMPL_CYCLE_COLLECTION(_class)                                           \
                                                                             \
  bool _class::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto, \
                          JS::MutableHandle<JSObject*> aObject) {            \
    return _class##_Binding::Wrap(aCx, this, aGivenProto, aObject);          \
  }

#endif  // DOM_SECURITY_TRUSTED_TYPES_TRUSTEDTYPEUTILS_H_
