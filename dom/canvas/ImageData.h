/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ImageData_h
#define mozilla_dom_ImageData_h

#include <cstdint>
#include <utility>
#include "js/RootingAPI.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/TypedArray.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

class JSObject;
class nsIGlobalObject;
struct JSContext;
struct JSStructuredCloneReader;
struct JSStructuredCloneWriter;

namespace mozilla {
class ErrorResult;

namespace dom {

class GlobalObject;
template <typename T>
class Optional;

// ImageData extends nsWrapperCache only to support nursery allocated wrapper.
class ImageData final : public nsISupports, public nsWrapperCache {
 public:
  ImageData(nsISupports* aOwner, uint32_t aWidth, uint32_t aHeight,
            JS::Handle<JSObject*> aData)
      : mOwner(aOwner), mWidth(aWidth), mHeight(aHeight), mData(aData) {
    HoldData();
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS(ImageData)

  nsISupports* GetParentObject() const { return mOwner; }

  static already_AddRefed<ImageData> Constructor(const GlobalObject& aGlobal,
                                                 const uint32_t aWidth,
                                                 const uint32_t aHeight,
                                                 ErrorResult& aRv);

  static already_AddRefed<ImageData> Constructor(
      const GlobalObject& aGlobal, const Uint8ClampedArray& aData,
      const uint32_t aWidth, const Optional<uint32_t>& aHeight,
      ErrorResult& aRv);

  uint32_t Width() const { return mWidth; }
  uint32_t Height() const { return mHeight; }
  void GetData(JSContext* cx, JS::MutableHandle<JSObject*> aData) const {
    aData.set(GetDataObject());
  }
  JSObject* GetDataObject() const { return mData; }

  JSObject* WrapObject(JSContext*, JS::Handle<JSObject*> aGivenProto) override;

  //[Serializable] implementation
  static already_AddRefed<ImageData> ReadStructuredClone(
      JSContext* aCx, nsIGlobalObject* aGlobal,
      JSStructuredCloneReader* aReader);
  bool WriteStructuredClone(JSContext* aCx,
                            JSStructuredCloneWriter* aWriter) const;

 private:
  void HoldData();
  void DropData();

  ImageData() = delete;
  ~ImageData() { DropData(); }

  nsCOMPtr<nsISupports> mOwner;

  const uint32_t mWidth;
  const uint32_t mHeight;

  JS::Heap<JSObject*> mData;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ImageData_h
