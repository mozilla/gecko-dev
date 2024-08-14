/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=8:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_WIDGET_GTK
#  ifdef MOZ_ENABLE_DBUS
#    include "nsDBusRemoteServer.h"
#    include "nsDBusRemoteClient.h"
#  else
#    include "nsGTKRemoteServer.h"
#    include "nsXRemoteClient.h"
#  endif
#elif defined(XP_WIN)
#  include "nsWinRemoteServer.h"
#  include "nsWinRemoteClient.h"
#elif defined(XP_DARWIN)
#  include "nsMacRemoteServer.h"
#  include "nsMacRemoteClient.h"
#endif
#include "nsRemoteService.h"

#include "nsIObserverService.h"
#include "nsString.h"
#include "nsServiceManagerUtils.h"
#include "SpecialSystemDirectory.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"

// Time to wait for the remoting service to start
#define START_TIMEOUT_SEC 5
#define START_SLEEP_MSEC 100

using namespace mozilla;

extern int gArgc;
extern char** gArgv;

using namespace mozilla;

NS_IMPL_ISUPPORTS(nsRemoteService, nsIObserver)

nsRemoteService::nsRemoteService(const char* aProgram) : mProgram(aProgram) {
  ToLowerCase(mProgram);
}

void nsRemoteService::SetProfile(nsACString& aProfile) { mProfile = aProfile; }

void nsRemoteService::LockStartup() {
  nsCOMPtr<nsIFile> mutexDir;
  nsresult rv = GetSpecialSystemDirectory(OS_TemporaryDirectory,
                                          getter_AddRefs(mutexDir));
  NS_ENSURE_SUCCESS_VOID(rv);
  rv = mutexDir->AppendNative(mProgram);
  NS_ENSURE_SUCCESS_VOID(rv);

  const mozilla::TimeStamp epoch = mozilla::TimeStamp::Now();
  do {
    // If we have been waiting for another instance to release the lock it will
    // have deleted the lock directory when doing so we have to make sure it
    // exists every time we poll for the lock.
    rv = mutexDir->Create(nsIFile::DIRECTORY_TYPE, 0700);
    if (NS_SUCCEEDED(rv) || rv == NS_ERROR_FILE_ALREADY_EXISTS) {
      mRemoteLockDir = mutexDir;
    } else {
      NS_WARNING("Unable to create startup lock directory.");
      return;
    }

    rv = mRemoteLock.Lock(mRemoteLockDir, nullptr);
    if (NS_SUCCEEDED(rv)) {
      return;
    }

    mRemoteLockDir = nullptr;
    PR_Sleep(START_SLEEP_MSEC);
  } while ((mozilla::TimeStamp::Now() - epoch) <
           mozilla::TimeDuration::FromSeconds(START_TIMEOUT_SEC));

  NS_WARNING("Failed to lock for startup, continuing anyway.");
}

void nsRemoteService::UnlockStartup() {
  if (mRemoteLockDir) {
    mRemoteLock.Unlock();
    mRemoteLock.Cleanup();

    mRemoteLockDir->Remove(false);
    mRemoteLockDir = nullptr;
  }
}

nsresult nsRemoteService::StartClient(const char* aStartupToken) {
  if (mProfile.IsEmpty()) {
    return NS_ERROR_FAILURE;
  }

  UniquePtr<nsRemoteClient> client;
#ifdef MOZ_WIDGET_GTK
#  if defined(MOZ_ENABLE_DBUS)
  client = MakeUnique<nsDBusRemoteClient>();
#  else
  client = MakeUnique<nsXRemoteClient>();
#  endif
#elif defined(XP_WIN)
  client = MakeUnique<nsWinRemoteClient>();
#elif defined(XP_DARWIN)
  client = MakeUnique<nsMacRemoteClient>();
#else
  return NS_ERROR_NOT_AVAILABLE;
#endif

  nsresult rv = client ? client->Init() : NS_ERROR_FAILURE;
  NS_ENSURE_SUCCESS(rv, rv);

  return client->SendCommandLine(mProgram.get(), mProfile.get(), gArgc,
                                 const_cast<const char**>(gArgv),
                                 aStartupToken);
}

void nsRemoteService::StartupServer() {
  if (mRemoteServer) {
    return;
  }

  if (mProfile.IsEmpty()) {
    return;
  }

#ifdef MOZ_WIDGET_GTK
#  if defined(MOZ_ENABLE_DBUS)
  mRemoteServer = MakeUnique<nsDBusRemoteServer>();
#  else
  mRemoteServer = MakeUnique<nsGTKRemoteServer>();
#  endif
#elif defined(XP_WIN)
  mRemoteServer = MakeUnique<nsWinRemoteServer>();
#elif defined(XP_DARWIN)
  mRemoteServer = MakeUnique<nsMacRemoteServer>();
#else
  return;
#endif

  if (!mRemoteServer) {
    return;
  }

  nsresult rv = mRemoteServer->Startup(mProgram.get(), mProfile.get());

  if (NS_FAILED(rv)) {
    mRemoteServer = nullptr;
    return;
  }

  nsCOMPtr<nsIObserverService> obs(
      do_GetService("@mozilla.org/observer-service;1"));
  if (obs) {
    obs->AddObserver(this, "xpcom-shutdown", false);
    obs->AddObserver(this, "quit-application", false);
  }
}

void nsRemoteService::ShutdownServer() { mRemoteServer = nullptr; }

nsRemoteService::~nsRemoteService() {
  UnlockStartup();
  ShutdownServer();
}

NS_IMETHODIMP
nsRemoteService::Observe(nsISupports* aSubject, const char* aTopic,
                         const char16_t* aData) {
  // This can be xpcom-shutdown or quit-application, but it's the same either
  // way.
  ShutdownServer();
  return NS_OK;
}
