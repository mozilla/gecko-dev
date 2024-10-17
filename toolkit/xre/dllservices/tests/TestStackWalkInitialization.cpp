/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWindowsHelpers.h"
#include "mozilla/Array.h"
#include "mozilla/Attributes.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/WindowsStackWalkInitialization.h"

#include <windows.h>

#include <cstdio>

#define TEST_FAILED(format, ...)                                               \
  do {                                                                         \
    wprintf(L"TEST-FAILED | TestStackWalkInitialization | " format __VA_OPT__( \
        , ) __VA_ARGS__);                                                      \
    ::exit(1);                                                                 \
  } while (0)

#define TEST_PASS(format, ...)                                               \
  do {                                                                       \
    wprintf(L"TEST-PASS | TestStackWalkInitialization | " format __VA_OPT__( \
        , ) __VA_ARGS__);                                                    \
  } while (0)

#define MAX_TIMEOUT_MS 5000

extern "C" __declspec(dllexport) uint64_t gPseudoLock{};

MOZ_NEVER_INLINE MOZ_NAKED __declspec(dllexport) void LockThroughRegisterRsi() {
  asm volatile(
      // Found in RtlAcquireSRWLockShared
      "lock cmpxchgq %rcx, (%rsi)");
}

MOZ_NEVER_INLINE MOZ_NAKED __declspec(dllexport) void LockThroughRegisterRcx() {
  asm volatile(
      // Found in RtlReleaseSRWLockShared
      "lock cmpxchgq %r10, (%rcx)");
}

MOZ_NEVER_INLINE MOZ_NAKED __declspec(dllexport) void LockThroughRegisterR10() {
  asm volatile("lock cmpxchgq %rcx, (%r10)");
}

MOZ_NEVER_INLINE MOZ_NAKED __declspec(dllexport) void
LockThroughRipRelativeAddr() {
  asm volatile(
      // Found in an inlined call to RtlAcquireSRWLockShared in
      // RtlpxLookupFunctionTable on Windows 10
      "lock cmpxchgq %r11, gPseudoLock(%rip)");
}

void TestLockExtraction() {
  void* extractedLock{};
  CONTEXT context{};

  context.Rip = reinterpret_cast<DWORD64>(LockThroughRegisterRsi);
  context.Rsi = reinterpret_cast<DWORD64>(&gPseudoLock);
  extractedLock = mozilla::ExtractLockFromCurrentCpuContext(&context);
  context.Rsi = 0;
  if (extractedLock != &gPseudoLock) {
    TEST_FAILED(
        L"Failed to extract the lock through register RSI (expected: %p, got: "
        L"%p)\n",
        &gPseudoLock, extractedLock);
  }

  context.Rip = reinterpret_cast<DWORD64>(LockThroughRegisterRcx);
  context.Rcx = reinterpret_cast<DWORD64>(&gPseudoLock);
  extractedLock = mozilla::ExtractLockFromCurrentCpuContext(&context);
  context.Rcx = 0;
  if (extractedLock != &gPseudoLock) {
    TEST_FAILED(
        L"Failed to extract the lock through register RCX (expected: %p, got: "
        L"%p)\n",
        &gPseudoLock, extractedLock);
  }

  context.Rip = reinterpret_cast<DWORD64>(LockThroughRegisterR10);
  context.R10 = reinterpret_cast<DWORD64>(&gPseudoLock);
  extractedLock = mozilla::ExtractLockFromCurrentCpuContext(&context);
  context.R10 = 0;
  if (extractedLock != &gPseudoLock) {
    TEST_FAILED(
        L"Failed to extract the lock through register R10 (expected: %p, got: "
        L"%p)\n",
        &gPseudoLock, extractedLock);
  }

  context.Rip = reinterpret_cast<DWORD64>(LockThroughRipRelativeAddr);
  extractedLock = mozilla::ExtractLockFromCurrentCpuContext(&context);
  if (extractedLock != &gPseudoLock) {
    TEST_FAILED(
        L"Failed to extract the lock through RIP-relative address (expected: "
        L"%p, got: %p)\n",
        &gPseudoLock, extractedLock);
  }

  TEST_PASS(L"Managed to extract the lock with all test patterns\n");
}

void TestLockCollectionAndValidation(
    mozilla::Array<void*, 2>& aStackWalkLocks) {
  if (!mozilla::CollectStackWalkLocks(aStackWalkLocks)) {
    TEST_FAILED(L"Failed to collect stack walk locks\n");
  }

  if (!mozilla::ValidateStackWalkLocks(aStackWalkLocks)) {
    TEST_FAILED(L"Failed to validate stack walk locks\n");
  }

  TEST_PASS(L"Collected and validated locks successfully\n");
}

DWORD WINAPI LookupThreadProc(LPVOID aEvents) {
  auto events = reinterpret_cast<nsAutoHandle*>(aEvents);
  auto& lookupThreadReady = events[0];
  auto& initiateLookup = events[1];
  auto& lookupThreadDone = events[2];

  // Signal that we are ready to enter lookup.
  ::SetEvent(lookupThreadReady);

  // Wait for the main thread to acquire the locks exclusively.
  if (::WaitForSingleObject(initiateLookup, MAX_TIMEOUT_MS) == WAIT_OBJECT_0) {
    // Do a lookup. We are supposed to get stuck until the locks are released.
    DWORD64 imageBase;
    ::RtlLookupFunctionEntry(reinterpret_cast<DWORD64>(LookupThreadProc),
                             &imageBase, nullptr);

    // Signal that we are not or no longer stuck.
    ::SetEvent(lookupThreadDone);
  }

  return 0;
}

// This test checks that the locks in aStackWalkLocks cause
// RtlLookupFunctionEntry to get stuck if they are held exclusively, i.e. there
// is a good chance that these are indeed the locks we are looking for.
void TestLocksPreventLookup(const mozilla::Array<void*, 2>& aStackWalkLocks) {
  nsAutoHandle events[3]{};
  for (int i = 0; i < 3; ++i) {
    nsAutoHandle event(::CreateEventW(nullptr, /* bManualReset */ TRUE,
                                      /* bInitialState */ FALSE, nullptr));
    if (!event) {
      TEST_FAILED(L"Failed to create event %d\n", i);
    }
    events[i].swap(event);
  }

  auto& lookupThreadReady = events[0];
  auto& initiateLookup = events[1];
  auto& lookupThreadDone = events[2];

  nsAutoHandle lookupThread(::CreateThread(nullptr, 0, LookupThreadProc,
                                           reinterpret_cast<void*>(events), 0,
                                           nullptr));
  if (!lookupThread) {
    TEST_FAILED(L"Failed to create lookup thread\n");
  }

  if (::WaitForSingleObject(lookupThreadReady, MAX_TIMEOUT_MS) !=
      WAIT_OBJECT_0) {
    TEST_FAILED(L"Lookup thread did not signal the lookupThreadReady event\n");
  }

  mozilla::Array<SRWLOCK*, 2> stackWalkLocks{
      reinterpret_cast<SRWLOCK*>(aStackWalkLocks[0]),
      reinterpret_cast<SRWLOCK*>(aStackWalkLocks[1])};
  if (!::TryAcquireSRWLockExclusive(stackWalkLocks[0])) {
    TEST_FAILED(L"Failed to acquire lock 0\n");
  }
  if (!::TryAcquireSRWLockExclusive(stackWalkLocks[1])) {
    ::ReleaseSRWLockExclusive(stackWalkLocks[0]);
    TEST_FAILED(L"Failed to acquire lock 1\n");
  }

  {
    auto onExitScope = mozilla::MakeScopeExit([&stackWalkLocks]() {
      ::ReleaseSRWLockExclusive(stackWalkLocks[1]);
      ::ReleaseSRWLockExclusive(stackWalkLocks[0]);
    });

    if (!::SetEvent(initiateLookup)) {
      TEST_FAILED(L"Failed to signal the initiateLookup event\n");
    }

    if (::WaitForSingleObject(lookupThreadDone, MAX_TIMEOUT_MS) !=
        WAIT_TIMEOUT) {
      TEST_FAILED(
          L"Lookup thread was not stuck during lookup while we acquired the "
          L"locks exclusively\n");
    }
  }

  if (::WaitForSingleObject(lookupThreadDone, MAX_TIMEOUT_MS) !=
      WAIT_OBJECT_0) {
    TEST_FAILED(
        L"Lookup thread did not signal the lookupThreadDone event after locks "
        L"were released\n");
  }

  TEST_PASS(L"Locks prevented lookup while acquired exclusively\n");
}

int wmain(int argc, wchar_t* argv[]) {
  TestLockExtraction();

  mozilla::Array<void*, 2> stackWalkLocks;
  TestLockCollectionAndValidation(stackWalkLocks);

  TestLocksPreventLookup(stackWalkLocks);

  return 0;
}
