/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RtspController.h"
#include "RtspMetaData.h"
#include "nsIURI.h"
#include "nsICryptoHash.h"
#include "nsIRunnable.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsICancelable.h"
#include "nsIStreamConverterService.h"
#include "nsIIOService2.h"
#include "nsIProtocolProxyService.h"
#include "nsIProxyInfo.h"
#include "nsIProxiedChannel.h"

#include "nsAutoPtr.h"
#include "nsStandardURL.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsXPIDLString.h"
#include "nsCRT.h"
#include "nsThreadUtils.h"
#include "nsError.h"
#include "nsStringStream.h"
#include "nsAlgorithm.h"
#include "nsProxyRelease.h"
#include "nsNetUtil.h"
#include "mozilla/Attributes.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "prlog.h"

#include "plbase64.h"
#include "prmem.h"
#include "prnetdb.h"
#include "zlib.h"
#include <algorithm>
#include "nsDebug.h"

extern PRLogModuleInfo* gRtspLog;
#undef LOG
#define LOG(args) PR_LOG(gRtspLog, PR_LOG_DEBUG, args)

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(RtspController,
                  nsIStreamingProtocolController)

RtspController::RtspController(nsIChannel *channel)
  : mState(INIT)
{
  LOG(("RtspController::RtspController()"));
}

RtspController::~RtspController()
{
  LOG(("RtspController::~RtspController()"));
  if (mRtspSource.get()) {
    mRtspSource.clear();
  }
}

NS_IMETHODIMP
RtspController::GetTrackMetaData(uint8_t index,
                                 nsIStreamingProtocolMetaData * *_retval)
{
  LOG(("RtspController::GetTrackMetaData()"));
  return NS_OK;
}

NS_IMETHODIMP
RtspController::Play(void)
{
  LOG(("RtspController::Play()"));
  if (!mRtspSource.get()) {
    MOZ_ASSERT(mRtspSource.get(), "mRtspSource should not be null!");
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (mState != CONNECTED) {
    return NS_ERROR_NOT_CONNECTED;
  }

  mRtspSource->play();
  return NS_OK;
}

NS_IMETHODIMP
RtspController::Pause(void)
{
  LOG(("RtspController::Pause()"));
  if (!mRtspSource.get()) {
    MOZ_ASSERT(mRtspSource.get(), "mRtspSource should not be null!");
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (mState != CONNECTED) {
    return NS_ERROR_NOT_CONNECTED;
  }

  mRtspSource->pause();
  return NS_OK;
}

NS_IMETHODIMP
RtspController::Resume(void)
{
  LOG(("RtspController::Resume()"));
  if (!mRtspSource.get()) {
    MOZ_ASSERT(mRtspSource.get(), "mRtspSource should not be null!");
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (mState != CONNECTED) {
    return NS_ERROR_NOT_CONNECTED;
  }

  mRtspSource->play();
  return NS_OK;
}

NS_IMETHODIMP
RtspController::Suspend(void)
{
  LOG(("RtspController::Suspend()"));
  if (!mRtspSource.get()) {
    MOZ_ASSERT(mRtspSource.get(), "mRtspSource should not be null!");
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (mState != CONNECTED) {
    return NS_ERROR_NOT_CONNECTED;
  }

  mRtspSource->pause();
  return NS_OK;
}

NS_IMETHODIMP
RtspController::Seek(uint64_t seekTimeUs)
{
  LOG(("RtspController::Seek() %llu", seekTimeUs));
  if (!mRtspSource.get()) {
    MOZ_ASSERT(mRtspSource.get(), "mRtspSource should not be null!");
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (mState != CONNECTED) {
    return NS_ERROR_NOT_CONNECTED;
  }

  mRtspSource->seek(seekTimeUs);
  return NS_OK;
}

NS_IMETHODIMP
RtspController::Stop()
{
  LOG(("RtspController::Stop()"));
  mState = INIT;
  if (!mRtspSource.get()) {
    MOZ_ASSERT(mRtspSource.get(), "mRtspSource should not be null!");
    return NS_ERROR_NOT_INITIALIZED;
  }

  mRtspSource->stop();
  return NS_OK;
}

NS_IMETHODIMP
RtspController::GetTotalTracks(uint8_t *aTracks)
{
  LOG(("RtspController::GetTotalTracks()"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RtspController::AsyncOpen(nsIStreamingProtocolListener *aListener)
{
  if (!aListener) {
    LOG(("RtspController::AsyncOpen() illegal listener"));
    return NS_ERROR_NOT_INITIALIZED;
  }

  mListener = aListener;

  if (!mURI) {
    LOG(("RtspController::AsyncOpen() illegal URI"));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsAutoCString uriSpec;
  mURI->GetSpec(uriSpec);
  LOG(("RtspController AsyncOpen uri=%s", uriSpec.get()));

  if (!mRtspSource.get()) {
    mRtspSource = new android::RTSPSource(this, uriSpec.get(), false, 0);
  }
  // Connect to Rtsp Server.
  mRtspSource->start();

  return NS_OK;
}

class SendMediaDataTask : public nsRunnable
{
public:
  SendMediaDataTask(nsIStreamingProtocolListener *listener,
                    uint8_t index,
                    const nsACString & data,
                    uint32_t length,
                    uint32_t offset,
                    nsIStreamingProtocolMetaData *meta)
    : mIndex(index)
    , mLength(length)
    , mOffset(offset)
    , mMetaData(meta)
    , mListener(listener)
  {
    mData.Assign(data);
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    mListener->OnMediaDataAvailable(mIndex, mData, mLength,
                                    mOffset, mMetaData);
    return NS_OK;
  }

private:
  uint8_t mIndex;
  nsCString mData;
  uint32_t mLength;
  uint32_t mOffset;
  nsRefPtr<nsIStreamingProtocolMetaData> mMetaData;
  nsCOMPtr<nsIStreamingProtocolListener> mListener;
};

NS_IMETHODIMP
RtspController::OnMediaDataAvailable(uint8_t index,
                                     const nsACString & data,
                                     uint32_t length,
                                     uint32_t offset,
                                     nsIStreamingProtocolMetaData *meta)
{
  if (mListener && mState == CONNECTED) {
    nsRefPtr<SendMediaDataTask> task =
      new SendMediaDataTask(mListener, index, data, length, offset, meta);
    return NS_DispatchToMainThread(task);
  }
  return NS_ERROR_NOT_AVAILABLE;
}

class SendOnConnectedTask : public nsRunnable
{
public:
  SendOnConnectedTask(nsIStreamingProtocolListener *listener,
                      uint8_t index,
                      nsIStreamingProtocolMetaData *meta)
    : mListener(listener)
    , mIndex(index)
    , mMetaData(meta)
  { }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    mListener->OnConnected(mIndex, mMetaData);
    return NS_OK;
  }

private:
  nsCOMPtr<nsIStreamingProtocolListener> mListener;
  uint8_t mIndex;
  nsRefPtr<nsIStreamingProtocolMetaData> mMetaData;
};


NS_IMETHODIMP
RtspController::OnConnected(uint8_t index,
                            nsIStreamingProtocolMetaData *meta)
{
  LOG(("RtspController::OnConnected()"));
  mState = CONNECTED;
  if (mListener) {
    nsRefPtr<SendOnConnectedTask> task =
      new SendOnConnectedTask(mListener, index, meta);
    return NS_DispatchToMainThread(task);
  }
  return NS_ERROR_NOT_AVAILABLE;
}

class SendOnDisconnectedTask : public nsRunnable
{
public:
  SendOnDisconnectedTask(nsIStreamingProtocolListener *listener,
                         uint8_t index,
                         nsresult reason)
    : mListener(listener)
    , mIndex(index)
    , mReason(reason)
  { }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    mListener->OnDisconnected(mIndex, mReason);
    return NS_OK;
  }

private:
  nsCOMPtr<nsIStreamingProtocolListener> mListener;
  uint8_t mIndex;
  nsresult mReason;
};

NS_IMETHODIMP
RtspController::OnDisconnected(uint8_t index,
                               nsresult reason)
{
  LOG(("RtspController::OnDisconnected() for track %d reason = 0x%x", index, reason));
  mState = DISCONNECTED;
  if (mListener) {
    nsRefPtr<SendOnDisconnectedTask> task =
      new SendOnDisconnectedTask(mListener, index, reason);
    // Break the cycle reference between the Listener (RtspControllerParent) and
    // us.
    mListener = nullptr;
    return NS_DispatchToMainThread(task);
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
RtspController::Init(nsIURI *aURI)
{
  nsresult rv;

  if (!aURI) {
    LOG(("RtspController::Init() - invalid URI"));
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsAutoCString host;
  int32_t port = -1;

  rv = aURI->GetAsciiHost(host);
  if (NS_FAILED(rv)) return rv;

  // Reject the URL if it doesn't specify a host
  if (host.IsEmpty())
    return NS_ERROR_MALFORMED_URI;

  rv = aURI->GetPort(&port);
  if (NS_FAILED(rv)) return rv;

  rv = aURI->GetAsciiSpec(mSpec);
  if (NS_FAILED(rv)) return rv;

  mURI = aURI;

  return NS_OK;
}

NS_IMETHODIMP
RtspController::PlaybackEnded()
{
  LOG(("RtspController::PlaybackEnded()"));
  if (!mRtspSource.get()) {
    MOZ_ASSERT(mRtspSource.get(), "mRtspSource should not be null!");
    return NS_ERROR_NOT_INITIALIZED;
  }

  mRtspSource->playbackEnded();
  return NS_OK;
}

} // namespace mozilla::net
} // namespace mozilla
