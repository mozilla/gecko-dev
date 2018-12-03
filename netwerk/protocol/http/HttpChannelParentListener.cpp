/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "HttpChannelParentListener.h"
#include "mozilla/dom/ServiceWorkerInterceptController.h"
#include "mozilla/dom/ServiceWorkerUtils.h"
#include "mozilla/net/HttpChannelParent.h"
#include "mozilla/net/RedirectChannelRegistrar.h"
#include "mozilla/Unused.h"
#include "nsIAuthPrompt.h"
#include "nsIAuthPrompt2.h"
#include "nsIHttpHeaderVisitor.h"
#include "nsIRedirectProcessChooser.h"
#include "nsITabParent.h"
#include "nsIPromptFactory.h"
#include "nsIWindowWatcher.h"
#include "nsQueryObject.h"

using mozilla::Unused;
using mozilla::dom::ServiceWorkerInterceptController;
using mozilla::dom::ServiceWorkerParentInterceptEnabled;

namespace mozilla {
namespace net {

HttpChannelParentListener::HttpChannelParentListener(
    HttpChannelParent* aInitialChannel)
    : mNextListener(aInitialChannel),
      mRedirectChannelId(0),
      mSuspendedForDiversion(false),
      mShouldIntercept(false),
      mShouldSuspendIntercept(false),
      mInterceptCanceled(false) {
  LOG((
      "HttpChannelParentListener::HttpChannelParentListener [this=%p, next=%p]",
      this, aInitialChannel));

  if (ServiceWorkerParentInterceptEnabled()) {
    mInterceptController = new ServiceWorkerInterceptController();
  }
}

HttpChannelParentListener::~HttpChannelParentListener() {
  LOG(("HttpChannelParentListener::~HttpChannelParentListener %p", this));
}

//-----------------------------------------------------------------------------
// HttpChannelParentListener::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ADDREF(HttpChannelParentListener)
NS_IMPL_RELEASE(HttpChannelParentListener)
NS_INTERFACE_MAP_BEGIN(HttpChannelParentListener)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsIChannelEventSink)
  NS_INTERFACE_MAP_ENTRY(nsIRedirectResultListener)
  NS_INTERFACE_MAP_ENTRY(nsINetworkInterceptController)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(HttpChannelParentListener)
NS_INTERFACE_MAP_END

//-----------------------------------------------------------------------------
// HttpChannelParentListener::nsIRequestObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParentListener::OnStartRequest(nsIRequest* aRequest,
                                          nsISupports* aContext) {
  MOZ_RELEASE_ASSERT(!mSuspendedForDiversion,
                     "Cannot call OnStartRequest if suspended for diversion!");

  if (!mNextListener) return NS_ERROR_UNEXPECTED;

  LOG(("HttpChannelParentListener::OnStartRequest [this=%p]\n", this));
  return mNextListener->OnStartRequest(aRequest, aContext);
}

NS_IMETHODIMP
HttpChannelParentListener::OnStopRequest(nsIRequest* aRequest,
                                         nsISupports* aContext,
                                         nsresult aStatusCode) {
  MOZ_RELEASE_ASSERT(!mSuspendedForDiversion,
                     "Cannot call OnStopRequest if suspended for diversion!");

  if (!mNextListener) return NS_ERROR_UNEXPECTED;

  LOG(("HttpChannelParentListener::OnStopRequest: [this=%p status=%" PRIu32
       "]\n",
       this, static_cast<uint32_t>(aStatusCode)));
  nsresult rv = mNextListener->OnStopRequest(aRequest, aContext, aStatusCode);

  mNextListener = nullptr;
  return rv;
}

//-----------------------------------------------------------------------------
// HttpChannelParentListener::nsIStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParentListener::OnDataAvailable(nsIRequest* aRequest,
                                           nsISupports* aContext,
                                           nsIInputStream* aInputStream,
                                           uint64_t aOffset, uint32_t aCount) {
  MOZ_RELEASE_ASSERT(!mSuspendedForDiversion,
                     "Cannot call OnDataAvailable if suspended for diversion!");

  if (!mNextListener) return NS_ERROR_UNEXPECTED;

  LOG(("HttpChannelParentListener::OnDataAvailable [this=%p]\n", this));
  return mNextListener->OnDataAvailable(aRequest, aContext, aInputStream,
                                        aOffset, aCount);
}

//-----------------------------------------------------------------------------
// HttpChannelParentListener::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParentListener::GetInterface(const nsIID& aIID, void** result) {
  if (aIID.Equals(NS_GET_IID(nsIChannelEventSink)) ||
      aIID.Equals(NS_GET_IID(nsINetworkInterceptController)) ||
      aIID.Equals(NS_GET_IID(nsIRedirectResultListener))) {
    return QueryInterface(aIID, result);
  }

  nsCOMPtr<nsIInterfaceRequestor> ir;
  if (mNextListener && NS_SUCCEEDED(CallQueryInterface(mNextListener.get(),
                                                       getter_AddRefs(ir)))) {
    return ir->GetInterface(aIID, result);
  }

  if (aIID.Equals(NS_GET_IID(nsIAuthPrompt)) ||
      aIID.Equals(NS_GET_IID(nsIAuthPrompt2))) {
    nsresult rv;
    nsCOMPtr<nsIPromptFactory> wwatch =
        do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    return wwatch->GetPrompt(nullptr, aIID, reinterpret_cast<void**>(result));
  }

  return NS_NOINTERFACE;
}

//-----------------------------------------------------------------------------
// HttpChannelParentListener::nsIChannelEventSink
//-----------------------------------------------------------------------------
nsresult HttpChannelParentListener::TriggerCrossProcessRedirect(
    nsIChannel* aChannel, nsILoadInfo* aLoadInfo, uint64_t aIdentifier) {
  RefPtr<HttpChannelParent> channelParent = do_QueryObject(mNextListener);
  MOZ_ASSERT(channelParent);
  channelParent->SetCrossProcessRedirect();

  nsCOMPtr<nsIChannel> channel = aChannel;
  RefPtr<nsHttpChannel> httpChannel = do_QueryObject(channel);
  RefPtr<nsHttpChannel::TabPromise> p = httpChannel->TakeRedirectTabPromise();
  nsCOMPtr<nsILoadInfo> loadInfo = aLoadInfo;

  RefPtr<HttpChannelParentListener> self = this;
  p->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [=](nsCOMPtr<nsITabParent> tp) {
        nsresult rv;

        // Register the new channel and obtain id for it
        nsCOMPtr<nsIRedirectChannelRegistrar> registrar =
            RedirectChannelRegistrar::GetOrCreate();
        MOZ_ASSERT(registrar);
        rv = registrar->RegisterChannel(channel, &self->mRedirectChannelId);
        NS_ENSURE_SUCCESS(rv, rv);

        LOG(("Registered %p channel under id=%d", channel.get(),
             self->mRedirectChannelId));

        OptionalLoadInfoArgs loadInfoArgs;
        MOZ_ALWAYS_SUCCEEDS(LoadInfoToLoadInfoArgs(loadInfo, &loadInfoArgs));

        uint32_t newLoadFlags = nsIRequest::LOAD_NORMAL;
        MOZ_ALWAYS_SUCCEEDS(channel->GetLoadFlags(&newLoadFlags));

        nsCOMPtr<nsIURI> uri;
        channel->GetURI(getter_AddRefs(uri));

        nsCOMPtr<nsIURI> originalURI;
        channel->GetOriginalURI(getter_AddRefs(originalURI));

        uint64_t channelId;
        MOZ_ALWAYS_SUCCEEDS(httpChannel->GetChannelId(&channelId));

        dom::TabParent* tabParent = dom::TabParent::GetFrom(tp);
        ContentParent* cp = tabParent->Manager()->AsContentParent();
        PNeckoParent* neckoParent =
            SingleManagedOrNull(cp->ManagedPNeckoParent());

        RefPtr<HttpChannelParent> channelParent =
            do_QueryObject(self->mNextListener);
        MOZ_ASSERT(channelParent);
        channelParent->SetCrossProcessRedirect();

        auto result = neckoParent->SendCrossProcessRedirect(
            self->mRedirectChannelId, uri, newLoadFlags, loadInfoArgs,
            channelId, originalURI, aIdentifier);

        MOZ_ASSERT(result, "SendCrossProcessRedirect failed");

        return result ? NS_OK : NS_ERROR_UNEXPECTED;
      },
      [httpChannel](nsresult aStatus) {
        MOZ_ASSERT(NS_FAILED(aStatus), "Status should be error");
        httpChannel->OnRedirectVerifyCallback(aStatus);
      });

  return NS_OK;
}

NS_IMETHODIMP
HttpChannelParentListener::AsyncOnChannelRedirect(
    nsIChannel* aOldChannel, nsIChannel* aNewChannel, uint32_t aRedirectFlags,
    nsIAsyncVerifyRedirectCallback* aCallback) {
  LOG(
      ("HttpChannelParentListener::AsyncOnChannelRedirect [this=%p, old=%p, "
       "new=%p, flags=%u]",
       this, aOldChannel, aNewChannel, aRedirectFlags));

  nsresult rv;

  nsCOMPtr<nsIParentRedirectingChannel> activeRedirectingChannel =
      do_QueryInterface(mNextListener);
  if (!activeRedirectingChannel) {
    NS_ERROR(
        "Channel got a redirect response, but doesn't implement "
        "nsIParentRedirectingChannel to handle it.");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Register the new channel and obtain id for it
  nsCOMPtr<nsIRedirectChannelRegistrar> registrar =
      RedirectChannelRegistrar::GetOrCreate();
  MOZ_ASSERT(registrar);

  rv = registrar->RegisterChannel(aNewChannel, &mRedirectChannelId);
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("Registered %p channel under id=%d", aNewChannel, mRedirectChannelId));

  return activeRedirectingChannel->StartRedirect(
      mRedirectChannelId, aNewChannel, aRedirectFlags, aCallback);
}

//-----------------------------------------------------------------------------
// HttpChannelParentListener::nsIRedirectResultListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParentListener::OnRedirectResult(bool succeeded) {
  LOG(("HttpChannelParentListener::OnRedirectResult [this=%p, suc=%d]", this,
       succeeded));

  nsresult rv;

  nsCOMPtr<nsIParentChannel> redirectChannel;
  if (mRedirectChannelId) {
    nsCOMPtr<nsIRedirectChannelRegistrar> registrar =
        RedirectChannelRegistrar::GetOrCreate();
    MOZ_ASSERT(registrar);

    rv = registrar->GetParentChannel(mRedirectChannelId,
                                     getter_AddRefs(redirectChannel));
    if (NS_FAILED(rv) || !redirectChannel) {
      // Redirect might get canceled before we got AsyncOnChannelRedirect
      LOG(("Registered parent channel not found under id=%d",
           mRedirectChannelId));

      nsCOMPtr<nsIChannel> newChannel;
      rv = registrar->GetRegisteredChannel(mRedirectChannelId,
                                           getter_AddRefs(newChannel));
      MOZ_ASSERT(newChannel, "Already registered channel not found");

      if (NS_SUCCEEDED(rv)) newChannel->Cancel(NS_BINDING_ABORTED);
    }

    // Release all previously registered channels, they are no longer need to be
    // kept in the registrar from this moment.
    registrar->DeregisterChannels(mRedirectChannelId);

    mRedirectChannelId = 0;
  }

  if (!redirectChannel) {
    succeeded = false;
  }

  nsCOMPtr<nsIParentRedirectingChannel> activeRedirectingChannel =
      do_QueryInterface(mNextListener);
  MOZ_ASSERT(activeRedirectingChannel,
             "Channel finished a redirect response, but doesn't implement "
             "nsIParentRedirectingChannel to complete it.");

  if (activeRedirectingChannel) {
    activeRedirectingChannel->CompleteRedirect(succeeded);
  } else {
    succeeded = false;
  }

  if (succeeded) {
    // Switch to redirect channel and delete the old one.  Only do this
    // if we are actually changing channels.  During a service worker
    // interception internal redirect we preserve the same HttpChannelParent.
    if (!SameCOMIdentity(redirectChannel, mNextListener)) {
      nsCOMPtr<nsIParentChannel> parent;
      parent = do_QueryInterface(mNextListener);
      MOZ_ASSERT(parent);
      parent->Delete();
      mInterceptCanceled = false;
      mNextListener = redirectChannel;
      MOZ_ASSERT(mNextListener);
      redirectChannel->SetParentListener(this);
    }
  } else if (redirectChannel) {
    // Delete the redirect target channel: continue using old channel
    redirectChannel->Delete();
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelParentListener::nsINetworkInterceptController
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParentListener::ShouldPrepareForIntercept(nsIURI* aURI,
                                                     nsIChannel* aChannel,
                                                     bool* aShouldIntercept) {
  // If parent-side interception is enabled just forward to the real
  // network controler.
  if (mInterceptController) {
    return mInterceptController->ShouldPrepareForIntercept(aURI, aChannel,
                                                           aShouldIntercept);
  }
  *aShouldIntercept = mShouldIntercept;
  return NS_OK;
}

class HeaderVisitor final : public nsIHttpHeaderVisitor {
  nsCOMPtr<nsIInterceptedChannel> mChannel;
  ~HeaderVisitor() = default;

 public:
  explicit HeaderVisitor(nsIInterceptedChannel* aChannel)
      : mChannel(aChannel) {}

  NS_DECL_ISUPPORTS

  NS_IMETHOD VisitHeader(const nsACString& aHeader,
                         const nsACString& aValue) override {
    mChannel->SynthesizeHeader(aHeader, aValue);
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(HeaderVisitor, nsIHttpHeaderVisitor)

class FinishSynthesizedResponse : public Runnable {
  nsCOMPtr<nsIInterceptedChannel> mChannel;

 public:
  explicit FinishSynthesizedResponse(nsIInterceptedChannel* aChannel)
      : Runnable("net::FinishSynthesizedResponse"), mChannel(aChannel) {}

  NS_IMETHOD Run() override {
    // The URL passed as an argument here doesn't matter, since the child will
    // receive a redirection notification as a result of this synthesized
    // response.
    mChannel->StartSynthesizedResponse(nullptr, nullptr, nullptr,
                                       EmptyCString(), false);
    mChannel->FinishSynthesizedResponse();
    return NS_OK;
  }
};

NS_IMETHODIMP
HttpChannelParentListener::ChannelIntercepted(nsIInterceptedChannel* aChannel) {
  // If parent-side interception is enabled just forward to the real
  // network controler.
  if (mInterceptController) {
    return mInterceptController->ChannelIntercepted(aChannel);
  }

  // Its possible for the child-side interception to complete and tear down
  // the actor before we even get this parent-side interception notification.
  // In this case we want to let the interception succeed, but then immediately
  // cancel it.  If we return an error code from here then it might get
  // propagated back to the child process where the interception did not
  // encounter an error.  Therefore cancel the new channel asynchronously from a
  // runnable.
  if (mInterceptCanceled) {
    nsCOMPtr<nsIRunnable> r = NewRunnableMethod<nsresult>(
        "HttpChannelParentListener::CancelInterception", aChannel,
        &nsIInterceptedChannel::CancelInterception, NS_BINDING_ABORTED);
    MOZ_ALWAYS_SUCCEEDS(SystemGroup::Dispatch(TaskCategory::Other, r.forget()));
    return NS_OK;
  }

  if (mShouldSuspendIntercept) {
    mInterceptedChannel = aChannel;
    return NS_OK;
  }

  nsAutoCString statusText;
  mSynthesizedResponseHead->StatusText(statusText);
  aChannel->SynthesizeStatus(mSynthesizedResponseHead->Status(), statusText);
  nsCOMPtr<nsIHttpHeaderVisitor> visitor = new HeaderVisitor(aChannel);
  DebugOnly<nsresult> rv = mSynthesizedResponseHead->VisitHeaders(
      visitor, nsHttpHeaderArray::eFilterResponse);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  nsCOMPtr<nsIRunnable> event = new FinishSynthesizedResponse(aChannel);
  NS_DispatchToCurrentThread(event);

  mSynthesizedResponseHead = nullptr;

  MOZ_ASSERT(mNextListener);
  RefPtr<HttpChannelParent> channel = do_QueryObject(mNextListener);
  MOZ_ASSERT(channel);
  channel->ResponseSynthesized();

  return NS_OK;
}

//-----------------------------------------------------------------------------

nsresult HttpChannelParentListener::SuspendForDiversion() {
  if (NS_WARN_IF(mSuspendedForDiversion)) {
    MOZ_ASSERT(!mSuspendedForDiversion, "Cannot SuspendForDiversion twice!");
    return NS_ERROR_UNEXPECTED;
  }

  // While this is set, no OnStart/OnData/OnStop callbacks should be forwarded
  // to mNextListener.
  mSuspendedForDiversion = true;

  return NS_OK;
}

nsresult HttpChannelParentListener::ResumeForDiversion() {
  MOZ_RELEASE_ASSERT(mSuspendedForDiversion, "Must already be suspended!");

  // Allow OnStart/OnData/OnStop callbacks to be forwarded to mNextListener.
  mSuspendedForDiversion = false;

  return NS_OK;
}

nsresult HttpChannelParentListener::DivertTo(nsIStreamListener* aListener) {
  MOZ_ASSERT(aListener);
  MOZ_RELEASE_ASSERT(mSuspendedForDiversion, "Must already be suspended!");

  // Reset mInterceptCanceled back to false every time a new listener is set.
  // We only want to cancel the interception if our current listener has
  // signaled its cleaning up.
  mInterceptCanceled = false;

  mNextListener = aListener;

  return ResumeForDiversion();
}

void HttpChannelParentListener::SetupInterception(
    const nsHttpResponseHead& aResponseHead) {
  mSynthesizedResponseHead = new nsHttpResponseHead(aResponseHead);
  mShouldIntercept = true;
}

void HttpChannelParentListener::SetupInterceptionAfterRedirect(
    bool aShouldIntercept) {
  mShouldIntercept = aShouldIntercept;
  if (mShouldIntercept) {
    // When an interception occurs, this channel should suspend all further
    // activity. It will be torn down and recreated if necessary.
    mShouldSuspendIntercept = true;
  }
}

void HttpChannelParentListener::ClearInterceptedChannel(
    nsIStreamListener* aListener) {
  // Only cancel the interception if this is from our current listener.  We
  // can get spurious calls here from other HttpChannelParent instances being
  // destroyed asynchronously.
  if (!SameCOMIdentity(mNextListener, aListener)) {
    return;
  }
  if (mInterceptedChannel) {
    mInterceptedChannel->CancelInterception(NS_ERROR_INTERCEPTION_FAILED);
    mInterceptedChannel = nullptr;
  }
  // Note that channel interception has been canceled.  If we got this before
  // the interception even occured we will trigger the cancel later.
  mInterceptCanceled = true;
}

}  // namespace net
}  // namespace mozilla
