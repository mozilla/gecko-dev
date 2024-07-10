/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TestReflectedHTMLAttribute.h"
#include "mozilla/dom/TestFunctionsBinding.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TestReflectedHTMLAttribute,
                                      mCachedElements, mNewElements)

/* static */
already_AddRefed<TestReflectedHTMLAttribute>
TestReflectedHTMLAttribute::Constructor(GlobalObject& aGlobal) {
  return MakeAndAddRef<TestReflectedHTMLAttribute>();
}

template <typename ArrayLike>
static void AssignElements(const ArrayLike& aFrom,
                           Nullable<nsTArray<RefPtr<Element>>>& aTo) {
  if (aTo.IsNull()) {
    aTo.SetValue();
  } else {
    aTo.Value().Clear();
  }
  aTo.Value().AppendElements(aFrom);
}

void TestReflectedHTMLAttribute::GetReflectedHTMLAttribute(
    bool* aUseCachedValue, Nullable<nsTArray<RefPtr<Element>>>& aResult) {
  if (aUseCachedValue) {
    if (mCachedElements == mNewElements) {
      *aUseCachedValue = true;
      return;
    }

    *aUseCachedValue = false;
  }

  if (mNewElements.IsNull()) {
    mCachedElements.SetNull();
    aResult.SetNull();
  } else {
    AssignElements(mNewElements.Value(), mCachedElements);
    aResult.SetValue(mCachedElements.Value().Clone());
  }
}

void TestReflectedHTMLAttribute::SetReflectedHTMLAttribute(
    const Nullable<Sequence<OwningNonNull<Element>>>& aValue) {
  // We're just testing getters, so do nothing. But this would either clear or
  // set the "explicitly set attr-elements".
}

void TestReflectedHTMLAttribute::SetReflectedHTMLAttributeValue(
    const Sequence<OwningNonNull<Element>>& aElements) {
  AssignElements(aElements, mNewElements);
}

JSObject* TestReflectedHTMLAttribute::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return TestReflectedHTMLAttribute_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
