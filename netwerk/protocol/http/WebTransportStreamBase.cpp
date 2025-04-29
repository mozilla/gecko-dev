/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebTransportStreamBase.h"

#include "nsIPipe.h"
#include "nsIOService.h"
#include "nsSocketTransportService2.h"

namespace mozilla::net {

WebTransportStreamBase::WebTransportStreamBase(
    uint64_t aSessionId,
    std::function<void(Result<RefPtr<WebTransportStreamBase>, nsresult>&&)>&&
        aCallback)
    : mSessionId(aSessionId), mStreamReadyCallback(std::move(aCallback)) {}

WebTransportStreamBase::~WebTransportStreamBase() = default;

nsresult WebTransportStreamBase::InitOutputPipe() {
  nsCOMPtr<nsIAsyncOutputStream> out;
  nsCOMPtr<nsIAsyncInputStream> in;
  NS_NewPipe2(getter_AddRefs(in), getter_AddRefs(out), true, true,
              nsIOService::gDefaultSegmentSize,
              nsIOService::gDefaultSegmentCount);

  {
    MutexAutoLock lock(mMutex);
    mSendStreamPipeIn = std::move(in);
    mSendStreamPipeOut = std::move(out);
  }

  nsresult rv =
      mSendStreamPipeIn->AsyncWait(this, 0, 0, gSocketTransportService);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mSendState = WAITING_DATA;
  return NS_OK;
}

nsresult WebTransportStreamBase::InitInputPipe() {
  nsCOMPtr<nsIAsyncOutputStream> out;
  nsCOMPtr<nsIAsyncInputStream> in;
  NS_NewPipe2(getter_AddRefs(in), getter_AddRefs(out), true, true,
              nsIOService::gDefaultSegmentSize,
              nsIOService::gDefaultSegmentCount);

  {
    MutexAutoLock lock(mMutex);
    mReceiveStreamPipeIn = std::move(in);
    mReceiveStreamPipeOut = std::move(out);
  }

  mRecvState = READING;
  return NS_OK;
}

void WebTransportStreamBase::GetWriterAndReader(
    nsIAsyncOutputStream** aOutOutputStream,
    nsIAsyncInputStream** aOutInputStream) {
  nsCOMPtr<nsIAsyncOutputStream> output;
  nsCOMPtr<nsIAsyncInputStream> input;
  {
    MutexAutoLock lock(mMutex);
    output = mSendStreamPipeOut;
    input = mReceiveStreamPipeIn;
  }

  output.forget(aOutOutputStream);
  input.forget(aOutInputStream);
}

}  // namespace mozilla::net
