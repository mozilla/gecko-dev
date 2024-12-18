/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MLS_h
#define mozilla_dom_MLS_h

// #include "mozilla/dom/TypedArray.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/MLSBinding.h"
#include "mozilla/dom/MLSTransactionChild.h"
#include "nsIGlobalObject.h"

class nsIGlobalObject;

namespace mozilla::dom {

class MLSGroupView;

class MLS final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(MLS)

  static already_AddRefed<MLS> Constructor(GlobalObject& aGlobal,
                                           ErrorResult& aRv);

  explicit MLS(nsIGlobalObject* aGlobalObject, MLSTransactionChild* aActor);

  nsIGlobalObject* GetParentObject() const { return mGlobalObject; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<mozilla::dom::Promise> DeleteState(ErrorResult& aRv);

  already_AddRefed<mozilla::dom::Promise> GenerateIdentity(ErrorResult& aRv);

  already_AddRefed<mozilla::dom::Promise> GenerateCredential(
      const MLSBytesOrUint8ArrayOrUTF8String& aJsCredContent, ErrorResult& aRv);

  already_AddRefed<mozilla::dom::Promise> GenerateKeyPackage(
      const MLSBytesOrUint8Array& aJsClientIdentifier,
      const MLSBytesOrUint8Array& aJsCredential, ErrorResult& aRv);

  already_AddRefed<mozilla::dom::Promise> GroupCreate(
      const MLSBytesOrUint8Array& aJsClientIdentifier,
      const MLSBytesOrUint8Array& aJsCredential, ErrorResult& aRv);

  already_AddRefed<mozilla::dom::Promise> GroupGet(
      const MLSBytesOrUint8Array& aJsGroupIdentifier,
      const MLSBytesOrUint8Array& aJsClientIdentifier, ErrorResult& aRv);

  already_AddRefed<mozilla::dom::Promise> GroupJoin(
      const MLSBytesOrUint8Array& aJsClientIdentifier,
      const MLSBytesOrUint8Array& aJsWelcome, ErrorResult& aRv);

  already_AddRefed<mozilla::dom::Promise> GetGroupIdFromMessage(
      const MLSBytesOrUint8Array& aJsMessage, ErrorResult& aRv);

 private:
  friend class MLSGroupView;

  virtual ~MLS();
  nsCOMPtr<nsIGlobalObject> mGlobalObject;
  RefPtr<MLSTransactionChild> mTransactionChild;
};

}  // namespace mozilla::dom

#endif
