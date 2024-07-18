/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SharedSSLState_h
#define SharedSSLState_h

#include "nsNSSIOLayer.h"

class nsIObserver;

namespace mozilla {
namespace psm {

class SharedSSLState {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SharedSSLState)
  explicit SharedSSLState(uint32_t aTlsFlags = 0);

  static void GlobalInit();
  static void GlobalCleanup();

  nsSSLIOLayerHelpers& IOLayerHelpers() { return mIOLayerHelpers; }

  // Main-thread only
  void ResetStoredData();
  void NotePrivateBrowsingStatus();

  // The following methods may be called from any thread
  bool SocketCreated();
  void NoteSocketCreated();
  static void NoteCertOverrideServiceInstantiated();

 private:
  ~SharedSSLState();

  void Cleanup();

  nsCOMPtr<nsIObserver> mObserver;
  nsSSLIOLayerHelpers mIOLayerHelpers;

  // True if any sockets have been created that use this shared data.
  // Requires synchronization between the socket and main threads for
  // reading/writing.
  Mutex mMutex MOZ_UNANNOTATED;
  bool mSocketCreated;
};

SharedSSLState* PublicSSLState();
SharedSSLState* PrivateSSLState();

}  // namespace psm
}  // namespace mozilla

#endif
