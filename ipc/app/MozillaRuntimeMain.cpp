/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULAppAPI.h"
#include "XREChildData.h"
#include "mozilla/Bootstrap.h"
#include "mozilla/ProcessType.h"
#include "mozilla/RuntimeExceptionModule.h"
#include "mozilla/ScopeExit.h"
#if defined(XP_WIN)
#  include "mozilla/WindowsDllBlocklist.h"
#  include "mozilla/GeckoArgs.h"

#  include "nsWindowsWMain.cpp"

#  ifdef MOZ_SANDBOX
#    include "mozilla/sandboxing/SandboxInitialization.h"
#    include "mozilla/sandboxing/sandboxLogging.h"
#  endif
#endif  // defined(XP_WIN)

using namespace mozilla;

int main(int argc, char* argv[]) {
  // Set the process type and gecko child id.
  if (argc < 2) {
    return 3;
  }
  SetGeckoProcessType(argv[--argc]);
  SetGeckoChildID(argv[--argc]);

  auto bootstrapResult = GetBootstrap();
  if (bootstrapResult.isErr()) {
    return 2;
  }

  Bootstrap::UniquePtr bootstrap = bootstrapResult.unwrap();

#if defined(MOZ_ENABLE_FORKSERVER)
  if (GetGeckoProcessType() == GeckoProcessType_ForkServer) {
    bootstrap->NS_LogInit();

    // Run a fork server in this process, single thread. When it returns, it
    // means the fork server have been stopped or a new child process is
    // created.
    //
    // For the latter case, XRE_ForkServer() will return false, running in a
    // child process just forked from the fork server process. argc & argv will
    // be updated with the values passing from the chrome process, as will
    // GeckoProcessType and GeckoChildID. With the new values, this function
    // continues the reset of the code acting as a child process.
    if (bootstrap->XRE_ForkServer(&argc, &argv)) {
      // Return from the fork server in the fork server process.
      // Stop the fork server.
      bootstrap->NS_LogTerm();
      return 0;
    }
  }
#endif

  // Register an external module to report on otherwise uncatchable
  // exceptions. Note that in child processes this must be called after Gecko
  // process type has been set.
  CrashReporter::RegisterRuntimeExceptionModule();

  // Make sure we unregister the runtime exception module before returning.
  auto unregisterRuntimeExceptionModule =
      MakeScopeExit([] { CrashReporter::UnregisterRuntimeExceptionModule(); });

#ifdef HAS_DLL_BLOCKLIST
  uint32_t initFlags = eDllBlocklistInitFlagIsChildProcess;
  SetDllBlocklistProcessTypeFlags(initFlags, GetGeckoProcessType());
  DllBlocklist_Initialize(initFlags);
#endif

  XREChildData childData;

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
  if (IsSandboxedProcess()) {
    childData.sandboxTargetServices =
        mozilla::sandboxing::GetInitializedTargetServices();
    if (!childData.sandboxTargetServices) {
      return 1;
    }

    childData.ProvideLogFunction = mozilla::sandboxing::ProvideLogFunction;
  }
#endif

  nsresult rv = bootstrap->XRE_InitChildProcess(argc, argv, &childData);

#if defined(DEBUG) && defined(HAS_DLL_BLOCKLIST)
  DllBlocklist_Shutdown();
#endif

  return NS_FAILED(rv) ? 1 : 0;
}
