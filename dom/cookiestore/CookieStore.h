/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CookieStore_h
#define mozilla_dom_CookieStore_h

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/CookieStoreBinding.h"

class nsIGlobalObject;

namespace mozilla::dom {

class Promise;

class CookieStore final : public DOMEventTargetHelper {
  friend class CookieStoreChild;

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(CookieStore, DOMEventTargetHelper)

  static already_AddRefed<CookieStore> Create(nsIGlobalObject* aGlobalObject);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<Promise> Get(const nsAString& aName, ErrorResult& aRv);

  already_AddRefed<Promise> Get(const CookieStoreGetOptions& aOptions,
                                ErrorResult& aRv);

  already_AddRefed<Promise> GetAll(const nsAString& aName, ErrorResult& aRv);

  already_AddRefed<Promise> GetAll(const CookieStoreGetOptions& aOptions,
                                   ErrorResult& aRv);

  already_AddRefed<Promise> Set(const nsAString& aName, const nsAString& aValue,
                                ErrorResult& aRv);

  already_AddRefed<Promise> Set(const CookieInit& aOptions, ErrorResult& aRv);

  already_AddRefed<Promise> Delete(const nsAString& aName, ErrorResult& aRv);

  already_AddRefed<Promise> Delete(const CookieStoreDeleteOptions& aOptions,
                                   ErrorResult& aRv);

  IMPL_EVENT_HANDLER(change);

 private:
  explicit CookieStore(nsIGlobalObject* aGlobal);
  ~CookieStore();
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_CookieStore_h */
