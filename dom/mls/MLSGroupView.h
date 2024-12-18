/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MLSGroup_h
#define mozilla_dom_MLSGroup_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/MLS.h"

class nsIGlobalObject;

namespace mozilla::dom {

class MLSGroupView final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MLSGroupView)

  explicit MLSGroupView(MLS* aMLS, nsTArray<uint8_t>&& aGroupId,
                        nsTArray<uint8_t>&& aClientId);

  nsISupports* GetParentObject() const { return mMLS; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  void GetGroupId(JSContext* aCx, JS::MutableHandle<JSObject*> aGroupId,
                  ErrorResult& aRv);
  void GetClientId(JSContext* aCx, JS::MutableHandle<JSObject*> aClientId,
                   ErrorResult& aRv);

  already_AddRefed<mozilla::dom::Promise> DeleteState(ErrorResult& aRv);

  already_AddRefed<mozilla::dom::Promise> Add(
      const MLSBytesOrUint8Array& aJsKeyPackage, ErrorResult& aRv);

  already_AddRefed<mozilla::dom::Promise> ProposeAdd(
      const MLSBytesOrUint8Array& aJsKeyPackage, ErrorResult& aRv);

  already_AddRefed<mozilla::dom::Promise> Remove(
      const MLSBytesOrUint8Array& aJsRemClientIdentifier, ErrorResult& aRv);

  already_AddRefed<mozilla::dom::Promise> ProposeRemove(
      const MLSBytesOrUint8Array& aJsRemClientIdentifier, ErrorResult& aRv);

  already_AddRefed<mozilla::dom::Promise> Close(ErrorResult& aRv);

  already_AddRefed<mozilla::dom::Promise> Details(ErrorResult& aRv);

  already_AddRefed<mozilla::dom::Promise> Send(
      const MLSBytesOrUint8ArrayOrUTF8String& aJsMessage, ErrorResult& aRv);

  already_AddRefed<mozilla::dom::Promise> Receive(
      const MLSBytesOrUint8Array& aJsMessage, ErrorResult& aRv);

  already_AddRefed<mozilla::dom::Promise> ApplyPendingCommit(ErrorResult& aRv);

  already_AddRefed<mozilla::dom::Promise> ExportSecret(
      const MLSBytesOrUint8ArrayOrUTF8String& aJsLabel,
      const MLSBytesOrUint8Array& aJsContext, const uint64_t aLen,
      ErrorResult& aRv);

 private:
  virtual ~MLSGroupView() { mozilla::DropJSObjects(this); }

  RefPtr<MLS> mMLS;
  nsTArray<uint8_t> mGroupId;
  nsTArray<uint8_t> mClientId;
  JS::Heap<JSObject*> mJsGroupId;
  JS::Heap<JSObject*> mJsClientId;
};

}  // namespace mozilla::dom

#endif
