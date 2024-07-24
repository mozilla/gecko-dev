/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ImageTrackList_h
#define mozilla_dom_ImageTrackList_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/ImageDecoderBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {
class MediaResult;

namespace image {
struct DecodeFrameCountResult;
struct DecodeMetadataResult;
}  // namespace image

namespace dom {

class ImageTrack;
class Promise;

class ImageTrackList final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(ImageTrackList)

 public:
  ImageTrackList(nsIGlobalObject* aParent, ImageDecoder* aDecoder);

  void Initialize(ErrorResult& aRv);
  void MaybeRejectReady(const MediaResult& aResult);
  void Destroy();
  void OnMetadataSuccess(const image::DecodeMetadataResult& aMetadata);
  void OnFrameCountSuccess(const image::DecodeFrameCountResult& aResult);
  void SetSelectedIndex(int32_t aIndex, bool aSelected);

 protected:
  ~ImageTrackList();

 public:
  nsIGlobalObject* GetParentObject() const { return mParent; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  Promise* Ready() const { return mReadyPromise; }

  bool IsReady() const { return mIsReady; }

  uint32_t Length() const { return mTracks.Length(); }

  int32_t SelectedIndex() const { return mSelectedIndex; }

  ImageTrack* GetSelectedTrack() const {
    if (mSelectedIndex < 0) {
      return nullptr;
    }
    return mTracks[mSelectedIndex];
  }

  ImageTrack* GetDefaultTrack() const {
    if (mTracks.IsEmpty()) {
      return nullptr;
    }
    return mTracks[0];
  }

  ImageTrack* IndexedGetter(uint32_t aIndex, bool& aFound) const {
    if (aIndex >= mTracks.Length()) {
      aFound = false;
      return nullptr;
    }

    MOZ_ASSERT(mTracks[aIndex]);
    aFound = true;
    return mTracks[aIndex];
  }

 private:
  // ImageTrackList can run on either main thread or worker thread.
  void AssertIsOnOwningThread() const {
    NS_ASSERT_OWNINGTHREAD(ImageTrackList);
  }

  nsCOMPtr<nsIGlobalObject> mParent;
  RefPtr<ImageDecoder> mDecoder;
  AutoTArray<RefPtr<ImageTrack>, 1> mTracks;
  RefPtr<Promise> mReadyPromise;
  int32_t mSelectedIndex = -1;
  bool mIsReady = false;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ImageTrackList_h
