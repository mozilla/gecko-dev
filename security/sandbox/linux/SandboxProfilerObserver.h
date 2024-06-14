/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef SANDBOX_PROFILER_STARTUP_H
#define SANDBOX_PROFILER_STARTUP_H

#include "mozilla/Services.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsISupportsImpl.h"

/*
 * This code is here to help bring up SandboxProfiler whenever the profiler
 * is started by the user. We cannot have that code live within
 * SandboxProfiler.cpp itself because we rely on libxul facilities, while the
 * sandbox code lives within libmozsandbox.
 */

namespace {

class ProfilerStartupObserverForSandboxProfiler final : public nsIObserver {
  ~ProfilerStartupObserverForSandboxProfiler() = default;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

NS_IMPL_ISUPPORTS(ProfilerStartupObserverForSandboxProfiler, nsIObserver)

NS_IMETHODIMP ProfilerStartupObserverForSandboxProfiler::Observe(
    nsISupports* aSubject, const char* aTopic, const char16_t* aData) {
  if (strcmp(aTopic, "profiler-started") == 0) {
    mozilla::CreateSandboxProfiler();
    return NS_OK;
  }

  if (strcmp(aTopic, "profiler-stopped") == 0) {
    mozilla::DestroySandboxProfiler();
    return NS_OK;
  }

  return NS_OK;
}

}  // anonymous namespace

inline void RegisterProfilerObserversForSandboxProfiler() {
  nsCOMPtr<nsIObserverService> obsServ(mozilla::services::GetObserverService());
  MOZ_ASSERT(!!obsServ);
  if (!obsServ) {
    return;
  }

  nsCOMPtr<nsIObserver> obs(new ProfilerStartupObserverForSandboxProfiler());
  obsServ->AddObserver(obs, "profiler-started", false);
  obsServ->AddObserver(obs, "profiler-stopped", false);
}

#endif  // SANDBOX_PROFILER_STARTUP_H
