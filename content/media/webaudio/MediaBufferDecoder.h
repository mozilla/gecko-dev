/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaBufferDecoder_h_
#define MediaBufferDecoder_h_

#include "nsWrapperCache.h"
#include "nsCOMPtr.h"
#include "nsIThreadPool.h"
#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/MemoryReporting.h"

namespace mozilla {

namespace dom {
class AudioBuffer;
class AudioContext;
class DecodeErrorCallback;
class DecodeSuccessCallback;
}

struct WebAudioDecodeJob MOZ_FINAL
{
  // You may omit both the success and failure callback, or you must pass both.
  // The callbacks are only necessary for asynchronous operation.
  WebAudioDecodeJob(const nsACString& aContentType,
                    dom::AudioContext* aContext,
                    dom::DecodeSuccessCallback* aSuccessCallback = nullptr,
                    dom::DecodeErrorCallback* aFailureCallback = nullptr);

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebAudioDecodeJob)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebAudioDecodeJob)

  enum ErrorCode {
    NoError,
    UnknownContent,
    UnknownError,
    InvalidContent,
    NoAudio
  };

  typedef void (WebAudioDecodeJob::*ResultFn)(ErrorCode);
  typedef nsAutoArrayPtr<float> ChannelBuffer;

  void OnSuccess(ErrorCode /* ignored */);
  void OnFailure(ErrorCode aErrorCode);

  bool AllocateBuffer();

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  nsCString mContentType;
  uint32_t mWriteIndex;
  nsRefPtr<dom::AudioContext> mContext;
  nsRefPtr<dom::DecodeSuccessCallback> mSuccessCallback;
  nsRefPtr<dom::DecodeErrorCallback> mFailureCallback; // can be null
  nsRefPtr<dom::AudioBuffer> mOutput;
  FallibleTArray<ChannelBuffer> mChannelBuffers;

private:
  ~WebAudioDecodeJob();
};

/**
 * This class is used to decode media buffers on a dedicated threadpool.
 *
 * This class manages the resources that it uses internally (such as the
 * thread-pool) and provides a clean external interface.
 */
class MediaBufferDecoder
{
public:
  void AsyncDecodeMedia(const char* aContentType, uint8_t* aBuffer,
                        uint32_t aLength, WebAudioDecodeJob& aDecodeJob);

  ~MediaBufferDecoder() { Shutdown(); }
  void Shutdown();

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    return 0;
  }

private:
  bool EnsureThreadPoolInitialized();

private:
  nsCOMPtr<nsIThreadPool> mThreadPool;
};

}

#endif

