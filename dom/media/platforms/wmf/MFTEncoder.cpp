/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFTEncoder.h"
#include "mozilla/Logging.h"
#include "mozilla/WindowsProcessMitigations.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/mscom/COMWrappers.h"
#include "mozilla/mscom/Utils.h"
#include "WMFUtils.h"
#include <comdef.h>

using Microsoft::WRL::ComPtr;

// Missing from MinGW.
#ifndef CODECAPI_AVEncAdaptiveMode
#  define STATIC_CODECAPI_AVEncAdaptiveMode \
    0x4419b185, 0xda1f, 0x4f53, 0xbc, 0x76, 0x9, 0x7d, 0xc, 0x1e, 0xfb, 0x1e
DEFINE_CODECAPI_GUID(AVEncAdaptiveMode, "4419b185-da1f-4f53-bc76-097d0c1efb1e",
                     0x4419b185, 0xda1f, 0x4f53, 0xbc, 0x76, 0x9, 0x7d, 0xc,
                     0x1e, 0xfb, 0x1e)
#  define CODECAPI_AVEncAdaptiveMode \
    DEFINE_CODECAPI_GUIDNAMED(AVEncAdaptiveMode)
#endif
#ifndef MF_E_NO_EVENTS_AVAILABLE
#  define MF_E_NO_EVENTS_AVAILABLE _HRESULT_TYPEDEF_(0xC00D3E80L)
#endif

#define MFT_ENC_LOGD(arg, ...)                        \
  MOZ_LOG(mozilla::sPEMLog, mozilla::LogLevel::Debug, \
          ("MFTEncoder(0x%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
#define MFT_ENC_LOGE(arg, ...)                        \
  MOZ_LOG(mozilla::sPEMLog, mozilla::LogLevel::Error, \
          ("MFTEncoder(0x%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
#define MFT_ENC_SLOGD(arg, ...)                       \
  MOZ_LOG(mozilla::sPEMLog, mozilla::LogLevel::Debug, \
          ("MFTEncoder::%s: " arg, __func__, ##__VA_ARGS__))
#define MFT_ENC_SLOGE(arg, ...)                       \
  MOZ_LOG(mozilla::sPEMLog, mozilla::LogLevel::Error, \
          ("MFTEncoder::%s: " arg, __func__, ##__VA_ARGS__))

#undef MFT_RETURN_IF_FAILED_IMPL
#define MFT_RETURN_IF_FAILED_IMPL(x, log_macro)                            \
  do {                                                                     \
    HRESULT rv = x;                                                        \
    if (MOZ_UNLIKELY(FAILED(rv))) {                                        \
      _com_error error(rv);                                                \
      log_macro("(" #x ") failed, rv=%lx(%ls)", rv, error.ErrorMessage()); \
      return rv;                                                           \
    }                                                                      \
  } while (false)

#undef MFT_RETURN_IF_FAILED
#define MFT_RETURN_IF_FAILED(x) MFT_RETURN_IF_FAILED_IMPL(x, MFT_ENC_LOGE)

#undef MFT_RETURN_IF_FAILED_S
#define MFT_RETURN_IF_FAILED_S(x) MFT_RETURN_IF_FAILED_IMPL(x, MFT_ENC_SLOGE)

#undef MFT_RETURN_VALUE_IF_FAILED_IMPL
#define MFT_RETURN_VALUE_IF_FAILED_IMPL(x, ret, log_macro)                 \
  do {                                                                     \
    HRESULT rv = x;                                                        \
    if (MOZ_UNLIKELY(FAILED(rv))) {                                        \
      _com_error error(rv);                                                \
      log_macro("(" #x ") failed, rv=%lx(%ls)", rv, error.ErrorMessage()); \
      return ret;                                                          \
    }                                                                      \
  } while (false)

#undef MFT_RETURN_VALUE_IF_FAILED
#define MFT_RETURN_VALUE_IF_FAILED(x, r) \
  MFT_RETURN_VALUE_IF_FAILED_IMPL(x, r, MFT_ENC_LOGE)

#undef MFT_RETURN_VALUE_IF_FAILED_S
#define MFT_RETURN_VALUE_IF_FAILED_S(x, r) \
  MFT_RETURN_VALUE_IF_FAILED_IMPL(x, r, MFT_ENC_SLOGE)

#undef MFT_RETURN_ERROR_IF_FAILED_IMPL
#define MFT_RETURN_ERROR_IF_FAILED_IMPL(x, log_macro)                      \
  do {                                                                     \
    HRESULT rv = x;                                                        \
    if (MOZ_UNLIKELY(FAILED(rv))) {                                        \
      _com_error error(rv);                                                \
      log_macro("(" #x ") failed, rv=%lx(%ls)", rv, error.ErrorMessage()); \
      return Err(rv);                                                      \
    }                                                                      \
  } while (false)

#undef MFT_RETURN_ERROR_IF_FAILED_S
#define MFT_RETURN_ERROR_IF_FAILED_S(x) \
  MFT_RETURN_ERROR_IF_FAILED_IMPL(x, MFT_ENC_SLOGE)

namespace mozilla {
extern LazyLogModule sPEMLog;

static const char* ErrorStr(HRESULT hr) {
  switch (hr) {
    case S_OK:
      return "OK";
    case MF_E_INVALIDMEDIATYPE:
      return "INVALIDMEDIATYPE";
    case MF_E_INVALIDSTREAMNUMBER:
      return "INVALIDSTREAMNUMBER";
    case MF_E_INVALIDTYPE:
      return "INVALIDTYPE";
    case MF_E_TRANSFORM_CANNOT_CHANGE_MEDIATYPE_WHILE_PROCESSING:
      return "TRANSFORM_PROCESSING";
    case MF_E_TRANSFORM_ASYNC_LOCKED:
      return "TRANSFORM_ASYNC_LOCKED";
    case MF_E_TRANSFORM_TYPE_NOT_SET:
      return "TRANSFORM_TYPE_NO_SET";
    case MF_E_UNSUPPORTED_D3D_TYPE:
      return "UNSUPPORTED_D3D_TYPE";
    case E_INVALIDARG:
      return "INVALIDARG";
    case MF_E_NO_SAMPLE_DURATION:
      return "NO_SAMPLE_DURATION";
    case MF_E_NO_SAMPLE_TIMESTAMP:
      return "NO_SAMPLE_TIMESTAMP";
    case MF_E_NOTACCEPTING:
      return "NOTACCEPTING";
    case MF_E_ATTRIBUTENOTFOUND:
      return "NOTFOUND";
    case MF_E_BUFFERTOOSMALL:
      return "BUFFERTOOSMALL";
    case E_NOTIMPL:
      return "NOTIMPL";
    default:
      return "OTHER";
  }
}

static const char* MediaEventTypeStr(MediaEventType aType) {
#define ENUM_TO_STR(enumVal) \
  case enumVal:              \
    return #enumVal
  switch (aType) {
    ENUM_TO_STR(MEUnknown);
    ENUM_TO_STR(METransformUnknown);
    ENUM_TO_STR(METransformNeedInput);
    ENUM_TO_STR(METransformHaveOutput);
    ENUM_TO_STR(METransformDrainComplete);
    ENUM_TO_STR(METransformMarker);
    ENUM_TO_STR(METransformInputStreamStateChanged);
    default:
      break;
  }
  return "Unknown MediaEventType";

#undef ENUM_TO_STR
}

static nsCString ErrorMessage(HRESULT hr) {
  nsCString msg(ErrorStr(hr));
  _com_error err(hr);
  msg.AppendFmt(" ({})", NS_ConvertUTF16toUTF8(err.ErrorMessage()).get());
  return msg;
}

static const char* CodecStr(const GUID& aGUID) {
  if (IsEqualGUID(aGUID, MFVideoFormat_H264)) {
    return "H.264";
  } else if (IsEqualGUID(aGUID, MFVideoFormat_VP80)) {
    return "VP8";
  } else if (IsEqualGUID(aGUID, MFVideoFormat_VP90)) {
    return "VP9";
  } else {
    return "Unsupported codec";
  }
}

static HRESULT GetFriendlyName(IMFActivate* aAttributes, nsCString& aName) {
  UINT32 len = 0;
  MFT_RETURN_IF_FAILED_S(
      aAttributes->GetStringLength(MFT_FRIENDLY_NAME_Attribute, &len));
  if (len > 0) {
    ++len;  // '\0'.
    WCHAR name[len];
    if (SUCCEEDED(aAttributes->GetString(MFT_FRIENDLY_NAME_Attribute, name, len,
                                         nullptr))) {
      aName.Append(NS_ConvertUTF16toUTF8(name));
    }
  }

  if (aName.Length() == 0) {
    aName.Append("Unknown MFT");
  }

  return S_OK;
}

static Result<nsTArray<ComPtr<IMFActivate>>, HRESULT> EnumMFT(
    GUID aCategory, UINT32 aFlags, const MFT_REGISTER_TYPE_INFO* aInType,
    const MFT_REGISTER_TYPE_INFO* aOutType) {
  nsTArray<ComPtr<IMFActivate>> activates;

  IMFActivate** enumerated;
  UINT32 num = 0;
  MFT_RETURN_ERROR_IF_FAILED_S(
      wmf::MFTEnumEx(aCategory, aFlags, aInType, aOutType, &enumerated, &num));
  for (UINT32 i = 0; i < num; ++i) {
    activates.AppendElement(ComPtr<IMFActivate>(enumerated[i]));
    // MFTEnumEx increments the reference count for each IMFActivate; decrement
    // here so ComPtr manages the lifetime correctly
    enumerated[i]->Release();
  }
  if (enumerated) {
    mscom::wrapped::CoTaskMemFree(enumerated);
  }
  return activates;
}

static nsTArray<ComPtr<IMFActivate>> EnumEncoders(
    const GUID& aSubtype, const MFTEncoder::HWPreference aHWPreference) {
  MFT_REGISTER_TYPE_INFO inType = {.guidMajorType = MFMediaType_Video,
                                   .guidSubtype = MFVideoFormat_NV12};
  MFT_REGISTER_TYPE_INFO outType = {.guidMajorType = MFMediaType_Video,
                                    .guidSubtype = aSubtype};

  auto log = [&](const nsTArray<ComPtr<IMFActivate>>& aActivates,
                 const char* aPrefix) {
    MFT_ENC_SLOGD("Found %zu %s encoders for %s", aActivates.Length(), aPrefix,
                  CodecStr(aSubtype));
    for (const auto& activate : aActivates) {
      nsAutoCString name;
      GetFriendlyName(activate.Get(), name);
      MFT_ENC_SLOGD("%s\n", name.get());
    }
  };

  nsTArray<ComPtr<IMFActivate>> swActivates;
  nsTArray<ComPtr<IMFActivate>> hwActivates;

  if (aHWPreference != MFTEncoder::HWPreference::SoftwareOnly) {
    // Some HW encoders use DXGI API and crash when locked down.
    // TODO: move HW encoding out of content process (bug 1754531).
    if (IsWin32kLockedDown()) {
      MFT_ENC_SLOGD("Don't use HW encoder when win32k locked down.");
    } else {
      auto r = EnumMFT(MFT_CATEGORY_VIDEO_ENCODER,
                       MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER,
                       &inType, &outType);
      if (r.isErr()) {
        MFT_ENC_SLOGE("enumerate HW encoder for %s: error=%s",
                      CodecStr(aSubtype), ErrorMessage(r.unwrapErr()).get());
      } else {
        hwActivates.AppendElements(r.unwrap());
        log(hwActivates, "HW");
      }
    }
  }

  if (aHWPreference != MFTEncoder::HWPreference::HardwareOnly) {
    auto r = EnumMFT(MFT_CATEGORY_VIDEO_ENCODER,
                     MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_ASYNCMFT |
                         MFT_ENUM_FLAG_SORTANDFILTER,
                     &inType, &outType);
    if (r.isErr()) {
      MFT_ENC_SLOGE("enumerate SW encoder for %s: error=%s", CodecStr(aSubtype),
                    ErrorMessage(r.unwrapErr()).get());
    } else {
      swActivates.AppendElements(r.unwrap());
      log(swActivates, "SW");
    }
  }

  nsTArray<ComPtr<IMFActivate>> activates;

  switch (aHWPreference) {
    case MFTEncoder::HWPreference::HardwareOnly:
      return hwActivates;
    case MFTEncoder::HWPreference::SoftwareOnly:
      return swActivates;
    case MFTEncoder::HWPreference::PreferHardware:
      activates.AppendElements(std::move(hwActivates));
      activates.AppendElements(std::move(swActivates));
      break;
    case MFTEncoder::HWPreference::PreferSoftware:
      activates.AppendElements(std::move(swActivates));
      activates.AppendElements(std::move(hwActivates));
      break;
  }

  return activates;
}

static void PopulateEncoderInfo(const GUID& aSubtype,
                                nsTArray<MFTEncoder::Info>& aInfos) {
  nsTArray<ComPtr<IMFActivate>> activates =
      EnumEncoders(aSubtype, MFTEncoder::HWPreference::PreferHardware);
  for (const auto& activate : activates) {
    MFTEncoder::Info info = {.mSubtype = aSubtype};
    GetFriendlyName(activate.Get(), info.mName);
    aInfos.AppendElement(info);
    MFT_ENC_SLOGD("<ENC> [%s] %s\n", CodecStr(aSubtype), info.mName.Data());
  }
}

Maybe<MFTEncoder::Info> MFTEncoder::GetInfo(const GUID& aSubtype) {
  nsTArray<Info>& infos = Infos();

  for (auto i : infos) {
    if (IsEqualGUID(aSubtype, i.mSubtype)) {
      return Some(i);
    }
  }
  return Nothing();
}

nsCString MFTEncoder::GetFriendlyName(const GUID& aSubtype) {
  Maybe<Info> info = GetInfo(aSubtype);

  return info ? info.ref().mName : "???"_ns;
}

// Called only once by Infos().
nsTArray<MFTEncoder::Info> MFTEncoder::Enumerate() {
  nsTArray<Info> infos;

  if (!wmf::MediaFoundationInitializer::HasInitialized()) {
    MFT_ENC_SLOGE("cannot init Media Foundation");
    return infos;
  }

  PopulateEncoderInfo(MFVideoFormat_H264, infos);
  PopulateEncoderInfo(MFVideoFormat_VP90, infos);
  PopulateEncoderInfo(MFVideoFormat_VP80, infos);

  return infos;
}

nsTArray<MFTEncoder::Info>& MFTEncoder::Infos() {
  static nsTArray<Info> infos = Enumerate();
  return infos;
}

already_AddRefed<IMFActivate> MFTEncoder::CreateFactory(const GUID& aSubtype) {
  nsTArray<ComPtr<IMFActivate>> activates =
      EnumEncoders(aSubtype, mHWPreference);
  if (activates.Length() == 0) {
    return nullptr;
  }

  // Keep the first and throw out others, if there is any.
  RefPtr<IMFActivate> factory = activates[0].Get();
  activates[0] = nullptr;
  return factory.forget();
}

HRESULT MFTEncoder::Create(const GUID& aSubtype) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  MOZ_ASSERT(!mEncoder);

  RefPtr<IMFActivate> factory = CreateFactory(aSubtype);
  if (!factory) {
    MFT_ENC_LOGE("CreateFactory error");
    return E_FAIL;
  }

  // Create MFT via the activation object.
  RefPtr<IMFTransform> encoder;
  HRESULT hr = factory->ActivateObject(
      IID_PPV_ARGS(static_cast<IMFTransform**>(getter_AddRefs(encoder))));
  if (FAILED(hr)) {
    _com_error error(hr);
    MFT_ENC_LOGE("MFTEncoder::Create: error = 0x%lX, %ls", hr,
                 error.ErrorMessage());
    return hr;
  }

  RefPtr<ICodecAPI> config;
  // Avoid IID_PPV_ARGS() here for MingGW fails to declare UUID for ICodecAPI.
  hr = encoder->QueryInterface(IID_ICodecAPI, getter_AddRefs(config));
  if (FAILED(hr)) {
    MFT_ENC_LOGE("QueryInterface IID_ICodecAPI error");
    encoder = nullptr;
    factory->ShutdownObject();
    return hr;
  }

  mFactory = std::move(factory);
  mEncoder = std::move(encoder);
  mConfig = std::move(config);
  return S_OK;
}

HRESULT
MFTEncoder::Destroy() {
  if (!mEncoder) {
    return S_OK;
  }

  mEncoder = nullptr;
  mConfig = nullptr;
  // Release MFT resources via activation object.
  HRESULT hr = mFactory->ShutdownObject();
  mFactory = nullptr;

  return hr;
}

HRESULT
MFTEncoder::SetMediaTypes(IMFMediaType* aInputType, IMFMediaType* aOutputType) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  MOZ_ASSERT(aInputType && aOutputType);
  MOZ_ASSERT(mFactory);
  MOZ_ASSERT(mEncoder);

  AsyncMFTResult asyncMFT = AttemptEnableAsync();
  if (asyncMFT.isErr()) {
    HRESULT hr = asyncMFT.inspectErr();
    MFT_ENC_LOGE("AttemptEnableAsync error: %s", ErrorMessage(hr).get());
    return hr;
  }
  bool isAsync = asyncMFT.unwrap();
  MFT_ENC_LOGD("MFTEncoder is %s", isAsync ? "asynchronous" : "synchronous");

  MFT_RETURN_IF_FAILED(GetStreamIDs());

  // Always set encoder output type before input.
  MFT_RETURN_IF_FAILED(
      mEncoder->SetOutputType(mOutputStreamID, aOutputType, 0));

  if (MatchInputSubtype(aInputType) == GUID_NULL) {
    MFT_ENC_LOGE("Input type does not match encoder input subtype");
    return MF_E_INVALIDMEDIATYPE;
  }

  MFT_RETURN_IF_FAILED(mEncoder->SetInputType(mInputStreamID, aInputType, 0));

  MFT_RETURN_IF_FAILED(
      mEncoder->GetInputStreamInfo(mInputStreamID, &mInputStreamInfo));

  MFT_RETURN_IF_FAILED(
      mEncoder->GetOutputStreamInfo(mInputStreamID, &mOutputStreamInfo));

  mOutputStreamProvidesSample =
      IsFlagSet(mOutputStreamInfo.dwFlags, MFT_OUTPUT_STREAM_PROVIDES_SAMPLES);

  MFT_RETURN_IF_FAILED(SendMFTMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0));

  MFT_RETURN_IF_FAILED(SendMFTMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0));

  if (isAsync) {
    RefPtr<IMFMediaEventGenerator> source;
    MFT_RETURN_IF_FAILED(mEncoder->QueryInterface(IID_PPV_ARGS(
        static_cast<IMFMediaEventGenerator**>(getter_AddRefs(source)))));
    mEventSource.SetAsyncEventGenerator(source.forget());
  } else {
    mEventSource.InitSyncMFTEventQueue();
  }

  mNumNeedInput = 0;
  return S_OK;
}

// Async MFT won't work without unlocking. See
// https://docs.microsoft.com/en-us/windows/win32/medfound/asynchronous-mfts#unlocking-asynchronous-mfts
MFTEncoder::AsyncMFTResult MFTEncoder::AttemptEnableAsync() {
  ComPtr<IMFAttributes> attributes = nullptr;
  HRESULT hr = mEncoder->GetAttributes(&attributes);
  if (FAILED(hr)) {
    MFT_ENC_LOGE("Encoder->GetAttribute error");
    return AsyncMFTResult(hr);
  }

  // Retrieve `MF_TRANSFORM_ASYNC` using `MFGetAttributeUINT32` rather than
  // `attributes->GetUINT32`, since `MF_TRANSFORM_ASYNC` may not be present in
  // the attributes.
  bool async =
      MFGetAttributeUINT32(attributes.Get(), MF_TRANSFORM_ASYNC, FALSE) == TRUE;
  if (!async) {
    MFT_ENC_LOGD("Encoder is not async");
    return AsyncMFTResult(false);
  }

  hr = attributes->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK, TRUE);
  if (FAILED(hr)) {
    MFT_ENC_LOGE("SetUINT32 async unlock error");
    return AsyncMFTResult(hr);
  }

  return AsyncMFTResult(true);
}

HRESULT MFTEncoder::GetStreamIDs() {
  DWORD numIns;
  DWORD numOuts;
  MFT_RETURN_IF_FAILED(mEncoder->GetStreamCount(&numIns, &numOuts));
  MFT_ENC_LOGD("input stream count: %lu, output stream count: %lu", numIns,
               numOuts);
  if (numIns < 1 || numOuts < 1) {
    MFT_ENC_LOGE("stream count error");
    return MF_E_INVALIDSTREAMNUMBER;
  }

  DWORD inIDs[numIns];
  DWORD outIDs[numOuts];
  HRESULT hr = mEncoder->GetStreamIDs(numIns, inIDs, numOuts, outIDs);
  if (SUCCEEDED(hr)) {
    mInputStreamID = inIDs[0];
    mOutputStreamID = outIDs[0];
  } else if (hr == E_NOTIMPL) {
    mInputStreamID = 0;
    mOutputStreamID = 0;
  } else {
    MFT_ENC_LOGE("failed to get stream IDs: %s", ErrorMessage(hr).get());
    return hr;
  }
  return S_OK;
}

GUID MFTEncoder::MatchInputSubtype(IMFMediaType* aInputType) {
  MOZ_ASSERT(mEncoder);
  MOZ_ASSERT(aInputType);

  GUID desired = GUID_NULL;
  MFT_RETURN_VALUE_IF_FAILED(aInputType->GetGUID(MF_MT_SUBTYPE, &desired),
                             GUID_NULL);
  MOZ_ASSERT(desired != GUID_NULL);

  DWORD i = 0;
  IMFMediaType* inputType = nullptr;
  GUID preferred = GUID_NULL;
  while (true) {
    HRESULT hr = mEncoder->GetInputAvailableType(mInputStreamID, i, &inputType);
    if (hr == MF_E_NO_MORE_TYPES) {
      break;
    }
    if (FAILED(hr)) {
      MFT_ENC_LOGE("GetInputAvailableType error: %s", ErrorMessage(hr).get());
      return GUID_NULL;
    }

    GUID sub = GUID_NULL;
    MFT_RETURN_VALUE_IF_FAILED(inputType->GetGUID(MF_MT_SUBTYPE, &sub),
                               GUID_NULL);

    if (IsEqualGUID(desired, sub)) {
      preferred = desired;
      break;
    }
    ++i;
  }

  return IsEqualGUID(preferred, desired) ? preferred : GUID_NULL;
}

HRESULT
MFTEncoder::SendMFTMessage(MFT_MESSAGE_TYPE aMsg, ULONG_PTR aData) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  MOZ_ASSERT(mEncoder);

  return mEncoder->ProcessMessage(aMsg, aData);
}

HRESULT MFTEncoder::SetModes(const EncoderConfig& aConfig) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  MOZ_ASSERT(mConfig);

  VARIANT var;
  var.vt = VT_UI4;
  switch (aConfig.mBitrateMode) {
    case BitrateMode::Constant:
      var.ulVal = eAVEncCommonRateControlMode_CBR;
      break;
    case BitrateMode::Variable:
      if (aConfig.mCodec == CodecType::VP8 ||
          aConfig.mCodec == CodecType::VP9) {
        MFT_ENC_LOGE(
            "Overriding requested VRB bitrate mode, forcing CBR for VP8/VP9 "
            "encoding.");
        var.ulVal = eAVEncCommonRateControlMode_CBR;
      } else {
        var.ulVal = eAVEncCommonRateControlMode_PeakConstrainedVBR;
      }
      break;
  }
  MFT_RETURN_IF_FAILED(
      mConfig->SetValue(&CODECAPI_AVEncCommonRateControlMode, &var));

  if (aConfig.mBitrate) {
    var.ulVal = aConfig.mBitrate;
    MFT_RETURN_IF_FAILED(
        mConfig->SetValue(&CODECAPI_AVEncCommonMeanBitRate, &var));
  }

  switch (aConfig.mScalabilityMode) {
    case ScalabilityMode::None:
      var.ulVal = 1;
      break;
    case ScalabilityMode::L1T2:
      var.ulVal = 2;
      break;
    case ScalabilityMode::L1T3:
      var.ulVal = 3;
      break;
  }

  bool isIntel = false;  // TODO check this
  if (aConfig.mScalabilityMode != ScalabilityMode::None || isIntel) {
    MFT_RETURN_IF_FAILED(
        mConfig->SetValue(&CODECAPI_AVEncVideoTemporalLayerCount, &var));
  }

  if (SUCCEEDED(mConfig->IsModifiable(&CODECAPI_AVEncAdaptiveMode))) {
    var.ulVal = eAVEncAdaptiveMode_Resolution;
    MFT_RETURN_IF_FAILED(mConfig->SetValue(&CODECAPI_AVEncAdaptiveMode, &var));
  }

  if (SUCCEEDED(mConfig->IsModifiable(&CODECAPI_AVLowLatencyMode))) {
    var.vt = VT_BOOL;
    var.boolVal =
        aConfig.mUsage == Usage::Realtime ? VARIANT_TRUE : VARIANT_FALSE;
    MFT_RETURN_IF_FAILED(mConfig->SetValue(&CODECAPI_AVLowLatencyMode, &var));
  }

  return S_OK;
}

HRESULT
MFTEncoder::SetBitrate(UINT32 aBitsPerSec) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  MOZ_ASSERT(mConfig);

  VARIANT var = {.vt = VT_UI4, .ulVal = aBitsPerSec};
  return mConfig->SetValue(&CODECAPI_AVEncCommonMeanBitRate, &var);
}

static HRESULT CreateSample(RefPtr<IMFSample>* aOutSample, DWORD aSize,
                            DWORD aAlignment) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());

  RefPtr<IMFSample> sample;
  MFT_RETURN_IF_FAILED_S(wmf::MFCreateSample(getter_AddRefs(sample)));

  RefPtr<IMFMediaBuffer> buffer;
  MFT_RETURN_IF_FAILED_S(wmf::MFCreateAlignedMemoryBuffer(
      aSize, aAlignment, getter_AddRefs(buffer)));

  MFT_RETURN_IF_FAILED_S(sample->AddBuffer(buffer));

  *aOutSample = sample.forget();

  return S_OK;
}

HRESULT
MFTEncoder::CreateInputSample(RefPtr<IMFSample>* aSample, size_t aSize) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());

  return CreateSample(
      aSample, aSize,
      mInputStreamInfo.cbAlignment > 0 ? mInputStreamInfo.cbAlignment - 1 : 0);
}

HRESULT
MFTEncoder::PushInput(const InputSample& aInput) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  MOZ_ASSERT(mEncoder);

  mPendingInputs.push_back(aInput);
  if (mEventSource.IsSync() && mNumNeedInput == 0) {
    // To step 2 in
    // https://docs.microsoft.com/en-us/windows/win32/medfound/basic-mft-processing-model#process-data
    mNumNeedInput++;
  }

  MFT_RETURN_IF_FAILED(ProcessInput());

  return ProcessEvents();
}

HRESULT MFTEncoder::ProcessInput() {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  MOZ_ASSERT(mEncoder);

  if (mNumNeedInput == 0 || mPendingInputs.empty()) {
    return S_OK;
  }

  auto input = mPendingInputs.front();
  mPendingInputs.pop_front();

  HRESULT hr = mEncoder->ProcessInput(mInputStreamID, input.mSample, 0);

  if (input.mKeyFrameRequested) {
    VARIANT v = {.vt = VT_UI4, .ulVal = 1};
    mConfig->SetValue(&CODECAPI_AVEncVideoForceKeyFrame, &v);
  }
  if (FAILED(hr)) {
    MFT_ENC_LOGE("ProcessInput failed: %s", ErrorMessage(hr).get());
    return hr;
  }
  --mNumNeedInput;

  if (!mEventSource.IsSync()) {
    return S_OK;
  }
  // For sync MFT: Step 3 in
  // https://docs.microsoft.com/en-us/windows/win32/medfound/basic-mft-processing-model#process-data
  DWORD flags = 0;
  hr = mEncoder->GetOutputStatus(&flags);
  MediaEventType evType = MEUnknown;
  switch (hr) {
    case S_OK:
      evType = flags == MFT_OUTPUT_STATUS_SAMPLE_READY
                   ? METransformHaveOutput  // To step 4: ProcessOutput().
                   : METransformNeedInput;  // To step 2: ProcessInput().
      break;
    case E_NOTIMPL:
      evType = METransformHaveOutput;  // To step 4: ProcessOutput().
      break;
    default:
      MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("undefined output status");
      return hr;
  }
  return mEventSource.QueueSyncMFTEvent(evType);
}

HRESULT MFTEncoder::ProcessEvents() {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  MOZ_ASSERT(mEncoder);

  HRESULT hr = E_FAIL;
  while (true) {
    Event event = mEventSource.GetEvent();
    if (event.isErr()) {
      hr = event.unwrapErr();
      break;
    }

    MediaEventType evType = event.unwrap();
    switch (evType) {
      case METransformNeedInput:
        ++mNumNeedInput;
        MFT_RETURN_IF_FAILED(ProcessInput());
        break;
      case METransformHaveOutput:
        MFT_RETURN_IF_FAILED(ProcessOutput());
        break;
      case METransformDrainComplete:
        MFT_ENC_LOGD("State is now DrainState::DRAINED");
        mDrainState = DrainState::DRAINED;
        break;
      default:
        MFT_ENC_LOGE("unsupported event: %s", MediaEventTypeStr(evType));
    }
  }

  switch (hr) {
    case MF_E_NO_EVENTS_AVAILABLE:
      return S_OK;
    case MF_E_MULTIPLE_SUBSCRIBERS:
    default:
      MFT_ENC_LOGE("failed to get event: %s", ErrorMessage(hr).get());
      return hr;
  }
}

HRESULT MFTEncoder::ProcessOutput() {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  MOZ_ASSERT(mEncoder);

  MFT_OUTPUT_DATA_BUFFER output = {.dwStreamID = mOutputStreamID,
                                   .pSample = nullptr,
                                   .dwStatus = 0,
                                   .pEvents = nullptr};
  RefPtr<IMFSample> sample;
  if (!mOutputStreamProvidesSample) {
    MFT_RETURN_IF_FAILED(CreateSample(&sample, mOutputStreamInfo.cbSize,
                                      mOutputStreamInfo.cbAlignment > 1
                                          ? mOutputStreamInfo.cbAlignment - 1
                                          : 0));
    output.pSample = sample;
  }

  DWORD status = 0;
  HRESULT hr = mEncoder->ProcessOutput(0, 1, &output, &status);
  if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
    MFT_ENC_LOGD("output stream change");
    if (output.dwStatus & MFT_OUTPUT_DATA_BUFFER_FORMAT_CHANGE) {
      // Follow the instructions in Microsoft doc:
      // https://docs.microsoft.com/en-us/windows/win32/medfound/handling-stream-changes#output-type
      IMFMediaType* outputType = nullptr;
      MFT_RETURN_IF_FAILED(
          mEncoder->GetOutputAvailableType(mOutputStreamID, 0, &outputType));
      MFT_RETURN_IF_FAILED(
          mEncoder->SetOutputType(mOutputStreamID, outputType, 0));
    }
    return MF_E_TRANSFORM_STREAM_CHANGE;
  }

  // Step 8 in
  // https://docs.microsoft.com/en-us/windows/win32/medfound/basic-mft-processing-model#process-data
  if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
    MOZ_ASSERT(mEventSource.IsSync());
    MOZ_ASSERT(mDrainState == DrainState::DRAINING);

    mEventSource.QueueSyncMFTEvent(METransformDrainComplete);
    return S_OK;
  }

  if (FAILED(hr)) {
    MFT_ENC_LOGE("ProcessOutput failed: %s", ErrorMessage(hr).get());
    return hr;
  }

  mOutputs.AppendElement(output.pSample);
  if (mOutputStreamProvidesSample) {
    // Release MFT provided sample.
    output.pSample->Release();
    output.pSample = nullptr;
  }

  return S_OK;
}

HRESULT MFTEncoder::TakeOutput(nsTArray<RefPtr<IMFSample>>& aOutput) {
  MOZ_ASSERT(aOutput.Length() == 0);
  aOutput.SwapElements(mOutputs);
  return S_OK;
}

HRESULT MFTEncoder::Drain(nsTArray<RefPtr<IMFSample>>& aOutput) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  MOZ_ASSERT(mEncoder);
  MOZ_ASSERT(aOutput.Length() == 0);

  switch (mDrainState) {
    case DrainState::DRAINABLE:
      // Exhaust pending inputs.
      while (!mPendingInputs.empty()) {
        if (mEventSource.IsSync()) {
          // Step 5 in
          // https://docs.microsoft.com/en-us/windows/win32/medfound/basic-mft-processing-model#process-data
          mEventSource.QueueSyncMFTEvent(METransformNeedInput);
        }
        MFT_RETURN_IF_FAILED(ProcessEvents());
      }
      SendMFTMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
      MFT_ENC_LOGD("State is now DrainState::DRAINING");
      mDrainState = DrainState::DRAINING;
      [[fallthrough]];  // To collect and return outputs.
    case DrainState::DRAINING:
      // Collect remaining outputs.
      while (mOutputs.Length() == 0 && mDrainState != DrainState::DRAINED) {
        if (mEventSource.IsSync()) {
          // Step 8 in
          // https://docs.microsoft.com/en-us/windows/win32/medfound/basic-mft-processing-model#process-data
          mEventSource.QueueSyncMFTEvent(METransformHaveOutput);
        }
        MFT_RETURN_IF_FAILED(ProcessEvents());
      }
      [[fallthrough]];  // To return outputs.
    case DrainState::DRAINED:
      aOutput.SwapElements(mOutputs);
      mDrainState = DrainState::DRAINABLE;
      return S_OK;
  }
}

HRESULT MFTEncoder::GetMPEGSequenceHeader(nsTArray<UINT8>& aHeader) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  MOZ_ASSERT(mEncoder);
  MOZ_ASSERT(aHeader.Length() == 0);

  RefPtr<IMFMediaType> outputType;
  MFT_RETURN_IF_FAILED(mEncoder->GetOutputCurrentType(
      mOutputStreamID, getter_AddRefs(outputType)));
  UINT32 length = 0;
  HRESULT hr = outputType->GetBlobSize(MF_MT_MPEG_SEQUENCE_HEADER, &length);
  if (hr == MF_E_ATTRIBUTENOTFOUND || length == 0) {
    return S_OK;
  }
  if (FAILED(hr)) {
    MFT_ENC_LOGE("GetBlobSize MF_MT_MPEG_SEQUENCE_HEADER error: %s",
                 ErrorMessage(hr).get());
    return hr;
  }
  MFT_ENC_LOGD("GetBlobSize MF_MT_MPEG_SEQUENCE_HEADER: %u", length);

  aHeader.SetCapacity(length);
  hr = outputType->GetBlob(MF_MT_MPEG_SEQUENCE_HEADER, aHeader.Elements(),
                           length, nullptr);
  aHeader.SetLength(SUCCEEDED(hr) ? length : 0);

  return hr;
}

MFTEncoder::Event MFTEncoder::EventSource::GetEvent() {
  if (IsSync()) {
    return GetSyncMFTEvent();
  }

  RefPtr<IMFMediaEvent> event;
  HRESULT hr = mImpl.as<RefPtr<IMFMediaEventGenerator>>()->GetEvent(
      MF_EVENT_FLAG_NO_WAIT, getter_AddRefs(event));
  MediaEventType type = MEUnknown;
  if (SUCCEEDED(hr)) {
    hr = event->GetType(&type);
  }
  return SUCCEEDED(hr) ? Event{type} : Event{hr};
}

HRESULT MFTEncoder::EventSource::QueueSyncMFTEvent(MediaEventType aEventType) {
  MOZ_ASSERT(IsSync());
  MOZ_ASSERT(IsOnCurrentThread());

  auto q = mImpl.as<UniquePtr<EventQueue>>().get();
  q->push(aEventType);
  return S_OK;
}

MFTEncoder::Event MFTEncoder::EventSource::GetSyncMFTEvent() {
  MOZ_ASSERT(IsOnCurrentThread());

  auto q = mImpl.as<UniquePtr<EventQueue>>().get();
  if (q->empty()) {
    return Event{MF_E_NO_EVENTS_AVAILABLE};
  }

  MediaEventType type = q->front();
  q->pop();
  return Event{type};
}

#ifdef DEBUG
bool MFTEncoder::EventSource::IsOnCurrentThread() {
  if (!mThread) {
    mThread = GetCurrentSerialEventTarget();
  }
  return mThread->IsOnCurrentThread();
}
#endif

}  // namespace mozilla

#undef MFT_ENC_SLOGE
#undef MFT_ENC_SLOGD
#undef MFT_ENC_LOGE
#undef MFT_ENC_LOGD
#undef MFT_RETURN_IF_FAILED
#undef MFT_RETURN_IF_FAILED_S
#undef MFT_RETURN_VALUE_IF_FAILED
#undef MFT_RETURN_VALUE_IF_FAILED_S
#undef MFT_RETURN_ERROR_IF_FAILED_S
#undef MFT_RETURN_IF_FAILED_IMPL
#undef MFT_RETURN_VALUE_IF_FAILED_IMPL
#undef MFT_RETURN_ERROR_IF_FAILED_IMPL
