/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsJSEnvironment_h
#define nsJSEnvironment_h

#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "prtime.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIXPConnect.h"
#include "nsIArray.h"
#include "mozilla/Attributes.h"
#include "nsThreadUtils.h"

class nsICycleCollectorListener;
class nsIXPConnectJSObjectHolder;
class nsScriptNameSpaceManager;
class nsCycleCollectionNoteRootCallback;

namespace JS {
class AutoValueVector;
}

namespace mozilla {
template <class> class Maybe;
struct CycleCollectorResults;
}

// The amount of time we wait between a request to GC (due to leaving
// a page) and doing the actual GC.
#define NS_GC_DELAY                 4000 // ms

class nsJSContext : public nsIScriptContext
{
public:
  nsJSContext(bool aGCOnDestruction, nsIScriptGlobalObject* aGlobalObject);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsJSContext,
                                                         nsIScriptContext)

  virtual nsIScriptGlobalObject *GetGlobalObject() MOZ_OVERRIDE;
  inline nsIScriptGlobalObject *GetGlobalObjectRef() { return mGlobalObjectRef; }

  virtual JSContext* GetNativeContext() MOZ_OVERRIDE;
  virtual nsresult InitContext() MOZ_OVERRIDE;
  virtual bool IsContextInitialized() MOZ_OVERRIDE;

  virtual nsresult SetProperty(JS::Handle<JSObject*> aTarget, const char* aPropName, nsISupports* aVal) MOZ_OVERRIDE;

  virtual bool GetProcessingScriptTag() MOZ_OVERRIDE;
  virtual void SetProcessingScriptTag(bool aResult) MOZ_OVERRIDE;

  virtual nsresult InitClasses(JS::Handle<JSObject*> aGlobalObj) MOZ_OVERRIDE;

  virtual void WillInitializeContext() MOZ_OVERRIDE;
  virtual void DidInitializeContext() MOZ_OVERRIDE;

  virtual void SetWindowProxy(JS::Handle<JSObject*> aWindowProxy) MOZ_OVERRIDE;
  virtual JSObject* GetWindowProxy() MOZ_OVERRIDE;
  virtual JSObject* GetWindowProxyPreserveColor() MOZ_OVERRIDE;

  static void LoadStart();
  static void LoadEnd();

  enum IsShrinking {
    ShrinkingGC,
    NonShrinkingGC
  };

  enum IsIncremental {
    IncrementalGC,
    NonIncrementalGC
  };

  // Setup all the statics etc - safe to call multiple times after Startup().
  void EnsureStatics();

  static void GarbageCollectNow(JS::gcreason::Reason reason,
                                IsIncremental aIncremental = NonIncrementalGC,
                                IsShrinking aShrinking = NonShrinkingGC,
                                int64_t aSliceMillis = 0);
  static void ShrinkGCBuffersNow();

  // If aExtraForgetSkippableCalls is -1, forgetSkippable won't be
  // called even if the previous collection was GC.
  static void CycleCollectNow(nsICycleCollectorListener *aListener = nullptr,
                              int32_t aExtraForgetSkippableCalls = 0);

  // Run a cycle collector slice, using a heuristic to decide how long to run it.
  static void RunCycleCollectorSlice();

  // Run a cycle collector slice, using the given work budget.
  static void RunCycleCollectorWorkSlice(int64_t aWorkBudget);

  static void BeginCycleCollectionCallback();
  static void EndCycleCollectionCallback(mozilla::CycleCollectorResults &aResults);

  // Return the longest CC slice time since ClearMaxCCSliceTime() was last called.
  static uint32_t GetMaxCCSliceTimeSinceClear();
  static void ClearMaxCCSliceTime();

  static void RunNextCollectorTimer();

  static void PokeGC(JS::gcreason::Reason aReason, int aDelay = 0);
  static void KillGCTimer();

  static void PokeShrinkGCBuffers();
  static void KillShrinkGCBuffersTimer();

  static void MaybePokeCC();
  static void KillCCTimer();
  static void KillICCTimer();
  static void KillFullGCTimer();
  static void KillInterSliceGCTimer();

  // Calling LikelyShortLivingObjectCreated() makes a GC more likely.
  static void LikelyShortLivingObjectCreated();

  virtual void GC(JS::gcreason::Reason aReason) MOZ_OVERRIDE;

  static uint32_t CleanupsSinceLastGC();

  nsIScriptGlobalObject* GetCachedGlobalObject()
  {
    // Verify that we have a global so that this
    // does always return a null when GetGlobalObject() is null.
    JSObject* global = GetWindowProxy();
    return global ? mGlobalObjectRef.get() : nullptr;
  }
protected:
  virtual ~nsJSContext();

  // Helper to convert xpcom datatypes to jsvals.
  nsresult ConvertSupportsTojsvals(nsISupports *aArgs,
                                   JS::Handle<JSObject*> aScope,
                                   JS::AutoValueVector &aArgsOut);

  nsresult AddSupportsPrimitiveTojsvals(nsISupports *aArg, JS::Value *aArgv);

  // Report the pending exception on our mContext, if any.  This
  // function will set aside the frame chain on mContext before
  // reporting.
  void ReportPendingException();

private:
  void DestroyJSContext();

  nsrefcnt GetCCRefcnt();

  JSContext *mContext;
  JS::Heap<JSObject*> mWindowProxy;

  bool mIsInitialized;
  bool mGCOnDestruction;
  bool mProcessingScriptTag;

  PRTime mModalStateTime;
  uint32_t mModalStateDepth;

  // mGlobalObjectRef ensures that the outer window stays alive as long as the
  // context does. It is eventually collected by the cycle collector.
  nsCOMPtr<nsIScriptGlobalObject> mGlobalObjectRef;

  static void JSOptionChangedCallback(const char *pref, void *data);

  static bool DOMOperationCallback(JSContext *cx);
};

class nsIJSRuntimeService;
class nsIPrincipal;
class nsPIDOMWindow;

namespace mozilla {
namespace dom {

void StartupJSEnvironment();
void ShutdownJSEnvironment();

// Get the NameSpaceManager, creating if necessary
nsScriptNameSpaceManager* GetNameSpaceManager();

// Runnable that's used to do async error reporting
class AsyncErrorReporter : public nsRunnable
{
public:
  // aWindow may be null if this error report is not associated with a window
  AsyncErrorReporter(JSRuntime* aRuntime,
                     JSErrorReport* aErrorReport,
                     const char* aFallbackMessage,
                     bool aIsChromeError, // To determine category
                     nsPIDOMWindow* aWindow);

  NS_IMETHOD Run()
  {
    ReportError();
    return NS_OK;
  }

protected:
  // Do the actual error reporting
  void ReportError();

  nsString mErrorMsg;
  nsString mFileName;
  nsString mSourceLine;
  nsCString mCategory;
  uint32_t mLineNumber;
  uint32_t mColumn;
  uint32_t mFlags;
  uint64_t mInnerWindowID;
};

} // namespace dom
} // namespace mozilla

// An interface for fast and native conversion to/from nsIArray. If an object
// supports this interface, JS can reach directly in for the argv, and avoid
// nsISupports conversion. If this interface is not supported, the object will
// be queried for nsIArray, and everything converted via xpcom objects.
#define NS_IJSARGARRAY_IID \
{ 0xb6acdac8, 0xf5c6, 0x432c, \
  { 0xa8, 0x6e, 0x33, 0xee, 0xb1, 0xb0, 0xcd, 0xdc } }

class nsIJSArgArray : public nsIArray
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IJSARGARRAY_IID)
  // Bug 312003 describes why this must be "void **", but after calling argv
  // may be cast to JS::Value* and the args found at:
  //    ((JS::Value*)argv)[0], ..., ((JS::Value*)argv)[argc - 1]
  virtual nsresult GetArgs(uint32_t *argc, void **argv) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIJSArgArray, NS_IJSARGARRAY_IID)

/* prototypes */
void NS_ScriptErrorReporter(JSContext *cx, const char *message, JSErrorReport *report);

JSObject* NS_DOMReadStructuredClone(JSContext* cx,
                                    JSStructuredCloneReader* reader, uint32_t tag,
                                    uint32_t data, void* closure);

bool NS_DOMWriteStructuredClone(JSContext* cx,
                                JSStructuredCloneWriter* writer,
                                JS::Handle<JSObject*> obj, void *closure);

void NS_DOMStructuredCloneError(JSContext* cx, uint32_t errorid);

#endif /* nsJSEnvironment_h */
