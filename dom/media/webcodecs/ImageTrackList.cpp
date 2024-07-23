/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ImageTrackList.h"
#include "MediaResult.h"
#include "mozilla/dom/ImageDecoder.h"
#include "mozilla/dom/ImageTrack.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/image/ImageUtils.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ImageTrackList, mParent, mDecoder,
                                      mReadyPromise, mTracks)
NS_IMPL_CYCLE_COLLECTING_ADDREF(ImageTrackList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ImageTrackList)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ImageTrackList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

ImageTrackList::ImageTrackList(nsIGlobalObject* aParent, ImageDecoder* aDecoder)
    : mParent(aParent), mDecoder(aDecoder) {}

ImageTrackList::~ImageTrackList() = default;

JSObject* ImageTrackList::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  AssertIsOnOwningThread();
  return ImageTrackList_Binding::Wrap(aCx, this, aGivenProto);
}

void ImageTrackList::Initialize(ErrorResult& aRv) {
  mReadyPromise = Promise::Create(mParent, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}

void ImageTrackList::Destroy() {
  if (!mIsReady && mReadyPromise && mReadyPromise->PromiseObj()) {
    mReadyPromise->MaybeRejectWithAbortError("ImageTrackList destroyed");
    mIsReady = true;
  }

  for (auto& track : mTracks) {
    track->Destroy();
  }
  mTracks.Clear();

  mDecoder = nullptr;
  mSelectedIndex = -1;
}

void ImageTrackList::MaybeRejectReady(const MediaResult& aResult) {
  if (mIsReady || !mReadyPromise || !mReadyPromise->PromiseObj()) {
    return;
  }
  aResult.RejectTo(mReadyPromise);
  mIsReady = true;
}

void ImageTrackList::OnMetadataSuccess(
    const image::DecodeMetadataResult& aMetadata) {
  // 10.2.5. Establish Tracks
  //
  // Note that our implementation only supports one track, so many of these
  // steps are simplified.

  // 4. Let newTrackList be a new list.
  MOZ_ASSERT(mTracks.IsEmpty());

  // 5. For each image track found in [[encoded data]]:
  // 5.1. Let newTrack be a new ImageTrack, initialized as follows:
  // 5.1.1. Assign this to [[ImageDecoder]].
  // 5.1.2. Assign tracks to [[ImageTrackList]].
  // 5.1.3. If image track is found to be animated, assign true to newTrack's
  //        [[animated]] internal slot. Otherwise, assign false.
  // 5.1.4. If image track is found to describe a frame count, assign that
  //        count to newTrack's [[frame count]] internal slot. Otherwise, assign
  //        0.
  // 5.1.5. If image track is found to describe a repetition count, assign that
  //        count to [[repetition count]] internal slot. Otherwise, assign 0.
  // 5.1.6. Assign false to newTrack’s [[selected]] internal slot.
  // 5.2. Append newTrack to newTrackList.
  // 6. Let selectedTrackIndex be the result of running the Get Default Selected
  //    Track Index algorithm with newTrackList.
  // 7. Let selectedTrack be the track at position selectedTrackIndex within
  //    newTrackList.
  // 8. Assign true to selectedTrack’s [[selected]] internal slot.
  // 9. Assign selectedTrackIndex to [[internal selected track index]].
  const float repetitions = aMetadata.mRepetitions < 0
                                ? std::numeric_limits<float>::infinity()
                                : static_cast<float>(aMetadata.mRepetitions);
  auto track = MakeRefPtr<ImageTrack>(
      this, /* aIndex */ 0, /* aSelected */ true, aMetadata.mAnimated,
      aMetadata.mFrameCount, aMetadata.mFrameCountComplete, repetitions);

  // 11. Queue a task to perform the following steps:
  //
  // Note that we were already dispatched by the image decoder.

  // 11.1. Assign newTrackList to the tracks [[track list]] internal slot.
  mTracks.AppendElement(std::move(track));

  // 11.2. Assign selectedTrackIndex to tracks [[selected index]].
  mSelectedIndex = 0;

  // 11.3. Resolve [[ready promise]].
  MOZ_ASSERT(!mIsReady);
  mReadyPromise->MaybeResolveWithUndefined();
  mIsReady = true;
}

void ImageTrackList::OnFrameCountSuccess(
    const image::DecodeFrameCountResult& aResult) {
  if (mTracks.IsEmpty()) {
    return;
  }

  // 10.2.5. Update Tracks
  //
  // Note that we were already dispatched from the decoding threads.

  // 3. Let trackList be a copy of tracks' [[track list]].
  // 4. For each track in trackList:
  // 4.1. Let trackIndex be the position of track in trackList.
  // 4.2. Let latestFrameCount be the frame count as indicated by
  //      [[encoded data]] for the track corresponding to track.
  // 4.3. Assert that latestFrameCount is greater than or equal to
  //      track.frameCount.
  // 4.4. If latestFrameCount is greater than track.frameCount:
  // 4.4.1. Let change be a track update struct whose track index is trackIndex
  //        and frame count is latestFrameCount.
  // 4.4.2. Append change to tracksChanges.
  // 5. If tracksChanges is empty, abort these steps.
  // 6. Queue a task to perform the following steps:
  // 6.1. For each update in trackChanges:
  // 6.1.1. Let updateTrack be the ImageTrack at position update.trackIndex
  //        within tracks' [[track list]].
  // 6.1.2. Assign update.frameCount to updateTrack’s [[frame count]].
  mTracks.LastElement()->OnFrameCountSuccess(aResult);
}

void ImageTrackList::SetSelectedIndex(int32_t aIndex, bool aSelected) {
  MOZ_ASSERT(aIndex >= 0);
  MOZ_ASSERT(uint32_t(aIndex) < mTracks.Length());

  // 10.7.2. Attributes - selected, of type boolean

  // 1. If [[ImageDecoder]]'s [[closed]] slot is true, abort these steps.
  if (!mDecoder) {
    return;
  }

  // 2. Let newValue be the given value.
  // 3. If newValue equals [[selected]], abort these steps.
  // 4. Assign newValue to [[selected]].
  // 5. Let parentTrackList be [[ImageTrackList]]
  // 6. Let oldSelectedIndex be the value of parentTrackList [[selected index]].
  // 7. If oldSelectedIndex is not -1:
  // 7.1. Let oldSelectedTrack be the ImageTrack in parentTrackList
  //      [[track list]] at the position of oldSelectedIndex.
  // 7.2. Assign false to oldSelectedTrack [[selected]]
  // 8. If newValue is true, let selectedIndex be the index of this ImageTrack
  //    within parentTrackList's [[track list]]. Otherwise, let selectedIndex be
  //    -1.
  // 9. Assign selectedIndex to parentTrackList [[selected index]].
  if (aSelected) {
    if (mSelectedIndex == -1) {
      MOZ_ASSERT(!mTracks[aIndex]->Selected());
      mTracks[aIndex]->MarkSelected();
      mSelectedIndex = aIndex;
    } else if (mSelectedIndex != aIndex) {
      MOZ_ASSERT(mTracks[mSelectedIndex]->Selected());
      MOZ_ASSERT(!mTracks[aIndex]->Selected());
      mTracks[mSelectedIndex]->ClearSelected();
      mTracks[aIndex]->MarkSelected();
      mSelectedIndex = aIndex;
    } else {
      MOZ_ASSERT(mTracks[mSelectedIndex]->Selected());
      return;
    }
  } else if (mSelectedIndex == aIndex) {
    mTracks[mSelectedIndex]->ClearSelected();
    mSelectedIndex = -1;
  } else {
    MOZ_ASSERT(!mTracks[aIndex]->Selected());
    return;
  }

  // 10. Run the Reset ImageDecoder algorithm on [[ImageDecoder]].
  mDecoder->Reset();

  // 11. Queue a control message to [[ImageDecoder]]'s control message queue to
  //     update the internal selected track index with selectedIndex.
  mDecoder->QueueSelectTrackMessage(mSelectedIndex);

  // 12. Process the control message queue belonging to [[ImageDecoder]].
  mDecoder->ProcessControlMessageQueue();
}

}  // namespace mozilla::dom
