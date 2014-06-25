/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/LinkedList.h"

#include "nsCrossSiteListenerProxy.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsError.h"
#include "nsContentUtils.h"
#include "nsIScriptSecurityManager.h"
#include "nsNetUtil.h"
#include "nsMimeTypes.h"
#include "nsIStreamConverterService.h"
#include "nsStringStream.h"
#include "nsGkAtoms.h"
#include "nsWhitespaceTokenizer.h"
#include "nsIChannelEventSink.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsAsyncRedirectVerifyHelper.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "nsStreamUtils.h"
#include "mozilla/Preferences.h"
#include "nsIScriptError.h"
#include "nsILoadGroup.h"
#include "nsILoadContext.h"
#include "nsIConsoleService.h"
#include "nsIDOMWindowUtils.h"
#include "nsIDOMWindow.h"
#include <algorithm>

using namespace mozilla;

#define PREFLIGHT_CACHE_SIZE 100

static bool gDisableCORS = false;
static bool gDisableCORSPrivateData = false;

static nsresult
LogBlockedRequest(nsIRequest* aRequest)
{
  nsresult rv = NS_OK;

  // Get the innerWindowID associated with the XMLHTTPRequest
  PRUint64 innerWindowID = 0;

  nsCOMPtr<nsILoadGroup> loadGroup;
  aRequest->GetLoadGroup(getter_AddRefs(loadGroup));
  if (loadGroup) {
    nsCOMPtr<nsIInterfaceRequestor> callbacks;
    loadGroup->GetNotificationCallbacks(getter_AddRefs(callbacks));
    if (callbacks) {
      nsCOMPtr<nsILoadContext> loadContext = do_GetInterface(callbacks);
      if(loadContext) {
        nsCOMPtr<nsIDOMWindow> window;
        loadContext->GetAssociatedWindow(getter_AddRefs(window));
        if (window) {
          nsCOMPtr<nsIDOMWindowUtils> du = do_GetInterface(window);
          du->GetCurrentInnerWindowID(&innerWindowID);
        }
      }
    }
  }

  if (!innerWindowID) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
  nsCOMPtr<nsIURI> aUri;
  channel->GetURI(getter_AddRefs(aUri));
  nsAutoCString spec;
  if (aUri) {
    aUri->GetSpec(spec);
  }

  // Generate the error message
  nsXPIDLString blockedMessage;
  NS_ConvertUTF8toUTF16 specUTF16(spec);
  const char16_t* params[] = { specUTF16.get() };
  rv = nsContentUtils::FormatLocalizedString(nsContentUtils::eSECURITY_PROPERTIES,
                                             "CrossSiteRequestBlocked",
                                             params,
                                             blockedMessage);

  // Build the error object and log it to the console
  nsCOMPtr<nsIConsoleService> console(do_GetService(NS_CONSOLESERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIScriptError> scriptError = do_CreateInstance(NS_SCRIPTERROR_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString msg(blockedMessage.get());
  rv = scriptError->InitWithWindowID(msg,
                                     NS_ConvertUTF8toUTF16(spec),
                                     EmptyString(),
                                     0,
                                     0,
                                     nsIScriptError::warningFlag,
                                     "CORS",
                                     innerWindowID);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = console->LogMessage(scriptError);
  return rv;
}

//////////////////////////////////////////////////////////////////////////
// Preflight cache

class nsPreflightCache
{
public:
  struct TokenTime
  {
    nsCString token;
    TimeStamp expirationTime;
  };

  struct CacheEntry : public LinkedListElement<CacheEntry>
  {
    CacheEntry(nsCString& aKey)
      : mKey(aKey)
    {
      MOZ_COUNT_CTOR(nsPreflightCache::CacheEntry);
    }
    
    ~CacheEntry()
    {
      MOZ_COUNT_DTOR(nsPreflightCache::CacheEntry);
    }

    void PurgeExpired(TimeStamp now);
    bool CheckRequest(const nsCString& aMethod,
                        const nsTArray<nsCString>& aCustomHeaders);

    nsCString mKey;
    nsTArray<TokenTime> mMethods;
    nsTArray<TokenTime> mHeaders;
  };

  nsPreflightCache()
  {
    MOZ_COUNT_CTOR(nsPreflightCache);
  }

  ~nsPreflightCache()
  {
    Clear();
    MOZ_COUNT_DTOR(nsPreflightCache);
  }

  bool Initialize()
  {
    return true;
  }

  CacheEntry* GetEntry(nsIURI* aURI, nsIPrincipal* aPrincipal,
                       bool aWithCredentials, bool aCreate);
  void RemoveEntries(nsIURI* aURI, nsIPrincipal* aPrincipal);

  void Clear();

private:
  static PLDHashOperator
    RemoveExpiredEntries(const nsACString& aKey, nsAutoPtr<CacheEntry>& aValue,
                         void* aUserData);

  static bool GetCacheKey(nsIURI* aURI, nsIPrincipal* aPrincipal,
                            bool aWithCredentials, nsACString& _retval);

  nsClassHashtable<nsCStringHashKey, CacheEntry> mTable;
  LinkedList<CacheEntry> mList;
};

// Will be initialized in EnsurePreflightCache.
static nsPreflightCache* sPreflightCache = nullptr;

static bool EnsurePreflightCache()
{
  if (sPreflightCache)
    return true;

  nsAutoPtr<nsPreflightCache> newCache(new nsPreflightCache());

  if (newCache->Initialize()) {
    sPreflightCache = newCache.forget();
    return true;
  }

  return false;
}

void
nsPreflightCache::CacheEntry::PurgeExpired(TimeStamp now)
{
  uint32_t i;
  for (i = 0; i < mMethods.Length(); ++i) {
    if (now >= mMethods[i].expirationTime) {
      mMethods.RemoveElementAt(i--);
    }
  }
  for (i = 0; i < mHeaders.Length(); ++i) {
    if (now >= mHeaders[i].expirationTime) {
      mHeaders.RemoveElementAt(i--);
    }
  }
}

bool
nsPreflightCache::CacheEntry::CheckRequest(const nsCString& aMethod,
                                           const nsTArray<nsCString>& aHeaders)
{
  PurgeExpired(TimeStamp::NowLoRes());

  if (!aMethod.EqualsLiteral("GET") && !aMethod.EqualsLiteral("POST")) {
    uint32_t i;
    for (i = 0; i < mMethods.Length(); ++i) {
      if (aMethod.Equals(mMethods[i].token))
        break;
    }
    if (i == mMethods.Length()) {
      return false;
    }
  }

  for (uint32_t i = 0; i < aHeaders.Length(); ++i) {
    uint32_t j;
    for (j = 0; j < mHeaders.Length(); ++j) {
      if (aHeaders[i].Equals(mHeaders[j].token,
                             nsCaseInsensitiveCStringComparator())) {
        break;
      }
    }
    if (j == mHeaders.Length()) {
      return false;
    }
  }

  return true;
}

nsPreflightCache::CacheEntry*
nsPreflightCache::GetEntry(nsIURI* aURI,
                           nsIPrincipal* aPrincipal,
                           bool aWithCredentials,
                           bool aCreate)
{
  nsCString key;
  if (!GetCacheKey(aURI, aPrincipal, aWithCredentials, key)) {
    NS_WARNING("Invalid cache key!");
    return nullptr;
  }

  CacheEntry* entry;

  if (mTable.Get(key, &entry)) {
    // Entry already existed so just return it. Also update the LRU list.

    // Move to the head of the list.
    entry->removeFrom(mList);
    mList.insertFront(entry);

    return entry;
  }

  if (!aCreate) {
    return nullptr;
  }

  // This is a new entry, allocate and insert into the table now so that any
  // failures don't cause items to be removed from a full cache.
  entry = new CacheEntry(key);
  if (!entry) {
    NS_WARNING("Failed to allocate new cache entry!");
    return nullptr;
  }

  NS_ASSERTION(mTable.Count() <= PREFLIGHT_CACHE_SIZE,
               "Something is borked, too many entries in the cache!");

  // Now enforce the max count.
  if (mTable.Count() == PREFLIGHT_CACHE_SIZE) {
    // Try to kick out all the expired entries.
    TimeStamp now = TimeStamp::NowLoRes();
    mTable.Enumerate(RemoveExpiredEntries, &now);

    // If that didn't remove anything then kick out the least recently used
    // entry.
    if (mTable.Count() == PREFLIGHT_CACHE_SIZE) {
      CacheEntry* lruEntry = static_cast<CacheEntry*>(mList.popLast());
      MOZ_ASSERT(lruEntry);

      // This will delete 'lruEntry'.
      mTable.Remove(lruEntry->mKey);

      NS_ASSERTION(mTable.Count() == PREFLIGHT_CACHE_SIZE - 1,
                   "Somehow tried to remove an entry that was never added!");
    }
  }
  
  mTable.Put(key, entry);
  mList.insertFront(entry);

  return entry;
}

void
nsPreflightCache::RemoveEntries(nsIURI* aURI, nsIPrincipal* aPrincipal)
{
  CacheEntry* entry;
  nsCString key;
  if (GetCacheKey(aURI, aPrincipal, true, key) &&
      mTable.Get(key, &entry)) {
    entry->removeFrom(mList);
    mTable.Remove(key);
  }

  if (GetCacheKey(aURI, aPrincipal, false, key) &&
      mTable.Get(key, &entry)) {
    entry->removeFrom(mList);
    mTable.Remove(key);
  }
}

void
nsPreflightCache::Clear()
{
  mList.clear();
  mTable.Clear();
}

/* static */ PLDHashOperator
nsPreflightCache::RemoveExpiredEntries(const nsACString& aKey,
                                           nsAutoPtr<CacheEntry>& aValue,
                                           void* aUserData)
{
  TimeStamp* now = static_cast<TimeStamp*>(aUserData);
  
  aValue->PurgeExpired(*now);
  
  if (aValue->mHeaders.IsEmpty() &&
      aValue->mMethods.IsEmpty()) {
    // Expired, remove from the list as well as the hash table.
    aValue->removeFrom(sPreflightCache->mList);
    return PL_DHASH_REMOVE;
  }
  
  return PL_DHASH_NEXT;
}

/* static */ bool
nsPreflightCache::GetCacheKey(nsIURI* aURI,
                              nsIPrincipal* aPrincipal,
                              bool aWithCredentials,
                              nsACString& _retval)
{
  NS_ASSERTION(aURI, "Null uri!");
  NS_ASSERTION(aPrincipal, "Null principal!");
  
  NS_NAMED_LITERAL_CSTRING(space, " ");

  nsCOMPtr<nsIURI> uri;
  nsresult rv = aPrincipal->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, false);
  
  nsAutoCString scheme, host, port;
  if (uri) {
    uri->GetScheme(scheme);
    uri->GetHost(host);
    port.AppendInt(NS_GetRealPort(uri));
  }

  nsAutoCString cred;
  if (aWithCredentials) {
    _retval.AssignLiteral("cred");
  }
  else {
    _retval.AssignLiteral("nocred");
  }

  nsAutoCString spec;
  rv = aURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, false);

  _retval.Assign(cred + space + scheme + space + host + space + port + space +
                 spec);

  return true;
}

//////////////////////////////////////////////////////////////////////////
// nsCORSListenerProxy

NS_IMPL_ISUPPORTS(nsCORSListenerProxy, nsIStreamListener,
                  nsIRequestObserver, nsIChannelEventSink,
                  nsIInterfaceRequestor, nsIAsyncVerifyRedirectCallback)

/* static */
void
nsCORSListenerProxy::Startup()
{
  Preferences::AddBoolVarCache(&gDisableCORS,
                               "content.cors.disable");
  Preferences::AddBoolVarCache(&gDisableCORSPrivateData,
                               "content.cors.no_private_data");
}

/* static */
void
nsCORSListenerProxy::Shutdown()
{
  delete sPreflightCache;
  sPreflightCache = nullptr;
}

nsCORSListenerProxy::nsCORSListenerProxy(nsIStreamListener* aOuter,
                                         nsIPrincipal* aRequestingPrincipal,
                                         bool aWithCredentials)
  : mOuterListener(aOuter),
    mRequestingPrincipal(aRequestingPrincipal),
    mOriginHeaderPrincipal(aRequestingPrincipal),
    mWithCredentials(aWithCredentials && !gDisableCORSPrivateData),
    mRequestApproved(false),
    mHasBeenCrossSite(false),
    mIsPreflight(false)
{
}

nsCORSListenerProxy::nsCORSListenerProxy(nsIStreamListener* aOuter,
                                         nsIPrincipal* aRequestingPrincipal,
                                         bool aWithCredentials,
                                         const nsCString& aPreflightMethod,
                                         const nsTArray<nsCString>& aPreflightHeaders)
  : mOuterListener(aOuter),
    mRequestingPrincipal(aRequestingPrincipal),
    mOriginHeaderPrincipal(aRequestingPrincipal),
    mWithCredentials(aWithCredentials && !gDisableCORSPrivateData),
    mRequestApproved(false),
    mHasBeenCrossSite(false),
    mIsPreflight(true),
    mPreflightMethod(aPreflightMethod),
    mPreflightHeaders(aPreflightHeaders)
{
  for (uint32_t i = 0; i < mPreflightHeaders.Length(); ++i) {
    ToLowerCase(mPreflightHeaders[i]);
  }
  mPreflightHeaders.Sort();
}

nsresult
nsCORSListenerProxy::Init(nsIChannel* aChannel, bool aAllowDataURI)
{
  aChannel->GetNotificationCallbacks(getter_AddRefs(mOuterNotificationCallbacks));
  aChannel->SetNotificationCallbacks(this);

  nsresult rv = UpdateChannel(aChannel, aAllowDataURI);
  if (NS_FAILED(rv)) {
    mOuterListener = nullptr;
    mRequestingPrincipal = nullptr;
    mOriginHeaderPrincipal = nullptr;
    mOuterNotificationCallbacks = nullptr;
  }
  return rv;
}

NS_IMETHODIMP
nsCORSListenerProxy::OnStartRequest(nsIRequest* aRequest,
                                    nsISupports* aContext)
{
  nsresult rv = CheckRequestApproved(aRequest);
  mRequestApproved = NS_SUCCEEDED(rv);
  if (!mRequestApproved) {
    rv = LogBlockedRequest(aRequest);
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Failed to log blocked cross-site request");
    if (sPreflightCache) {
      nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
      if (channel) {
        nsCOMPtr<nsIURI> uri;
        NS_GetFinalChannelURI(channel, getter_AddRefs(uri));
        if (uri) {
          // OK to use mRequestingPrincipal since preflights never get
          // redirected.
          sPreflightCache->RemoveEntries(uri, mRequestingPrincipal);
        }
      }
    }

    aRequest->Cancel(NS_ERROR_DOM_BAD_URI);
    mOuterListener->OnStartRequest(aRequest, aContext);

    return NS_ERROR_DOM_BAD_URI;
  }

  return mOuterListener->OnStartRequest(aRequest, aContext);
}

bool
IsValidHTTPToken(const nsCSubstring& aToken)
{
  if (aToken.IsEmpty()) {
    return false;
  }

  nsCSubstring::const_char_iterator iter, end;

  aToken.BeginReading(iter);
  aToken.EndReading(end);

  while (iter != end) {
    if (*iter <= 32 ||
        *iter >= 127 ||
        *iter == '(' ||
        *iter == ')' ||
        *iter == '<' ||
        *iter == '>' ||
        *iter == '@' ||
        *iter == ',' ||
        *iter == ';' ||
        *iter == ':' ||
        *iter == '\\' ||
        *iter == '\"' ||
        *iter == '/' ||
        *iter == '[' ||
        *iter == ']' ||
        *iter == '?' ||
        *iter == '=' ||
        *iter == '{' ||
        *iter == '}') {
      return false;
    }
    ++iter;
  }

  return true;
}

nsresult
nsCORSListenerProxy::CheckRequestApproved(nsIRequest* aRequest)
{
  // Check if this was actually a cross domain request
  if (!mHasBeenCrossSite) {
    return NS_OK;
  }

  if (gDisableCORS) {
    return NS_ERROR_DOM_BAD_URI;
  }

  // Check if the request failed
  nsresult status;
  nsresult rv = aRequest->GetStatus(&status);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_SUCCESS(status, status);

  // Test that things worked on a HTTP level
  nsCOMPtr<nsIHttpChannel> http = do_QueryInterface(aRequest);
  NS_ENSURE_TRUE(http, NS_ERROR_DOM_BAD_URI);

  // Check the Access-Control-Allow-Origin header
  nsAutoCString allowedOriginHeader;
  rv = http->GetResponseHeader(
    NS_LITERAL_CSTRING("Access-Control-Allow-Origin"), allowedOriginHeader);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mWithCredentials || !allowedOriginHeader.EqualsLiteral("*")) {
    nsAutoCString origin;
    rv = nsContentUtils::GetASCIIOrigin(mOriginHeaderPrincipal, origin);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!allowedOriginHeader.Equals(origin)) {
      return NS_ERROR_DOM_BAD_URI;
    }
  }

  // Check Access-Control-Allow-Credentials header
  if (mWithCredentials) {
    nsAutoCString allowCredentialsHeader;
    rv = http->GetResponseHeader(
      NS_LITERAL_CSTRING("Access-Control-Allow-Credentials"), allowCredentialsHeader);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!allowCredentialsHeader.EqualsLiteral("true")) {
      return NS_ERROR_DOM_BAD_URI;
    }
  }

  if (mIsPreflight) {
    bool succeedded;
    rv = http->GetRequestSucceeded(&succeedded);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!succeedded) {
      return NS_ERROR_DOM_BAD_URI;
    }

    nsAutoCString headerVal;
    // The "Access-Control-Allow-Methods" header contains a comma separated
    // list of method names.
    http->GetResponseHeader(NS_LITERAL_CSTRING("Access-Control-Allow-Methods"),
                            headerVal);
    bool foundMethod = mPreflightMethod.EqualsLiteral("GET") ||
                         mPreflightMethod.EqualsLiteral("HEAD") ||
                         mPreflightMethod.EqualsLiteral("POST");
    nsCCharSeparatedTokenizer methodTokens(headerVal, ',');
    while(methodTokens.hasMoreTokens()) {
      const nsDependentCSubstring& method = methodTokens.nextToken();
      if (method.IsEmpty()) {
        continue;
      }
      if (!IsValidHTTPToken(method)) {
        return NS_ERROR_DOM_BAD_URI;
      }
      foundMethod |= mPreflightMethod.Equals(method);
    }
    NS_ENSURE_TRUE(foundMethod, NS_ERROR_DOM_BAD_URI);

    // The "Access-Control-Allow-Headers" header contains a comma separated
    // list of header names.
    headerVal.Truncate();
    http->GetResponseHeader(NS_LITERAL_CSTRING("Access-Control-Allow-Headers"),
                            headerVal);
    nsTArray<nsCString> headers;
    nsCCharSeparatedTokenizer headerTokens(headerVal, ',');
    while(headerTokens.hasMoreTokens()) {
      const nsDependentCSubstring& header = headerTokens.nextToken();
      if (header.IsEmpty()) {
        continue;
      }
      if (!IsValidHTTPToken(header)) {
        return NS_ERROR_DOM_BAD_URI;
      }
      headers.AppendElement(header);
    }
    for (uint32_t i = 0; i < mPreflightHeaders.Length(); ++i) {
      if (!headers.Contains(mPreflightHeaders[i],
                            nsCaseInsensitiveCStringArrayComparator())) {
        return NS_ERROR_DOM_BAD_URI;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCORSListenerProxy::OnStopRequest(nsIRequest* aRequest,
                                   nsISupports* aContext,
                                   nsresult aStatusCode)
{
  nsresult rv = mOuterListener->OnStopRequest(aRequest, aContext, aStatusCode);
  mOuterListener = nullptr;
  mOuterNotificationCallbacks = nullptr;
  mRedirectCallback = nullptr;
  mOldRedirectChannel = nullptr;
  mNewRedirectChannel = nullptr;
  return rv;
}

NS_IMETHODIMP
nsCORSListenerProxy::OnDataAvailable(nsIRequest* aRequest,
                                     nsISupports* aContext, 
                                     nsIInputStream* aInputStream,
                                     uint64_t aOffset,
                                     uint32_t aCount)
{
  if (!mRequestApproved) {
    return NS_ERROR_DOM_BAD_URI;
  }
  return mOuterListener->OnDataAvailable(aRequest, aContext, aInputStream,
                                         aOffset, aCount);
}

NS_IMETHODIMP
nsCORSListenerProxy::GetInterface(const nsIID & aIID, void **aResult)
{
  if (aIID.Equals(NS_GET_IID(nsIChannelEventSink))) {
    *aResult = static_cast<nsIChannelEventSink*>(this);
    NS_ADDREF_THIS();

    return NS_OK;
  }

  return mOuterNotificationCallbacks ?
    mOuterNotificationCallbacks->GetInterface(aIID, aResult) :
    NS_ERROR_NO_INTERFACE;
}

NS_IMETHODIMP
nsCORSListenerProxy::AsyncOnChannelRedirect(nsIChannel *aOldChannel,
                                            nsIChannel *aNewChannel,
                                            uint32_t aFlags,
                                            nsIAsyncVerifyRedirectCallback *cb)
{
  nsresult rv;
  if (!NS_IsInternalSameURIRedirect(aOldChannel, aNewChannel, aFlags)) {
    rv = CheckRequestApproved(aOldChannel);
    if (NS_FAILED(rv)) {
      rv = LogBlockedRequest(aOldChannel);
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Failed to log blocked cross-site request");

      if (sPreflightCache) {
        nsCOMPtr<nsIURI> oldURI;
        NS_GetFinalChannelURI(aOldChannel, getter_AddRefs(oldURI));
        if (oldURI) {
          // OK to use mRequestingPrincipal since preflights never get
          // redirected.
          sPreflightCache->RemoveEntries(oldURI, mRequestingPrincipal);
        }
      }
      aOldChannel->Cancel(NS_ERROR_DOM_BAD_URI);
      return NS_ERROR_DOM_BAD_URI;
    }

    if (mHasBeenCrossSite) {
      // Once we've been cross-site, cross-origin redirects reset our source
      // origin.
      nsCOMPtr<nsIPrincipal> oldChannelPrincipal;
      nsContentUtils::GetSecurityManager()->
        GetChannelPrincipal(aOldChannel, getter_AddRefs(oldChannelPrincipal));
      nsCOMPtr<nsIPrincipal> newChannelPrincipal;
      nsContentUtils::GetSecurityManager()->
        GetChannelPrincipal(aNewChannel, getter_AddRefs(newChannelPrincipal));
      if (!oldChannelPrincipal || !newChannelPrincipal) {
        rv = NS_ERROR_OUT_OF_MEMORY;
      }

      if (NS_SUCCEEDED(rv)) {
        bool equal;
        rv = oldChannelPrincipal->Equals(newChannelPrincipal, &equal);
        if (NS_SUCCEEDED(rv)) {
          if (!equal) {
            // Spec says to set our source origin to a unique origin.
            mOriginHeaderPrincipal = do_CreateInstance("@mozilla.org/nullprincipal;1");
            if (!mOriginHeaderPrincipal) {
              rv = NS_ERROR_OUT_OF_MEMORY;
            }
          }
        }
      }

      if (NS_FAILED(rv)) {
        aOldChannel->Cancel(rv);
        return rv;
      }
    }
  }

  // Prepare to receive callback
  mRedirectCallback = cb;
  mOldRedirectChannel = aOldChannel;
  mNewRedirectChannel = aNewChannel;

  nsCOMPtr<nsIChannelEventSink> outer =
    do_GetInterface(mOuterNotificationCallbacks);
  if (outer) {
    rv = outer->AsyncOnChannelRedirect(aOldChannel, aNewChannel, aFlags, this);
    if (NS_FAILED(rv)) {
        aOldChannel->Cancel(rv); // is this necessary...?
        mRedirectCallback = nullptr;
        mOldRedirectChannel = nullptr;
        mNewRedirectChannel = nullptr;
    }
    return rv;  
  }

  (void) OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}

NS_IMETHODIMP
nsCORSListenerProxy::OnRedirectVerifyCallback(nsresult result)
{
  NS_ASSERTION(mRedirectCallback, "mRedirectCallback not set in callback");
  NS_ASSERTION(mOldRedirectChannel, "mOldRedirectChannel not set in callback");
  NS_ASSERTION(mNewRedirectChannel, "mNewRedirectChannel not set in callback");

  if (NS_SUCCEEDED(result)) {
      nsresult rv = UpdateChannel(mNewRedirectChannel);
      if (NS_FAILED(rv)) {
          NS_WARNING("nsCORSListenerProxy::OnRedirectVerifyCallback: "
                     "UpdateChannel() returned failure");
      }
      result = rv;
  }

  if (NS_FAILED(result)) {
    mOldRedirectChannel->Cancel(result);
  }

  mOldRedirectChannel = nullptr;
  mNewRedirectChannel = nullptr;
  mRedirectCallback->OnRedirectVerifyCallback(result);
  mRedirectCallback   = nullptr;
  return NS_OK;
}

nsresult
nsCORSListenerProxy::UpdateChannel(nsIChannel* aChannel, bool aAllowDataURI)
{
  nsCOMPtr<nsIURI> uri, originalURI;
  nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aChannel->GetOriginalURI(getter_AddRefs(originalURI));
  NS_ENSURE_SUCCESS(rv, rv);

  // exempt data URIs from the same origin check.
  if (aAllowDataURI && originalURI == uri) {
    bool dataScheme = false;
    rv = uri->SchemeIs("data", &dataScheme);
    NS_ENSURE_SUCCESS(rv, rv);
    if (dataScheme) {
      return NS_OK;
    }
  }

  // Check that the uri is ok to load
  rv = nsContentUtils::GetSecurityManager()->
    CheckLoadURIWithPrincipal(mRequestingPrincipal, uri,
                              nsIScriptSecurityManager::STANDARD);
  NS_ENSURE_SUCCESS(rv, rv);

  if (originalURI != uri) {
    rv = nsContentUtils::GetSecurityManager()->
      CheckLoadURIWithPrincipal(mRequestingPrincipal, originalURI,
                                nsIScriptSecurityManager::STANDARD);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!mHasBeenCrossSite &&
      NS_SUCCEEDED(mRequestingPrincipal->CheckMayLoad(uri, false, false)) &&
      (originalURI == uri ||
       NS_SUCCEEDED(mRequestingPrincipal->CheckMayLoad(originalURI,
                                                       false, false)))) {
    return NS_OK;
  }

  // It's a cross site load
  mHasBeenCrossSite = true;

  nsCString userpass;
  uri->GetUserPass(userpass);
  NS_ENSURE_TRUE(userpass.IsEmpty(), NS_ERROR_DOM_BAD_URI);

  // Add the Origin header
  nsAutoCString origin;
  rv = nsContentUtils::GetASCIIOrigin(mOriginHeaderPrincipal, origin);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHttpChannel> http = do_QueryInterface(aChannel);
  NS_ENSURE_TRUE(http, NS_ERROR_FAILURE);

  rv = http->SetRequestHeader(NS_LITERAL_CSTRING("Origin"), origin, false);
  NS_ENSURE_SUCCESS(rv, rv);

  // Add preflight headers if this is a preflight request
  if (mIsPreflight) {
    rv = http->
      SetRequestHeader(NS_LITERAL_CSTRING("Access-Control-Request-Method"),
                       mPreflightMethod, false);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!mPreflightHeaders.IsEmpty()) {
      nsAutoCString headers;
      for (uint32_t i = 0; i < mPreflightHeaders.Length(); ++i) {
        if (i != 0) {
          headers += ',';
        }
        headers += mPreflightHeaders[i];
      }
      rv = http->
        SetRequestHeader(NS_LITERAL_CSTRING("Access-Control-Request-Headers"),
                         headers, false);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // Make cookie-less if needed
  if (mIsPreflight || !mWithCredentials) {
    nsLoadFlags flags;
    rv = http->GetLoadFlags(&flags);
    NS_ENSURE_SUCCESS(rv, rv);

    flags |= nsIRequest::LOAD_ANONYMOUS;
    rv = http->SetLoadFlags(flags);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

//////////////////////////////////////////////////////////////////////////
// Preflight proxy

// Class used as streamlistener and notification callback when
// doing the initial OPTIONS request for a CORS check
class nsCORSPreflightListener MOZ_FINAL : public nsIStreamListener,
                                          public nsIInterfaceRequestor,
                                          public nsIChannelEventSink
{
public:
  nsCORSPreflightListener(nsIChannel* aOuterChannel,
                          nsIStreamListener* aOuterListener,
                          nsISupports* aOuterContext,
                          nsIPrincipal* aReferrerPrincipal,
                          const nsACString& aRequestMethod,
                          bool aWithCredentials)
   : mOuterChannel(aOuterChannel), mOuterListener(aOuterListener),
     mOuterContext(aOuterContext), mReferrerPrincipal(aReferrerPrincipal),
     mRequestMethod(aRequestMethod), mWithCredentials(aWithCredentials)
  { }

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSICHANNELEVENTSINK

private:
  ~nsCORSPreflightListener() {}

  void AddResultToCache(nsIRequest* aRequest);

  nsCOMPtr<nsIChannel> mOuterChannel;
  nsCOMPtr<nsIStreamListener> mOuterListener;
  nsCOMPtr<nsISupports> mOuterContext;
  nsCOMPtr<nsIPrincipal> mReferrerPrincipal;
  nsCString mRequestMethod;
  bool mWithCredentials;
};

NS_IMPL_ISUPPORTS(nsCORSPreflightListener, nsIStreamListener,
                  nsIRequestObserver, nsIInterfaceRequestor,
                  nsIChannelEventSink)

void
nsCORSPreflightListener::AddResultToCache(nsIRequest *aRequest)
{
  nsCOMPtr<nsIHttpChannel> http = do_QueryInterface(aRequest);
  NS_ASSERTION(http, "Request was not http");

  // The "Access-Control-Max-Age" header should return an age in seconds.
  nsAutoCString headerVal;
  http->GetResponseHeader(NS_LITERAL_CSTRING("Access-Control-Max-Age"),
                          headerVal);
  if (headerVal.IsEmpty()) {
    return;
  }

  // Sanitize the string. We only allow 'delta-seconds' as specified by
  // http://dev.w3.org/2006/waf/access-control (digits 0-9 with no leading or
  // trailing non-whitespace characters).
  uint32_t age = 0;
  nsCSubstring::const_char_iterator iter, end;
  headerVal.BeginReading(iter);
  headerVal.EndReading(end);
  while (iter != end) {
    if (*iter < '0' || *iter > '9') {
      return;
    }
    age = age * 10 + (*iter - '0');
    // Cap at 24 hours. This also avoids overflow
    age = std::min(age, 86400U);
    ++iter;
  }

  if (!age || !EnsurePreflightCache()) {
    return;
  }


  // String seems fine, go ahead and cache.
  // Note that we have already checked that these headers follow the correct
  // syntax.

  nsCOMPtr<nsIURI> uri;
  NS_GetFinalChannelURI(http, getter_AddRefs(uri));

  TimeStamp expirationTime = TimeStamp::NowLoRes() + TimeDuration::FromSeconds(age);

  nsPreflightCache::CacheEntry* entry =
    sPreflightCache->GetEntry(uri, mReferrerPrincipal, mWithCredentials,
                              true);
  if (!entry) {
    return;
  }

  // The "Access-Control-Allow-Methods" header contains a comma separated
  // list of method names.
  http->GetResponseHeader(NS_LITERAL_CSTRING("Access-Control-Allow-Methods"),
                          headerVal);

  nsCCharSeparatedTokenizer methods(headerVal, ',');
  while(methods.hasMoreTokens()) {
    const nsDependentCSubstring& method = methods.nextToken();
    if (method.IsEmpty()) {
      continue;
    }
    uint32_t i;
    for (i = 0; i < entry->mMethods.Length(); ++i) {
      if (entry->mMethods[i].token.Equals(method)) {
        entry->mMethods[i].expirationTime = expirationTime;
        break;
      }
    }
    if (i == entry->mMethods.Length()) {
      nsPreflightCache::TokenTime* newMethod =
        entry->mMethods.AppendElement();
      if (!newMethod) {
        return;
      }

      newMethod->token = method;
      newMethod->expirationTime = expirationTime;
    }
  }

  // The "Access-Control-Allow-Headers" header contains a comma separated
  // list of method names.
  http->GetResponseHeader(NS_LITERAL_CSTRING("Access-Control-Allow-Headers"),
                          headerVal);

  nsCCharSeparatedTokenizer headers(headerVal, ',');
  while(headers.hasMoreTokens()) {
    const nsDependentCSubstring& header = headers.nextToken();
    if (header.IsEmpty()) {
      continue;
    }
    uint32_t i;
    for (i = 0; i < entry->mHeaders.Length(); ++i) {
      if (entry->mHeaders[i].token.Equals(header)) {
        entry->mHeaders[i].expirationTime = expirationTime;
        break;
      }
    }
    if (i == entry->mHeaders.Length()) {
      nsPreflightCache::TokenTime* newHeader =
        entry->mHeaders.AppendElement();
      if (!newHeader) {
        return;
      }

      newHeader->token = header;
      newHeader->expirationTime = expirationTime;
    }
  }
}

NS_IMETHODIMP
nsCORSPreflightListener::OnStartRequest(nsIRequest *aRequest,
                                        nsISupports *aContext)
{
  nsresult status;
  nsresult rv = aRequest->GetStatus(&status);

  if (NS_SUCCEEDED(rv)) {
    rv = status;
  }

  if (NS_SUCCEEDED(rv)) {
    // Everything worked, try to cache and then fire off the actual request.
    AddResultToCache(aRequest);

    rv = mOuterChannel->AsyncOpen(mOuterListener, mOuterContext);
  }

  if (NS_FAILED(rv)) {
    mOuterChannel->Cancel(rv);
    mOuterListener->OnStartRequest(mOuterChannel, mOuterContext);
    mOuterListener->OnStopRequest(mOuterChannel, mOuterContext, rv);
    
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCORSPreflightListener::OnStopRequest(nsIRequest *aRequest,
                                       nsISupports *aContext,
                                       nsresult aStatus)
{
  mOuterChannel = nullptr;
  mOuterListener = nullptr;
  mOuterContext = nullptr;
  return NS_OK;
}

/** nsIStreamListener methods **/

NS_IMETHODIMP
nsCORSPreflightListener::OnDataAvailable(nsIRequest *aRequest,
                                         nsISupports *ctxt,
                                         nsIInputStream *inStr,
                                         uint64_t sourceOffset,
                                         uint32_t count)
{
  uint32_t totalRead;
  return inStr->ReadSegments(NS_DiscardSegment, nullptr, count, &totalRead);
}

NS_IMETHODIMP
nsCORSPreflightListener::AsyncOnChannelRedirect(nsIChannel *aOldChannel,
                                                nsIChannel *aNewChannel,
                                                uint32_t aFlags,
                                                nsIAsyncVerifyRedirectCallback *callback)
{
  // Only internal redirects allowed for now.
  if (!NS_IsInternalSameURIRedirect(aOldChannel, aNewChannel, aFlags))
    return NS_ERROR_DOM_BAD_URI;

  callback->OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}

NS_IMETHODIMP
nsCORSPreflightListener::GetInterface(const nsIID & aIID, void **aResult)
{
  return QueryInterface(aIID, aResult);
}


nsresult
NS_StartCORSPreflight(nsIChannel* aRequestChannel,
                      nsIStreamListener* aListener,
                      nsIPrincipal* aPrincipal,
                      bool aWithCredentials,
                      nsTArray<nsCString>& aUnsafeHeaders,
                      nsIChannel** aPreflightChannel)
{
  *aPreflightChannel = nullptr;

  nsAutoCString method;
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aRequestChannel));
  NS_ENSURE_TRUE(httpChannel, NS_ERROR_UNEXPECTED);
  httpChannel->GetRequestMethod(method);

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_GetFinalChannelURI(aRequestChannel, getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsPreflightCache::CacheEntry* entry =
    sPreflightCache ?
    sPreflightCache->GetEntry(uri, aPrincipal, aWithCredentials, false) :
    nullptr;

  if (entry && entry->CheckRequest(method, aUnsafeHeaders)) {
    // We have a cached preflight result, just start the original channel
    return aRequestChannel->AsyncOpen(aListener, nullptr);
  }

  // Either it wasn't cached or the cached result has expired. Build a
  // channel for the OPTIONS request.

  nsCOMPtr<nsILoadGroup> loadGroup;
  rv = aRequestChannel->GetLoadGroup(getter_AddRefs(loadGroup));
  NS_ENSURE_SUCCESS(rv, rv);

  nsLoadFlags loadFlags;
  rv = aRequestChannel->GetLoadFlags(&loadFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> preflightChannel;
  rv = NS_NewChannel(getter_AddRefs(preflightChannel), uri, nullptr,
                     loadGroup, nullptr, loadFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHttpChannel> preHttp = do_QueryInterface(preflightChannel);
  NS_ASSERTION(preHttp, "Failed to QI to nsIHttpChannel!");

  rv = preHttp->SetRequestMethod(NS_LITERAL_CSTRING("OPTIONS"));
  NS_ENSURE_SUCCESS(rv, rv);
  
  // Set up listener which will start the original channel
  nsCOMPtr<nsIStreamListener> preflightListener =
    new nsCORSPreflightListener(aRequestChannel, aListener, nullptr, aPrincipal,
                                method, aWithCredentials);
  NS_ENSURE_TRUE(preflightListener, NS_ERROR_OUT_OF_MEMORY);

  nsRefPtr<nsCORSListenerProxy> corsListener =
    new nsCORSListenerProxy(preflightListener, aPrincipal,
                            aWithCredentials, method,
                            aUnsafeHeaders);
  rv = corsListener->Init(preflightChannel);
  NS_ENSURE_SUCCESS(rv, rv);
  preflightListener = corsListener;

  // Start preflight
  rv = preflightChannel->AsyncOpen(preflightListener, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);
  
  // Return newly created preflight channel
  preflightChannel.forget(aPreflightChannel);

  return NS_OK;
}

