/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TOOLKIT_COMPONENTS_REMOTE_NSREMOTESERVER_H_
#define TOOLKIT_COMPONENTS_REMOTE_NSREMOTESERVER_H_

#include "nsRemoteServer.h"
#include "nsIObserver.h"
#include "nsIRemoteService.h"
#include "mozilla/UniquePtr.h"
#include "nsIFile.h"
#include "nsProfileLock.h"

class nsRemoteService final : public nsIObserver, public nsIRemoteService {
 public:
  // We will be a static singleton, so don't use the ordinary methods.
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIREMOTESERVICE

  explicit nsRemoteService(const char* aProgram);
  void SetProfile(nsACString& aProfile);
#ifdef MOZ_WIDGET_GTK
  void SetStartupToken(nsACString& aStartupToken);
#endif

  void LockStartup();
  void UnlockStartup();

  nsresult StartClient();
  void StartupServer();
  void ShutdownServer();

 private:
  ~nsRemoteService();
  nsresult SendCommandLine(const nsACString& aProfile, size_t aArgc,
                           const char** aArgv, bool aRaise);

  mozilla::UniquePtr<nsRemoteServer> mRemoteServer;
  nsProfileLock mRemoteLock;
  nsCOMPtr<nsIFile> mRemoteLockDir;
  nsCString mProgram;
  nsCString mProfile;
#ifdef MOZ_WIDGET_GTK
  nsCString mStartupToken;
#endif
};

#endif  // TOOLKIT_COMPONENTS_REMOTE_NSREMOTESERVER_H_
