/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CycleCollectedJSRuntime_h__
#define mozilla_CycleCollectedJSRuntime_h__

#include "mozilla/MemoryReporting.h"
#include "jsapi.h"

#include "nsCycleCollector.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsTArray.h"

class nsCycleCollectionNoteRootCallback;
class nsIException;

namespace js {
class Class;
}

namespace mozilla {

class JSGCThingParticipant: public nsCycleCollectionParticipant
{
public:
  NS_IMETHOD_(void) Root(void* aPtr)
  {
  }

  NS_IMETHOD_(void) Unlink(void* aPtr)
  {
  }

  NS_IMETHOD_(void) Unroot(void* aPtr)
  {
  }

  NS_IMETHOD_(void) DeleteCycleCollectable(void* aPtr)
  {
  }

  NS_IMETHOD Traverse(void* aPtr, nsCycleCollectionTraversalCallback& aCb);
};

class JSZoneParticipant : public nsCycleCollectionParticipant
{
public:
  MOZ_CONSTEXPR JSZoneParticipant(): nsCycleCollectionParticipant()
  {
  }

  NS_IMETHOD_(void) Root(void* aPtr)
  {
  }

  NS_IMETHOD_(void) Unlink(void* aPtr)
  {
  }

  NS_IMETHOD_(void) Unroot(void* aPtr)
  {
  }

  NS_IMETHOD_(void) DeleteCycleCollectable(void* aPtr)
  {
  }

  NS_IMETHOD Traverse(void* aPtr, nsCycleCollectionTraversalCallback& aCb);
};

class IncrementalFinalizeRunnable;

// Contains various stats about the cycle collection.
struct CycleCollectorResults
{
  CycleCollectorResults()
  {
    // Initialize here so when we increment mNumSlices the first time we're
    // not using uninitialized memory.
    Init();
  }

  void Init()
  {
    mForcedGC = false;
    mMergedZones = false;
    mVisitedRefCounted = 0;
    mVisitedGCed = 0;
    mFreedRefCounted = 0;
    mFreedGCed = 0;
    mNumSlices = 1;
    // mNumSlices is initialized to one, because we call Init() after the
    // per-slice increment of mNumSlices has already occurred.
  }

  bool mForcedGC;
  bool mMergedZones;
  uint32_t mVisitedRefCounted;
  uint32_t mVisitedGCed;
  uint32_t mFreedRefCounted;
  uint32_t mFreedGCed;
  uint32_t mNumSlices;
};

class CycleCollectedJSRuntime
{
  friend class JSGCThingParticipant;
  friend class JSZoneParticipant;
  friend class IncrementalFinalizeRunnable;
protected:
  CycleCollectedJSRuntime(JSRuntime* aParentRuntime,
                          uint32_t aMaxbytes);
  virtual ~CycleCollectedJSRuntime();

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;
  void UnmarkSkippableJSHolders();

  virtual void TraverseAdditionalNativeRoots(nsCycleCollectionNoteRootCallback& aCb)
  {
  }
  virtual void TraceAdditionalNativeGrayRoots(JSTracer* aTracer)
  {
  }

  virtual void CustomGCCallback(JSGCStatus aStatus)
  {
  }
  virtual void CustomOutOfMemoryCallback()
  {
  }
  virtual void CustomLargeAllocationFailureCallback()
  {
  }
  virtual bool CustomContextCallback(JSContext* aCx, unsigned aOperation)
  {
    return true; // Don't block context creation.
  }

private:

  void
  DescribeGCThing(bool aIsMarked, void* aThing, JSGCTraceKind aTraceKind,
                  nsCycleCollectionTraversalCallback& aCb) const;

  virtual bool
  DescribeCustomObjects(JSObject* aObject, const js::Class* aClasp,
                        char (&aName)[72]) const
  {
    return false; // We did nothing.
  }

  void
  NoteGCThingJSChildren(void* aThing, JSGCTraceKind aTraceKind,
                        nsCycleCollectionTraversalCallback& aCb) const;

  void
  NoteGCThingXPCOMChildren(const js::Class* aClasp, JSObject* aObj,
                           nsCycleCollectionTraversalCallback& aCb) const;

  virtual bool
  NoteCustomGCThingXPCOMChildren(const js::Class* aClasp, JSObject* aObj,
                                 nsCycleCollectionTraversalCallback& aCb) const
  {
    return false; // We did nothing.
  }

  enum TraverseSelect {
    TRAVERSE_CPP,
    TRAVERSE_FULL
  };

  void
  TraverseGCThing(TraverseSelect aTs, void* aThing,
                  JSGCTraceKind aTraceKind,
                  nsCycleCollectionTraversalCallback& aCb);

  void
  TraverseZone(JS::Zone* aZone, nsCycleCollectionTraversalCallback& aCb);

  static void
  TraverseObjectShim(void* aData, void* aThing);

  void TraverseNativeRoots(nsCycleCollectionNoteRootCallback& aCb);

  static void TraceBlackJS(JSTracer* aTracer, void* aData);
  static void TraceGrayJS(JSTracer* aTracer, void* aData);
  static void GCCallback(JSRuntime* aRuntime, JSGCStatus aStatus, void* aData);
  static void OutOfMemoryCallback(JSContext *aContext, void *aData);
  static void LargeAllocationFailureCallback(void *aData);
  static bool ContextCallback(JSContext* aCx, unsigned aOperation,
                              void* aData);

  virtual void TraceNativeBlackRoots(JSTracer* aTracer) { };
  void TraceNativeGrayRoots(JSTracer* aTracer);

  enum DeferredFinalizeType {
    FinalizeIncrementally,
    FinalizeNow,
  };

  void FinalizeDeferredThings(DeferredFinalizeType aType);

public:
  // Two conditions, JSOutOfMemory and JSLargeAllocationFailure, are noted in
  // crash reports. Here are the values that can appear in the reports:
  MOZ_BEGIN_NESTED_ENUM_CLASS(OOMState, uint32_t)
    // The condition has never happened. No entry appears in the crash report.
    OK,

    // We are currently reporting the given condition.
    // 
    // Suppose a crash report contains "JSLargeAllocationFailure:
    // Reporting". This means we crashed while executing memory-pressure
    // observers, trying to shake loose some memory. The large allocation in
    // question did not return null: it is still on the stack. Had we not
    // crashed, it would have been retried.
    Reporting,

    // The condition has been reported since the last GC.
    //
    // If a crash report contains "JSOutOfMemory: Reported", that means a small
    // allocation failed, and then we crashed, probably due to buggy
    // error-handling code that ran after allocation returned null.
    //
    // This contrasts with "Reporting" which means that no error-handling code
    // had executed yet.
    Reported,

    // The condition has happened, but a GC cycle ended since then.
    //
    // GC is taken as a proxy for "we've been banging on the heap a good bit
    // now and haven't crashed; the OOM was probably handled correctly".
    Recovered
  MOZ_END_NESTED_ENUM_CLASS(OOMState)

private:
  void AnnotateAndSetOutOfMemory(OOMState *aStatePtr, OOMState aNewState);
  void OnGC(JSGCStatus aStatus);
  void OnOutOfMemory();
  void OnLargeAllocationFailure();

public:
  void AddJSHolder(void* aHolder, nsScriptObjectTracer* aTracer);
  void RemoveJSHolder(void* aHolder);
#ifdef DEBUG
  bool IsJSHolder(void* aHolder);
  void AssertNoObjectsToTrace(void* aPossibleJSHolder);
#endif

  already_AddRefed<nsIException> GetPendingException() const;
  void SetPendingException(nsIException* aException);

  nsCycleCollectionParticipant* GCThingParticipant();
  nsCycleCollectionParticipant* ZoneParticipant();

  nsresult TraverseRoots(nsCycleCollectionNoteRootCallback& aCb);
  bool UsefulToMergeZones() const;
  void FixWeakMappingGrayBits() const;
  bool AreGCGrayBitsValid() const;
  void GarbageCollect(uint32_t aReason) const;

  void DeferredFinalize(DeferredFinalizeAppendFunction aAppendFunc,
                        DeferredFinalizeFunction aFunc,
                        void* aThing);
  void DeferredFinalize(nsISupports* aSupports);

  void DumpJSHeap(FILE* aFile);

  virtual void PrepareForForgetSkippable() = 0;
  virtual void BeginCycleCollectionCallback() = 0;
  virtual void EndCycleCollectionCallback(CycleCollectorResults& aResults) = 0;
  virtual void DispatchDeferredDeletion(bool aContinuation) = 0;

  JSRuntime* Runtime() const
  {
    MOZ_ASSERT(mJSRuntime);
    return mJSRuntime;
  }

  // Get the current thread's CycleCollectedJSRuntime.  Returns null if there
  // isn't one.
  static CycleCollectedJSRuntime* Get();

private:
  JSGCThingParticipant mGCThingCycleCollectorGlobal;

  JSZoneParticipant mJSZoneCycleCollectorGlobal;

  JSRuntime* mJSRuntime;

  nsDataHashtable<nsPtrHashKey<void>, nsScriptObjectTracer*> mJSHolders;

  nsTArray<nsISupports*> mDeferredSupports;
  typedef nsDataHashtable<nsFuncPtrHashKey<DeferredFinalizeFunction>, void*>
    DeferredFinalizerTable;
  DeferredFinalizerTable mDeferredFinalizerTable;

  nsRefPtr<IncrementalFinalizeRunnable> mFinalizeRunnable;

  nsCOMPtr<nsIException> mPendingException;

  OOMState mOutOfMemoryState;
  OOMState mLargeAllocationFailureState;
};

MOZ_FINISH_NESTED_ENUM_CLASS(CycleCollectedJSRuntime::OOMState)

void TraceScriptHolder(nsISupports* aHolder, JSTracer* aTracer);

} // namespace mozilla

#endif // mozilla_CycleCollectedJSRuntime_h__
