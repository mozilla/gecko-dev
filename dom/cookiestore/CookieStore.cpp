/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CookieStore.h"

#include "mozilla/dom/Promise.h"
#include "nsIGlobalObject.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(CookieStore)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(CookieStore,
                                                DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(CookieStore,
                                                  DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CookieStore)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(CookieStore, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(CookieStore, DOMEventTargetHelper)

// static
already_AddRefed<CookieStore> CookieStore::Create(nsIGlobalObject* aGlobal) {
  return do_AddRef(new CookieStore(aGlobal));
}

CookieStore::CookieStore(nsIGlobalObject* aGlobal)
    : DOMEventTargetHelper(aGlobal) {}

CookieStore::~CookieStore() = default;

JSObject* CookieStore::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return CookieStore_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<Promise> CookieStore::Get(const nsAString& aName,
                                           ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}

already_AddRefed<Promise> CookieStore::Get(
    const CookieStoreGetOptions& aOptions, ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}

already_AddRefed<Promise> CookieStore::GetAll(const nsAString& aName,
                                              ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}

already_AddRefed<Promise> CookieStore::GetAll(
    const CookieStoreGetOptions& aOptions, ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}

already_AddRefed<Promise> CookieStore::Set(const nsAString& aName,
                                           const nsAString& aValue,
                                           ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}

already_AddRefed<Promise> CookieStore::Set(const CookieInit& aOptions,
                                           ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}

already_AddRefed<Promise> CookieStore::Delete(const nsAString& aName,
                                              ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}

already_AddRefed<Promise> CookieStore::Delete(
    const CookieStoreDeleteOptions& aOptions, ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}

}  // namespace mozilla::dom
