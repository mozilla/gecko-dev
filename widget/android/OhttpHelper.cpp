/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OhttpHelper.h"

#include "WebExecutorSupport.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/java/WebMessageWrappers.h"
#include "mozilla/Preferences.h"

#include "nsContentUtils.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIChannel.h"
#include "nsIChannelEventSink.h"
#include "nsIHttpChannel.h"
#include "nsIInputStream.h"
#include "nsIObliviousHttp.h"
#include "nsIStreamListener.h"
#include "nsIUploadChannel2.h"
#include "nsNetUtil.h"

namespace mozilla::widget {

class CallbackResponseListener : public nsIStreamListener,
                                 public nsIChannelEventSink {
 public:
  NS_DECL_ISUPPORTS

  CallbackResponseListener(
      nsIChannel* aChannel,
      std::function<void(nsresult, int64_t, const nsTArray<uint8_t>&)>
          aCallback)
      : mCallback(std::move(aCallback)), mChannel(aChannel) {}

  NS_IMETHODIMP OnStartRequest(nsIRequest* aRequest) override { return NS_OK; }

  NS_IMETHODIMP OnDataAvailable(nsIRequest* aRequest,
                                nsIInputStream* aInputStream, uint64_t aOffset,
                                uint32_t aCount) override {
    nsTArray<uint8_t> buffer(aCount);
    void* dest = buffer.Elements();
    nsresult rv = NS_ReadInputStreamToBuffer(aInputStream, &dest,
                                             static_cast<int64_t>(aCount));
    if (NS_FAILED(rv)) {
      mChannel->Cancel(NS_BINDING_ABORTED);
    }

    mBuffer.AppendElements(buffer.Elements(), aCount);
    return NS_OK;
  }

#define ENSURE_SUCCESS(rv)     \
  if (NS_FAILED(rv)) {         \
    mCallback(rv, 0, mBuffer); \
    return NS_OK;              \
  }

#define ENSURE_TRUE(instance)                \
  if (!(instance)) {                         \
    mCallback(NS_ERROR_FAILURE, 0, mBuffer); \
    return NS_OK;                            \
  }

  NS_IMETHODIMP OnStopRequest(nsIRequest* aRequest,
                              nsresult aStatusCode) override {
    auto defer = MakeScopeExit([&] { mChannel = nullptr; });
    ENSURE_SUCCESS(aStatusCode)

    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aRequest);
    ENSURE_TRUE(httpChannel);

    uint32_t status;
    nsresult rv = httpChannel->GetResponseStatus(&status);
    ENSURE_SUCCESS(rv)

    ENSURE_TRUE(status <= INT32_MAX);

    mCallback(NS_OK, static_cast<int32_t>(status), mBuffer);

    return NS_OK;
  }

#undef ENSURE_SUCCESS
#undef ENSURE_TRUE

  NS_IMETHODIMP AsyncOnChannelRedirect(
      nsIChannel* oldChannel, nsIChannel* newChannel, uint32_t flags,
      nsIAsyncVerifyRedirectCallback* callback) override {
    // We don't support redirects.
    callback->OnRedirectVerifyCallback(NS_ERROR_ABORT);
    return NS_OK;
  }

 private:
  virtual ~CallbackResponseListener() {}

  // nsresult, http status code, http headers, response data
  // only called from OnStopRequest
  std::function<void(nsresult, int64_t, const nsTArray<uint8_t>&)> mCallback;

  nsCOMPtr<nsIChannel> mChannel;
  nsTArray<uint8_t> mBuffer;
};

NS_IMPL_ISUPPORTS(CallbackResponseListener, nsIStreamListener,
                  nsIChannelEventSink);

nsresult OhttpHelper::EnsurePrefsRead() {
  if (sInitializationBitset & InitializationBit::PREFS) {
    return NS_OK;
  }

  nsresult rv = Preferences::GetCString("network.ohttp.configURL", sConfigUrl);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = Preferences::GetCString("network.ohttp.relayURL", sRelayUrl);
  NS_ENSURE_SUCCESS(rv, rv);

  sInitializationBitset |= PREFS;

  return NS_OK;
}

void OhttpHelper::QueueOhttpRequest(OhttpRequest* aRequest) {
  // Initialize the pending requests array if it hasn't been initialized yet.
  if (!(sInitializationBitset & InitializationBit::PENDING_REQUESTS)) {
    sPendingRequests = new nsTArray<RefPtr<OhttpRequest>>();
    ClearOnShutdown(&sPendingRequests);
    sInitializationBitset |= InitializationBit::PENDING_REQUESTS;
  }

  // Queue the request.
  sPendingRequests->AppendElement(aRequest);
}

nsresult OhttpHelper::CreateChannel(java::WebRequest::Param aRequest,
                                    nsIURI* aUri, nsIChannel** outChannel) {
  MOZ_ASSERT(sInitializationBitset & InitializationBit::CONFIG_FETCHED);

  const auto reqBase =
      java::WebMessage::LocalRef(aRequest.Cast<java::WebMessage>());

  nsCOMPtr<nsIObliviousHttpService> ohttpService(
      do_GetService("@mozilla.org/network/oblivious-http-service;1"));
  NS_ENSURE_TRUE(ohttpService, NS_ERROR_FAILURE);

  RefPtr<nsIURI> relayUri;
  nsresult rv = NS_NewURI(getter_AddRefs(relayUri), sRelayUrl);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> channel;
  rv = ohttpService->NewChannel(relayUri, aUri, *sConfigData,
                                getter_AddRefs(channel));
  NS_ENSURE_SUCCESS(rv, rv);

  channel.forget(outChannel);
  return NS_OK;
}

nsresult OhttpHelper::FetchConfigAndFulfillRequests() {
  if (sInitializationBitset & InitializationBit::CONFIG_FETCHING) {
    return NS_OK;
  }

  nsCOMPtr<nsIChannel> httpChannel;
  nsresult rv = CreateConfigRequest(getter_AddRefs(httpChannel));
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<CallbackResponseListener> listener = new CallbackResponseListener(
      httpChannel,
      [](nsresult rv, int64_t status, const nsTArray<uint8_t>& buffer) {
        sInitializationBitset &= ~InitializationBit::CONFIG_FETCHING;

        if (NS_FAILED(rv) || status != 200) {
          FailRequests(rv);
          return;
        }

        sConfigData = new nsTArray<uint8_t>(buffer.Length());
        ClearOnShutdown(&sConfigData);
        sConfigData->Assign(buffer);
        sInitializationBitset |= InitializationBit::CONFIG_FETCHED;

        for (auto& request : *sPendingRequests) {
          rv = WebExecutorSupport::CreateStreamLoader(
              request->request, request->flags, request->result);
          if (NS_FAILED(rv)) {
            WebExecutorSupport::CompleteWithError(request->result, rv);
          }
        }
        sPendingRequests->Clear();
      });
  rv = httpChannel->AsyncOpen(listener);
  NS_ENSURE_SUCCESS(rv, rv);

  sInitializationBitset |= InitializationBit::CONFIG_FETCHING;
  return NS_OK;
}

void OhttpHelper::FailRequests(nsresult aStatus) {
  for (auto& request : *sPendingRequests) {
    WebExecutorSupport::CompleteWithError(request->result, aStatus);
  }

  sPendingRequests->Clear();
}

nsresult OhttpHelper::CreateConfigRequest(nsIChannel** outChannel) {
  nsresult rv = EnsurePrefsRead();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> configURI;
  rv = NS_NewURI(getter_AddRefs(configURI), sConfigUrl);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), configURI,
                     nsContentUtils::GetSystemPrincipal(),
                     nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL |
                         nsILoadInfo::SEC_COOKIES_OMIT,
                     nsIContentPolicy::TYPE_OTHER);
  NS_ENSURE_SUCCESS(rv, rv);

  // Flags from
  // https://searchfox.org/mozilla-central/rev/6fc1a107c44e011a585614cbbe8826f9e01ec802/netwerk/protocol/http/ObliviousHttpService.cpp#87-88
  rv = channel->SetLoadFlags(
      nsIRequest::LOAD_ANONYMOUS | nsIRequest::INHIBIT_CACHING |
      nsIRequest::LOAD_BYPASS_CACHE | nsIChannel::LOAD_BYPASS_URL_CLASSIFIER);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(channel);
  rv = httpChannel->SetRequestMethod("GET"_ns);
  NS_ENSURE_SUCCESS(rv, rv);

  httpChannel.forget(outChannel);
  return NS_OK;
}

}  // namespace mozilla::widget
