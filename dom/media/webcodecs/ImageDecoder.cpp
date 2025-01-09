/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ImageDecoder.h"
#include <algorithm>
#include <cstdint>
#include "ImageContainer.h"
#include "ImageDecoderReadRequest.h"
#include "MediaResult.h"
#include "mozilla/dom/ImageTrack.h"
#include "mozilla/dom/ImageTrackList.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/VideoFrame.h"
#include "mozilla/dom/VideoFrameBinding.h"
#include "mozilla/dom/WebCodecsUtils.h"
#include "mozilla/image/ImageUtils.h"
#include "mozilla/image/SourceBuffer.h"
#include "mozilla/Logging.h"
#include "mozilla/StaticPrefs_dom.h"
#include "nsComponentManagerUtils.h"
#include "nsTHashSet.h"

extern mozilla::LazyLogModule gWebCodecsLog;

namespace mozilla::dom {

class ImageDecoder::ControlMessage {
 public:
  ControlMessage() = default;
  virtual ~ControlMessage() = default;

  virtual ConfigureMessage* AsConfigureMessage() { return nullptr; }
  virtual DecodeMetadataMessage* AsDecodeMetadataMessage() { return nullptr; }
  virtual DecodeFrameMessage* AsDecodeFrameMessage() { return nullptr; }
  virtual SelectTrackMessage* AsSelectTrackMessage() { return nullptr; }
};

class ImageDecoder::ConfigureMessage final
    : public ImageDecoder::ControlMessage {
 public:
  explicit ConfigureMessage(const Maybe<gfx::IntSize>& aOutputSize,
                            ColorSpaceConversion aColorSpaceConversion)
      : mOutputSize(aOutputSize),
        mColorSpaceConversion(aColorSpaceConversion) {}

  ConfigureMessage* AsConfigureMessage() override { return this; }

  const Maybe<gfx::IntSize> mOutputSize;
  const ColorSpaceConversion mColorSpaceConversion;
};

class ImageDecoder::DecodeMetadataMessage final
    : public ImageDecoder::ControlMessage {
 public:
  DecodeMetadataMessage* AsDecodeMetadataMessage() override { return this; }
};

class ImageDecoder::DecodeFrameMessage final
    : public ImageDecoder::ControlMessage {
 public:
  DecodeFrameMessage* AsDecodeFrameMessage() override { return this; }
};

class ImageDecoder::SelectTrackMessage final
    : public ImageDecoder::ControlMessage {
 public:
  explicit SelectTrackMessage(uint32_t aSelectedTrack)
      : mSelectedTrack(aSelectedTrack) {}

  SelectTrackMessage* AsSelectTrackMessage() override { return this; }

  const uint32_t mSelectedTrack;
};

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(ImageDecoder)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ImageDecoder)
  tmp->Destroy();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTracks)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mReadRequest)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCompletePromise)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOutstandingDecodes)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ImageDecoder)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTracks)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mReadRequest)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCompletePromise)
  for (uint32_t i = 0; i < tmp->mOutstandingDecodes.Length(); ++i) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOutstandingDecodes[i].mPromise);
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ImageDecoder)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(ImageDecoder)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ImageDecoder)

ImageDecoder::ImageDecoder(nsCOMPtr<nsIGlobalObject>&& aParent,
                           const nsAString& aType)
    : mParent(std::move(aParent)),
      mType(aType),
      mFramesTimestamp(image::FrameTimeout::Zero()) {
  MOZ_LOG(gWebCodecsLog, LogLevel::Debug,
          ("ImageDecoder %p ImageDecoder", this));
}

ImageDecoder::~ImageDecoder() {
  MOZ_LOG(gWebCodecsLog, LogLevel::Debug,
          ("ImageDecoder %p ~ImageDecoder", this));
  Destroy();
}

JSObject* ImageDecoder::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  AssertIsOnOwningThread();
  return ImageDecoder_Binding::Wrap(aCx, this, aGivenProto);
}

void ImageDecoder::Destroy() {
  MOZ_LOG(gWebCodecsLog, LogLevel::Debug, ("ImageDecoder %p Destroy", this));
  MOZ_ASSERT(mOutstandingDecodes.IsEmpty());

  if (mReadRequest) {
    mReadRequest->Destroy(/* aCancel */ false);
    mReadRequest = nullptr;
  }

  if (mDecoder) {
    mDecoder->Destroy();
  }

  if (mTracks) {
    mTracks->Destroy();
  }

  if (mShutdownWatcher) {
    mShutdownWatcher->Destroy();
    mShutdownWatcher = nullptr;
  }

  mSourceBuffer = nullptr;
  mDecoder = nullptr;
  mParent = nullptr;
}

void ImageDecoder::QueueConfigureMessage(
    const Maybe<gfx::IntSize>& aOutputSize,
    ColorSpaceConversion aColorSpaceConversion) {
  mControlMessageQueue.push(
      MakeUnique<ConfigureMessage>(aOutputSize, aColorSpaceConversion));
}

void ImageDecoder::QueueDecodeMetadataMessage() {
  mControlMessageQueue.push(MakeUnique<DecodeMetadataMessage>());
}

void ImageDecoder::QueueDecodeFrameMessage() {
  mControlMessageQueue.push(MakeUnique<DecodeFrameMessage>());
}

void ImageDecoder::QueueSelectTrackMessage(uint32_t aSelectedIndex) {
  mControlMessageQueue.push(MakeUnique<SelectTrackMessage>(aSelectedIndex));
}

void ImageDecoder::ResumeControlMessageQueue() {
  MOZ_ASSERT(mMessageQueueBlocked);
  mMessageQueueBlocked = false;
  ProcessControlMessageQueue();
}

void ImageDecoder::ProcessControlMessageQueue() {
  while (!mMessageQueueBlocked && !mControlMessageQueue.empty()) {
    auto& msg = mControlMessageQueue.front();
    auto result = MessageProcessedResult::Processed;
    if (auto* submsg = msg->AsConfigureMessage()) {
      result = ProcessConfigureMessage(submsg);
    } else if (auto* submsg = msg->AsDecodeMetadataMessage()) {
      result = ProcessDecodeMetadataMessage(submsg);
    } else if (auto* submsg = msg->AsDecodeFrameMessage()) {
      result = ProcessDecodeFrameMessage(submsg);
    } else if (auto* submsg = msg->AsSelectTrackMessage()) {
      result = ProcessSelectTrackMessage(submsg);
    } else {
      MOZ_ASSERT_UNREACHABLE("Unhandled control message type!");
    }

    if (result == MessageProcessedResult::NotProcessed) {
      break;
    }

    mControlMessageQueue.pop();
  }
}

MessageProcessedResult ImageDecoder::ProcessConfigureMessage(
    ConfigureMessage* aMsg) {
  // 10.2.2. Running a control message to configure the image decoder means
  // running these steps:

  // 1. Let supported be the result of running the Check Type Support algorithm
  // with init.type.
  //
  // 2. If supported is false, run the Close ImageDecoder algorithm with a
  // NotSupportedError DOMException and return "processed".
  //
  // Note that DecoderType::ICON is mostly an internal type that we use for
  // system icons and shouldn't be exposed for general use on the web. This is
  // not to be confused with DecoderType::ICO which is for .ico files.
  NS_ConvertUTF16toUTF8 mimeType(mType);
  image::DecoderType type = image::ImageUtils::GetDecoderType(mimeType);
  if (NS_WARN_IF(type == image::DecoderType::UNKNOWN) ||
      NS_WARN_IF(type == image::DecoderType::ICON)) {
    MOZ_LOG(gWebCodecsLog, LogLevel::Error,
            ("ImageDecoder %p Initialize -- unsupported mime type '%s'", this,
             mimeType.get()));
    Close(MediaResult(NS_ERROR_DOM_NOT_SUPPORTED_ERR,
                      "Unsupported mime type"_ns));
    return MessageProcessedResult::Processed;
  }

  image::SurfaceFlags surfaceFlags = image::DefaultSurfaceFlags();
  switch (aMsg->mColorSpaceConversion) {
    case ColorSpaceConversion::None:
      surfaceFlags |= image::SurfaceFlags::NO_COLORSPACE_CONVERSION;
      break;
    case ColorSpaceConversion::Default:
      break;
    default:
      MOZ_LOG(
          gWebCodecsLog, LogLevel::Error,
          ("ImageDecoder %p Initialize -- unsupported colorspace conversion",
           this));
      Close(MediaResult(NS_ERROR_DOM_NOT_SUPPORTED_ERR,
                        "Unsupported colorspace conversion"_ns));
      return MessageProcessedResult::Processed;
  }

  // 3. Otherwise, assign the [[codec implementation]] internal slot with an
  // implementation supporting init.type
  mDecoder = image::ImageUtils::CreateDecoder(mSourceBuffer, type,
                                              aMsg->mOutputSize, surfaceFlags);
  if (NS_WARN_IF(!mDecoder)) {
    MOZ_LOG(gWebCodecsLog, LogLevel::Error,
            ("ImageDecoder %p Initialize -- failed to create platform decoder",
             this));
    Close(MediaResult(NS_ERROR_DOM_NOT_SUPPORTED_ERR,
                      "Failed to create platform decoder"_ns));
    return MessageProcessedResult::Processed;
  }

  // 4. Assign true to [[message queue blocked]].
  mMessageQueueBlocked = true;

  NS_DispatchToCurrentThread(NS_NewCancelableRunnableFunction(
      "ImageDecoder::ProcessConfigureMessage", [self = RefPtr{this}] {
        // 5. Enqueue the following steps to the [[codec work queue]]:
        // 5.1. Configure [[codec implementation]] in accordance with the values
        //      given for colorSpaceConversion, desiredWidth, and desiredHeight.
        // 5.2. Assign false to [[message queue blocked]].
        // 5.3. Queue a task to Process the control message queue.
        self->ResumeControlMessageQueue();
      }));

  // 6. Return "processed".
  return MessageProcessedResult::Processed;
}

MessageProcessedResult ImageDecoder::ProcessDecodeMetadataMessage(
    DecodeMetadataMessage* aMsg) {
  // 10.2.2. Running a control message to decode track metadata means running
  // these steps:

  if (!mDecoder) {
    return MessageProcessedResult::Processed;
  }

  // 1. Enqueue the following steps to the [[codec work queue]]:
  // 1.1. Run the Establish Tracks algorithm.
  mDecoder->DecodeMetadata()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [self = RefPtr{this}](const image::DecodeMetadataResult& aMetadata) {
        self->OnMetadataSuccess(aMetadata);
      },
      [self = RefPtr{this}](const nsresult& aErr) {
        self->OnMetadataFailed(aErr);
      });
  return MessageProcessedResult::Processed;
}

MessageProcessedResult ImageDecoder::ProcessDecodeFrameMessage(
    DecodeFrameMessage* aMsg) {
  // 10.4.2. Running a control message to decode the image means running these
  // steps:
  //
  // 1. Enqueue the following steps to the [[codec work queue]]:
  // 1.1. Wait for [[tracks established]] to become true.
  //
  // 1.2. If options.completeFramesOnly is false and the image is a
  //      Progressive Image for which the User Agent supports progressive
  //      decoding, run the Decode Progressive Frame algorithm with
  //      options.frameIndex and promise.
  //
  // 1.3. Otherwise, run the Decode Complete Frame algorithm with
  //      options.frameIndex and promise.
  NS_DispatchToCurrentThread(NS_NewCancelableRunnableFunction(
      "ImageDecoder::ProcessDecodeFrameMessage",
      [self = RefPtr{this}] { self->CheckOutstandingDecodes(); }));
  return MessageProcessedResult::Processed;
}

MessageProcessedResult ImageDecoder::ProcessSelectTrackMessage(
    SelectTrackMessage* aMsg) {
  // 10.7.2. Running a control message to update the internal selected track
  // index means running these steps:
  //
  // 1. Enqueue the following steps to [[ImageDecoder]]'s [[codec work queue]]:
  // 1.1. Assign selectedIndex to [[internal selected track index]].
  // 1.2. Remove all entries from [[progressive frame generations]].
  //
  // At this time, progressive images and multi-track images are not supported.
  return MessageProcessedResult::Processed;
}

void ImageDecoder::CheckOutstandingDecodes() {
  // 10.2.5. Resolve Decode (with promise and result)

  // 1. If [[closed]], abort these steps.
  if (mClosed || !mTracks) {
    return;
  }

  ImageTrack* track = mTracks->GetDefaultTrack();
  if (!track) {
    return;
  }

  const uint32_t decodedFrameCount = track->DecodedFrameCount();
  const uint32_t frameCount = track->FrameCount();
  const bool frameCountComplete = track->FrameCountComplete();
  const bool decodedFramesComplete = track->DecodedFramesComplete();

  AutoTArray<OutstandingDecode, 4> resolved;
  AutoTArray<OutstandingDecode, 4> rejectedRange;
  AutoTArray<OutstandingDecode, 4> rejectedState;
  uint32_t minFrameIndex = UINT32_MAX;

  // 3. Remove promise from [[pending decode promises]].
  for (uint32_t i = 0; i < mOutstandingDecodes.Length();) {
    auto& decode = mOutstandingDecodes[i];
    const auto frameIndex = decode.mFrameIndex;
    if (frameIndex < decodedFrameCount) {
      MOZ_LOG(gWebCodecsLog, LogLevel::Debug,
              ("ImageDecoder %p CheckOutstandingDecodes -- resolved index %u",
               this, frameIndex));
      resolved.AppendElement(std::move(decode));
      mOutstandingDecodes.RemoveElementAt(i);
    } else if (frameCountComplete && frameCount <= frameIndex) {
      // We have gotten the frame count from the decoder, so we must reject any
      // unfulfilled requests that are beyond it with a RangeError.
      MOZ_LOG(gWebCodecsLog, LogLevel::Warning,
              ("ImageDecoder %p CheckOutstandingDecodes -- rejected index %u "
               "out-of-bounds",
               this, frameIndex));
      rejectedRange.AppendElement(std::move(decode));
      mOutstandingDecodes.RemoveElementAt(i);
    } else if (frameCountComplete && decodedFramesComplete) {
      // We have decoded all of the frames, but we produced fewer than the frame
      // count indicated. This means we ran into problems while decoding and
      // aborted. We must reject any unfulfilled requests with an
      // InvalidStateError.
      MOZ_LOG(gWebCodecsLog, LogLevel::Warning,
              ("ImageDecoder %p CheckOutstandingDecodes -- rejected index %u "
               "decode error",
               this, frameIndex));
      rejectedState.AppendElement(std::move(decode));
      mOutstandingDecodes.RemoveElementAt(i);
    } else if (!decodedFramesComplete) {
      // We haven't gotten the last frame yet, so we can advance to the next
      // one.
      MOZ_LOG(gWebCodecsLog, LogLevel::Debug,
              ("ImageDecoder %p CheckOutstandingDecodes -- pending index %u",
               this, frameIndex));
      if (frameCount > frameIndex) {
        minFrameIndex = std::min(minFrameIndex, frameIndex);
      }
      ++i;
    } else {
      // If none of the above, we have finished decoding all the frames we can,
      // but we raced against the frame count completion. Once that finishes, we
      // will run again, and we can appropriately fail frame requests as either
      // out-of-bounds or decoding failures.
      MOZ_ASSERT(!frameCountComplete);
    }
  }

  if (minFrameIndex < UINT32_MAX) {
    RequestDecodeFrames(minFrameIndex + 1 - decodedFrameCount);
  }

  // 4. Resolve promise with result.
  for (const auto& i : resolved) {
    ImageDecodeResult result;
    result.mImage = track->GetDecodedFrame(i.mFrameIndex);
    // TODO(aosmond): progressive images
    result.mComplete = true;
    i.mPromise->MaybeResolve(result);
  }

  for (const auto& i : rejectedRange) {
    i.mPromise->MaybeRejectWithRangeError("No more frames available"_ns);
  }

  for (const auto& i : rejectedState) {
    i.mPromise->MaybeRejectWithInvalidStateError("Error decoding frame"_ns);
  }
}

/* static */ already_AddRefed<ImageDecoder> ImageDecoder::Constructor(
    const GlobalObject& aGlobal, const ImageDecoderInit& aInit,
    ErrorResult& aRv) {
  // 10.2.2.1. If init is not valid ImageDecoderInit, throw a TypeError.
  // 10.3.1. If type is not a valid image MIME type, return false.
  const auto mimeType = Substring(aInit.mType, 0, 6);
  if (!mimeType.Equals(u"image/"_ns)) {
    MOZ_LOG(gWebCodecsLog, LogLevel::Error,
            ("ImageDecoder Constructor -- bad mime type"));
    aRv.ThrowTypeError("Invalid MIME type, must be 'image'");
    return nullptr;
  }

  RefPtr<ImageDecoderReadRequest> readRequest;

  if (aInit.mData.IsReadableStream()) {
    const auto& stream = aInit.mData.GetAsReadableStream();
    // 10.3.2. If data is of type ReadableStream and the ReadableStream is
    // disturbed or locked, return false.
    if (stream->Disturbed() || stream->Locked()) {
      MOZ_LOG(gWebCodecsLog, LogLevel::Error,
              ("ImageDecoder Constructor -- bad stream"));
      aRv.ThrowTypeError("ReadableStream data is disturbed and/or locked");
      return nullptr;
    }
  } else {
    // 10.3.3. If data is of type BufferSource:
    bool empty;
    if (aInit.mData.IsArrayBufferView()) {
      const auto& view = aInit.mData.GetAsArrayBufferView();
      empty = view.ProcessData(
          [](const Span<uint8_t>& aData, JS::AutoCheckCannotGC&&) {
            return aData.IsEmpty();
          });
    } else if (aInit.mData.IsArrayBuffer()) {
      const auto& buffer = aInit.mData.GetAsArrayBuffer();
      empty = buffer.ProcessData(
          [](const Span<uint8_t>& aData, JS::AutoCheckCannotGC&&) {
            return aData.IsEmpty();
          });
    } else {
      MOZ_ASSERT_UNREACHABLE("Unsupported data type!");
      aRv.ThrowNotSupportedError("Unsupported data type");
      return nullptr;
    }

    // 10.3.3.1. If data is [detached], return false.
    // 10.3.3.2. If data is empty, return false.
    if (empty) {
      MOZ_LOG(gWebCodecsLog, LogLevel::Error,
              ("ImageDecoder Constructor -- detached/empty BufferSource"));
      aRv.ThrowTypeError("BufferSource is detached/empty");
      return nullptr;
    }
  }

  // 10.3.4. If desiredWidth exists and desiredHeight does not exist, return
  // false.
  // 10.3.5. If desiredHeight exists and desiredWidth does not exist, return
  // false.
  if (aInit.mDesiredHeight.WasPassed() != aInit.mDesiredWidth.WasPassed()) {
    MOZ_LOG(gWebCodecsLog, LogLevel::Error,
            ("ImageDecoder Constructor -- both/neither desiredHeight/width "
             "needed"));
    aRv.ThrowTypeError(
        "Both or neither of desiredHeight and desiredWidth must be passed");
    return nullptr;
  }

  nsTHashSet<const ArrayBuffer*> transferSet;
  for (const auto& buffer : aInit.mTransfer) {
    // 10.2.2.2. If init.transfer contains more than one reference to the same
    // ArrayBuffer, then throw a DataCloneError DOMException.
    if (transferSet.Contains(&buffer)) {
      MOZ_LOG(
          gWebCodecsLog, LogLevel::Error,
          ("ImageDecoder Constructor -- duplicate transferred ArrayBuffer"));
      aRv.ThrowDataCloneError(
          "Transfer contains duplicate ArrayBuffer objects");
      return nullptr;
    }
    transferSet.Insert(&buffer);
    // 10.2.2.3.1. If [[Detached]] internal slot is true, then throw a
    // DataCloneError DOMException.
    bool empty = buffer.ProcessData(
        [&](const Span<uint8_t>& aData, JS::AutoCheckCannotGC&&) {
          return aData.IsEmpty();
        });
    if (empty) {
      MOZ_LOG(gWebCodecsLog, LogLevel::Error,
              ("ImageDecoder Constructor -- empty/detached transferred "
               "ArrayBuffer"));
      aRv.ThrowDataCloneError(
          "Transfer contains empty/detached ArrayBuffer objects");
      return nullptr;
    }
  }

  // 10.2.2.4. Let d be a new ImageDecoder object. In the steps below, all
  //           mentions of ImageDecoder members apply to d unless stated
  //           otherwise.
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  auto imageDecoder = MakeRefPtr<ImageDecoder>(std::move(global), aInit.mType);
  imageDecoder->Initialize(aGlobal, aInit, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    MOZ_LOG(gWebCodecsLog, LogLevel::Error,
            ("ImageDecoder Constructor -- initialize failed"));
    return nullptr;
  }

  // 10.2.2.19. For each transferable in init.transfer:
  // 10.2.2.19.1. Perform DetachArrayBuffer on transferable
  for (const auto& buffer : aInit.mTransfer) {
    JS::Rooted<JSObject*> obj(aGlobal.Context(), buffer.Obj());
    JS::DetachArrayBuffer(aGlobal.Context(), obj);
  }

  // 10.2.2.20. return d.
  return imageDecoder.forget();
}

/* static */ already_AddRefed<Promise> ImageDecoder::IsTypeSupported(
    const GlobalObject& aGlobal, const nsAString& aType, ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  const auto subType = Substring(aType, 0, 6);
  if (!subType.Equals(u"image/"_ns)) {
    promise->MaybeRejectWithTypeError("Invalid MIME type, must be 'image'"_ns);
    return promise.forget();
  }

  NS_ConvertUTF16toUTF8 mimeType(aType);
  image::DecoderType type = image::ImageUtils::GetDecoderType(mimeType);
  promise->MaybeResolve(type != image::DecoderType::UNKNOWN);
  return promise.forget();
}

void ImageDecoder::Initialize(const GlobalObject& aGlobal,
                              const ImageDecoderInit& aInit, ErrorResult& aRv) {
  mShutdownWatcher = media::ShutdownWatcher::Create(this);
  if (!mShutdownWatcher) {
    MOZ_LOG(
        gWebCodecsLog, LogLevel::Error,
        ("ImageDecoder %p Initialize -- create shutdown watcher failed", this));
    aRv.ThrowInvalidStateError("Could not create shutdown watcher");
    return;
  }

  mCompletePromise = Promise::Create(mParent, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    MOZ_LOG(gWebCodecsLog, LogLevel::Error,
            ("ImageDecoder %p Initialize -- create promise failed", this));
    return;
  }

  // 10.2.2.8. Assign [[ImageTrackList]] a new ImageTrackList initialized as
  // follows:
  // 10.2.2.8.1. Assign a new list to [[track list]].
  mTracks = MakeAndAddRef<ImageTrackList>(mParent, this);
  mTracks->Initialize(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    MOZ_LOG(gWebCodecsLog, LogLevel::Error,
            ("ImageDecoder %p Initialize -- create tracks failed", this));
    return;
  }

  mSourceBuffer = MakeRefPtr<image::SourceBuffer>();

  const auto fnSourceBufferFromSpan = [&](const Span<uint8_t>& aData) {
    nsresult rv = mSourceBuffer->ExpectLength(aData.Length());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      MOZ_LOG(
          gWebCodecsLog, LogLevel::Error,
          ("ImageDecoder %p Initialize -- failed to pre-allocate source buffer",
           this));
      aRv.ThrowRangeError("Could not allocate for encoded source buffer");
      return;
    }

    // 10.2.2.18.3.2. Assign a copy of init.data to [[encoded data]].
    rv = mSourceBuffer->Append(reinterpret_cast<const char*>(aData.Elements()),
                               aData.Length());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      MOZ_LOG(gWebCodecsLog, LogLevel::Error,
              ("ImageDecoder %p Initialize -- failed to append source buffer",
               this));
      aRv.ThrowRangeError("Could not allocate for encoded source buffer");
      return;
    }

    mSourceBuffer->Complete(NS_OK);

    // 10.2.2.18.4. Assign true to [[complete]].
    // 10.2.2.18.5. Resolve [[completed promise]].
    OnCompleteSuccess();
  };

  if (aInit.mData.IsReadableStream()) {
    // 10.2.2.17. If init’s data member is of type ReadableStream:
    const auto& stream = aInit.mData.GetAsReadableStream();

    // 10.2.2.17.2. Assign false to [[complete]]
    MOZ_ASSERT(!mComplete);

    // 10.2.2.17.5. Let reader be the result of getting a reader for data.
    // 10.2.2.17.6. In parallel, perform the Fetch Stream Data Loop on d with
    //              reader.
    mReadRequest = MakeAndAddRef<ImageDecoderReadRequest>(mSourceBuffer);
    if (NS_WARN_IF(!mReadRequest->Initialize(aGlobal, this, stream))) {
      MOZ_LOG(
          gWebCodecsLog, LogLevel::Error,
          ("ImageDecoder %p Initialize -- create read request failed", this));
      aRv.ThrowInvalidStateError("Could not create reader for ReadableStream");
      return;
    }
  } else if (aInit.mData.IsArrayBufferView()) {
    // 10.2.2.18.3.1. Assert that init.data is of type BufferSource.
    const auto& view = aInit.mData.GetAsArrayBufferView();
    view.ProcessFixedData(fnSourceBufferFromSpan);
    if (aRv.Failed()) {
      return;
    }
  } else if (aInit.mData.IsArrayBuffer()) {
    // 10.2.2.18.3.1. Assert that init.data is of type BufferSource.
    const auto& buffer = aInit.mData.GetAsArrayBuffer();
    buffer.ProcessFixedData(fnSourceBufferFromSpan);
    if (aRv.Failed()) {
      return;
    }
  } else {
    MOZ_ASSERT_UNREACHABLE("Unsupported data type!");
    aRv.ThrowNotSupportedError("Unsupported data type");
    return;
  }

  Maybe<gfx::IntSize> desiredSize;
  if (aInit.mDesiredWidth.WasPassed() && aInit.mDesiredHeight.WasPassed()) {
    desiredSize.emplace(
        std::min(aInit.mDesiredWidth.Value(), static_cast<uint32_t>(INT32_MAX)),
        std::min(aInit.mDesiredHeight.Value(),
                 static_cast<uint32_t>(INT32_MAX)));
  }

  // 10.2.2.17.3 / 10.2.2.18.6.
  //   Queue a control message to configure the image decoder with init.
  QueueConfigureMessage(desiredSize, aInit.mColorSpaceConversion);

  // 10.2.10.2.2.18.7. Queue a control message to decode track metadata.
  //
  // Note that for readable streams it doesn't ever say to decode the metadata,
  // but we can reasonably assume it means to decode the metadata in parallel
  // with the reading of the stream.
  QueueDecodeMetadataMessage();

  // 10.2.2.18.8. Process the control message queue.
  ProcessControlMessageQueue();
}

void ImageDecoder::OnSourceBufferComplete(const MediaResult& aResult) {
  MOZ_LOG(gWebCodecsLog, LogLevel::Debug,
          ("ImageDecoder %p OnSourceBufferComplete -- success %d", this,
           NS_SUCCEEDED(aResult.Code())));

  MOZ_ASSERT(mSourceBuffer->IsComplete());

  if (NS_WARN_IF(NS_FAILED(aResult.Code()))) {
    OnCompleteFailed(aResult);
    return;
  }

  OnCompleteSuccess();
}

void ImageDecoder::OnCompleteSuccess() {
  if (mComplete) {
    return;
  }

  // There are two conditions we need to fulfill before we are complete:
  //
  // 10.2.1. Internal Slots - [[complete]]
  // A boolean indicating whether [[encoded data]] is completely buffered.
  //
  // 10.6.1. Internal Slots - [[ready promise]]
  // NOTE: ImageTrack frameCount can receive subsequent updates until complete
  // is true.
  if (!mSourceBuffer->IsComplete() || !mHasFrameCount) {
    MOZ_LOG(gWebCodecsLog, LogLevel::Debug,
            ("ImageDecoder %p OnCompleteSuccess -- not complete yet; "
             "sourceBuffer %d, hasFrameCount %d",
             this, mSourceBuffer->IsComplete(), mHasFrameCount));
    return;
  }

  MOZ_LOG(gWebCodecsLog, LogLevel::Debug,
          ("ImageDecoder %p OnCompleteSuccess -- complete", this));
  mComplete = true;
  mCompletePromise->MaybeResolveWithUndefined();
}

void ImageDecoder::OnCompleteFailed(const MediaResult& aResult) {
  if (mComplete) {
    return;
  }

  MOZ_LOG(gWebCodecsLog, LogLevel::Error,
          ("ImageDecoder %p OnCompleteFailed -- complete", this));
  mComplete = true;
  aResult.RejectTo(mCompletePromise);
}

void ImageDecoder::OnMetadataSuccess(
    const image::DecodeMetadataResult& aMetadata) {
  if (mClosed || !mTracks) {
    return;
  }

  // 10.2.5. Establish Tracks

  // 1. Assert [[tracks established]] is false.
  MOZ_ASSERT(!mTracksEstablished);

  // 2. and 3. See ImageDecoder::OnMetadataFailed.

  MOZ_LOG(gWebCodecsLog, LogLevel::Debug,
          ("ImageDecoder %p OnMetadataSuccess -- %dx%d, repetitions %d, "
           "animated %d, frameCount %u, frameCountComplete %d",
           this, aMetadata.mWidth, aMetadata.mHeight, aMetadata.mRepetitions,
           aMetadata.mAnimated, aMetadata.mFrameCount,
           aMetadata.mFrameCountComplete));

  // 4. - 9., 11. See ImageTrackList::OnMetadataSuccess
  mTracks->OnMetadataSuccess(aMetadata);

  // 10. Assign true to [[tracks established]].
  mTracksEstablished = true;

  // If our encoded data comes from a ReadableStream, we may not have reached
  // the end of the stream yet. As such, our frame count may be incomplete.
  OnFrameCountSuccess(image::DecodeFrameCountResult{
      aMetadata.mFrameCount, aMetadata.mFrameCountComplete});
}

void ImageDecoder::OnMetadataFailed(const nsresult& aErr) {
  MOZ_LOG(gWebCodecsLog, LogLevel::Error,
          ("ImageDecoder %p OnMetadataFailed 0x%08x", this,
           static_cast<uint32_t>(aErr)));

  // 10.2.5. Establish Tracks

  // 1. Assert [[tracks established]] is false.
  MOZ_ASSERT(!mTracksEstablished);

  // 2. If [[encoded data]] does not contain enough data to determine the
  //    number of tracks:
  // 2.1. If complete is true, queue a task to run the Close ImageDecoder
  //      algorithm.
  // 2.2. Abort these steps.
  // 3. If the number of tracks is found to be 0, queue a task to run the Close
  //    ImageDecoder algorithm and abort these steps.
  Close(MediaResult(NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR,
                    "Metadata decoding failed"_ns));
}

void ImageDecoder::RequestFrameCount(uint32_t aKnownFrameCount) {
  MOZ_ASSERT(!mHasFrameCount);

  if (NS_WARN_IF(!mDecoder)) {
    return;
  }

  MOZ_LOG(gWebCodecsLog, LogLevel::Debug,
          ("ImageDecoder %p RequestFrameCount -- knownFrameCount %u", this,
           aKnownFrameCount));
  mDecoder->DecodeFrameCount(aKnownFrameCount)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [self = RefPtr{this}](const image::DecodeFrameCountResult& aResult) {
            self->OnFrameCountSuccess(aResult);
          },
          [self = RefPtr{this}](const nsresult& aErr) {
            self->OnFrameCountFailed(aErr);
          });
}

void ImageDecoder::RequestDecodeFrames(uint32_t aFramesToDecode) {
  if (!mDecoder || mHasFramePending) {
    return;
  }

  mHasFramePending = true;

  MOZ_LOG(gWebCodecsLog, LogLevel::Debug,
          ("ImageDecoder %p RequestDecodeFrames -- framesToDecode %u", this,
           aFramesToDecode));

  mDecoder->DecodeFrames(aFramesToDecode)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [self = RefPtr{this}](const image::DecodeFramesResult& aResult) {
            self->OnDecodeFramesSuccess(aResult);
          },
          [self = RefPtr{this}](const nsresult& aErr) {
            self->OnDecodeFramesFailed(aErr);
          });
}

void ImageDecoder::OnFrameCountSuccess(
    const image::DecodeFrameCountResult& aResult) {
  if (mClosed || !mTracks) {
    return;
  }

  MOZ_LOG(gWebCodecsLog, LogLevel::Debug,
          ("ImageDecoder %p OnFrameCountSuccess -- frameCount %u, finished %d",
           this, aResult.mFrameCount, aResult.mFinished));

  // 10.2.5. Update Tracks.

  // 1. Assert [[tracks established]] is true.
  MOZ_ASSERT(mTracksEstablished);

  // 2. - 6. See ImageTrackList::OnFrameCountSuccess.
  mTracks->OnFrameCountSuccess(aResult);

  if (aResult.mFinished) {
    mHasFrameCount = true;
    OnCompleteSuccess();
  } else {
    RequestFrameCount(aResult.mFrameCount);
  }

  CheckOutstandingDecodes();
}

void ImageDecoder::OnFrameCountFailed(const nsresult& aErr) {
  MOZ_LOG(gWebCodecsLog, LogLevel::Error,
          ("ImageDecoder %p OnFrameCountFailed", this));
  Close(MediaResult(NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR,
                    "Frame count decoding failed"_ns));
}

void ImageDecoder::GetType(nsAString& aType) const { aType.Assign(mType); }

already_AddRefed<Promise> ImageDecoder::Decode(
    const ImageDecodeOptions& aOptions, ErrorResult& aRv) {
  // 10.2.4. decode(options)

  // 4. Let promise be a new Promise.
  RefPtr<Promise> promise = Promise::Create(mParent, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    MOZ_LOG(gWebCodecsLog, LogLevel::Error,
            ("ImageDecoder %p Decode -- create promise failed", this));
    return nullptr;
  }

  // NOTE: Calling decode() on the constructed ImageDecoder will trigger a
  // NotSupportedError if the User Agent does not support type. This would have
  // been set in Close by ProcessConfigureMessage.
  if (mTypeNotSupported) {
    MOZ_LOG(gWebCodecsLog, LogLevel::Error,
            ("ImageDecoder %p Decode -- not supported", this));
    promise->MaybeRejectWithNotSupportedError("Unsupported MIME type"_ns);
    return promise.forget();
  }

  // 1. If [[closed]] is true, return a Promise rejected with an
  //    InvalidStateError DOMException.
  if (mClosed || !mTracks || !mDecoder) {
    MOZ_LOG(gWebCodecsLog, LogLevel::Error,
            ("ImageDecoder %p Decode -- closed", this));
    promise->MaybeRejectWithInvalidStateError("Closed decoder"_ns);
    return promise.forget();
  }

  // 2. If [[ImageTrackList]]'s [[selected index]] is '-1', return a Promise
  //    rejected with an InvalidStateError DOMException.
  //
  // This must be balanced with the fact that we might get a decode call before
  // the tracks are established and we are supposed to wait.
  ImageTrack* track = mTracks->GetSelectedTrack();
  if (mTracksEstablished && !track) {
    MOZ_LOG(gWebCodecsLog, LogLevel::Error,
            ("ImageDecoder %p Decode -- no track selected", this));
    promise->MaybeRejectWithInvalidStateError("No track selected"_ns);
    return promise.forget();
  }

  // 3. If options is undefined, assign a new ImageDecodeOptions to options.
  // 5. Append promise to [[pending decode promises]].
  mOutstandingDecodes.AppendElement(OutstandingDecode{
      promise, aOptions.mFrameIndex, aOptions.mCompleteFramesOnly});

  // 6. Queue a control message to decode the image with options, and promise.
  QueueDecodeFrameMessage();

  // 7. Process the control message queue.
  ProcessControlMessageQueue();

  // 8. Return promise.
  return promise.forget();
}

void ImageDecoder::OnDecodeFramesSuccess(
    const image::DecodeFramesResult& aResult) {
  // 10.2.5. Decode Complete Frame (with frameIndex and promise)
  MOZ_ASSERT(mHasFramePending);
  mHasFramePending = false;

  // 1. Assert that [[tracks established]] is true.
  MOZ_ASSERT(mTracksEstablished);

  if (mClosed || !mTracks) {
    return;
  }

  ImageTrack* track = mTracks->GetDefaultTrack();
  if (NS_WARN_IF(!track)) {
    MOZ_ASSERT_UNREACHABLE("Must have default track!");
    return;
  }

  track->OnDecodeFramesSuccess(aResult);

  CheckOutstandingDecodes();
}

void ImageDecoder::OnDecodeFramesFailed(const nsresult& aErr) {
  MOZ_ASSERT(mHasFramePending);
  mHasFramePending = false;

  MOZ_LOG(gWebCodecsLog, LogLevel::Error,
          ("ImageDecoder %p OnDecodeFramesFailed", this));

  AutoTArray<OutstandingDecode, 1> rejected = std::move(mOutstandingDecodes);
  for (const auto& i : rejected) {
    MOZ_LOG(gWebCodecsLog, LogLevel::Error,
            ("ImageDecoder %p OnDecodeFramesFailed -- reject index %u", this,
             i.mFrameIndex));
    i.mPromise->MaybeRejectWithRangeError("No more frames available"_ns);
  }
}

void ImageDecoder::Reset(const MediaResult& aResult) {
  MOZ_LOG(gWebCodecsLog, LogLevel::Debug, ("ImageDecoder %p Reset", this));
  // 10.2.5. Reset ImageDecoder (with exception)

  // 1. Signal [[codec implementation]] to abort any active decoding operation.
  if (mDecoder) {
    mDecoder->CancelDecodeFrames();
  }

  // 2. For each decodePromise in [[pending decode promises]]:
  // 2.1. Reject decodePromise with exception.
  // 2.3. Remove decodePromise from [[pending decode promises]].
  AutoTArray<OutstandingDecode, 1> rejected = std::move(mOutstandingDecodes);
  for (const auto& i : rejected) {
    MOZ_LOG(gWebCodecsLog, LogLevel::Debug,
            ("ImageDecoder %p Reset -- reject index %u", this, i.mFrameIndex));
    aResult.RejectTo(i.mPromise);
  }
}

void ImageDecoder::Close(const MediaResult& aResult) {
  MOZ_LOG(gWebCodecsLog, LogLevel::Debug, ("ImageDecoder %p Close", this));

  // 10.2.5. Algorithms - Close ImageDecoder (with exception)
  mClosed = true;
  mTypeNotSupported = aResult.Code() == NS_ERROR_DOM_NOT_SUPPORTED_ERR;

  // 1. Run the Reset ImageDecoder algorithm with exception.
  Reset(aResult);

  // 3. Clear [[codec implementation]] and release associated system resources.
  if (mDecoder) {
    mDecoder->Destroy();
  }

  if (mReadRequest) {
    mReadRequest->Destroy(/* aCancel */ true);
    mReadRequest = nullptr;
  }

  mSourceBuffer = nullptr;
  mDecoder = nullptr;
  mType = u""_ns;

  // 4. Remove all entries from [[ImageTrackList]].
  // 5. Assign -1 to [[ImageTrackList]]'s [[selected index]].
  if (mTracks) {
    mTracks->MaybeRejectReady(aResult);
    mTracks->Destroy();
  }

  if (!mComplete) {
    aResult.RejectTo(mCompletePromise);
    mComplete = true;
  }

  if (mShutdownWatcher) {
    mShutdownWatcher->Destroy();
    mShutdownWatcher = nullptr;
  }
}

void ImageDecoder::Reset() {
  Reset(MediaResult(NS_ERROR_DOM_ABORT_ERR, "Reset decoder"_ns));
}

void ImageDecoder::Close() {
  Close(MediaResult(NS_ERROR_DOM_ABORT_ERR, "Closed decoder"_ns));
}

void ImageDecoder::OnShutdown() {
  Close(MediaResult(NS_ERROR_DOM_ABORT_ERR, "Shutdown"_ns));
}

}  // namespace mozilla::dom
