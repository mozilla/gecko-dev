/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/WindowsStackWalkInitialization.h"

#include "nsWindowsDllInterceptor.h"
#include "mozilla/NativeNt.h"
#include "mozilla/StackWalk_windows.h"
#include "mozilla/WindowsDiagnostics.h"

namespace mozilla {

#if defined(_M_AMD64) || defined(_M_ARM64)
MOZ_RUNINIT static WindowsDllInterceptor NtDllIntercept;

typedef NTSTATUS(NTAPI* LdrUnloadDll_func)(HMODULE module);
static WindowsDllInterceptor::FuncHookType<LdrUnloadDll_func> stub_LdrUnloadDll;

static NTSTATUS NTAPI patched_LdrUnloadDll(HMODULE module) {
  // Prevent the stack walker from suspending this thread when LdrUnloadDll
  // holds the RtlLookupFunctionEntry lock.
  AutoSuppressStackWalking suppress;
  return stub_LdrUnloadDll(module);
}

// These pointers are disguised as PVOID to avoid pulling in obscure headers
typedef PVOID(WINAPI* LdrResolveDelayLoadedAPI_func)(
    PVOID ParentModuleBase, PVOID DelayloadDescriptor, PVOID FailureDllHook,
    PVOID FailureSystemHook, PVOID ThunkAddress, ULONG Flags);
static WindowsDllInterceptor::FuncHookType<LdrResolveDelayLoadedAPI_func>
    stub_LdrResolveDelayLoadedAPI;

static PVOID WINAPI patched_LdrResolveDelayLoadedAPI(
    PVOID ParentModuleBase, PVOID DelayloadDescriptor, PVOID FailureDllHook,
    PVOID FailureSystemHook, PVOID ThunkAddress, ULONG Flags) {
  // Prevent the stack walker from suspending this thread when
  // LdrResolveDelayLoadAPI holds the RtlLookupFunctionEntry lock.
  AutoSuppressStackWalking suppress;
  return stub_LdrResolveDelayLoadedAPI(ParentModuleBase, DelayloadDescriptor,
                                       FailureDllHook, FailureSystemHook,
                                       ThunkAddress, Flags);
}

void WindowsStackWalkInitialization() {
  // This function could be called by both profilers, but we only want to run
  // it once.
  static bool ran = false;
  if (ran) {
    return;
  }
  ran = true;

  // Attempt to initialize strategy (1) for avoiding deadlocks. See comments in
  // StackWalk.cpp near InitializeStackWalkLocks().
  Array<void*, 2> stackWalkLocks;
  if (CollectStackWalkLocks(stackWalkLocks)) {
    bool locksArePlausible = ValidateStackWalkLocks(stackWalkLocks);

    // If this crashes then most likely our lock collection code is broken.
    MOZ_ASSERT(locksArePlausible);

    if (locksArePlausible) {
      InitializeStackWalkLocks(stackWalkLocks);
      return;
    }
  }

  // Strategy (2): We will rely on stack walk suppressions. We use hooking
  // to install stack walk suppression on specific Windows calls which are
  // known to acquire the locks exclusively. Some of these calls, e.g.
  // LdrLoadDll, are already hooked by other parts of our code base; in this
  // case the stack walk suppressions are already added there directly.
  NtDllIntercept.Init("ntdll.dll");
  stub_LdrUnloadDll.Set(NtDllIntercept, "LdrUnloadDll", &patched_LdrUnloadDll);
  stub_LdrResolveDelayLoadedAPI.Set(NtDllIntercept, "LdrResolveDelayLoadedAPI",
                                    &patched_LdrResolveDelayLoadedAPI);
}

[[clang::optnone]] void UnoptimizedLookup() {
  DWORD64 imageBase;
  ::RtlLookupFunctionEntry(0, &imageBase, nullptr);
}

MFBT_API
bool CollectStackWalkLocks(Array<void*, 2>& aStackWalkLocks) {
// At the moment we are only capable of enabling strategy (1) for x86-64
// because WindowsDiagnostics.h does not implement single-stepping for arm64.
#  if defined(_M_AMD64)
  struct LockCollectionData {
    Array<void*, 2> mCollectedLocks;
    int mCollectedLocksCount;
    DebugOnly<bool> mLookupCalled;
  };

  LockCollectionData data{};

  // Do a single-stepped call to RtlLookupFunctionEntry, and monitor the calls
  // to RtlAcquireSRWLockShared and RtlReleaseSRWLockShared.
  WindowsDiagnosticsError error = CollectSingleStepData(
      UnoptimizedLookup,
      [](void* aState, CONTEXT* aContext) {
        LockCollectionData& data =
            *reinterpret_cast<LockCollectionData*>(aState);

#    ifdef DEBUG
        if (aContext->Rip ==
            reinterpret_cast<DWORD64>(::RtlLookupFunctionEntry)) {
          data.mLookupCalled = true;
        }
#    endif

        void* lock = ExtractLockFromCurrentCpuContext(aContext);
        if (lock) {
          bool alreadyCollected = false;
          for (auto collectedLock : data.mCollectedLocks) {
            if (collectedLock == lock) {
              alreadyCollected = true;
              break;
            }
          }
          if (!alreadyCollected) {
            if (data.mCollectedLocksCount <
                std::numeric_limits<
                    decltype(data.mCollectedLocksCount)>::max()) {
              ++data.mCollectedLocksCount;
            }
            if (data.mCollectedLocksCount <= 2) {
              data.mCollectedLocks[data.mCollectedLocksCount - 1] = lock;
            }
          }
        }

        // Continue single-stepping
        return true;
      },
      &data);

  // We only expect to fail if a debugger is present.
  MOZ_ASSERT(error == WindowsDiagnosticsError::None ||
             error == WindowsDiagnosticsError::DebuggerPresent);

  if (error != WindowsDiagnosticsError::None) {
    return false;
  }

  // Crashing here most likely means that the optimizer was too aggressive.
  MOZ_ASSERT(data.mLookupCalled);

  // If we managed to collect exactly two locks, then we assume that these
  // are the locks we are looking for.
  bool isAcquisitionSuccessful = data.mCollectedLocksCount == 2;

  // We always expect that RtlLookupFunctionEntry's behavior results in a
  // successful acquisition. If this crashes then we likely failed to detect
  // the instructions that acquire and release the locks in our function
  // ExtractLockFromCurrentCpuContext.
  MOZ_ASSERT(isAcquisitionSuccessful);
  if (!isAcquisitionSuccessful) {
    return false;
  }

  aStackWalkLocks[0] = data.mCollectedLocks[0];
  aStackWalkLocks[1] = data.mCollectedLocks[1];
  return true;
#  else
  return false;
#  endif  // _M_AMD64
}

// Based on a single-step CPU context, extract a pointer to a lock that is
// being acquired or released (if any).
MFBT_API
void* ExtractLockFromCurrentCpuContext(void* aContext) {
#  if defined(_M_AMD64)
  // rex bits
  constexpr BYTE kMaskHighNibble = 0xF0;
  constexpr BYTE kRexOpcode = 0x40;
  constexpr BYTE kMaskRexW = 0x08;
  constexpr BYTE kMaskRexB = 0x01;

  // mod r/m bits
  constexpr BYTE kMaskMod = 0xC0;
  constexpr BYTE kMaskRm = 0x07;
  constexpr BYTE kModNoRegDisp = 0x00;
  constexpr BYTE kRmNeedSib = 0x04;
  constexpr BYTE kRmNoRegDispDisp32 = 0x05;

  auto context = reinterpret_cast<CONTEXT*>(aContext);
  auto opcode = reinterpret_cast<uint8_t*>(context->Rip);
  // lock rex.w(?rxb) cmpxchg r/m64, r64
  if (opcode[0] == 0xf0 &&
      (opcode[1] & (kMaskHighNibble | kMaskRexW)) == (kRexOpcode | kMaskRexW) &&
      opcode[2] == 0x0f && opcode[3] == 0xb1) {
    if ((opcode[4] & kMaskMod) == kModNoRegDisp) {
      BYTE const rm = opcode[4] & kMaskRm;  // low 3 bits, no offset

      if (rm == kRmNeedSib) {
        // uses SIB byte; decoding not implemented
        return nullptr;
      }

      if (rm == kRmNoRegDispDisp32) {
        // rip-relative
        return reinterpret_cast<void*>(
            static_cast<int64_t>(context->Rip) + 9i64 +
            static_cast<int64_t>(*reinterpret_cast<int32_t*>(opcode + 5)));
      }

      // otherwise, this reads/writes from [reg] -- and conveniently, the
      // registers in the CONTEXT struct form an indexable subarray in "opcode
      // order"
      BYTE const regIndex = ((opcode[1] & kMaskRexB) << 3) | rm;
      DWORD64 const regValue = (&context->Rax)[regIndex];
      return reinterpret_cast<void*>(regValue);
    }
  }
  return nullptr;
#  else
  return nullptr;
#  endif  // _M_AMD64
}

MFBT_API
bool ValidateStackWalkLocks(const Array<void*, 2>& aStackWalkLocks) {
  if (!aStackWalkLocks[0] || !aStackWalkLocks[1]) {
    return false;
  }

  // We check that the pointers live in ntdll's .data section as a best effort.
  mozilla::nt::PEHeaders ntdllImage(::GetModuleHandleW(L"ntdll.dll"));
  if (!ntdllImage) {
    return false;
  }

  auto dataSection = ntdllImage.GetDataSectionInfo();
  if (dataSection.isNothing()) {
    return false;
  }

  return dataSection.isSome() &&
         &*dataSection->cbegin() <= aStackWalkLocks[0] &&
         aStackWalkLocks[0] <= &*(dataSection->cend() - 1) &&
         &*dataSection->cbegin() <= aStackWalkLocks[1] &&
         aStackWalkLocks[1] <= &*(dataSection->cend() - 1);
}

#endif  // _M_AMD64 || _M_ARM64

}  // namespace mozilla
