/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TestReflectedHTMLAttribute_h
#define mozilla_dom_TestReflectedHTMLAttribute_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Nullable.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {

class TestReflectedHTMLAttribute final : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(TestReflectedHTMLAttribute)
  NS_DECL_CYCLE_COLLECTION_NATIVE_WRAPPERCACHE_CLASS(TestReflectedHTMLAttribute)

  static already_AddRefed<TestReflectedHTMLAttribute> Constructor(
      GlobalObject& aGlobal);

  void GetReflectedHTMLAttribute(bool* aUseCachedValue,
                                 Nullable<nsTArray<RefPtr<Element>>>& aResult);
  void SetReflectedHTMLAttribute(
      const Nullable<Sequence<OwningNonNull<Element>>>& aValue);
  void SetReflectedHTMLAttributeValue(
      const Sequence<OwningNonNull<Element>>& aElements);

  nsISupports* GetParentObject() const { return nullptr; }
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

 private:
  ~TestReflectedHTMLAttribute() = default;

  Nullable<nsTArray<RefPtr<Element>>> mCachedElements;
  Nullable<nsTArray<RefPtr<Element>>> mNewElements;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_TestReflectedHTMLAttribute_h
