/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PROFILER_PSEUDO_STACK_H_
#define PROFILER_PSEUDO_STACK_H_

#include "mozilla/ArrayUtils.h"
#include "mozilla/NullPtr.h"
#include <stdint.h>
#include "js/ProfilingStack.h"
#include <stdlib.h>
#include "mozilla/Atomics.h"

/* we duplicate this code here to avoid header dependencies
 * which make it more difficult to include in other places */
#if defined(_M_X64) || defined(__x86_64__)
#define V8_HOST_ARCH_X64 1
#elif defined(_M_IX86) || defined(__i386__) || defined(__i386)
#define V8_HOST_ARCH_IA32 1
#elif defined(__ARMEL__)
#define V8_HOST_ARCH_ARM 1
#else
#warning Please add support for your architecture in chromium_types.h
#endif

// STORE_SEQUENCER: Because signals can interrupt our profile modification
//                  we need to make stores are not re-ordered by the compiler
//                  or hardware to make sure the profile is consistent at
//                  every point the signal can fire.
#ifdef V8_HOST_ARCH_ARM
// TODO Is there something cheaper that will prevent
//      memory stores from being reordered

typedef void (*LinuxKernelMemoryBarrierFunc)(void);
LinuxKernelMemoryBarrierFunc pLinuxKernelMemoryBarrier __attribute__((weak)) =
    (LinuxKernelMemoryBarrierFunc) 0xffff0fa0;

# define STORE_SEQUENCER() pLinuxKernelMemoryBarrier()
#elif defined(V8_HOST_ARCH_IA32) || defined(V8_HOST_ARCH_X64)
# if defined(_MSC_VER)
#if _MSC_VER > 1400
#  include <intrin.h>
#else // _MSC_VER > 1400
    // MSVC2005 has a name collision bug caused when both <intrin.h> and <winnt.h> are included together.
#ifdef _WINNT_
#  define _interlockedbittestandreset _interlockedbittestandreset_NAME_CHANGED_TO_AVOID_MSVS2005_ERROR
#  define _interlockedbittestandset _interlockedbittestandset_NAME_CHANGED_TO_AVOID_MSVS2005_ERROR
#  include <intrin.h>
#else
#  include <intrin.h>
#  define _interlockedbittestandreset _interlockedbittestandreset_NAME_CHANGED_TO_AVOID_MSVS2005_ERROR
#  define _interlockedbittestandset _interlockedbittestandset_NAME_CHANGED_TO_AVOID_MSVS2005_ERROR
#endif
   // Even though MSVC2005 has the intrinsic _ReadWriteBarrier, it fails to link to it when it's
   // not explicitly declared.
#  pragma intrinsic(_ReadWriteBarrier)
#endif // _MSC_VER > 1400
#  define STORE_SEQUENCER() _ReadWriteBarrier();
# elif defined(__INTEL_COMPILER)
#  define STORE_SEQUENCER() __memory_barrier();
# elif __GNUC__
#  define STORE_SEQUENCER() asm volatile("" ::: "memory");
# else
#  error "Memory clobber not supported for your compiler."
# endif
#else
# error "Memory clobber not supported for your platform."
#endif

// We can't include <algorithm> because it causes issues on OS X, so we use
// our own min function.
static inline uint32_t sMin(uint32_t l, uint32_t r) {
  return l < r ? l : r;
}

// A stack entry exists to allow the JS engine to inform SPS of the current
// backtrace, but also to instrument particular points in C++ in case stack
// walking is not available on the platform we are running on.
//
// Each entry has a descriptive string, a relevant stack address, and some extra
// information the JS engine might want to inform SPS of. This class inherits
// from the JS engine's version of the entry to ensure that the size and layout
// of the two representations are consistent.
class StackEntry : public js::ProfileEntry
{
};

class ProfilerMarkerPayload;
template<typename T>
class ProfilerLinkedList;
class JSStreamWriter;
class JSCustomArray;
class ThreadProfile;
class ProfilerMarker {
  friend class ProfilerLinkedList<ProfilerMarker>;
public:
  ProfilerMarker(const char* aMarkerName,
         ProfilerMarkerPayload* aPayload = nullptr,
         float aTime = 0);

  ~ProfilerMarker();

  const char* GetMarkerName() const {
    return mMarkerName;
  }

  void
  StreamJSObject(JSStreamWriter& b) const;

  void SetGeneration(int aGenID);

  bool HasExpired(int aGenID) const {
    return mGenID + 2 <= aGenID;
  }

  float GetTime();

private:
  char* mMarkerName;
  ProfilerMarkerPayload* mPayload;
  ProfilerMarker* mNext;
  float mTime;
  int mGenID;
};

// Foward declaration
typedef struct _UnwinderThreadBuffer UnwinderThreadBuffer;

/**
 * This struct is used to add a mNext field to UnwinderThreadBuffer objects for
 * use with ProfilerLinkedList. It is done this way so that UnwinderThreadBuffer
 * may continue to be opaque with respect to code outside of UnwinderThread2.cpp
 */
struct LinkedUWTBuffer
{
  LinkedUWTBuffer()
    :mNext(nullptr)
  {}
  virtual ~LinkedUWTBuffer() {}
  virtual UnwinderThreadBuffer* GetBuffer() = 0;
  LinkedUWTBuffer*  mNext;
};

template<typename T>
class ProfilerLinkedList {
public:
  ProfilerLinkedList()
    : mHead(nullptr)
    , mTail(nullptr)
  {}

  void insert(T* elem)
  {
    if (!mTail) {
      mHead = elem;
      mTail = elem;
    } else {
      mTail->mNext = elem;
      mTail = elem;
    }
    elem->mNext = nullptr;
  }

  T* popHead()
  {
    if (!mHead) {
      MOZ_ASSERT(false);
      return nullptr;
    }

    T* head = mHead;

    mHead = head->mNext;
    if (!mHead) {
      mTail = nullptr;
    }

    return head;
  }

  const T* peek() {
    return mHead;
  }

private:
  T* mHead;
  T* mTail;
};

typedef ProfilerLinkedList<ProfilerMarker> ProfilerMarkerLinkedList;
typedef ProfilerLinkedList<LinkedUWTBuffer> UWTBufferLinkedList;

class PendingMarkers {
public:
  PendingMarkers()
    : mSignalLock(false)
  {}

  ~PendingMarkers();

  void addMarker(ProfilerMarker *aMarker);

  void updateGeneration(int aGenID);

  /**
   * Track a marker which has been inserted into the ThreadProfile.
   * This marker can safely be deleted once the generation has
   * expired.
   */
  void addStoredMarker(ProfilerMarker *aStoredMarker);

  // called within signal. Function must be reentrant
  ProfilerMarkerLinkedList* getPendingMarkers()
  {
    // if mSignalLock then the stack is inconsistent because it's being
    // modified by the profiled thread. Post pone these markers
    // for the next sample. The odds of a livelock are nearly impossible
    // and would show up in a profile as many sample in 'addMarker' thus
    // we ignore this scenario.
    if (mSignalLock) {
      return nullptr;
    }
    return &mPendingMarkers;
  }

  void clearMarkers()
  {
    while (mPendingMarkers.peek()) {
      delete mPendingMarkers.popHead();
    }
    while (mStoredMarkers.peek()) {
      delete mStoredMarkers.popHead();
    }
  }

private:
  // Keep a list of active markers to be applied to the next sample taken
  ProfilerMarkerLinkedList mPendingMarkers;
  ProfilerMarkerLinkedList mStoredMarkers;
  // If this is set then it's not safe to read mStackPointer from the signal handler
  volatile bool mSignalLock;
  // We don't want to modify _markers from within the signal so we allow
  // it to queue a clear operation.
  volatile mozilla::sig_safe_t mGenID;
};

class PendingUWTBuffers
{
public:
  PendingUWTBuffers()
    : mSignalLock(false)
  {
  }

  void addLinkedUWTBuffer(LinkedUWTBuffer* aBuff)
  {
    MOZ_ASSERT(aBuff);
    mSignalLock = true;
    STORE_SEQUENCER();
    mPendingUWTBuffers.insert(aBuff);
    STORE_SEQUENCER();
    mSignalLock = false;
  }

  // called within signal. Function must be reentrant
  UWTBufferLinkedList* getLinkedUWTBuffers()
  {
    if (mSignalLock) {
      return nullptr;
    }
    return &mPendingUWTBuffers;
  }

private:
  UWTBufferLinkedList mPendingUWTBuffers;
  volatile bool       mSignalLock;
};

// Stub eventMarker function for js-engine event generation.
void ProfilerJSEventMarker(const char *event);

// the PseudoStack members are read by signal
// handlers, so the mutation of them needs to be signal-safe.
struct PseudoStack
{
public:
  PseudoStack()
    : mStackPointer(0)
    , mSleepId(0)
    , mSleepIdObserved(0)
    , mSleeping(false)
    , mRuntime(nullptr)
    , mStartJSSampling(false)
    , mPrivacyMode(false)
  { }

  ~PseudoStack() {
    if (mStackPointer != 0) {
      // We're releasing the pseudostack while it's still in use.
      // The label macros keep a non ref counted reference to the
      // stack to avoid a TLS. If these are not all cleared we will
      // get a use-after-free so better to crash now.
      abort();
    }
  }

  // This is called on every profiler restart. Put things that should happen at that time here.
  void reinitializeOnResume() {
    // This is needed to cause an initial sample to be taken from sleeping threads. Otherwise sleeping
    // threads would not have any samples to copy forward while sleeping.
    mSleepId++;
  }

  void addLinkedUWTBuffer(LinkedUWTBuffer* aBuff)
  {
    mPendingUWTBuffers.addLinkedUWTBuffer(aBuff);
  }

  UWTBufferLinkedList* getLinkedUWTBuffers()
  {
    return mPendingUWTBuffers.getLinkedUWTBuffers();
  }

  void addMarker(const char *aMarkerStr, ProfilerMarkerPayload *aPayload, float aTime)
  {
    ProfilerMarker* marker = new ProfilerMarker(aMarkerStr, aPayload, aTime);
    mPendingMarkers.addMarker(marker);
  }

  void addStoredMarker(ProfilerMarker *aStoredMarker) {
    mPendingMarkers.addStoredMarker(aStoredMarker);
  }

  void updateGeneration(int aGenID) {
    mPendingMarkers.updateGeneration(aGenID);
  }

  // called within signal. Function must be reentrant
  ProfilerMarkerLinkedList* getPendingMarkers()
  {
    return mPendingMarkers.getPendingMarkers();
  }

  void push(const char *aName, js::ProfileEntry::Category aCategory, uint32_t line)
  {
    push(aName, aCategory, nullptr, false, line);
  }

  void push(const char *aName, js::ProfileEntry::Category aCategory,
    void *aStackAddress, bool aCopy, uint32_t line)
  {
    if (size_t(mStackPointer) >= mozilla::ArrayLength(mStack)) {
      mStackPointer++;
      return;
    }

    volatile StackEntry &entry = mStack[mStackPointer];

    // Make sure we increment the pointer after the name has
    // been written such that mStack is always consistent.
    entry.setLabel(aName);
    entry.setCppFrame(aStackAddress, line);
    MOZ_ASSERT(entry.flags() == js::ProfileEntry::IS_CPP_ENTRY);

    uint32_t uint_category = static_cast<uint32_t>(aCategory);
    MOZ_ASSERT(
      uint_category >= static_cast<uint32_t>(js::ProfileEntry::Category::FIRST) &&
      uint_category <= static_cast<uint32_t>(js::ProfileEntry::Category::LAST));

    entry.setFlag(uint_category);

    // Track if mLabel needs a copy.
    if (aCopy)
      entry.setFlag(js::ProfileEntry::FRAME_LABEL_COPY);
    else
      entry.unsetFlag(js::ProfileEntry::FRAME_LABEL_COPY);

    // Prevent the optimizer from re-ordering these instructions
    STORE_SEQUENCER();
    mStackPointer++;
  }
  void pop()
  {
    mStackPointer--;
  }
  bool isEmpty()
  {
    return mStackPointer == 0;
  }
  uint32_t stackSize() const
  {
    return sMin(mStackPointer, mozilla::sig_safe_t(mozilla::ArrayLength(mStack)));
  }

  void sampleRuntime(JSRuntime *runtime) {
    mRuntime = runtime;
    if (!runtime) {
      // JS shut down
      return;
    }

    static_assert(sizeof(mStack[0]) == sizeof(js::ProfileEntry),
                  "mStack must be binary compatible with js::ProfileEntry.");
    js::SetRuntimeProfilingStack(runtime,
                                 (js::ProfileEntry*) mStack,
                                 (uint32_t*) &mStackPointer,
                                 uint32_t(mozilla::ArrayLength(mStack)));
    if (mStartJSSampling)
      enableJSSampling();
  }
  void enableJSSampling() {
    if (mRuntime) {
      js::EnableRuntimeProfilingStack(mRuntime, true);
      js::RegisterRuntimeProfilingEventMarker(mRuntime, &ProfilerJSEventMarker);
      mStartJSSampling = false;
    } else {
      mStartJSSampling = true;
    }
  }
  void jsOperationCallback() {
    if (mStartJSSampling)
      enableJSSampling();
  }
  void disableJSSampling() {
    mStartJSSampling = false;
    if (mRuntime)
      js::EnableRuntimeProfilingStack(mRuntime, false);
  }

  // Keep a list of active checkpoints
  StackEntry volatile mStack[1024];
 private:
  // Keep a list of pending markers that must be moved
  // to the circular buffer
  PendingMarkers mPendingMarkers;
  // List of LinkedUWTBuffers that must be processed on the next tick
  PendingUWTBuffers mPendingUWTBuffers;
  // This may exceed the length of mStack, so instead use the stackSize() method
  // to determine the number of valid samples in mStack
  mozilla::sig_safe_t mStackPointer;
  // Incremented at every sleep/wake up of the thread
  int mSleepId;
  // Previous id observed. If this is not the same as mSleepId, this thread is not sleeping in the same place any more
  mozilla::Atomic<int> mSleepIdObserved;
  // Keeps tack of whether the thread is sleeping or not (1 when sleeping 0 when awake)
  mozilla::Atomic<int> mSleeping;
 public:
  // The runtime which is being sampled
  JSRuntime *mRuntime;
  // Start JS Profiling when possible
  bool mStartJSSampling;
  bool mPrivacyMode;

  enum SleepState {NOT_SLEEPING, SLEEPING_FIRST, SLEEPING_AGAIN};

  // The first time this is called per sleep cycle we return SLEEPING_FIRST
  // and any other subsequent call within the same sleep cycle we return SLEEPING_AGAIN
  SleepState observeSleeping() {
    if (mSleeping != 0) {
      if (mSleepIdObserved == mSleepId) {
        return SLEEPING_AGAIN;
      } else {
        mSleepIdObserved = mSleepId;
        return SLEEPING_FIRST;
      }
    } else {
      return NOT_SLEEPING;
    }
  }


  // Call this whenever the current thread sleeps or wakes up
  // Calling setSleeping with the same value twice in a row is an error
  void setSleeping(int sleeping) {
    MOZ_ASSERT(mSleeping != sleeping);
    mSleepId++;
    mSleeping = sleeping;
  }
};

#endif

