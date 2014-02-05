/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "RtspMediaResource.h"

#include "MediaDecoder.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/Monitor.h"
#include "mozilla/Preferences.h"
#include "nsIScriptSecurityManager.h"
#include "nsIStreamingProtocolService.h"
#include "nsServiceManagerUtils.h"

#ifdef PR_LOGGING
PRLogModuleInfo* gRtspMediaResourceLog;
#define RTSP_LOG(msg, ...) PR_LOG(gRtspMediaResourceLog, PR_LOG_DEBUG, \
                                  (msg, ##__VA_ARGS__))
// Debug logging macro with object pointer and class name.
#define RTSPMLOG(msg, ...) \
        RTSP_LOG("%p [RtspMediaResource]: " msg, this, ##__VA_ARGS__)
#else
#define RTSP_LOG(msg, ...)
#define RTSPMLOG(msg, ...)
#endif

namespace mozilla {

/* class RtspTrackBuffer: a ring buffer implementation for audio/video track
 * un-decoded data.
 * The ring buffer is divided into BUFFER_SLOT_NUM slots,
 * and each slot's size is fixed(mSlotSize).
 * Even though the ring buffer is divided into fixed size slots, it still can
 * store the data which size is larger than one slot size.
 * */
#define BUFFER_SLOT_NUM 8192
#define BUFFER_SLOT_DEFAULT_SIZE 256
#define BUFFER_SLOT_MAX_SIZE 8192
#define BUFFER_SLOT_INVALID -1
#define BUFFER_SLOT_EMPTY 0

struct BufferSlotData {
  int32_t mLength;
  uint64_t mTime;
};

class RtspTrackBuffer
{
public:
  RtspTrackBuffer(const char *aMonitor, int32_t aTrackIdx, uint32_t aSlotSize)
  : mMonitor(aMonitor)
  , mSlotSize(aSlotSize)
  , mTotalBufferSize(BUFFER_SLOT_NUM * mSlotSize)
  , mFrameType(0)
  , mIsStarted(false) {
    MOZ_COUNT_CTOR(RtspTrackBuffer);
#ifdef PR_LOGGING
    mTrackIdx = aTrackIdx;
#endif
    MOZ_ASSERT(mSlotSize < UINT32_MAX / BUFFER_SLOT_NUM);
    mRingBuffer = new uint8_t[mTotalBufferSize];
    Reset();
  };
  ~RtspTrackBuffer() {
    MOZ_COUNT_DTOR(RtspTrackBuffer);
    mRingBuffer = nullptr;
  };
  void Start() {
    MonitorAutoLock monitor(mMonitor);
    mIsStarted = true;
    mFrameType = 0;
  }
  void Stop() {
    MonitorAutoLock monitor(mMonitor);
    mIsStarted = false;
  }

  // Read the data from mRingBuffer[mConsumerIdx*mSlotSize] into aToBuffer.
  // If the aToBufferSize is smaller than mBufferSlotDataLength[mConsumerIdx],
  // early return and set the aFrameSize to notify the reader the aToBuffer
  // doesn't have enough space. The reader must realloc the aToBuffer if it
  // wishes to read the data.
  nsresult ReadBuffer(uint8_t* aToBuffer, uint32_t aToBufferSize,
                      uint32_t& aReadCount, uint64_t& aFrameTime,
                      uint32_t& aFrameSize);
  // Write the data from aFromBuffer into mRingBuffer[mProducerIdx*mSlotSize].
  void WriteBuffer(const char *aFromBuffer, uint32_t aWriteCount,
                   uint64_t aFrameTime, uint32_t aFrameType);
  // Reset the mProducerIdx, mConsumerIdx, mBufferSlotDataLength[],
  // mBufferSlotDataTime[].
  void Reset();

  // We should call SetFrameType first then reset().
  // If we call reset() first, the queue may still has some "garbage" frame
  // from another thread's |OnMediaDataAvailable| before |SetFrameType|.
  void ResetWithFrameType(uint32_t aFrameType) {
    SetFrameType(aFrameType);
    Reset();
  }

private:
  // The FrameType is sync to nsIStreamingProtocolController.h
  void SetFrameType(uint32_t aFrameType) {
    MonitorAutoLock monitor(mMonitor);
    mFrameType = mFrameType | aFrameType;
  }

  // A monitor lock to prevent racing condition.
  Monitor mMonitor;
#ifdef PR_LOGGING
  // Indicate the track number for Rtsp.
  int32_t mTrackIdx;
#endif
  // mProducerIdx: A slot index that we store data from
  // nsIStreamingProtocolController.
  // mConsumerIdx: A slot index that we read when decoder need(from OMX decoder).
  int32_t mProducerIdx;
  int32_t mConsumerIdx;

  // Because each slot's size is fixed, we need an array to record the real
  // data length and data time stamp.
  // The value in mBufferSlotData[index].mLength represents:
  // -1(BUFFER_SLOT_INVALID): The index of slot data is invalid, mConsumerIdx
  //                          should go forward.
  // 0(BUFFER_SLOT_EMPTY): The index slot is empty. mConsumerIdx should wait here.
  // positive value: The index slot contains valid data and the value is data size.
  BufferSlotData mBufferSlotData[BUFFER_SLOT_NUM];

  // The ring buffer pointer.
  nsAutoArrayPtr<uint8_t> mRingBuffer;
  // Each slot's size.
  uint32_t mSlotSize;
  // Total mRingBuffer's total size.
  uint32_t mTotalBufferSize;
  // A flag that that indicate the incoming data should be dropped or stored.
  // When we are seeking, the incoming data should be dropped.
  // Bit definition in |nsIStreamingProtocolController.h|
  uint32_t mFrameType;

  // Set true/false when |Start()/Stop()| is called.
  bool mIsStarted;
};

nsresult RtspTrackBuffer::ReadBuffer(uint8_t* aToBuffer, uint32_t aToBufferSize,
                                     uint32_t& aReadCount, uint64_t& aFrameTime,
                                     uint32_t& aFrameSize)
{
  MonitorAutoLock monitor(mMonitor);
  RTSPMLOG("ReadBuffer mTrackIdx %d mProducerIdx %d mConsumerIdx %d "
           "mBufferSlotData[mConsumerIdx].mLength %d"
           ,mTrackIdx ,mProducerIdx ,mConsumerIdx
           ,mBufferSlotData[mConsumerIdx].mLength);
  // Reader should skip the slots with mLength==BUFFER_SLOT_INVALID.
  // The loop ends when
  // 1. Read data successfully
  // 2. Fail to read data due to aToBuffer's space
  // 3. No data in this buffer
  // 4. mIsStarted is not set
  while (1) {
    if (mBufferSlotData[mConsumerIdx].mLength > 0) {
      // Check the aToBuffer space is enough for data copy.
      if ((int32_t)aToBufferSize < mBufferSlotData[mConsumerIdx].mLength) {
        aFrameSize = mBufferSlotData[mConsumerIdx].mLength;
        break;
      }
      uint32_t slots = (mBufferSlotData[mConsumerIdx].mLength / mSlotSize) + 1;
      // we have data, copy to aToBuffer
      MOZ_ASSERT(mBufferSlotData[mConsumerIdx].mLength <=
                 (int32_t)((BUFFER_SLOT_NUM - mConsumerIdx) * mSlotSize));
      memcpy(aToBuffer,
             (void *)(&mRingBuffer[mSlotSize * mConsumerIdx]),
             mBufferSlotData[mConsumerIdx].mLength);

      aFrameSize = aReadCount = mBufferSlotData[mConsumerIdx].mLength;
      aFrameTime = mBufferSlotData[mConsumerIdx].mTime;
      RTSPMLOG("DataLength %d, data time %lld"
               ,mBufferSlotData[mConsumerIdx].mLength
               ,mBufferSlotData[mConsumerIdx].mTime);
      // After reading the data, we set current index of mBufferSlotDataLength
      // to BUFFER_SLOT_EMPTY to indicate these slots are free.
      for (uint32_t i = mConsumerIdx; i < mConsumerIdx + slots; ++i) {
        mBufferSlotData[i].mLength = BUFFER_SLOT_EMPTY;
        mBufferSlotData[i].mTime = BUFFER_SLOT_EMPTY;
      }
      mConsumerIdx = (mConsumerIdx + slots) % BUFFER_SLOT_NUM;
      break;
    } else if (mBufferSlotData[mConsumerIdx].mLength == BUFFER_SLOT_INVALID) {
      mConsumerIdx = (mConsumerIdx + 1) % BUFFER_SLOT_NUM;
      RTSPMLOG("BUFFER_SLOT_INVALID move forward");
    } else {
      // No data, and disconnected.
      if (!mIsStarted) {
        return NS_ERROR_FAILURE;
      }
      // No data, the decode thread is blocked here until we receive
      // OnMediaDataAvailable. The OnMediaDataAvailable will call WriteBuffer()
      // to wake up the decode thread.
      RTSPMLOG("monitor.Wait()");
      monitor.Wait();
    }
  }
  return NS_OK;
}

/* When we perform a WriteBuffer, we check mIsStarted and aFrameType first.
 * These flags prevent "garbage" frames from being written into the buffer.
 *
 * After writing the data into the buffer, we check to see if we wrote over a
 * slot, and update mConsumerIdx if necessary.
 * This ensures that the decoder will get the "oldest" data available in the
 * buffer.
 *
 * If the incoming data is larger than one slot size (isMultipleSlots), we do
 * |mBufferSlotData[].mLength = BUFFER_SLOT_INVALID;| for other slots except the
 * first slot, in order to notify the reader that some slots are unavailable.
 *
 * If the incoming data is isMultipleSlots and crosses the end of
 * BUFFER_SLOT_NUM, returnToHead is set to true and the data will continue to
 * be written from head(index 0).
 *
 * MEDIASTREAM_FRAMETYPE_DISCONTINUITY currently is used when we are seeking.
 * */
void RtspTrackBuffer::WriteBuffer(const char *aFromBuffer, uint32_t aWriteCount,
                                  uint64_t aFrameTime, uint32_t aFrameType)
{
  MonitorAutoLock monitor(mMonitor);
  if (!mIsStarted) {
    RTSPMLOG("mIsStarted is false");
    return;
  }
  if (mTotalBufferSize < aWriteCount) {
    RTSPMLOG("mTotalBufferSize < aWriteCount, incoming data is too large");
    return;
  }
  // Checking the incoming data's frame type.
  // If we receive MEDIASTREAM_FRAMETYPE_DISCONTINUITY, clear the mFrameType
  // imply the RtspTrackBuffer is ready for receive data.
  if (aFrameType & MEDIASTREAM_FRAMETYPE_DISCONTINUITY) {
    mFrameType = mFrameType & (~MEDIASTREAM_FRAMETYPE_DISCONTINUITY);
    RTSPMLOG("Clear mFrameType");
    return;
  }
  // Checking current buffer frame type.
  // If the MEDIASTREAM_FRAMETYPE_DISCONTINUNITY bit is set, imply the
  // RtspTrackBuffer can't receive data now. So we drop the frame until we
  // receive MEDIASTREAM_FRAMETYPE_DISCONTINUNITY.
  if (mFrameType & MEDIASTREAM_FRAMETYPE_DISCONTINUITY) {
    RTSPMLOG("Return because the mFrameType is set");
    return;
  }
  // The flag is true if the incoming data is larger than one slot size.
  bool isMultipleSlots = false;
  // The flag is true if the incoming data is larger than remainder free slots
  bool returnToHead = false;
  // Calculate how many slots the incoming data needed.
  int32_t slots = 1;
  int32_t i;
  RTSPMLOG("WriteBuffer mTrackIdx %d mProducerIdx %d mConsumerIdx %d",
           mTrackIdx, mProducerIdx,mConsumerIdx);
  if (aWriteCount > mSlotSize) {
    isMultipleSlots = true;
    slots = (aWriteCount / mSlotSize) + 1;
  }
  if (isMultipleSlots &&
      (aWriteCount > (BUFFER_SLOT_NUM - mProducerIdx) * mSlotSize)) {
    returnToHead = true;
  }
  RTSPMLOG("slots %d isMultipleSlots %d returnToHead %d",
           slots, isMultipleSlots, returnToHead);
  if (returnToHead) {
    // Clear the rest index of mBufferSlotData[].mLength
    for (i = mProducerIdx; i < BUFFER_SLOT_NUM; ++i) {
      mBufferSlotData[i].mLength = BUFFER_SLOT_INVALID;
    }
    // We wrote one or more slots that the decode thread has not yet read.
    // So the mConsumerIdx returns to the head of slot buffer and moves forward
    // to the oldest slot.
    if (mProducerIdx <= mConsumerIdx && mConsumerIdx < mProducerIdx + slots) {
      mConsumerIdx = 0;
      for (i = mConsumerIdx; i < BUFFER_SLOT_NUM; ++i) {
        if (mBufferSlotData[i].mLength > 0) {
          mConsumerIdx = i;
          break;
        }
      }
    }
    mProducerIdx = 0;
  }

  memcpy(&(mRingBuffer[mSlotSize * mProducerIdx]), aFromBuffer, aWriteCount);

  if (mProducerIdx <= mConsumerIdx && mConsumerIdx < mProducerIdx + slots
      && mBufferSlotData[mConsumerIdx].mLength > 0) {
    // Wrote one or more slots that the decode thread has not yet read.
    RTSPMLOG("overwrite!! %d time %lld"
             ,mTrackIdx,mBufferSlotData[mConsumerIdx].mTime);
    mBufferSlotData[mProducerIdx].mLength = aWriteCount;
    mBufferSlotData[mProducerIdx].mTime = aFrameTime;
    // Clear the mBufferSlotDataLength except the start slot.
    if (isMultipleSlots) {
      for (i = mProducerIdx + 1; i < mProducerIdx + slots; ++i) {
        mBufferSlotData[i].mLength = BUFFER_SLOT_INVALID;
      }
    }
    mProducerIdx = (mProducerIdx + slots) % BUFFER_SLOT_NUM;
    // Move the mConsumerIdx forward to ensure that the decoder reads the
    // oldest data available.
    mConsumerIdx = mProducerIdx;
  } else {
    // Normal case, the writer doesn't take over the reader.
    mBufferSlotData[mProducerIdx].mLength = aWriteCount;
    mBufferSlotData[mProducerIdx].mTime = aFrameTime;
    // Clear the mBufferSlotData[].mLength except the start slot.
    if (isMultipleSlots) {
      for (i = mProducerIdx + 1; i < mProducerIdx + slots; ++i) {
        mBufferSlotData[i].mLength = BUFFER_SLOT_INVALID;
      }
    }
    mProducerIdx = (mProducerIdx + slots) % BUFFER_SLOT_NUM;
  }

  mMonitor.NotifyAll();
}

void RtspTrackBuffer::Reset() {
  MonitorAutoLock monitor(mMonitor);
  mProducerIdx = 0;
  mConsumerIdx = 0;
  for (uint32_t i = 0; i < BUFFER_SLOT_NUM; ++i) {
    mBufferSlotData[i].mLength = BUFFER_SLOT_EMPTY;
    mBufferSlotData[i].mTime = BUFFER_SLOT_EMPTY;
  }
  mMonitor.NotifyAll();
}

RtspMediaResource::RtspMediaResource(MediaDecoder* aDecoder,
    nsIChannel* aChannel, nsIURI* aURI, const nsACString& aContentType)
  : BaseMediaResource(aDecoder, aChannel, aURI, aContentType)
  , mIsConnected(false)
  , mRealTime(false)
{
  nsCOMPtr<nsIStreamingProtocolControllerService> mediaControllerService =
    do_GetService(MEDIASTREAMCONTROLLERSERVICE_CONTRACTID);
  MOZ_ASSERT(mediaControllerService);
  if (mediaControllerService) {
    mediaControllerService->Create(mChannel,
                                   getter_AddRefs(mMediaStreamController));
    MOZ_ASSERT(mMediaStreamController);
    mListener = new Listener(this);
    mMediaStreamController->AsyncOpen(mListener);
  }
#ifdef PR_LOGGING
  if (!gRtspMediaResourceLog) {
    gRtspMediaResourceLog = PR_NewLogModule("RtspMediaResource");
  }
#endif
}

RtspMediaResource::~RtspMediaResource()
{
  RTSPMLOG("~RtspMediaResource");
  if (mListener) {
    // Kill its reference to us since we're going away
    mListener->Revoke();
  }
}

NS_IMPL_ISUPPORTS2(RtspMediaResource::Listener,
                   nsIInterfaceRequestor, nsIStreamingProtocolListener);

nsresult
RtspMediaResource::Listener::OnMediaDataAvailable(uint8_t aTrackIdx,
                                                  const nsACString &data,
                                                  uint32_t length,
                                                  uint32_t offset,
                                                  nsIStreamingProtocolMetaData *meta)
{
  if (!mResource)
    return NS_OK;
  return mResource->OnMediaDataAvailable(aTrackIdx, data, length, offset, meta);
}

nsresult
RtspMediaResource::Listener::OnConnected(uint8_t aTrackIdx,
                                         nsIStreamingProtocolMetaData *meta)
{
  if (!mResource)
    return NS_OK;
  return mResource->OnConnected(aTrackIdx, meta);
}

nsresult
RtspMediaResource::Listener::OnDisconnected(uint8_t aTrackIdx, nsresult reason)
{
  if (!mResource)
    return NS_OK;
  return mResource->OnDisconnected(aTrackIdx, reason);
}

nsresult
RtspMediaResource::Listener::GetInterface(const nsIID & aIID, void **aResult)
{
  return QueryInterface(aIID, aResult);
}

nsresult
RtspMediaResource::ReadFrameFromTrack(uint8_t* aBuffer, uint32_t aBufferSize,
                                      uint32_t aTrackIdx, uint32_t& aBytes,
                                      uint64_t& aTime, uint32_t& aFrameSize)
{
  NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");
  NS_ASSERTION(aTrackIdx < mTrackBuffer.Length(),
               "ReadTrack index > mTrackBuffer");
  MOZ_ASSERT(aBuffer);

  return mTrackBuffer[aTrackIdx]->ReadBuffer(aBuffer, aBufferSize, aBytes,
                                             aTime, aFrameSize);
}

nsresult
RtspMediaResource::OnMediaDataAvailable(uint8_t aTrackIdx,
                                        const nsACString &data,
                                        uint32_t length,
                                        uint32_t offset,
                                        nsIStreamingProtocolMetaData *meta)
{
  uint64_t time;
  uint32_t frameType;
  meta->GetTimeStamp(&time);
  meta->GetFrameType(&frameType);
  if (mRealTime) {
    time = 0;
  }
  mTrackBuffer[aTrackIdx]->WriteBuffer(data.BeginReading(), length, time,
                                       frameType);
  return NS_OK;
}

// Bug 962309 - Video RTSP support should be disabled in 1.3
bool
RtspMediaResource::IsVideoEnabled()
{
  return Preferences::GetBool("media.rtsp.video.enabled", false);
}

bool
RtspMediaResource::IsVideo(uint8_t tracks, nsIStreamingProtocolMetaData *meta)
{
  bool isVideo = false;
  for (int i = 0; i < tracks; ++i) {
    nsCOMPtr<nsIStreamingProtocolMetaData> trackMeta;
    mMediaStreamController->GetTrackMetaData(i, getter_AddRefs(trackMeta));
    MOZ_ASSERT(trackMeta);
    uint32_t w = 0, h = 0;
    trackMeta->GetWidth(&w);
    trackMeta->GetHeight(&h);
    if (w > 0 || h > 0) {
      isVideo = true;
      break;
    }
  }
  return isVideo;
}

nsresult
RtspMediaResource::OnConnected(uint8_t aTrackIdx,
                               nsIStreamingProtocolMetaData *meta)
{
  if (mIsConnected) {
    for (uint32_t i = 0 ; i < mTrackBuffer.Length(); ++i) {
      mTrackBuffer[i]->Start();
    }
    return NS_OK;
  }

  uint8_t tracks;
  mMediaStreamController->GetTotalTracks(&tracks);

  // If the preference of RTSP video feature is not enabled and the streaming is
  // video, we give up moving forward.
  if (!IsVideoEnabled() && IsVideo(tracks, meta)) {
    // Give up, report error to media element.
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(mDecoder, &MediaDecoder::DecodeError);
    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
    return NS_ERROR_FAILURE;
  }
  uint64_t duration = 0;
  for (int i = 0; i < tracks; ++i) {
    nsCString rtspTrackId("RtspTrack");
    rtspTrackId.AppendInt(i);
    nsCOMPtr<nsIStreamingProtocolMetaData> trackMeta;
    mMediaStreamController->GetTrackMetaData(i, getter_AddRefs(trackMeta));
    MOZ_ASSERT(trackMeta);
    trackMeta->GetDuration(&duration);

    // Here is a heuristic to estimate the slot size.
    // For video track, calculate the width*height.
    // For audio track, use the BUFFER_SLOT_DEFAULT_SIZE because the w*h is 0.
    // Finally clamp them into (BUFFER_SLOT_DEFAULT_SIZE,BUFFER_SLOT_MAX_SIZE)
    uint32_t w, h;
    uint32_t slotSize;
    trackMeta->GetWidth(&w);
    trackMeta->GetHeight(&h);
    slotSize = clamped((int32_t)(w * h), BUFFER_SLOT_DEFAULT_SIZE,
                       BUFFER_SLOT_MAX_SIZE);
    mTrackBuffer.AppendElement(new RtspTrackBuffer(rtspTrackId.get(),
                                                   i, slotSize));
    mTrackBuffer[i]->Start();
  }

  if (!mDecoder) {
    return NS_ERROR_FAILURE;
  }

  // If the duration is 0, imply the stream is live stream.
  if (duration) {
    // Not live stream.
    mRealTime = false;
    bool seekable = true;
    mDecoder->SetInfinite(false);
    mDecoder->SetTransportSeekable(seekable);
    mDecoder->SetDuration(duration);
  } else {
    // Live stream.
    // Check the preference "media.realtime_decoder.enabled".
    if (!Preferences::GetBool("media.realtime_decoder.enabled", false)) {
      // Give up, report error to media element.
      nsCOMPtr<nsIRunnable> event =
        NS_NewRunnableMethod(mDecoder, &MediaDecoder::DecodeError);
      NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
      return NS_ERROR_FAILURE;
    } else {
      mRealTime = true;
      bool seekable = false;
      mDecoder->SetInfinite(true);
      mDecoder->SetTransportSeekable(seekable);
      mDecoder->SetMediaSeekable(seekable);
    }
  }
  // Fires an initial progress event and sets up the stall counter so stall events
  // fire if no download occurs within the required time frame.
  mDecoder->Progress(false);

  MediaDecoderOwner* owner = mDecoder->GetMediaOwner();
  NS_ENSURE_TRUE(owner, NS_ERROR_FAILURE);
  dom::HTMLMediaElement* element = owner->GetMediaElement();
  NS_ENSURE_TRUE(element, NS_ERROR_FAILURE);

  element->FinishDecoderSetup(mDecoder, this);
  mIsConnected = true;

  return NS_OK;
}

nsresult
RtspMediaResource::OnDisconnected(uint8_t aTrackIdx, nsresult aReason)
{
  NS_ASSERTION(NS_IsMainThread(), "Don't call on non-main thread");

  for (uint32_t i = 0 ; i < mTrackBuffer.Length(); ++i) {
    mTrackBuffer[i]->Stop();
    mTrackBuffer[i]->Reset();
  }

  // If mDecoder is null pointer, it means this OnDisconnected event is
  // triggered when media element was destroyed and mDecoder was already
  // shutdown.
  if (!mDecoder) {
    return NS_OK;
  }

  if (aReason == NS_ERROR_NOT_INITIALIZED ||
      aReason == NS_ERROR_CONNECTION_REFUSED ||
      aReason == NS_ERROR_NOT_CONNECTED ||
      aReason == NS_ERROR_NET_TIMEOUT) {
    RTSPMLOG("Error in OnDisconnected 0x%x", aReason);
    mDecoder->NetworkError();
    return NS_OK;
  }

  // Resetting the decoder and media element when the connection
  // between Rtsp client and server goes down.
  mDecoder->ResetConnectionState();
  return NS_OK;
}

void RtspMediaResource::Suspend(bool aCloseImmediately)
{
  NS_ASSERTION(NS_IsMainThread(), "Don't call on non-main thread");

  MediaDecoderOwner* owner = mDecoder->GetMediaOwner();
  NS_ENSURE_TRUE_VOID(owner);
  dom::HTMLMediaElement* element = owner->GetMediaElement();
  NS_ENSURE_TRUE_VOID(element);

  mMediaStreamController->Suspend();
  element->DownloadSuspended();
}

void RtspMediaResource::Resume()
{
  NS_ASSERTION(NS_IsMainThread(), "Don't call on non-main thread");

  MediaDecoderOwner* owner = mDecoder->GetMediaOwner();
  NS_ENSURE_TRUE_VOID(owner);
  dom::HTMLMediaElement* element = owner->GetMediaElement();
  NS_ENSURE_TRUE_VOID(element);

  if (mChannel) {
    element->DownloadResumed();
  }
  mMediaStreamController->Resume();
}

nsresult RtspMediaResource::Open(nsIStreamListener **aStreamListener)
{
  return NS_OK;
}

nsresult RtspMediaResource::Close()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  mMediaStreamController->Stop();
  // Since mDecoder is not an nsCOMPtr in BaseMediaResource, we have to
  // explicitly set it as null pointer in order to prevent misuse from this
  // object (RtspMediaResource).
  if (mDecoder) {
    mDecoder = nullptr;
  }
  return NS_OK;
}

already_AddRefed<nsIPrincipal> RtspMediaResource::GetCurrentPrincipal()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  nsCOMPtr<nsIPrincipal> principal;
  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  if (!secMan || !mChannel)
    return nullptr;
  secMan->GetChannelPrincipal(mChannel, getter_AddRefs(principal));
  return principal.forget();
}

nsresult RtspMediaResource::SeekTime(int64_t aOffset)
{
  NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");

  RTSPMLOG("Seek requested for aOffset [%lld] for decoder [%p]",
           aOffset, mDecoder);
  // Clear buffer and raise the frametype flag.
  for(uint32_t i = 0 ; i < mTrackBuffer.Length(); ++i) {
    mTrackBuffer[i]->ResetWithFrameType(MEDIASTREAM_FRAMETYPE_DISCONTINUITY);
  }

  return mMediaStreamController->Seek(aOffset);
}

} // namespace mozilla

