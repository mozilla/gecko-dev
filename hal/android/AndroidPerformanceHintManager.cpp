/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "Hal.h"
#include "HalLog.h"
#include "HalTypes.h"

#include "AndroidBuild.h"

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <android/performance_hint.h>

typedef struct APerformanceHintManager APerformanceHintManager;
typedef struct APerformanceHintSession APerformanceHintSession;

namespace mozilla {
namespace hal_impl {

class __attribute__((
    availability(android, introduced = 33))) AndroidPerformanceHintSession final
    : public hal::PerformanceHintSession {
  // Creates a PerformanceHintSession wrapping the provided NDK
  // APerformanceHintSession instance. This assumes ownership of aSession,
  // therefore the caller must not close the session itself.
  explicit AndroidPerformanceHintSession(APerformanceHintSession* aSession)
      : mSession(aSession) {}

 public:
  AndroidPerformanceHintSession(AndroidPerformanceHintSession& aOther) = delete;
  AndroidPerformanceHintSession(AndroidPerformanceHintSession&& aOther) {
    mSession = aOther.mSession;
    aOther.mSession = nullptr;
  }

  static UniquePtr<AndroidPerformanceHintSession> Create(
      APerformanceHintManager* manager, const nsTArray<pid_t>& threads,
      int64_t initialTargetWorkDurationNanos) {
    if (APerformanceHintSession* session = APerformanceHint_createSession(
            manager, threads.Elements(), threads.Length(),
            initialTargetWorkDurationNanos)) {
      return WrapUnique(new AndroidPerformanceHintSession(session));
    } else {
      return nullptr;
    }
  }

  ~AndroidPerformanceHintSession() {
    if (mSession) {
      APerformanceHint_closeSession(mSession);
    }
  }

  void UpdateTargetWorkDuration(TimeDuration aDuration) override {
    APerformanceHint_updateTargetWorkDuration(
        mSession, aDuration.ToMicroseconds() * 1000);
  }

  void ReportActualWorkDuration(TimeDuration aDuration) override {
    APerformanceHint_reportActualWorkDuration(
        mSession, aDuration.ToMicroseconds() * 1000);
  }

 private:
  APerformanceHintSession* mSession;
};

static APerformanceHintManager* InitManager() {
  if (__builtin_available(android 33, *)) {
    // At the time of writing we are only aware of PerformanceHintManager being
    // implemented on Tensor devices (Pixel 6 and 7 families). On most devices
    // createSession() will simply return null. However, on some devices
    // createSession() does return a session but scheduling does not appear to
    // be affected in any way. Rather than pretending to the caller that
    // PerformanceHintManager is available on such devices, return null allowing
    // them to use another means of achieving the performance they require.
    const auto socManufacturer =
        java::sdk::Build::SOC_MANUFACTURER()->ToString();
    if (!socManufacturer.EqualsASCII("Google")) {
      return nullptr;
    }

    return APerformanceHint_getManager();
  } else {
    return nullptr;
  }
}

UniquePtr<hal::PerformanceHintSession> CreatePerformanceHintSession(
    const nsTArray<PlatformThreadHandle>& aThreads,
    mozilla::TimeDuration aTargetWorkDuration) {
  // C++ guarantees local static variable initialization is thread safe
  static APerformanceHintManager* manager = InitManager();

  if (!manager) {
    return nullptr;
  }

  nsTArray<pid_t> tids(aThreads.Length());
  std::transform(aThreads.cbegin(), aThreads.cend(), MakeBackInserter(tids),
                 [](pthread_t handle) { return pthread_gettid_np(handle); });

  if (__builtin_available(android 33, *)) {
    return AndroidPerformanceHintSession::Create(
        manager, tids, aTargetWorkDuration.ToMicroseconds() * 1000);
  } else {
    return nullptr;
  }
}

}  // namespace hal_impl
}  // namespace mozilla
