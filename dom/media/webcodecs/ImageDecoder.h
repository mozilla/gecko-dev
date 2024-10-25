/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ImageDecoder_h
#define mozilla_dom_ImageDecoder_h

#include "FrameTimeout.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/NotNull.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/ImageDecoderBinding.h"
#include "mozilla/dom/WebCodecsUtils.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/media/MediaUtils.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {
class MediaResult;

namespace image {
class AnonymousDecoder;
class SourceBuffer;
enum class DecoderType;
enum class SurfaceFlags : uint8_t;
struct DecodeFramesResult;
struct DecodeFrameCountResult;
struct DecodeMetadataResult;
}  // namespace image

namespace dom {
class Promise;
struct ImageDecoderReadRequest;

class ImageDecoder final : public nsISupports,
                           public nsWrapperCache,
                           public media::ShutdownConsumer {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(ImageDecoder)

 public:
  ImageDecoder(nsCOMPtr<nsIGlobalObject>&& aParent, const nsAString& aType);

 public:
  nsIGlobalObject* GetParentObject() const { return mParent; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<ImageDecoder> Constructor(
      const GlobalObject& aGlobal, const ImageDecoderInit& aInit,
      ErrorResult& aRv);

  static already_AddRefed<Promise> IsTypeSupported(const GlobalObject& aGlobal,
                                                   const nsAString& aType,
                                                   ErrorResult& aRv);

  void GetType(nsAString& aType) const;

  bool Complete() const { return mComplete; }

  Promise* Completed() const { return mCompletePromise; }

  ImageTrackList* Tracks() const { return mTracks; }

  already_AddRefed<Promise> Decode(const ImageDecodeOptions& aOptions,
                                   ErrorResult& aRv);

  void Reset();

  void Close();

  void OnShutdown() override;

  void OnSourceBufferComplete(const MediaResult& aResult);

  void QueueSelectTrackMessage(uint32_t aSelectedIndex);
  void ProcessControlMessageQueue();

 private:
  ~ImageDecoder();

  class ControlMessage;
  class ConfigureMessage;
  class DecodeMetadataMessage;
  class DecodeFrameMessage;
  class SelectTrackMessage;

  std::queue<UniquePtr<ControlMessage>> mControlMessageQueue;
  bool mMessageQueueBlocked = false;
  bool mTracksEstablished = false;

  struct OutstandingDecode {
    RefPtr<Promise> mPromise;
    uint32_t mFrameIndex = 0;
    bool mCompleteFramesOnly = true;
  };

  // VideoFrame can run on either main thread or worker thread.
  void AssertIsOnOwningThread() const { NS_ASSERT_OWNINGTHREAD(ImageDecoder); }

  void Initialize(const GlobalObject& aGLobal, const ImageDecoderInit& aInit,
                  ErrorResult& aRv);
  void Destroy();
  void Reset(const MediaResult& aResult);
  void Close(const MediaResult& aResult);

  void QueueConfigureMessage(const Maybe<gfx::IntSize>& aOutputSize,
                             ColorSpaceConversion aColorSpaceConversion);
  void QueueDecodeMetadataMessage();
  void QueueDecodeFrameMessage();

  void ResumeControlMessageQueue();
  MessageProcessedResult ProcessConfigureMessage(ConfigureMessage* aMsg);
  MessageProcessedResult ProcessDecodeMetadataMessage(
      DecodeMetadataMessage* aMsg);
  MessageProcessedResult ProcessDecodeFrameMessage(DecodeFrameMessage* aMsg);
  MessageProcessedResult ProcessSelectTrackMessage(SelectTrackMessage* aMsg);

  void CheckOutstandingDecodes();

  void OnCompleteSuccess();
  void OnCompleteFailed(const MediaResult& aResult);

  void OnMetadataSuccess(const image::DecodeMetadataResult& aMetadata);
  void OnMetadataFailed(const nsresult& aErr);

  void RequestFrameCount(uint32_t aKnownFrameCount);
  void OnFrameCountSuccess(const image::DecodeFrameCountResult& aResult);
  void OnFrameCountFailed(const nsresult& aErr);

  void RequestDecodeFrames(uint32_t aFramesToDecode);
  void OnDecodeFramesSuccess(const image::DecodeFramesResult& aResult);
  void OnDecodeFramesFailed(const nsresult& aErr);

  nsCOMPtr<nsIGlobalObject> mParent;
  RefPtr<media::ShutdownWatcher> mShutdownWatcher;
  RefPtr<ImageTrackList> mTracks;
  RefPtr<ImageDecoderReadRequest> mReadRequest;
  RefPtr<Promise> mCompletePromise;
  RefPtr<image::SourceBuffer> mSourceBuffer;
  RefPtr<image::AnonymousDecoder> mDecoder;
  AutoTArray<OutstandingDecode, 1> mOutstandingDecodes;
  nsAutoString mType;
  image::FrameTimeout mFramesTimestamp;
  bool mComplete = false;
  bool mHasFrameCount = false;
  bool mHasFramePending = false;
  bool mTypeNotSupported = false;
  bool mClosed = false;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ImageDecoder_h
