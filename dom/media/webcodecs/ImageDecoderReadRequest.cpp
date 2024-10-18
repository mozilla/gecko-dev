/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageDecoderReadRequest.h"
#include "MediaResult.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/dom/ImageDecoder.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/ReadableStreamDefaultReader.h"
#include "mozilla/image/SourceBuffer.h"
#include "mozilla/Logging.h"

extern mozilla::LazyLogModule gWebCodecsLog;

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(ImageDecoderReadRequest, ReadRequest,
                                   mDecoder, mReader)
NS_IMPL_ADDREF_INHERITED(ImageDecoderReadRequest, ReadRequest)
NS_IMPL_RELEASE_INHERITED(ImageDecoderReadRequest, ReadRequest)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ImageDecoderReadRequest)
NS_INTERFACE_MAP_END_INHERITING(ReadRequest)

ImageDecoderReadRequest::ImageDecoderReadRequest(
    image::SourceBuffer* aSourceBuffer)
    : mSourceBuffer(std::move(aSourceBuffer)) {
  MOZ_LOG(gWebCodecsLog, LogLevel::Debug,
          ("ImageDecoderReadRequest %p ImageDecoderReadRequest", this));
}

ImageDecoderReadRequest::~ImageDecoderReadRequest() {
  MOZ_LOG(gWebCodecsLog, LogLevel::Debug,
          ("ImageDecoderReadRequest %p ~ImageDecoderReadRequest", this));
}

bool ImageDecoderReadRequest::Initialize(const GlobalObject& aGlobal,
                                         ImageDecoder* aDecoder,
                                         ReadableStream& aStream) {
  IgnoredErrorResult rv;
  mReader = aStream.GetReader(rv);
  if (NS_WARN_IF(rv.Failed())) {
    MOZ_LOG(
        gWebCodecsLog, LogLevel::Error,
        ("ImageDecoderReadRequest %p Initialize -- cannot get stream reader",
         this));
    mSourceBuffer->Complete(NS_ERROR_FAILURE);
    Destroy(/* aCancel */ false);
    return false;
  }

  mDecoder = aDecoder;
  QueueRead();
  return true;
}

void ImageDecoderReadRequest::Destroy(bool aCancel) {
  MOZ_LOG(gWebCodecsLog, LogLevel::Debug,
          ("ImageDecoderReadRequest %p Destroy", this));

  if (aCancel) {
    // Ensure we stop reading from the ReadableStream.
    Cancel();
  }

  if (mSourceBuffer) {
    if (!mSourceBuffer->IsComplete()) {
      mSourceBuffer->Complete(NS_ERROR_ABORT);
    }
    mSourceBuffer = nullptr;
  }

  mDecoder = nullptr;
  mReader = nullptr;
}

void ImageDecoderReadRequest::QueueRead() {
  class ReadRunnable final : public CancelableRunnable {
   public:
    explicit ReadRunnable(ImageDecoderReadRequest* aOwner)
        : CancelableRunnable(
              "mozilla::dom::ImageDecoderReadRequest::QueueRead"),
          mOwner(aOwner) {}

    NS_IMETHODIMP Run() override {
      mOwner->Read();
      mOwner = nullptr;
      return NS_OK;
    }

    nsresult Cancel() override {
      mOwner->Complete(
          MediaResult(NS_ERROR_DOM_MEDIA_ABORT_ERR, "Read cancelled"_ns));
      mOwner = nullptr;
      return NS_OK;
    }

   private:
    virtual ~ReadRunnable() {
      if (mOwner) {
        Cancel();
      }
    }

    RefPtr<ImageDecoderReadRequest> mOwner;
  };

  if (!mReader) {
    MOZ_LOG(gWebCodecsLog, LogLevel::Debug,
            ("ImageDecoderReadRequest %p QueueRead -- destroyed", this));
    return;
  }

  MOZ_LOG(gWebCodecsLog, LogLevel::Debug,
          ("ImageDecoderReadRequest %p QueueRead -- queue", this));
  auto task = MakeRefPtr<ReadRunnable>(this);
  NS_DispatchToCurrentThread(task.forget());
}

void ImageDecoderReadRequest::Read() {
  if (!mReader || !mDecoder) {
    MOZ_LOG(gWebCodecsLog, LogLevel::Debug,
            ("ImageDecoderReadRequest %p Read -- destroyed", this));
    return;
  }

  AutoJSAPI jsapi;
  if (!jsapi.Init(mDecoder->GetParentObject())) {
    MOZ_LOG(gWebCodecsLog, LogLevel::Debug,
            ("ImageDecoderReadRequest %p Read -- no jsapi", this));
    Complete(MediaResult(NS_ERROR_DOM_FILE_NOT_READABLE_ERR,
                         "Reader cannot init jsapi"_ns));
    return;
  }

  RefPtr<ImageDecoderReadRequest> self(this);
  RefPtr<ReadableStreamDefaultReader> reader(mReader);

  MOZ_LOG(gWebCodecsLog, LogLevel::Debug,
          ("ImageDecoderReadRequest %p Read -- begin read chunk", this));

  IgnoredErrorResult err;
  reader->ReadChunk(jsapi.cx(), *self, err);
  if (NS_WARN_IF(err.Failed())) {
    MOZ_LOG(gWebCodecsLog, LogLevel::Error,
            ("ImageDecoderReadRequest %p Read -- read chunk failed", this));
    Complete(MediaResult(NS_ERROR_DOM_FILE_NOT_READABLE_ERR,
                         "Reader cannot read chunk from stream"_ns));
  }

  MOZ_LOG(gWebCodecsLog, LogLevel::Debug,
          ("ImageDecoderReadRequest %p Read -- end read chunk", this));
}

void ImageDecoderReadRequest::Cancel() {
  RefPtr<ReadableStreamDefaultReader> reader = std::move(mReader);
  if (!reader || !mDecoder) {
    return;
  }

  RefPtr<ImageDecoderReadRequest> self(this);

  AutoJSAPI jsapi;
  if (!jsapi.Init(mDecoder->GetParentObject())) {
    MOZ_LOG(gWebCodecsLog, LogLevel::Debug,
            ("ImageDecoderReadRequest %p Cancel -- no jsapi", this));
    return;
  }

  ErrorResult rv;
  rv.ThrowAbortError("ImageDecoderReadRequest destroyed");

  JS::Rooted<JS::Value> errorValue(jsapi.cx());
  if (ToJSValue(jsapi.cx(), std::move(rv), &errorValue)) {
    IgnoredErrorResult ignoredRv;
    if (RefPtr<Promise> p = reader->Cancel(jsapi.cx(), errorValue, ignoredRv)) {
      MOZ_ALWAYS_TRUE(p->SetAnyPromiseIsHandled());
    }
  }

  jsapi.ClearException();
}

void ImageDecoderReadRequest::Complete(const MediaResult& aResult) {
  if (!mReader) {
    return;
  }

  MOZ_LOG(gWebCodecsLog, LogLevel::Debug,
          ("ImageDecoderReadRequest %p Read -- complete, success %d", this,
           NS_SUCCEEDED(aResult.Code())));

  if (mSourceBuffer && !mSourceBuffer->IsComplete()) {
    mSourceBuffer->Complete(aResult.Code());
  }

  if (mDecoder) {
    mDecoder->OnSourceBufferComplete(aResult);
  }

  Destroy(/* aCancel */ false);
}

void ImageDecoderReadRequest::ChunkSteps(JSContext* aCx,
                                         JS::Handle<JS::Value> aChunk,
                                         ErrorResult& aRv) {
  // 10.2.5. Fetch Stream Data Loop (with reader) - chunk steps

  // 1. If [[closed]] is true, abort these steps.
  if (!mSourceBuffer) {
    return;
  }

  // 2. If chunk is not a Uint8Array object, queue a task to run the Close
  // ImageDecoder algorithm with a DataError DOMException and abort these steps.
  RootedSpiderMonkeyInterface<Uint8Array> chunk(aCx);
  if (!aChunk.isObject() || !chunk.Init(&aChunk.toObject())) {
    MOZ_LOG(gWebCodecsLog, LogLevel::Error,
            ("ImageDecoderReadRequest %p ChunkSteps -- bad chunk", this));
    Complete(MediaResult(NS_ERROR_DOM_DATA_ERR,
                         "Reader cannot read chunk from stream"_ns));
    return;
  }

  chunk.ProcessFixedData([&](const Span<uint8_t>& aData) {
    MOZ_LOG(gWebCodecsLog, LogLevel::Debug,
            ("ImageDecoderReadRequest %p ChunkSteps -- write %zu bytes", this,
             aData.Length()));

    // 3. Let bytes be the byte sequence represented by the Uint8Array object.
    // 4. Append bytes to the [[encoded data]] internal slot.
    nsresult rv = mSourceBuffer->Append(
        reinterpret_cast<const char*>(aData.Elements()), aData.Length());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      MOZ_LOG(
          gWebCodecsLog, LogLevel::Debug,
          ("ImageDecoderReadRequest %p ChunkSteps -- failed to append", this));
      Complete(MediaResult(NS_ERROR_DOM_UNKNOWN_ERR,
                           "Reader cannot allocate storage for chunk"_ns));
    }

    // 5. If [[tracks established]] is false, run the Establish Tracks
    //    algorithm.
    // 6. Otherwise, run the Update Tracks algorithm.
    //
    // Note that these steps will be triggered by the decoder promise callbacks.
  });

  // 7. Run the Fetch Stream Data Loop algorithm with reader.
  QueueRead();
}

void ImageDecoderReadRequest::CloseSteps(JSContext* aCx, ErrorResult& aRv) {
  MOZ_LOG(gWebCodecsLog, LogLevel::Debug,
          ("ImageDecoderReadRequest %p CloseSteps", this));

  // 10.2.5. Fetch Stream Data Loop (with reader) - close steps
  // 1. Assign true to [[complete]]
  // 2. Resolve [[completed promise]].
  Complete(MediaResult(NS_OK));
}

void ImageDecoderReadRequest::ErrorSteps(JSContext* aCx,
                                         JS::Handle<JS::Value> aError,
                                         ErrorResult& aRv) {
  MOZ_LOG(gWebCodecsLog, LogLevel::Debug,
          ("ImageDecoderReadRequest %p ErrorSteps", this));

  // 10.2.5. Fetch Stream Data Loop (with reader) - error steps
  // 1. Queue a task to run the Close ImageDecoder algorithm with a
  //    NotReadableError DOMException
  Complete(MediaResult(NS_ERROR_DOM_FILE_NOT_READABLE_ERR,
                       "Reader failed while waiting for chunk from stream"_ns));
}

}  // namespace mozilla::dom
