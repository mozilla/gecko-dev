/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "prsystem.h"

#include "nsError.h"
#include "nsHttp.h"
#include "nsHttpHandler.h"
#include "nsHttpChannel.h"
#include "nsHttpAuthCache.h"
#include "nsStandardURL.h"
#include "nsIDOMWindow.h"
#include "nsIHttpChannel.h"
#include "nsIStandardURL.h"
#include "LoadContextInfo.h"
#include "nsCategoryManagerUtils.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefLocalizedString.h"
#include "nsSocketProviderService.h"
#include "nsISocketProvider.h"
#include "nsPrintfCString.h"
#include "nsCOMPtr.h"
#include "nsNetCID.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Printf.h"
#include "mozilla/Sprintf.h"
#include "nsAsyncRedirectVerifyHelper.h"
#include "nsSocketTransportService2.h"
#include "nsAlgorithm.h"
#include "ASpdySession.h"
#include "EventTokenBucket.h"
#include "Tickler.h"
#include "nsIXULAppInfo.h"
#include "nsICookieService.h"
#include "nsIObserverService.h"
#include "nsISiteSecurityService.h"
#include "nsIStreamConverterService.h"
#include "nsCRT.h"
#include "nsIMemoryReporter.h"
#include "nsIParentalControlsService.h"
#include "nsPIDOMWindow.h"
#include "nsINetworkLinkService.h"
#include "nsHttpChannelAuthProvider.h"
#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsSocketTransportService2.h"
#include "nsIOService.h"
#include "nsISupportsPrimitives.h"
#include "nsIXULRuntime.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsRFPService.h"
#include "rust-helper/src/helper.h"

#include "mozilla/net/NeckoChild.h"
#include "mozilla/net/NeckoParent.h"
#include "mozilla/net/RequestContextService.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "mozilla/BasePrincipal.h"

#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Navigator.h"
#include "mozilla/dom/network/Connection.h"

#include "nsNSSComponent.h"

#if defined(XP_UNIX)
#include <sys/utsname.h>
#endif

#if defined(XP_WIN)
#include <windows.h>
#include "mozilla/WindowsVersion.h"
#endif

#if defined(XP_MACOSX)
#include <CoreServices/CoreServices.h>
#include "nsCocoaFeatures.h"
#endif

#ifdef MOZ_TASK_TRACER
#include "GeckoTaskTracer.h"
#endif

//-----------------------------------------------------------------------------
#include "mozilla/net/HttpChannelChild.h"

#define UA_PREF_PREFIX "general.useragent."
#ifdef XP_WIN
#define UA_SPARE_PLATFORM
#endif

#define HTTP_PREF_PREFIX "network.http."
#define INTL_ACCEPT_LANGUAGES "intl.accept_languages"
#define BROWSER_PREF_PREFIX "browser.cache."
#define DONOTTRACK_HEADER_ENABLED "privacy.donottrackheader.enabled"
#define H2MANDATORY_SUITE "security.ssl3.ecdhe_rsa_aes_128_gcm_sha256"
#define TELEMETRY_ENABLED "toolkit.telemetry.enabled"
#define ALLOW_EXPERIMENTS "network.allow-experiments"
#define SAFE_HINT_HEADER_VALUE "safeHint.enabled"
#define SECURITY_PREFIX "security."

#define TCP_FAST_OPEN_ENABLE "network.tcp.tcp_fastopen_enable"
#define TCP_FAST_OPEN_FAILURE_LIMIT \
  "network.tcp.tcp_fastopen_consecutive_failure_limit"
#define TCP_FAST_OPEN_STALLS_LIMIT "network.tcp.tcp_fastopen_http_stalls_limit"
#define TCP_FAST_OPEN_STALLS_IDLE \
  "network.tcp.tcp_fastopen_http_check_for_stalls_only_if_idle_for"
#define TCP_FAST_OPEN_STALLS_TIMEOUT \
  "network.tcp.tcp_fastopen_http_stalls_timeout"

#define UA_PREF(_pref) UA_PREF_PREFIX _pref
#define HTTP_PREF(_pref) HTTP_PREF_PREFIX _pref
#define BROWSER_PREF(_pref) BROWSER_PREF_PREFIX _pref

#define NS_HTTP_PROTOCOL_FLAGS \
  (URI_STD | ALLOWS_PROXY | ALLOWS_PROXY_HTTP | URI_LOADABLE_BY_ANYONE)

//-----------------------------------------------------------------------------

using mozilla::Telemetry::LABELS_NETWORK_HTTP_REDIRECT_TO_SCHEME;

namespace mozilla {
namespace net {

LazyLogModule gHttpLog("nsHttp");

static nsresult NewURI(const nsACString &aSpec, const char *aCharset,
                       nsIURI *aBaseURI, int32_t aDefaultPort, nsIURI **aURI) {
  nsCOMPtr<nsIURI> base(aBaseURI);
  return NS_MutateURI(new nsStandardURL::Mutator())
      .Apply(NS_MutatorMethod(&nsIStandardURLMutator::Init,
                              nsIStandardURL::URLTYPE_AUTHORITY, aDefaultPort,
                              nsCString(aSpec), aCharset, base, nullptr))
      .Finalize(aURI);
}

#ifdef ANDROID
static nsCString GetDeviceModelId() {
  // Assumed to be running on the main thread
  // We need the device property in either case
  nsAutoCString deviceModelId;
  nsCOMPtr<nsIPropertyBag2> infoService =
      do_GetService("@mozilla.org/system-info;1");
  MOZ_ASSERT(infoService, "Could not find a system info service");
  nsAutoString androidDevice;
  nsresult rv = infoService->GetPropertyAsAString(NS_LITERAL_STRING("device"),
                                                  androidDevice);
  if (NS_SUCCEEDED(rv)) {
    deviceModelId = NS_LossyConvertUTF16toASCII(androidDevice);
  }
  nsAutoCString deviceString;
  rv = Preferences::GetCString(UA_PREF("device_string"), deviceString);
  if (NS_SUCCEEDED(rv)) {
    deviceString.Trim(" ", true, true);
    deviceString.ReplaceSubstring(NS_LITERAL_CSTRING("%DEVICEID%"),
                                  deviceModelId);
    return deviceString;
  }
  return deviceModelId;
}
#endif

//-----------------------------------------------------------------------------
// nsHttpHandler <public>
//-----------------------------------------------------------------------------

StaticRefPtr<nsHttpHandler> gHttpHandler;

/* static */ already_AddRefed<nsHttpHandler> nsHttpHandler::GetInstance() {
  if (!gHttpHandler) {
    gHttpHandler = new nsHttpHandler();
    DebugOnly<nsresult> rv = gHttpHandler->Init();
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    ClearOnShutdown(&gHttpHandler);
  }
  RefPtr<nsHttpHandler> httpHandler = gHttpHandler;
  return httpHandler.forget();
}

nsHttpHandler::nsHttpHandler()
    : mHttpVersion(HttpVersion::v1_1),
      mProxyHttpVersion(HttpVersion::v1_1),
      mCapabilities(NS_HTTP_ALLOW_KEEPALIVE),
      mReferrerLevel(0xff)  // by default we always send a referrer
      ,
      mSpoofReferrerSource(false),
      mHideOnionReferrerSource(false),
      mReferrerTrimmingPolicy(0),
      mReferrerXOriginTrimmingPolicy(0),
      mReferrerXOriginPolicy(0),
      mFastFallbackToIPv4(false),
      mIdleTimeout(PR_SecondsToInterval(10)),
      mSpdyTimeout(PR_SecondsToInterval(180)),
      mResponseTimeout(PR_SecondsToInterval(300)),
      mResponseTimeoutEnabled(false),
      mNetworkChangedTimeout(5000),
      mMaxRequestAttempts(6),
      mMaxRequestDelay(10),
      mIdleSynTimeout(250),
      mFallbackSynTimeout(5),
      mH2MandatorySuiteEnabled(false),
      mMaxUrgentExcessiveConns(3),
      mMaxConnections(24),
      mMaxPersistentConnectionsPerServer(2),
      mMaxPersistentConnectionsPerProxy(4),
      mThrottleEnabled(true),
      mThrottleVersion(2),
      mThrottleSuspendFor(3000),
      mThrottleResumeFor(200),
      mThrottleReadLimit(8000),
      mThrottleReadInterval(500),
      mThrottleHoldTime(600),
      mThrottleMaxTime(3000),
      mSendWindowSize(1024),
      mUrgentStartEnabled(true),
      mTailBlockingEnabled(true),
      mTailDelayQuantum(600),
      mTailDelayQuantumAfterDCL(100),
      mTailDelayMax(6000),
      mTailTotalMax(0),
      mRedirectionLimit(10),
      mPhishyUserPassLength(1),
      mQoSBits(0x00),
      mEnforceAssocReq(false),
      mLastUniqueID(NowInSeconds()),
      mSessionStartTime(0),
      mLegacyAppName("Mozilla"),
      mLegacyAppVersion("5.0"),
      mProduct("Gecko"),
      mCompatFirefoxEnabled(false),
      mUserAgentIsDirty(true),
      mAcceptLanguagesIsDirty(true),
      mPromptTempRedirect(true),
      mEnablePersistentHttpsCaching(false),
      mDoNotTrackEnabled(false),
      mSafeHintEnabled(false),
      mParentalControlEnabled(false),
      mHandlerActive(false),
      mTelemetryEnabled(false),
      mAllowExperiments(true),
      mDebugObservations(false),
      mEnableSpdy(false),
      mHttp2Enabled(true),
      mUseH2Deps(true),
      mEnforceHttp2TlsProfile(true),
      mCoalesceSpdy(true),
      mSpdyPersistentSettings(false),
      mAllowPush(true),
      mEnableAltSvc(false),
      mEnableAltSvcOE(false),
      mEnableOriginExtension(false),
      mEnableH2Websockets(true),
      mDumpHpackTables(false),
      mSpdySendingChunkSize(ASpdySession::kSendingChunkSize),
      mSpdySendBufferSize(ASpdySession::kTCPSendBufferSize),
      mSpdyPushAllowance(131072)  // match default pref
      ,
      mSpdyPullAllowance(ASpdySession::kInitialRwin),
      mDefaultSpdyConcurrent(ASpdySession::kDefaultMaxConcurrent),
      mSpdyPingThreshold(PR_SecondsToInterval(58)),
      mSpdyPingTimeout(PR_SecondsToInterval(8)),
      mConnectTimeout(90000),
      mTLSHandshakeTimeout(30000),
      mParallelSpeculativeConnectLimit(6),
      mSpeculativeConnectEnabled(true),
      mRequestTokenBucketEnabled(true),
      mRequestTokenBucketMinParallelism(6),
      mRequestTokenBucketHz(100),
      mRequestTokenBucketBurst(32),
      mCriticalRequestPrioritization(true),
      mTCPKeepaliveShortLivedEnabled(false),
      mTCPKeepaliveShortLivedTimeS(60),
      mTCPKeepaliveShortLivedIdleTimeS(10),
      mTCPKeepaliveLongLivedEnabled(false),
      mTCPKeepaliveLongLivedIdleTimeS(600),
      mEnforceH1Framing(FRAMECHECK_BARELY),
      mDefaultHpackBuffer(4096),
      mMaxHttpResponseHeaderSize(393216),
      mFocusedWindowTransactionRatio(0.9f),
      mUseFastOpen(true),
      mFastOpenConsecutiveFailureLimit(5),
      mFastOpenConsecutiveFailureCounter(0),
      mFastOpenStallsLimit(3),
      mFastOpenStallsCounter(0),
      mFastOpenStallsIdleTime(10),
      mFastOpenStallsTimeout(20),
      mActiveTabPriority(true),
      mProcessId(0),
      mNextChannelId(1),
      mLastActiveTabLoadOptimizationLock(
          "nsHttpConnectionMgr::LastActiveTabLoadOptimization") {
  LOG(("Creating nsHttpHandler [this=%p].\n", this));

  mUserAgentOverride.SetIsVoid(true);

  MOZ_ASSERT(!gHttpHandler, "HTTP handler already created!");

  nsCOMPtr<nsIXULRuntime> runtime = do_GetService("@mozilla.org/xre/runtime;1");
  if (runtime) {
    runtime->GetProcessID(&mProcessId);
  }
  SetFastOpenOSSupport();
}

void nsHttpHandler::SetFastOpenOSSupport() {
  mFastOpenSupported = false;
#if !defined(XP_WIN) && !defined(XP_LINUX) && !defined(ANDROID) && \
    !defined(HAS_CONNECTX)
  return;
#elif defined(XP_WIN)
  mFastOpenSupported = IsWindows10BuildOrLater(16299);

  if (mFastOpenSupported) {
    // We have some problems with lavasoft software and tcp fast open.
    if (GetModuleHandleW(L"pmls64.dll") || GetModuleHandleW(L"rlls64.dll")) {
      mFastOpenSupported = false;
    }
  }
#else

  nsAutoCString version;
  nsresult rv;
#ifdef ANDROID
  nsCOMPtr<nsIPropertyBag2> infoService =
      do_GetService("@mozilla.org/system-info;1");
  MOZ_ASSERT(infoService, "Could not find a system info service");
  rv = infoService->GetPropertyAsACString(NS_LITERAL_STRING("sdk_version"),
                                          version);
#else
  char buf[SYS_INFO_BUFFER_LENGTH];
  if (PR_GetSystemInfo(PR_SI_RELEASE, buf, sizeof(buf)) == PR_SUCCESS) {
    version = buf;
    rv = NS_OK;
  } else {
    rv = NS_ERROR_FAILURE;
  }
#endif

  LOG(("nsHttpHandler::SetFastOpenOSSupport version %s", version.get()));

  if (NS_SUCCEEDED(rv)) {
    // set min version minus 1.
#if XP_MACOSX
    int min_version[] = {17, 5};  // High Sierra 10.13.4
#elif ANDROID
    int min_version[] = {4, 4};
#elif XP_LINUX
    int min_version[] = {3, 6};
#endif
    int inx = 0;
    nsCCharSeparatedTokenizer tokenizer(version, '.');
    while ((inx < 2) && tokenizer.hasMoreTokens()) {
      nsAutoCString token(tokenizer.nextToken());
      const char *nondigit = NS_strspnp("0123456789", token.get());
      if (nondigit && *nondigit) {
        break;
      }
      nsresult rv;
      int32_t ver = token.ToInteger(&rv);
      if (NS_FAILED(rv)) {
        break;
      }
      if (ver > min_version[inx]) {
        mFastOpenSupported = true;
        break;
      } else if (ver == min_version[inx] && inx == 1) {
        mFastOpenSupported = true;
      } else if (ver < min_version[inx]) {
        break;
      }
      inx++;
    }
  }
#endif
  LOG(("nsHttpHandler::SetFastOpenOSSupport %s supported.\n",
       mFastOpenSupported ? "" : "not"));
}

void nsHttpHandler::EnsureUAOverridesInit() {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv;
  nsCOMPtr<nsISupports> bootstrapper =
      do_GetService("@mozilla.org/network/ua-overrides-bootstrapper;1", &rv);
  MOZ_ASSERT(bootstrapper);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

nsHttpHandler::~nsHttpHandler() {
  LOG(("Deleting nsHttpHandler [this=%p]\n", this));

  // make sure the connection manager is shutdown
  if (mConnMgr) {
    nsresult rv = mConnMgr->Shutdown();
    if (NS_FAILED(rv)) {
      LOG(
          ("nsHttpHandler [this=%p] "
           "failed to shutdown connection manager (%08x)\n",
           this, static_cast<uint32_t>(rv)));
    }
    mConnMgr = nullptr;
  }

  // Note: don't call NeckoChild::DestroyNeckoChild() here, as it's too late
  // and it'll segfault.  NeckoChild will get cleaned up by process exit.

  nsHttp::DestroyAtomTable();
}

static const char *gCallbackPrefs[] = {
    HTTP_PREF_PREFIX,
    UA_PREF_PREFIX,
    INTL_ACCEPT_LANGUAGES,
    BROWSER_PREF("disk_cache_ssl"),
    DONOTTRACK_HEADER_ENABLED,
    TELEMETRY_ENABLED,
    H2MANDATORY_SUITE,
    HTTP_PREF("tcp_keepalive.short_lived_connections"),
    HTTP_PREF("tcp_keepalive.long_lived_connections"),
    SAFE_HINT_HEADER_VALUE,
    SECURITY_PREFIX,
    TCP_FAST_OPEN_ENABLE,
    TCP_FAST_OPEN_FAILURE_LIMIT,
    TCP_FAST_OPEN_STALLS_LIMIT,
    TCP_FAST_OPEN_STALLS_IDLE,
    TCP_FAST_OPEN_STALLS_TIMEOUT,
    nullptr,
};

nsresult nsHttpHandler::Init() {
  nsresult rv;

  LOG(("nsHttpHandler::Init\n"));
  MOZ_ASSERT(NS_IsMainThread());

  rv = nsHttp::CreateAtomTable();
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIIOService> service = do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    NS_WARNING("unable to continue without io service");
    return rv;
  }
  mIOService = new nsMainThreadPtrHolder<nsIIOService>(
      "nsHttpHandler::mIOService", service);

  if (IsNeckoChild()) NeckoChild::InitNeckoChild();

  InitUserAgentComponents();

  // This perference is only used in parent process.
  if (!IsNeckoChild()) {
    mActiveTabPriority =
        Preferences::GetBool(HTTP_PREF("active_tab_priority"), true);
  }

  // monitor some preference changes
  Preferences::RegisterPrefixCallbacks(
      PREF_CHANGE_METHOD(nsHttpHandler::PrefsChanged), gCallbackPrefs, this);
  PrefsChanged(nullptr);

  mMisc.AssignLiteral("rv:" MOZILLA_UAVERSION);

  mCompatFirefox.AssignLiteral("Firefox/" MOZILLA_UAVERSION);

  nsCOMPtr<nsIXULAppInfo> appInfo =
      do_GetService("@mozilla.org/xre/app-info;1");

  mAppName.AssignLiteral(MOZ_APP_UA_NAME);
  if (mAppName.Length() == 0 && appInfo) {
    // Try to get the UA name from appInfo, falling back to the name
    appInfo->GetUAName(mAppName);
    if (mAppName.Length() == 0) {
      appInfo->GetName(mAppName);
    }
    appInfo->GetVersion(mAppVersion);
    mAppName.StripChars(R"( ()<>@,;:\"/[]?={})");
  } else {
    mAppVersion.AssignLiteral(MOZ_APP_UA_VERSION);
  }

  // Generating the spoofed User Agent for fingerprinting resistance.
  rv = nsRFPService::GetSpoofedUserAgent(mSpoofedUserAgent);
  if (NS_FAILED(rv)) {
    // Empty mSpoofedUserAgent to make sure the unsuccessful spoofed UA string
    // will not be used anywhere.
    mSpoofedUserAgent.Truncate();
  }

  mSessionStartTime = NowInSeconds();
  mHandlerActive = true;

  rv = InitConnectionMgr();
  if (NS_FAILED(rv)) return rv;

  mRequestContextService = RequestContextService::GetOrCreate();

#if defined(ANDROID)
  mProductSub.AssignLiteral(MOZILLA_UAVERSION);
#else
  mProductSub.AssignLiteral(LEGACY_UA_GECKO_TRAIL);
#endif

#if DEBUG
  // dump user agent prefs
  LOG(("> legacy-app-name = %s\n", mLegacyAppName.get()));
  LOG(("> legacy-app-version = %s\n", mLegacyAppVersion.get()));
  LOG(("> platform = %s\n", mPlatform.get()));
  LOG(("> oscpu = %s\n", mOscpu.get()));
  LOG(("> misc = %s\n", mMisc.get()));
  LOG(("> product = %s\n", mProduct.get()));
  LOG(("> product-sub = %s\n", mProductSub.get()));
  LOG(("> app-name = %s\n", mAppName.get()));
  LOG(("> app-version = %s\n", mAppVersion.get()));
  LOG(("> compat-firefox = %s\n", mCompatFirefox.get()));
  LOG(("> user-agent = %s\n", UserAgent().get()));
#endif

  // Startup the http category
  // Bring alive the objects in the http-protocol-startup category
  NS_CreateServicesFromCategory(
      NS_HTTP_STARTUP_CATEGORY,
      static_cast<nsISupports *>(static_cast<void *>(this)),
      NS_HTTP_STARTUP_TOPIC);

  nsCOMPtr<nsIObserverService> obsService = services::GetObserverService();
  if (obsService) {
    // register the handler object as a weak callback as we don't need to worry
    // about shutdown ordering.
    obsService->AddObserver(this, "profile-change-net-teardown", true);
    obsService->AddObserver(this, "profile-change-net-restore", true);
    obsService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, true);
    obsService->AddObserver(this, "net:clear-active-logins", true);
    obsService->AddObserver(this, "net:prune-dead-connections", true);
    // Sent by the TorButton add-on in the Tor Browser
    obsService->AddObserver(this, "net:prune-all-connections", true);
    obsService->AddObserver(this, "net:cancel-all-connections", true);
    obsService->AddObserver(this, "last-pb-context-exited", true);
    obsService->AddObserver(this, "browser:purge-session-history", true);
    obsService->AddObserver(this, NS_NETWORK_LINK_TOPIC, true);
    obsService->AddObserver(this, "application-background", true);
    obsService->AddObserver(this, "psm:user-certificate-added", true);
    obsService->AddObserver(this, "psm:user-certificate-deleted", true);
    obsService->AddObserver(this, "intl:app-locales-changed", true);

    if (!IsNeckoChild()) {
      obsService->AddObserver(
          this, "net:current-toplevel-outer-content-windowid", true);
    }

    if (mFastOpenSupported) {
      obsService->AddObserver(this, "captive-portal-login", true);
      obsService->AddObserver(this, "captive-portal-login-success", true);
    }

    // disabled as its a nop right now
    // obsService->AddObserver(this, "net:failed-to-process-uri-content", true);
  }

  MakeNewRequestTokenBucket();
  mWifiTickler = new Tickler();
  if (NS_FAILED(mWifiTickler->Init())) mWifiTickler = nullptr;

  nsCOMPtr<nsIParentalControlsService> pc =
      do_CreateInstance("@mozilla.org/parental-controls-service;1");
  if (pc) {
    pc->GetParentalControlsEnabled(&mParentalControlEnabled);
  }
  return NS_OK;
}

void nsHttpHandler::MakeNewRequestTokenBucket() {
  LOG(("nsHttpHandler::MakeNewRequestTokenBucket this=%p child=%d\n", this,
       IsNeckoChild()));
  if (!mConnMgr || IsNeckoChild()) {
    return;
  }
  RefPtr<EventTokenBucket> tokenBucket =
      new EventTokenBucket(RequestTokenBucketHz(), RequestTokenBucketBurst());
  // NOTE The thread or socket may be gone already.
  nsresult rv = mConnMgr->UpdateRequestTokenBucket(tokenBucket);
  if (NS_FAILED(rv)) {
    LOG(("    failed to update request token bucket\n"));
  }
}

nsresult nsHttpHandler::InitConnectionMgr() {
  // Init ConnectionManager only on parent!
  if (IsNeckoChild()) {
    return NS_OK;
  }

  nsresult rv;

  if (!mConnMgr) {
    mConnMgr = new nsHttpConnectionMgr();
  }

  rv = mConnMgr->Init(
      mMaxUrgentExcessiveConns, mMaxConnections,
      mMaxPersistentConnectionsPerServer, mMaxPersistentConnectionsPerProxy,
      mMaxRequestDelay, mThrottleEnabled, mThrottleVersion, mThrottleSuspendFor,
      mThrottleResumeFor, mThrottleReadLimit, mThrottleReadInterval,
      mThrottleHoldTime, mThrottleMaxTime);
  return rv;
}

nsresult nsHttpHandler::AddStandardRequestHeaders(nsHttpRequestHead *request,
                                                  bool isSecure) {
  nsresult rv;

  // Add the "User-Agent" header
  rv = request->SetHeader(nsHttp::User_Agent, UserAgent(), false,
                          nsHttpHeaderArray::eVarietyRequestDefault);
  if (NS_FAILED(rv)) return rv;

  // MIME based content negotiation lives!
  // Add the "Accept" header.  Note, this is set as an override because the
  // service worker expects to see it.  The other "default" headers are
  // hidden from service worker interception.
  rv = request->SetHeader(nsHttp::Accept, mAccept, false,
                          nsHttpHeaderArray::eVarietyRequestOverride);
  if (NS_FAILED(rv)) return rv;

  // Add the "Accept-Language" header.  This header is also exposed to the
  // service worker.
  if (mAcceptLanguagesIsDirty) {
    rv = SetAcceptLanguages();
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  // Add the "Accept-Language" header
  if (!mAcceptLanguages.IsEmpty()) {
    rv = request->SetHeader(nsHttp::Accept_Language, mAcceptLanguages, false,
                            nsHttpHeaderArray::eVarietyRequestOverride);
    if (NS_FAILED(rv)) return rv;
  }

  // Add the "Accept-Encoding" header
  if (isSecure) {
    rv = request->SetHeader(nsHttp::Accept_Encoding, mHttpsAcceptEncodings,
                            false, nsHttpHeaderArray::eVarietyRequestDefault);
  } else {
    rv = request->SetHeader(nsHttp::Accept_Encoding, mHttpAcceptEncodings,
                            false, nsHttpHeaderArray::eVarietyRequestDefault);
  }
  if (NS_FAILED(rv)) return rv;

  // add the "Send Hint" header
  if (mSafeHintEnabled || mParentalControlEnabled) {
    rv = request->SetHeader(nsHttp::Prefer, NS_LITERAL_CSTRING("safe"), false,
                            nsHttpHeaderArray::eVarietyRequestDefault);
    if (NS_FAILED(rv)) return rv;
  }
  return NS_OK;
}

nsresult nsHttpHandler::AddConnectionHeader(nsHttpRequestHead *request,
                                            uint32_t caps) {
  // RFC2616 section 19.6.2 states that the "Connection: keep-alive"
  // and "Keep-alive" request headers should not be sent by HTTP/1.1
  // user-agents.  But this is not a problem in practice, and the
  // alternative proxy-connection is worse. see 570283

  NS_NAMED_LITERAL_CSTRING(close, "close");
  NS_NAMED_LITERAL_CSTRING(keepAlive, "keep-alive");

  const nsLiteralCString *connectionType = &close;
  if (caps & NS_HTTP_ALLOW_KEEPALIVE) {
    connectionType = &keepAlive;
  }

  return request->SetHeader(nsHttp::Connection, *connectionType);
}

bool nsHttpHandler::IsAcceptableEncoding(const char *enc, bool isSecure) {
  if (!enc) return false;

  // we used to accept x-foo anytime foo was acceptable, but that's just
  // continuing bad behavior.. so limit it to known x-* patterns
  bool rv;
  if (isSecure) {
    rv = nsHttp::FindToken(mHttpsAcceptEncodings.get(), enc, HTTP_LWS ",") !=
         nullptr;
  } else {
    rv = nsHttp::FindToken(mHttpAcceptEncodings.get(), enc, HTTP_LWS ",") !=
         nullptr;
  }
  // gzip and deflate are inherently acceptable in modern HTTP - always
  // process them if a stream converter can also be found.
  if (!rv &&
      (!PL_strcasecmp(enc, "gzip") || !PL_strcasecmp(enc, "deflate") ||
       !PL_strcasecmp(enc, "x-gzip") || !PL_strcasecmp(enc, "x-deflate"))) {
    rv = true;
  }
  LOG(("nsHttpHandler::IsAceptableEncoding %s https=%d %d\n", enc, isSecure,
       rv));
  return rv;
}

void nsHttpHandler::IncrementFastOpenConsecutiveFailureCounter() {
  LOG(
      ("nsHttpHandler::IncrementFastOpenConsecutiveFailureCounter - "
       "failed=%d failure_limit=%d",
       mFastOpenConsecutiveFailureCounter, mFastOpenConsecutiveFailureLimit));
  if (mFastOpenConsecutiveFailureCounter < mFastOpenConsecutiveFailureLimit) {
    mFastOpenConsecutiveFailureCounter++;
    if (mFastOpenConsecutiveFailureCounter ==
        mFastOpenConsecutiveFailureLimit) {
      LOG(
          ("nsHttpHandler::IncrementFastOpenConsecutiveFailureCounter - "
           "Fast open failed too many times"));
    }
  }
}

void nsHttpHandler::IncrementFastOpenStallsCounter() {
  LOG(
      ("nsHttpHandler::IncrementFastOpenStallsCounter - failed=%d "
       "failure_limit=%d",
       mFastOpenStallsCounter, mFastOpenStallsLimit));
  if (mFastOpenStallsCounter < mFastOpenStallsLimit) {
    mFastOpenStallsCounter++;
    if (mFastOpenStallsCounter == mFastOpenStallsLimit) {
      LOG(
          ("nsHttpHandler::IncrementFastOpenStallsCounter - "
           "There are too many stalls involving TFO and TLS."));
    }
  }
}

nsresult nsHttpHandler::GetStreamConverterService(
    nsIStreamConverterService **result) {
  if (!mStreamConvSvc) {
    nsresult rv;
    nsCOMPtr<nsIStreamConverterService> service =
        do_GetService(NS_STREAMCONVERTERSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    mStreamConvSvc = new nsMainThreadPtrHolder<nsIStreamConverterService>(
        "nsHttpHandler::mStreamConvSvc", service);
  }
  *result = mStreamConvSvc;
  NS_ADDREF(*result);
  return NS_OK;
}

nsISiteSecurityService *nsHttpHandler::GetSSService() {
  if (!mSSService) {
    nsCOMPtr<nsISiteSecurityService> service =
        do_GetService(NS_SSSERVICE_CONTRACTID);
    mSSService = new nsMainThreadPtrHolder<nsISiteSecurityService>(
        "nsHttpHandler::mSSService", service);
  }
  return mSSService;
}

nsICookieService *nsHttpHandler::GetCookieService() {
  if (!mCookieService) {
    nsCOMPtr<nsICookieService> service =
        do_GetService(NS_COOKIESERVICE_CONTRACTID);
    mCookieService = new nsMainThreadPtrHolder<nsICookieService>(
        "nsHttpHandler::mCookieService", service);
  }
  return mCookieService;
}

nsresult nsHttpHandler::GetIOService(nsIIOService **result) {
  NS_ENSURE_ARG_POINTER(result);

  NS_ADDREF(*result = mIOService);
  return NS_OK;
}

uint32_t nsHttpHandler::Get32BitsOfPseudoRandom() {
  // only confirm rand seeding on socket thread
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  // rand() provides different amounts of PRNG on different platforms.
  // 15 or 31 bits are common amounts.

  static_assert(RAND_MAX >= 0xfff, "RAND_MAX should be >= 12 bits");

#if RAND_MAX < 0xffffU
  return ((uint16_t)rand() << 20) | (((uint16_t)rand() & 0xfff) << 8) |
         ((uint16_t)rand() & 0xff);
#elif RAND_MAX < 0xffffffffU
  return ((uint16_t)rand() << 16) | ((uint16_t)rand() & 0xffff);
#else
  return (uint32_t)rand();
#endif
}

void nsHttpHandler::NotifyObservers(nsIHttpChannel *chan, const char *event) {
  LOG(("nsHttpHandler::NotifyObservers [chan=%p event=\"%s\"]\n", chan, event));
  nsCOMPtr<nsIObserverService> obsService = services::GetObserverService();
  if (obsService) obsService->NotifyObservers(chan, event, nullptr);
}

nsresult nsHttpHandler::AsyncOnChannelRedirect(
    nsIChannel *oldChan, nsIChannel *newChan, uint32_t flags,
    nsIEventTarget *mainThreadEventTarget) {
  MOZ_ASSERT(NS_IsMainThread() && (oldChan && newChan));

  nsCOMPtr<nsIURI> newURI;
  newChan->GetURI(getter_AddRefs(newURI));
  MOZ_ASSERT(newURI);

  nsAutoCString scheme;
  newURI->GetScheme(scheme);
  MOZ_ASSERT(!scheme.IsEmpty());

  Telemetry::AccumulateCategoricalKeyed(
      scheme, oldChan->IsDocument()
                  ? LABELS_NETWORK_HTTP_REDIRECT_TO_SCHEME::topLevel
                  : LABELS_NETWORK_HTTP_REDIRECT_TO_SCHEME::subresource);

  // TODO E10S This helper has to be initialized on the other process
  RefPtr<nsAsyncRedirectVerifyHelper> redirectCallbackHelper =
      new nsAsyncRedirectVerifyHelper();

  return redirectCallbackHelper->Init(oldChan, newChan, flags,
                                      mainThreadEventTarget);
}

/* static */ nsresult nsHttpHandler::GenerateHostPort(const nsCString &host,
                                                      int32_t port,
                                                      nsACString &hostLine) {
  return NS_GenerateHostPort(host, port, hostLine);
}

//-----------------------------------------------------------------------------
// nsHttpHandler <private>
//-----------------------------------------------------------------------------

const nsCString &nsHttpHandler::UserAgent() {
  if (nsContentUtils::ShouldResistFingerprinting() &&
      !mSpoofedUserAgent.IsEmpty()) {
    LOG(("using spoofed userAgent : %s\n", mSpoofedUserAgent.get()));
    return mSpoofedUserAgent;
  }

  if (!mUserAgentOverride.IsVoid()) {
    LOG(("using general.useragent.override : %s\n", mUserAgentOverride.get()));
    return mUserAgentOverride;
  }

  if (mUserAgentIsDirty) {
    BuildUserAgent();
    mUserAgentIsDirty = false;
  }

  return mUserAgent;
}

void nsHttpHandler::BuildUserAgent() {
  LOG(("nsHttpHandler::BuildUserAgent\n"));

  MOZ_ASSERT(!mLegacyAppName.IsEmpty() && !mLegacyAppVersion.IsEmpty(),
             "HTTP cannot send practical requests without this much");

  // preallocate to worst-case size, which should always be better
  // than if we didn't preallocate at all.
  mUserAgent.SetCapacity(mLegacyAppName.Length() + mLegacyAppVersion.Length() +
#ifndef UA_SPARE_PLATFORM
                         mPlatform.Length() +
#endif
                         mOscpu.Length() + mMisc.Length() + mProduct.Length() +
                         mProductSub.Length() + mAppName.Length() +
                         mAppVersion.Length() + mCompatFirefox.Length() +
                         mCompatDevice.Length() + mDeviceModelId.Length() + 13);

  // Application portion
  mUserAgent.Assign(mLegacyAppName);
  mUserAgent += '/';
  mUserAgent += mLegacyAppVersion;
  mUserAgent += ' ';

  // Application comment
  mUserAgent += '(';
#ifndef UA_SPARE_PLATFORM
  if (!mPlatform.IsEmpty()) {
    mUserAgent += mPlatform;
    mUserAgent.AppendLiteral("; ");
  }
#endif
  if (!mCompatDevice.IsEmpty()) {
    mUserAgent += mCompatDevice;
    mUserAgent.AppendLiteral("; ");
  } else if (!mOscpu.IsEmpty()) {
    mUserAgent += mOscpu;
    mUserAgent.AppendLiteral("; ");
  }
  if (!mDeviceModelId.IsEmpty()) {
    mUserAgent += mDeviceModelId;
    mUserAgent.AppendLiteral("; ");
  }
  mUserAgent += mMisc;
  mUserAgent += ')';

  // Product portion
  mUserAgent += ' ';
  mUserAgent += mProduct;
  mUserAgent += '/';
  mUserAgent += mProductSub;

  bool isFirefox = mAppName.EqualsLiteral("Firefox");
  if (isFirefox || mCompatFirefoxEnabled) {
    // "Firefox/x.y" (compatibility) app token
    mUserAgent += ' ';
    mUserAgent += mCompatFirefox;
  }
  if (!isFirefox) {
    // App portion
    mUserAgent += ' ';
    mUserAgent += mAppName;
    mUserAgent += '/';
    mUserAgent += mAppVersion;
  }
}

#ifdef XP_WIN
#define WNT_BASE "Windows NT %ld.%ld"
#define W64_PREFIX "; Win64"
#endif

void nsHttpHandler::InitUserAgentComponents() {
#ifndef MOZ_UA_OS_AGNOSTIC
  // Gather platform.
  mPlatform.AssignLiteral(
#if defined(ANDROID)
      "Android"
#elif defined(XP_WIN)
      "Windows"
#elif defined(XP_MACOSX)
      "Macintosh"
#elif defined(XP_UNIX)
      // We historically have always had X11 here,
      // and there seems little a webpage can sensibly do
      // based on it being something else, so use X11 for
      // backwards compatibility in all cases.
      "X11"
#endif
  );
#endif

#ifdef ANDROID
  nsCOMPtr<nsIPropertyBag2> infoService =
      do_GetService("@mozilla.org/system-info;1");
  MOZ_ASSERT(infoService, "Could not find a system info service");
  nsresult rv;
  // Add the Android version number to the Fennec platform identifier.
#if defined MOZ_WIDGET_ANDROID
#ifndef MOZ_UA_OS_AGNOSTIC  // Don't add anything to mPlatform since it's empty.
  nsAutoString androidVersion;
  rv = infoService->GetPropertyAsAString(NS_LITERAL_STRING("release_version"),
                                         androidVersion);
  if (NS_SUCCEEDED(rv)) {
    mPlatform += " ";
    // If the 2nd character is a ".", we know the major version is a single
    // digit. If we're running on a version below 4 we pretend to be on
    // Android KitKat (4.4) to work around scripts sniffing for low versions.
    if (androidVersion[1] == 46 && androidVersion[0] < 52) {
      mPlatform += "4.4";
    } else {
      mPlatform += NS_LossyConvertUTF16toASCII(androidVersion);
    }
  }
#endif
#endif
  // Add the `Mobile` or `Tablet` or `TV` token when running on device.
  bool isTablet;
  rv = infoService->GetPropertyAsBool(NS_LITERAL_STRING("tablet"), &isTablet);
  if (NS_SUCCEEDED(rv) && isTablet) {
    mCompatDevice.AssignLiteral("Tablet");
  } else {
    bool isTV;
    rv = infoService->GetPropertyAsBool(NS_LITERAL_STRING("tv"), &isTV);
    if (NS_SUCCEEDED(rv) && isTV) {
      mCompatDevice.AssignLiteral("TV");
    } else {
      mCompatDevice.AssignLiteral("Mobile");
    }
  }

  if (Preferences::GetBool(UA_PREF("use_device"), false)) {
    mDeviceModelId = mozilla::net::GetDeviceModelId();
  }
#endif  // ANDROID

#ifndef MOZ_UA_OS_AGNOSTIC
  // Gather OS/CPU.
#if defined(XP_WIN)
  OSVERSIONINFO info = {sizeof(OSVERSIONINFO)};
#pragma warning(push)
#pragma warning(disable : 4996)
  if (GetVersionEx(&info)) {
#pragma warning(pop)
    const char *format;
#if defined _M_IA64
    format = WNT_BASE W64_PREFIX "; IA64";
#elif defined _M_X64 || defined _M_AMD64
    format = WNT_BASE W64_PREFIX "; x64";
#else
    BOOL isWow64 = FALSE;
    if (!IsWow64Process(GetCurrentProcess(), &isWow64)) {
      isWow64 = FALSE;
    }
    format = isWow64 ? WNT_BASE "; WOW64" : WNT_BASE;
#endif
    SmprintfPointer buf =
        mozilla::Smprintf(format, info.dwMajorVersion, info.dwMinorVersion);
    if (buf) {
      mOscpu = buf.get();
    }
  }
#elif defined(XP_MACOSX)
#if defined(__ppc__)
  mOscpu.AssignLiteral("PPC Mac OS X");
#elif defined(__i386__) || defined(__x86_64__)
  mOscpu.AssignLiteral("Intel Mac OS X");
#endif
  SInt32 majorVersion = nsCocoaFeatures::OSXVersionMajor();
  SInt32 minorVersion = nsCocoaFeatures::OSXVersionMinor();
  mOscpu += nsPrintfCString(" %d.%d", static_cast<int>(majorVersion),
                            static_cast<int>(minorVersion));
#elif defined(XP_UNIX)
  struct utsname name;

  int ret = uname(&name);
  if (ret >= 0) {
    nsAutoCString buf;
    buf = (char *)name.sysname;

    if (strcmp(name.machine, "x86_64") == 0 &&
        sizeof(void *) == sizeof(int32_t)) {
      // We're running 32-bit code on x86_64. Make this browser
      // look like it's running on i686 hardware, but append "
      // (x86_64)" to the end of the oscpu identifier to be able
      // to differentiate this from someone running 64-bit code
      // on x86_64..

      buf += " i686 on x86_64";
    } else {
      buf += ' ';

#ifdef AIX
      // AIX uname returns machine specific info in the uname.machine
      // field and does not return the cpu type like other platforms.
      // We use the AIX version and release numbers instead.
      buf += (char *)name.version;
      buf += '.';
      buf += (char *)name.release;
#else
      buf += (char *)name.machine;
#endif
    }

    mOscpu.Assign(buf);
  }
#endif
#endif

  mUserAgentIsDirty = true;
}

uint32_t nsHttpHandler::MaxSocketCount() {
  PR_CallOnce(&nsSocketTransportService::gMaxCountInitOnce,
              nsSocketTransportService::DiscoverMaxCount);
  // Don't use the full max count because sockets can be held in
  // the persistent connection pool for a long time and that could
  // starve other users.

  uint32_t maxCount = nsSocketTransportService::gMaxCount;
  if (maxCount <= 8)
    maxCount = 1;
  else
    maxCount -= 8;

  return maxCount;
}

void nsHttpHandler::PrefsChanged(const char *pref) {
  nsresult rv = NS_OK;
  int32_t val;

  LOG(("nsHttpHandler::PrefsChanged [pref=%s]\n", pref));

#define PREF_CHANGED(p) ((pref == nullptr) || !PL_strcmp(pref, p))
#define MULTI_PREF_CHANGED(p) \
  ((pref == nullptr) || !PL_strncmp(pref, p, sizeof(p) - 1))

  // If a security pref changed, lets clear our connection pool reuse
  if (MULTI_PREF_CHANGED(SECURITY_PREFIX)) {
    LOG(("nsHttpHandler::PrefsChanged Security Pref Changed %s\n", pref));
    if (mConnMgr) {
      rv = mConnMgr->DoShiftReloadConnectionCleanup(nullptr);
      if (NS_FAILED(rv)) {
        LOG(
            ("nsHttpHandler::PrefsChanged "
             "DoShiftReloadConnectionCleanup failed (%08x)\n",
             static_cast<uint32_t>(rv)));
      }
      rv = mConnMgr->PruneDeadConnections();
      if (NS_FAILED(rv)) {
        LOG(
            ("nsHttpHandler::PrefsChanged "
             "PruneDeadConnections failed (%08x)\n",
             static_cast<uint32_t>(rv)));
      }
    }
  }

  //
  // UA components
  //

  bool cVar = false;

  if (PREF_CHANGED(UA_PREF("compatMode.firefox"))) {
    rv = Preferences::GetBool(UA_PREF("compatMode.firefox"), &cVar);
    mCompatFirefoxEnabled = (NS_SUCCEEDED(rv) && cVar);
    mUserAgentIsDirty = true;
  }

  // general.useragent.override
  if (PREF_CHANGED(UA_PREF("override"))) {
    Preferences::GetCString(UA_PREF("override"), mUserAgentOverride);
    mUserAgentIsDirty = true;
  }

#ifdef ANDROID
  // general.useragent.use_device
  if (PREF_CHANGED(UA_PREF("use_device"))) {
    if (Preferences::GetBool(UA_PREF("use_device"), false)) {
      mDeviceModelId = mozilla::net::GetDeviceModelId();
    } else {
      mDeviceModelId = EmptyCString();
    }
    mUserAgentIsDirty = true;
  }
#endif

  //
  // HTTP options
  //

  if (PREF_CHANGED(HTTP_PREF("keep-alive.timeout"))) {
    rv = Preferences::GetInt(HTTP_PREF("keep-alive.timeout"), &val);
    if (NS_SUCCEEDED(rv))
      mIdleTimeout = PR_SecondsToInterval(clamped(val, 1, 0xffff));
  }

  if (PREF_CHANGED(HTTP_PREF("request.max-attempts"))) {
    rv = Preferences::GetInt(HTTP_PREF("request.max-attempts"), &val);
    if (NS_SUCCEEDED(rv))
      mMaxRequestAttempts = (uint16_t)clamped(val, 1, 0xffff);
  }

  if (PREF_CHANGED(HTTP_PREF("request.max-start-delay"))) {
    rv = Preferences::GetInt(HTTP_PREF("request.max-start-delay"), &val);
    if (NS_SUCCEEDED(rv)) {
      mMaxRequestDelay = (uint16_t)clamped(val, 0, 0xffff);
      if (mConnMgr) {
        rv = mConnMgr->UpdateParam(nsHttpConnectionMgr::MAX_REQUEST_DELAY,
                                   mMaxRequestDelay);
        if (NS_FAILED(rv)) {
          LOG(
              ("nsHttpHandler::PrefsChanged (request.max-start-delay)"
               "UpdateParam failed (%08x)\n",
               static_cast<uint32_t>(rv)));
        }
      }
    }
  }

  if (PREF_CHANGED(HTTP_PREF("response.timeout"))) {
    rv = Preferences::GetInt(HTTP_PREF("response.timeout"), &val);
    if (NS_SUCCEEDED(rv))
      mResponseTimeout = PR_SecondsToInterval(clamped(val, 0, 0xffff));
  }

  if (PREF_CHANGED(HTTP_PREF("network-changed.timeout"))) {
    rv = Preferences::GetInt(HTTP_PREF("network-changed.timeout"), &val);
    if (NS_SUCCEEDED(rv)) mNetworkChangedTimeout = clamped(val, 1, 600) * 1000;
  }

  if (PREF_CHANGED(HTTP_PREF("max-connections"))) {
    rv = Preferences::GetInt(HTTP_PREF("max-connections"), &val);
    if (NS_SUCCEEDED(rv)) {
      mMaxConnections =
          (uint16_t)clamped((uint32_t)val, (uint32_t)1, MaxSocketCount());

      if (mConnMgr) {
        rv = mConnMgr->UpdateParam(nsHttpConnectionMgr::MAX_CONNECTIONS,
                                   mMaxConnections);
        if (NS_FAILED(rv)) {
          LOG(
              ("nsHttpHandler::PrefsChanged (max-connections)"
               "UpdateParam failed (%08x)\n",
               static_cast<uint32_t>(rv)));
        }
      }
    }
  }

  if (PREF_CHANGED(
          HTTP_PREF("max-urgent-start-excessive-connections-per-host"))) {
    rv = Preferences::GetInt(
        HTTP_PREF("max-urgent-start-excessive-connections-per-host"), &val);
    if (NS_SUCCEEDED(rv)) {
      mMaxUrgentExcessiveConns = (uint8_t)clamped(val, 1, 0xff);
      if (mConnMgr) {
        rv = mConnMgr->UpdateParam(nsHttpConnectionMgr::MAX_URGENT_START_Q,
                                   mMaxUrgentExcessiveConns);
        if (NS_FAILED(rv)) {
          LOG(
              ("nsHttpHandler::PrefsChanged "
               "(max-urgent-start-excessive-connections-per-host)"
               "UpdateParam failed (%08x)\n",
               static_cast<uint32_t>(rv)));
        }
      }
    }
  }

  if (PREF_CHANGED(HTTP_PREF("max-persistent-connections-per-server"))) {
    rv = Preferences::GetInt(HTTP_PREF("max-persistent-connections-per-server"),
                             &val);
    if (NS_SUCCEEDED(rv)) {
      mMaxPersistentConnectionsPerServer = (uint8_t)clamped(val, 1, 0xff);
      if (mConnMgr) {
        rv = mConnMgr->UpdateParam(
            nsHttpConnectionMgr::MAX_PERSISTENT_CONNECTIONS_PER_HOST,
            mMaxPersistentConnectionsPerServer);
        if (NS_FAILED(rv)) {
          LOG(
              ("nsHttpHandler::PrefsChanged "
               "(max-persistent-connections-per-server)"
               "UpdateParam failed (%08x)\n",
               static_cast<uint32_t>(rv)));
        }
      }
    }
  }

  if (PREF_CHANGED(HTTP_PREF("max-persistent-connections-per-proxy"))) {
    rv = Preferences::GetInt(HTTP_PREF("max-persistent-connections-per-proxy"),
                             &val);
    if (NS_SUCCEEDED(rv)) {
      mMaxPersistentConnectionsPerProxy = (uint8_t)clamped(val, 1, 0xff);
      if (mConnMgr) {
        rv = mConnMgr->UpdateParam(
            nsHttpConnectionMgr::MAX_PERSISTENT_CONNECTIONS_PER_PROXY,
            mMaxPersistentConnectionsPerProxy);
        if (NS_FAILED(rv)) {
          LOG(
              ("nsHttpHandler::PrefsChanged "
               "(max-persistent-connections-per-proxy)"
               "UpdateParam failed (%08x)\n",
               static_cast<uint32_t>(rv)));
        }
      }
    }
  }

  if (PREF_CHANGED(HTTP_PREF("sendRefererHeader"))) {
    rv = Preferences::GetInt(HTTP_PREF("sendRefererHeader"), &val);
    if (NS_SUCCEEDED(rv)) mReferrerLevel = (uint8_t)clamped(val, 0, 0xff);
  }

  if (PREF_CHANGED(HTTP_PREF("referer.spoofSource"))) {
    rv = Preferences::GetBool(HTTP_PREF("referer.spoofSource"), &cVar);
    if (NS_SUCCEEDED(rv)) mSpoofReferrerSource = cVar;
  }

  if (PREF_CHANGED(HTTP_PREF("referer.hideOnionSource"))) {
    rv = Preferences::GetBool(HTTP_PREF("referer.hideOnionSource"), &cVar);
    if (NS_SUCCEEDED(rv)) mHideOnionReferrerSource = cVar;
  }

  if (PREF_CHANGED(HTTP_PREF("referer.trimmingPolicy"))) {
    rv = Preferences::GetInt(HTTP_PREF("referer.trimmingPolicy"), &val);
    if (NS_SUCCEEDED(rv)) mReferrerTrimmingPolicy = (uint8_t)clamped(val, 0, 2);
  }

  if (PREF_CHANGED(HTTP_PREF("referer.XOriginTrimmingPolicy"))) {
    rv = Preferences::GetInt(HTTP_PREF("referer.XOriginTrimmingPolicy"), &val);
    if (NS_SUCCEEDED(rv))
      mReferrerXOriginTrimmingPolicy = (uint8_t)clamped(val, 0, 2);
  }

  if (PREF_CHANGED(HTTP_PREF("referer.XOriginPolicy"))) {
    rv = Preferences::GetInt(HTTP_PREF("referer.XOriginPolicy"), &val);
    if (NS_SUCCEEDED(rv))
      mReferrerXOriginPolicy = (uint8_t)clamped(val, 0, 0xff);
  }

  if (PREF_CHANGED(HTTP_PREF("redirection-limit"))) {
    rv = Preferences::GetInt(HTTP_PREF("redirection-limit"), &val);
    if (NS_SUCCEEDED(rv)) mRedirectionLimit = (uint8_t)clamped(val, 0, 0xff);
  }

  if (PREF_CHANGED(HTTP_PREF("connection-retry-timeout"))) {
    rv = Preferences::GetInt(HTTP_PREF("connection-retry-timeout"), &val);
    if (NS_SUCCEEDED(rv)) mIdleSynTimeout = (uint16_t)clamped(val, 0, 3000);
  }

  if (PREF_CHANGED(HTTP_PREF("fast-fallback-to-IPv4"))) {
    rv = Preferences::GetBool(HTTP_PREF("fast-fallback-to-IPv4"), &cVar);
    if (NS_SUCCEEDED(rv)) mFastFallbackToIPv4 = cVar;
  }

  if (PREF_CHANGED(HTTP_PREF("fallback-connection-timeout"))) {
    rv = Preferences::GetInt(HTTP_PREF("fallback-connection-timeout"), &val);
    if (NS_SUCCEEDED(rv))
      mFallbackSynTimeout = (uint16_t)clamped(val, 0, 10 * 60);
  }

  if (PREF_CHANGED(HTTP_PREF("version"))) {
    nsAutoCString httpVersion;
    Preferences::GetCString(HTTP_PREF("version"), httpVersion);
    if (!httpVersion.IsVoid()) {
      if (httpVersion.EqualsLiteral("1.1"))
        mHttpVersion = HttpVersion::v1_1;
      else if (httpVersion.EqualsLiteral("0.9"))
        mHttpVersion = HttpVersion::v0_9;
      else
        mHttpVersion = HttpVersion::v1_0;
    }
  }

  if (PREF_CHANGED(HTTP_PREF("proxy.version"))) {
    nsAutoCString httpVersion;
    Preferences::GetCString(HTTP_PREF("proxy.version"), httpVersion);
    if (!httpVersion.IsVoid()) {
      if (httpVersion.EqualsLiteral("1.1"))
        mProxyHttpVersion = HttpVersion::v1_1;
      else
        mProxyHttpVersion = HttpVersion::v1_0;
      // it does not make sense to issue a HTTP/0.9 request to a proxy server
    }
  }

  if (PREF_CHANGED(HTTP_PREF("qos"))) {
    rv = Preferences::GetInt(HTTP_PREF("qos"), &val);
    if (NS_SUCCEEDED(rv)) mQoSBits = (uint8_t)clamped(val, 0, 0xff);
  }

  if (PREF_CHANGED(HTTP_PREF("accept.default"))) {
    nsAutoCString accept;
    rv = Preferences::GetCString(HTTP_PREF("accept.default"), accept);
    if (NS_SUCCEEDED(rv)) {
      rv = SetAccept(accept.get());
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }

  if (PREF_CHANGED(HTTP_PREF("accept-encoding"))) {
    nsAutoCString acceptEncodings;
    rv = Preferences::GetCString(HTTP_PREF("accept-encoding"), acceptEncodings);
    if (NS_SUCCEEDED(rv)) {
      rv = SetAcceptEncodings(acceptEncodings.get(), false);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }

  if (PREF_CHANGED(HTTP_PREF("accept-encoding.secure"))) {
    nsAutoCString acceptEncodings;
    rv = Preferences::GetCString(HTTP_PREF("accept-encoding.secure"),
                                 acceptEncodings);
    if (NS_SUCCEEDED(rv)) {
      rv = SetAcceptEncodings(acceptEncodings.get(), true);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }

  if (PREF_CHANGED(HTTP_PREF("default-socket-type"))) {
    nsAutoCString sval;
    rv = Preferences::GetCString(HTTP_PREF("default-socket-type"), sval);
    if (NS_SUCCEEDED(rv)) {
      if (sval.IsEmpty())
        mDefaultSocketType.SetIsVoid(true);
      else {
        // verify that this socket type is actually valid
        nsCOMPtr<nsISocketProviderService> sps =
            nsSocketProviderService::GetOrCreate();
        if (sps) {
          nsCOMPtr<nsISocketProvider> sp;
          rv = sps->GetSocketProvider(sval.get(), getter_AddRefs(sp));
          if (NS_SUCCEEDED(rv)) {
            // OK, this looks like a valid socket provider.
            mDefaultSocketType.Assign(sval);
          }
        }
      }
    }
  }

  if (PREF_CHANGED(HTTP_PREF("prompt-temp-redirect"))) {
    rv = Preferences::GetBool(HTTP_PREF("prompt-temp-redirect"), &cVar);
    if (NS_SUCCEEDED(rv)) {
      mPromptTempRedirect = cVar;
    }
  }

  if (PREF_CHANGED(HTTP_PREF("assoc-req.enforce"))) {
    cVar = false;
    rv = Preferences::GetBool(HTTP_PREF("assoc-req.enforce"), &cVar);
    if (NS_SUCCEEDED(rv)) mEnforceAssocReq = cVar;
  }

  // enable Persistent caching for HTTPS - bug#205921
  if (PREF_CHANGED(BROWSER_PREF("disk_cache_ssl"))) {
    cVar = false;
    rv = Preferences::GetBool(BROWSER_PREF("disk_cache_ssl"), &cVar);
    if (NS_SUCCEEDED(rv)) mEnablePersistentHttpsCaching = cVar;
  }

  if (PREF_CHANGED(HTTP_PREF("phishy-userpass-length"))) {
    rv = Preferences::GetInt(HTTP_PREF("phishy-userpass-length"), &val);
    if (NS_SUCCEEDED(rv))
      mPhishyUserPassLength = (uint8_t)clamped(val, 0, 0xff);
  }

  if (PREF_CHANGED(HTTP_PREF("spdy.enabled"))) {
    rv = Preferences::GetBool(HTTP_PREF("spdy.enabled"), &cVar);
    if (NS_SUCCEEDED(rv)) mEnableSpdy = cVar;
  }

  if (PREF_CHANGED(HTTP_PREF("spdy.enabled.http2"))) {
    rv = Preferences::GetBool(HTTP_PREF("spdy.enabled.http2"), &cVar);
    if (NS_SUCCEEDED(rv)) mHttp2Enabled = cVar;
  }

  if (PREF_CHANGED(HTTP_PREF("spdy.enabled.deps"))) {
    rv = Preferences::GetBool(HTTP_PREF("spdy.enabled.deps"), &cVar);
    if (NS_SUCCEEDED(rv)) mUseH2Deps = cVar;
  }

  if (PREF_CHANGED(HTTP_PREF("spdy.enforce-tls-profile"))) {
    rv = Preferences::GetBool(HTTP_PREF("spdy.enforce-tls-profile"), &cVar);
    if (NS_SUCCEEDED(rv)) mEnforceHttp2TlsProfile = cVar;
  }

  if (PREF_CHANGED(HTTP_PREF("spdy.coalesce-hostnames"))) {
    rv = Preferences::GetBool(HTTP_PREF("spdy.coalesce-hostnames"), &cVar);
    if (NS_SUCCEEDED(rv)) mCoalesceSpdy = cVar;
  }

  if (PREF_CHANGED(HTTP_PREF("spdy.persistent-settings"))) {
    rv = Preferences::GetBool(HTTP_PREF("spdy.persistent-settings"), &cVar);
    if (NS_SUCCEEDED(rv)) mSpdyPersistentSettings = cVar;
  }

  if (PREF_CHANGED(HTTP_PREF("spdy.timeout"))) {
    rv = Preferences::GetInt(HTTP_PREF("spdy.timeout"), &val);
    if (NS_SUCCEEDED(rv))
      mSpdyTimeout = PR_SecondsToInterval(clamped(val, 1, 0xffff));
  }

  if (PREF_CHANGED(HTTP_PREF("spdy.chunk-size"))) {
    // keep this within http/2 ranges of 1 to 2^14-1
    rv = Preferences::GetInt(HTTP_PREF("spdy.chunk-size"), &val);
    if (NS_SUCCEEDED(rv))
      mSpdySendingChunkSize = (uint32_t)clamped(val, 1, 0x3fff);
  }

  // The amount of idle seconds on a spdy connection before initiating a
  // server ping. 0 will disable.
  if (PREF_CHANGED(HTTP_PREF("spdy.ping-threshold"))) {
    rv = Preferences::GetInt(HTTP_PREF("spdy.ping-threshold"), &val);
    if (NS_SUCCEEDED(rv))
      mSpdyPingThreshold =
          PR_SecondsToInterval((uint16_t)clamped(val, 0, 0x7fffffff));
  }

  // The amount of seconds to wait for a spdy ping response before
  // closing the session.
  if (PREF_CHANGED(HTTP_PREF("spdy.ping-timeout"))) {
    rv = Preferences::GetInt(HTTP_PREF("spdy.ping-timeout"), &val);
    if (NS_SUCCEEDED(rv))
      mSpdyPingTimeout =
          PR_SecondsToInterval((uint16_t)clamped(val, 0, 0x7fffffff));
  }

  if (PREF_CHANGED(HTTP_PREF("spdy.allow-push"))) {
    rv = Preferences::GetBool(HTTP_PREF("spdy.allow-push"), &cVar);
    if (NS_SUCCEEDED(rv)) mAllowPush = cVar;
  }

  if (PREF_CHANGED(HTTP_PREF("altsvc.enabled"))) {
    rv = Preferences::GetBool(HTTP_PREF("altsvc.enabled"), &cVar);
    if (NS_SUCCEEDED(rv)) mEnableAltSvc = cVar;
  }

  if (PREF_CHANGED(HTTP_PREF("altsvc.oe"))) {
    rv = Preferences::GetBool(HTTP_PREF("altsvc.oe"), &cVar);
    if (NS_SUCCEEDED(rv)) mEnableAltSvcOE = cVar;
  }

  if (PREF_CHANGED(HTTP_PREF("originextension"))) {
    rv = Preferences::GetBool(HTTP_PREF("originextension"), &cVar);
    if (NS_SUCCEEDED(rv)) mEnableOriginExtension = cVar;
  }

  if (PREF_CHANGED(HTTP_PREF("spdy.websockets"))) {
    rv = Preferences::GetBool(HTTP_PREF("spdy.websockets"), &cVar);
    if (NS_SUCCEEDED(rv)) {
      mEnableH2Websockets = cVar;
    }
  }

  if (PREF_CHANGED(HTTP_PREF("spdy.push-allowance"))) {
    rv = Preferences::GetInt(HTTP_PREF("spdy.push-allowance"), &val);
    if (NS_SUCCEEDED(rv)) {
      mSpdyPushAllowance = static_cast<uint32_t>(
          clamped(val, 1024, static_cast<int32_t>(ASpdySession::kInitialRwin)));
    }
  }

  if (PREF_CHANGED(HTTP_PREF("spdy.pull-allowance"))) {
    rv = Preferences::GetInt(HTTP_PREF("spdy.pull-allowance"), &val);
    if (NS_SUCCEEDED(rv)) {
      mSpdyPullAllowance =
          static_cast<uint32_t>(clamped(val, 1024, 0x7fffffff));
    }
  }

  if (PREF_CHANGED(HTTP_PREF("spdy.default-concurrent"))) {
    rv = Preferences::GetInt(HTTP_PREF("spdy.default-concurrent"), &val);
    if (NS_SUCCEEDED(rv)) {
      mDefaultSpdyConcurrent = static_cast<uint32_t>(
          std::max<int32_t>(std::min<int32_t>(val, 9999), 1));
    }
  }

  // The amount of seconds to wait for a spdy ping response before
  // closing the session.
  if (PREF_CHANGED(HTTP_PREF("spdy.send-buffer-size"))) {
    rv = Preferences::GetInt(HTTP_PREF("spdy.send-buffer-size"), &val);
    if (NS_SUCCEEDED(rv))
      mSpdySendBufferSize = (uint32_t)clamped(val, 1500, 0x7fffffff);
  }

  if (PREF_CHANGED(HTTP_PREF("spdy.enable-hpack-dump"))) {
    rv = Preferences::GetBool(HTTP_PREF("spdy.enable-hpack-dump"), &cVar);
    if (NS_SUCCEEDED(rv)) {
      mDumpHpackTables = cVar;
    }
  }

  // The maximum amount of time to wait for socket transport to be
  // established
  if (PREF_CHANGED(HTTP_PREF("connection-timeout"))) {
    rv = Preferences::GetInt(HTTP_PREF("connection-timeout"), &val);
    if (NS_SUCCEEDED(rv))
      // the pref is in seconds, but the variable is in milliseconds
      mConnectTimeout = clamped(val, 1, 0xffff) * PR_MSEC_PER_SEC;
  }

  // The maximum amount of time to wait for a tls handshake to finish.
  if (PREF_CHANGED(HTTP_PREF("tls-handshake-timeout"))) {
    rv = Preferences::GetInt(HTTP_PREF("tls-handshake-timeout"), &val);
    if (NS_SUCCEEDED(rv))
      // the pref is in seconds, but the variable is in milliseconds
      mTLSHandshakeTimeout = clamped(val, 1, 0xffff) * PR_MSEC_PER_SEC;
  }

  // The maximum number of current global half open sockets allowable
  // for starting a new speculative connection.
  if (PREF_CHANGED(HTTP_PREF("speculative-parallel-limit"))) {
    rv = Preferences::GetInt(HTTP_PREF("speculative-parallel-limit"), &val);
    if (NS_SUCCEEDED(rv))
      mParallelSpeculativeConnectLimit = (uint32_t)clamped(val, 0, 1024);
  }

  // Whether or not to block requests for non head js/css items (e.g. media)
  // while those elements load.
  if (PREF_CHANGED(HTTP_PREF("rendering-critical-requests-prioritization"))) {
    rv = Preferences::GetBool(
        HTTP_PREF("rendering-critical-requests-prioritization"), &cVar);
    if (NS_SUCCEEDED(rv)) mCriticalRequestPrioritization = cVar;
  }

  // on transition of network.http.diagnostics to true print
  // a bunch of information to the console
  if (pref && PREF_CHANGED(HTTP_PREF("diagnostics"))) {
    rv = Preferences::GetBool(HTTP_PREF("diagnostics"), &cVar);
    if (NS_SUCCEEDED(rv) && cVar) {
      if (mConnMgr) mConnMgr->PrintDiagnostics();
    }
  }

  if (PREF_CHANGED(HTTP_PREF("max_response_header_size"))) {
    rv = Preferences::GetInt(HTTP_PREF("max_response_header_size"), &val);
    if (NS_SUCCEEDED(rv)) {
      mMaxHttpResponseHeaderSize = val;
    }
  }

  if (PREF_CHANGED(HTTP_PREF("throttle.enable"))) {
    rv = Preferences::GetBool(HTTP_PREF("throttle.enable"), &mThrottleEnabled);
    if (NS_SUCCEEDED(rv) && mConnMgr) {
      Unused << mConnMgr->UpdateParam(nsHttpConnectionMgr::THROTTLING_ENABLED,
                                      static_cast<int32_t>(mThrottleEnabled));
    }
  }

  if (PREF_CHANGED(HTTP_PREF("throttle.version"))) {
    Unused << Preferences::GetInt(HTTP_PREF("throttle.version"), &val);
    mThrottleVersion = (uint32_t)clamped(val, 1, 2);
  }

  if (PREF_CHANGED(HTTP_PREF("throttle.suspend-for"))) {
    rv = Preferences::GetInt(HTTP_PREF("throttle.suspend-for"), &val);
    mThrottleSuspendFor = (uint32_t)clamped(val, 0, 120000);
    if (NS_SUCCEEDED(rv) && mConnMgr) {
      Unused << mConnMgr->UpdateParam(
          nsHttpConnectionMgr::THROTTLING_SUSPEND_FOR, mThrottleSuspendFor);
    }
  }

  if (PREF_CHANGED(HTTP_PREF("throttle.resume-for"))) {
    rv = Preferences::GetInt(HTTP_PREF("throttle.resume-for"), &val);
    mThrottleResumeFor = (uint32_t)clamped(val, 0, 120000);
    if (NS_SUCCEEDED(rv) && mConnMgr) {
      Unused << mConnMgr->UpdateParam(
          nsHttpConnectionMgr::THROTTLING_RESUME_FOR, mThrottleResumeFor);
    }
  }

  if (PREF_CHANGED(HTTP_PREF("throttle.read-limit-bytes"))) {
    rv = Preferences::GetInt(HTTP_PREF("throttle.read-limit-bytes"), &val);
    mThrottleReadLimit = (uint32_t)clamped(val, 0, 500000);
    if (NS_SUCCEEDED(rv) && mConnMgr) {
      Unused << mConnMgr->UpdateParam(
          nsHttpConnectionMgr::THROTTLING_READ_LIMIT, mThrottleReadLimit);
    }
  }

  if (PREF_CHANGED(HTTP_PREF("throttle.read-interval-ms"))) {
    rv = Preferences::GetInt(HTTP_PREF("throttle.read-interval-ms"), &val);
    mThrottleReadInterval = (uint32_t)clamped(val, 0, 120000);
    if (NS_SUCCEEDED(rv) && mConnMgr) {
      Unused << mConnMgr->UpdateParam(
          nsHttpConnectionMgr::THROTTLING_READ_INTERVAL, mThrottleReadInterval);
    }
  }

  if (PREF_CHANGED(HTTP_PREF("throttle.hold-time-ms"))) {
    rv = Preferences::GetInt(HTTP_PREF("throttle.hold-time-ms"), &val);
    mThrottleHoldTime = (uint32_t)clamped(val, 0, 120000);
    if (NS_SUCCEEDED(rv) && mConnMgr) {
      Unused << mConnMgr->UpdateParam(nsHttpConnectionMgr::THROTTLING_HOLD_TIME,
                                      mThrottleHoldTime);
    }
  }

  if (PREF_CHANGED(HTTP_PREF("throttle.max-time-ms"))) {
    rv = Preferences::GetInt(HTTP_PREF("throttle.max-time-ms"), &val);
    mThrottleMaxTime = (uint32_t)clamped(val, 0, 120000);
    if (NS_SUCCEEDED(rv) && mConnMgr) {
      Unused << mConnMgr->UpdateParam(nsHttpConnectionMgr::THROTTLING_MAX_TIME,
                                      mThrottleMaxTime);
    }
  }

  if (PREF_CHANGED(HTTP_PREF("send_window_size"))) {
    Unused << Preferences::GetInt(HTTP_PREF("send_window_size"), &val);
    mSendWindowSize = val >= 0 ? val : 0;
  }

  if (PREF_CHANGED(HTTP_PREF("on_click_priority"))) {
    Unused << Preferences::GetBool(HTTP_PREF("on_click_priority"),
                                   &mUrgentStartEnabled);
  }

  if (PREF_CHANGED(HTTP_PREF("tailing.enabled"))) {
    Unused << Preferences::GetBool(HTTP_PREF("tailing.enabled"),
                                   &mTailBlockingEnabled);
  }
  if (PREF_CHANGED(HTTP_PREF("tailing.delay-quantum"))) {
    Unused << Preferences::GetInt(HTTP_PREF("tailing.delay-quantum"), &val);
    mTailDelayQuantum = (uint32_t)clamped(val, 0, 60000);
  }
  if (PREF_CHANGED(HTTP_PREF("tailing.delay-quantum-after-domcontentloaded"))) {
    Unused << Preferences::GetInt(
        HTTP_PREF("tailing.delay-quantum-after-domcontentloaded"), &val);
    mTailDelayQuantumAfterDCL = (uint32_t)clamped(val, 0, 60000);
  }
  if (PREF_CHANGED(HTTP_PREF("tailing.delay-max"))) {
    Unused << Preferences::GetInt(HTTP_PREF("tailing.delay-max"), &val);
    mTailDelayMax = (uint32_t)clamped(val, 0, 60000);
  }
  if (PREF_CHANGED(HTTP_PREF("tailing.total-max"))) {
    Unused << Preferences::GetInt(HTTP_PREF("tailing.total-max"), &val);
    mTailTotalMax = (uint32_t)clamped(val, 0, 60000);
  }

  if (PREF_CHANGED(HTTP_PREF("focused_window_transaction_ratio"))) {
    float ratio = 0;
    rv = Preferences::GetFloat(HTTP_PREF("focused_window_transaction_ratio"),
                               &ratio);
    if (NS_SUCCEEDED(rv)) {
      if (ratio > 0 && ratio < 1) {
        mFocusedWindowTransactionRatio = ratio;
      } else {
        NS_WARNING("Wrong value for focused_window_transaction_ratio");
      }
    }
  }

  //
  // INTL options
  //

  if (PREF_CHANGED(INTL_ACCEPT_LANGUAGES)) {
    // We don't want to set the new accept languages here since
    // this pref is a complex type and it may be racy with flushing
    // string resources.
    mAcceptLanguagesIsDirty = true;
  }

  //
  // Tracking options
  //

  if (PREF_CHANGED(DONOTTRACK_HEADER_ENABLED)) {
    cVar = false;
    rv = Preferences::GetBool(DONOTTRACK_HEADER_ENABLED, &cVar);
    if (NS_SUCCEEDED(rv)) {
      mDoNotTrackEnabled = cVar;
    }
  }
  // Hint option
  if (PREF_CHANGED(SAFE_HINT_HEADER_VALUE)) {
    cVar = false;
    rv = Preferences::GetBool(SAFE_HINT_HEADER_VALUE, &cVar);
    if (NS_SUCCEEDED(rv)) {
      mSafeHintEnabled = cVar;
    }
  }

  // toggle to true anytime a token bucket related pref is changed.. that
  // includes telemetry and allow-experiments because of the abtest profile
  bool requestTokenBucketUpdated = false;

  //
  // Telemetry
  //

  if (PREF_CHANGED(TELEMETRY_ENABLED)) {
    cVar = false;
    requestTokenBucketUpdated = true;
    rv = Preferences::GetBool(TELEMETRY_ENABLED, &cVar);
    if (NS_SUCCEEDED(rv)) {
      mTelemetryEnabled = cVar;
    }
  }

  // "security.ssl3.ecdhe_rsa_aes_128_gcm_sha256" is the required h2 interop
  // suite.

  if (PREF_CHANGED(H2MANDATORY_SUITE)) {
    cVar = false;
    rv = Preferences::GetBool(H2MANDATORY_SUITE, &cVar);
    if (NS_SUCCEEDED(rv)) {
      mH2MandatorySuiteEnabled = cVar;
    }
  }

  //
  // network.allow-experiments
  //
  if (PREF_CHANGED(ALLOW_EXPERIMENTS)) {
    cVar = true;
    requestTokenBucketUpdated = true;
    rv = Preferences::GetBool(ALLOW_EXPERIMENTS, &cVar);
    if (NS_SUCCEEDED(rv)) {
      mAllowExperiments = cVar;
    }
  }

  // network.http.debug-observations
  if (PREF_CHANGED("network.http.debug-observations")) {
    cVar = false;
    rv = Preferences::GetBool("network.http.debug-observations", &cVar);
    if (NS_SUCCEEDED(rv)) {
      mDebugObservations = cVar;
    }
  }

  if (PREF_CHANGED(HTTP_PREF("pacing.requests.enabled"))) {
    rv = Preferences::GetBool(HTTP_PREF("pacing.requests.enabled"), &cVar);
    if (NS_SUCCEEDED(rv)) {
      mRequestTokenBucketEnabled = cVar;
      requestTokenBucketUpdated = true;
    }
  }
  if (PREF_CHANGED(HTTP_PREF("pacing.requests.min-parallelism"))) {
    rv =
        Preferences::GetInt(HTTP_PREF("pacing.requests.min-parallelism"), &val);
    if (NS_SUCCEEDED(rv)) {
      mRequestTokenBucketMinParallelism =
          static_cast<uint16_t>(clamped(val, 1, 1024));
      requestTokenBucketUpdated = true;
    }
  }
  if (PREF_CHANGED(HTTP_PREF("pacing.requests.hz"))) {
    rv = Preferences::GetInt(HTTP_PREF("pacing.requests.hz"), &val);
    if (NS_SUCCEEDED(rv)) {
      mRequestTokenBucketHz = static_cast<uint32_t>(clamped(val, 1, 10000));
      requestTokenBucketUpdated = true;
    }
  }
  if (PREF_CHANGED(HTTP_PREF("pacing.requests.burst"))) {
    rv = Preferences::GetInt(HTTP_PREF("pacing.requests.burst"), &val);
    if (NS_SUCCEEDED(rv)) {
      mRequestTokenBucketBurst = val ? val : 1;
      requestTokenBucketUpdated = true;
    }
  }
  if (requestTokenBucketUpdated) {
    MakeNewRequestTokenBucket();
  }

  // Keepalive values for initial and idle connections.
  if (PREF_CHANGED(HTTP_PREF("tcp_keepalive.short_lived_connections"))) {
    rv = Preferences::GetBool(
        HTTP_PREF("tcp_keepalive.short_lived_connections"), &cVar);
    if (NS_SUCCEEDED(rv) && cVar != mTCPKeepaliveShortLivedEnabled) {
      mTCPKeepaliveShortLivedEnabled = cVar;
    }
  }

  if (PREF_CHANGED(HTTP_PREF("tcp_keepalive.short_lived_time"))) {
    rv = Preferences::GetInt(HTTP_PREF("tcp_keepalive.short_lived_time"), &val);
    if (NS_SUCCEEDED(rv) && val > 0)
      mTCPKeepaliveShortLivedTimeS = clamped(val, 1, 300);  // Max 5 mins.
  }

  if (PREF_CHANGED(HTTP_PREF("tcp_keepalive.short_lived_idle_time"))) {
    rv = Preferences::GetInt(HTTP_PREF("tcp_keepalive.short_lived_idle_time"),
                             &val);
    if (NS_SUCCEEDED(rv) && val > 0)
      mTCPKeepaliveShortLivedIdleTimeS = clamped(val, 1, kMaxTCPKeepIdle);
  }

  // Keepalive values for Long-lived Connections.
  if (PREF_CHANGED(HTTP_PREF("tcp_keepalive.long_lived_connections"))) {
    rv = Preferences::GetBool(HTTP_PREF("tcp_keepalive.long_lived_connections"),
                              &cVar);
    if (NS_SUCCEEDED(rv) && cVar != mTCPKeepaliveLongLivedEnabled) {
      mTCPKeepaliveLongLivedEnabled = cVar;
    }
  }

  if (PREF_CHANGED(HTTP_PREF("tcp_keepalive.long_lived_idle_time"))) {
    rv = Preferences::GetInt(HTTP_PREF("tcp_keepalive.long_lived_idle_time"),
                             &val);
    if (NS_SUCCEEDED(rv) && val > 0)
      mTCPKeepaliveLongLivedIdleTimeS = clamped(val, 1, kMaxTCPKeepIdle);
  }

  if (PREF_CHANGED(HTTP_PREF("enforce-framing.http1")) ||
      PREF_CHANGED(HTTP_PREF("enforce-framing.soft"))) {
    rv = Preferences::GetBool(HTTP_PREF("enforce-framing.http1"), &cVar);
    if (NS_SUCCEEDED(rv) && cVar) {
      mEnforceH1Framing = FRAMECHECK_STRICT;
    } else {
      rv = Preferences::GetBool(HTTP_PREF("enforce-framing.soft"), &cVar);
      if (NS_SUCCEEDED(rv) && cVar) {
        mEnforceH1Framing = FRAMECHECK_BARELY;
      } else {
        mEnforceH1Framing = FRAMECHECK_LAX;
      }
    }
  }

  if (PREF_CHANGED(TCP_FAST_OPEN_ENABLE)) {
    rv = Preferences::GetBool(TCP_FAST_OPEN_ENABLE, &cVar);
    if (NS_SUCCEEDED(rv)) {
      mUseFastOpen = cVar;
    }
  }

  if (PREF_CHANGED(TCP_FAST_OPEN_FAILURE_LIMIT)) {
    rv = Preferences::GetInt(TCP_FAST_OPEN_FAILURE_LIMIT, &val);
    if (NS_SUCCEEDED(rv)) {
      if (val < 0) {
        val = 0;
      }
      mFastOpenConsecutiveFailureLimit = val;
    }
  }

  if (PREF_CHANGED(TCP_FAST_OPEN_STALLS_LIMIT)) {
    rv = Preferences::GetInt(TCP_FAST_OPEN_STALLS_LIMIT, &val);
    if (NS_SUCCEEDED(rv)) {
      if (val < 0) {
        val = 0;
      }
      mFastOpenStallsLimit = val;
    }
  }

  if (PREF_CHANGED(TCP_FAST_OPEN_STALLS_TIMEOUT)) {
    rv = Preferences::GetInt(TCP_FAST_OPEN_STALLS_TIMEOUT, &val);
    if (NS_SUCCEEDED(rv)) {
      if (val < 0) {
        val = 0;
      }
      mFastOpenStallsTimeout = val;
    }
  }

  if (PREF_CHANGED(TCP_FAST_OPEN_STALLS_IDLE)) {
    rv = Preferences::GetInt(TCP_FAST_OPEN_STALLS_IDLE, &val);
    if (NS_SUCCEEDED(rv)) {
      if (val < 0) {
        val = 0;
      }
      mFastOpenStallsIdleTime = val;
    }
  }

  if (PREF_CHANGED(HTTP_PREF("spdy.default-hpack-buffer"))) {
    rv = Preferences::GetInt(HTTP_PREF("spdy.default-hpack-buffer"), &val);
    if (NS_SUCCEEDED(rv)) {
      mDefaultHpackBuffer = val;
    }
  }

  // Enable HTTP response timeout if TCP Keepalives are disabled.
  mResponseTimeoutEnabled =
      !mTCPKeepaliveShortLivedEnabled && !mTCPKeepaliveLongLivedEnabled;

#undef PREF_CHANGED
#undef MULTI_PREF_CHANGED
}

/**
 *  Allocates a C string into that contains a ISO 639 language list
 *  notated with HTTP "q" values for output with a HTTP Accept-Language
 *  header. Previous q values will be stripped because the order of
 *  the langs imply the q value. The q values are calculated by dividing
 *  1.0 amongst the number of languages present.
 *
 *  Ex: passing: "en, ja"
 *      returns: "en,ja;q=0.5"
 *
 *      passing: "en, ja, fr_CA"
 *      returns: "en,ja;q=0.7,fr_CA;q=0.3"
 */
static nsresult PrepareAcceptLanguages(const char *i_AcceptLanguages,
                                       nsACString &o_AcceptLanguages) {
  if (!i_AcceptLanguages) return NS_OK;

  const nsAutoCString ns_accept_languages(i_AcceptLanguages);
  return rust_prepare_accept_languages(&ns_accept_languages,
                                       &o_AcceptLanguages);
}

nsresult nsHttpHandler::SetAcceptLanguages() {
  mAcceptLanguagesIsDirty = false;

  nsAutoCString acceptLanguages;
  Preferences::GetLocalizedCString(INTL_ACCEPT_LANGUAGES, acceptLanguages);

  nsAutoCString buf;
  nsresult rv = PrepareAcceptLanguages(acceptLanguages.get(), buf);
  if (NS_SUCCEEDED(rv)) {
    mAcceptLanguages.Assign(buf);
  }
  return rv;
}

nsresult nsHttpHandler::SetAccept(const char *aAccept) {
  mAccept = aAccept;
  return NS_OK;
}

nsresult nsHttpHandler::SetAcceptEncodings(const char *aAcceptEncodings,
                                           bool isSecure) {
  if (isSecure) {
    mHttpsAcceptEncodings = aAcceptEncodings;
  } else {
    // use legacy list if a secure override is not specified
    mHttpAcceptEncodings = aAcceptEncodings;
    if (mHttpsAcceptEncodings.IsEmpty()) {
      mHttpsAcceptEncodings = aAcceptEncodings;
    }
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpHandler::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsHttpHandler, nsIHttpProtocolHandler,
                  nsIProxiedProtocolHandler, nsIProtocolHandler, nsIObserver,
                  nsISupportsWeakReference, nsISpeculativeConnect)

//-----------------------------------------------------------------------------
// nsHttpHandler::nsIProtocolHandler
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpHandler::GetScheme(nsACString &aScheme) {
  aScheme.AssignLiteral("http");
  return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetDefaultPort(int32_t *result) {
  *result = NS_HTTP_DEFAULT_PORT;
  return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetProtocolFlags(uint32_t *result) {
  *result = NS_HTTP_PROTOCOL_FLAGS;
  return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::NewURI(const nsACString &aSpec, const char *aCharset,
                      nsIURI *aBaseURI, nsIURI **aURI) {
  return mozilla::net::NewURI(aSpec, aCharset, aBaseURI, NS_HTTP_DEFAULT_PORT,
                              aURI);
}

NS_IMETHODIMP
nsHttpHandler::NewChannel2(nsIURI *uri, nsILoadInfo *aLoadInfo,
                           nsIChannel **result) {
  LOG(("nsHttpHandler::NewChannel\n"));

  NS_ENSURE_ARG_POINTER(uri);
  NS_ENSURE_ARG_POINTER(result);

  bool isHttp = false, isHttps = false;

  // Verify that we have been given a valid scheme
  nsresult rv = uri->SchemeIs("http", &isHttp);
  if (NS_FAILED(rv)) return rv;
  if (!isHttp) {
    rv = uri->SchemeIs("https", &isHttps);
    if (NS_FAILED(rv)) return rv;
    if (!isHttps) {
      NS_WARNING("Invalid URI scheme");
      return NS_ERROR_UNEXPECTED;
    }
  }

  return NewProxiedChannel2(uri, nullptr, 0, nullptr, aLoadInfo, result);
}

NS_IMETHODIMP
nsHttpHandler::NewChannel(nsIURI *uri, nsIChannel **result) {
  return NewChannel2(uri, nullptr, result);
}

NS_IMETHODIMP
nsHttpHandler::AllowPort(int32_t port, const char *scheme, bool *_retval) {
  // don't override anything.
  *_retval = false;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpHandler::nsIProxiedProtocolHandler
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpHandler::NewProxiedChannel2(nsIURI *uri, nsIProxyInfo *givenProxyInfo,
                                  uint32_t proxyResolveFlags, nsIURI *proxyURI,
                                  nsILoadInfo *aLoadInfo, nsIChannel **result) {
  RefPtr<HttpBaseChannel> httpChannel;

  LOG(("nsHttpHandler::NewProxiedChannel [proxyInfo=%p]\n", givenProxyInfo));

#ifdef MOZ_TASK_TRACER
  if (tasktracer::IsStartLogging()) {
    nsAutoCString urispec;
    uri->GetSpec(urispec);
    tasktracer::AddLabel("nsHttpHandler::NewProxiedChannel2 %s", urispec.get());
  }
#endif

  nsCOMPtr<nsProxyInfo> proxyInfo;
  if (givenProxyInfo) {
    proxyInfo = do_QueryInterface(givenProxyInfo);
    NS_ENSURE_ARG(proxyInfo);
  }

  bool https;
  nsresult rv = uri->SchemeIs("https", &https);
  if (NS_FAILED(rv)) return rv;

  if (IsNeckoChild()) {
    httpChannel = new HttpChannelChild();
  } else {
    httpChannel = new nsHttpChannel();
  }

  uint32_t caps = mCapabilities;

  if (!IsNeckoChild()) {
    // HACK: make sure PSM gets initialized on the main thread.
    net_EnsurePSMInit();
  }

  if (XRE_IsParentProcess()) {
    // Load UserAgentOverrides.jsm before any HTTP request is issued.
    EnsureUAOverridesInit();
  }

  uint64_t channelId;
  rv = NewChannelId(channelId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = httpChannel->Init(uri, caps, proxyInfo, proxyResolveFlags, proxyURI,
                         channelId);
  if (NS_FAILED(rv)) return rv;

  // set the loadInfo on the new channel
  rv = httpChannel->SetLoadInfo(aLoadInfo);
  if (NS_FAILED(rv)) {
    return rv;
  }

  httpChannel.forget(result);
  return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::NewProxiedChannel(nsIURI *uri, nsIProxyInfo *givenProxyInfo,
                                 uint32_t proxyResolveFlags, nsIURI *proxyURI,
                                 nsIChannel **result) {
  return NewProxiedChannel2(uri, givenProxyInfo, proxyResolveFlags, proxyURI,
                            nullptr, result);
}

//-----------------------------------------------------------------------------
// nsHttpHandler::nsIHttpProtocolHandler
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpHandler::GetUserAgent(nsACString &value) {
  value = UserAgent();
  return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetAppName(nsACString &value) {
  value = mLegacyAppName;
  return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetAppVersion(nsACString &value) {
  value = mLegacyAppVersion;
  return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetPlatform(nsACString &value) {
  value = mPlatform;
  return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetOscpu(nsACString &value) {
  value = mOscpu;
  return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetMisc(nsACString &value) {
  value = mMisc;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpHandler::nsIObserver
//-----------------------------------------------------------------------------

static bool CanEnableSpeculativeConnect();  // forward declaration

NS_IMETHODIMP
nsHttpHandler::Observe(nsISupports *subject, const char *topic,
                       const char16_t *data) {
  MOZ_ASSERT(NS_IsMainThread());
  LOG(("nsHttpHandler::Observe [topic=\"%s\"]\n", topic));

  nsresult rv;
  if (!strcmp(topic, "profile-change-net-teardown") ||
      !strcmp(topic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    mHandlerActive = false;

    // clear cache of all authentication credentials.
    Unused << mAuthCache.ClearAll();
    Unused << mPrivateAuthCache.ClearAll();
    if (mWifiTickler) mWifiTickler->Cancel();

    // Inform nsIOService that network is tearing down.
    gIOService->SetHttpHandlerAlreadyShutingDown();

    ShutdownConnectionManager();

    // need to reset the session start time since cache validation may
    // depend on this value.
    mSessionStartTime = NowInSeconds();

    if (!mDoNotTrackEnabled) {
      Telemetry::Accumulate(Telemetry::DNT_USAGE, 2);
    } else {
      Telemetry::Accumulate(Telemetry::DNT_USAGE, 1);
    }

    if (UseFastOpen()) {
      Telemetry::Accumulate(Telemetry::TCP_FAST_OPEN_STATUS, 0);
    } else if (!mFastOpenSupported) {
      Telemetry::Accumulate(Telemetry::TCP_FAST_OPEN_STATUS, 1);
    } else if (!mUseFastOpen) {
      Telemetry::Accumulate(Telemetry::TCP_FAST_OPEN_STATUS, 2);
    } else if (mFastOpenConsecutiveFailureCounter >=
               mFastOpenConsecutiveFailureLimit) {
      Telemetry::Accumulate(Telemetry::TCP_FAST_OPEN_STATUS, 3);
    } else {
      Telemetry::Accumulate(Telemetry::TCP_FAST_OPEN_STATUS, 4);
    }
  } else if (!strcmp(topic, "profile-change-net-restore")) {
    // initialize connection manager
    rv = InitConnectionMgr();
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  } else if (!strcmp(topic, "net:clear-active-logins")) {
    Unused << mAuthCache.ClearAll();
    Unused << mPrivateAuthCache.ClearAll();
  } else if (!strcmp(topic, "net:cancel-all-connections")) {
    if (mConnMgr) {
      mConnMgr->AbortAndCloseAllConnections(0, nullptr);
    }
  } else if (!strcmp(topic, "net:prune-dead-connections")) {
    if (mConnMgr) {
      rv = mConnMgr->PruneDeadConnections();
      if (NS_FAILED(rv)) {
        LOG(("    PruneDeadConnections failed (%08x)\n",
             static_cast<uint32_t>(rv)));
      }
    }
  } else if (!strcmp(topic, "net:prune-all-connections")) {
    if (mConnMgr) {
      rv = mConnMgr->DoShiftReloadConnectionCleanup(nullptr);
      if (NS_FAILED(rv)) {
        LOG(("    DoShiftReloadConnectionCleanup failed (%08x)\n",
             static_cast<uint32_t>(rv)));
      }
      rv = mConnMgr->PruneDeadConnections();
      if (NS_FAILED(rv)) {
        LOG(("    PruneDeadConnections failed (%08x)\n",
             static_cast<uint32_t>(rv)));
      }
    }
#if 0
    } else if (!strcmp(topic, "net:failed-to-process-uri-content")) {
         // nop right now - we used to cancel h1 pipelines based on this,
         // but those are no longer implemented
         nsCOMPtr<nsIURI> uri = do_QueryInterface(subject);
#endif
  } else if (!strcmp(topic, "last-pb-context-exited")) {
    Unused << mPrivateAuthCache.ClearAll();
    if (mConnMgr) {
      mConnMgr->ClearAltServiceMappings();
    }
  } else if (!strcmp(topic, "browser:purge-session-history")) {
    if (mConnMgr) {
      if (gSocketTransportService) {
        nsCOMPtr<nsIRunnable> event = NewRunnableMethod(
            "net::nsHttpConnectionMgr::ClearConnectionHistory", mConnMgr,
            &nsHttpConnectionMgr::ClearConnectionHistory);
        gSocketTransportService->Dispatch(event, NS_DISPATCH_NORMAL);
      }
      mConnMgr->ClearAltServiceMappings();
    }
  } else if (!strcmp(topic, NS_NETWORK_LINK_TOPIC)) {
    nsAutoCString converted = NS_ConvertUTF16toUTF8(data);
    if (!strcmp(converted.get(), NS_NETWORK_LINK_DATA_CHANGED)) {
      if (mConnMgr) {
        rv = mConnMgr->PruneDeadConnections();
        if (NS_FAILED(rv)) {
          LOG(("    PruneDeadConnections failed (%08x)\n",
               static_cast<uint32_t>(rv)));
        }
        rv = mConnMgr->VerifyTraffic();
        if (NS_FAILED(rv)) {
          LOG(("    VerifyTraffic failed (%08x)\n", static_cast<uint32_t>(rv)));
        }
      }
    }
  } else if (!strcmp(topic, "application-background")) {
    // going to the background on android means we should close
    // down idle connections for power conservation
    if (mConnMgr) {
      rv = mConnMgr->DoShiftReloadConnectionCleanup(nullptr);
      if (NS_FAILED(rv)) {
        LOG(("    DoShiftReloadConnectionCleanup failed (%08x)\n",
             static_cast<uint32_t>(rv)));
      }
    }
  } else if (!strcmp(topic, "net:current-toplevel-outer-content-windowid")) {
    nsCOMPtr<nsISupportsPRUint64> wrapper = do_QueryInterface(subject);
    MOZ_RELEASE_ASSERT(wrapper);

    uint64_t windowId = 0;
    wrapper->GetData(&windowId);
    MOZ_ASSERT(windowId);

    static uint64_t sCurrentTopLevelOuterContentWindowId = 0;
    if (sCurrentTopLevelOuterContentWindowId != windowId) {
      sCurrentTopLevelOuterContentWindowId = windowId;
      if (mConnMgr) {
        mConnMgr->UpdateCurrentTopLevelOuterContentWindowId(
            sCurrentTopLevelOuterContentWindowId);
      }
    }
  } else if (!strcmp(topic, "captive-portal-login") ||
             !strcmp(topic, "captive-portal-login-success")) {
    // We have detected a captive portal and we will reset the Fast Open
    // failure counter.
    ResetFastOpenConsecutiveFailureCounter();
  } else if (!strcmp(topic, "psm:user-certificate-added")) {
    // A user certificate has just been added.
    // We should immediately disable speculative connect
    mSpeculativeConnectEnabled = false;
  } else if (!strcmp(topic, "psm:user-certificate-deleted")) {
    // If a user certificate has been removed, we need to check if there
    // are others installed
    mSpeculativeConnectEnabled = CanEnableSpeculativeConnect();
  } else if (!strcmp(topic, "intl:app-locales-changed")) {
    // If the locale changed, there's a chance the accept language did too
    mAcceptLanguagesIsDirty = true;
  }

  return NS_OK;
}

// nsISpeculativeConnect

static bool CanEnableSpeculativeConnect() {
  MOZ_ASSERT(NS_IsMainThread(), "Main thread only");

  nsCOMPtr<nsINSSComponent> component(do_GetService(PSM_COMPONENT_CONTRACTID));
  if (!component) {
    return false;
  }

  // Check if any 3rd party PKCS#11 module are installed, as they may produce
  // client certificates
  bool activeSmartCards = false;
  nsresult rv = component->HasActiveSmartCards(&activeSmartCards);
  if (NS_FAILED(rv) || activeSmartCards) {
    return false;
  }

  // If there are any client certificates installed, we can't enable speculative
  // connect, as it may pop up the certificate chooser at any time.
  bool hasUserCerts = false;
  rv = component->HasUserCertsInstalled(&hasUserCerts);
  if (NS_FAILED(rv) || hasUserCerts) {
    return false;
  }

  return true;
}

nsresult nsHttpHandler::SpeculativeConnectInternal(
    nsIURI *aURI, nsIPrincipal *aPrincipal, nsIInterfaceRequestor *aCallbacks,
    bool anonymous) {
  if (IsNeckoChild()) {
    ipc::URIParams params;
    SerializeURI(aURI, params);
    gNeckoChild->SendSpeculativeConnect(params, IPC::Principal(aPrincipal),
                                        anonymous);
    return NS_OK;
  }

  if (!mHandlerActive) return NS_OK;

  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIObserverService> obsService = services::GetObserverService();
  if (mDebugObservations && obsService) {
    // this is basically used for test coverage of an otherwise 'hintable'
    // feature
    obsService->NotifyObservers(nullptr, "speculative-connect-request",
                                nullptr);
    for (auto *cp :
         dom::ContentParent::AllProcesses(dom::ContentParent::eLive)) {
      PNeckoParent *neckoParent =
          SingleManagedOrNull(cp->ManagedPNeckoParent());
      if (!neckoParent) {
        continue;
      }
      Unused << neckoParent->SendSpeculativeConnectRequest();
    }
  }

  nsISiteSecurityService *sss = gHttpHandler->GetSSService();
  bool isStsHost = false;
  if (!sss) return NS_OK;

  nsCOMPtr<nsILoadContext> loadContext = do_GetInterface(aCallbacks);
  uint32_t flags = 0;
  if (loadContext && loadContext->UsePrivateBrowsing())
    flags |= nsISocketProvider::NO_PERMANENT_STORAGE;

  OriginAttributes originAttributes;
  // If the principal is given, we use the originAttributes from this
  // principal. Otherwise, we use the originAttributes from the
  // loadContext.
  if (aPrincipal) {
    originAttributes = aPrincipal->OriginAttributesRef();
  } else if (loadContext) {
    loadContext->GetOriginAttributes(originAttributes);
  }

  nsCOMPtr<nsIURI> clone;
  if (NS_SUCCEEDED(sss->IsSecureURI(nsISiteSecurityService::HEADER_HSTS, aURI,
                                    flags, originAttributes, nullptr, nullptr,
                                    &isStsHost)) &&
      isStsHost) {
    if (NS_SUCCEEDED(NS_GetSecureUpgradedURI(aURI, getter_AddRefs(clone)))) {
      aURI = clone.get();
      // (NOTE: We better make sure |clone| stays alive until the end
      // of the function now, since our aURI arg now points to it!)
    }
  }

  nsAutoCString scheme;
  nsresult rv = aURI->GetScheme(scheme);
  if (NS_FAILED(rv)) return rv;

  // If this is HTTPS, make sure PSM is initialized as the channel
  // creation path may have been bypassed
  if (scheme.EqualsLiteral("https")) {
    if (!IsNeckoChild()) {
      // make sure PSM gets initialized on the main thread.
      net_EnsurePSMInit();
    }
  }
  // Ensure that this is HTTP or HTTPS, otherwise we don't do preconnect here
  else if (!scheme.EqualsLiteral("http"))
    return NS_ERROR_UNEXPECTED;

  // Construct connection info object
  bool usingSSL = false;
  rv = aURI->SchemeIs("https", &usingSSL);
  if (NS_FAILED(rv)) return rv;

  static bool sCheckedIfSpeculativeEnabled = false;
  if (!sCheckedIfSpeculativeEnabled) {
    sCheckedIfSpeculativeEnabled = true;
    mSpeculativeConnectEnabled = CanEnableSpeculativeConnect();
  }

  if (usingSSL && !mSpeculativeConnectEnabled) {
    return NS_ERROR_UNEXPECTED;
  }

  nsAutoCString host;
  rv = aURI->GetAsciiHost(host);
  if (NS_FAILED(rv)) return rv;

  int32_t port = -1;
  rv = aURI->GetPort(&port);
  if (NS_FAILED(rv)) return rv;

  nsAutoCString username;
  aURI->GetUsername(username);

  auto *ci = new nsHttpConnectionInfo(host, port, EmptyCString(), username,
                                      nullptr, originAttributes, usingSSL);
  ci->SetAnonymous(anonymous);

  return SpeculativeConnect(ci, aCallbacks);
}

NS_IMETHODIMP
nsHttpHandler::SpeculativeConnect2(nsIURI *aURI, nsIPrincipal *aPrincipal,
                                   nsIInterfaceRequestor *aCallbacks) {
  return SpeculativeConnectInternal(aURI, aPrincipal, aCallbacks, false);
}

NS_IMETHODIMP
nsHttpHandler::SpeculativeAnonymousConnect2(nsIURI *aURI,
                                            nsIPrincipal *aPrincipal,
                                            nsIInterfaceRequestor *aCallbacks) {
  return SpeculativeConnectInternal(aURI, aPrincipal, aCallbacks, true);
}

void nsHttpHandler::TickleWifi(nsIInterfaceRequestor *cb) {
  if (!cb || !mWifiTickler) return;

  // If B2G requires a similar mechanism nsINetworkManager, currently only avail
  // on B2G, contains the necessary information on wifi and gateway

  nsCOMPtr<nsIDOMWindow> domWindow = do_GetInterface(cb);
  nsCOMPtr<nsPIDOMWindowOuter> piWindow = do_QueryInterface(domWindow);
  if (!piWindow) return;

  RefPtr<dom::Navigator> navigator = piWindow->GetNavigator();
  if (!navigator) return;

  RefPtr<dom::network::Connection> networkProperties =
      navigator->GetConnection(IgnoreErrors());
  if (!networkProperties) return;

  uint32_t gwAddress = networkProperties->GetDhcpGateway();
  bool isWifi = networkProperties->GetIsWifi();
  if (!gwAddress || !isWifi) return;

  mWifiTickler->SetIPV4Address(gwAddress);
  mWifiTickler->Tickle();
}

//-----------------------------------------------------------------------------
// nsHttpsHandler implementation
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsHttpsHandler, nsIHttpProtocolHandler,
                  nsIProxiedProtocolHandler, nsIProtocolHandler,
                  nsISupportsWeakReference, nsISpeculativeConnect)

nsresult nsHttpsHandler::Init() {
  nsCOMPtr<nsIProtocolHandler> httpHandler(
      do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http"));
  MOZ_ASSERT(httpHandler.get() != nullptr);
  return NS_OK;
}

NS_IMETHODIMP
nsHttpsHandler::GetScheme(nsACString &aScheme) {
  aScheme.AssignLiteral("https");
  return NS_OK;
}

NS_IMETHODIMP
nsHttpsHandler::GetDefaultPort(int32_t *aPort) {
  *aPort = NS_HTTPS_DEFAULT_PORT;
  return NS_OK;
}

NS_IMETHODIMP
nsHttpsHandler::GetProtocolFlags(uint32_t *aProtocolFlags) {
  *aProtocolFlags = NS_HTTP_PROTOCOL_FLAGS | URI_IS_POTENTIALLY_TRUSTWORTHY;
  return NS_OK;
}

NS_IMETHODIMP
nsHttpsHandler::NewURI(const nsACString &aSpec, const char *aOriginCharset,
                       nsIURI *aBaseURI, nsIURI **_retval) {
  return mozilla::net::NewURI(aSpec, aOriginCharset, aBaseURI,
                              NS_HTTPS_DEFAULT_PORT, _retval);
}

NS_IMETHODIMP
nsHttpsHandler::NewChannel2(nsIURI *aURI, nsILoadInfo *aLoadInfo,
                            nsIChannel **_retval) {
  MOZ_ASSERT(gHttpHandler);
  if (!gHttpHandler) return NS_ERROR_UNEXPECTED;
  return gHttpHandler->NewChannel2(aURI, aLoadInfo, _retval);
}

NS_IMETHODIMP
nsHttpsHandler::NewChannel(nsIURI *aURI, nsIChannel **_retval) {
  return NewChannel2(aURI, nullptr, _retval);
}

NS_IMETHODIMP
nsHttpsHandler::AllowPort(int32_t aPort, const char *aScheme, bool *_retval) {
  // don't override anything.
  *_retval = false;
  return NS_OK;
}

void nsHttpHandler::ShutdownConnectionManager() {
  // ensure connection manager is shutdown
  if (mConnMgr) {
    nsresult rv = mConnMgr->Shutdown();
    if (NS_FAILED(rv)) {
      LOG(
          ("nsHttpHandler::ShutdownConnectionManager\n"
           "    failed to shutdown connection manager\n"));
    }
  }
}

nsresult nsHttpHandler::NewChannelId(uint64_t &channelId) {
  MOZ_ASSERT(NS_IsMainThread());
  channelId =
      ((static_cast<uint64_t>(mProcessId) << 32) & 0xFFFFFFFF00000000LL) |
      mNextChannelId++;
  return NS_OK;
}

void nsHttpHandler::NotifyActiveTabLoadOptimization() {
  SetLastActiveTabLoadOptimizationHit(TimeStamp::Now());
}

TimeStamp const nsHttpHandler::GetLastActiveTabLoadOptimizationHit() {
  MutexAutoLock lock(mLastActiveTabLoadOptimizationLock);

  return mLastActiveTabLoadOptimizationHit;
}

void nsHttpHandler::SetLastActiveTabLoadOptimizationHit(TimeStamp const &when) {
  MutexAutoLock lock(mLastActiveTabLoadOptimizationLock);

  if (mLastActiveTabLoadOptimizationHit.IsNull() ||
      (!when.IsNull() && mLastActiveTabLoadOptimizationHit < when)) {
    mLastActiveTabLoadOptimizationHit = when;
  }
}

bool nsHttpHandler::IsBeforeLastActiveTabLoadOptimization(
    TimeStamp const &when) {
  MutexAutoLock lock(mLastActiveTabLoadOptimizationLock);

  return !mLastActiveTabLoadOptimizationHit.IsNull() &&
         when <= mLastActiveTabLoadOptimizationHit;
}

void nsHttpHandler::BlacklistSpdy(const nsHttpConnectionInfo *ci) {
  mConnMgr->BlacklistSpdy(ci);
  mBlacklistedSpdyOrigins.PutEntry(ci->GetOrigin());
}

bool nsHttpHandler::IsSpdyBlacklisted(const nsHttpConnectionInfo *ci) {
  return mBlacklistedSpdyOrigins.Contains(ci->GetOrigin());
}

}  // namespace net
}  // namespace mozilla
