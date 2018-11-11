/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ACCURATE_SEEK_TASK_H
#define ACCURATE_SEEK_TASK_H

#include "SeekTask.h"
#include "MediaDecoderReader.h"
#include "SeekJob.h"

namespace mozilla {

class AccurateSeekTask final : public SeekTask {
public:
  AccurateSeekTask(const void* aDecoderID,
                   AbstractThread* aThread,
                   MediaDecoderReaderWrapper* aReader,
                   const SeekTarget& aTarget,
                   const MediaInfo& aInfo,
                   const media::TimeUnit& aEnd,
                   int64_t aCurrentMediaTime);

  void Discard() override;

  RefPtr<SeekTaskPromise> Seek(const media::TimeUnit& aDuration) override;

  bool NeedToResetMDSM() const override;

private:
  ~AccurateSeekTask();

  void RequestVideoData();

  void RequestAudioData();

  nsresult DropAudioUpToSeekTarget(MediaData* aSample);

  nsresult DropVideoUpToSeekTarget(MediaData* aSample);

  void MaybeFinishSeek();

  void OnSeekResolved(media::TimeUnit);

  void OnSeekRejected(nsresult aResult);

  void OnAudioDecoded(MediaData* aAudioSample);

  void OnVideoDecoded(MediaData* aVideoSample);

  void OnNotDecoded(MediaData::Type, const MediaResult&);

  void SetCallbacks();

  void CancelCallbacks();

  void AdjustFastSeekIfNeeded(MediaData* aSample);

  /*
   * Internal state.
   */
  const media::TimeUnit mCurrentTimeBeforeSeek;
  const uint32_t mAudioRate;  // Audio sample rate.
  bool mDoneAudioSeeking;
  bool mDoneVideoSeeking;

  // This temporarily stores the first frame we decode after we seek.
  // This is so that if we hit end of stream while we're decoding to reach
  // the seek target, we will still have a frame that we can display as the
  // last frame in the media.
  RefPtr<MediaData> mFirstVideoFrameAfterSeek;

  /*
   * Track the current seek promise made by the reader.
   */
  MozPromiseRequestHolder<MediaDecoderReader::SeekPromise> mSeekRequest;

  MediaEventListener mAudioCallback;
  MediaEventListener mVideoCallback;
  MediaEventListener mAudioWaitCallback;
  MediaEventListener mVideoWaitCallback;
};

} // namespace mozilla

#endif /* ACCURATE_SEEK_TASK_H */
