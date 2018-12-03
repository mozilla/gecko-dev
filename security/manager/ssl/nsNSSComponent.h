/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsNSSComponent_h_
#define _nsNSSComponent_h_

#include "nsINSSComponent.h"

#include "ScopedNSSTypes.h"
#include "SharedCertVerifier.h"
#include "mozilla/Attributes.h"
#include "mozilla/Monitor.h"
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsNSSCallbacks.h"
#include "prerror.h"
#include "sslt.h"

#ifdef XP_WIN
#include "windows.h"  // this needs to be before the following includes
#include "wincrypt.h"
#endif  // XP_WIN

class nsIDOMWindow;
class nsIPrompt;
class nsIX509CertList;
class SmartCardThreadList;

namespace mozilla {
namespace psm {

MOZ_MUST_USE
::already_AddRefed<mozilla::psm::SharedCertVerifier> GetDefaultCertVerifier();

}  // namespace psm
}  // namespace mozilla

#define NS_NSSCOMPONENT_CID                          \
  {                                                  \
    0x4cb64dfd, 0xca98, 0x4e24, {                    \
      0xbe, 0xfd, 0x0d, 0x92, 0x85, 0xa3, 0x3b, 0xcb \
    }                                                \
  }

extern bool EnsureNSSInitializedChromeOrContent();

// Implementation of the PSM component interface.
class nsNSSComponent final : public nsINSSComponent, public nsIObserver {
 public:
  // LoadLoadableRootsTask updates mLoadableRootsLoaded and
  // mLoadableRootsLoadedResult and then signals mLoadableRootsLoadedMonitor.
  friend class LoadLoadableRootsTask;

  nsNSSComponent();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSINSSCOMPONENT
  NS_DECL_NSIOBSERVER

  nsresult Init();

  static nsresult GetNewPrompter(nsIPrompt** result);

  static void FillTLSVersionRange(SSLVersionRange& rangeOut,
                                  uint32_t minFromPrefs, uint32_t maxFromPrefs,
                                  SSLVersionRange defaults);

 protected:
  virtual ~nsNSSComponent();

 private:
  nsresult InitializeNSS();
  void ShutdownNSS();

  void setValidationOptions(bool isInitialSetting,
                            const mozilla::MutexAutoLock& proofOfLock);
  nsresult setEnabledTLSVersions();
  nsresult RegisterObservers();

  void MaybeImportEnterpriseRoots();
  void ImportEnterpriseRoots();
  void UnloadEnterpriseRoots();

  void MaybeEnableFamilySafetyCompatibility(uint32_t familySafetyMode);
  void UnloadFamilySafetyRoot();

  nsresult TrustLoaded3rdPartyRoots();

#ifdef XP_WIN
  nsresult MaybeImportFamilySafetyRoot(PCCERT_CONTEXT certificate,
                                       bool& wasFamilySafetyRoot);
  nsresult LoadFamilySafetyRoot();
#endif  // XP_WIN

  // mLoadableRootsLoadedMonitor protects mLoadableRootsLoaded.
  mozilla::Monitor mLoadableRootsLoadedMonitor;
  bool mLoadableRootsLoaded;
  nsresult mLoadableRootsLoadedResult;

  // mMutex protects all members that are accessed from more than one thread.
  mozilla::Mutex mMutex;

  // The following members are accessed from more than one thread:

#ifdef DEBUG
  nsString mTestBuiltInRootHash;
#endif
  nsString mContentSigningRootHash;
  RefPtr<mozilla::psm::SharedCertVerifier> mDefaultCertVerifier;
  nsString mMitmCanaryIssuer;
  bool mMitmDetecionEnabled;
  mozilla::UniqueCERTCertList mEnterpriseRoots;
  mozilla::UniqueCERTCertificate mFamilySafetyRoot;

  // The following members are accessed only on the main thread:
  static int mInstanceCount;
  // If InitializeNSS succeeds, then we have dispatched an event to load the
  // loadable roots module on a background thread. We must wait for it to
  // complete before attempting to unload the module again in ShutdownNSS. If we
  // never dispatched the event, then we can't wait for it to complete (because
  // it will never complete) so we use this boolean to keep track of if we
  // should wait.
  bool mLoadLoadableRootsTaskDispatched;
};

inline nsresult BlockUntilLoadableRootsLoaded() {
  nsCOMPtr<nsINSSComponent> component(do_GetService(PSM_COMPONENT_CONTRACTID));
  if (!component) {
    return NS_ERROR_FAILURE;
  }
  return component->BlockUntilLoadableRootsLoaded();
}

inline nsresult CheckForSmartCardChanges() {
#ifndef MOZ_NO_SMART_CARDS
  nsCOMPtr<nsINSSComponent> component(do_GetService(PSM_COMPONENT_CONTRACTID));
  if (!component) {
    return NS_ERROR_FAILURE;
  }
  return component->CheckForSmartCardChanges();
#else
  return NS_OK;
#endif
}

#endif  // _nsNSSComponent_h_
