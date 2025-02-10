/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The multiplex stream concatenates a list of input streams into a single
 * stream.
 */

#ifndef _nsMultiplexInputStream_h_
#define _nsMultiplexInputStream_h_

#include "nsIBufferedStreams.h"
#include "nsICloneableInputStream.h"
#include "nsIMultiplexInputStream.h"
#include "nsISeekableStream.h"
#include "nsCOMPtr.h"
#include "nsIIPCSerializableInputStream.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "nsIAsyncInputStream.h"
#include "nsIInputStreamLength.h"
#include "nsNetUtil.h"
#include "nsStreamUtils.h"

#define NS_MULTIPLEXINPUTSTREAM_CONTRACTID \
  "@mozilla.org/io/multiplex-input-stream;1"
#define NS_MULTIPLEXINPUTSTREAM_CID           \
  {/* 565e3a2c-1dd2-11b2-8da1-b4cef17e568d */ \
   0x565e3a2c,                                \
   0x1dd2,                                    \
   0x11b2,                                    \
   {0x8d, 0xa1, 0xb4, 0xce, 0xf1, 0x7e, 0x56, 0x8d}}

extern nsresult nsMultiplexInputStreamConstructor(REFNSIID aIID,
                                                  void** aResult);

namespace mozilla {
class nsMultiplexInputStream final : public nsIMultiplexInputStream,
                                     public nsISeekableStream,
                                     public nsIIPCSerializableInputStream,
                                     public nsICloneableInputStream,
                                     public nsIAsyncInputStream,
                                     public nsIInputStreamCallback,
                                     public nsIInputStreamLength,
                                     public nsIAsyncInputStreamLength {
 public:
  nsMultiplexInputStream();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIMULTIPLEXINPUTSTREAM
  NS_DECL_NSISEEKABLESTREAM
  NS_DECL_NSITELLABLESTREAM
  NS_DECL_NSIIPCSERIALIZABLEINPUTSTREAM
  NS_DECL_NSICLONEABLEINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSIINPUTSTREAMLENGTH
  NS_DECL_NSIASYNCINPUTSTREAMLENGTH

  // This is used for nsIAsyncInputStream::AsyncWait
  void AsyncWaitCompleted();

  // This is used for nsIAsyncInputStreamLength::AsyncLengthWait
  void AsyncWaitCompleted(int64_t aLength, const MutexAutoLock& aProofOfLock)
      MOZ_REQUIRES(mLock);

  struct StreamData {
    nsresult Initialize(nsIInputStream* aOriginalStream) {
      mCurrentPos = 0;

      mOriginalStream = aOriginalStream;

      mBufferedStream = aOriginalStream;
      if (!NS_InputStreamIsBuffered(mBufferedStream)) {
        nsCOMPtr<nsIInputStream> bufferedStream;
        nsresult rv = NS_NewBufferedInputStream(getter_AddRefs(bufferedStream),
                                                mBufferedStream.forget(), 4096);
        NS_ENSURE_SUCCESS(rv, rv);
        mBufferedStream = bufferedStream;
      }

      mAsyncStream = do_QueryInterface(mBufferedStream);
      mSeekableStream = do_QueryInterface(mBufferedStream);

      return NS_OK;
    }

    nsCOMPtr<nsIInputStream> mOriginalStream;

    // Equal to mOriginalStream or a wrap around the original stream to make it
    // buffered.
    nsCOMPtr<nsIInputStream> mBufferedStream;

    // This can be null.
    nsCOMPtr<nsIAsyncInputStream> mAsyncStream;
    // This can be null.
    nsCOMPtr<nsISeekableStream> mSeekableStream;

    uint64_t mCurrentPos;
  };

  Mutex& GetLock() MOZ_RETURN_CAPABILITY(mLock) { return mLock; }

 private:
  ~nsMultiplexInputStream() = default;

  void NextStream() MOZ_REQUIRES(mLock) {
    ++mCurrentStream;
    mStartedReadingCurrent = false;
  }

  nsresult AsyncWaitInternal();

  // This method updates mSeekableStreams, mTellableStreams,
  // mIPCSerializableStreams and mCloneableStreams values.
  void UpdateQIMap(StreamData& aStream) MOZ_REQUIRES(mLock);

  struct MOZ_STACK_CLASS ReadSegmentsState {
    nsCOMPtr<nsIInputStream> mThisStream;
    uint32_t mOffset;
    nsWriteSegmentFun mWriter;
    void* mClosure;
    bool mDone;
  };

  void SerializedComplexityInternal(uint32_t aMaxSize, uint32_t* aSizeUsed,
                                    uint32_t* aPipes, uint32_t* aTransferables,
                                    bool* aSerializeAsPipe);

  static nsresult ReadSegCb(nsIInputStream* aIn, void* aClosure,
                            const char* aFromRawSegment, uint32_t aToOffset,
                            uint32_t aCount, uint32_t* aWriteCount);

  bool IsSeekable() const;
  bool IsIPCSerializable() const;
  bool IsCloneable() const;
  bool IsAsyncInputStream() const;
  bool IsInputStreamLength() const;
  bool IsAsyncInputStreamLength() const;

  Mutex mLock;  // Protects access to all data members.

  nsTArray<StreamData> mStreams MOZ_GUARDED_BY(mLock);

  uint32_t mCurrentStream MOZ_GUARDED_BY(mLock);
  bool mStartedReadingCurrent MOZ_GUARDED_BY(mLock);
  nsresult mStatus MOZ_GUARDED_BY(mLock);
  nsCOMPtr<nsIInputStreamCallback> mAsyncWaitCallback MOZ_GUARDED_BY(mLock);
  uint32_t mAsyncWaitFlags MOZ_GUARDED_BY(mLock);
  uint32_t mAsyncWaitRequestedCount MOZ_GUARDED_BY(mLock);
  nsCOMPtr<nsIEventTarget> mAsyncWaitEventTarget MOZ_GUARDED_BY(mLock);
  nsCOMPtr<nsIInputStreamLengthCallback> mAsyncWaitLengthCallback
      MOZ_GUARDED_BY(mLock);

  class AsyncWaitLengthHelper;
  RefPtr<AsyncWaitLengthHelper> mAsyncWaitLengthHelper MOZ_GUARDED_BY(mLock);

  uint32_t mSeekableStreams MOZ_GUARDED_BY(mLock);
  uint32_t mIPCSerializableStreams MOZ_GUARDED_BY(mLock);
  uint32_t mCloneableStreams MOZ_GUARDED_BY(mLock);

  // These are Atomics so that we can check them in QueryInterface without
  // taking a lock (to look at mStreams.Length() and the numbers above)
  // With no streams added yet, all of these are possible
  Atomic<bool, Relaxed> mIsSeekableStream{true};
  Atomic<bool, Relaxed> mIsIPCSerializableStream{true};
  Atomic<bool, Relaxed> mIsCloneableStream{true};

  Atomic<bool, Relaxed> mIsAsyncInputStream{false};
  Atomic<bool, Relaxed> mIsInputStreamLength{false};
  Atomic<bool, Relaxed> mIsAsyncInputStreamLength{false};
};

}  // namespace mozilla

#endif  //  _nsMultiplexInputStream_h_
