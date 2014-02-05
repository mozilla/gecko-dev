/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "nsCOMPtr.h"
#include "nsDOMClassInfo.h"
#include "nsHashPropertyBag.h"
#include "nsThread.h"
#include "DeviceStorage.h"
#include "DeviceStorageFileDescriptor.h"
#include "mozilla/dom/CameraControlBinding.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/MediaManager.h"
#include "mozilla/Services.h"
#include "mozilla/unused.h"
#include "nsIAppsService.h"
#include "nsIObserverService.h"
#include "nsIDOMDeviceStorage.h"
#include "nsIScriptSecurityManager.h"
#include "nsXULAppAPI.h"
#include "DOMCameraManager.h"
#include "DOMCameraCapabilities.h"
#include "DOMCameraControl.h"
#include "CameraCommon.h"
#include "mozilla/dom/CameraManagerBinding.h"
#include "mozilla/dom/BindingUtils.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::idl;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_2(nsDOMCameraControl, mDOMCapabilities, mWindow)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMCameraControl)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMCameraControl)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMCameraControl)

nsDOMCameraControl::~nsDOMCameraControl()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
}

JSObject*
nsDOMCameraControl::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return CameraControlBinding::Wrap(aCx, aScope, this);
}

nsICameraCapabilities*
nsDOMCameraControl::Capabilities()
{
  if (!mDOMCapabilities) {
    mDOMCapabilities = new DOMCameraCapabilities(mCameraControl);
  }

  return mDOMCapabilities;
}

void
nsDOMCameraControl::GetEffect(nsString& aEffect, ErrorResult& aRv)
{
  aRv = mCameraControl->Get(CAMERA_PARAM_EFFECT, aEffect);
}
void
nsDOMCameraControl::SetEffect(const nsAString& aEffect, ErrorResult& aRv)
{
  aRv = mCameraControl->Set(CAMERA_PARAM_EFFECT, aEffect);
}

void
nsDOMCameraControl::GetWhiteBalanceMode(nsString& aWhiteBalanceMode, ErrorResult& aRv)
{
  aRv = mCameraControl->Get(CAMERA_PARAM_WHITEBALANCE, aWhiteBalanceMode);
}
void
nsDOMCameraControl::SetWhiteBalanceMode(const nsAString& aWhiteBalanceMode, ErrorResult& aRv)
{
  aRv = mCameraControl->Set(CAMERA_PARAM_WHITEBALANCE, aWhiteBalanceMode);
}

void
nsDOMCameraControl::GetSceneMode(nsString& aSceneMode, ErrorResult& aRv)
{
  aRv = mCameraControl->Get(CAMERA_PARAM_SCENEMODE, aSceneMode);
}
void
nsDOMCameraControl::SetSceneMode(const nsAString& aSceneMode, ErrorResult& aRv)
{
  aRv = mCameraControl->Set(CAMERA_PARAM_SCENEMODE, aSceneMode);
}

void
nsDOMCameraControl::GetFlashMode(nsString& aFlashMode, ErrorResult& aRv)
{
  aRv = mCameraControl->Get(CAMERA_PARAM_FLASHMODE, aFlashMode);
}
void
nsDOMCameraControl::SetFlashMode(const nsAString& aFlashMode, ErrorResult& aRv)
{
  aRv = mCameraControl->Set(CAMERA_PARAM_FLASHMODE, aFlashMode);
}

void
nsDOMCameraControl::GetFocusMode(nsString& aFocusMode, ErrorResult& aRv)
{
  aRv = mCameraControl->Get(CAMERA_PARAM_FOCUSMODE, aFocusMode);
}
void
nsDOMCameraControl::SetFocusMode(const nsAString& aFocusMode, ErrorResult& aRv)
{
  aRv = mCameraControl->Set(CAMERA_PARAM_FOCUSMODE, aFocusMode);
}

double
nsDOMCameraControl::GetZoom(ErrorResult& aRv)
{
  double zoom;
  aRv = mCameraControl->Get(CAMERA_PARAM_ZOOM, &zoom);
  return zoom;
}

void
nsDOMCameraControl::SetZoom(double aZoom, ErrorResult& aRv)
{
  aRv = mCameraControl->Set(CAMERA_PARAM_ZOOM, aZoom);
}

/* attribute jsval meteringAreas; */
JS::Value
nsDOMCameraControl::GetMeteringAreas(JSContext* cx, ErrorResult& aRv)
{
  JS::Rooted<JS::Value> areas(cx);
  aRv = mCameraControl->Get(cx, CAMERA_PARAM_METERINGAREAS, areas.address());
  return areas;
}

void
nsDOMCameraControl::SetMeteringAreas(JSContext* cx, JS::Handle<JS::Value> aMeteringAreas, ErrorResult& aRv)
{
  aRv = mCameraControl->SetMeteringAreas(cx, aMeteringAreas);
}

JS::Value
nsDOMCameraControl::GetFocusAreas(JSContext* cx, ErrorResult& aRv)
{
  JS::Rooted<JS::Value> value(cx);
  aRv = mCameraControl->Get(cx, CAMERA_PARAM_FOCUSAREAS, value.address());
  return value;
}
void
nsDOMCameraControl::SetFocusAreas(JSContext* cx, JS::Handle<JS::Value> aFocusAreas, ErrorResult& aRv)
{
  aRv = mCameraControl->SetFocusAreas(cx, aFocusAreas);
}

static nsresult
GetSize(JSContext* aCx, JS::Value* aValue, const CameraSize& aSize)
{
  JS::Rooted<JSObject*> o(aCx, JS_NewObject(aCx, nullptr, JS::NullPtr(), JS::NullPtr()));
  if (!o) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  JS::Rooted<JS::Value> v(aCx);

  v = INT_TO_JSVAL(aSize.width);
  if (!JS_SetProperty(aCx, o, "width", v)) {
    return NS_ERROR_FAILURE;
  }
  v = INT_TO_JSVAL(aSize.height);
  if (!JS_SetProperty(aCx, o, "height", v)) {
    return NS_ERROR_FAILURE;
  }

  *aValue = JS::ObjectValue(*o);
  return NS_OK;
}

/* attribute any pictureSize */
JS::Value
nsDOMCameraControl::GetPictureSize(JSContext* cx, ErrorResult& aRv)
{
  JS::Rooted<JS::Value> value(cx);
  
  CameraSize size;
  aRv = mCameraControl->Get(CAMERA_PARAM_PICTURESIZE, size);
  if (aRv.Failed()) {
    return value;
  }

  aRv = GetSize(cx, value.address(), size);
  return value;
}
void
nsDOMCameraControl::SetPictureSize(JSContext* cx, JS::Handle<JS::Value> aSize, ErrorResult& aRv)
{
  CameraSize size;
  aRv = size.Init(cx, aSize.address());
  if (aRv.Failed()) {
    return;
  }

  aRv = mCameraControl->Set(CAMERA_PARAM_PICTURESIZE, size);
}

/* attribute any thumbnailSize */
JS::Value
nsDOMCameraControl::GetThumbnailSize(JSContext* cx, ErrorResult& aRv)
{
  JS::Rooted<JS::Value> value(cx);
  
  CameraSize size;
  aRv = mCameraControl->Get(CAMERA_PARAM_THUMBNAILSIZE, size);
  if (aRv.Failed()) {
    return value;
  }

  aRv = GetSize(cx, value.address(), size);
  return value;
}
void
nsDOMCameraControl::SetThumbnailSize(JSContext* cx, JS::Handle<JS::Value> aSize, ErrorResult& aRv)
{
  CameraSize size;
  aRv = size.Init(cx, aSize.address());
  if (aRv.Failed()) {
    return;
  }

  aRv = mCameraControl->Set(CAMERA_PARAM_THUMBNAILSIZE, size);
}

double
nsDOMCameraControl::GetFocalLength(ErrorResult& aRv)
{
  double focalLength;
  aRv = mCameraControl->Get(CAMERA_PARAM_FOCALLENGTH, &focalLength);
  return focalLength;
}

double
nsDOMCameraControl::GetFocusDistanceNear(ErrorResult& aRv)
{
  double distance;
  aRv = mCameraControl->Get(CAMERA_PARAM_FOCUSDISTANCENEAR, &distance);
  return distance;
}

double
nsDOMCameraControl::GetFocusDistanceOptimum(ErrorResult& aRv)
{
  double distance;
  aRv = mCameraControl->Get(CAMERA_PARAM_FOCUSDISTANCEOPTIMUM, &distance);
  return distance;
}

double
nsDOMCameraControl::GetFocusDistanceFar(ErrorResult& aRv)
{
  double distance;
  aRv = mCameraControl->Get(CAMERA_PARAM_FOCUSDISTANCEFAR, &distance);
  return distance;
}

void
nsDOMCameraControl::SetExposureCompensation(const Optional<double>& aCompensation, ErrorResult& aRv)
{
  if (!aCompensation.WasPassed()) {
    // use NaN to switch the camera back into auto mode
    aRv = mCameraControl->Set(CAMERA_PARAM_EXPOSURECOMPENSATION, NAN);
  }

  aRv = mCameraControl->Set(CAMERA_PARAM_EXPOSURECOMPENSATION, aCompensation.Value());
}

double
nsDOMCameraControl::GetExposureCompensation(ErrorResult& aRv)
{
  double compensation;
  aRv = mCameraControl->Get(CAMERA_PARAM_EXPOSURECOMPENSATION, &compensation);
  return compensation;
}

int32_t
nsDOMCameraControl::SensorAngle()
{
  int32_t angle;
  mCameraControl->Get(CAMERA_PARAM_SENSORANGLE, &angle);
  return angle;
}

already_AddRefed<nsICameraShutterCallback>
nsDOMCameraControl::GetOnShutter(ErrorResult& aRv)
{
  nsCOMPtr<nsICameraShutterCallback> cb;
  aRv = mCameraControl->Get(getter_AddRefs(cb));
  return cb.forget();
}

void
nsDOMCameraControl::SetOnShutter(nsICameraShutterCallback* aOnShutter,
                                 ErrorResult& aRv)
{
  aRv = mCameraControl->Set(aOnShutter);
}

/* attribute nsICameraClosedCallback onClosed; */
already_AddRefed<nsICameraClosedCallback>
nsDOMCameraControl::GetOnClosed(ErrorResult& aRv)
{
  nsCOMPtr<nsICameraClosedCallback> onClosed;
  aRv = mCameraControl->Get(getter_AddRefs(onClosed));
  return onClosed.forget();
}

void
nsDOMCameraControl::SetOnClosed(nsICameraClosedCallback* aOnClosed,
                                ErrorResult& aRv)
{
  aRv = mCameraControl->Set(aOnClosed);
}

already_AddRefed<nsICameraRecorderStateChange>
nsDOMCameraControl::GetOnRecorderStateChange(ErrorResult& aRv)
{
  nsCOMPtr<nsICameraRecorderStateChange> cb;
  aRv = mCameraControl->Get(getter_AddRefs(cb));
  return cb.forget();
}

void
nsDOMCameraControl::SetOnRecorderStateChange(nsICameraRecorderStateChange* aOnRecorderStateChange,
                                             ErrorResult& aRv)
{
  aRv = mCameraControl->Set(aOnRecorderStateChange);
}

void
nsDOMCameraControl::StartRecording(JSContext* aCx,
                                   JS::Handle<JS::Value> aOptions,
                                   nsDOMDeviceStorage& storageArea,
                                   const nsAString& filename,
                                   nsICameraStartRecordingCallback* onSuccess,
                                   const Optional<nsICameraErrorCallback*>& onError,
                                   ErrorResult& aRv)
{
  MOZ_ASSERT(onSuccess, "no onSuccess handler passed");

  // Default values, until the dictionary parser can handle them.
  mOptions.rotation = 0;
  mOptions.maxFileSizeBytes = 0;
  mOptions.maxVideoLengthMs = 0;
  aRv = mOptions.Init(aCx, aOptions.address());
  if (aRv.Failed()) {
    return;
  }

  aRv = NotifyRecordingStatusChange(NS_LITERAL_STRING("starting"));

  #ifdef MOZ_B2G
  if (!mAudioChannelAgent) {
    mAudioChannelAgent = do_CreateInstance("@mozilla.org/audiochannelagent;1");
    if (mAudioChannelAgent) {
      // Camera app will stop recording when it falls to the background, so no callback is necessary.
      mAudioChannelAgent->Init(AUDIO_CHANNEL_CONTENT, nullptr);
      // Video recording doesn't output any sound, so it's not necessary to check canPlay.
      int32_t canPlay;
      mAudioChannelAgent->StartPlaying(&canPlay);
    }
  }
  #endif

  nsCOMPtr<nsIDOMDOMRequest> request;
  mDSFileDescriptor = new DeviceStorageFileDescriptor();
  aRv = storageArea.CreateFileDescriptor(filename, mDSFileDescriptor.get(),
                                         getter_AddRefs(request));
  if (aRv.Failed()) {
    return;
  }

  mOnSuccessCb = onSuccess;
  mOnErrorCb = onError.WasPassed() ? onError.Value() : nullptr;

  request->AddEventListener(NS_LITERAL_STRING("success"), this, false);
  request->AddEventListener(NS_LITERAL_STRING("error"), this, false);
}

NS_IMETHODIMP
nsDOMCameraControl::HandleEvent(nsIDOMEvent* aEvent)
{
  nsString  eventType;
  aEvent->GetType(eventType);
  ErrorResult rv;

  if ((eventType.EqualsLiteral("success")) &&
      mDSFileDescriptor->mFileDescriptor.IsValid()) {

    rv = mCameraControl->StartRecording(&mOptions,
                                        mDSFileDescriptor.get(),
                                        mOnSuccessCb.get(),
                                        mOnErrorCb.get());
    if (!rv.Failed()) {
      return rv.ErrorCode();
    }

    // An error happened. Fall through and call the error callback.
  }

  // We're already be on the main thread, so go ahead and call the
  // error callback directly.

  MOZ_ASSERT(NS_IsMainThread());

  if (mOnErrorCb &&
      nsDOMCameraManager::IsWindowStillActive(mWindow->WindowID())) {
    mOnErrorCb->HandleEvent(NS_LITERAL_STRING("FAILURE"));
  }

  return NS_OK;
}

void
nsDOMCameraControl::StopRecording(ErrorResult& aRv)
{
  aRv = NotifyRecordingStatusChange(NS_LITERAL_STRING("shutdown"));

  #ifdef MOZ_B2G
  if (mAudioChannelAgent) {
    mAudioChannelAgent->StopPlaying();
    mAudioChannelAgent = nullptr;
  }
  #endif

  aRv = mCameraControl->StopRecording();
}

void
nsDOMCameraControl::GetPreviewStream(JSContext* aCx,
                                     JS::Handle<JS::Value> aOptions,
                                     nsICameraPreviewStreamCallback* onSuccess,
                                     const Optional<nsICameraErrorCallback*>& onError,
                                     ErrorResult& aRv)
{
  mozilla::idl::CameraSize size;
  aRv = size.Init(aCx, aOptions.address());
  if (aRv.Failed()) {
    return;
  }

  aRv = mCameraControl->GetPreviewStream(size, onSuccess,
                                         onError.WasPassed()
                                         ? onError.Value() : nullptr);
}

void
nsDOMCameraControl::ResumePreview(ErrorResult& aRv)
{
  aRv = mCameraControl->StartPreview(nullptr);
}

already_AddRefed<nsICameraPreviewStateChange>
nsDOMCameraControl::GetOnPreviewStateChange() const
{
  nsCOMPtr<nsICameraPreviewStateChange> cb;
  mCameraControl->Get(getter_AddRefs(cb));
  return cb.forget();
}

void
nsDOMCameraControl::SetOnPreviewStateChange(nsICameraPreviewStateChange* aCb)
{
  mCameraControl->Set(aCb);
}

void
nsDOMCameraControl::AutoFocus(nsICameraAutoFocusCallback* onSuccess,
                              const Optional<nsICameraErrorCallback*>& onError,
                              ErrorResult& aRv)
{
  aRv = mCameraControl->AutoFocus(onSuccess,
                                  onError.WasPassed() ? onError.Value() : nullptr);
}

void
nsDOMCameraControl::TakePicture(JSContext* aCx,
                                const CameraPictureOptions& aOptions,
                                nsICameraTakePictureCallback* onSuccess,
                                const Optional<nsICameraErrorCallback*>& aOnError,
                                ErrorResult& aRv)
{
  mozilla::idl::CameraSize           size;
  mozilla::idl::CameraPosition       pos;

  aRv = size.Init(aCx, &aOptions.mPictureSize);
  if (aRv.Failed()) {
    return;
  }

  /**
   * Default values, until the dictionary parser can handle them.
   * NaN indicates no value provided.
   */
  pos.latitude = NAN;
  pos.longitude = NAN;
  pos.altitude = NAN;
  pos.timestamp = NAN;
  aRv = pos.Init(aCx, &aOptions.mPosition);
  if (aRv.Failed()) {
    return;
  }

  nsICameraErrorCallback* onError =
    aOnError.WasPassed() ? aOnError.Value() : nullptr;
  aRv = mCameraControl->TakePicture(size, aOptions.mRotation,
                                    aOptions.mFileFormat, pos,
                                    aOptions.mDateTime, onSuccess, onError);
}

void
nsDOMCameraControl::GetPreviewStreamVideoMode(JSContext* aCx,
                                              JS::Handle<JS::Value> aOptions,
                                              nsICameraPreviewStreamCallback* onSuccess,
                                              const Optional<nsICameraErrorCallback*>& onError,
                                              ErrorResult& aRv)
{
  mozilla::idl::CameraRecorderOptions options;
  aRv = options.Init(aCx, aOptions.address());
  if (aRv.Failed()) {
    return;
  }

  aRv = mCameraControl->GetPreviewStreamVideoMode(&options, onSuccess,
                                                  onError.WasPassed()
                                                  ? onError.Value() : nullptr);
}

void
nsDOMCameraControl::ReleaseHardware(const Optional<nsICameraReleaseCallback*>& onSuccess,
                                    const Optional<nsICameraErrorCallback*>& onError,
                                    ErrorResult& aRv)
{
  aRv =
    mCameraControl->ReleaseHardware(
        onSuccess.WasPassed() ? onSuccess.Value() : nullptr,
        onError.WasPassed() ? onError.Value() : nullptr);
}

class GetCameraResult : public nsRunnable
{
public:
  GetCameraResult(nsDOMCameraControl* aDOMCameraControl,
    nsresult aResult,
    const nsMainThreadPtrHandle<nsICameraGetCameraCallback>& onSuccess,
    const nsMainThreadPtrHandle<nsICameraErrorCallback>& onError,
    uint64_t aWindowId)
    : mDOMCameraControl(aDOMCameraControl)
    , mResult(aResult)
    , mOnSuccessCb(onSuccess)
    , mOnErrorCb(onError)
    , mWindowId(aWindowId)
  { }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (nsDOMCameraManager::IsWindowStillActive(mWindowId)) {
      DOM_CAMERA_LOGT("%s : this=%p -- BEFORE CALLBACK\n", __func__, this);
      if (NS_FAILED(mResult)) {
        if (mOnErrorCb.get()) {
          mOnErrorCb->HandleEvent(NS_LITERAL_STRING("FAILURE"));
        }
      } else {
        if (mOnSuccessCb.get()) {
          mOnSuccessCb->HandleEvent(mDOMCameraControl);
        }
      }
      DOM_CAMERA_LOGT("%s : this=%p -- AFTER CALLBACK\n", __func__, this);
    }

    /**
     * Finally, release the extra reference to the DOM-facing camera control.
     * See the nsDOMCameraControl constructor for the corresponding call to
     * NS_ADDREF_THIS().
     */
    NS_RELEASE(mDOMCameraControl);
    return NS_OK;
  }

protected:
  /**
   * 'mDOMCameraControl' is a raw pointer to a previously ADDREF()ed object,
   * which is released in Run().
   */
  nsDOMCameraControl* mDOMCameraControl;
  nsresult mResult;
  nsMainThreadPtrHandle<nsICameraGetCameraCallback> mOnSuccessCb;
  nsMainThreadPtrHandle<nsICameraErrorCallback> mOnErrorCb;
  uint64_t mWindowId;
};

nsresult
nsDOMCameraControl::Result(nsresult aResult,
                           const nsMainThreadPtrHandle<nsICameraGetCameraCallback>& onSuccess,
                           const nsMainThreadPtrHandle<nsICameraErrorCallback>& onError,
                           uint64_t aWindowId)
{
  nsCOMPtr<GetCameraResult> getCameraResult = new GetCameraResult(this, aResult, onSuccess, onError, aWindowId);
  return NS_DispatchToMainThread(getCameraResult);
}

void
nsDOMCameraControl::Shutdown()
{
  DOM_CAMERA_LOGI("%s:%d\n", __func__, __LINE__);
  mCameraControl->Shutdown();
}

nsRefPtr<ICameraControl>
nsDOMCameraControl::GetNativeCameraControl()
{
  return mCameraControl;
}

nsresult
nsDOMCameraControl::NotifyRecordingStatusChange(const nsString& aMsg)
{
  NS_ENSURE_TRUE(mWindow, NS_ERROR_FAILURE);

  return MediaManager::NotifyRecordingStatusChange(mWindow,
                                                   aMsg,
                                                   true /* aIsAudio */,
                                                   true /* aIsVideo */);
}

