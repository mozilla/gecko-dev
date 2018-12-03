/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSComponent.h"

#include "EnterpriseRoots.h"
#include "ExtendedValidation.h"
#include "NSSCertDBTrustDomain.h"
#include "PKCS11ModuleDB.h"
#include "ScopedNSSTypes.h"
#include "SharedSSLState.h"
#include "cert.h"
#include "certdb.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Casting.h"
#include "mozilla/Preferences.h"
#include "mozilla/PodOperations.h"
#include "mozilla/PublicSSL.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Unused.h"
#include "mozilla/Vector.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsCRT.h"
#include "nsClientAuthRemember.h"
#include "nsComponentManagerUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsICertOverrideService.h"
#include "nsIFile.h"
#include "nsILocalFileWin.h"
#include "nsIObserverService.h"
#include "nsIPrompt.h"
#include "nsIProperties.h"
#include "nsISiteSecurityService.h"
#include "nsITokenPasswordDialogs.h"
#include "nsIWindowWatcher.h"
#include "nsIXULRuntime.h"
#include "nsLiteralString.h"
#include "nsNSSCertificateDB.h"
#include "nsNSSHelper.h"
#include "nsPK11TokenDB.h"
#include "nsPrintfCString.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "nss.h"
#include "p12plcy.h"
#include "mozpkix/pkixnss.h"
#include "secerr.h"
#include "secmod.h"
#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"
#include "prmem.h"

#if defined(XP_LINUX) && !defined(ANDROID)
#include <linux/magic.h>
#include <sys/vfs.h>
#endif

#ifdef XP_WIN
#include "mozilla/WindowsVersion.h"
#include "nsILocalFileWin.h"

#include "windows.h"  // this needs to be before the following includes
#include "lmcons.h"
#include "sddl.h"
#include "wincrypt.h"
#include "nsIWindowsRegKey.h"
#endif

using namespace mozilla;
using namespace mozilla::psm;

LazyLogModule gPIPNSSLog("pipnss");

int nsNSSComponent::mInstanceCount = 0;

// This function can be called from chrome or content processes
// to ensure that NSS is initialized.
bool EnsureNSSInitializedChromeOrContent() {
  static Atomic<bool> initialized(false);

  if (initialized) {
    return true;
  }

  // If this is not the main thread (i.e. probably a worker) then forward this
  // call to the main thread.
  if (!NS_IsMainThread()) {
    nsCOMPtr<nsIThread> mainThread;
    nsresult rv = NS_GetMainThread(getter_AddRefs(mainThread));
    if (NS_FAILED(rv)) {
      return false;
    }

    // Forward to the main thread synchronously.
    mozilla::SyncRunnable::DispatchToThread(
        mainThread, new SyncRunnable(NS_NewRunnableFunction(
                        "EnsureNSSInitializedChromeOrContent",
                        []() { EnsureNSSInitializedChromeOrContent(); })));

    return initialized;
  }

  if (XRE_IsParentProcess()) {
    nsCOMPtr<nsISupports> nss = do_GetService(PSM_COMPONENT_CONTRACTID);
    if (!nss) {
      return false;
    }
    initialized = true;
    return true;
  }

  if (NSS_IsInitialized()) {
    initialized = true;
    return true;
  }

  if (NSS_NoDB_Init(nullptr) != SECSuccess) {
    return false;
  }

  if (NS_FAILED(mozilla::psm::InitializeCipherSuite())) {
    return false;
  }

  mozilla::psm::DisableMD5();
  mozilla::pkix::RegisterErrorTable();
  initialized = true;
  return true;
}

static const uint32_t OCSP_TIMEOUT_MILLISECONDS_SOFT_DEFAULT = 2000;
static const uint32_t OCSP_TIMEOUT_MILLISECONDS_SOFT_MAX = 5000;
static const uint32_t OCSP_TIMEOUT_MILLISECONDS_HARD_DEFAULT = 10000;
static const uint32_t OCSP_TIMEOUT_MILLISECONDS_HARD_MAX = 20000;

static void GetRevocationBehaviorFromPrefs(
    /*out*/ CertVerifier::OcspDownloadConfig* odc,
    /*out*/ CertVerifier::OcspStrictConfig* osc,
    /*out*/ uint32_t* certShortLifetimeInDays,
    /*out*/ TimeDuration& softTimeout,
    /*out*/ TimeDuration& hardTimeout, const MutexAutoLock& /*proofOfLock*/) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(odc);
  MOZ_ASSERT(osc);
  MOZ_ASSERT(certShortLifetimeInDays);

  // 0 = disabled
  // 1 = enabled for everything (default)
  // 2 = enabled for EV certificates only
  int32_t ocspLevel = Preferences::GetInt("security.OCSP.enabled", 1);
  switch (ocspLevel) {
    case 0:
      *odc = CertVerifier::ocspOff;
      break;
    case 2:
      *odc = CertVerifier::ocspEVOnly;
      break;
    default:
      *odc = CertVerifier::ocspOn;
      break;
  }

  *osc = Preferences::GetBool("security.OCSP.require", false)
             ? CertVerifier::ocspStrict
             : CertVerifier::ocspRelaxed;

  // If we pass in just 0 as the second argument to Preferences::GetUint, there
  // are two function signatures that match (given that 0 can be intepreted as
  // a null pointer). Thus the compiler will complain without the cast.
  *certShortLifetimeInDays = Preferences::GetUint(
      "security.pki.cert_short_lifetime_in_days", static_cast<uint32_t>(0));

  uint32_t softTimeoutMillis =
      Preferences::GetUint("security.OCSP.timeoutMilliseconds.soft",
                           OCSP_TIMEOUT_MILLISECONDS_SOFT_DEFAULT);
  softTimeoutMillis =
      std::min(softTimeoutMillis, OCSP_TIMEOUT_MILLISECONDS_SOFT_MAX);
  softTimeout = TimeDuration::FromMilliseconds(softTimeoutMillis);

  uint32_t hardTimeoutMillis =
      Preferences::GetUint("security.OCSP.timeoutMilliseconds.hard",
                           OCSP_TIMEOUT_MILLISECONDS_HARD_DEFAULT);
  hardTimeoutMillis =
      std::min(hardTimeoutMillis, OCSP_TIMEOUT_MILLISECONDS_HARD_MAX);
  hardTimeout = TimeDuration::FromMilliseconds(hardTimeoutMillis);

  SSL_ClearSessionCache();
}

nsNSSComponent::nsNSSComponent()
    : mLoadableRootsLoadedMonitor("nsNSSComponent.mLoadableRootsLoadedMonitor"),
      mLoadableRootsLoaded(false),
      mLoadableRootsLoadedResult(NS_ERROR_FAILURE),
      mMutex("nsNSSComponent.mMutex"),
      mMitmDetecionEnabled(false),
      mLoadLoadableRootsTaskDispatched(false) {
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("nsNSSComponent::ctor\n"));
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(mInstanceCount == 0,
             "nsNSSComponent is a singleton, but instantiated multiple times!");
  ++mInstanceCount;
}

nsNSSComponent::~nsNSSComponent() {
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("nsNSSComponent::dtor\n"));
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  // All cleanup code requiring services needs to happen in xpcom_shutdown

  ShutdownNSS();
  SharedSSLState::GlobalCleanup();
  RememberCertErrorsTable::Cleanup();
  --mInstanceCount;

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("nsNSSComponent::dtor finished\n"));
}

#ifdef XP_WIN
static bool GetUserSid(nsAString& sidString) {
  // UNLEN is the maximum user name length (see Lmcons.h). +1 for the null
  // terminator.
  WCHAR lpAccountName[UNLEN + 1];
  DWORD lcAccountName = sizeof(lpAccountName) / sizeof(lpAccountName[0]);
  BOOL success = GetUserName(lpAccountName, &lcAccountName);
  if (!success) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("GetUserName failed"));
    return false;
  }
  char sid_buffer[SECURITY_MAX_SID_SIZE];
  SID* sid = BitwiseCast<SID*, char*>(sid_buffer);
  DWORD cbSid = ArrayLength(sid_buffer);
  SID_NAME_USE eUse;
  // There doesn't appear to be a defined maximum length for the domain name
  // here. To deal with this, we start with a reasonable buffer length and
  // see if that works. If it fails and the error indicates insufficient length,
  // we use the indicated required length and try again.
  DWORD cchReferencedDomainName = 128;
  auto ReferencedDomainName(MakeUnique<WCHAR[]>(cchReferencedDomainName));
  success = LookupAccountName(nullptr, lpAccountName, sid, &cbSid,
                              ReferencedDomainName.get(),
                              &cchReferencedDomainName, &eUse);
  if (!success && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("LookupAccountName failed"));
    return false;
  }
  if (!success) {
    ReferencedDomainName = MakeUnique<WCHAR[]>(cchReferencedDomainName);
    success = LookupAccountName(nullptr, lpAccountName, sid, &cbSid,
                                ReferencedDomainName.get(),
                                &cchReferencedDomainName, &eUse);
  }
  if (!success) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("LookupAccountName failed"));
    return false;
  }
  LPTSTR StringSid;
  success = ConvertSidToStringSid(sid, &StringSid);
  if (!success) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("ConvertSidToStringSid failed"));
    return false;
  }
  sidString.Assign(StringSid);
  LocalFree(StringSid);
  return true;
}

// This is a specialized helper function to read the value of a registry key
// that might not be present. If it is present, returns (via the output
// parameter) its value. Otherwise, returns the given default value.
// This function handles one level of nesting. That is, if the desired value
// is actually in a direct child of the given registry key (where the child
// and/or the value being sought may not actually be present), this function
// will handle that. In the normal case, though, optionalChildName will be
// null.
static nsresult ReadRegKeyValueWithDefault(nsCOMPtr<nsIWindowsRegKey> regKey,
                                           uint32_t flags,
                                           const wchar_t* optionalChildName,
                                           const wchar_t* valueName,
                                           uint32_t defaultValue,
                                           uint32_t& valueOut) {
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("ReadRegKeyValueWithDefault"));
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("attempting to read '%S%s%S' with default '%u'",
           optionalChildName ? optionalChildName : L"",
           optionalChildName ? "\\" : "", valueName, defaultValue));
  if (optionalChildName) {
    nsDependentString childNameString(optionalChildName);
    bool hasChild;
    nsresult rv = regKey->HasChild(childNameString, &hasChild);
    if (NS_FAILED(rv)) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
              ("failed to determine if child key is present"));
      return rv;
    }
    if (!hasChild) {
      valueOut = defaultValue;
      return NS_OK;
    }
    nsCOMPtr<nsIWindowsRegKey> childRegKey;
    rv = regKey->OpenChild(childNameString, flags, getter_AddRefs(childRegKey));
    if (NS_FAILED(rv)) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("couldn't open child key"));
      return rv;
    }
    return ReadRegKeyValueWithDefault(childRegKey, flags, nullptr, valueName,
                                      defaultValue, valueOut);
  }
  nsDependentString valueNameString(valueName);
  bool hasValue;
  nsresult rv = regKey->HasValue(valueNameString, &hasValue);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("failed to determine if value is present"));
    return rv;
  }
  if (!hasValue) {
    valueOut = defaultValue;
    return NS_OK;
  }
  rv = regKey->ReadIntValue(valueNameString, &valueOut);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("failed to read value"));
    return rv;
  }
  return NS_OK;
}

static nsresult AccountHasFamilySafetyEnabled(bool& enabled) {
  enabled = false;
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("AccountHasFamilySafetyEnabled?"));
  nsCOMPtr<nsIWindowsRegKey> parentalControlsKey(
      do_CreateInstance("@mozilla.org/windows-registry-key;1"));
  if (!parentalControlsKey) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("couldn't create nsIWindowsRegKey"));
    return NS_ERROR_FAILURE;
  }
  uint32_t flags = nsIWindowsRegKey::ACCESS_READ | nsIWindowsRegKey::WOW64_64;
  NS_NAMED_LITERAL_STRING(
      familySafetyPath,
      "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Parental Controls");
  nsresult rv = parentalControlsKey->Open(
      nsIWindowsRegKey::ROOT_KEY_LOCAL_MACHINE, familySafetyPath, flags);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("couldn't open parentalControlsKey"));
    return rv;
  }
  NS_NAMED_LITERAL_STRING(usersString, "Users");
  bool hasUsers;
  rv = parentalControlsKey->HasChild(usersString, &hasUsers);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("HasChild(Users) failed"));
    return rv;
  }
  if (!hasUsers) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("Users subkey not present - Parental Controls not enabled"));
    return NS_OK;
  }
  nsCOMPtr<nsIWindowsRegKey> usersKey;
  rv = parentalControlsKey->OpenChild(usersString, flags,
                                      getter_AddRefs(usersKey));
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("failed to open Users subkey"));
    return rv;
  }
  nsAutoString sid;
  if (!GetUserSid(sid)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("couldn't get sid"));
    return NS_ERROR_FAILURE;
  }
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("our sid is '%S'", sid.get()));
  bool hasSid;
  rv = usersKey->HasChild(sid, &hasSid);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("HasChild(sid) failed"));
    return rv;
  }
  if (!hasSid) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("sid not present in Family Safety Users"));
    return NS_OK;
  }
  nsCOMPtr<nsIWindowsRegKey> sidKey;
  rv = usersKey->OpenChild(sid, flags, getter_AddRefs(sidKey));
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("couldn't open sid key"));
    return rv;
  }
  // There are three keys we're interested in: "Parental Controls On",
  // "Logging Required", and "Web\\Filter On". These keys will have value 0
  // or 1, indicating a particular feature is disabled or enabled,
  // respectively. So, if "Parental Controls On" is not 1, Family Safety is
  // disabled and we don't care about anything else. If both "Logging
  // Required" and "Web\\Filter On" are 0, the proxy will not be running,
  // so for our purposes we can consider Family Safety disabled in that
  // case.
  // By default, "Logging Required" is 1 and "Web\\Filter On" is 0,
  // reflecting the initial settings when Family Safety is enabled for an
  // account for the first time, However, these sub-keys are not created
  // unless they are switched away from the default value.
  uint32_t parentalControlsOn;
  rv = sidKey->ReadIntValue(NS_LITERAL_STRING("Parental Controls On"),
                            &parentalControlsOn);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("couldn't read Parental Controls On"));
    return rv;
  }
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("Parental Controls On: %u", parentalControlsOn));
  if (parentalControlsOn != 1) {
    return NS_OK;
  }
  uint32_t loggingRequired;
  rv = ReadRegKeyValueWithDefault(sidKey, flags, nullptr, L"Logging Required",
                                  1, loggingRequired);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("failed to read value of Logging Required"));
    return rv;
  }
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("Logging Required: %u", loggingRequired));
  uint32_t webFilterOn;
  rv = ReadRegKeyValueWithDefault(sidKey, flags, L"Web", L"Filter On", 0,
                                  webFilterOn);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("failed to read value of Web\\Filter On"));
    return rv;
  }
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("Web\\Filter On: %u", webFilterOn));
  enabled = loggingRequired == 1 || webFilterOn == 1;
  return NS_OK;
}

nsresult nsNSSComponent::MaybeImportFamilySafetyRoot(
    PCCERT_CONTEXT certificate, bool& wasFamilySafetyRoot) {
  MutexAutoLock lock(mMutex);
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("MaybeImportFamilySafetyRoot"));
  wasFamilySafetyRoot = false;

  UniqueCERTCertificate nssCertificate(
      PCCERT_CONTEXTToCERTCertificate(certificate));
  if (!nssCertificate) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("couldn't decode certificate"));
    return NS_ERROR_FAILURE;
  }
  // Looking for a certificate with the common name 'Microsoft Family Safety'
  UniquePORTString subjectName(CERT_GetCommonName(&nssCertificate->subject));
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("subject name is '%s'", subjectName.get()));
  if (kMicrosoftFamilySafetyCN.Equals(subjectName.get())) {
    wasFamilySafetyRoot = true;
    MOZ_ASSERT(!mFamilySafetyRoot);
    mFamilySafetyRoot = std::move(nssCertificate);
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("added Family Safety root"));
  }
  return NS_OK;
}

nsresult nsNSSComponent::LoadFamilySafetyRoot() {
  ScopedCertStore certstore(
      CertOpenSystemStore(0, kWindowsDefaultRootStoreName));
  if (!certstore.get()) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("couldn't get certstore '%S'", kWindowsDefaultRootStoreName));
    return NS_ERROR_FAILURE;
  }
  // Any resources held by the certificate are released by the next call to
  // CertFindCertificateInStore.
  PCCERT_CONTEXT certificate = nullptr;
  while ((certificate = CertFindCertificateInStore(
              certstore.get(), X509_ASN_ENCODING, 0, CERT_FIND_ANY, nullptr,
              certificate))) {
    bool wasFamilySafetyRoot = false;
    nsresult rv = MaybeImportFamilySafetyRoot(certificate, wasFamilySafetyRoot);
    if (NS_SUCCEEDED(rv) && wasFamilySafetyRoot) {
      return NS_OK;  // We're done (we're only expecting one root).
    }
  }
  return NS_ERROR_FAILURE;
}
#endif  // XP_WIN

void nsNSSComponent::UnloadFamilySafetyRoot() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return;
  }
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("UnloadFamilySafetyRoot"));

  // We can't call ChangeCertTrustWithPossibleAuthentication while holding
  // mMutex (because it could potentially call back in to nsNSSComponent and
  // attempt to acquire mMutex), so we move mFamilySafetyRoot out of
  // nsNSSComponent into a local handle. This has the side-effect of clearing
  // mFamilySafetyRoot, which is what we want anyway.
  UniqueCERTCertificate familySafetyRoot;
  {
    MutexAutoLock lock(mMutex);
    if (!mFamilySafetyRoot) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
              ("Family Safety Root wasn't present"));
      return;
    }
    familySafetyRoot = std::move(mFamilySafetyRoot);
    MOZ_ASSERT(!mFamilySafetyRoot);
  }
  MOZ_ASSERT(familySafetyRoot);
  // It would be intuitive to set the trust to { 0, 0, 0 } here. However, this
  // doesn't work for temporary certificates because CERT_ChangeCertTrust first
  // looks up the current trust settings in the permanent cert database, finds
  // that such trust doesn't exist, considers the current trust to be
  // { 0, 0, 0 }, and decides that it doesn't need to update the trust since
  // they're the same. To work around this, we set a non-zero flag to ensure
  // that the trust will get updated.
  CERTCertTrust trust = {CERTDB_USER, 0, 0};
  if (ChangeCertTrustWithPossibleAuthentication(familySafetyRoot, trust,
                                                nullptr) != SECSuccess) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("couldn't untrust certificate for TLS server auth"));
  }
}

// The supported values of this pref are:
// 0: disable detecting Family Safety mode and importing the root
// 1: only attempt to detect Family Safety mode (don't import the root)
// 2: detect Family Safety mode and import the root
const char* kFamilySafetyModePref = "security.family_safety.mode";
const uint32_t kFamilySafetyModeDefault = 0;

// The telemetry gathered by this function is as follows:
// 0-2: the value of the Family Safety mode pref
// 3: detecting Family Safety mode failed
// 4: Family Safety was not enabled
// 5: Family Safety was enabled
// 6: failed to import the Family Safety root
// 7: successfully imported the root
void nsNSSComponent::MaybeEnableFamilySafetyCompatibility(
    uint32_t familySafetyMode) {
#ifdef XP_WIN
  if (!(IsWin8Point1OrLater() && !IsWin10OrLater())) {
    return;
  }
  if (familySafetyMode > 2) {
    familySafetyMode = 0;
  }
  if (familySafetyMode == 0) {
    return;
  }
  bool familySafetyEnabled;
  nsresult rv = AccountHasFamilySafetyEnabled(familySafetyEnabled);
  if (NS_FAILED(rv)) {
    return;
  }
  if (!familySafetyEnabled) {
    return;
  }
  if (familySafetyMode == 2) {
    rv = LoadFamilySafetyRoot();
    if (NS_FAILED(rv)) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
              ("failed to load Family Safety root"));
    }
  }
#endif  // XP_WIN
}

void nsNSSComponent::UnloadEnterpriseRoots() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return;
  }
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("UnloadEnterpriseRoots"));

  // We can't call ChangeCertTrustWithPossibleAuthentication while holding
  // mMutex (because it could potentially call back in to nsNSSComponent and
  // attempt to acquire mMutex), so we move mEnterpriseRoots out of
  // nsNSSComponent into a local handle. This has the side-effect of clearing
  // mEnterpriseRoots, which is what we want anyway.
  UniqueCERTCertList enterpriseRoots;
  {
    MutexAutoLock lock(mMutex);
    if (!mEnterpriseRoots) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
              ("no enterprise roots were present"));
      return;
    }
    enterpriseRoots = std::move(mEnterpriseRoots);
    MOZ_ASSERT(!mEnterpriseRoots);
  }
  MOZ_ASSERT(enterpriseRoots);
  // It would be intuitive to set the trust to { 0, 0, 0 } here. However, this
  // doesn't work for temporary certificates because CERT_ChangeCertTrust first
  // looks up the current trust settings in the permanent cert database, finds
  // that such trust doesn't exist, considers the current trust to be
  // { 0, 0, 0 }, and decides that it doesn't need to update the trust since
  // they're the same. To work around this, we set a non-zero flag to ensure
  // that the trust will get updated.
  CERTCertTrust trust = {CERTDB_USER, 0, 0};
  for (CERTCertListNode* n = CERT_LIST_HEAD(enterpriseRoots.get());
       !CERT_LIST_END(n, enterpriseRoots.get()); n = CERT_LIST_NEXT(n)) {
    if (!n) {
      break;
    }
    if (!n->cert) {
      continue;
    }
    UniqueCERTCertificate cert(CERT_DupCertificate(n->cert));
    if (ChangeCertTrustWithPossibleAuthentication(cert, trust, nullptr) !=
        SECSuccess) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
              ("couldn't untrust certificate for TLS server auth"));
    }
  }
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("unloaded enterprise roots"));
}

static const char* kEnterpriseRootModePref =
    "security.enterprise_roots.enabled";

void nsNSSComponent::MaybeImportEnterpriseRoots() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return;
  }
  bool importEnterpriseRoots =
      Preferences::GetBool(kEnterpriseRootModePref, false);
  if (!importEnterpriseRoots) {
    return;
  }
  ImportEnterpriseRoots();
}

void nsNSSComponent::ImportEnterpriseRoots() {
  UniqueCERTCertList roots;
  nsresult rv = GatherEnterpriseRoots(roots);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("failed gathering enterprise roots"));
    return;
  }

  {
    MutexAutoLock lock(mMutex);
    mEnterpriseRoots = std::move(roots);
  }
}

nsresult nsNSSComponent::TrustLoaded3rdPartyRoots() {
  // We can't call ChangeCertTrustWithPossibleAuthentication while holding
  // mMutex (because it could potentially call back in to nsNSSComponent and
  // attempt to acquire mMutex), so we copy mEnterpriseRoots.
  UniqueCERTCertList enterpriseRoots;
  {
    MutexAutoLock lock(mMutex);
    if (mEnterpriseRoots) {
      enterpriseRoots = nsNSSCertList::DupCertList(mEnterpriseRoots);
      if (!enterpriseRoots) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  CERTCertTrust trust = {CERTDB_TRUSTED_CA | CERTDB_VALID_CA | CERTDB_USER, 0,
                         0};
  if (enterpriseRoots) {
    for (CERTCertListNode* n = CERT_LIST_HEAD(enterpriseRoots.get());
         !CERT_LIST_END(n, enterpriseRoots.get()); n = CERT_LIST_NEXT(n)) {
      if (!n) {
        break;
      }
      if (!n->cert) {
        continue;
      }
      UniqueCERTCertificate cert(CERT_DupCertificate(n->cert));
      if (ChangeCertTrustWithPossibleAuthentication(cert, trust, nullptr) !=
          SECSuccess) {
        MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
                ("couldn't trust enterprise certificate for TLS server auth"));
      }
    }
  }
#ifdef XP_WIN
  // Again copy mFamilySafetyRoot so we don't hold mMutex while calling
  // ChangeCertTrustWithPossibleAuthentication.
  UniqueCERTCertificate familySafetyRoot;
  {
    MutexAutoLock lock(mMutex);
    if (mFamilySafetyRoot) {
      familySafetyRoot.reset(CERT_DupCertificate(mFamilySafetyRoot.get()));
      if (!familySafetyRoot) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }
  if (familySafetyRoot && ChangeCertTrustWithPossibleAuthentication(
                              familySafetyRoot, trust, nullptr) != SECSuccess) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("couldn't trust family safety certificate for TLS server auth"));
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsNSSComponent::GetEnterpriseRoots(nsIX509CertList** enterpriseRoots) {
  MutexAutoLock nsNSSComponentLock(mMutex);
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }
  NS_ENSURE_ARG_POINTER(enterpriseRoots);

  if (!mEnterpriseRoots) {
    *enterpriseRoots = nullptr;
    return NS_OK;
  }
  UniqueCERTCertList enterpriseRootsCopy(
      nsNSSCertList::DupCertList(mEnterpriseRoots));
  if (!enterpriseRootsCopy) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIX509CertList> enterpriseRootsCertList(
      new nsNSSCertList(std::move(enterpriseRootsCopy)));
  if (!enterpriseRootsCertList) {
    return NS_ERROR_FAILURE;
  }
  enterpriseRootsCertList.forget(enterpriseRoots);
  return NS_OK;
}

class LoadLoadableRootsTask final : public Runnable {
 public:
  LoadLoadableRootsTask(nsNSSComponent* nssComponent,
                        bool importEnterpriseRoots, uint32_t familySafetyMode,
                        Vector<nsCString>&& possibleLoadableRootsLocations)
      : Runnable("LoadLoadableRootsTask"),
        mNSSComponent(nssComponent),
        mImportEnterpriseRoots(importEnterpriseRoots),
        mFamilySafetyMode(familySafetyMode),
        mPossibleLoadableRootsLocations(
            std::move(possibleLoadableRootsLocations)) {
    MOZ_ASSERT(nssComponent);
  }

  ~LoadLoadableRootsTask() = default;

  nsresult Dispatch();

 private:
  NS_IMETHOD Run() override;
  nsresult LoadLoadableRoots();
  RefPtr<nsNSSComponent> mNSSComponent;
  bool mImportEnterpriseRoots;
  uint32_t mFamilySafetyMode;
  Vector<nsCString> mPossibleLoadableRootsLocations;
  nsCOMPtr<nsIThread> mThread;
};

nsresult LoadLoadableRootsTask::Dispatch() {
  // Can't add 'this' as the event to run, since mThread may not be set yet
  nsresult rv = NS_NewNamedThread("LoadRoots", getter_AddRefs(mThread), nullptr,
                                  nsIThreadManager::DEFAULT_STACK_SIZE);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Note: event must not null out mThread!
  return mThread->Dispatch(this, NS_DISPATCH_NORMAL);
}

NS_IMETHODIMP
LoadLoadableRootsTask::Run() {
  // First we Run() on the "LoadRoots" thread, do our work, and then we Run()
  // again on the main thread so we can shut down the thread (since we don't
  // need it any more). We can't shut down the thread while we're *on* the
  // thread, which is why we do the dispatch to the main thread. CryptoTask.cpp
  // (which informs this code) says that we can't null out mThread. This appears
  // to be because its refcount could be decreased while this dispatch is being
  // processed, so it might get prematurely destroyed. I'm not sure this makes
  // sense but it'll get cleaned up in our destructor anyway, so it's fine to
  // not null it out here (as long as we don't run twice on the main thread,
  // which shouldn't be possible).
  if (NS_IsMainThread()) {
    if (mThread) {
      mThread->Shutdown();
    }
    return NS_OK;
  }

  nsresult loadLoadableRootsResult = LoadLoadableRoots();
  if (NS_WARN_IF(NS_FAILED(loadLoadableRootsResult))) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Error, ("LoadLoadableRoots failed"));
    // We don't return loadLoadableRootsResult here because then
    // BlockUntilLoadableRootsLoaded will just wait forever. Instead we'll save
    // its value (below) so we can inform code that relies on the roots module
    // being present that loading it failed.
  }

  // Loading EV information will only succeed if we've successfully loaded the
  // loadable roots module.
  if (NS_SUCCEEDED(loadLoadableRootsResult)) {
    if (NS_FAILED(LoadExtendedValidationInfo())) {
      // This isn't a show-stopper in the same way that failing to load the
      // roots module is.
      MOZ_LOG(gPIPNSSLog, LogLevel::Error, ("failed to load EV info"));
    }
  }

  if (mImportEnterpriseRoots) {
    mNSSComponent->ImportEnterpriseRoots();
  }
  mNSSComponent->MaybeEnableFamilySafetyCompatibility(mFamilySafetyMode);
  nsresult rv = mNSSComponent->TrustLoaded3rdPartyRoots();
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Error,
            ("failed to trust loaded 3rd party roots"));
  }

  {
    MonitorAutoLock rootsLoadedLock(mNSSComponent->mLoadableRootsLoadedMonitor);
    mNSSComponent->mLoadableRootsLoaded = true;
    // Cache the result of LoadLoadableRoots so BlockUntilLoadableRootsLoaded
    // can return it to all callers later.
    mNSSComponent->mLoadableRootsLoadedResult = loadLoadableRootsResult;
    rv = mNSSComponent->mLoadableRootsLoadedMonitor.NotifyAll();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Error,
              ("failed to notify loadable roots loaded monitor"));
    }
  }

  // Go back to the main thread to clean up this worker thread.
  return NS_DispatchToMainThread(this);
}

NS_IMETHODIMP
nsNSSComponent::HasActiveSmartCards(bool* result) {
  NS_ENSURE_ARG_POINTER(result);
  MOZ_ASSERT(NS_IsMainThread(), "Main thread only");
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

#ifndef MOZ_NO_SMART_CARDS
  MutexAutoLock nsNSSComponentLock(mMutex);

  AutoSECMODListReadLock secmodLock;
  SECMODModuleList* list = SECMOD_GetDefaultModuleList();
  while (list) {
    if (SECMOD_HasRemovableSlots(list->module)) {
      *result = true;
      return NS_OK;
    }
    list = list->next;
  }
#endif
  *result = false;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSComponent::HasUserCertsInstalled(bool* result) {
  NS_ENSURE_ARG_POINTER(result);
  MOZ_ASSERT(NS_IsMainThread(), "Main thread only");
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  *result = false;
  UniqueCERTCertList certList(CERT_FindUserCertsByUsage(
      CERT_GetDefaultCertDB(), certUsageSSLClient, false, true, nullptr));
  if (!certList) {
    return NS_OK;
  }

  // check if the list is empty
  if (CERT_LIST_END(CERT_LIST_HEAD(certList), certList)) {
    return NS_OK;
  }

  // The list is not empty, meaning at least one cert is installed
  *result = true;
  return NS_OK;
}

nsresult nsNSSComponent::BlockUntilLoadableRootsLoaded() {
  MonitorAutoLock rootsLoadedLock(mLoadableRootsLoadedMonitor);
  while (!mLoadableRootsLoaded) {
    rootsLoadedLock.Wait();
  }
  MOZ_ASSERT(mLoadableRootsLoaded);

  return mLoadableRootsLoadedResult;
}

nsresult nsNSSComponent::CheckForSmartCardChanges() {
#ifndef MOZ_NO_SMART_CARDS
  // SECMOD_UpdateSlotList attempts to acquire the list lock as well,
  // so we have to do this in two steps. The lock protects the list itself, so
  // if we get our own owned references to the modules we're interested in,
  // there's no thread safety concern here.
  Vector<UniqueSECMODModule> modulesWithRemovableSlots;
  {
    AutoSECMODListReadLock secmodLock;
    SECMODModuleList* list = SECMOD_GetDefaultModuleList();
    while (list) {
      if (SECMOD_HasRemovableSlots(list->module)) {
        UniqueSECMODModule module(SECMOD_ReferenceModule(list->module));
        if (!modulesWithRemovableSlots.append(std::move(module))) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
      }
      list = list->next;
    }
  }
  for (auto& module : modulesWithRemovableSlots) {
    // Best-effort.
    Unused << SECMOD_UpdateSlotList(module.get());
    for (int i = 0; i < module->slotCount; i++) {
      // We actually don't care about the return value here - we just need to
      // call this to get NSS to update its view of this slot.
      Unused << PK11_IsPresent(module->slots[i]);
    }
  }
#endif

  return NS_OK;
}

// Returns by reference the path to the directory containing the file that has
// been loaded as MOZ_DLL_PREFIX nss3 MOZ_DLL_SUFFIX.
static nsresult GetNSS3Directory(nsCString& result) {
  MOZ_ASSERT(NS_IsMainThread());

  UniquePRString nss3Path(
      PR_GetLibraryFilePathname(MOZ_DLL_PREFIX "nss3" MOZ_DLL_SUFFIX,
                                reinterpret_cast<PRFuncPtr>(NSS_Initialize)));
  if (!nss3Path) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("nss not loaded?"));
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIFile> nss3File(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
  if (!nss3File) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("couldn't create a file?"));
    return NS_ERROR_FAILURE;
  }
  nsAutoCString nss3PathAsString(nss3Path.get());
  nsresult rv = nss3File->InitWithNativePath(nss3PathAsString);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("couldn't initialize file with path '%s'", nss3Path.get()));
    return rv;
  }
  nsCOMPtr<nsIFile> nss3Directory;
  rv = nss3File->GetParent(getter_AddRefs(nss3Directory));
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("couldn't get parent directory?"));
    return rv;
  }
#ifdef XP_WIN
  // Native path will drop Unicode characters that cannot be mapped to system's
  // codepage, using short (canonical) path as workaround.
  nsCOMPtr<nsILocalFileWin> nss3DirectoryWin = do_QueryInterface(nss3Directory);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("couldn't get nsILocalFileWin"));
    return rv;
  }
  return nss3DirectoryWin->GetNativeCanonicalPath(result);
#else
  return nss3Directory->GetNativePath(result);
#endif
}

// Returns by reference the path to the desired directory, based on the current
// settings in the directory service.
static nsresult GetDirectoryPath(const char* directoryKey, nsCString& result) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIProperties> directoryService(
      do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID));
  if (!directoryService) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("could not get directory service"));
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIFile> directory;
  nsresult rv = directoryService->Get(directoryKey, NS_GET_IID(nsIFile),
                                      getter_AddRefs(directory));
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("could not get '%s' from directory service", directoryKey));
    return rv;
  }
#ifdef XP_WIN
  // Native path will drop Unicode characters that cannot be mapped to system's
  // codepage, using short (canonical) path as workaround.
  nsCOMPtr<nsILocalFileWin> directoryWin = do_QueryInterface(directory);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("couldn't get nsILocalFileWin"));
    return rv;
  }
  return directoryWin->GetNativeCanonicalPath(result);
#else
  return directory->GetNativePath(result);
#endif
}

// The loadable roots library is probably in the same directory we loaded the
// NSS shared library from, but in some cases it may be elsewhere. This function
// enumerates and returns the possible locations as nsCStrings.
static nsresult ListPossibleLoadableRootsLocations(
    Vector<nsCString>& possibleLoadableRootsLocations) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  // First try in the directory where we've already loaded
  // MOZ_DLL_PREFIX nss3 MOZ_DLL_SUFFIX, since that's likely to be correct.
  nsAutoCString nss3Dir;
  nsresult rv = GetNSS3Directory(nss3Dir);
  if (NS_SUCCEEDED(rv)) {
    if (!possibleLoadableRootsLocations.append(std::move(nss3Dir))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  } else {
    // For some reason this fails on android. In any case, we should try with
    // the other potential locations we have.
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("could not determine where nss was loaded from"));
  }
  nsAutoCString currentProcessDir;
  rv = GetDirectoryPath(NS_XPCOM_CURRENT_PROCESS_DIR, currentProcessDir);
  if (NS_SUCCEEDED(rv)) {
    if (!possibleLoadableRootsLocations.append(std::move(currentProcessDir))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  } else {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("could not get current process directory"));
  }
  nsAutoCString greDir;
  rv = GetDirectoryPath(NS_GRE_DIR, greDir);
  if (NS_SUCCEEDED(rv)) {
    if (!possibleLoadableRootsLocations.append(std::move(greDir))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  } else {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("could not get gre directory"));
  }
  // As a last resort, this will cause the library loading code to use the OS'
  // default library search path.
  nsAutoCString emptyString;
  if (!possibleLoadableRootsLocations.append(std::move(emptyString))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

nsresult LoadLoadableRootsTask::LoadLoadableRoots() {
  for (const auto& possibleLocation : mPossibleLoadableRootsLocations) {
    if (mozilla::psm::LoadLoadableRoots(possibleLocation)) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
              ("loaded CKBI from %s", possibleLocation.get()));
      return NS_OK;
    }
  }
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("could not load loadable roots"));
  return NS_ERROR_FAILURE;
}

// Table of pref names and SSL cipher ID
typedef struct {
  const char* pref;
  long id;
  bool enabledByDefault;
} CipherPref;

// Update the switch statement in AccumulateCipherSuite in nsNSSCallbacks.cpp
// when you add/remove cipher suites here.
static const CipherPref sCipherPrefs[] = {
    {"security.ssl3.ecdhe_rsa_aes_128_gcm_sha256",
     TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256, true},
    {"security.ssl3.ecdhe_ecdsa_aes_128_gcm_sha256",
     TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, true},

    {"security.ssl3.ecdhe_ecdsa_chacha20_poly1305_sha256",
     TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256, true},
    {"security.ssl3.ecdhe_rsa_chacha20_poly1305_sha256",
     TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256, true},

    {"security.ssl3.ecdhe_ecdsa_aes_256_gcm_sha384",
     TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384, true},
    {"security.ssl3.ecdhe_rsa_aes_256_gcm_sha384",
     TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384, true},

    {"security.ssl3.ecdhe_rsa_aes_128_sha", TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
     true},
    {"security.ssl3.ecdhe_ecdsa_aes_128_sha",
     TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA, true},

    {"security.ssl3.ecdhe_rsa_aes_256_sha", TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,
     true},
    {"security.ssl3.ecdhe_ecdsa_aes_256_sha",
     TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA, true},

    {"security.ssl3.dhe_rsa_aes_128_sha", TLS_DHE_RSA_WITH_AES_128_CBC_SHA,
     true},

    {"security.ssl3.dhe_rsa_aes_256_sha", TLS_DHE_RSA_WITH_AES_256_CBC_SHA,
     true},

    {"security.tls13.aes_128_gcm_sha256", TLS_AES_128_GCM_SHA256, true},
    {"security.tls13.chacha20_poly1305_sha256", TLS_CHACHA20_POLY1305_SHA256,
     true},
    {"security.tls13.aes_256_gcm_sha384", TLS_AES_256_GCM_SHA384, true},

    {"security.ssl3.rsa_aes_128_sha", TLS_RSA_WITH_AES_128_CBC_SHA,
     true},  // deprecated (RSA key exchange)
    {"security.ssl3.rsa_aes_256_sha", TLS_RSA_WITH_AES_256_CBC_SHA,
     true},  // deprecated (RSA key exchange)
    {"security.ssl3.rsa_des_ede3_sha", TLS_RSA_WITH_3DES_EDE_CBC_SHA,
     true},  // deprecated (RSA key exchange, 3DES)

    // All the rest are disabled

    {nullptr, 0}  // end marker
};

// This function will convert from pref values like 1, 2, ...
// to the internal values of SSL_LIBRARY_VERSION_TLS_1_0,
// SSL_LIBRARY_VERSION_TLS_1_1, ...
/*static*/ void nsNSSComponent::FillTLSVersionRange(SSLVersionRange& rangeOut,
                                                    uint32_t minFromPrefs,
                                                    uint32_t maxFromPrefs,
                                                    SSLVersionRange defaults) {
  rangeOut = defaults;
  // determine what versions are supported
  SSLVersionRange supported;
  if (SSL_VersionRangeGetSupported(ssl_variant_stream, &supported) !=
      SECSuccess) {
    return;
  }

  // Clip the defaults by what NSS actually supports to enable
  // working with a system NSS with different ranges.
  rangeOut.min = std::max(rangeOut.min, supported.min);
  rangeOut.max = std::min(rangeOut.max, supported.max);

  // convert min/maxFromPrefs to the internal representation
  minFromPrefs += SSL_LIBRARY_VERSION_3_0;
  maxFromPrefs += SSL_LIBRARY_VERSION_3_0;
  // if min/maxFromPrefs are invalid, use defaults
  if (minFromPrefs > maxFromPrefs || minFromPrefs < supported.min ||
      maxFromPrefs > supported.max ||
      minFromPrefs < SSL_LIBRARY_VERSION_TLS_1_0) {
    return;
  }

  // fill out rangeOut
  rangeOut.min = (uint16_t)minFromPrefs;
  rangeOut.max = (uint16_t)maxFromPrefs;
}

static const int32_t OCSP_ENABLED_DEFAULT = 1;
static const bool REQUIRE_SAFE_NEGOTIATION_DEFAULT = false;
static const bool FALSE_START_ENABLED_DEFAULT = true;
static const bool ALPN_ENABLED_DEFAULT = false;
static const bool ENABLED_0RTT_DATA_DEFAULT = false;
static const bool HELLO_DOWNGRADE_CHECK_DEFAULT = false;

static void ConfigureTLSSessionIdentifiers() {
  bool disableSessionIdentifiers =
      Preferences::GetBool("security.ssl.disable_session_identifiers", false);
  SSL_OptionSetDefault(SSL_ENABLE_SESSION_TICKETS, !disableSessionIdentifiers);
  SSL_OptionSetDefault(SSL_NO_CACHE, disableSessionIdentifiers);
}

namespace {

class CipherSuiteChangeObserver : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static nsresult StartObserve();

 protected:
  virtual ~CipherSuiteChangeObserver() {}

 private:
  static StaticRefPtr<CipherSuiteChangeObserver> sObserver;
  CipherSuiteChangeObserver() {}
};

NS_IMPL_ISUPPORTS(CipherSuiteChangeObserver, nsIObserver)

// static
StaticRefPtr<CipherSuiteChangeObserver> CipherSuiteChangeObserver::sObserver;

// static
nsresult CipherSuiteChangeObserver::StartObserve() {
  MOZ_ASSERT(NS_IsMainThread(),
             "CipherSuiteChangeObserver::StartObserve() can only be accessed "
             "on the main thread");
  if (!sObserver) {
    RefPtr<CipherSuiteChangeObserver> observer =
        new CipherSuiteChangeObserver();
    nsresult rv = Preferences::AddStrongObserver(observer.get(), "security.");
    if (NS_FAILED(rv)) {
      sObserver = nullptr;
      return rv;
    }

    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    observerService->AddObserver(observer, NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                                 false);

    sObserver = observer;
  }
  return NS_OK;
}

nsresult CipherSuiteChangeObserver::Observe(nsISupports* /*aSubject*/,
                                            const char* aTopic,
                                            const char16_t* someData) {
  MOZ_ASSERT(NS_IsMainThread(),
             "CipherSuiteChangeObserver::Observe can only be accessed on main "
             "thread");
  if (nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) == 0) {
    NS_ConvertUTF16toUTF8 prefName(someData);
    // Look through the cipher table and set according to pref setting
    const CipherPref* const cp = sCipherPrefs;
    for (size_t i = 0; cp[i].pref; ++i) {
      if (prefName.Equals(cp[i].pref)) {
        bool cipherEnabled =
            Preferences::GetBool(cp[i].pref, cp[i].enabledByDefault);
        SSL_CipherPrefSetDefault(cp[i].id, cipherEnabled);
        SSL_ClearSessionCache();
        break;
      }
    }
  } else if (nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    Preferences::RemoveObserver(this, "security.");
    MOZ_ASSERT(sObserver.get() == this);
    sObserver = nullptr;
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
  }
  return NS_OK;
}

}  // namespace

void nsNSSComponent::setValidationOptions(
    bool isInitialSetting, const mozilla::MutexAutoLock& proofOfLock) {
  // This preference controls whether we do OCSP fetching and does not affect
  // OCSP stapling.
  // 0 = disabled, 1 = enabled
  int32_t ocspEnabled =
      Preferences::GetInt("security.OCSP.enabled", OCSP_ENABLED_DEFAULT);

  bool ocspRequired =
      ocspEnabled && Preferences::GetBool("security.OCSP.require", false);

  // We measure the setting of the pref at startup only to minimize noise by
  // addons that may muck with the settings, though it probably doesn't matter.
  if (isInitialSetting) {
    Telemetry::Accumulate(Telemetry::CERT_OCSP_ENABLED, ocspEnabled);
    Telemetry::Accumulate(Telemetry::CERT_OCSP_REQUIRED, ocspRequired);
  }

  bool ocspStaplingEnabled =
      Preferences::GetBool("security.ssl.enable_ocsp_stapling", true);
  PublicSSLState()->SetOCSPStaplingEnabled(ocspStaplingEnabled);
  PrivateSSLState()->SetOCSPStaplingEnabled(ocspStaplingEnabled);

  bool ocspMustStapleEnabled =
      Preferences::GetBool("security.ssl.enable_ocsp_must_staple", true);
  PublicSSLState()->SetOCSPMustStapleEnabled(ocspMustStapleEnabled);
  PrivateSSLState()->SetOCSPMustStapleEnabled(ocspMustStapleEnabled);

  const CertVerifier::CertificateTransparencyMode defaultCTMode =
      CertVerifier::CertificateTransparencyMode::TelemetryOnly;
  CertVerifier::CertificateTransparencyMode ctMode =
      static_cast<CertVerifier::CertificateTransparencyMode>(
          Preferences::GetInt("security.pki.certificate_transparency.mode",
                              static_cast<int32_t>(defaultCTMode)));
  switch (ctMode) {
    case CertVerifier::CertificateTransparencyMode::Disabled:
    case CertVerifier::CertificateTransparencyMode::TelemetryOnly:
      break;
    default:
      ctMode = defaultCTMode;
      break;
  }
  bool sctsEnabled =
      ctMode != CertVerifier::CertificateTransparencyMode::Disabled;
  PublicSSLState()->SetSignedCertTimestampsEnabled(sctsEnabled);
  PrivateSSLState()->SetSignedCertTimestampsEnabled(sctsEnabled);

  CertVerifier::PinningMode pinningMode =
      static_cast<CertVerifier::PinningMode>(
          Preferences::GetInt("security.cert_pinning.enforcement_level",
                              CertVerifier::pinningDisabled));
  if (pinningMode > CertVerifier::pinningEnforceTestMode) {
    pinningMode = CertVerifier::pinningDisabled;
  }

  CertVerifier::SHA1Mode sha1Mode =
      static_cast<CertVerifier::SHA1Mode>(Preferences::GetInt(
          "security.pki.sha1_enforcement_level",
          static_cast<int32_t>(CertVerifier::SHA1Mode::Allowed)));
  switch (sha1Mode) {
    case CertVerifier::SHA1Mode::Allowed:
    case CertVerifier::SHA1Mode::Forbidden:
    case CertVerifier::SHA1Mode::UsedToBeBefore2016ButNowIsForbidden:
    case CertVerifier::SHA1Mode::ImportedRoot:
    case CertVerifier::SHA1Mode::ImportedRootOrBefore2016:
      break;
    default:
      sha1Mode = CertVerifier::SHA1Mode::Allowed;
      break;
  }

  // Convert a previously-available setting to a safe one.
  if (sha1Mode == CertVerifier::SHA1Mode::UsedToBeBefore2016ButNowIsForbidden) {
    sha1Mode = CertVerifier::SHA1Mode::Forbidden;
  }

  BRNameMatchingPolicy::Mode nameMatchingMode =
      static_cast<BRNameMatchingPolicy::Mode>(Preferences::GetInt(
          "security.pki.name_matching_mode",
          static_cast<int32_t>(BRNameMatchingPolicy::Mode::DoNotEnforce)));
  switch (nameMatchingMode) {
    case BRNameMatchingPolicy::Mode::Enforce:
    case BRNameMatchingPolicy::Mode::EnforceAfter23August2015:
    case BRNameMatchingPolicy::Mode::EnforceAfter23August2016:
    case BRNameMatchingPolicy::Mode::DoNotEnforce:
      break;
    default:
      nameMatchingMode = BRNameMatchingPolicy::Mode::DoNotEnforce;
      break;
  }

  NetscapeStepUpPolicy netscapeStepUpPolicy =
      static_cast<NetscapeStepUpPolicy>(Preferences::GetUint(
          "security.pki.netscape_step_up_policy",
          static_cast<uint32_t>(NetscapeStepUpPolicy::AlwaysMatch)));
  switch (netscapeStepUpPolicy) {
    case NetscapeStepUpPolicy::AlwaysMatch:
    case NetscapeStepUpPolicy::MatchBefore23August2016:
    case NetscapeStepUpPolicy::MatchBefore23August2015:
    case NetscapeStepUpPolicy::NeverMatch:
      break;
    default:
      netscapeStepUpPolicy = NetscapeStepUpPolicy::AlwaysMatch;
      break;
  }

  DistrustedCAPolicy defaultCAPolicyMode =
      DistrustedCAPolicy::DistrustSymantecRoots;
  DistrustedCAPolicy distrustedCAPolicy = static_cast<DistrustedCAPolicy>(
      Preferences::GetUint("security.pki.distrust_ca_policy",
                           static_cast<uint32_t>(defaultCAPolicyMode)));
  // If distrustedCAPolicy sets any bits larger than the maximum mask, fall back
  // to the default.
  if (distrustedCAPolicy & ~DistrustedCAPolicyMaxAllowedValueMask) {
    distrustedCAPolicy = defaultCAPolicyMode;
  }

  CertVerifier::OcspDownloadConfig odc;
  CertVerifier::OcspStrictConfig osc;
  uint32_t certShortLifetimeInDays;
  TimeDuration softTimeout;
  TimeDuration hardTimeout;

  GetRevocationBehaviorFromPrefs(&odc, &osc, &certShortLifetimeInDays,
                                 softTimeout, hardTimeout, proofOfLock);
  mDefaultCertVerifier = new SharedCertVerifier(
      odc, osc, softTimeout, hardTimeout, certShortLifetimeInDays, pinningMode,
      sha1Mode, nameMatchingMode, netscapeStepUpPolicy, ctMode,
      distrustedCAPolicy);
}

// Enable the TLS versions given in the prefs, defaulting to TLS 1.0 (min) and
// TLS 1.2 (max) when the prefs aren't set or set to invalid values.
nsresult nsNSSComponent::setEnabledTLSVersions() {
  // keep these values in sync with security-prefs.js
  // 1 means TLS 1.0, 2 means TLS 1.1, etc.
  static const uint32_t PSM_DEFAULT_MIN_TLS_VERSION = 1;
  static const uint32_t PSM_DEFAULT_MAX_TLS_VERSION = 4;

  uint32_t minFromPrefs = Preferences::GetUint("security.tls.version.min",
                                               PSM_DEFAULT_MIN_TLS_VERSION);
  uint32_t maxFromPrefs = Preferences::GetUint("security.tls.version.max",
                                               PSM_DEFAULT_MAX_TLS_VERSION);

  SSLVersionRange defaults = {
      SSL_LIBRARY_VERSION_3_0 + PSM_DEFAULT_MIN_TLS_VERSION,
      SSL_LIBRARY_VERSION_3_0 + PSM_DEFAULT_MAX_TLS_VERSION};
  SSLVersionRange filledInRange;
  FillTLSVersionRange(filledInRange, minFromPrefs, maxFromPrefs, defaults);

  SECStatus srv =
      SSL_VersionRangeSetDefault(ssl_variant_stream, &filledInRange);
  if (srv != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

#if defined(XP_WIN) || (defined(XP_LINUX) && !defined(ANDROID))
// If the profile directory is on a networked drive, we want to set the
// environment variable NSS_SDB_USE_CACHE to yes (as long as it hasn't been set
// before).
static void SetNSSDatabaseCacheModeAsAppropriate() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIFile> profileFile;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(profileFile));
  if (NS_FAILED(rv)) {
    // We're probably running without a profile directory, so this is
    // irrelevant.
    return;
  }

  static const char sNSS_SDB_USE_CACHE[] = "NSS_SDB_USE_CACHE";
  static const char sNSS_SDB_USE_CACHE_WITH_VALUE[] = "NSS_SDB_USE_CACHE=yes";
  auto profilePath = profileFile->NativePath();

#if defined(XP_LINUX) && !defined(ANDROID)
  struct statfs statfs_s;
  if (statfs(profilePath.get(), &statfs_s) == 0 &&
      statfs_s.f_type == NFS_SUPER_MAGIC && !PR_GetEnv(sNSS_SDB_USE_CACHE)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("profile is remote (and NSS_SDB_USE_CACHE wasn't set): "
             "setting NSS_SDB_USE_CACHE"));
    PR_SetEnv(sNSS_SDB_USE_CACHE_WITH_VALUE);
  } else {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("not setting NSS_SDB_USE_CACHE"));
  }
#endif  // defined(XP_LINUX) && !defined(ANDROID)

#ifdef XP_WIN
  wchar_t volPath[MAX_PATH];
  if (::GetVolumePathNameW(profilePath.get(), volPath, MAX_PATH) &&
      ::GetDriveTypeW(volPath) == DRIVE_REMOTE &&
      !PR_GetEnv(sNSS_SDB_USE_CACHE)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("profile is remote (and NSS_SDB_USE_CACHE wasn't set): "
             "setting NSS_SDB_USE_CACHE"));
    PR_SetEnv(sNSS_SDB_USE_CACHE_WITH_VALUE);
  } else {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("not setting NSS_SDB_USE_CACHE"));
  }
#endif  // XP_WIN
}
#endif  // defined(XP_WIN) || (defined(XP_LINUX) && !defined(ANDROID))

static nsresult GetNSSProfilePath(nsAutoCString& aProfilePath) {
  aProfilePath.Truncate();
  nsCOMPtr<nsIFile> profileFile;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(profileFile));
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "NSS will be initialized without a profile directory. "
        "Some things may not work as expected.");
    return NS_OK;
  }

#if defined(XP_WIN)
  // SQLite always takes UTF-8 file paths regardless of the current system
  // code page.
  nsCOMPtr<nsILocalFileWin> profileFileWin(do_QueryInterface(profileFile));
  if (!profileFileWin) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Error,
            ("Could not get nsILocalFileWin for profile directory.\n"));
    return NS_ERROR_FAILURE;
  }
  nsAutoString u16ProfilePath;
  rv = profileFileWin->GetCanonicalPath(u16ProfilePath);
  CopyUTF16toUTF8(u16ProfilePath, aProfilePath);
#else
  rv = profileFile->GetNativePath(aProfilePath);
#endif
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Error,
            ("Could not get native path for profile directory.\n"));
    return rv;
  }

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("NSS profile at '%s'\n", aProfilePath.get()));
  return NS_OK;
}

#ifndef ANDROID
// Given a profile path, attempt to rename the PKCS#11 module DB to
// "<original name>.fips". In the case of a catastrophic failure (e.g. out of
// memory), returns a failing nsresult. If execution could conceivably proceed,
// returns NS_OK even if renaming the file didn't work. This simplifies the
// logic of the calling code.
static nsresult AttemptToRenamePKCS11ModuleDB(
    const nsACString& profilePath, const nsACString& moduleDBFilename) {
  nsAutoCString destModuleDBFilename(moduleDBFilename);
  destModuleDBFilename.Append(".fips");
  nsCOMPtr<nsIFile> dbFile = do_CreateInstance("@mozilla.org/file/local;1");
  if (!dbFile) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv = dbFile->InitWithNativePath(profilePath);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = dbFile->AppendNative(moduleDBFilename);
  if (NS_FAILED(rv)) {
    return rv;
  }
  // If the PKCS#11 module DB doesn't exist, renaming it won't help.
  bool exists;
  rv = dbFile->Exists(&exists);
  if (NS_FAILED(rv)) {
    return rv;
  }
  // This is strange, but not a catastrophic failure.
  if (!exists) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("%s doesn't exist?", PromiseFlatCString(moduleDBFilename).get()));
    return NS_OK;
  }
  nsCOMPtr<nsIFile> destDBFile = do_CreateInstance("@mozilla.org/file/local;1");
  if (!destDBFile) {
    return NS_ERROR_FAILURE;
  }
  rv = destDBFile->InitWithNativePath(profilePath);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = destDBFile->AppendNative(destModuleDBFilename);
  if (NS_FAILED(rv)) {
    return rv;
  }
  // If the destination exists, presumably we've already tried this. Doing it
  // again won't help.
  rv = destDBFile->Exists(&exists);
  if (NS_FAILED(rv)) {
    return rv;
  }
  // Unfortunate, but not a catastrophic failure.
  if (exists) {
    MOZ_LOG(
        gPIPNSSLog, LogLevel::Debug,
        ("%s already exists - not overwriting", destModuleDBFilename.get()));
    return NS_OK;
  }
  // Now do the actual move.
  nsCOMPtr<nsIFile> profileDir = do_CreateInstance("@mozilla.org/file/local;1");
  if (!profileDir) {
    return NS_ERROR_FAILURE;
  }
  rv = profileDir->InitWithNativePath(profilePath);
  if (NS_FAILED(rv)) {
    return rv;
  }
  // This may fail on, e.g., a read-only file system. This would be unfortunate,
  // but again it isn't catastropic and we would want to fall back to
  // initializing NSS in no-DB mode.
  Unused << dbFile->MoveToNative(profileDir, destModuleDBFilename);
  return NS_OK;
}

// The platform now only uses the sqlite-backed databases, so we'll try to
// rename "pkcs11.txt". However, if we're upgrading from a version that used the
// old format, we need to try to rename the old "secmod.db" as well (if we were
// to only rename "pkcs11.txt", initializing NSS will still fail due to the old
// database being in FIPS mode).
static nsresult AttemptToRenameBothPKCS11ModuleDBVersions(
    const nsACString& profilePath) {
  NS_NAMED_LITERAL_CSTRING(legacyModuleDBFilename, "secmod.db");
  NS_NAMED_LITERAL_CSTRING(sqlModuleDBFilename, "pkcs11.txt");
  nsresult rv =
      AttemptToRenamePKCS11ModuleDB(profilePath, legacyModuleDBFilename);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return AttemptToRenamePKCS11ModuleDB(profilePath, sqlModuleDBFilename);
}

// Helper function to take a path and a file name and create a handle for the
// file in that location, if it exists.
static nsresult GetFileIfExists(const nsACString& path,
                                const nsACString& filename,
                                /* out */ nsIFile** result) {
  MOZ_ASSERT(result);
  if (!result) {
    return NS_ERROR_INVALID_ARG;
  }
  *result = nullptr;
  nsCOMPtr<nsIFile> file = do_CreateInstance("@mozilla.org/file/local;1");
  if (!file) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv = file->InitWithNativePath(path);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = file->AppendNative(filename);
  if (NS_FAILED(rv)) {
    return rv;
  }
  bool exists;
  rv = file->Exists(&exists);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (exists) {
    file.forget(result);
  }
  return NS_OK;
}

// When we changed from the old dbm database format to the newer sqlite
// implementation, the upgrade process left behind the existing files. Suppose a
// user had not set a password for the old key3.db (which is about 99% of
// users). After upgrading, both the old database and the new database are
// unprotected. If the user then sets a password for the new database, the old
// one will not be protected. In this scenario, we should probably just remove
// the old database (it would only be relevant if the user downgraded to a
// version of Firefox before 58, but we have to trade this off against the
// user's old private keys being unexpectedly unprotected after setting a
// password).
// This was never an issue on Android because we always used the new
// implementation.
static void MaybeCleanUpOldNSSFiles(const nsACString& profilePath) {
  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
  if (!slot) {
    return;
  }
  // Unfortunately we can't now tell the difference between "there already was a
  // password when the upgrade happened" and "there was not a password but then
  // the user added one after upgrading".
  bool hasPassword =
      PK11_NeedLogin(slot.get()) && !PK11_NeedUserInit(slot.get());
  if (!hasPassword) {
    return;
  }
  NS_NAMED_LITERAL_CSTRING(newKeyDBFilename, "key4.db");
  nsCOMPtr<nsIFile> newDBFile;
  nsresult rv =
      GetFileIfExists(profilePath, newKeyDBFilename, getter_AddRefs(newDBFile));
  if (NS_FAILED(rv)) {
    return;
  }
  // If the new key DB file doesn't exist, we don't want to remove the old DB
  // file. This can happen if the system is configured to use the old DB format
  // even though we're a version of Firefox that expects to use the new format.
  if (!newDBFile) {
    return;
  }
  NS_NAMED_LITERAL_CSTRING(oldKeyDBFilename, "key3.db");
  nsCOMPtr<nsIFile> oldDBFile;
  rv =
      GetFileIfExists(profilePath, oldKeyDBFilename, getter_AddRefs(oldDBFile));
  if (NS_FAILED(rv)) {
    return;
  }
  if (!oldDBFile) {
    return;
  }
  // Since this isn't a directory, the `recursive` argument to `Remove` is
  // irrelevant.
  Unused << oldDBFile->Remove(false);
}
#endif  // ifndef ANDROID

// Given a profile directory, attempt to initialize NSS. If nocertdb is true,
// (or if we don't have a profile directory) simply initialize NSS in no DB mode
// and return. Otherwise, first attempt to initialize in read/write mode, and
// then read-only mode if that fails. If both attempts fail, we may be failing
// to initialize an NSS DB collection that has FIPS mode enabled. Attempt to
// ascertain if this is the case, and if so, rename the offending PKCS#11 module
// DB so we can (hopefully) initialize NSS in read-write mode. Again attempt
// read-only mode if that fails. Finally, fall back to no DB mode. On Android
// we can skip the FIPS workaround since it was never possible to enable FIPS
// there anyway.
static nsresult InitializeNSSWithFallbacks(const nsACString& profilePath,
                                           bool nocertdb, bool safeMode) {
  if (nocertdb || profilePath.IsEmpty()) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("nocertdb mode or empty profile path -> NSS_NoDB_Init"));
    SECStatus srv = NSS_NoDB_Init(nullptr);
    return srv == SECSuccess ? NS_OK : NS_ERROR_FAILURE;
  }

  // Try read/write mode. If we're in safeMode, we won't load PKCS#11 modules.
#ifndef ANDROID
  PRErrorCode savedPRErrorCode1;
#endif  // ifndef ANDROID
  SECStatus srv = ::mozilla::psm::InitializeNSS(profilePath, false, !safeMode);
  if (srv == SECSuccess) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("initialized NSS in r/w mode"));
#ifndef ANDROID
    MaybeCleanUpOldNSSFiles(profilePath);
#endif  // ifndef ANDROID
    return NS_OK;
  }
#ifndef ANDROID
  savedPRErrorCode1 = PR_GetError();
  PRErrorCode savedPRErrorCode2;
#endif  // ifndef ANDROID
  // That failed. Try read-only mode.
  srv = ::mozilla::psm::InitializeNSS(profilePath, true, !safeMode);
  if (srv == SECSuccess) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("initialized NSS in r-o mode"));
    return NS_OK;
  }
#ifndef ANDROID
  savedPRErrorCode2 = PR_GetError();

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("failed to initialize NSS with codes %d %d", savedPRErrorCode1,
           savedPRErrorCode2));
#endif  // ifndef ANDROID

#ifndef ANDROID
  // That failed as well. Maybe we're trying to load a PKCS#11 module DB that is
  // in FIPS mode, but we don't support FIPS? Test load NSS without PKCS#11
  // modules. If that succeeds, that's probably what's going on.
  if (!safeMode && (savedPRErrorCode1 == SEC_ERROR_LEGACY_DATABASE ||
                    savedPRErrorCode2 == SEC_ERROR_LEGACY_DATABASE ||
                    savedPRErrorCode1 == SEC_ERROR_PKCS11_DEVICE_ERROR ||
                    savedPRErrorCode2 == SEC_ERROR_PKCS11_DEVICE_ERROR)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("attempting no-module db init"));
    // It would make sense to initialize NSS in read-only mode here since this
    // is just a test to see if the PKCS#11 module DB being in FIPS mode is the
    // problem, but for some reason the combination of read-only and no-moddb
    // flags causes NSS initialization to fail, so unfortunately we have to use
    // read-write mode.
    srv = ::mozilla::psm::InitializeNSS(profilePath, false, false);
    if (srv == SECSuccess) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("FIPS may be the problem"));
      // Unload NSS so we can attempt to fix this situation for the user.
      srv = NSS_Shutdown();
      if (srv != SECSuccess) {
        return NS_ERROR_FAILURE;
      }
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("trying to rename module db"));
      // If this fails non-catastrophically, we'll attempt to initialize NSS
      // again in r/w then r-o mode (both of which will fail), and then we'll
      // fall back to NSS_NoDB_Init, which is the behavior we want.
      nsresult rv = AttemptToRenameBothPKCS11ModuleDBVersions(profilePath);
      if (NS_FAILED(rv)) {
        return rv;
      }
      srv = ::mozilla::psm::InitializeNSS(profilePath, false, true);
      if (srv == SECSuccess) {
        MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("initialized in r/w mode"));
        return NS_OK;
      }
      srv = ::mozilla::psm::InitializeNSS(profilePath, true, true);
      if (srv == SECSuccess) {
        MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("initialized in r-o mode"));
        return NS_OK;
      }
    }
  }
#endif

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("last-resort NSS_NoDB_Init"));
  srv = NSS_NoDB_Init(nullptr);
  return srv == SECSuccess ? NS_OK : NS_ERROR_FAILURE;
}

nsresult nsNSSComponent::InitializeNSS() {
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("nsNSSComponent::InitializeNSS\n"));

  static_assert(
      nsINSSErrorsService::NSS_SEC_ERROR_BASE == SEC_ERROR_BASE &&
          nsINSSErrorsService::NSS_SEC_ERROR_LIMIT == SEC_ERROR_LIMIT &&
          nsINSSErrorsService::NSS_SSL_ERROR_BASE == SSL_ERROR_BASE &&
          nsINSSErrorsService::NSS_SSL_ERROR_LIMIT == SSL_ERROR_LIMIT,
      "You must update the values in nsINSSErrorsService.idl");

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("NSS Initialization beginning\n"));

  nsAutoCString profileStr;
  nsresult rv = GetNSSProfilePath(profileStr);
  if (NS_FAILED(rv)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

#if defined(XP_WIN) || (defined(XP_LINUX) && !defined(ANDROID))
  SetNSSDatabaseCacheModeAsAppropriate();
#endif

  bool nocertdb = Preferences::GetBool("security.nocertdb", false);
  bool inSafeMode = true;
  nsCOMPtr<nsIXULRuntime> runtime(do_GetService("@mozilla.org/xre/runtime;1"));
  // There might not be an nsIXULRuntime in embedded situations. This will
  // default to assuming we are in safe mode (as a result, no external PKCS11
  // modules will be loaded).
  if (runtime) {
    rv = runtime->GetInSafeMode(&inSafeMode);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("inSafeMode: %u\n", inSafeMode));

  rv = InitializeNSSWithFallbacks(profileStr, nocertdb, inSafeMode);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("failed to initialize NSS"));
    return rv;
  }

  PK11_SetPasswordFunc(PK11PasswordPrompt);

  SharedSSLState::GlobalInit();

  // Register an observer so we can inform NSS when these prefs change
  Preferences::AddStrongObserver(this, "security.");

  SSL_OptionSetDefault(SSL_ENABLE_SSL2, false);
  SSL_OptionSetDefault(SSL_V2_COMPATIBLE_HELLO, false);

  rv = setEnabledTLSVersions();
  if (NS_FAILED(rv)) {
    return NS_ERROR_UNEXPECTED;
  }

  DisableMD5();

  ConfigureTLSSessionIdentifiers();

  bool requireSafeNegotiation =
      Preferences::GetBool("security.ssl.require_safe_negotiation",
                           REQUIRE_SAFE_NEGOTIATION_DEFAULT);
  SSL_OptionSetDefault(SSL_REQUIRE_SAFE_NEGOTIATION, requireSafeNegotiation);

  SSL_OptionSetDefault(SSL_ENABLE_RENEGOTIATION, SSL_RENEGOTIATE_REQUIRES_XTN);

  SSL_OptionSetDefault(SSL_ENABLE_EXTENDED_MASTER_SECRET, true);

  bool enableDowngradeCheck = Preferences::GetBool(
      "security.tls.hello_downgrade_check", HELLO_DOWNGRADE_CHECK_DEFAULT);
  SSL_OptionSetDefault(SSL_ENABLE_HELLO_DOWNGRADE_CHECK, enableDowngradeCheck);

  SSL_OptionSetDefault(SSL_ENABLE_FALSE_START,
                       Preferences::GetBool("security.ssl.enable_false_start",
                                            FALSE_START_ENABLED_DEFAULT));

  // SSL_ENABLE_ALPN also requires calling SSL_SetNextProtoNego in order for
  // the extensions to be negotiated.
  // WebRTC does not do that so it will not use ALPN even when this preference
  // is true.
  SSL_OptionSetDefault(
      SSL_ENABLE_ALPN,
      Preferences::GetBool("security.ssl.enable_alpn", ALPN_ENABLED_DEFAULT));

  SSL_OptionSetDefault(SSL_ENABLE_0RTT_DATA,
                       Preferences::GetBool("security.tls.enable_0rtt_data",
                                            ENABLED_0RTT_DATA_DEFAULT));

  if (NS_FAILED(InitializeCipherSuite())) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Error,
            ("Unable to initialize cipher suite settings\n"));
    return NS_ERROR_FAILURE;
  }

  mozilla::pkix::RegisterErrorTable();

  if (PK11_IsFIPS()) {
    Telemetry::Accumulate(Telemetry::FIPS_ENABLED, true);
  }

  // Gather telemetry on any PKCS#11 modules we have loaded. Note that because
  // we load the built-in root module asynchronously after this, the telemetry
  // will not include it.
  {  // Introduce scope for the AutoSECMODListReadLock.
    AutoSECMODListReadLock lock;
    for (SECMODModuleList* list = SECMOD_GetDefaultModuleList(); list;
         list = list->next) {
      nsAutoString scalarKey;
      GetModuleNameForTelemetry(list->module, scalarKey);
      // Scalar keys must be between 0 and 70 characters (exclusive).
      // GetModuleNameForTelemetry takes care of keys that are too long. If for
      // some reason it couldn't come up with an appropriate name and returned
      // an empty result, however, we need to not attempt to record this (it
      // wouldn't give us anything useful anyway).
      if (scalarKey.Length() > 0) {
        Telemetry::ScalarSet(
            Telemetry::ScalarID::SECURITY_PKCS11_MODULES_LOADED, scalarKey,
            true);
      }
    }
  }

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("NSS Initialization done\n"));

  {
    MutexAutoLock lock(mMutex);

    // ensure we have initial values for various root hashes
#ifdef DEBUG
    mTestBuiltInRootHash.Truncate();
    Preferences::GetString("security.test.built_in_root_hash",
                           mTestBuiltInRootHash);
#endif
    mContentSigningRootHash.Truncate();
    Preferences::GetString("security.content.signature.root_hash",
                           mContentSigningRootHash);

    mMitmCanaryIssuer.Truncate();
    Preferences::GetString("security.pki.mitm_canary_issuer",
                           mMitmCanaryIssuer);
    mMitmDetecionEnabled =
        Preferences::GetBool("security.pki.mitm_canary_issuer.enabled", true);

    // Set dynamic options from prefs. This has to run after
    // SSL_ConfigServerSessionIDCache.
    setValidationOptions(true, lock);

    bool importEnterpriseRoots =
        Preferences::GetBool(kEnterpriseRootModePref, false);
    uint32_t familySafetyMode =
        Preferences::GetUint(kFamilySafetyModePref, kFamilySafetyModeDefault);
    Vector<nsCString> possibleLoadableRootsLocations;
    rv = ListPossibleLoadableRootsLocations(possibleLoadableRootsLocations);
    if (NS_FAILED(rv)) {
      return rv;
    }
    RefPtr<LoadLoadableRootsTask> loadLoadableRootsTask(
        new LoadLoadableRootsTask(this, importEnterpriseRoots, familySafetyMode,
                                  std::move(possibleLoadableRootsLocations)));
    rv = loadLoadableRootsTask->Dispatch();
    if (NS_FAILED(rv)) {
      return rv;
    }

    mLoadLoadableRootsTaskDispatched = true;
    return NS_OK;
  }
}

void nsNSSComponent::ShutdownNSS() {
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("nsNSSComponent::ShutdownNSS\n"));
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  bool loadLoadableRootsTaskDispatched;
  {
    MutexAutoLock lock(mMutex);
    loadLoadableRootsTaskDispatched = mLoadLoadableRootsTaskDispatched;
  }
  // We have to block until the load loadable roots task has completed, because
  // otherwise we might try to unload the loadable roots while the loadable
  // roots loading thread is setting up EV information, which can cause
  // it to fail to find the roots it is expecting. However, if initialization
  // failed, we won't have dispatched the load loadable roots background task.
  // In that case, we don't want to block on an event that will never happen.
  if (loadLoadableRootsTaskDispatched) {
    Unused << BlockUntilLoadableRootsLoaded();
  }

  ::mozilla::psm::UnloadLoadableRoots();

  MutexAutoLock lock(mMutex);
#ifdef XP_WIN
  mFamilySafetyRoot = nullptr;
  mEnterpriseRoots = nullptr;
#endif

  PK11_SetPasswordFunc((PK11PasswordFunc) nullptr);

  Preferences::RemoveObserver(this, "security.");

  // Release the default CertVerifier. This will cause any held NSS resources
  // to be released.
  mDefaultCertVerifier = nullptr;
  // We don't actually shut down NSS - XPCOM does, after all threads have been
  // joined and the component manager has been shut down (and so there shouldn't
  // be any XPCOM objects holding NSS resources).
}

nsresult nsNSSComponent::Init() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  MOZ_ASSERT(XRE_IsParentProcess());
  if (!XRE_IsParentProcess()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("Beginning NSS initialization\n"));

  nsresult rv = InitializeNSS();
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Error,
            ("nsNSSComponent::InitializeNSS() failed\n"));
    return rv;
  }

  RememberCertErrorsTable::Init();

  return RegisterObservers();
}

// nsISupports Implementation for the class
NS_IMPL_ISUPPORTS(nsNSSComponent, nsINSSComponent, nsIObserver)

static const char* const PROFILE_BEFORE_CHANGE_TOPIC = "profile-before-change";

NS_IMETHODIMP
nsNSSComponent::Observe(nsISupports* aSubject, const char* aTopic,
                        const char16_t* someData) {
  // In some tests, we don't receive a "profile-before-change" topic. However,
  // we still have to shut down before the storage service shuts down, because
  // closing the sql-backed softoken requires sqlite still be available. Thus,
  // we observe "xpcom-shutdown" just in case.
  if (nsCRT::strcmp(aTopic, PROFILE_BEFORE_CHANGE_TOPIC) == 0 ||
      nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("receiving profile change or XPCOM shutdown notification"));
    ShutdownNSS();
  } else if (nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) == 0) {
    bool clearSessionCache = true;
    NS_ConvertUTF16toUTF8 prefName(someData);

    if (prefName.EqualsLiteral("security.tls.version.min") ||
        prefName.EqualsLiteral("security.tls.version.max")) {
      (void)setEnabledTLSVersions();
    } else if (prefName.EqualsLiteral("security.tls.hello_downgrade_check")) {
      bool enableDowngradeCheck = Preferences::GetBool(
          "security.tls.hello_downgrade_check", HELLO_DOWNGRADE_CHECK_DEFAULT);
      SSL_OptionSetDefault(SSL_ENABLE_HELLO_DOWNGRADE_CHECK,
                           enableDowngradeCheck);
    } else if (prefName.EqualsLiteral(
                   "security.ssl.require_safe_negotiation")) {
      bool requireSafeNegotiation =
          Preferences::GetBool("security.ssl.require_safe_negotiation",
                               REQUIRE_SAFE_NEGOTIATION_DEFAULT);
      SSL_OptionSetDefault(SSL_REQUIRE_SAFE_NEGOTIATION,
                           requireSafeNegotiation);
    } else if (prefName.EqualsLiteral("security.ssl.enable_false_start")) {
      SSL_OptionSetDefault(
          SSL_ENABLE_FALSE_START,
          Preferences::GetBool("security.ssl.enable_false_start",
                               FALSE_START_ENABLED_DEFAULT));
    } else if (prefName.EqualsLiteral("security.ssl.enable_alpn")) {
      SSL_OptionSetDefault(SSL_ENABLE_ALPN,
                           Preferences::GetBool("security.ssl.enable_alpn",
                                                ALPN_ENABLED_DEFAULT));
    } else if (prefName.EqualsLiteral("security.tls.enable_0rtt_data")) {
      SSL_OptionSetDefault(SSL_ENABLE_0RTT_DATA,
                           Preferences::GetBool("security.tls.enable_0rtt_data",
                                                ENABLED_0RTT_DATA_DEFAULT));
    } else if (prefName.EqualsLiteral(
                   "security.ssl.disable_session_identifiers")) {
      ConfigureTLSSessionIdentifiers();
    } else if (prefName.EqualsLiteral("security.OCSP.enabled") ||
               prefName.EqualsLiteral("security.OCSP.require") ||
               prefName.EqualsLiteral(
                   "security.pki.cert_short_lifetime_in_days") ||
               prefName.EqualsLiteral("security.ssl.enable_ocsp_stapling") ||
               prefName.EqualsLiteral("security.ssl.enable_ocsp_must_staple") ||
               prefName.EqualsLiteral(
                   "security.pki.certificate_transparency.mode") ||
               prefName.EqualsLiteral(
                   "security.cert_pinning.enforcement_level") ||
               prefName.EqualsLiteral("security.pki.sha1_enforcement_level") ||
               prefName.EqualsLiteral("security.pki.name_matching_mode") ||
               prefName.EqualsLiteral("security.pki.netscape_step_up_policy") ||
               prefName.EqualsLiteral(
                   "security.OCSP.timeoutMilliseconds.soft") ||
               prefName.EqualsLiteral(
                   "security.OCSP.timeoutMilliseconds.hard") ||
               prefName.EqualsLiteral("security.pki.distrust_ca_policy")) {
      MutexAutoLock lock(mMutex);
      setValidationOptions(false, lock);
#ifdef DEBUG
    } else if (prefName.EqualsLiteral("security.test.built_in_root_hash")) {
      MutexAutoLock lock(mMutex);
      mTestBuiltInRootHash.Truncate();
      Preferences::GetString("security.test.built_in_root_hash",
                             mTestBuiltInRootHash);
#endif  // DEBUG
    } else if (prefName.Equals(kFamilySafetyModePref)) {
      // When the pref changes, it is safe to change the trust of 3rd party
      // roots in the same event tick that they're loaded.
      UnloadFamilySafetyRoot();
      uint32_t familySafetyMode =
          Preferences::GetUint(kFamilySafetyModePref, kFamilySafetyModeDefault);
      MaybeEnableFamilySafetyCompatibility(familySafetyMode);
      TrustLoaded3rdPartyRoots();
    } else if (prefName.EqualsLiteral("security.content.signature.root_hash")) {
      MutexAutoLock lock(mMutex);
      mContentSigningRootHash.Truncate();
      Preferences::GetString("security.content.signature.root_hash",
                             mContentSigningRootHash);
    } else if (prefName.Equals(kEnterpriseRootModePref)) {
      // When the pref changes, it is safe to change the trust of 3rd party
      // roots in the same event tick that they're loaded.
      UnloadEnterpriseRoots();
      MaybeImportEnterpriseRoots();
      TrustLoaded3rdPartyRoots();
    } else if (prefName.EqualsLiteral("security.pki.mitm_canary_issuer")) {
      MutexAutoLock lock(mMutex);
      mMitmCanaryIssuer.Truncate();
      Preferences::GetString("security.pki.mitm_canary_issuer",
                             mMitmCanaryIssuer);
    } else if (prefName.EqualsLiteral(
                   "security.pki.mitm_canary_issuer.enabled")) {
      MutexAutoLock lock(mMutex);
      mMitmDetecionEnabled =
          Preferences::GetBool("security.pki.mitm_canary_issuer.enabled", true);
    } else {
      clearSessionCache = false;
    }
    if (clearSessionCache) SSL_ClearSessionCache();
  }

  return NS_OK;
}

/*static*/ nsresult nsNSSComponent::GetNewPrompter(nsIPrompt** result) {
  NS_ENSURE_ARG_POINTER(result);
  *result = nullptr;

  if (!NS_IsMainThread()) {
    NS_ERROR("nsSDRContext::GetNewPrompter called off the main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  nsresult rv;
  nsCOMPtr<nsIWindowWatcher> wwatch(
      do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = wwatch->GetNewPrompter(0, result);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

nsresult nsNSSComponent::LogoutAuthenticatedPK11() {
  nsCOMPtr<nsICertOverrideService> icos =
      do_GetService("@mozilla.org/security/certoverride;1");
  if (icos) {
    icos->ClearValidityOverride(
        NS_LITERAL_CSTRING("all:temporary-certificates"), 0);
  }

  nsClientAuthRememberService::ClearAllRememberedDecisions();

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    os->NotifyObservers(nullptr, "net:cancel-all-connections", nullptr);
  }

  return NS_OK;
}

nsresult nsNSSComponent::RegisterObservers() {
  nsCOMPtr<nsIObserverService> observerService(
      do_GetService("@mozilla.org/observer-service;1"));
  if (!observerService) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("nsNSSComponent: couldn't get observer service\n"));
    return NS_ERROR_FAILURE;
  }

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("nsNSSComponent: adding observers\n"));
  // Using false for the ownsweak parameter means the observer service will
  // keep a strong reference to this component. As a result, this will live at
  // least as long as the observer service.
  observerService->AddObserver(this, PROFILE_BEFORE_CHANGE_TOPIC, false);
  observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);

  return NS_OK;
}

NS_IMETHODIMP
nsNSSComponent::IsCertTestBuiltInRoot(CERTCertificate* cert, bool* result) {
  NS_ENSURE_ARG_POINTER(cert);
  NS_ENSURE_ARG_POINTER(result);
  *result = false;

#ifdef DEBUG
  RefPtr<nsNSSCertificate> nsc = nsNSSCertificate::Create(cert);
  if (!nsc) {
    return NS_ERROR_FAILURE;
  }
  nsAutoString certHash;
  nsresult rv = nsc->GetSha256Fingerprint(certHash);
  if (NS_FAILED(rv)) {
    return rv;
  }

  MutexAutoLock lock(mMutex);
  if (mTestBuiltInRootHash.IsEmpty()) {
    return NS_OK;
  }

  *result = mTestBuiltInRootHash.Equals(certHash);
#endif  // DEBUG

  return NS_OK;
}

NS_IMETHODIMP
nsNSSComponent::IsCertContentSigningRoot(CERTCertificate* cert, bool* result) {
  NS_ENSURE_ARG_POINTER(result);
  *result = false;

  RefPtr<nsNSSCertificate> nsc = nsNSSCertificate::Create(cert);
  if (!nsc) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("creating nsNSSCertificate failed"));
    return NS_ERROR_FAILURE;
  }
  nsAutoString certHash;
  nsresult rv = nsc->GetSha256Fingerprint(certHash);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("getting cert fingerprint failed"));
    return rv;
  }

  MutexAutoLock lock(mMutex);

  if (mContentSigningRootHash.IsEmpty()) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("mContentSigningRootHash is empty"));
    return NS_ERROR_FAILURE;
  }

  *result = mContentSigningRootHash.Equals(certHash);
  return NS_OK;
}

NS_IMETHODIMP
nsNSSComponent::IssuerMatchesMitmCanary(const char* aCertIssuer) {
  MutexAutoLock lock(mMutex);
  if (mMitmDetecionEnabled && !mMitmCanaryIssuer.IsEmpty()) {
    nsString certIssuer = NS_ConvertUTF8toUTF16(aCertIssuer);
    if (mMitmCanaryIssuer.Equals(certIssuer)) {
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

SharedCertVerifier::~SharedCertVerifier() {}

NS_IMETHODIMP
nsNSSComponent::GetDefaultCertVerifier(SharedCertVerifier** result) {
  MutexAutoLock lock(mMutex);
  NS_ENSURE_ARG_POINTER(result);
  RefPtr<SharedCertVerifier> certVerifier(mDefaultCertVerifier);
  certVerifier.forget(result);
  return NS_OK;
}

namespace mozilla {
namespace psm {

already_AddRefed<SharedCertVerifier> GetDefaultCertVerifier() {
  static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID));
  if (!nssComponent) {
    return nullptr;
  }
  RefPtr<SharedCertVerifier> result;
  nsresult rv = nssComponent->GetDefaultCertVerifier(getter_AddRefs(result));
  if (NS_FAILED(rv)) {
    return nullptr;
  }
  return result.forget();
}

}  // namespace psm
}  // namespace mozilla

NS_IMPL_ISUPPORTS(PipUIContext, nsIInterfaceRequestor)

PipUIContext::PipUIContext() {}

PipUIContext::~PipUIContext() {}

NS_IMETHODIMP
PipUIContext::GetInterface(const nsIID& uuid, void** result) {
  NS_ENSURE_ARG_POINTER(result);
  *result = nullptr;

  if (!NS_IsMainThread()) {
    NS_ERROR("PipUIContext::GetInterface called off the main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  if (!uuid.Equals(NS_GET_IID(nsIPrompt))) return NS_ERROR_NO_INTERFACE;

  nsIPrompt* prompt = nullptr;
  nsresult rv = nsNSSComponent::GetNewPrompter(&prompt);
  *result = prompt;
  return rv;
}

nsresult getNSSDialogs(void** _result, REFNSIID aIID, const char* contract) {
  if (!NS_IsMainThread()) {
    NS_ERROR("getNSSDialogs called off the main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  nsresult rv;

  nsCOMPtr<nsISupports> svc = do_GetService(contract, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = svc->QueryInterface(aIID, _result);

  return rv;
}

nsresult setPassword(PK11SlotInfo* slot, nsIInterfaceRequestor* ctx) {
  MOZ_ASSERT(slot);
  MOZ_ASSERT(ctx);
  NS_ENSURE_ARG_POINTER(slot);
  NS_ENSURE_ARG_POINTER(ctx);

  if (PK11_NeedUserInit(slot)) {
    nsCOMPtr<nsITokenPasswordDialogs> dialogs;
    nsresult rv = getNSSDialogs(getter_AddRefs(dialogs),
                                NS_GET_IID(nsITokenPasswordDialogs),
                                NS_TOKENPASSWORDSDIALOG_CONTRACTID);
    if (NS_FAILED(rv)) {
      return rv;
    }

    bool canceled;
    nsCOMPtr<nsIPK11Token> token = new nsPK11Token(slot);
    rv = dialogs->SetPassword(ctx, token, &canceled);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (canceled) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  return NS_OK;
}

// NSS will call this during PKCS12 export to potentially switch the endianness
// of the characters of `inBuf` to big (network) endian. Since we already did
// that in nsPKCS12Blob::stringToBigEndianBytes, we just perform a memcpy here.
extern "C" {
PRBool pkcs12StringEndiannessConversion(PRBool, unsigned char* inBuf,
                                        unsigned int inBufLen,
                                        unsigned char* outBuf, unsigned int,
                                        unsigned int* outBufLen, PRBool) {
  *outBufLen = inBufLen;
  memcpy(outBuf, inBuf, inBufLen);
  return true;
}
}

namespace mozilla {
namespace psm {

nsresult InitializeCipherSuite() {
  MOZ_ASSERT(NS_IsMainThread(),
             "InitializeCipherSuite() can only be accessed on the main thread");

  if (NSS_SetDomesticPolicy() != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  // Disable any ciphers that NSS might have enabled by default
  for (uint16_t i = 0; i < SSL_NumImplementedCiphers; ++i) {
    uint16_t cipher_id = SSL_ImplementedCiphers[i];
    SSL_CipherPrefSetDefault(cipher_id, false);
  }

  // Now only set SSL/TLS ciphers we knew about at compile time
  const CipherPref* const cp = sCipherPrefs;
  for (size_t i = 0; cp[i].pref; ++i) {
    bool cipherEnabled =
        Preferences::GetBool(cp[i].pref, cp[i].enabledByDefault);
    SSL_CipherPrefSetDefault(cp[i].id, cipherEnabled);
  }

  // Enable ciphers for PKCS#12
  SEC_PKCS12EnableCipher(PKCS12_RC4_40, 1);
  SEC_PKCS12EnableCipher(PKCS12_RC4_128, 1);
  SEC_PKCS12EnableCipher(PKCS12_RC2_CBC_40, 1);
  SEC_PKCS12EnableCipher(PKCS12_RC2_CBC_128, 1);
  SEC_PKCS12EnableCipher(PKCS12_DES_56, 1);
  SEC_PKCS12EnableCipher(PKCS12_DES_EDE3_168, 1);
  SEC_PKCS12SetPreferredCipher(PKCS12_DES_EDE3_168, 1);
  PORT_SetUCS2_ASCIIConversionFunction(pkcs12StringEndiannessConversion);

  // PSM enforces a minimum RSA key size of 1024 bits, which is overridable.
  // NSS has its own minimum, which is not overridable (the default is 1023
  // bits). This sets the NSS minimum to 512 bits so users can still connect to
  // devices like wifi routers with woefully small keys (they would have to add
  // an override to do so, but they already do for such devices).
  NSS_OptionSet(NSS_RSA_MIN_KEY_SIZE, 512);

  // Observe preference change around cipher suite setting.
  return CipherSuiteChangeObserver::StartObserve();
}

}  // namespace psm
}  // namespace mozilla
