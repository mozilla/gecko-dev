/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCORSListenerProxy_h__
#define nsCORSListenerProxy_h__

#include "nsIStreamListener.h"
#include "nsIInterfaceRequestor.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIURI.h"
#include "nsTArray.h"
#include "nsIInterfaceRequestor.h"
#include "nsIChannelEventSink.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "mozilla/Attributes.h"

class nsIURI;
class nsIParser;
class nsIPrincipal;

extern bool
IsValidHTTPToken(const nsCSubstring& aToken);

nsresult
NS_StartCORSPreflight(nsIChannel* aRequestChannel,
                      nsIStreamListener* aListener,
                      nsIPrincipal* aPrincipal,
                      bool aWithCredentials,
                      nsTArray<nsCString>& aACUnsafeHeaders,
                      nsIChannel** aPreflightChannel);

class nsCORSListenerProxy MOZ_FINAL : public nsIStreamListener,
                                      public nsIInterfaceRequestor,
                                      public nsIChannelEventSink,
                                      public nsIAsyncVerifyRedirectCallback
{
public:
  nsCORSListenerProxy(nsIStreamListener* aOuter,
                      nsIPrincipal* aRequestingPrincipal,
                      bool aWithCredentials);
  nsCORSListenerProxy(nsIStreamListener* aOuter,
                      nsIPrincipal* aRequestingPrincipal,
                      bool aWithCredentials,
                      const nsCString& aPreflightMethod,
                      const nsTArray<nsCString>& aPreflightHeaders);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIASYNCVERIFYREDIRECTCALLBACK

  // Must be called at startup.
  static void Startup();

  static void Shutdown();

  nsresult Init(nsIChannel* aChannel, bool aAllowDataURI = false);

private:
  ~nsCORSListenerProxy() {}

  nsresult UpdateChannel(nsIChannel* aChannel, bool aAllowDataURI = false);
  nsresult CheckRequestApproved(nsIRequest* aRequest);

  nsCOMPtr<nsIStreamListener> mOuterListener;
  // The principal that originally kicked off the request
  nsCOMPtr<nsIPrincipal> mRequestingPrincipal;
  // The principal to use for our Origin header ("source origin" in spec terms).
  // This can get changed during redirects, unlike mRequestingPrincipal.
  nsCOMPtr<nsIPrincipal> mOriginHeaderPrincipal;
  nsCOMPtr<nsIInterfaceRequestor> mOuterNotificationCallbacks;
  bool mWithCredentials;
  bool mRequestApproved;
  bool mHasBeenCrossSite;
  bool mIsPreflight;
  nsCString mPreflightMethod;
  nsTArray<nsCString> mPreflightHeaders;
  nsCOMPtr<nsIAsyncVerifyRedirectCallback> mRedirectCallback;
  nsCOMPtr<nsIChannel> mOldRedirectChannel;
  nsCOMPtr<nsIChannel> mNewRedirectChannel;
};

#endif
