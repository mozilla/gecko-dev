/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIOService_h__
#define nsIOService_h__

#include "nsStringFwd.h"
#include "nsIIOService2.h"
#include "nsTArray.h"
#include "nsCOMPtr.h"
#include "nsWeakPtr.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsINetUtil.h"
#include "nsIChannelEventSink.h"
#include "nsCategoryCache.h"
#include "nsISpeculativeConnect.h"
#include "nsDataHashtable.h"
#include "mozilla/Attributes.h"

#define NS_N(x) (sizeof(x)/sizeof(*x))

// We don't want to expose this observer topic.
// Intended internal use only for remoting offline/inline events.
// See Bug 552829
#define NS_IPC_IOSERVICE_SET_OFFLINE_TOPIC "ipc:network:set-offline"
#define NS_IPC_IOSERVICE_SET_CONNECTIVITY_TOPIC "ipc:network:set-connectivity"

static const char gScheme[][sizeof("resource")] =
    {"chrome", "file", "http", "https", "jar", "data", "resource"};

class nsAsyncRedirectVerifyHelper;
class nsINetworkLinkService;
class nsIPrefBranch;
class nsIProtocolProxyService2;
class nsIProxyInfo;
class nsPIDNSService;
class nsPISocketTransportService;

namespace mozilla {
namespace net {
    class NeckoChild;
} // namespace net
} // namespace mozilla

class nsIOService final : public nsIIOService2
                        , public nsIObserver
                        , public nsINetUtil
                        , public nsISpeculativeConnect
                        , public nsSupportsWeakReference
                        , public nsIIOServiceInternal
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIIOSERVICE
    NS_DECL_NSIIOSERVICE2
    NS_DECL_NSIOBSERVER
    NS_DECL_NSINETUTIL
    NS_DECL_NSISPECULATIVECONNECT
    NS_DECL_NSIIOSERVICEINTERNAL

    // Gets the singleton instance of the IO Service, creating it as needed
    // Returns nullptr on out of memory or failure to initialize.
    // Returns an addrefed pointer.
    static nsIOService* GetInstance();

    nsresult Init();
    nsresult NewURI(const char* aSpec, nsIURI* aBaseURI,
                                nsIURI* *result,
                                nsIProtocolHandler* *hdlrResult);

    // Called by channels before a redirect happens. This notifies the global
    // redirect observers.
    nsresult AsyncOnChannelRedirect(nsIChannel* oldChan, nsIChannel* newChan,
                                    uint32_t flags,
                                    nsAsyncRedirectVerifyHelper *helper);

    bool IsOffline() { return mOffline; }
    bool IsShutdown() { return mShutdown; }
    bool IsLinkUp();

    // Should only be called from NeckoChild. Use SetAppOffline instead.
    void SetAppOfflineInternal(uint32_t appId, int32_t status);

private:
    // These shouldn't be called directly:
    // - construct using GetInstance
    // - destroy using Release
    nsIOService();
    ~nsIOService();
    nsresult SetConnectivityInternal(bool aConnectivity);

    nsresult OnNetworkLinkEvent(const char *data);

    nsresult GetCachedProtocolHandler(const char *scheme,
                                                  nsIProtocolHandler* *hdlrResult,
                                                  uint32_t start=0,
                                                  uint32_t end=0);
    nsresult CacheProtocolHandler(const char *scheme,
                                              nsIProtocolHandler* hdlr);

    // Prefs wrangling
    void PrefsChanged(nsIPrefBranch *prefs, const char *pref = nullptr);
    void GetPrefBranch(nsIPrefBranch **);
    void ParsePortList(nsIPrefBranch *prefBranch, const char *pref, bool remove);

    nsresult InitializeSocketTransportService();
    nsresult InitializeNetworkLinkService();

    // consolidated helper function
    void LookupProxyInfo(nsIURI *aURI, nsIURI *aProxyURI, uint32_t aProxyFlags,
                         nsCString *aScheme, nsIProxyInfo **outPI);

    // notify content processes of offline status
    // 'status' must be a nsIAppOfflineInfo mode constant.
    void NotifyAppOfflineStatus(uint32_t appId, int32_t status);
    static PLDHashOperator EnumerateWifiAppsChangingState(const unsigned int &, int32_t, void*);

    nsresult NewChannelFromURIWithProxyFlagsInternal(nsIURI* aURI,
                                                     nsIURI* aProxyURI,
                                                     uint32_t aProxyFlags,
                                                     nsILoadInfo* aLoadInfo,
                                                     nsIChannel** result);

    nsresult SpeculativeConnectInternal(nsIURI *aURI,
                                        nsIInterfaceRequestor *aCallbacks,
                                        bool aAnonymous);

private:
    bool                                 mOffline;
    bool                                 mOfflineForProfileChange;
    bool                                 mManageLinkStatus;
    bool                                 mConnectivity;
    // If true, the connectivity state will be mirrored by IOService.offline
    // meaning if !mConnectivity, GetOffline() will return true
    bool                                 mOfflineMirrorsConnectivity;

    // Used to handle SetOffline() reentrancy.  See the comment in
    // SetOffline() for more details.
    bool                                 mSettingOffline;
    bool                                 mSetOfflineValue;

    bool                                 mShutdown;

    nsCOMPtr<nsPISocketTransportService> mSocketTransportService;
    nsCOMPtr<nsPIDNSService>             mDNSService;
    nsCOMPtr<nsIProtocolProxyService2>   mProxyService;
    nsCOMPtr<nsINetworkLinkService>      mNetworkLinkService;
    bool                                 mNetworkLinkServiceInitialized;

    // Cached protocol handlers
    nsWeakPtr                            mWeakHandler[NS_N(gScheme)];

    // cached categories
    nsCategoryCache<nsIChannelEventSink> mChannelEventSinks;

    nsTArray<int32_t>                    mRestrictedPortList;

    bool                                 mAutoDialEnabled;
    bool                                 mNetworkNotifyChanged;
    int32_t                              mPreviousWifiState;
    // Hashtable of (appId, nsIAppOffineInfo::mode) pairs
    // that is used especially in IsAppOffline
    nsDataHashtable<nsUint32HashKey, int32_t> mAppsOfflineStatus;

    static bool                          sTelemetryEnabled;
public:
    // Used for all default buffer sizes that necko allocates.
    static uint32_t   gDefaultSegmentSize;
    static uint32_t   gDefaultSegmentCount;
};

/**
 * This class is passed as the subject to a NotifyObservers call for the
 * "network:app-offline-status-changed" topic.
 * Observers will use the appId and mode to get the offline status of an app.
 */
class nsAppOfflineInfo : public nsIAppOfflineInfo
{
    NS_DECL_THREADSAFE_ISUPPORTS
public:
    nsAppOfflineInfo(uint32_t aAppId, int32_t aMode)
        : mAppId(aAppId), mMode(aMode)
    {
    }

    NS_IMETHODIMP GetMode(int32_t *aMode) override
    {
        *aMode = mMode;
        return NS_OK;
    }

    NS_IMETHODIMP GetAppId(uint32_t *aAppId) override
    {
        *aAppId = mAppId;
        return NS_OK;
    }

private:
    virtual ~nsAppOfflineInfo() {}

    uint32_t mAppId;
    int32_t mMode;
};

/**
 * Reference to the IO service singleton. May be null.
 */
extern nsIOService* gIOService;

#endif // nsIOService_h__
