/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string>
#include <sstream>
#include "GeckoProfiler.h"
#include "nsIFileStreams.h"
#include "nsProfiler.h"
#include "nsProfilerStartParams.h"
#include "nsMemory.h"
#include "nsString.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"
#include "nsIInterfaceRequestor.h"
#include "nsILoadContext.h"
#include "nsIWebNavigation.h"
#include "nsIInterfaceRequestorUtils.h"
#include "shared-libraries.h"
#include "js/JSON.h"
#include "js/Value.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/Move.h"
#include "mozilla/SystemGroup.h"
#include "nsLocalFile.h"
#include "nsThreadUtils.h"
#include "ProfilerParent.h"
#include "platform.h"

using namespace mozilla;

using dom::AutoJSAPI;
using dom::Promise;
using std::string;

extern "C" {
// This function is defined in the profiler rust module at
// tools/profiler/rust-helper. nsProfiler::SymbolTable and CompactSymbolTable
// have identical memory layout.
bool profiler_get_symbol_table(const char* debug_path, const char* breakpad_id,
                               nsProfiler::SymbolTable* symbol_table);
}

NS_IMPL_ISUPPORTS(nsProfiler, nsIProfiler, nsIObserver)

nsProfiler::nsProfiler()
    : mLockedForPrivateBrowsing(false),
      mPendingProfiles(0),
      mGathering(false) {}

nsProfiler::~nsProfiler() {
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->RemoveObserver(this, "chrome-document-global-created");
    observerService->RemoveObserver(this, "last-pb-context-exited");
  }
  if (mSymbolTableThread) {
    mSymbolTableThread->Shutdown();
  }
}

nsresult nsProfiler::Init() {
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(this, "chrome-document-global-created", false);
    observerService->AddObserver(this, "last-pb-context-exited", false);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::Observe(nsISupports* aSubject, const char* aTopic,
                    const char16_t* aData) {
  // The profiler's handling of private browsing is as simple as possible: it
  // is stopped when the first PB window opens, and left stopped when the last
  // PB window closes.
  if (strcmp(aTopic, "chrome-document-global-created") == 0) {
    nsCOMPtr<nsIInterfaceRequestor> requestor = do_QueryInterface(aSubject);
    nsCOMPtr<nsIWebNavigation> parentWebNav = do_GetInterface(requestor);
    nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(parentWebNav);
    if (loadContext && loadContext->UsePrivateBrowsing() &&
        !mLockedForPrivateBrowsing) {
      mLockedForPrivateBrowsing = true;
      profiler_stop();
    }
  } else if (strcmp(aTopic, "last-pb-context-exited") == 0) {
    mLockedForPrivateBrowsing = false;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::CanProfile(bool* aCanProfile) {
  *aCanProfile = !mLockedForPrivateBrowsing;
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::StartProfiler(uint32_t aEntries, double aInterval,
                          const char** aFeatures, uint32_t aFeatureCount,
                          const char** aFilters, uint32_t aFilterCount,
                          double aDuration) {
  if (mLockedForPrivateBrowsing) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  ResetGathering();

  uint32_t features = ParseFeaturesFromStringArray(aFeatures, aFeatureCount);
  Maybe<double> duration = aDuration > 0.0 ? Some(aDuration) : Nothing();
  profiler_start(aEntries, aInterval, features, aFilters, aFilterCount,
                 duration);

  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::StopProfiler() {
  // If we have a Promise in flight, we should reject it.
  if (mPromiseHolder.isSome()) {
    mPromiseHolder->RejectIfExists(NS_ERROR_DOM_ABORT_ERR, __func__);
  }
  mExitProfiles.Clear();
  ResetGathering();

  profiler_stop();

  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::IsPaused(bool* aIsPaused) {
  *aIsPaused = profiler_is_paused();
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::PauseSampling() {
  profiler_pause();
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::ResumeSampling() {
  profiler_resume();
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::AddMarker(const char* aMarker) {
  profiler_add_marker(aMarker);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetProfile(double aSinceTime, char** aProfile) {
  mozilla::UniquePtr<char[]> profile = profiler_get_profile(aSinceTime);
  *aProfile = profile.release();
  return NS_OK;
}

namespace {
struct StringWriteFunc : public JSONWriteFunc {
  nsAString& mBuffer;  // This struct must not outlive this buffer
  explicit StringWriteFunc(nsAString& buffer) : mBuffer(buffer) {}

  void Write(const char* aStr) override {
    mBuffer.Append(NS_ConvertUTF8toUTF16(aStr));
  }
};
}  // namespace

NS_IMETHODIMP
nsProfiler::GetSharedLibraries(JSContext* aCx,
                               JS::MutableHandle<JS::Value> aResult) {
  JS::RootedValue val(aCx);
  {
    nsString buffer;
    JSONWriter w(MakeUnique<StringWriteFunc>(buffer));
    w.StartArrayElement();
    AppendSharedLibraries(w);
    w.EndArray();
    MOZ_ALWAYS_TRUE(JS_ParseJSON(aCx,
                                 static_cast<const char16_t*>(buffer.get()),
                                 buffer.Length(), &val));
  }
  JS::RootedObject obj(aCx, &val.toObject());
  if (!obj) {
    return NS_ERROR_FAILURE;
  }
  aResult.setObject(*obj);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::DumpProfileToFile(const char* aFilename) {
  profiler_save_profile_to_file(aFilename);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetProfileData(double aSinceTime, JSContext* aCx,
                           JS::MutableHandle<JS::Value> aResult) {
  mozilla::UniquePtr<char[]> profile = profiler_get_profile(aSinceTime);
  if (!profile) {
    return NS_ERROR_FAILURE;
  }

  NS_ConvertUTF8toUTF16 js_string(nsDependentCString(profile.get()));
  auto profile16 = static_cast<const char16_t*>(js_string.get());

  JS::RootedValue val(aCx);
  MOZ_ALWAYS_TRUE(JS_ParseJSON(aCx, profile16, js_string.Length(), &val));

  aResult.set(val);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetProfileDataAsync(double aSinceTime, JSContext* aCx,
                                Promise** aPromise) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!profiler_is_active()) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(!aCx)) {
    return NS_ERROR_FAILURE;
  }

  nsIGlobalObject* globalObject = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!globalObject)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(globalObject, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  StartGathering(aSinceTime)
      ->Then(GetMainThreadSerialEventTarget(), __func__,
             [promise](nsCString aResult) {
               AutoJSAPI jsapi;
               if (NS_WARN_IF(!jsapi.Init(promise->GlobalJSObject()))) {
                 // We're really hosed if we can't get a JS context for some
                 // reason.
                 promise->MaybeReject(NS_ERROR_DOM_UNKNOWN_ERR);
                 return;
               }

               JSContext* cx = jsapi.cx();

               // Now parse the JSON so that we resolve with a JS Object.
               JS::RootedValue val(cx);
               {
                 NS_ConvertUTF8toUTF16 js_string(aResult);
                 if (!JS_ParseJSON(
                         cx, static_cast<const char16_t*>(js_string.get()),
                         js_string.Length(), &val)) {
                   if (!jsapi.HasException()) {
                     promise->MaybeReject(NS_ERROR_DOM_UNKNOWN_ERR);
                   } else {
                     JS::RootedValue exn(cx);
                     DebugOnly<bool> gotException = jsapi.StealException(&exn);
                     MOZ_ASSERT(gotException);

                     jsapi.ClearException();
                     promise->MaybeReject(cx, exn);
                   }
                 } else {
                   promise->MaybeResolve(val);
                 }
               }
             },
             [promise](nsresult aRv) { promise->MaybeReject(aRv); });

  promise.forget(aPromise);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetProfileDataAsArrayBuffer(double aSinceTime, JSContext* aCx,
                                        Promise** aPromise) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!profiler_is_active()) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(!aCx)) {
    return NS_ERROR_FAILURE;
  }

  nsIGlobalObject* globalObject = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!globalObject)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(globalObject, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  StartGathering(aSinceTime)
      ->Then(GetMainThreadSerialEventTarget(), __func__,
             [promise](nsCString aResult) {
               AutoJSAPI jsapi;
               if (NS_WARN_IF(!jsapi.Init(promise->GlobalJSObject()))) {
                 // We're really hosed if we can't get a JS context for some
                 // reason.
                 promise->MaybeReject(NS_ERROR_DOM_UNKNOWN_ERR);
                 return;
               }

               JSContext* cx = jsapi.cx();
               JSObject* typedArray = dom::ArrayBuffer::Create(
                   cx, aResult.Length(),
                   reinterpret_cast<const uint8_t*>(aResult.Data()));
               if (typedArray) {
                 JS::RootedValue val(cx, JS::ObjectValue(*typedArray));
                 promise->MaybeResolve(val);
               } else {
                 promise->MaybeReject(NS_ERROR_OUT_OF_MEMORY);
               }
             },
             [promise](nsresult aRv) { promise->MaybeReject(aRv); });

  promise.forget(aPromise);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::DumpProfileToFileAsync(const nsACString& aFilename,
                                   double aSinceTime, JSContext* aCx,
                                   Promise** aPromise) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!profiler_is_active()) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(!aCx)) {
    return NS_ERROR_FAILURE;
  }

  nsIGlobalObject* globalObject = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!globalObject)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(globalObject, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  nsCString filename(aFilename);

  StartGathering(aSinceTime)
      ->Then(GetMainThreadSerialEventTarget(), __func__,
             [filename, promise](const nsCString& aResult) {
               nsCOMPtr<nsIFile> file =
                   do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
               nsresult rv = file->InitWithNativePath(filename);
               if (NS_FAILED(rv)) {
                 MOZ_CRASH();
               }
               nsCOMPtr<nsIFileOutputStream> of = do_CreateInstance(
                   "@mozilla.org/network/file-output-stream;1");
               of->Init(file, -1, -1, 0);
               uint32_t sz;
               of->Write(aResult.get(), aResult.Length(), &sz);
               of->Close();

               promise->MaybeResolveWithUndefined();
             },
             [promise](nsresult aRv) { promise->MaybeReject(aRv); });

  promise.forget(aPromise);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetSymbolTable(const nsACString& aDebugPath,
                           const nsACString& aBreakpadID, JSContext* aCx,
                           nsISupports** aPromise) {
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!aCx)) {
    return NS_ERROR_FAILURE;
  }

  nsIGlobalObject* globalObject =
      xpc::NativeGlobal(JS::CurrentGlobalOrNull(aCx));

  if (NS_WARN_IF(!globalObject)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(globalObject, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  GetSymbolTableMozPromise(aDebugPath, aBreakpadID)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [promise](const SymbolTable& aSymbolTable) {
            AutoJSAPI jsapi;
            if (NS_WARN_IF(!jsapi.Init(promise->GlobalJSObject()))) {
              // We're really hosed if we can't get a JS context for some
              // reason.
              promise->MaybeReject(NS_ERROR_DOM_UNKNOWN_ERR);
              return;
            }

            JSContext* cx = jsapi.cx();

            JS::RootedObject addrsArray(
                cx, dom::Uint32Array::Create(cx, aSymbolTable.mAddrs.Length(),
                                             aSymbolTable.mAddrs.Elements()));
            JS::RootedObject indexArray(
                cx, dom::Uint32Array::Create(cx, aSymbolTable.mIndex.Length(),
                                             aSymbolTable.mIndex.Elements()));
            JS::RootedObject bufferArray(
                cx, dom::Uint8Array::Create(cx, aSymbolTable.mBuffer.Length(),
                                            aSymbolTable.mBuffer.Elements()));

            if (addrsArray && indexArray && bufferArray) {
              JS::RootedObject tuple(cx, JS_NewArrayObject(cx, 3));
              JS_SetElement(cx, tuple, 0, addrsArray);
              JS_SetElement(cx, tuple, 1, indexArray);
              JS_SetElement(cx, tuple, 2, bufferArray);
              promise->MaybeResolve(tuple);
            } else {
              promise->MaybeReject(NS_ERROR_FAILURE);
            }
          },
          [promise](nsresult aRv) { promise->MaybeReject(aRv); });

  promise.forget(aPromise);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetElapsedTime(double* aElapsedTime) {
  *aElapsedTime = profiler_time();
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::IsActive(bool* aIsActive) {
  *aIsActive = profiler_is_active();
  return NS_OK;
}

static void GetArrayOfStringsForFeatures(uint32_t aFeatures, uint32_t* aCount,
                                         char*** aFeatureList) {
#define COUNT_IF_SET(n_, str_, Name_)           \
  if (ProfilerFeature::Has##Name_(aFeatures)) { \
    len++;                                      \
  }

  // Count the number of features in use.
  uint32_t len = 0;
  PROFILER_FOR_EACH_FEATURE(COUNT_IF_SET)

#undef COUNT_IF_SET

  auto featureList = static_cast<char**>(moz_xmalloc(len * sizeof(char*)));

#define DUP_IF_SET(n_, str_, Name_)             \
  if (ProfilerFeature::Has##Name_(aFeatures)) { \
    featureList[i] = moz_xstrdup(str_);         \
    i++;                                        \
  }

  // Insert the strings for the features in use.
  size_t i = 0;
  PROFILER_FOR_EACH_FEATURE(DUP_IF_SET)

#undef DUP_IF_SET

  *aFeatureList = featureList;
  *aCount = len;
}

NS_IMETHODIMP
nsProfiler::GetFeatures(uint32_t* aCount, char*** aFeatureList) {
  uint32_t features = profiler_get_available_features();
  GetArrayOfStringsForFeatures(features, aCount, aFeatureList);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetAllFeatures(uint32_t* aCount, char*** aFeatureList) {
  GetArrayOfStringsForFeatures((uint32_t)-1, aCount, aFeatureList);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetBufferInfo(uint32_t* aCurrentPosition, uint32_t* aTotalSize,
                          uint32_t* aGeneration) {
  MOZ_ASSERT(aCurrentPosition);
  MOZ_ASSERT(aTotalSize);
  MOZ_ASSERT(aGeneration);
  Maybe<ProfilerBufferInfo> info = profiler_get_buffer_info();
  if (info) {
    *aCurrentPosition = info->mRangeEnd % info->mEntryCount;
    *aTotalSize = info->mEntryCount;
    *aGeneration = info->mRangeEnd / info->mEntryCount;
  } else {
    *aCurrentPosition = 0;
    *aTotalSize = 0;
    *aGeneration = 0;
  }
  return NS_OK;
}

void nsProfiler::GatheredOOPProfile(const nsACString& aProfile) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!profiler_is_active()) {
    return;
  }

  if (!mGathering) {
    // If we're not actively gathering, then we don't actually care that we
    // gathered a profile here. This can happen for processes that exit while
    // profiling.
    return;
  }

  MOZ_RELEASE_ASSERT(mWriter.isSome(),
                     "Should always have a writer if mGathering is true");

  if (!aProfile.IsEmpty()) {
    mWriter->Splice(PromiseFlatCString(aProfile).get());
  }

  mPendingProfiles--;

  if (mPendingProfiles == 0) {
    // We've got all of the async profiles now. Let's
    // finish off the profile and resolve the Promise.
    FinishGathering();
  }
}

void nsProfiler::ReceiveShutdownProfile(const nsCString& aProfile) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  Maybe<ProfilerBufferInfo> bufferInfo = profiler_get_buffer_info();
  if (!bufferInfo) {
    // The profiler is not running. Discard the profile.
    return;
  }

  // Append the exit profile to mExitProfiles so that it can be picked up when
  // a profile is requested.
  uint64_t bufferPosition = bufferInfo->mRangeEnd;
  mExitProfiles.AppendElement(ExitProfile{aProfile, bufferPosition});

  // This is a good time to clear out exit profiles whose time ranges have no
  // overlap with this process's profile buffer contents any more.
  ClearExpiredExitProfiles();
}

RefPtr<nsProfiler::GatheringPromise> nsProfiler::StartGathering(
    double aSinceTime) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (mGathering) {
    // If we're already gathering, return a rejected promise - this isn't
    // going to end well.
    return GatheringPromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE, __func__);
  }

  mGathering = true;

  // Request profiles from the other processes. This will trigger asynchronous
  // calls to ProfileGatherer::GatheredOOPProfile as the profiles arrive.
  //
  // Do this before the call to profiler_stream_json_for_this_process() because
  // that call is slow and we want to let the other processes grab their
  // profiles as soon as possible.
  nsTArray<RefPtr<ProfilerParent::SingleProcessProfilePromise>> profiles =
      ProfilerParent::GatherProfiles();

  mWriter.emplace();

  // Start building up the JSON result and grab the profile from this process.
  mWriter->Start();
  if (!profiler_stream_json_for_this_process(*mWriter, aSinceTime,
                                             /* aIsShuttingDown */ false)) {
    // The profiler is inactive. This either means that it was inactive even
    // at the time that ProfileGatherer::Start() was called, or that it was
    // stopped on a different thread since that call. Either way, we need to
    // reject the promise and stop gathering.
    return GatheringPromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE, __func__);
  }

  mWriter->StartArrayProperty("processes");

  ClearExpiredExitProfiles();

  // If we have any process exit profiles, add them immediately.
  for (auto& exitProfile : mExitProfiles) {
    if (!exitProfile.mJSON.IsEmpty()) {
      mWriter->Splice(exitProfile.mJSON.get());
    }
  }

  mPromiseHolder.emplace();
  RefPtr<GatheringPromise> promise = mPromiseHolder->Ensure(__func__);

  // Keep the array property "processes" and the root object in mWriter open
  // until FinishGathering() is called. As profiles from the other processes
  // come in, they will be inserted and end up in the right spot.
  // FinishGathering() will close the array and the root object.

  mPendingProfiles = profiles.Length();
  RefPtr<nsProfiler> self = this;
  for (auto profile : profiles) {
    profile->Then(GetMainThreadSerialEventTarget(), __func__,
                  [self](const mozilla::ipc::Shmem& aResult) {
                    const nsDependentCSubstring profileString(
                        aResult.get<char>(), aResult.Size<char>() - 1);
                    self->GatheredOOPProfile(profileString);
                  },
                  [self](ipc::ResponseRejectReason aReason) {
                    self->GatheredOOPProfile(NS_LITERAL_CSTRING(""));
                  });
  }
  if (!mPendingProfiles) {
    FinishGathering();
  }

  return promise;
}

RefPtr<nsProfiler::SymbolTablePromise> nsProfiler::GetSymbolTableMozPromise(
    const nsACString& aDebugPath, const nsACString& aBreakpadID) {
  MozPromiseHolder<SymbolTablePromise> promiseHolder;
  RefPtr<SymbolTablePromise> promise = promiseHolder.Ensure(__func__);

  if (!mSymbolTableThread) {
    nsresult rv = NS_NewNamedThread("ProfSymbolTable",
                                    getter_AddRefs(mSymbolTableThread));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      promiseHolder.Reject(NS_ERROR_FAILURE, __func__);
      return promise;
    }
  }

  mSymbolTableThread->Dispatch(NS_NewRunnableFunction(
      "nsProfiler::GetSymbolTableMozPromise runnable on ProfSymbolTable thread",
      [promiseHolder = std::move(promiseHolder),
       debugPath = nsCString(aDebugPath),
       breakpadID = nsCString(aBreakpadID)]() mutable {
        AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING("profiler_get_symbol_table",
                                              OTHER, debugPath);
        SymbolTable symbolTable;
        bool succeeded = profiler_get_symbol_table(
            debugPath.get(), breakpadID.get(), &symbolTable);
        SystemGroup::Dispatch(
            TaskCategory::Other,
            NS_NewRunnableFunction(
                "nsProfiler::GetSymbolTableMozPromise result on main thread",
                [promiseHolder = std::move(promiseHolder),
                 symbolTable = std::move(symbolTable), succeeded]() mutable {
                  if (succeeded) {
                    promiseHolder.Resolve(std::move(symbolTable), __func__);
                  } else {
                    promiseHolder.Reject(NS_ERROR_FAILURE, __func__);
                  }
                }));
      }));

  return promise;
}

void nsProfiler::FinishGathering() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(mWriter.isSome());
  MOZ_RELEASE_ASSERT(mPromiseHolder.isSome());

  // Close the "processes" array property.
  mWriter->EndArray();

  // Close the root object of the generated JSON.
  mWriter->End();

  UniquePtr<char[]> buf = mWriter->WriteFunc()->CopyData();
  nsCString result(buf.get());
  mPromiseHolder->Resolve(result, __func__);

  ResetGathering();
}

void nsProfiler::ResetGathering() {
  mPromiseHolder.reset();
  mPendingProfiles = 0;
  mGathering = false;
  mWriter.reset();
}

void nsProfiler::ClearExpiredExitProfiles() {
  Maybe<ProfilerBufferInfo> bufferInfo = profiler_get_buffer_info();
  MOZ_RELEASE_ASSERT(bufferInfo,
                     "the profiler should be running at the moment");
  uint64_t bufferRangeStart = bufferInfo->mRangeStart;
  // Discard any exit profiles that were gathered before bufferRangeStart.
  mExitProfiles.RemoveElementsBy([bufferRangeStart](ExitProfile& aExitProfile) {
    return aExitProfile.mBufferPositionAtGatherTime < bufferRangeStart;
  });
}
