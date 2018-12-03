/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CycleCollectedJSRuntime_h
#define mozilla_CycleCollectedJSRuntime_h

#include <queue>

#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/DeferredFinalize.h"
#include "mozilla/LinkedList.h"
#include "mozilla/mozalloc.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/SegmentedVector.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/TraceKind.h"

#include "nsCycleCollectionParticipant.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsTHashtable.h"

class nsCycleCollectionNoteRootCallback;
class nsIException;
class nsIRunnable;
class nsWrapperCache;

namespace js {
struct Class;
}  // namespace js

namespace mozilla {

class JSGCThingParticipant : public nsCycleCollectionParticipant {
 public:
  constexpr JSGCThingParticipant() : nsCycleCollectionParticipant(false) {}

  NS_IMETHOD_(void) Root(void*) override {
    MOZ_ASSERT(false, "Don't call Root on GC things");
  }

  NS_IMETHOD_(void) Unlink(void*) override {
    MOZ_ASSERT(false, "Don't call Unlink on GC things, as they may be dead");
  }

  NS_IMETHOD_(void) Unroot(void*) override {
    MOZ_ASSERT(false, "Don't call Unroot on GC things, as they may be dead");
  }

  NS_IMETHOD_(void) DeleteCycleCollectable(void* aPtr) override {
    MOZ_ASSERT(false, "Can't directly delete a cycle collectable GC thing");
  }

  NS_IMETHOD TraverseNative(void* aPtr,
                            nsCycleCollectionTraversalCallback& aCb) override;

  NS_DECL_CYCLE_COLLECTION_CLASS_NAME_METHOD(JSGCThingParticipant)
};

class JSZoneParticipant : public nsCycleCollectionParticipant {
 public:
  constexpr JSZoneParticipant() : nsCycleCollectionParticipant(false) {}

  NS_IMETHOD_(void) Root(void*) override {
    MOZ_ASSERT(false, "Don't call Root on GC things");
  }

  NS_IMETHOD_(void) Unlink(void*) override {
    MOZ_ASSERT(false, "Don't call Unlink on GC things, as they may be dead");
  }

  NS_IMETHOD_(void) Unroot(void*) override {
    MOZ_ASSERT(false, "Don't call Unroot on GC things, as they may be dead");
  }

  NS_IMETHOD_(void) DeleteCycleCollectable(void*) override {
    MOZ_ASSERT(false, "Can't directly delete a cycle collectable GC thing");
  }

  NS_IMETHOD TraverseNative(void* aPtr,
                            nsCycleCollectionTraversalCallback& aCb) override;

  NS_DECL_CYCLE_COLLECTION_CLASS_NAME_METHOD(JSZoneParticipant)
};

class IncrementalFinalizeRunnable;

struct JSHolderInfo {
  void* mHolder;
  nsScriptObjectTracer* mTracer;
};

class CycleCollectedJSRuntime {
  friend class JSGCThingParticipant;
  friend class JSZoneParticipant;
  friend class IncrementalFinalizeRunnable;
  friend class CycleCollectedJSContext;

 protected:
  CycleCollectedJSRuntime(JSContext* aMainContext);
  virtual ~CycleCollectedJSRuntime();

  virtual void Shutdown(JSContext* cx);

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;
  void UnmarkSkippableJSHolders();

  virtual void TraverseAdditionalNativeRoots(
      nsCycleCollectionNoteRootCallback& aCb) {}
  virtual void TraceAdditionalNativeGrayRoots(JSTracer* aTracer) {}

  virtual void CustomGCCallback(JSGCStatus aStatus) {}
  virtual void CustomOutOfMemoryCallback() {}

  LinkedList<CycleCollectedJSContext>& Contexts() { return mContexts; }

 private:
  void DescribeGCThing(bool aIsMarked, JS::GCCellPtr aThing,
                       nsCycleCollectionTraversalCallback& aCb) const;

  virtual bool DescribeCustomObjects(JSObject* aObject, const js::Class* aClasp,
                                     char (&aName)[72]) const {
    return false;  // We did nothing.
  }

  void NoteGCThingJSChildren(JS::GCCellPtr aThing,
                             nsCycleCollectionTraversalCallback& aCb) const;

  void NoteGCThingXPCOMChildren(const js::Class* aClasp, JSObject* aObj,
                                nsCycleCollectionTraversalCallback& aCb) const;

  virtual bool NoteCustomGCThingXPCOMChildren(
      const js::Class* aClasp, JSObject* aObj,
      nsCycleCollectionTraversalCallback& aCb) const {
    return false;  // We did nothing.
  }

  enum TraverseSelect { TRAVERSE_CPP, TRAVERSE_FULL };

  void TraverseGCThing(TraverseSelect aTs, JS::GCCellPtr aThing,
                       nsCycleCollectionTraversalCallback& aCb);

  void TraverseZone(JS::Zone* aZone, nsCycleCollectionTraversalCallback& aCb);

  static void TraverseObjectShim(void* aData, JS::GCCellPtr aThing);

  void TraverseNativeRoots(nsCycleCollectionNoteRootCallback& aCb);

  static void TraceBlackJS(JSTracer* aTracer, void* aData);
  static void TraceGrayJS(JSTracer* aTracer, void* aData);
  static void GCCallback(JSContext* aContext, JSGCStatus aStatus, void* aData);
  static void GCSliceCallback(JSContext* aContext, JS::GCProgress aProgress,
                              const JS::GCDescription& aDesc);
  static void GCNurseryCollectionCallback(JSContext* aContext,
                                          JS::GCNurseryProgress aProgress,
                                          JS::gcreason::Reason aReason);
  static void OutOfMemoryCallback(JSContext* aContext, void* aData);
  /**
   * Callback for reporting external string memory.
   */
  static size_t SizeofExternalStringCallback(
      JSString* aStr, mozilla::MallocSizeOf aMallocSizeOf);

  static bool ContextCallback(JSContext* aCx, unsigned aOperation, void* aData);

  virtual void TraceNativeBlackRoots(JSTracer* aTracer){};
  void TraceNativeGrayRoots(JSTracer* aTracer);

 public:
  void FinalizeDeferredThings(
      CycleCollectedJSContext::DeferredFinalizeType aType);

  virtual void PrepareForForgetSkippable() = 0;
  virtual void BeginCycleCollectionCallback() = 0;
  virtual void EndCycleCollectionCallback(CycleCollectorResults& aResults) = 0;
  virtual void DispatchDeferredDeletion(bool aContinuation,
                                        bool aPurge = false) = 0;

  // Two conditions, JSOutOfMemory and JSLargeAllocationFailure, are noted in
  // crash reports. Here are the values that can appear in the reports:
  enum class OOMState : uint32_t {
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
  };

  const char* OOMStateToString(const OOMState aOomState) const;

  void SetLargeAllocationFailure(OOMState aNewState);

  void AnnotateAndSetOutOfMemory(OOMState* aStatePtr, OOMState aNewState);
  void OnGC(JSContext* aContext, JSGCStatus aStatus);
  void OnOutOfMemory();
  void OnLargeAllocationFailure();

  JSRuntime* Runtime() { return mJSRuntime; }
  const JSRuntime* Runtime() const { return mJSRuntime; }

  bool HasPendingIdleGCTask() const {
    // Idle GC task associates with JSRuntime.
    MOZ_ASSERT_IF(mHasPendingIdleGCTask, Runtime());
    return mHasPendingIdleGCTask;
  }
  void SetPendingIdleGCTask() {
    // Idle GC task associates with JSRuntime.
    MOZ_ASSERT(Runtime());
    mHasPendingIdleGCTask = true;
  }
  void ClearPendingIdleGCTask() { mHasPendingIdleGCTask = false; }

  void RunIdleTimeGCTask() {
    if (HasPendingIdleGCTask()) {
      JS::RunIdleTimeGCTask(Runtime());
      ClearPendingIdleGCTask();
    }
  }

  bool IsIdleGCTaskNeeded() {
    return !HasPendingIdleGCTask() && Runtime() &&
           JS::IsIdleGCTaskNeeded(Runtime());
  }

 public:
  void AddJSHolder(void* aHolder, nsScriptObjectTracer* aTracer);
  void RemoveJSHolder(void* aHolder);
#ifdef DEBUG
  bool IsJSHolder(void* aHolder);
  void AssertNoObjectsToTrace(void* aPossibleJSHolder);
#endif

  nsCycleCollectionParticipant* GCThingParticipant();
  nsCycleCollectionParticipant* ZoneParticipant();

  nsresult TraverseRoots(nsCycleCollectionNoteRootCallback& aCb);
  virtual bool UsefulToMergeZones() const;
  void FixWeakMappingGrayBits() const;
  void CheckGrayBits() const;
  bool AreGCGrayBitsValid() const;
  void GarbageCollect(uint32_t aReason) const;

  // This needs to be an nsWrapperCache, not a JSObject, because we need to know
  // when our object gets moved.  But we can't trace it (and hence update our
  // storage), because we do not want to keep it alive.  nsWrapperCache handles
  // this for us via its "object moved" handling.
  void NurseryWrapperAdded(nsWrapperCache* aCache);
  void NurseryWrapperPreserved(JSObject* aWrapper);
  void JSObjectsTenured();

  void DeferredFinalize(DeferredFinalizeAppendFunction aAppendFunc,
                        DeferredFinalizeFunction aFunc, void* aThing);
  void DeferredFinalize(nsISupports* aSupports);

  void DumpJSHeap(FILE* aFile);

  // Add aZone to the set of zones waiting for a GC.
  void AddZoneWaitingForGC(JS::Zone* aZone) {
    mZonesWaitingForGC.PutEntry(aZone);
  }

  // Prepare any zones for GC that have been passed to AddZoneWaitingForGC()
  // since the last GC or since the last call to PrepareWaitingZonesForGC(),
  // whichever was most recent. If there were no such zones, prepare for a
  // full GC.
  void PrepareWaitingZonesForGC();

  // Get the current thread's CycleCollectedJSRuntime.  Returns null if there
  // isn't one.
  static CycleCollectedJSRuntime* Get();

  void AddContext(CycleCollectedJSContext* aContext);
  void RemoveContext(CycleCollectedJSContext* aContext);

#ifdef NIGHTLY_BUILD
  bool GetRecentDevError(JSContext* aContext,
                         JS::MutableHandle<JS::Value> aError);
  void ClearRecentDevError();
#endif  // defined(NIGHTLY_BUILD)

 private:
  LinkedList<CycleCollectedJSContext> mContexts;

  JSGCThingParticipant mGCThingCycleCollectorGlobal;

  JSZoneParticipant mJSZoneCycleCollectorGlobal;

  JSRuntime* mJSRuntime;
  bool mHasPendingIdleGCTask;

  JS::GCSliceCallback mPrevGCSliceCallback;
  JS::GCNurseryCollectionCallback mPrevGCNurseryCollectionCallback;

  mozilla::TimeStamp mLatestNurseryCollectionStart;

  SegmentedVector<JSHolderInfo, 1024, InfallibleAllocPolicy> mJSHolders;
  nsDataHashtable<nsPtrHashKey<void>, JSHolderInfo*> mJSHolderMap;

  typedef nsDataHashtable<nsFuncPtrHashKey<DeferredFinalizeFunction>, void*>
      DeferredFinalizerTable;
  DeferredFinalizerTable mDeferredFinalizerTable;

  RefPtr<IncrementalFinalizeRunnable> mFinalizeRunnable;

  OOMState mOutOfMemoryState;
  OOMState mLargeAllocationFailureState;

  static const size_t kSegmentSize = 512;
  SegmentedVector<nsWrapperCache*, kSegmentSize, InfallibleAllocPolicy>
      mNurseryObjects;
  SegmentedVector<JS::PersistentRooted<JSObject*>, kSegmentSize,
                  InfallibleAllocPolicy>
      mPreservedNurseryObjects;

  nsTHashtable<nsPtrHashKey<JS::Zone>> mZonesWaitingForGC;

  struct EnvironmentPreparer : public js::ScriptEnvironmentPreparer {
    void invoke(JS::HandleObject global, Closure& closure) override;
  };
  EnvironmentPreparer mEnvironmentPreparer;

#ifdef DEBUG
  bool mShutdownCalled;
#endif

#ifdef NIGHTLY_BUILD
  // Implementation of the error interceptor.
  // Built on nightly only to avoid any possible performance impact on release

  struct ErrorInterceptor final : public JSErrorInterceptor {
    virtual void interceptError(JSContext* cx, JS::HandleValue exn) override;
    void Shutdown(JSRuntime* rt);

    // Copy of the details of the exception.
    // We store this rather than the exception itself to avoid dealing with
    // complicated garbage-collection scenarios, e.g. a JSContext being killed
    // while we still hold onto an exception thrown from it.
    struct ErrorDetails {
      nsString mFilename;
      nsString mMessage;
      nsString mStack;
      JSExnType mType;
      uint32_t mLine;
      uint32_t mColumn;
    };

    // If we have encountered at least one developer error,
    // the first error we have encountered. Otherwise, or
    // if we have reset since the latest error, `None`.
    Maybe<ErrorDetails> mThrownError;
  };
  ErrorInterceptor mErrorInterceptor;

#endif  // defined(NIGHTLY_BUILD)
};

void TraceScriptHolder(nsISupports* aHolder, JSTracer* aTracer);

}  // namespace mozilla

#endif  // mozilla_CycleCollectedJSRuntime_h
