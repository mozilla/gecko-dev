/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "nsIFaviconService.h"
#include "nsIChannelEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIStreamListener.h"
#include "mozIPlacesPendingOperation.h"
#include "nsThreadUtils.h"
#include "nsProxyRelease.h"

class nsIPrincipal;

#include "Database.h"
#include "mozilla/storage.h"

#define ICON_STATUS_UNKNOWN 0
#define ICON_STATUS_CHANGED 1 << 0
#define ICON_STATUS_SAVED 1 << 1
#define ICON_STATUS_ASSOCIATED 1 << 2
#define ICON_STATUS_CACHED 1 << 3

#define TO_CHARBUFFER(_buffer) \
  reinterpret_cast<char*>(const_cast<uint8_t*>(_buffer))
#define TO_INTBUFFER(_string) \
  reinterpret_cast<uint8_t*>(const_cast<char*>(_string.get()))

/**
 * The maximum time we will keep a favicon around.  We always ask the cache, if
 * we can, but default to this value if we do not get a time back, or the time
 * is more in the future than this.
 * Currently set to one week from now.
 */
#define MAX_FAVICON_EXPIRATION ((PRTime)7 * 24 * 60 * 60 * PR_USEC_PER_SEC)

namespace mozilla {
namespace places {

/**
 * Indicates when a icon should be fetched from network.
 */
enum AsyncFaviconFetchMode {
  FETCH_NEVER = 0
, FETCH_IF_MISSING
, FETCH_ALWAYS
};

/**
 * Data cache for a icon entry.
 */
struct IconData
{
  IconData()
  : id(0)
  , expiration(0)
  , fetchMode(FETCH_NEVER)
  , status(ICON_STATUS_UNKNOWN)
  {
  }

  int64_t id;
  nsCString spec;
  nsCString data;
  nsCString mimeType;
  PRTime expiration;
  enum AsyncFaviconFetchMode fetchMode;
  uint16_t status; // This is a bitset, see ICON_STATUS_* defines above.
};

/**
 * Data cache for a page entry.
 */
struct PageData
{
  PageData()
  : id(0)
  , canAddToHistory(true)
  , iconId(0)
  {
    guid.SetIsVoid(true);
  }

  int64_t id;
  nsCString spec;
  nsCString bookmarkedSpec;
  nsString revHost;
  bool canAddToHistory; // False for disabled history and unsupported schemas.
  int64_t iconId;
  nsCString guid;
};

/**
 * Async fetches icon from database or network, associates it with the required
 * page and finally notifies the change.
 */
class AsyncFetchAndSetIconForPage final : public Runnable
                                        , public nsIStreamListener
                                        , public nsIInterfaceRequestor
                                        , public nsIChannelEventSink
                                        , public mozIPlacesPendingOperation
 {
 public:
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_MOZIPLACESPENDINGOPERATION
  NS_DECL_ISUPPORTS_INHERITED

  /**
   * Constructor.
   *
   * @param aIcon
   *        Icon to be fetched and associated.
   * @param aPage
   *        Page to which associate the icon.
   * @param aFaviconLoadPrivate
   *        Whether this favicon load is in private browsing.
   * @param aCallback
   *        Function to be called when the fetch-and-associate process finishes.
   * @param aLoadingPrincipal
   *        LoadingPrincipal of the icon to be fetched.
   */
  AsyncFetchAndSetIconForPage(IconData& aIcon,
                              PageData& aPage,
                              bool aFaviconLoadPrivate,
                              nsIFaviconDataCallback* aCallback,
                              nsIPrincipal* aLoadingPrincipal);

private:
  nsresult FetchFromNetwork();
  virtual ~AsyncFetchAndSetIconForPage() {}

  nsMainThreadPtrHandle<nsIFaviconDataCallback> mCallback;
  IconData mIcon;
  PageData mPage;
  const bool mFaviconLoadPrivate;
  nsMainThreadPtrHandle<nsIPrincipal> mLoadingPrincipal;
  bool mCanceled;
  nsCOMPtr<nsIRequest> mRequest;
};

/**
 * Associates the icon to the required page, finally dispatches an event to the
 * main thread to notify the change to observers.
 */
class AsyncAssociateIconToPage final : public Runnable
{
public:
  NS_DECL_NSIRUNNABLE

  /**
   * Constructor.
   *
   * @param aIcon
   *        Icon to be associated.
   * @param aPage
   *        Page to which associate the icon.
   * @param aCallback
   *        Function to be called when the associate process finishes.
   */
  AsyncAssociateIconToPage(const IconData& aIcon,
                           const PageData& aPage,
                           const nsMainThreadPtrHandle<nsIFaviconDataCallback>& aCallback);

private:
  nsMainThreadPtrHandle<nsIFaviconDataCallback> mCallback;
  IconData mIcon;
  PageData mPage;
};

/**
 * Asynchronously tries to get the URL of a page's favicon, then notifies the
 * given observer.
 */
class AsyncGetFaviconURLForPage final : public Runnable
{
public:
  NS_DECL_NSIRUNNABLE

  /**
   * Constructor.
   *
   * @param aPageSpec
   *        URL of the page whose favicon's URL we're fetching
   * @param aCallback
   *        function to be called once finished
   */
  AsyncGetFaviconURLForPage(const nsACString& aPageSpec,
                            nsIFaviconDataCallback* aCallback);

private:
  nsMainThreadPtrHandle<nsIFaviconDataCallback> mCallback;
  nsCString mPageSpec;
};


/**
 * Asynchronously tries to get the URL and data of a page's favicon, then
 * notifies the given observer.
 */
class AsyncGetFaviconDataForPage final : public Runnable
{
public:
  NS_DECL_NSIRUNNABLE

  /**
   * Constructor.
   *
   * @param aPageSpec
   *        URL of the page whose favicon URL and data we're fetching
   * @param aCallback
   *        function to be called once finished
   */
  AsyncGetFaviconDataForPage(const nsACString& aPageSpec,
                             nsIFaviconDataCallback* aCallback);

private:
  nsMainThreadPtrHandle<nsIFaviconDataCallback> mCallback;
  nsCString mPageSpec;
};

class AsyncReplaceFaviconData final : public Runnable
{
public:
  NS_DECL_NSIRUNNABLE

  explicit AsyncReplaceFaviconData(const IconData& aIcon);

private:
  nsresult RemoveIconDataCacheEntry();

  IconData mIcon;
};

/**
 * Notifies the icon change to favicon observers.
 */
class NotifyIconObservers final : public Runnable
{
public:
  NS_DECL_NSIRUNNABLE

  /**
   * Constructor.
   *
   * @param aIcon
   *        Icon information. Can be empty if no icon is associated to the page.
   * @param aPage
   *        Page to which the icon information applies.
   * @param aCallback
   *        Function to be notified in all cases.
   */
  NotifyIconObservers(const IconData& aIcon,
                      const PageData& aPage,
                      const nsMainThreadPtrHandle<nsIFaviconDataCallback>& aCallback);

private:
  nsMainThreadPtrHandle<nsIFaviconDataCallback> mCallback;
  IconData mIcon;
  PageData mPage;

  void SendGlobalNotifications(nsIURI* aIconURI);
};

} // namespace places
} // namespace mozilla
