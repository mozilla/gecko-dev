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
#include "mozilla/ThreadSafeWeakPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsIFile.h"
#include "nsProfileLock.h"
#include "mozilla/MozPromise.h"

class nsStartupLock final
    : public mozilla::SupportsThreadSafeWeakPtr<nsStartupLock> {
 public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(nsStartupLock)
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(nsStartupLock)

  nsStartupLock(nsIFile* aDir, nsProfileLock& aLock);

 private:
  ~nsStartupLock();

  nsCOMPtr<nsIFile> mDir;
  nsProfileLock mLock;
};

class nsRemoteService final : public nsIObserver, public nsIRemoteService {
 public:
  // We will be a static singleton, so don't use the ordinary methods.
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIREMOTESERVICE

  nsRemoteService();
  void SetProgram(const char* aProgram);
  void SetProfile(nsACString& aProfile);
#ifdef MOZ_WIDGET_GTK
  void SetStartupToken(nsACString& aStartupToken);
#endif

  using StartupLockPromise =
      mozilla::MozPromise<RefPtr<nsStartupLock>, nsresult, false>;

  /**
   * Attempts to asynchronously lock Firefox startup files. Resolves when the
   * lock is acquired or the timeout is reached
   *
   * Locking is attempted by polling so if multiple instances are attempting to
   * lock it is undefined which one will acquire it when it becomes available.
   * If this instance already has the lock then this returns the same lock.
   * The lock will be released once all instances of `nsStartupLock` have been
   * released.
   *
   * Since this blocks the main thread it should only be called during startup.
   */
  RefPtr<StartupLockPromise> AsyncLockStartup(double aTimeout);

  /**
   * Attempts to synchronously lock startup files. Returns then the lock is
   * acquired or a timeout is reached. In the event of a timeout or other
   * failure a nullptr is returned. Since this blocks the main thread it should
   * only be called during startup.
   *
   * Locking is attempted by polling so if multiple instances are attempting to
   * lock it is undefined which one will acquire it when it becomes available.
   * If this instance already has the lock then this returns the same lock.
   * The lock will be released once all instances of `nsStartupLock` have been
   * released.
   */
  already_AddRefed<nsStartupLock> LockStartup();

  nsresult StartClient();
  void StartupServer();
  void ShutdownServer();

 private:
  friend nsStartupLock;

  mozilla::ThreadSafeWeakPtr<nsStartupLock> mStartupLock;
  RefPtr<nsRemoteService::StartupLockPromise> mStartupLockPromise;

  ~nsRemoteService();
  nsresult SendCommandLine(const nsACString& aProfile, size_t aArgc,
                           const char** aArgv, bool aRaise);

  mozilla::UniquePtr<nsRemoteServer> mRemoteServer;
  nsCString mProgram;
  nsCString mProfile;
#ifdef MOZ_WIDGET_GTK
  nsCString mStartupToken;
#endif
};

#endif  // TOOLKIT_COMPONENTS_REMOTE_NSREMOTESERVER_H_
