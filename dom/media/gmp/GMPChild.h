/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPChild_h_
#define GMPChild_h_

#include "mozilla/gmp/PGMPChild.h"
#include "GMPTimerChild.h"
#include "GMPStorageChild.h"
#include "GMPLoader.h"
#include "gmp-async-shutdown.h"
#include "gmp-entrypoints.h"
#include "prlink.h"

namespace mozilla {
namespace gmp {

class GMPContentChild;

class GMPChild : public PGMPChild
               , public GMPAsyncShutdownHost
{
public:
  GMPChild();
  virtual ~GMPChild();

  bool Init(const nsAString& aPluginPath,
            const nsAString& aVoucherPath,
            base::ProcessId aParentPid,
            MessageLoop* aIOLoop,
            IPC::Channel* aChannel);
#ifdef XP_WIN
  bool PreLoadLibraries(const nsAString& aPluginPath);
#endif
  MessageLoop* GMPMessageLoop();

  // Main thread only.
  GMPTimerChild* GetGMPTimers();
  GMPStorageChild* GetGMPStorage();

  // GMPAsyncShutdownHost
  void ShutdownComplete() override;

#if defined(XP_MACOSX) && defined(MOZ_GMP_SANDBOX)
  bool SetMacSandboxInfo();
#endif

private:
  friend class GMPContentChild;

  bool PreLoadPluginVoucher();
  void PreLoadSandboxVoucher();

  bool GetUTF8LibPath(nsACString& aOutLibPath);

  virtual bool RecvSetNodeId(const nsCString& aNodeId) override;
  virtual bool RecvStartPlugin() override;

  virtual PCrashReporterChild* AllocPCrashReporterChild(const NativeThreadId& aThread) override;
  virtual bool DeallocPCrashReporterChild(PCrashReporterChild*) override;

  virtual PGMPTimerChild* AllocPGMPTimerChild() override;
  virtual bool DeallocPGMPTimerChild(PGMPTimerChild* aActor) override;

  virtual PGMPStorageChild* AllocPGMPStorageChild() override;
  virtual bool DeallocPGMPStorageChild(PGMPStorageChild* aActor) override;

  virtual PGMPContentChild* AllocPGMPContentChild(Transport* aTransport,
                                                  ProcessId aOtherPid) override;
  void GMPContentChildActorDestroy(GMPContentChild* aGMPContentChild);

  virtual bool RecvCrashPluginNow() override;
  virtual bool RecvBeginAsyncShutdown() override;
  virtual bool RecvCloseActive() override;

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  virtual void ProcessingError(Result aCode, const char* aReason) override;

  GMPErr GetAPI(const char* aAPIName, void* aHostAPI, void** aPluginAPI);

  nsTArray<UniquePtr<GMPContentChild>> mGMPContentChildren;

  GMPAsyncShutdown* mAsyncShutdown;
  nsRefPtr<GMPTimerChild> mTimerChild;
  nsRefPtr<GMPStorageChild> mStorage;

  MessageLoop* mGMPMessageLoop;
  nsString mPluginPath;
  nsString mSandboxVoucherPath;
  nsCString mNodeId;
  GMPLoader* mGMPLoader;
  nsTArray<uint8_t> mPluginVoucher;
  nsTArray<uint8_t> mSandboxVoucher;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPChild_h_
