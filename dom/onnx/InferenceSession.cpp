/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/dom/InferenceSession.h"
#include <prlink.h>
#include "mozilla/ScopeExit.h"
#include "mozilla/Logging.h"
#include <thread>
#include "nsXPCOMPrivate.h"
#include "mozilla/FileUtils.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/dom/ContentChild.h"
#include "ErrorList.h"
#include "GeckoProfiler.h"
#include "fmt/format.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ONNXBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsString.h"
#include "onnxruntime_c_api.h"
#include "mozilla/dom/Tensor.h"
#include "mozilla/Attributes.h"
mozilla::LazyLogModule gONNXLog("ONNXNative");
#define LOGV(fmt, ...) \
  MOZ_LOG_FMT(gONNXLog, LogLevel::Verbose, fmt, ##__VA_ARGS__)
#define LOGD(fmt, ...) \
  MOZ_LOG_FMT(gONNXLog, LogLevel::Debug, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) \
  MOZ_LOG_FMT(gONNXLog, LogLevel::Error, fmt, ##__VA_ARGS__)

namespace mozilla::dom {

// Initialized when the first InferenceSession is initialized,
// valid until the shutdown of the inference process.
static OrtEnv* sEnv = nullptr;
static OrtApi* sAPI = nullptr;

class AutoOrtStatus {
 public:
  MOZ_IMPLICIT AutoOrtStatus(OrtStatus* aStatus = nullptr) : mStatus(aStatus) {
    MOZ_ASSERT(sAPI);
  }
  ~AutoOrtStatus() {
    if (mStatus) {
      sAPI->ReleaseStatus(mStatus);
    }
  }
  explicit operator bool() const { return !!mStatus; }
  const char* Message() const { return sAPI->GetErrorMessage(mStatus); }
  OrtStatus* mStatus;
};

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(InferenceSession);

NS_IMPL_CYCLE_COLLECTING_ADDREF(InferenceSession)
NS_IMPL_CYCLE_COLLECTING_RELEASE(InferenceSession)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(InferenceSession)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

#define DYLIB_PATH "onnxruntime"

OrtSessionOptions* ToOrtSessionOption(
    const InferenceSessionSessionOptions& aOptions) {
  OrtSessionOptions* sessionOptions = nullptr;
  AutoOrtStatus status = sAPI->CreateSessionOptions(&sessionOptions);
  if (status) {
    LOGD("CreateSessionOptions error: {}", status.Message());
    return nullptr;
  }
#define SET_BOOL_ON_SESSION(x)                                       \
  do {                                                               \
    if (aOptions.mEnable##x) {                                       \
      status = sAPI->Enable##x(sessionOptions);                     \
    } else {                                                         \
      status = sAPI->Disable##x(sessionOptions);                    \
    }                                                                \
    if (status) {                                                    \
      LOGE("Setter {} (val: {}) error: {}", #x, aOptions.mEnable##x, \
           status.Message());                                        \
      return nullptr;                                                \
    }                                                                \
  } while (0)

  LOGD("CpuMemArena: {}", aOptions.mEnableCpuMemArena);
  SET_BOOL_ON_SESSION(CpuMemArena);
  LOGD("MemPattern: {}", aOptions.mEnableMemPattern);
  SET_BOOL_ON_SESSION(MemPattern);

#define CALL_API(x, ...)                                           \
  do {                                                             \
    status = sAPI->x(sessionOptions, __VA_ARGS__);                \
    if (status) {                                                  \
      LOGD("SetSessionExecutionMode error: {}", status.Message()); \
      return nullptr;                                              \
    }                                                              \
  } while (0);

  LOGD("Session execution mode: {}", aOptions.mExecutionMode);
  CALL_API(SetSessionExecutionMode,
           aOptions.mExecutionMode.EqualsASCII("parallel")
               ? ExecutionMode::ORT_PARALLEL
               : ExecutionMode::ORT_SEQUENTIAL);

  LOGD("Inter op num threads: {}", aOptions.mInterOpNumThreads);
  CALL_API(SetInterOpNumThreads, aOptions.mInterOpNumThreads);
  LOGD("Inter op num threads: {}", aOptions.mIntraOpNumThreads);
  CALL_API(SetInterOpNumThreads, aOptions.mIntraOpNumThreads);
  CALL_API(SetSessionLogId, aOptions.mLogId.get());
  CALL_API(SetSessionLogSeverityLevel, aOptions.mLogSeverityLevel);
  CALL_API(SetSessionLogVerbosityLevel, aOptions.mLogVerbosityLevel);
  PathString path;
  #ifdef XP_WIN
  path = NS_ConvertUTF8toUTF16(aOptions.mOptimizedModelFilePath.get());

  #else
  path = aOptions.mOptimizedModelFilePath.get();
  #endif
  CALL_API(SetOptimizedModelFilePath, path.get());
  GraphOptimizationLevel level = ORT_ENABLE_BASIC;
  LOGD("Graph optimization level: {}", aOptions.mGraphOptimizationLevel);
  if (aOptions.mGraphOptimizationLevel.EqualsASCII("all")) {
    level = ORT_ENABLE_ALL;
  } else if (aOptions.mGraphOptimizationLevel.EqualsASCII("basic")) {
    level = ORT_ENABLE_BASIC;
  } else if (aOptions.mGraphOptimizationLevel.EqualsASCII("extended")) {
    level = ORT_ENABLE_EXTENDED;
  } else if (aOptions.mGraphOptimizationLevel.EqualsASCII("all")) {
    level = ORT_ENABLE_ALL;
  }
  CALL_API(SetSessionGraphOptimizationLevel, level);

  if (aOptions.mFreeDimensionOverrides.WasPassed()) {
    for (const auto& rec : aOptions.mFreeDimensionOverrides.Value().Entries()) {
      LOGD("Adding free dimension override for key: {}, value: {}", rec.mKey,
           rec.mValue);
      CALL_API(AddFreeDimensionOverride, rec.mKey.get(), rec.mValue);
    }
  }

  return sessionOptions;
}  // namespace mozilla::dom

OrtApi* GetOrtAPI() {
#ifdef XP_WIN
  PathString path = GetLibraryFilePathname(LXUL_DLL, (PRFuncPtr)&GetOrtAPI);
#else
  PathString path = GetLibraryFilePathname(XUL_DLL, (PRFuncPtr)&GetOrtAPI);
#endif
  if (path.IsEmpty()) {
    LOGE("Could not locate XUL library when loading onnxruntime");
    return nullptr;
  }
  nsCOMPtr<nsIFile> libFile;
  if (NS_FAILED(NS_NewPathStringLocalFile(path, getter_AddRefs(libFile)))) {
    LOGE("Could not get path string for local file when loading onnxruntime");
    return nullptr;
  }

  if (NS_FAILED(libFile->SetNativeLeafName(
          MOZ_DLL_PREFIX "onnxruntime" MOZ_DLL_SUFFIX ""_ns))) {
    LOGE("SetNativeLeavName error when loading onnxruntime");
    return nullptr;
  }
  PRLibSpec lspec;
  PathString nativePath = libFile->NativePath();
#ifdef XP_WIN
  lspec.type = PR_LibSpec_PathnameU;
  lspec.value.pathname_u = nativePath.get();
#else
  lspec.type = PR_LibSpec_Pathname;
  lspec.value.pathname = nativePath.get();
#endif
#ifdef MOZ_WIDGET_ANDROID
  PRLibrary* handle = PR_LoadLibraryWithFlags(lspec, PR_LD_NOW | PR_LD_GLOBAL);
#else
  PRLibrary* handle = PR_LoadLibraryWithFlags(lspec, PR_LD_NOW | PR_LD_LOCAL);
#endif
  if (!handle) {
    PRErrorCode code = PR_GetError();
    const char* msg = PR_ErrorToString(code, PR_LANGUAGE_I_DEFAULT);
    LOGE("Couldn't load onnxruntime shared library ({:x}: {})", PR_GetOSError(),
         msg);
    return nullptr;
  }

  using OrtApiBaseFn = const OrtApiBase* (*)();
  auto ortGetApiBaseFnPtr =
      reinterpret_cast<OrtApiBaseFn>(PR_FindSymbol(handle, "OrtGetApiBase"));
  if (!ortGetApiBaseFnPtr) {
    LOGE("Couldn't fetch symbol OrgGetApiBase");
    PR_UnloadLibrary(handle);
    return nullptr;
  }
  const OrtApiBase* apiBase = ortGetApiBaseFnPtr();
  OrtApi* ortAPI = const_cast<OrtApi*>(apiBase->GetApi(ORT_API_VERSION));
  if (!ortAPI) {
    LOGE("Couldn't get ahold of the OrtApi pointer");
    PR_UnloadLibrary(handle);
    return nullptr;
  }

  return ortAPI;
}

bool InferenceSession::InInferenceProcess(JSContext*, JSObject*) {
  if (!ContentChild::GetSingleton()) {
    return false;
  }
  return ContentChild::GetSingleton()->GetRemoteType().Equals(
      INFERENCE_REMOTE_TYPE);
}

nsCString InferenceSessionSessionOptionsToString(
    const InferenceSessionSessionOptions& aOptions) {
  return nsFmtCString(
      FMT_STRING("EnableCpuMemArena: {}, "
                 "EnableGraphCapture: {}, "
                 "EnableMemPattern: {}, "
                 "EnableProfiling: {}, "
                 "ExecutionMode: {}, "
                 "ExecutionProviders: {}, "
                 "Extra: {}, "
                 "FreeDimensionOverrides: {}, "
                 "GraphOptimizationLevel: {}, "
                 "InterOpNumThreads: {}, "
                 "IntraOpNumThreads: {}, "
                 "LogId: {}, "
                 "LogSeverityLevel: {}, "
                 "LogVerbosityLevel: {}, "
                 "OptimizedModelFilePath: {}, "
                 "PreferredOutputLocation: {}, "
                 "ProfileFilePrefix: {}"),
      aOptions.mEnableCpuMemArena, aOptions.mEnableGraphCapture,
      aOptions.mEnableMemPattern, aOptions.mEnableProfiling,
      aOptions.mExecutionMode,
      aOptions.mExecutionProviders.WasPassed() ? "<passed>" : "<not passed>",
      aOptions.mExtra.WasPassed() ? "<passed>" : "<not passed>",
      aOptions.mFreeDimensionOverrides.WasPassed() ? "<passed>"
                                                   : "<not passed>",
      aOptions.mGraphOptimizationLevel, aOptions.mInterOpNumThreads,
      aOptions.mIntraOpNumThreads, aOptions.mLogId, aOptions.mLogSeverityLevel,
      aOptions.mLogVerbosityLevel, aOptions.mOptimizedModelFilePath,
      aOptions.mPreferredOutputLocation.WasPassed() ? "<passed>"
                                                    : "<not passed>",
      aOptions.mProfileFilePrefix);
}

OrtCustomThreadHandle WrapProfilerRegister(void* options, void (*func)(void*),
                                           void* param) {
  // We don't use options for now
  MOZ_ASSERT(!options);
  auto wrapperFunc = [func](void* param) {
    char stacktop;
    profiler_register_thread("onnx_worker", &stacktop);
    LOGD("Starting thread");
    (static_cast<OrtThreadWorkerFn>(func))(param);
  };

  auto* t = new std::thread(wrapperFunc, param);

  return reinterpret_cast<OrtCustomThreadHandle>(t);
}

void WrapProfilerUnregister(OrtCustomThreadHandle thread) {
  LOGD("Joining thread");
  std::thread* t = (std::thread*)thread;
  t->join();
  delete t;
}

RefPtr<Promise> InferenceSession::Create(
    GlobalObject& aGlobal, const UTF8StringOrUint8Array& aUriOrBuffer,
    const InferenceSessionSessionOptions& aOptions, ErrorResult& aRv) {
  LOGD("{}", __PRETTY_FUNCTION__);
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<Promise> p = Promise::Create(global, aRv);
  RefPtr<InferenceSession> session = new InferenceSession(aGlobal);
  session->Init(p, aUriOrBuffer, aOptions);
  return p;
}

void InferenceSession::Init(const RefPtr<Promise>& aPromise,
                            const UTF8StringOrUint8Array& aUriOrBuffer,
                            const InferenceSessionSessionOptions& aOptions) {
  LOGD("InferenceSession::Init called with a {}",
       aUriOrBuffer.IsUTF8String() ? "string" : "buffer");

  if (!sEnv) {
    sAPI = GetOrtAPI();
    if (!sAPI) {
      LOGD("Couldn't get ahold of ORT API");
      aPromise->MaybeReject(NS_ERROR_FAILURE);
      return;
    }
    OrtThreadingOptions* threadingOptions;

    AutoOrtStatus status = sAPI->CreateThreadingOptions(&threadingOptions);
    if (status) {
      LOGD("CreateThreadingOptions error");
      aPromise->MaybeRejectWithUndefined();
      return;
    }
    status = sAPI->SetGlobalCustomCreateThreadFn(threadingOptions,
                                                 WrapProfilerRegister);
    if (status) {
      LOGD("SetGlobalCustomCreateThreadFn error");
      aPromise->MaybeRejectWithUndefined();
      return;
    }

    status = sAPI->SetGlobalCustomJoinThreadFn(threadingOptions,
                                               WrapProfilerUnregister);
    if (status) {
      LOGD("SetGlobalCustomJoinThreadFn error");
      aPromise->MaybeRejectWithUndefined();
      return;
    }

    status = sAPI->SetGlobalInterOpNumThreads(
        threadingOptions, AssertedCast<int>(aOptions.mInterOpNumThreads));
    if (status) {
      LOGD("SetGlobalInterOpNumThreads error");
      aPromise->MaybeRejectWithUndefined();
      return;
    }

    status = sAPI->SetGlobalIntraOpNumThreads(
        threadingOptions, AssertedCast<int>(aOptions.mIntraOpNumThreads));
    if (status) {
      LOGD("SetGlobalIntraOpNumThreads error");
      aPromise->MaybeRejectWithUndefined();
      return;
    }

    status = sAPI->SetGlobalDenormalAsZero(threadingOptions);
    if (status) {
      LOGD("SetGlobalDenormalsAreZero error");
      aPromise->MaybeRejectWithUndefined();
      return;
    }

    status = sAPI->SetGlobalSpinControl(threadingOptions, 0);
    if (status) {
      LOGD("SetGlobalSpinControl error");
      aPromise->MaybeRejectWithUndefined();
      return;
    }

    status = sAPI->CreateEnvWithGlobalThreadPools(
        ORT_LOGGING_LEVEL_FATAL, "my_env", threadingOptions, &sEnv);
    if (status) {
      LOGD("CreateEnv error: {}", status.Message());
      MOZ_CRASH("Init CreateEnv");
    }
    LOGD("CreateEnv OK");
  }

  mOptions = ToOrtSessionOption(aOptions);
  AutoOrtStatus status = sAPI->DisablePerSessionThreads(mOptions);
  if (status) {
    LOGD("DisablePerSessionThreads error: {}", status.Message());
  }

  OrtSession* session = nullptr;
  if (aUriOrBuffer.IsUTF8String()) {
    LOGE("Passing a URI to a model isn't implemented, pass the bytes directly");
    aPromise->MaybeRejectWithNotSupportedError("Not implemented");
    return;
  }
  aUriOrBuffer.GetAsUint8Array().ProcessFixedData(
      [&](const Span<uint8_t>& aData) {
        AUTO_PROFILER_MARKER_UNTYPED("CreateSessionFromArray", ML_SETUP, {});
        status = sAPI->CreateSessionFromArray(
            sEnv, aData.data(), aData.Length(), mOptions, &session);
      });
  if (status) {
    LOGD("CreateSession error: {}", status.Message());
    MOZ_CRASH("CreateSession error");
  }
  LOGD("Successfully created ONNX Runtime session.");
  mSession = session;
  aPromise->MaybeResolve(this);
}

nsCString FeedsToString(
    const Record<nsCString, OwningNonNull<Tensor>>& aFeeds) {
  nsCString rv;
  for (const auto& input : aFeeds.Entries()) {
    rv.AppendFmt("[{}: {}],", input.mKey, input.mValue->ToString().get());
  }
  return rv;
}

already_AddRefed<Promise> InferenceSession::Run(
    const Record<nsCString, OwningNonNull<Tensor>>& feeds,
    const InferenceSessionRunOptions& options, ErrorResult& aRv) {
  LOGD("{} {}", __PRETTY_FUNCTION__, fmt::ptr(this));
  RefPtr<Promise> p = Promise::Create(GetParentObject(), aRv);

  if (!mSession) {
    LOGD("runInference: session pointer is null.");
  }
  if (!sAPI || !sEnv) {
    LOGD("Need API {} and Env {} here", fmt::ptr(sAPI), fmt::ptr(sEnv));
    MOZ_CRASH("In run");
    p->MaybeReject(NS_ERROR_UNEXPECTED);
    return p.forget();
  }

  OrtMemoryInfo* memoryInfo = nullptr;
  auto guard = MakeScopeExit([&] { sAPI->ReleaseMemoryInfo(memoryInfo); });
  AutoOrtStatus status = sAPI->CreateCpuMemoryInfo(
      OrtArenaAllocator, OrtMemTypeDefault, &memoryInfo);
  if (status) {
    LOGD("CreateCpuMemoryInfo failed: {}", status.Message());
    p->MaybeReject(NS_ERROR_UNEXPECTED);
    return p.forget();
  }

  LOGD("Inputs:");
  nsTArray<OrtValue*> inputValues;
  auto scope = MakeScopeExit([&] {
    for (auto& v : inputValues) {
      sAPI->ReleaseValue(v);
    }
  });
  for (const auto& input : feeds.Entries()) {
    OrtValue* inputOrt = nullptr;
    const auto& val = input.mValue;
    AutoTArray<int64_t, 16> dims64;
    for (uint32_t i = 0; i < val->DimsSize(); i++) {
      dims64.AppendElement(val->Dims()[i]);
    }
    LOGD("{}: {}", input.mKey.get(),
         val->ToString().get());
    AUTO_PROFILER_MARKER_FMT("CreateTensorWithDataAsOrtValue", ML_INFERENCE, {},
                             "{}", input.mKey.get());
    status = sAPI->CreateTensorWithDataAsOrtValue(
        memoryInfo, val->Data(), val->Size(), dims64.Elements(),
        val->DimsSize(), val->Type(), &inputOrt);
    if (status) {
      LOGD("CreateTensorWithDataAsOrtValue for input_ids {} failed: {}",
           input.mKey, status.Message());
      p->MaybeReject(NS_ERROR_UNEXPECTED);
      return p.forget();
    }

    inputValues.AppendElement(inputOrt);
  }

  nsTArray<nsCString> inputNames;
  nsTArray<const char*> inputNamesPtrs;
  GetNames(inputNames, NameDirection::Input);
  for (const auto& name : inputNames) {
    inputNamesPtrs.AppendElement(name.get());
  }
  nsTArray<nsCString> outputNames;
  nsTArray<const char*> outputNamesPtrs;
  GetNames(outputNames, NameDirection::Output);
  LOGD("Outputs names:");
  for (const auto& name : outputNames) {
    LOGD("- {}", name.get());
    outputNamesPtrs.AppendElement(name.get());
  }
  nsTArray<OrtValue*> outputs;
  outputs.SetLength(outputNames.Length());
  for (uint32_t i = 0; i < outputNames.Length(); i++) {
    outputs[i] = nullptr;
  }
  OrtValue** ptr = outputs.Elements();

  {
    AUTO_PROFILER_MARKER_UNTYPED("Ort::Run", ML_INFERENCE, {});
    status = sAPI->Run(mSession,
                       nullptr,  // Run options
                       inputNamesPtrs.Elements(), inputValues.Elements(),
                       inputNamesPtrs.Length(), outputNamesPtrs.Elements(),
                       outputNamesPtrs.Length(), ptr);
  }
  if (status) {
    LOGD("Session Run failed: {}", status.Message());
    p->MaybeReject(NS_ERROR_UNEXPECTED);
    return p.forget();
  }

  Record<nsCString, OwningNonNull<Tensor>> rv;
  for (size_t i = 0; i < outputs.Length(); i++) {
    TimeStamp start = TimeStamp::Now();
    // outputData has the same lifetime as output[i]. For now, the actual data
    // is copied into the Tensor object below. This copy will be removed in the
    // future.
    uint8_t* outputData = nullptr;
    status = sAPI->GetTensorMutableData(outputs[i], (void**)&outputData);
    if (status) {
      LOGD("GetTensorMutableData failed: {}", status.Message());
      p->MaybeReject(NS_ERROR_UNEXPECTED);
      return p.forget();
    }

    OrtTypeInfo* typeInfo;
    status = sAPI->SessionGetOutputTypeInfo(mSession, i, &typeInfo);
    if (status) {
      LOGD("GetOutputTypeInfo failed: {}", status.Message());
      p->MaybeReject(NS_ERROR_UNEXPECTED);
      return p.forget();
    }

    OrtTensorTypeAndShapeInfo* typeAndShapeInfo;
    status = sAPI->GetTensorTypeAndShape(outputs[i], &typeAndShapeInfo);
    if (status) {
      LOGD("GetTensorTypeAndShape failed: {}", status.Message());
      p->MaybeReject(NS_ERROR_UNEXPECTED);
      return p.forget();
    }

    ONNXType type;
    status = sAPI->GetOnnxTypeFromTypeInfo(typeInfo, &type);
    if (status) {
      LOGD("GetOnnxTypeFromTypeInfo failed: {}", status.Message());
      p->MaybeReject(NS_ERROR_UNEXPECTED);
      return p.forget();
    }
    MOZ_ASSERT(type == ONNX_TYPE_TENSOR);

    ONNXTensorElementDataType outputTensorType;
    status =
        sAPI->GetTensorElementType(typeAndShapeInfo, &outputTensorType);
    if (status) {
      LOGD("GetTensorElementType failed: {}", status.Message());
      p->MaybeReject(NS_ERROR_UNEXPECTED);
      return p.forget();
    }

    size_t dimCount;
    status = sAPI->GetDimensionsCount(typeAndShapeInfo, &dimCount);
    if (status) {
      LOGD("GetDimensionsCount failed: {}", status.Message());
      p->MaybeReject(NS_ERROR_UNEXPECTED);
      return p.forget();
    }

    AutoTArray<int64_t, 16> dims;
    dims.SetLength(dimCount);
    status =
        sAPI->GetDimensions(typeAndShapeInfo, dims.Elements(), dimCount);

    size_t outputSize = 1;
    for (size_t d = 0; d < dimCount; ++d) {
      outputSize *= dims[d];
    }

    // TODO skip this copy by using CreateTensorWithDataAsOrtValue
    nsTArray<uint8_t> output;
    output.AppendElements(
        outputData, outputSize * Tensor::DataTypeSize(outputTensorType));
    GlobalObject global(mCtx, GetParentObject()->GetGlobalJSObject());
    auto outputTensor = MakeRefPtr<Tensor>(global, outputTensorType,
                                           std::move(output), std::move(dims));
    AUTO_PROFILER_MARKER_FMT(
        "Output tensor", ML_INFERENCE,
        MarkerOptions(MarkerTiming::IntervalUntilNowFrom(start)), "{}: {}",
        outputNames[i], outputTensor->ToString().get());

    sAPI->ReleaseTensorTypeAndShapeInfo(typeAndShapeInfo);

    auto elem = rv.Entries().AppendElement();
    elem->mKey = outputNames[i];
    elem->mValue = outputTensor;
  }

  p->MaybeResolve(rv);

  return p.forget();
}

void InferenceSession::Destroy() {
  LOGD("{} {}", __PRETTY_FUNCTION__, fmt::ptr(this));
  if (mSession) {
    sAPI->ReleaseSession(mSession);
  }
  if (mOptions) {
    sAPI->ReleaseSessionOptions(mOptions);
  }
}

already_AddRefed<Promise> InferenceSession::ReleaseSession() {
  LOGD("{} {}", __PRETTY_FUNCTION__, fmt::ptr(this));

  Destroy();
  RefPtr<Promise> p = Promise::CreateInfallible(mGlobal);
  p->MaybeResolveWithUndefined();
  return p.forget();
}

void InferenceSession::StartProfiling() {
  LOGD("{} {}", __PRETTY_FUNCTION__, fmt::ptr(this));
}

void InferenceSession::EndProfiling() {
  LOGD("{} {}", __PRETTY_FUNCTION__, fmt::ptr(this));
}

void InferenceSession::GetNames(nsTArray<nsCString>& aRetVal,
                                NameDirection aDirection) const {
  const char* NameDirection2String[2] = {"Input", "Output"};

  if (!mSession) {
    return;
  }
  size_t nameCount = 0;
  AutoOrtStatus status;
  if (aDirection == NameDirection::Input) {
    status = sAPI->SessionGetInputCount(mSession, &nameCount);
  } else {
    status = sAPI->SessionGetOutputCount(mSession, &nameCount);
  }
  if (status) {
    LOGD("SessionGet{}Count failed: ",
         NameDirection2String[static_cast<int>(aDirection)], status.Message());
    return;
  }

  OrtAllocator* allocator = nullptr;
  status = sAPI->GetAllocatorWithDefaultOptions(&allocator);
  if (status) {
    LOGD("GetAllocatorWithDefaultOptions failed: {}", status.Message());
    return;
  }
  aRetVal.SetCapacity(nameCount);
  for (size_t i = 0; i < nameCount; i++) {
    // Allocated by onnxruntiem, must be freed by AllocatorFree
    char* name = nullptr;

    if (aDirection == NameDirection::Input) {
      status = sAPI->SessionGetInputName(mSession, i, allocator, &name);
    } else {
      status = sAPI->SessionGetOutputName(mSession, i, allocator, &name);
    }
    if (status) {
      LOGD("SessionGet{}Name failed: ",
           NameDirection2String[static_cast<int>(aDirection)],
           status.Message());
      continue;
    }
    aRetVal.AppendElement(name);
    status = sAPI->AllocatorFree(allocator, name);
    if (status) {
      LOGD("AllocatorFree failed: ", status.Message());
      continue;
    }
  }
}

void InferenceSession::GetInputNames(nsTArray<nsCString>& aRetVal) const {
  LOGD("{} {}", __PRETTY_FUNCTION__, fmt::ptr(this));
  GetNames(aRetVal, NameDirection::Input);
  if (MOZ_LOG_TEST(gONNXLog, LogLevel::Debug)) {
    for (auto& name : aRetVal) {
      LOGD("- {}", name);
    }
  }
}

void InferenceSession::GetOutputNames(nsTArray<nsCString>& aRetVal) const {
  LOGD("{} {}", __PRETTY_FUNCTION__, fmt::ptr(this));
  GetNames(aRetVal, NameDirection::Output);
  if (MOZ_LOG_TEST(gONNXLog, LogLevel::Debug)) {
    for (auto& name : aRetVal) {
      LOGD("- {}", name);
    }
  }
}

JSObject* InferenceSession::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return InferenceSession_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
