/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// PHC is a probabilistic heap checker. A tiny fraction of randomly chosen heap
// allocations are subject to some expensive checking via the use of OS page
// access protection. A failed check triggers a crash, whereupon useful
// information about the failure is put into the crash report. The cost and
// coverage for each user is minimal, but spread over the entire user base the
// coverage becomes significant.
//
// The idea comes from Chromium, where it is called GWP-ASAN. (Firefox uses PHC
// as the name because GWP-ASAN is long, awkward, and doesn't have any
// particular meaning.)
//
// In the current implementation up to 64 allocations per process can become
// PHC allocations. These allocations must be page-sized or smaller. Each PHC
// allocation gets its own page, and when the allocation is freed its page is
// marked inaccessible until the page is reused for another allocation. This
// means that a use-after-free defect (which includes double-frees) will be
// caught if the use occurs before the page is reused for another allocation.
// The crash report will contain stack traces for the allocation site, the free
// site, and the use-after-free site, which is often enough to diagnose the
// defect.
//
// Also, each PHC allocation is followed by a guard page. The PHC allocation is
// positioned so that its end abuts the guard page (or as close as possible,
// given alignment constraints). This means that a bounds violation at the end
// of the allocation (overflow) will be caught. The crash report will contain
// stack traces for the allocation site and the bounds violation use site,
// which is often enough to diagnose the defect.
//
// (A bounds violation at the start of the allocation (underflow) will not be
// caught, unless it is sufficiently large to hit the preceding allocation's
// guard page, which is not that likely. It would be possible to look more
// assiduously for underflow by randomly placing some allocations at the end of
// the page and some at the start of the page, and GWP-ASAN does this. PHC does
// not, however, because overflow is likely to be much more common than
// underflow in practice.)
//
// We use a simple heuristic to categorize a guard page access as overflow or
// underflow: if the address falls in the lower half of the guard page, we
// assume it is overflow, otherwise we assume it is underflow. More
// sophisticated heuristics are possible, but this one is very simple, and it is
// likely that most overflows/underflows in practice are very close to the page
// boundary.
//
// The design space for the randomization strategy is large. The current
// implementation has a large random delay before it starts operating, and a
// small random delay between each PHC allocation attempt. Each freed PHC
// allocation is quarantined for a medium random delay before being reused, in
// order to increase the chance of catching UAFs.
//
// The basic cost of PHC's operation is as follows.
//
// - The physical memory cost is 64 pages plus some metadata (including stack
//   traces) for each page. This amounts to 256 KiB per process on
//   architectures with 4 KiB pages and 1024 KiB on macOS/AArch64 which uses
//   16 KiB pages.
//
// - The virtual memory cost is the physical memory cost plus the guard pages:
//   another 64 pages. This amounts to another 256 KiB per process on
//   architectures with 4 KiB pages and 1024 KiB on macOS/AArch64 which uses
//   16 KiB pages. PHC is currently only enabled on 64-bit platforms so the
//   impact of the virtual memory usage is negligible.
//
// - Every allocation requires a size check and a decrement-and-check of an
//   atomic counter. When the counter reaches zero a PHC allocation can occur,
//   which involves marking a page as accessible and getting a stack trace for
//   the allocation site. Otherwise, mozjemalloc performs the allocation.
//
// - Every deallocation requires a range check on the pointer to see if it
//   involves a PHC allocation. (The choice to only do PHC allocations that are
//   a page or smaller enables this range check, because the 64 pages are
//   contiguous. Allowing larger allocations would make this more complicated,
//   and we definitely don't want something as slow as a hash table lookup on
//   every deallocation.) PHC deallocations involve marking a page as
//   inaccessible and getting a stack trace for the deallocation site.
//
// Note that calls to realloc(), free(), and malloc_usable_size() will
// immediately crash if the given pointer falls within a page allocation's
// page, but does not point to the start of the allocation itself.
//
//   void* p = malloc(64);
//   free(p + 1);     // p+1 doesn't point to the allocation start; crash
//
// Such crashes will not have the PHC fields in the crash report.
//
// PHC-specific tests can be run with the following commands:
// - gtests: `./mach gtest '*PHC*'`
// - xpcshell-tests: `./mach test toolkit/crashreporter/test/unit`
//   - This runs some non-PHC tests as well.

#include "PHC.h"

#include <stdlib.h>
#include <time.h>

#include <algorithm>

#ifdef XP_WIN
#  include <process.h>
#else
#  include <sys/mman.h>
#  include <sys/types.h>
#  include <pthread.h>
#  include <unistd.h>
#endif

#include "mozjemalloc.h"

#include "mozjemalloc.h"
#include "FdPrintf.h"
#include "Mutex.h"
#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/Maybe.h"
#include "mozilla/StackWalk.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/XorShift128PlusRNG.h"

using namespace mozilla;

//---------------------------------------------------------------------------
// Utilities
//---------------------------------------------------------------------------

#ifdef ANDROID
// Android doesn't have pthread_atfork defined in pthread.h.
extern "C" MOZ_EXPORT int pthread_atfork(void (*)(void), void (*)(void),
                                         void (*)(void));
#endif

#ifndef DISALLOW_COPY_AND_ASSIGN
#  define DISALLOW_COPY_AND_ASSIGN(T) \
    T(const T&);                      \
    void operator=(const T&)
#endif

// This class provides infallible operations for the small number of heap
// allocations that PHC does for itself. It would be nice if we could use the
// InfallibleAllocPolicy from mozalloc, but PHC cannot use mozalloc.
class InfallibleAllocPolicy {
 public:
  static void AbortOnFailure(const void* aP) {
    if (!aP) {
      MOZ_CRASH("PHC failed to allocate");
    }
  }

  template <class T>
  static T* new_() {
    void* p = MozJemalloc::malloc(sizeof(T));
    AbortOnFailure(p);
    return new (p) T;
  }
};

//---------------------------------------------------------------------------
// Stack traces
//---------------------------------------------------------------------------

// This code is similar to the equivalent code within DMD.

class StackTrace : public phc::StackTrace {
 public:
  StackTrace() = default;

  void Clear() { mLength = 0; }

  void Fill();

 private:
  static void StackWalkCallback(uint32_t aFrameNumber, void* aPc, void* aSp,
                                void* aClosure) {
    StackTrace* st = (StackTrace*)aClosure;
    MOZ_ASSERT(st->mLength < kMaxFrames);
    st->mPcs[st->mLength] = aPc;
    st->mLength++;
    MOZ_ASSERT(st->mLength == aFrameNumber);
  }
};

// WARNING WARNING WARNING: this function must only be called when PHC::mMutex
// is *not* locked, otherwise we might get deadlocks.
//
// How? On Windows, MozStackWalk() can lock a mutex, M, from the shared library
// loader. Another thread might call malloc() while holding M locked (when
// loading a shared library) and try to lock PHC::mMutex, causing a deadlock.
// So PHC::mMutex can't be locked during the call to MozStackWalk(). (For
// details, see https://bugzilla.mozilla.org/show_bug.cgi?id=374829#c8. On
// Linux, something similar can happen; see bug 824340. So we just disallow it
// on all platforms.)
//
// In DMD, to avoid this problem we temporarily unlock the equivalent mutex for
// the MozStackWalk() call. But that's grotty, and things are a bit different
// here, so we just require that stack traces be obtained before locking
// PHC::mMutex.
//
// Unfortunately, there is no reliable way at compile-time or run-time to ensure
// this pre-condition. Hence this large comment.
//
void StackTrace::Fill() {
  mLength = 0;

// These ifdefs should be kept in sync with the conditions in
// phc_implies_frame_pointers in build/moz.configure/memory.configure
#if defined(XP_WIN) && defined(_M_IX86)
  // This avoids MozStackWalk(), which causes unusably slow startup on Win32
  // when it is called during static initialization (see bug 1241684).
  //
  // This code is cribbed from the Gecko Profiler, which also uses
  // FramePointerStackWalk() on Win32: Registers::SyncPopulate() for the
  // frame pointer, and GetStackTop() for the stack end.
  CONTEXT context;
  RtlCaptureContext(&context);
  void** fp = reinterpret_cast<void**>(context.Ebp);

  PNT_TIB pTib = reinterpret_cast<PNT_TIB>(NtCurrentTeb());
  void* stackEnd = static_cast<void*>(pTib->StackBase);
  FramePointerStackWalk(StackWalkCallback, kMaxFrames, this, fp, stackEnd);
#elif defined(XP_DARWIN)
  // This avoids MozStackWalk(), which has become unusably slow on Mac due to
  // changes in libunwind.
  //
  // This code is cribbed from the Gecko Profiler, which also uses
  // FramePointerStackWalk() on Mac: Registers::SyncPopulate() for the frame
  // pointer, and GetStackTop() for the stack end.
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wframe-address"
  void** fp = reinterpret_cast<void**>(__builtin_frame_address(1));
#  pragma GCC diagnostic pop
  void* stackEnd = pthread_get_stackaddr_np(pthread_self());
  FramePointerStackWalk(StackWalkCallback, kMaxFrames, this, fp, stackEnd);
#else
  MozStackWalk(StackWalkCallback, nullptr, kMaxFrames, this);
#endif
}

//---------------------------------------------------------------------------
// Logging
//---------------------------------------------------------------------------

// Change this to 1 to enable some PHC logging. Useful for debugging.
#define PHC_LOGGING 0

#if PHC_LOGGING

static size_t GetPid() { return size_t(getpid()); }

static size_t GetTid() {
#  if defined(XP_WIN)
  return size_t(GetCurrentThreadId());
#  else
  return size_t(pthread_self());
#  endif
}

#  if defined(XP_WIN)
#    define LOG_STDERR \
      reinterpret_cast<intptr_t>(GetStdHandle(STD_ERROR_HANDLE))
#  else
#    define LOG_STDERR 2
#  endif
#  define LOG(fmt, ...)                                                \
    FdPrintf(LOG_STDERR, "PHC[%zu,%zu,~%zu] " fmt, GetPid(), GetTid(), \
             size_t(PHC::Now()), ##__VA_ARGS__)

#else

#  define LOG(fmt, ...)

#endif  // PHC_LOGGING

//---------------------------------------------------------------------------
// Global state
//---------------------------------------------------------------------------

// Throughout this entire file time is measured as the number of sub-page
// allocations performed (by PHC and mozjemalloc combined). `Time` is 64-bit
// because we could have more than 2**32 allocations in a long-running session.
// `Delay` is 32-bit because the delays used within PHC are always much smaller
// than 2**32.  Delay must be unsigned so that IsPowerOfTwo() can work on some
// Delay values.
using Time = uint64_t;   // A moment in time.
using Delay = uint32_t;  // A time duration.
static constexpr Delay DELAY_MAX = UINT32_MAX / 2;

// PHC only runs if the page size is 4 KiB; anything more is uncommon and would
// use too much memory. So we hardwire this size for all platforms but macOS
// on ARM processors. For the latter we make an exception because the minimum
// page size supported is 16KiB so there's no way to go below that.
static const size_t kPageSize =
#if defined(XP_DARWIN) && defined(__aarch64__)
    16384
#else
    4096
#endif
    ;

// We align the PHC area to a multiple of the jemalloc and JS GC chunk size
// (both use 1MB aligned chunks) so that their address computations don't lead
// from non-PHC memory into PHC memory causing misleading PHC stacks to be
// attached to a crash report.
static const size_t kPhcAlign = 1024 * 1024;

static_assert(IsPowerOfTwo(kPhcAlign));
static_assert((kPhcAlign % kPageSize) == 0);

// There are two kinds of page.
// - Allocation pages, from which allocations are made.
// - Guard pages, which are never touched by PHC.
//
// These page kinds are interleaved; each allocation page has a guard page on
// either side.
#ifdef EARLY_BETA_OR_EARLIER
static const size_t kNumAllocPages = kPageSize == 4096 ? 4096 : 1024;
#else
// This will use between 82KiB and 1.1MiB per process (depending on how many
// objects are currently allocated).  We will tune this in the future.
static const size_t kNumAllocPages = kPageSize == 4096 ? 256 : 64;
#endif
static const size_t kNumAllPages = kNumAllocPages * 2 + 1;

// The total size of the allocation pages and guard pages.
static const size_t kAllPagesSize = kNumAllPages * kPageSize;

// jemalloc adds a guard page to the end of our allocation, see the comment in
// AllocAllPages() for more information.
static const size_t kAllPagesJemallocSize = kAllPagesSize - kPageSize;

// The amount to decrement from the shared allocation delay each time a thread's
// local allocation delay reaches zero.
static const Delay kDelayDecrementAmount = 256;

// When PHC is disabled on the current thread wait this many allocations before
// accessing sAllocDelay once more.
static const Delay kDelayBackoffAmount = 64;

// When PHC is disabled globally reset the shared delay by this many allocations
// to keep code running on the fast path.
static const Delay kDelayResetWhenDisabled = 64 * 1024;

// The default state for PHC.  Either Enabled or OnlyFree.
#define DEFAULT_STATE mozilla::phc::OnlyFree

// The maximum time.
static const Time kMaxTime = ~(Time(0));

// Truncate aRnd to the range (1 .. aAvgDelay*2). If aRnd is random, this
// results in an average value of aAvgDelay + 0.5, which is close enough to
// aAvgDelay. aAvgDelay must be a power-of-two for speed.
constexpr Delay Rnd64ToDelay(Delay aAvgDelay, uint64_t aRnd) {
  MOZ_ASSERT(IsPowerOfTwo(aAvgDelay), "must be a power of two");

  return (aRnd & (uint64_t(aAvgDelay) * 2 - 1)) + 1;
}

static Delay CheckProbability(int64_t aProb) {
  // Limit delays calculated from prefs to 0x80000000, this is the largest
  // power-of-two that fits in a Delay since it is a uint32_t.
  // The minimum is 2 that way not every allocation goes straight to PHC.
  return RoundUpPow2(std::clamp(aProb, int64_t(2), int64_t(0x80000000)));
}

// Maps a pointer to a PHC-specific structure:
// - Nothing
// - A guard page (it is unspecified which one)
// - An allocation page (with an index < kNumAllocPages)
//
// The standard way of handling a PtrKind is to check IsNothing(), and if that
// fails, to check IsGuardPage(), and if that fails, to call AllocPage().
class PtrKind {
 private:
  enum class Tag : uint8_t {
    Nothing,
    GuardPage,
    AllocPage,
  };

  Tag mTag;
  uintptr_t mIndex;  // Only used if mTag == Tag::AllocPage.

 public:
  // Detect what a pointer points to. This constructor must be fast because it
  // is called for every call to free(), realloc(), malloc_usable_size(), and
  // jemalloc_ptr_info().
  PtrKind(const void* aPtr, const uint8_t* aPagesStart,
          const uint8_t* aPagesLimit) {
    if (!(aPagesStart <= aPtr && aPtr < aPagesLimit)) {
      mTag = Tag::Nothing;
    } else {
      uintptr_t offset = static_cast<const uint8_t*>(aPtr) - aPagesStart;
      uintptr_t allPageIndex = offset / kPageSize;
      MOZ_ASSERT(allPageIndex < kNumAllPages);
      if (allPageIndex & 1) {
        // Odd-indexed pages are allocation pages.
        uintptr_t allocPageIndex = allPageIndex / 2;
        MOZ_ASSERT(allocPageIndex < kNumAllocPages);
        mTag = Tag::AllocPage;
        mIndex = allocPageIndex;
      } else {
        // Even-numbered pages are guard pages.
        mTag = Tag::GuardPage;
      }
    }
  }

  bool IsNothing() const { return mTag == Tag::Nothing; }
  bool IsGuardPage() const { return mTag == Tag::GuardPage; }

  // This should only be called after IsNothing() and IsGuardPage() have been
  // checked and failed.
  uintptr_t AllocPageIndex() const {
    MOZ_RELEASE_ASSERT(mTag == Tag::AllocPage);
    return mIndex;
  }
};

// On MacOS, the first __thread/thread_local access calls malloc, which leads
// to an infinite loop. So we use pthread-based TLS instead, which somehow
// doesn't have this problem.
#if !defined(XP_DARWIN)
#  define PHC_THREAD_LOCAL(T) MOZ_THREAD_LOCAL(T)
#else
#  define PHC_THREAD_LOCAL(T) \
    detail::ThreadLocal<T, detail::ThreadLocalKeyStorage>
#endif

// The virtual address space reserved by PHC.  It is shared, immutable global
// state. Initialized by phc_init() and never changed after that. phc_init()
// runs early enough that no synchronization is needed.
class PHCRegion {
 private:
  // The bounds of the allocated pages.
  uint8_t* const mPagesStart;
  uint8_t* const mPagesLimit;

  // Allocates the allocation pages and the guard pages, contiguously.
  uint8_t* AllocAllPages() {
    // The memory allocated here is never freed, because it would happen at
    // process termination when it would be of little use.

    // We can rely on jemalloc's behaviour that when it allocates memory aligned
    // with its own chunk size it will over-allocate and guarantee that the
    // memory after the end of our allocation, but before the next chunk, is
    // decommitted and inaccessible. Elsewhere in PHC we assume that we own
    // that page (so that memory errors in it get caught by PHC) but here we
    // use kAllPagesJemallocSize which subtracts jemalloc's guard page.
    void* pages = MozJemalloc::memalign(kPhcAlign, kAllPagesJemallocSize);
    if (!pages) {
      MOZ_CRASH();
    }

    // Make the pages inaccessible.
#ifdef XP_WIN
    if (!VirtualFree(pages, kAllPagesJemallocSize, MEM_DECOMMIT)) {
      MOZ_CRASH("VirtualFree failed");
    }
#else
    if (mmap(pages, kAllPagesJemallocSize, PROT_NONE,
             MAP_FIXED | MAP_PRIVATE | MAP_ANON, -1, 0) == MAP_FAILED) {
      MOZ_CRASH("mmap failed");
    }
#endif

    return static_cast<uint8_t*>(pages);
  }

 public:
  PHCRegion();

  class PtrKind PtrKind(const void* aPtr) {
    class PtrKind pk(aPtr, mPagesStart, mPagesLimit);
    return pk;
  }

  bool IsInFirstGuardPage(const void* aPtr) {
    return mPagesStart <= aPtr && aPtr < mPagesStart + kPageSize;
  }

  // Get the address of the allocation page referred to via an index. Used when
  // marking the page as accessible/inaccessible.
  uint8_t* AllocPagePtr(uintptr_t aIndex) {
    MOZ_ASSERT(aIndex < kNumAllocPages);
    // Multiply by two and add one to account for allocation pages *and* guard
    // pages.
    return mPagesStart + (2 * aIndex + 1) * kPageSize;
  }
};

// This type is used as a proof-of-lock token, to make it clear which functions
// require mMutex to be locked.
using PHCLock = const MutexAutoLock&;

// Shared, mutable global state.  Many fields are protected by sMutex; functions
// that access those feilds should take a PHCLock as proof that mMutex is held.
// Other fields are TLS or Atomic and don't need the lock.
class PHC {
  enum class AllocPageState {
    NeverAllocated = 0,
    InUse = 1,
    Freed = 2,
  };

  // Metadata for each allocation page.
  class AllocPageInfo {
   public:
    AllocPageInfo()
        : mState(AllocPageState::NeverAllocated),
          mBaseAddr(nullptr),
          mReuseTime(0) {}

    // The current allocation page state.
    AllocPageState mState;

    // The arena that the allocation is nominally from. This isn't meaningful
    // within PHC, which has no arenas. But it is necessary for reallocation of
    // page allocations as normal allocations, such as in this code:
    //
    //   p = moz_arena_malloc(arenaId, 4096);
    //   realloc(p, 8192);
    //
    // The realloc is more than one page, and thus too large for PHC to handle.
    // Therefore, if PHC handles the first allocation, it must ask mozjemalloc
    // to allocate the 8192 bytes in the correct arena, and to do that, it must
    // call MozJemalloc::moz_arena_malloc with the correct arenaId under the
    // covers. Therefore it must record that arenaId.
    //
    // This field is also needed for jemalloc_ptr_info() to work, because it
    // also returns the arena ID (but only in debug builds).
    //
    // - NeverAllocated: must be 0.
    // - InUse | Freed: can be any valid arena ID value.
    Maybe<arena_id_t> mArenaId;

    // The starting address of the allocation. Will not be the same as the page
    // address unless the allocation is a full page.
    // - NeverAllocated: must be 0.
    // - InUse | Freed: must be within the allocation page.
    uint8_t* mBaseAddr;

    // Usable size is computed as the number of bytes between the pointer and
    // the end of the allocation page. This might be bigger than the requested
    // size, especially if an outsized alignment is requested.
    size_t UsableSize() const {
      return mState == AllocPageState::NeverAllocated
                 ? 0
                 : kPageSize - (reinterpret_cast<uintptr_t>(mBaseAddr) &
                                (kPageSize - 1));
    }

    // The internal fragmentation for this allocation.
    size_t FragmentationBytes() const {
      MOZ_ASSERT(kPageSize >= UsableSize());
      return mState == AllocPageState::InUse ? kPageSize - UsableSize() : 0;
    }

    // The allocation stack.
    // - NeverAllocated: Nothing.
    // - InUse | Freed: Some.
    Maybe<StackTrace> mAllocStack;

    // The free stack.
    // - NeverAllocated | InUse: Nothing.
    // - Freed: Some.
    Maybe<StackTrace> mFreeStack;

    // The time at which the page is available for reuse, as measured against
    // mNow. When the page is in use this value will be kMaxTime.
    // - NeverAllocated: must be 0.
    // - InUse: must be kMaxTime.
    // - Freed: must be > 0 and < kMaxTime.
    Time mReuseTime;

    // The next index for a free list of pages.`
    Maybe<uintptr_t> mNextPage;
  };

 public:
  // The RNG seeds here are poor, but non-reentrant since this can be called
  // from malloc().  SetState() will reset the RNG later.
  PHC() : mRNG(RandomSeed<1>(), RandomSeed<2>()) {
    mMutex.Init();
    if (!tlsIsDisabled.init()) {
      MOZ_CRASH();
    }
    if (!tlsAllocDelay.init()) {
      MOZ_CRASH();
    }
    if (!tlsLastDelay.init()) {
      MOZ_CRASH();
    }

    // This constructor is part of PHC's very early initialisation,
    // see phc_init(), and if PHC is default-on it'll start marking allocations
    // and we must setup the delay.  However once XPCOM starts it'll call
    // SetState() which will re-initialise the RNG and allocation delay.
    MutexAutoLock lock(mMutex);

    ForceSetNewAllocDelay(Rnd64ToDelay(mAvgFirstAllocDelay, Random64(lock)));

    for (uintptr_t i = 0; i < kNumAllocPages; i++) {
      AppendPageToFreeList(lock, i);
    }
  }

  uint64_t Random64(PHCLock) { return mRNG.next(); }

  bool IsPageInUse(PHCLock, uintptr_t aIndex) {
    return mAllocPages[aIndex].mState == AllocPageState::InUse;
  }

  // Is the page free? And if so, has enough time passed that we can use it?
  bool IsPageAllocatable(PHCLock, uintptr_t aIndex, Time aNow) {
    const AllocPageInfo& page = mAllocPages[aIndex];
    return page.mState != AllocPageState::InUse && aNow >= page.mReuseTime;
  }

  // Get the address of the allocation page referred to via an index. Used
  // when checking pointers against page boundaries.
  uint8_t* AllocPageBaseAddr(PHCLock, uintptr_t aIndex) {
    return mAllocPages[aIndex].mBaseAddr;
  }

  Maybe<arena_id_t> PageArena(PHCLock aLock, uintptr_t aIndex) {
    const AllocPageInfo& page = mAllocPages[aIndex];
    AssertAllocPageInUse(aLock, page);

    return page.mArenaId;
  }

  size_t PageUsableSize(PHCLock aLock, uintptr_t aIndex) {
    const AllocPageInfo& page = mAllocPages[aIndex];
    AssertAllocPageInUse(aLock, page);

    return page.UsableSize();
  }

  // The total fragmentation in PHC
  size_t FragmentationBytes() const {
    size_t sum = 0;
    for (const auto& page : mAllocPages) {
      sum += page.FragmentationBytes();
    }
    return sum;
  }

  void SetPageInUse(PHCLock aLock, uintptr_t aIndex,
                    const Maybe<arena_id_t>& aArenaId, uint8_t* aBaseAddr,
                    const StackTrace& aAllocStack) {
    AllocPageInfo& page = mAllocPages[aIndex];
    AssertAllocPageNotInUse(aLock, page);

    page.mState = AllocPageState::InUse;
    page.mArenaId = aArenaId;
    page.mBaseAddr = aBaseAddr;
    page.mAllocStack = Some(aAllocStack);
    page.mFreeStack = Nothing();
    page.mReuseTime = kMaxTime;
    MOZ_ASSERT(!page.mNextPage);
  }

#if PHC_LOGGING
  Time GetFreeTime(uintptr_t aIndex) const { return mFreeTime[aIndex]; }
#endif

  void ResizePageInUse(PHCLock aLock, uintptr_t aIndex,
                       const Maybe<arena_id_t>& aArenaId, uint8_t* aNewBaseAddr,
                       const StackTrace& aAllocStack) {
    AllocPageInfo& page = mAllocPages[aIndex];
    AssertAllocPageInUse(aLock, page);

    // page.mState is not changed.
    if (aArenaId.isSome()) {
      // Crash if the arenas don't match.
      MOZ_RELEASE_ASSERT(page.mArenaId == aArenaId);
    }
    page.mBaseAddr = aNewBaseAddr;
    // We could just keep the original alloc stack, but the realloc stack is
    // more recent and therefore seems more useful.
    page.mAllocStack = Some(aAllocStack);
    // page.mFreeStack is not changed.
    // page.mReuseTime is not changed.
    // page.mNextPage is not changed.
  };

  void SetPageFreed(PHCLock aLock, uintptr_t aIndex,
                    const Maybe<arena_id_t>& aArenaId,
                    const StackTrace& aFreeStack, Delay aReuseDelay) {
    AllocPageInfo& page = mAllocPages[aIndex];
    AssertAllocPageInUse(aLock, page);

    page.mState = AllocPageState::Freed;

    // page.mArenaId is left unchanged, for jemalloc_ptr_info() calls that
    // occur after freeing (e.g. in the PtrInfo test in TestJemalloc.cpp).
    if (aArenaId.isSome()) {
      // Crash if the arenas don't match.
      MOZ_RELEASE_ASSERT(page.mArenaId == aArenaId);
    }

    // page.musableSize is left unchanged, for reporting on UAF, and for
    // jemalloc_ptr_info() calls that occur after freeing (e.g. in the PtrInfo
    // test in TestJemalloc.cpp).

    // page.mAllocStack is left unchanged, for reporting on UAF.

    page.mFreeStack = Some(aFreeStack);
    Time now = Now();
#if PHC_LOGGING
    mFreeTime[aIndex] = now;
#endif
    page.mReuseTime = now + aReuseDelay;

    MOZ_ASSERT(!page.mNextPage);
    AppendPageToFreeList(aLock, aIndex);
  }

  static void CrashOnGuardPage(void* aPtr) {
    // An operation on a guard page? This is a bounds violation. Deliberately
    // touch the page in question to cause a crash that triggers the usual PHC
    // machinery.
    LOG("CrashOnGuardPage(%p), bounds violation\n", aPtr);
    *static_cast<uint8_t*>(aPtr) = 0;
    MOZ_CRASH("unreachable");
  }

  void EnsureValidAndInUse(PHCLock, void* aPtr, uintptr_t aIndex)
      MOZ_REQUIRES(mMutex) {
    const AllocPageInfo& page = mAllocPages[aIndex];

    // The pointer must point to the start of the allocation.
    MOZ_RELEASE_ASSERT(page.mBaseAddr == aPtr);

    if (page.mState == AllocPageState::Freed) {
      LOG("EnsureValidAndInUse(%p), use-after-free\n", aPtr);
      // An operation on a freed page? This is a particular kind of
      // use-after-free. Deliberately touch the page in question, in order to
      // cause a crash that triggers the usual PHC machinery. But unlock mMutex
      // first, because that self-same PHC machinery needs to re-lock it, and
      // the crash causes non-local control flow so mMutex won't be unlocked
      // the normal way in the caller.
      mMutex.Unlock();
      *static_cast<uint8_t*>(aPtr) = 0;
      MOZ_CRASH("unreachable");
    }
  }

  // This expects GMUt::mMutex to be locked but can't check it with a parameter
  // since we try-lock it.
  void FillAddrInfo(uintptr_t aIndex, const void* aBaseAddr, bool isGuardPage,
                    phc::AddrInfo& aOut) {
    const AllocPageInfo& page = mAllocPages[aIndex];
    if (isGuardPage) {
      aOut.mKind = phc::AddrInfo::Kind::GuardPage;
    } else {
      switch (page.mState) {
        case AllocPageState::NeverAllocated:
          aOut.mKind = phc::AddrInfo::Kind::NeverAllocatedPage;
          break;

        case AllocPageState::InUse:
          aOut.mKind = phc::AddrInfo::Kind::InUsePage;
          break;

        case AllocPageState::Freed:
          aOut.mKind = phc::AddrInfo::Kind::FreedPage;
          break;

        default:
          MOZ_CRASH();
      }
    }
    aOut.mBaseAddr = page.mBaseAddr;
    aOut.mUsableSize = page.UsableSize();
    aOut.mAllocStack = page.mAllocStack;
    aOut.mFreeStack = page.mFreeStack;
  }

  void FillJemallocPtrInfo(PHCLock, const void* aPtr, uintptr_t aIndex,
                           jemalloc_ptr_info_t* aInfo) {
    const AllocPageInfo& page = mAllocPages[aIndex];
    switch (page.mState) {
      case AllocPageState::NeverAllocated:
        break;

      case AllocPageState::InUse: {
        // Only return TagLiveAlloc if the pointer is within the bounds of the
        // allocation's usable size.
        uint8_t* base = page.mBaseAddr;
        uint8_t* limit = base + page.UsableSize();
        if (base <= aPtr && aPtr < limit) {
          *aInfo = {TagLiveAlloc, page.mBaseAddr, page.UsableSize(),
                    page.mArenaId.valueOr(0)};
          return;
        }
        break;
      }

      case AllocPageState::Freed: {
        // Only return TagFreedAlloc if the pointer is within the bounds of the
        // former allocation's usable size.
        uint8_t* base = page.mBaseAddr;
        uint8_t* limit = base + page.UsableSize();
        if (base <= aPtr && aPtr < limit) {
          *aInfo = {TagFreedAlloc, page.mBaseAddr, page.UsableSize(),
                    page.mArenaId.valueOr(0)};
          return;
        }
        break;
      }

      default:
        MOZ_CRASH();
    }

    // Pointers into guard pages will end up here, as will pointers into
    // allocation pages that aren't within the allocation's bounds.
    *aInfo = {TagUnknown, nullptr, 0, 0};
  }

#ifndef XP_WIN
  static void prefork() MOZ_NO_THREAD_SAFETY_ANALYSIS {
    PHC::sPHC->mMutex.Lock();
  }
  static void postfork_parent() MOZ_NO_THREAD_SAFETY_ANALYSIS {
    PHC::sPHC->mMutex.Unlock();
  }
  static void postfork_child() { PHC::sPHC->mMutex.Init(); }
#endif

#if PHC_LOGGING
  void IncPageAllocHits(PHCLock) { mPageAllocHits++; }
  void IncPageAllocMisses(PHCLock) { mPageAllocMisses++; }
#else
  void IncPageAllocHits(PHCLock) {}
  void IncPageAllocMisses(PHCLock) {}
#endif

  phc::PHCStats GetPageStats(PHCLock) {
    phc::PHCStats stats;

    for (const auto& page : mAllocPages) {
      stats.mSlotsAllocated += page.mState == AllocPageState::InUse ? 1 : 0;
      stats.mSlotsFreed += page.mState == AllocPageState::Freed ? 1 : 0;
    }
    stats.mSlotsUnused =
        kNumAllocPages - stats.mSlotsAllocated - stats.mSlotsFreed;

    return stats;
  }

#if PHC_LOGGING
  size_t PageAllocHits(PHCLock) { return mPageAllocHits; }
  size_t PageAllocAttempts(PHCLock) {
    return mPageAllocHits + mPageAllocMisses;
  }

  // This is an integer because FdPrintf only supports integer printing.
  size_t PageAllocHitRate(PHCLock) {
    return mPageAllocHits * 100 / (mPageAllocHits + mPageAllocMisses);
  }
#endif

  // Should we make new PHC allocations?
  bool ShouldMakeNewAllocations() const {
    return mPhcState == mozilla::phc::Enabled;
  }

  using PHCState = mozilla::phc::PHCState;
  void SetState(PHCState aState) {
    if (mPhcState != PHCState::Enabled && aState == PHCState::Enabled) {
      MutexAutoLock lock(mMutex);
      // Reset the RNG at this point with a better seed.
      ResetRNG(lock);

      ForceSetNewAllocDelay(Rnd64ToDelay(mAvgFirstAllocDelay, Random64(lock)));
    }

    mPhcState = aState;
  }

  void ResetRNG(MutexAutoLock&) {
    mRNG = non_crypto::XorShift128PlusRNG(RandomSeed<0>(), RandomSeed<1>());
  }

  void SetProbabilities(int64_t aAvgDelayFirst, int64_t aAvgDelayNormal,
                        int64_t aAvgDelayPageReuse) {
    MutexAutoLock lock(mMutex);

    mAvgFirstAllocDelay = CheckProbability(aAvgDelayFirst);
    mAvgAllocDelay = CheckProbability(aAvgDelayNormal);
    mAvgPageReuseDelay = CheckProbability(aAvgDelayPageReuse);
  }

  static void DisableOnCurrentThread() {
    MOZ_ASSERT(!tlsIsDisabled.get());
    tlsIsDisabled.set(true);
  }

  void EnableOnCurrentThread() {
    MOZ_ASSERT(tlsIsDisabled.get());
    tlsIsDisabled.set(false);
  }

  static bool IsDisabledOnCurrentThread() { return tlsIsDisabled.get(); }

  static Time Now() {
    if (!sPHC) {
      return 0;
    }

    return sPHC->mNow;
  }

  void AdvanceNow(uint32_t delay = 0) {
    mNow += tlsLastDelay.get() - delay;
    tlsLastDelay.set(delay);
  }

  // Decrements the delay and returns true if it's time to make a new PHC
  // allocation.
  static bool DecrementDelay() {
    const Delay alloc_delay = tlsAllocDelay.get();

    if (MOZ_LIKELY(alloc_delay > 0)) {
      tlsAllocDelay.set(alloc_delay - 1);
      return false;
    }
    // The local delay has expired, check the shared delay.  This path is also
    // executed on a new thread's first allocation, the result is the same: all
    // the thread's TLS fields will be initialised.

    // This accesses sPHC but we want to ensure it's still a static member
    // function so that sPHC isn't dereferenced until after the hot path above.
    MOZ_ASSERT(sPHC);
    sPHC->AdvanceNow();

    // Use an atomic fetch-and-subtract.  This uses unsigned underflow semantics
    // to avoid doing a full compare-and-swap.
    Delay new_delay = (sAllocDelay -= kDelayDecrementAmount);
    Delay old_delay = new_delay + kDelayDecrementAmount;
    if (MOZ_LIKELY(new_delay < DELAY_MAX)) {
      // Normal case, we decremented the shared delay but it's not yet
      // underflowed.
      tlsAllocDelay.set(kDelayDecrementAmount);
      tlsLastDelay.set(kDelayDecrementAmount);
      LOG("Update sAllocDelay <- %zu, tlsAllocDelay <- %zu\n",
          size_t(new_delay), size_t(kDelayDecrementAmount));
      return false;
    }

    if (old_delay < new_delay) {
      // The shared delay only just underflowed, so unless we hit exactly zero
      // we should set our local counter and continue.
      LOG("Update sAllocDelay <- %zu, tlsAllocDelay <- %zu\n",
          size_t(new_delay), size_t(old_delay));
      if (old_delay == 0) {
        // We don't need to set tlsAllocDelay because it's already zero, we know
        // because the condition at the beginning of this function failed.
        return true;
      }
      tlsAllocDelay.set(old_delay);
      tlsLastDelay.set(old_delay);
      return false;
    }

    // The delay underflowed on another thread or a previous failed allocation
    // by this thread.  Return true and attempt the next allocation, if the
    // other thread wins we'll check for that before committing.
    LOG("Update sAllocDelay <- %zu, tlsAllocDelay <- %zu\n", size_t(new_delay),
        size_t(alloc_delay));
    return true;
  }

  static void ResetLocalAllocDelay(Delay aDelay = 0) {
    // We could take some delay from the shared delay but we'd need a
    // compare-and-swap because this is called on paths that don't make
    // allocations.  Or we can set the local delay to zero and let it get
    // initialised on the next allocation.
    tlsAllocDelay.set(aDelay);
    tlsLastDelay.set(aDelay);
  }

  static void ForceSetNewAllocDelay(Delay aNewAllocDelay) {
    LOG("Setting sAllocDelay <- %zu\n", size_t(aNewAllocDelay));
    sAllocDelay = aNewAllocDelay;
    ResetLocalAllocDelay();
  }

  // Set a new allocation delay and return true if the delay was less than zero
  // (but it's unsigned so interpret it as signed) indicating that we won the
  // race to make the next allocation.
  static bool SetNewAllocDelay(Delay aNewAllocDelay) {
    bool cas_retry;
    do {
      // We read the current delay on every iteration, we consider that the PHC
      // allocation is still "up for grabs" if sAllocDelay < 0.  This is safe
      // even while other threads continuing to fetch-and-subtract sAllocDelay
      // in DecrementDelay(), up to DELAY_MAX (2^31) calls to DecrementDelay().
      Delay read_delay = sAllocDelay;
      if (read_delay < DELAY_MAX) {
        // Another thread already set a valid delay.
        LOG("Observe delay %zu this thread lost the race\n",
            size_t(read_delay));
        ResetLocalAllocDelay();
        return false;
      } else {
        LOG("Preparing for CAS, read sAllocDelay %zu\n", size_t(read_delay));
      }

      cas_retry = !sAllocDelay.compareExchange(read_delay, aNewAllocDelay);
      if (cas_retry) {
        LOG("Lost the CAS, sAllocDelay is now %zu\n", size_t(sAllocDelay));
        cpu_pause();
        //  We raced against another thread and lost.
      }
    } while (cas_retry);
    LOG("Won the CAS, set sAllocDelay = %zu\n", size_t(sAllocDelay));
    ResetLocalAllocDelay();
    return true;
  }

  static Delay LocalAllocDelay() { return tlsAllocDelay.get(); }
  static Delay SharedAllocDelay() { return sAllocDelay; }

  static Delay LastDelay() { return tlsLastDelay.get(); }

  Maybe<uintptr_t> PopNextFreeIfAllocatable(const MutexAutoLock& lock,
                                            Time now) {
    if (!mFreePageListHead) {
      return Nothing();
    }

    uintptr_t index = mFreePageListHead.value();

    MOZ_RELEASE_ASSERT(index < kNumAllocPages);
    AllocPageInfo& page = mAllocPages[index];
    AssertAllocPageNotInUse(lock, page);

    if (!IsPageAllocatable(lock, index, now)) {
      return Nothing();
    }

    mFreePageListHead = page.mNextPage;
    page.mNextPage = Nothing();
    if (!mFreePageListHead) {
      mFreePageListTail = Nothing();
    }

    return Some(index);
  }

  void UnpopNextFree(const MutexAutoLock& lock, uintptr_t index) {
    AllocPageInfo& page = mAllocPages[index];
    MOZ_ASSERT(!page.mNextPage);

    page.mNextPage = mFreePageListHead;
    mFreePageListHead = Some(index);
    if (!mFreePageListTail) {
      mFreePageListTail = Some(index);
    }
  }

  void AppendPageToFreeList(const MutexAutoLock& lock, uintptr_t aIndex) {
    MOZ_RELEASE_ASSERT(aIndex < kNumAllocPages);
    AllocPageInfo& page = mAllocPages[aIndex];
    MOZ_ASSERT(!page.mNextPage);
    MOZ_ASSERT(mFreePageListHead != Some(aIndex) &&
               mFreePageListTail != Some(aIndex));

    if (!mFreePageListTail) {
      // The list is empty this page will become the beginning and end.
      MOZ_ASSERT(!mFreePageListHead);
      mFreePageListHead = Some(aIndex);
    } else {
      MOZ_ASSERT(mFreePageListTail.value() < kNumAllocPages);
      AllocPageInfo& tail_page = mAllocPages[mFreePageListTail.value()];
      MOZ_ASSERT(!tail_page.mNextPage);
      tail_page.mNextPage = Some(aIndex);
    }
    page.mNextPage = Nothing();
    mFreePageListTail = Some(aIndex);
  }

 private:
  template <int N>
  uint64_t RandomSeed() {
    // An older version of this code used RandomUint64() here, but on Mac that
    // function uses arc4random(), which can allocate, which would cause
    // re-entry, which would be bad. So we just use time(), a local variable
    // address and a global variable address. These are mediocre sources of
    // entropy, but good enough for PHC.
    static_assert(N == 0 || N == 1 || N == 2, "must be 0, 1 or 2");
    uint64_t seed;
    if (N == 0) {
      time_t t = time(nullptr);
      seed = t ^ (t << 32);
    } else if (N == 1) {
      seed = uintptr_t(&seed) ^ (uintptr_t(&seed) << 32);
    } else {
      seed = uintptr_t(&sRegion) ^ (uintptr_t(&sRegion) << 32);
    }
    return seed;
  }

  void AssertAllocPageInUse(PHCLock, const AllocPageInfo& aPage) {
    MOZ_ASSERT(aPage.mState == AllocPageState::InUse);
    // There is nothing to assert about aPage.mArenaId.
    MOZ_ASSERT(aPage.mBaseAddr);
    MOZ_ASSERT(aPage.UsableSize() > 0);
    MOZ_ASSERT(aPage.mAllocStack.isSome());
    MOZ_ASSERT(aPage.mFreeStack.isNothing());
    MOZ_ASSERT(aPage.mReuseTime == kMaxTime);
    MOZ_ASSERT(!aPage.mNextPage);
  }

  void AssertAllocPageNotInUse(PHCLock, const AllocPageInfo& aPage) {
    // We can assert a lot about `NeverAllocated` pages, but not much about
    // `Freed` pages.
#ifdef DEBUG
    bool isFresh = aPage.mState == AllocPageState::NeverAllocated;
    MOZ_ASSERT(isFresh || aPage.mState == AllocPageState::Freed);
    MOZ_ASSERT_IF(isFresh, aPage.mArenaId == Nothing());
    MOZ_ASSERT(isFresh == (aPage.mBaseAddr == nullptr));
    MOZ_ASSERT(isFresh == (aPage.mAllocStack.isNothing()));
    MOZ_ASSERT(isFresh == (aPage.mFreeStack.isNothing()));
    MOZ_ASSERT(aPage.mReuseTime != kMaxTime);
#endif
  }

  // To improve locality we try to order this file by how frequently different
  // fields are modified and place all the modified-together fields early and
  // ideally within a single cache line.
 public:
  // The mutex that protects the other members.
  alignas(kCacheLineSize) Mutex mMutex MOZ_UNANNOTATED;

 private:
  // The current time. We use ReleaseAcquire semantics since we attempt to
  // update this by larger increments and don't want to lose an entire update.
  Atomic<Time, ReleaseAcquire> mNow;

  // This will only ever be updated from one thread.  The other threads should
  // eventually get the update.
  Atomic<PHCState, Relaxed> mPhcState =
      Atomic<PHCState, Relaxed>(DEFAULT_STATE);

  // RNG for deciding which allocations to treat specially. It doesn't need to
  // be high quality.
  //
  // This is a raw pointer for the reason explained in the comment above
  // PHC's constructor. Don't change it to UniquePtr or anything like that.
  non_crypto::XorShift128PlusRNG mRNG;

  // A linked list of free pages. Pages are allocated from the head of the list
  // and returned to the tail. The list will naturally order itself by "last
  // freed time" so if the head of the list can't satisfy an allocation due to
  // time then none of the pages can.
  Maybe<uintptr_t> mFreePageListHead;
  Maybe<uintptr_t> mFreePageListTail;

#if PHC_LOGGING
  // How many allocations that could have been page allocs actually were? As
  // constrained kNumAllocPages. If the hit ratio isn't close to 100% it's
  // likely that the global constants are poorly chosen.
  size_t mPageAllocHits = 0;
  size_t mPageAllocMisses = 0;
#endif

  // The remaining fields are updated much less often, place them on the next
  // cache line.

  // The average delay before doing any page allocations at the start of a
  // process. Note that roughly 1 million allocations occur in the main process
  // while starting the browser. The delay range is 1..gAvgFirstAllocDelay*2.
  alignas(kCacheLineSize) Delay mAvgFirstAllocDelay = 64 * 1024;

  // The average delay until the next attempted page allocation, once we get
  // past the first delay. The delay range is 1..kAvgAllocDelay*2.
  Delay mAvgAllocDelay = 16 * 1024;

  // The average delay before reusing a freed page. Should be significantly
  // larger than kAvgAllocDelay, otherwise there's not much point in having it.
  // The delay range is (kAvgAllocDelay / 2)..(kAvgAllocDelay / 2 * 3). This is
  // different to the other delay ranges in not having a minimum of 1, because
  // that's such a short delay that there is a high likelihood of bad stacks in
  // any crash report.
  Delay mAvgPageReuseDelay = 256 * 1024;

  // When true, PHC does as little as possible.
  //
  // (a) It does not allocate any new page allocations.
  //
  // (b) It avoids doing any operations that might call malloc/free/etc., which
  //     would cause re-entry into PHC. (In practice, MozStackWalk() is the
  //     only such operation.) Note that calls to the functions in MozJemalloc
  //     are ok.
  //
  // For example, replace_malloc() will just fall back to mozjemalloc. However,
  // operations involving existing allocations are more complex, because those
  // existing allocations may be page allocations. For example, if
  // replace_free() is passed a page allocation on a PHC-disabled thread, it
  // will free the page allocation in the usual way, but it will get a dummy
  // freeStack in order to avoid calling MozStackWalk(), as per (b) above.
  //
  // This single disabling mechanism has two distinct uses.
  //
  // - It's used to prevent re-entry into PHC, which can cause correctness
  //   problems. For example, consider this sequence.
  //
  //   1. enter replace_free()
  //   2. which calls PageFree()
  //   3. which calls MozStackWalk()
  //   4. which locks a mutex M, and then calls malloc
  //   5. enter replace_malloc()
  //   6. which calls MaybePageAlloc()
  //   7. which calls MozStackWalk()
  //   8. which (re)locks a mutex M --> deadlock
  //
  //   We avoid this sequence by "disabling" the thread in PageFree() (at step
  //   2), which causes MaybePageAlloc() to fail, avoiding the call to
  //   MozStackWalk() (at step 7).
  //
  //   In practice, realloc or free of a PHC allocation is unlikely on a thread
  //   that is disabled because of this use: MozStackWalk() will probably only
  //   realloc/free allocations that it allocated itself, but those won't be
  //   page allocations because PHC is disabled before calling MozStackWalk().
  //
  //   (Note that MaybePageAlloc() could safely do a page allocation so long as
  //   it avoided calling MozStackWalk() by getting a dummy allocStack. But it
  //   wouldn't be useful, and it would prevent the second use below.)
  //
  // - It's used to prevent PHC allocations in some tests that rely on
  //   mozjemalloc's exact allocation behaviour, which PHC does not replicate
  //   exactly. (Note that (b) isn't necessary for this use -- MozStackWalk()
  //   could be safely called -- but it is necessary for the first use above.)
  //
  static PHC_THREAD_LOCAL(bool) tlsIsDisabled;

  // Delay until the next attempt at a page allocation.  The delay is made up of
  // two parts the global delay and each thread's local portion of that delay:
  //
  //  delay = sDelay + sum_all_threads(tlsAllocDelay)
  //
  // Threads use their local delay to reduce contention on the shared delay.
  //
  // See the comment in MaybePageAlloc() for an explanation of why it uses
  // ReleaseAcquire semantics.
  static Atomic<Delay, ReleaseAcquire> sAllocDelay;
  static PHC_THREAD_LOCAL(Delay) tlsAllocDelay;

  // The last value we set tlsAllocDelay to before starting to count down.
  static PHC_THREAD_LOCAL(Delay) tlsLastDelay;

  AllocPageInfo mAllocPages[kNumAllocPages];
#if PHC_LOGGING
  Time mFreeTime[kNumAllocPages];
#endif

 public:
  Delay GetAvgAllocDelay(const MutexAutoLock&) { return mAvgAllocDelay; }
  Delay GetAvgFirstAllocDelay(const MutexAutoLock&) {
    return mAvgFirstAllocDelay;
  }
  Delay GetAvgPageReuseDelay(const MutexAutoLock&) {
    return mAvgPageReuseDelay;
  }

  // Both of these are accessed early on hot code paths.  We make them both
  // static variables rathan making sRegion a member of sPHC to keep these hot
  // code paths as fast as possible.  They're both "write once" so they can
  // share a cache line.
  static PHCRegion* sRegion;
  static PHC* sPHC;
};

// These globals are read together and hardly ever written.  They should be on
// the same cache line.  They should be in a different cache line to data that
// is manipulated often (sMutex and mNow are members of sPHC for that reason) so
// that this cache line can be shared amoung cores.  This makes a measurable
// impact to calls to maybe_init()
alignas(kCacheLineSize) PHCRegion* PHC::sRegion;
PHC* PHC::sPHC;

PHC_THREAD_LOCAL(bool) PHC::tlsIsDisabled;
PHC_THREAD_LOCAL(Delay) PHC::tlsAllocDelay;
Atomic<Delay, ReleaseAcquire> PHC::sAllocDelay;
PHC_THREAD_LOCAL(Delay) PHC::tlsLastDelay;

// This must be defined after the PHC class.
PHCRegion::PHCRegion()
    : mPagesStart(AllocAllPages()), mPagesLimit(mPagesStart + kAllPagesSize) {
  LOG("AllocAllPages at %p..%p\n", mPagesStart, mPagesLimit);
}

// When PHC wants to crash we first have to unlock so that the crash reporter
// can call into PHC to lockup its pointer. That also means that before calling
// PHCCrash please ensure that state is consistent.  Because this can report an
// arbitrary string, use of it must be reviewed by Firefox data stewards.
static void PHCCrash(PHCLock, const char* aMessage)
    MOZ_REQUIRES(PHC::sPHC->mMutex) {
  PHC::sPHC->mMutex.Unlock();
  MOZ_CRASH_UNSAFE(aMessage);
}

class AutoDisableOnCurrentThread {
 public:
  AutoDisableOnCurrentThread(const AutoDisableOnCurrentThread&) = delete;

  const AutoDisableOnCurrentThread& operator=(
      const AutoDisableOnCurrentThread&) = delete;

  explicit AutoDisableOnCurrentThread() { PHC::DisableOnCurrentThread(); }
  ~AutoDisableOnCurrentThread() { PHC::sPHC->EnableOnCurrentThread(); }
};

//---------------------------------------------------------------------------
// Initialisation
//---------------------------------------------------------------------------

// WARNING: this function runs *very* early -- before all static initializers
// have run. For this reason, non-scalar globals (sRegion, sPHC) are allocated
// dynamically (so we can guarantee their construction in this function) rather
// than statically.
static bool phc_init() {
  if (GetKernelPageSize() != kPageSize) {
    return false;
  }

  // sRegion and sPHC are never freed. They live for the life of the process.
  PHC::sRegion = InfallibleAllocPolicy::new_<PHCRegion>();

  PHC::sPHC = InfallibleAllocPolicy::new_<PHC>();

#ifndef XP_WIN
  // Avoid deadlocks when forking by acquiring our state lock prior to forking
  // and releasing it after forking. See |LogAlloc|'s |phc_init| for
  // in-depth details.
  pthread_atfork(PHC::prefork, PHC::postfork_parent, PHC::postfork_child);
#endif

  return true;
}

static inline bool maybe_init() {
  // This runs on hot paths and we can save some memory accesses by using sPHC
  // to test if we've already initialised PHC successfully.
  if (MOZ_UNLIKELY(!PHC::sPHC)) {
    // The lambda will only be called once and is thread safe.
    static bool sInitSuccess = []() { return phc_init(); }();
    return sInitSuccess;
  }

  return true;
}

//---------------------------------------------------------------------------
// Page allocation operations
//---------------------------------------------------------------------------

// This is the hot-path for testing if we should make a PHC allocation, it
// should be inlined into the caller while the remainder of the tests that are
// in MaybePageAlloc need not be inlined.
static MOZ_ALWAYS_INLINE bool ShouldPageAllocHot(size_t aReqSize) {
  if (MOZ_UNLIKELY(!maybe_init())) {
    return false;
  }

  if (MOZ_UNLIKELY(aReqSize > kPageSize)) {
    return false;
  }

  // Decrement the delay. If it's zero, we do a page allocation and reset the
  // delay to a random number.
  if (MOZ_LIKELY(!PHC::DecrementDelay())) {
    return false;
  }

  return true;
}

static void LogNoAlloc(size_t aReqSize, size_t aAlignment,
                       Delay newAllocDelay) {
  // No pages are available, or VirtualAlloc/mprotect failed.
#if PHC_LOGGING
  phc::PHCStats stats = PHC::sPHC->GetPageStats(lock);
#endif
  LOG("No PageAlloc(%zu, %zu), sAllocDelay <- %zu, fullness %zu/%zu/%zu, "
      "hits %zu/%zu (%zu%%)\n",
      aReqSize, aAlignment, size_t(newAllocDelay), stats.mSlotsAllocated,
      stats.mSlotsFreed, kNumAllocPages, PHC::sPHC->PageAllocHits(lock),
      PHC::sPHC->PageAllocAttempts(lock), PHC::sPHC->PageAllocHitRate(lock));
}

// Attempt a page allocation if the time and the size are right. Allocated
// memory is zeroed if aZero is true. On failure, the caller should attempt a
// normal allocation via MozJemalloc. Can be called in a context where
// PHC::mMutex is locked.
static void* MaybePageAlloc(const Maybe<arena_id_t>& aArenaId, size_t aReqSize,
                            size_t aAlignment, bool aZero) {
  MOZ_ASSERT(IsPowerOfTwo(aAlignment));
  MOZ_ASSERT(PHC::sPHC);
  if (!PHC::sPHC->ShouldMakeNewAllocations()) {
    // Reset the allocation delay so that we take the fast path most of the
    // time.  Rather than take the lock and use the RNG which are unnecessary
    // when PHC is disabled, instead set the delay to a reasonably high number,
    // the default average first allocation delay.  This is reset when PHC is
    // re-enabled anyway.
    PHC::ForceSetNewAllocDelay(kDelayResetWhenDisabled);
    return nullptr;
  }

  if (PHC::IsDisabledOnCurrentThread()) {
    // We don't reset sAllocDelay since that might affect other threads.  We
    // assume this is okay because either this thread will be re-enabled after
    // less than DELAY_MAX allocations or that there are other active threads
    // that will reset sAllocDelay.  We do reset our local delay which will
    // cause this thread to "back off" from updating sAllocDelay on future
    // allocations.
    PHC::ResetLocalAllocDelay(kDelayBackoffAmount);
    return nullptr;
  }

  // Disable on this thread *before* getting the stack trace.
  AutoDisableOnCurrentThread disable;

  // Get the stack trace *before* locking the mutex. If we return nullptr then
  // it was a waste, but it's not so frequent, and doing a stack walk while
  // the mutex is locked is problematic (see the big comment on
  // StackTrace::Fill() for details).
  StackTrace allocStack;
  allocStack.Fill();

  MutexAutoLock lock(PHC::sPHC->mMutex);

  Time now = PHC::Now();

  Delay newAllocDelay = Rnd64ToDelay(PHC::sPHC->GetAvgAllocDelay(lock),
                                     PHC::sPHC->Random64(lock));
  if (!PHC::sPHC->SetNewAllocDelay(newAllocDelay)) {
    return nullptr;
  }

  // Pages are allocated from a free list populated in order of when they're
  // freed.  If the page at the head of the list is too recently freed to be
  // reused then no other pages on the list will be either.

  Maybe<uintptr_t> mb_index = PHC::sPHC->PopNextFreeIfAllocatable(lock, now);
  if (!mb_index) {
    PHC::sPHC->IncPageAllocMisses(lock);
    LogNoAlloc(aReqSize, aAlignment, newAllocDelay);
    return nullptr;
  }
  uintptr_t index = mb_index.value();

#if PHC_LOGGING
  Time lifetime = 0;
#endif
  uint8_t* pagePtr = PHC::sRegion->AllocPagePtr(index);
  MOZ_ASSERT(pagePtr);
  bool ok =
#ifdef XP_WIN
      !!VirtualAlloc(pagePtr, kPageSize, MEM_COMMIT, PAGE_READWRITE);
#else
      mprotect(pagePtr, kPageSize, PROT_READ | PROT_WRITE) == 0;
#endif

  if (!ok) {
    PHC::sPHC->UnpopNextFree(lock, index);
    PHC::sPHC->IncPageAllocMisses(lock);
    LogNoAlloc(aReqSize, aAlignment, newAllocDelay);
    return nullptr;
  }

  size_t usableSize = MozJemalloc::malloc_good_size(aReqSize);
  MOZ_ASSERT(usableSize > 0);

  // Put the allocation as close to the end of the page as possible,
  // allowing for alignment requirements.
  uint8_t* ptr = pagePtr + kPageSize - usableSize;
  if (aAlignment != 1) {
    ptr = reinterpret_cast<uint8_t*>(
        (reinterpret_cast<uintptr_t>(ptr) & ~(aAlignment - 1)));
  }

#if PHC_LOGGING
  Time then = PHC::sPHC->GetFreeTime(i);
  lifetime = then != 0 ? now - then : 0;
#endif

  PHC::sPHC->SetPageInUse(lock, index, aArenaId, ptr, allocStack);

  if (aZero) {
    memset(ptr, 0, usableSize);
  } else {
#ifdef DEBUG
    memset(ptr, kAllocJunk, usableSize);
#endif
  }

  PHC::sPHC->IncPageAllocHits(lock);
#if PHC_LOGGING
  phc::PHCStats stats = PHC::sPHC->GetPageStats(lock);
#endif
  LOG("PageAlloc(%zu, %zu) -> %p[%zu]/%p (%zu) (z%zu), sAllocDelay <- %zu, "
      "fullness %zu/%zu/%zu, hits %zu/%zu (%zu%%), lifetime %zu\n",
      aReqSize, aAlignment, pagePtr, i, ptr, usableSize, size_t(newAllocDelay),
      size_t(PHC::SharedAllocDelay()), stats.mSlotsAllocated, stats.mSlotsFreed,
      kNumAllocPages, PHC::sPHC->PageAllocHits(lock),
      PHC::sPHC->PageAllocAttempts(lock), PHC::sPHC->PageAllocHitRate(lock),
      lifetime);

  return ptr;
}

static void FreePage(PHCLock aLock, uintptr_t aIndex,
                     const Maybe<arena_id_t>& aArenaId,
                     const StackTrace& aFreeStack, Delay aReuseDelay)
    MOZ_REQUIRES(PHC::sPHC->mMutex) {
  void* pagePtr = PHC::sRegion->AllocPagePtr(aIndex);

#ifdef XP_WIN
  if (!VirtualFree(pagePtr, kPageSize, MEM_DECOMMIT)) {
    PHCCrash(aLock, "VirtualFree failed");
  }
#else
  if (mmap(pagePtr, kPageSize, PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANON,
           -1, 0) == MAP_FAILED) {
    PHCCrash(aLock, "mmap failed");
  }
#endif

  PHC::sPHC->SetPageFreed(aLock, aIndex, aArenaId, aFreeStack, aReuseDelay);
}

//---------------------------------------------------------------------------
// replace-malloc machinery
//---------------------------------------------------------------------------

// This handles malloc, moz_arena_malloc, and realloc-with-a-nullptr.
MOZ_ALWAYS_INLINE static void* PageMalloc(const Maybe<arena_id_t>& aArenaId,
                                          size_t aReqSize) {
  void* ptr = ShouldPageAllocHot(aReqSize)
                  // The test on aArenaId here helps the compiler optimise away
                  // the construction of Nothing() in the caller.
                  ? MaybePageAlloc(aArenaId.isSome() ? aArenaId : Nothing(),
                                   aReqSize, /* aAlignment */ 1,
                                   /* aZero */ false)
                  : nullptr;
  return ptr ? ptr
             : (aArenaId.isSome()
                    ? MozJemalloc::moz_arena_malloc(*aArenaId, aReqSize)
                    : MozJemalloc::malloc(aReqSize));
}

inline void* MozJemallocPHC::malloc(size_t aReqSize) {
  return PageMalloc(Nothing(), aReqSize);
}

static Delay ReuseDelay(PHCLock aLock) {
  Delay avg_reuse_delay = PHC::sPHC->GetAvgPageReuseDelay(aLock);
  return (avg_reuse_delay / 2) +
         Rnd64ToDelay(avg_reuse_delay / 2, PHC::sPHC->Random64(aLock));
}

// This handles both calloc and moz_arena_calloc.
MOZ_ALWAYS_INLINE static void* PageCalloc(const Maybe<arena_id_t>& aArenaId,
                                          size_t aNum, size_t aReqSize) {
  CheckedInt<size_t> checkedSize = CheckedInt<size_t>(aNum) * aReqSize;
  if (!checkedSize.isValid()) {
    return nullptr;
  }

  void* ptr = ShouldPageAllocHot(checkedSize.value())
                  // The test on aArenaId here helps the compiler optimise away
                  // the construction of Nothing() in the caller.
                  ? MaybePageAlloc(aArenaId.isSome() ? aArenaId : Nothing(),
                                   checkedSize.value(), /* aAlignment */ 1,
                                   /* aZero */ true)
                  : nullptr;
  return ptr ? ptr
             : (aArenaId.isSome()
                    ? MozJemalloc::moz_arena_calloc(*aArenaId, aNum, aReqSize)
                    : MozJemalloc::calloc(aNum, aReqSize));
}

inline void* MozJemallocPHC::calloc(size_t aNum, size_t aReqSize) {
  return PageCalloc(Nothing(), aNum, aReqSize);
}

// This function handles both realloc and moz_arena_realloc.
//
// As always, realloc is complicated, and doubly so when there are two
// different kinds of allocations in play. Here are the possible transitions,
// and what we do in practice.
//
// - normal-to-normal: This is straightforward and obviously necessary.
//
// - normal-to-page: This is disallowed because it would require getting the
//   arenaId of the normal allocation, which isn't possible in non-DEBUG builds
//   for security reasons.
//
// - page-to-page: This is done whenever possible, i.e. whenever the new size
//   is less than or equal to 4 KiB. This choice counterbalances the
//   disallowing of normal-to-page allocations, in order to avoid biasing
//   towards or away from page allocations. It always occurs in-place.
//
// - page-to-normal: this is done only when necessary, i.e. only when the new
//   size is greater than 4 KiB. This choice naturally flows from the
//   prior choice on page-to-page transitions.
//
// In summary: realloc doesn't change the allocation kind unless it must.
//
// This function may return:
// - Some(pointer) when PHC handled the reallocation.
// - Some(nullptr) when PHC should have handled a page-to-normal transition
//   but couldn't because of OOM.
// - Nothing() when PHC is disabled or the original allocation was not
//   under PHC.
MOZ_ALWAYS_INLINE static Maybe<void*> MaybePageRealloc(
    const Maybe<arena_id_t>& aArenaId, void* aOldPtr, size_t aNewSize) {
  if (!aOldPtr) {
    // Null pointer. Treat like malloc(aNewSize).
    return Some(PageMalloc(aArenaId, aNewSize));
  }

  if (!maybe_init()) {
    return Nothing();
  }

  PtrKind pk = PHC::sRegion->PtrKind(aOldPtr);
  if (pk.IsNothing()) {
    // A normal-to-normal transition.
    return Nothing();
  }

  if (pk.IsGuardPage()) {
    PHC::CrashOnGuardPage(aOldPtr);
  }

  // At this point we know we have an allocation page.
  uintptr_t index = pk.AllocPageIndex();

  // A page-to-something transition.
  PHC::sPHC->AdvanceNow(PHC::LocalAllocDelay());

  // Note that `disable` has no effect unless it is emplaced below.
  Maybe<AutoDisableOnCurrentThread> disable;
  // Get the stack trace *before* locking the mutex.
  StackTrace stack;
  if (PHC::IsDisabledOnCurrentThread()) {
    // PHC is disabled on this thread. Leave the stack empty.
  } else {
    // Disable on this thread *before* getting the stack trace.
    disable.emplace();
    stack.Fill();
  }

  MutexAutoLock lock(PHC::sPHC->mMutex);

  // Check for realloc() of a freed block.
  PHC::sPHC->EnsureValidAndInUse(lock, aOldPtr, index);

  if (aNewSize <= kPageSize && PHC::sPHC->ShouldMakeNewAllocations()) {
    // A page-to-page transition. Just keep using the page allocation. We do
    // this even if the thread is disabled, because it doesn't create a new
    // page allocation. Note that ResizePageInUse() checks aArenaId.
    //
    // Move the bytes with memmove(), because the old allocation and the new
    // allocation overlap. Move the usable size rather than the requested size,
    // because the user might have used malloc_usable_size() and filled up the
    // usable size.
    size_t oldUsableSize = PHC::sPHC->PageUsableSize(lock, index);
    size_t newUsableSize = MozJemalloc::malloc_good_size(aNewSize);
    uint8_t* pagePtr = PHC::sRegion->AllocPagePtr(index);
    uint8_t* newPtr = pagePtr + kPageSize - newUsableSize;
    memmove(newPtr, aOldPtr, std::min(oldUsableSize, aNewSize));
    PHC::sPHC->ResizePageInUse(lock, index, aArenaId, newPtr, stack);
    LOG("PageRealloc-Reuse(%p, %zu) -> %p\n", aOldPtr, aNewSize, newPtr);
    return Some(newPtr);
  }

  // A page-to-normal transition (with the new size greater than page-sized).
  // (Note that aArenaId is checked below.)
  void* newPtr;
  if (aArenaId.isSome()) {
    newPtr = MozJemalloc::moz_arena_malloc(*aArenaId, aNewSize);
  } else {
    Maybe<arena_id_t> oldArenaId = PHC::sPHC->PageArena(lock, index);
    newPtr = (oldArenaId.isSome()
                  ? MozJemalloc::moz_arena_malloc(*oldArenaId, aNewSize)
                  : MozJemalloc::malloc(aNewSize));
  }
  if (!newPtr) {
    return Some(nullptr);
  }

  Delay reuseDelay = ReuseDelay(lock);

  // Copy the usable size rather than the requested size, because the user
  // might have used malloc_usable_size() and filled up the usable size. Note
  // that FreePage() checks aArenaId (via SetPageFreed()).
  size_t oldUsableSize = PHC::sPHC->PageUsableSize(lock, index);
  memcpy(newPtr, aOldPtr, std::min(oldUsableSize, aNewSize));
  FreePage(lock, index, aArenaId, stack, reuseDelay);
  LOG("PageRealloc-Free(%p[%zu], %zu) -> %p, %zu delay, reuse at ~%zu\n",
      aOldPtr, index, aNewSize, newPtr, size_t(reuseDelay),
      size_t(PHC::Now()) + reuseDelay);

  return Some(newPtr);
}

MOZ_ALWAYS_INLINE static void* PageRealloc(const Maybe<arena_id_t>& aArenaId,
                                           void* aOldPtr, size_t aNewSize) {
  Maybe<void*> ptr = MaybePageRealloc(aArenaId, aOldPtr, aNewSize);

  return ptr.isSome()
             ? *ptr
             : (aArenaId.isSome() ? MozJemalloc::moz_arena_realloc(
                                        *aArenaId, aOldPtr, aNewSize)
                                  : MozJemalloc::realloc(aOldPtr, aNewSize));
}

inline void* MozJemallocPHC::realloc(void* aOldPtr, size_t aNewSize) {
  return PageRealloc(Nothing(), aOldPtr, aNewSize);
}

// This handles both free and moz_arena_free.
static void DoPageFree(const Maybe<arena_id_t>& aArenaId, void* aPtr) {
  PtrKind pk = PHC::sRegion->PtrKind(aPtr);
  if (pk.IsGuardPage()) {
    PHC::CrashOnGuardPage(aPtr);
  }

  // At this point we know we have an allocation page.
  PHC::sPHC->AdvanceNow(PHC::LocalAllocDelay());
  uintptr_t index = pk.AllocPageIndex();

  // Note that `disable` has no effect unless it is emplaced below.
  Maybe<AutoDisableOnCurrentThread> disable;
  // Get the stack trace *before* locking the mutex.
  StackTrace freeStack;
  if (PHC::IsDisabledOnCurrentThread()) {
    // PHC is disabled on this thread. Leave the stack empty.
  } else {
    // Disable on this thread *before* getting the stack trace.
    disable.emplace();
    freeStack.Fill();
  }

  MutexAutoLock lock(PHC::sPHC->mMutex);

  // Check for a double-free.
  PHC::sPHC->EnsureValidAndInUse(lock, aPtr, index);

  // Note that FreePage() checks aArenaId (via SetPageFreed()).
  Delay reuseDelay = ReuseDelay(lock);
  FreePage(lock, index, aArenaId, freeStack, reuseDelay);

#if PHC_LOGGING
  phc::PHCStats stats = PHC::sPHC->GetPageStats(lock);
#endif
  LOG("PageFree(%p[%zu]), %zu delay, reuse at ~%zu, fullness %zu/%zu/%zu\n",
      aPtr, index, size_t(reuseDelay), size_t(PHC::Now()) + reuseDelay,
      stats.mSlotsAllocated, stats.mSlotsFreed, kNumAllocPages);
}

MOZ_ALWAYS_INLINE static bool FastIsPHCPtr(void* aPtr) {
  if (MOZ_UNLIKELY(!maybe_init())) {
    return false;
  }

  PtrKind pk = PHC::sRegion->PtrKind(aPtr);
  return !pk.IsNothing();
}

MOZ_ALWAYS_INLINE static void PageFree(const Maybe<arena_id_t>& aArenaId,
                                       void* aPtr) {
  if (MOZ_UNLIKELY(FastIsPHCPtr(aPtr))) {
    // The tenery expression here helps the compiler optimise away the
    // construction of Nothing() in the caller.
    DoPageFree(aArenaId.isSome() ? aArenaId : Nothing(), aPtr);
    return;
  }

  aArenaId.isSome() ? MozJemalloc::moz_arena_free(*aArenaId, aPtr)
                    : MozJemalloc::free(aPtr);
}

inline void MozJemallocPHC::free(void* aPtr) { PageFree(Nothing(), aPtr); }

// This handles memalign and moz_arena_memalign.
MOZ_ALWAYS_INLINE static void* PageMemalign(const Maybe<arena_id_t>& aArenaId,
                                            size_t aAlignment,
                                            size_t aReqSize) {
  MOZ_RELEASE_ASSERT(IsPowerOfTwo(aAlignment));

  // PHC can't satisfy an alignment greater than a page size, so fall back to
  // mozjemalloc in that case.
  void* ptr = nullptr;
  if (ShouldPageAllocHot(aReqSize) && aAlignment <= kPageSize) {
    // The test on aArenaId here helps the compiler optimise away
    // the construction of Nothing() in the caller.
    ptr = MaybePageAlloc(aArenaId.isSome() ? aArenaId : Nothing(), aReqSize,
                         aAlignment, /* aZero */ false);
  }
  return ptr ? ptr
             : (aArenaId.isSome()
                    ? MozJemalloc::moz_arena_memalign(*aArenaId, aAlignment,
                                                      aReqSize)
                    : MozJemalloc::memalign(aAlignment, aReqSize));
}

inline void* MozJemallocPHC::memalign(size_t aAlignment, size_t aReqSize) {
  return PageMemalign(Nothing(), aAlignment, aReqSize);
}

inline size_t MozJemallocPHC::malloc_usable_size(usable_ptr_t aPtr) {
  if (!maybe_init()) {
    return MozJemalloc::malloc_usable_size(aPtr);
  }

  PtrKind pk = PHC::sRegion->PtrKind(aPtr);
  if (pk.IsNothing()) {
    // Not a page allocation. Measure it normally.
    return MozJemalloc::malloc_usable_size(aPtr);
  }

  if (pk.IsGuardPage()) {
    PHC::CrashOnGuardPage(const_cast<void*>(aPtr));
  }

  // At this point we know aPtr lands within an allocation page, due to the
  // math done in the PtrKind constructor. But if aPtr points to memory
  // before the base address of the allocation, we return 0.
  uintptr_t index = pk.AllocPageIndex();

  MutexAutoLock lock(PHC::sPHC->mMutex);

  void* pageBaseAddr = PHC::sPHC->AllocPageBaseAddr(lock, index);

  if (MOZ_UNLIKELY(aPtr < pageBaseAddr)) {
    return 0;
  }

  return PHC::sPHC->PageUsableSize(lock, index);
}

static size_t metadata_size() {
  return MozJemalloc::malloc_usable_size(PHC::sRegion) +
         MozJemalloc::malloc_usable_size(PHC::sPHC);
}

inline void MozJemallocPHC::jemalloc_stats_internal(
    jemalloc_stats_t* aStats, jemalloc_bin_stats_t* aBinStats) {
  MozJemalloc::jemalloc_stats_internal(aStats, aBinStats);

  if (!maybe_init()) {
    // If we're not initialised, then we're not using any additional memory and
    // have nothing to add to the report.
    return;
  }

  // We allocate our memory from jemalloc so it has already counted our memory
  // usage within "mapped" and "allocated", we must subtract the memory we
  // allocated from jemalloc from allocated before adding in only the parts that
  // we have allocated out to Firefox.

  aStats->allocated -= kAllPagesJemallocSize;

  size_t allocated = 0;
  {
    MutexAutoLock lock(PHC::sPHC->mMutex);

    // Add usable space of in-use allocations to `allocated`.
    for (size_t i = 0; i < kNumAllocPages; i++) {
      if (PHC::sPHC->IsPageInUse(lock, i)) {
        allocated += PHC::sPHC->PageUsableSize(lock, i);
      }
    }
  }
  aStats->allocated += allocated;

  // guards is the gap between `allocated` and `mapped`. In some ways this
  // almost fits into aStats->wasted since it feels like wasted memory. However
  // wasted should only include committed memory and these guard pages are
  // uncommitted. Therefore we don't include it anywhere.
  // size_t guards = mapped - allocated;

  // aStats.page_cache and aStats.bin_unused are left unchanged because PHC
  // doesn't have anything corresponding to those.

  // The metadata is stored in normal heap allocations, so they're measured by
  // mozjemalloc as `allocated`. Move them into `bookkeeping`.
  // They're also reported under explicit/heap-overhead/phc/fragmentation in
  // about:memory.
  size_t bookkeeping = metadata_size();
  aStats->allocated -= bookkeeping;
  aStats->bookkeeping += bookkeeping;
}

inline void MozJemallocPHC::jemalloc_ptr_info(const void* aPtr,
                                              jemalloc_ptr_info_t* aInfo) {
  if (!maybe_init()) {
    return MozJemalloc::jemalloc_ptr_info(aPtr, aInfo);
  }

  // We need to implement this properly, because various code locations do
  // things like checking that allocations are in the expected arena.
  PtrKind pk = PHC::sRegion->PtrKind(aPtr);
  if (pk.IsNothing()) {
    // Not a page allocation.
    return MozJemalloc::jemalloc_ptr_info(aPtr, aInfo);
  }

  if (pk.IsGuardPage()) {
    // Treat a guard page as unknown because there's no better alternative.
    *aInfo = {TagUnknown, nullptr, 0, 0};
    return;
  }

  // At this point we know we have an allocation page.
  uintptr_t index = pk.AllocPageIndex();

  MutexAutoLock lock(PHC::sPHC->mMutex);

  PHC::sPHC->FillJemallocPtrInfo(lock, aPtr, index, aInfo);
#if DEBUG
  LOG("JemallocPtrInfo(%p[%zu]) -> {%zu, %p, %zu, %zu}\n", aPtr, index,
      size_t(aInfo->tag), aInfo->addr, aInfo->size, aInfo->arenaId);
#else
  LOG("JemallocPtrInfo(%p[%zu]) -> {%zu, %p, %zu}\n", aPtr, index,
      size_t(aInfo->tag), aInfo->addr, aInfo->size);
#endif
}

inline void* MozJemallocPHC::moz_arena_malloc(arena_id_t aArenaId,
                                              size_t aReqSize) {
  return PageMalloc(Some(aArenaId), aReqSize);
}

inline void* MozJemallocPHC::moz_arena_calloc(arena_id_t aArenaId, size_t aNum,
                                              size_t aReqSize) {
  return PageCalloc(Some(aArenaId), aNum, aReqSize);
}

inline void* MozJemallocPHC::moz_arena_realloc(arena_id_t aArenaId,
                                               void* aOldPtr, size_t aNewSize) {
  return PageRealloc(Some(aArenaId), aOldPtr, aNewSize);
}

inline void MozJemallocPHC::moz_arena_free(arena_id_t aArenaId, void* aPtr) {
  return PageFree(Some(aArenaId), aPtr);
}

inline void* MozJemallocPHC::moz_arena_memalign(arena_id_t aArenaId,
                                                size_t aAlignment,
                                                size_t aReqSize) {
  return PageMemalign(Some(aArenaId), aAlignment, aReqSize);
}

namespace mozilla::phc {

bool IsPHCAllocation(const void* aPtr, AddrInfo* aOut) {
  if (!maybe_init()) {
    return false;
  }

  PtrKind pk = PHC::sRegion->PtrKind(aPtr);
  if (pk.IsNothing()) {
    return false;
  }

  bool isGuardPage = false;
  if (pk.IsGuardPage()) {
    if ((uintptr_t(aPtr) % kPageSize) < (kPageSize / 2)) {
      // The address is in the lower half of a guard page, so it's probably an
      // overflow. But first check that it is not on the very first guard
      // page, in which case it cannot be an overflow, and we ignore it.
      if (PHC::sRegion->IsInFirstGuardPage(aPtr)) {
        return false;
      }

      // Get the allocation page preceding this guard page.
      pk = PHC::sRegion->PtrKind(static_cast<const uint8_t*>(aPtr) - kPageSize);

    } else {
      // The address is in the upper half of a guard page, so it's probably an
      // underflow. Get the allocation page following this guard page.
      pk = PHC::sRegion->PtrKind(static_cast<const uint8_t*>(aPtr) + kPageSize);
    }

    // Make a note of the fact that we hit a guard page.
    isGuardPage = true;
  }

  // At this point we know we have an allocation page.
  uintptr_t index = pk.AllocPageIndex();

  if (aOut) {
    if (PHC::sPHC->mMutex.TryLock()) {
      PHC::sPHC->FillAddrInfo(index, aPtr, isGuardPage, *aOut);
      LOG("IsPHCAllocation: %zu, %p, %zu, %zu, %zu\n", size_t(aOut->mKind),
          aOut->mBaseAddr, aOut->mUsableSize,
          aOut->mAllocStack.isSome() ? aOut->mAllocStack->mLength : 0,
          aOut->mFreeStack.isSome() ? aOut->mFreeStack->mLength : 0);
      PHC::sPHC->mMutex.Unlock();
    } else {
      LOG("IsPHCAllocation: PHC is locked\n");
      aOut->mPhcWasLocked = true;
    }
  }
  return true;
}

void DisablePHCOnCurrentThread() {
  PHC::DisableOnCurrentThread();
  LOG("DisablePHCOnCurrentThread: %zu\n", 0ul);
}

void ReenablePHCOnCurrentThread() {
  PHC::sPHC->EnableOnCurrentThread();
  LOG("ReenablePHCOnCurrentThread: %zu\n", 0ul);
}

bool IsPHCEnabledOnCurrentThread() {
  bool enabled = !PHC::IsDisabledOnCurrentThread();
  LOG("IsPHCEnabledOnCurrentThread: %zu\n", size_t(enabled));
  return enabled;
}

void PHCMemoryUsage(MemoryUsage& aMemoryUsage) {
  if (!maybe_init()) {
    aMemoryUsage = MemoryUsage();
    return;
  }

  aMemoryUsage.mMetadataBytes = metadata_size();
  if (PHC::sPHC) {
    MutexAutoLock lock(PHC::sPHC->mMutex);
    aMemoryUsage.mFragmentationBytes = PHC::sPHC->FragmentationBytes();
  } else {
    aMemoryUsage.mFragmentationBytes = 0;
  }
}

void GetPHCStats(PHCStats& aStats) {
  if (!maybe_init()) {
    aStats = PHCStats();
    return;
  }

  MutexAutoLock lock(PHC::sPHC->mMutex);

  aStats = PHC::sPHC->GetPageStats(lock);
}

// Enable or Disable PHC at runtime.  If PHC is disabled it will still trap
// bad uses of previous allocations, but won't track any new allocations.
void SetPHCState(PHCState aState) {
  if (!maybe_init()) {
    return;
  }

  PHC::sPHC->SetState(aState);
}

void SetPHCProbabilities(int64_t aAvgDelayFirst, int64_t aAvgDelayNormal,
                         int64_t aAvgDelayPageReuse) {
  if (!maybe_init()) {
    return;
  }

  PHC::sPHC->SetProbabilities(aAvgDelayFirst, aAvgDelayNormal,
                              aAvgDelayPageReuse);
}

}  // namespace mozilla::phc
