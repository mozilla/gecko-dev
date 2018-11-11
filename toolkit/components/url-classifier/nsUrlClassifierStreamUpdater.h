//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUrlClassifierStreamUpdater_h_
#define nsUrlClassifierStreamUpdater_h_

#include <nsISupportsUtils.h>

#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsIUrlClassifierStreamUpdater.h"
#include "nsIStreamListener.h"
#include "nsIChannel.h"
#include "nsTArray.h"
#include "nsITimer.h"
#include "mozilla/Attributes.h"

// Forward declare pointers
class nsIURI;

class nsUrlClassifierStreamUpdater final : public nsIUrlClassifierStreamUpdater,
                                           public nsIUrlClassifierUpdateObserver,
                                           public nsIStreamListener,
                                           public nsIObserver,
                                           public nsIInterfaceRequestor,
                                           public nsITimerCallback
{
public:
  nsUrlClassifierStreamUpdater();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERSTREAMUPDATER
  NS_DECL_NSIURLCLASSIFIERUPDATEOBSERVER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIOBSERVER
  NS_DECL_NSITIMERCALLBACK

private:
  // No subclassing
  ~nsUrlClassifierStreamUpdater() {}

  // When the dbservice sends an UpdateComplete or UpdateFailure, we call this
  // to reset the stream updater.
  void DownloadDone();

  // Disallow copy constructor
  nsUrlClassifierStreamUpdater(nsUrlClassifierStreamUpdater&);

  nsresult AddRequestBody(const nsACString &aRequestBody);

  // Fetches an update for a single table.
  nsresult FetchUpdate(nsIURI *aURI,
                       const nsACString &aRequest,
                       bool aIsPostRequest,
                       const nsACString &aTable);
  // Dumb wrapper so we don't have to create URIs.
  nsresult FetchUpdate(const nsACString &aURI,
                       const nsACString &aRequest,
                       bool aIsPostRequest,
                       const nsACString &aTable);

  // Fetches the next table, from mPendingUpdates.
  nsresult FetchNext();
  // Fetches the next request, from mPendingRequests
  nsresult FetchNextRequest();


  bool mIsUpdating;
  bool mInitialized;
  bool mDownloadError;
  bool mBeganStream;
  nsCString mStreamTable;
  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsIUrlClassifierDBService> mDBService;
  nsCOMPtr<nsITimer> mTimer;

  struct PendingRequest {
    nsCString mTables;
    nsCString mRequestPayload;
    bool mIsPostRequest;
    nsCString mUrl;
    nsCOMPtr<nsIUrlClassifierCallback> mSuccessCallback;
    nsCOMPtr<nsIUrlClassifierCallback> mUpdateErrorCallback;
    nsCOMPtr<nsIUrlClassifierCallback> mDownloadErrorCallback;
  };
  nsTArray<PendingRequest> mPendingRequests;

  struct PendingUpdate {
    nsCString mUrl;
    nsCString mTable;
  };
  nsTArray<PendingUpdate> mPendingUpdates;

  nsCOMPtr<nsIUrlClassifierCallback> mSuccessCallback;
  nsCOMPtr<nsIUrlClassifierCallback> mUpdateErrorCallback;
  nsCOMPtr<nsIUrlClassifierCallback> mDownloadErrorCallback;
};

#endif // nsUrlClassifierStreamUpdater_h_
