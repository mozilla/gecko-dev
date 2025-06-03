/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <atomic>
#include <audiopolicy.h>
#include <windows.h>
#include <mmdeviceapi.h>

#include "mozilla/AppShutdown.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "nsIStringBundle.h"

#include "nsCOMPtr.h"
#include "nsID.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "mozilla/Attributes.h"
#include "mozilla/mscom/AgileReference.h"
#include "mozilla/Logging.h"
#include "mozilla/mscom/Utils.h"
#include "mozilla/Mutex.h"
#include "mozilla/WindowsVersion.h"

#ifdef MOZ_BACKGROUNDTASKS
#  include "mozilla/BackgroundTasks.h"
#endif

namespace mozilla {
namespace widget {

static mozilla::LazyLogModule sAudioSessionLog("AudioSession");
#define LOGD(...)                                                      \
  MOZ_LOG(mozilla::widget::sAudioSessionLog, mozilla::LogLevel::Debug, \
          (__VA_ARGS__))

/*
 * The AudioSession is most visible as the controller for the Firefox entries
 * in the Windows volume mixer in Windows 10.  This class wraps
 * IAudioSessionControl and implements IAudioSessionEvents for callbacks from
 * Windows -- we only need OnSessionDisconnected, which happens when the audio
 * device changes.  This should be used on background (MTA) threads only.
 * This may be used concurrently by MSCOM as IAudioSessionEvents, so
 * methods must be threadsafe and public methods cannot use MOZ_REQUIRES.
 */
class AudioSession final : public IAudioSessionEvents {
 public:
  static void Create(nsString&& aDisplayName, nsString&& aIconPath,
                     nsID&& aSessionGroupingParameter) {
    MOZ_ASSERT(mscom::IsCurrentThreadMTA());
    LOGD("Gecko will create the AudioSession object.");
    if (AppShutdown::IsShutdownImpending()) {
      // Quick shutdown is guaranteed.  Don't create as we may already be past
      // DestroyAudioSession.
      LOGD("Did not create AudioSession.  Shutting down.");
      return;
    }

    StaticMutexAutoLock lock(sMutex);
    // Shouldn't create twice.
    MOZ_ASSERT(!sService);
    NS_ENSURE_TRUE_VOID(!sService);
    sService = new AudioSession(std::move(aDisplayName), std::move(aIconPath),
                                std::move(aSessionGroupingParameter));
    LOGD("Created AudioSession.");
  }

  static void MaybeRestart() {
    MOZ_ASSERT(mscom::IsCurrentThreadMTA());
    if (AppShutdown::IsShutdownImpending()) {
      LOGD("Did not restart AudioSession.  Shutting down.");
      return;
    }

    LOGD("Gecko will restart the AudioSession object.");
    StaticMutexAutoLock lock(sMutex);
    // Since IsShutdownImpending was false, Gecko hasn't destroyed
    // the AudioSession yet.  And since we are restarting, we must already
    // have a previously stopped one.
    MOZ_ASSERT(sService);
    NS_ENSURE_TRUE_VOID(sService);
    sService->Start();
    LOGD("Restarted AudioSession.");
  }

  static void Destroy() {
    MOZ_ASSERT(NS_IsMainThread() && AppShutdown::IsShutdownImpending());
    StaticMutexAutoLock lock(sMutex);
    LOGD("Gecko will release the AudioSession object | sService: %p",
         sService.get());
    NS_ENSURE_TRUE_VOID(sService);
    sService->Stop(false /* shouldRestart */);
    sService = nullptr;
    LOGD("Released AudioSession object.");
  }

  // COM IUnknown
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP QueryInterface(REFIID, void**) override;
  STDMETHODIMP_(ULONG) Release() override;

  // IAudioSessionEvents
  STDMETHODIMP OnSessionDisconnected(
      AudioSessionDisconnectReason aReason) override {
    MOZ_ASSERT(mscom::IsCurrentThreadMTA());
    LOGD("%s | aReason: %d | Attempting to recreate.", __FUNCTION__, aReason);
    StaticMutexAutoLock lock(sMutex);
    Stop(true /* shouldRestart */);
    return S_OK;
  }

  STDMETHODIMP OnChannelVolumeChanged(DWORD aChannelCount,
                                      float aChannelVolumeArray[],
                                      DWORD aChangedChannel,
                                      LPCGUID aContext) override {
    return S_OK;
  }
  STDMETHODIMP OnDisplayNameChanged(LPCWSTR aDisplayName,
                                    LPCGUID aContext) override {
    return S_OK;
  }
  STDMETHODIMP OnGroupingParamChanged(LPCGUID aGroupingParam,
                                      LPCGUID aContext) override {
    return S_OK;
  }
  STDMETHODIMP OnIconPathChanged(LPCWSTR aIconPath, LPCGUID aContext) override {
    return S_OK;
  }
  STDMETHODIMP OnSimpleVolumeChanged(float aVolume, BOOL aMute,
                                     LPCGUID aContext) override {
    return S_OK;
  }
  STDMETHODIMP OnStateChanged(AudioSessionState aState) override {
    return S_OK;
  }

 private:
  AudioSession(nsString&& aDisplayName, nsString&& aIconPath,
               nsID&& aSessionGroupingParameter) MOZ_REQUIRES(sMutex)
      : mDisplayName(aDisplayName),
        mIconPath(aIconPath),
        mSessionGroupingParameter(aSessionGroupingParameter) {
    Start();
  }
  ~AudioSession() {
    // Must have stopped and not restarted.
    MOZ_ASSERT(!mAudioSessionControl);
    LOGD("AudioSession object was destroyed.");
  }

  void Start() MOZ_REQUIRES(sMutex);
  void Stop(bool shouldRestart) MOZ_REQUIRES(sMutex);

  static StaticMutex sMutex;

  RefPtr<IAudioSessionControl> mAudioSessionControl MOZ_GUARDED_BY(sMutex);
  nsString mDisplayName MOZ_GUARDED_BY(sMutex);
  nsString mIconPath MOZ_GUARDED_BY(sMutex);
  nsID mSessionGroupingParameter MOZ_GUARDED_BY(sMutex);

  ThreadSafeAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD

  // Background (MTA) threads only.  The object itself may be used
  // concurrently but the sService variable is synchronized.
  static StaticRefPtr<AudioSession> sService MOZ_GUARDED_BY(sMutex);
};

/* static */ StaticMutex AudioSession::sMutex;
/* static */ StaticRefPtr<AudioSession> AudioSession::sService;

void CreateAudioSession() {
  MOZ_ASSERT(XRE_IsParentProcess());

#ifdef MOZ_BACKGROUNDTASKS
  if (BackgroundTasks::IsBackgroundTaskMode()) {
    LOGD("In BackgroundTasks mode.  CreateAudioSession was not run.");
    return;
  }
#endif

  LOGD("CreateAudioSession");
  // This looks odd since it is already running on the main thread, but it is
  // similar to the audio library's initialization in
  // CubebUtils::InitBrandName.  We need to delay reading the brand name for
  // use with the volume control in the mixer because nsIStringBundles are
  // delay initialized and AudioSession init happens very early.
  // These services require us to do this on the main thread.
  NS_DispatchToMainThread(
      NS_NewRunnableFunction("DelayStartAudioSession", []() {
        nsCOMPtr<nsIStringBundleService> bundleService =
            do_GetService(NS_STRINGBUNDLE_CONTRACTID);
        MOZ_ASSERT(bundleService);

        nsCOMPtr<nsIStringBundle> bundle;
        bundleService->CreateBundle("chrome://branding/locale/brand.properties",
                                    getter_AddRefs(bundle));
        MOZ_ASSERT(bundle);

        nsString displayName;
        bundle->GetStringFromName("brandFullName", displayName);

        nsString iconPath;
        wchar_t* buffer;
        iconPath.GetMutableData(&buffer, MAX_PATH);
        ::GetModuleFileNameW(nullptr, buffer, MAX_PATH);

        nsID sessionGroupingParameter;
        [[maybe_unused]] nsresult rv =
            nsID::GenerateUUIDInPlace(sessionGroupingParameter);
        MOZ_ASSERT(rv == NS_OK);

        // Construct AudioSession on background thread.
        NS_DispatchBackgroundTask(NS_NewCancelableRunnableFunction(
            "CreateAudioSession", [displayName = std::move(displayName),
                                   iconPath = std::move(iconPath),
                                   sessionGroupingParameter = std::move(
                                       sessionGroupingParameter)]() mutable {
              AudioSession::Create(std::move(displayName), std::move(iconPath),
                                   std::move(sessionGroupingParameter));
            }));
      }));
}

void DestroyAudioSession() {
  MOZ_ASSERT(XRE_IsParentProcess());

#ifdef MOZ_BACKGROUNDTASKS
  if (BackgroundTasks::IsBackgroundTaskMode()) {
    LOGD("In BackgroundTasks mode.  DestroyAudioSession was not run.");
    return;
  }
#endif

  LOGD("DestroyAudioSession");
  MOZ_ASSERT(AppShutdown::IsShutdownImpending());
  AudioSession::Destroy();
}

NS_IMPL_ADDREF(AudioSession)
NS_IMPL_RELEASE(AudioSession)

STDMETHODIMP
AudioSession::QueryInterface(REFIID iid, void** ppv) {
  const IID IID_IAudioSessionEvents = __uuidof(IAudioSessionEvents);
  if ((IID_IUnknown == iid) || (IID_IAudioSessionEvents == iid)) {
    *ppv = static_cast<IAudioSessionEvents*>(this);
    AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

void AudioSession::Start() {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());

  const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
  const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
  const IID IID_IAudioSessionManager = __uuidof(IAudioSessionManager);

  MOZ_ASSERT(!mAudioSessionControl);
  MOZ_ASSERT(!mDisplayName.IsEmpty() || !mIconPath.IsEmpty(),
             "Should never happen ...");

  LOGD("Starting AudioSession.");

  // MOZ_GUARDED_BY isn't recognized in MakeScopeExit, but Start() already
  // requires sMutex, so just escape thread analysis.
  auto scopeExit = MakeScopeExit([&]() MOZ_NO_THREAD_SAFETY_ANALYSIS {
    LOGD("Failed to properly start AudioSession.  Stopping.");
    Stop(false /* shouldRestart */);
  });

  RefPtr<IMMDeviceEnumerator> enumerator;
  HRESULT hr =
      ::CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                         IID_IMMDeviceEnumerator, getter_AddRefs(enumerator));
  if (FAILED(hr)) {
    return;
  }

  RefPtr<IMMDevice> device;
  hr = enumerator->GetDefaultAudioEndpoint(
      EDataFlow::eRender, ERole::eMultimedia, getter_AddRefs(device));
  if (FAILED(hr)) {
    return;
  }

  RefPtr<IAudioSessionManager> manager;
  hr = device->Activate(IID_IAudioSessionManager, CLSCTX_ALL, nullptr,
                        getter_AddRefs(manager));
  if (FAILED(hr)) {
    return;
  }

  hr = manager->GetAudioSessionControl(&GUID_NULL, 0,
                                       getter_AddRefs(mAudioSessionControl));

  if (FAILED(hr) || !mAudioSessionControl) {
    return;
  }

  // Increments refcount of 'this'.
  hr = mAudioSessionControl->RegisterAudioSessionNotification(this);
  if (FAILED(hr)) {
    return;
  }

  hr = mAudioSessionControl->SetGroupingParam(
      (LPGUID) & (mSessionGroupingParameter), nullptr);
  if (FAILED(hr)) {
    return;
  }

  hr = mAudioSessionControl->SetDisplayName(mDisplayName.get(), nullptr);
  if (FAILED(hr)) {
    return;
  }

  hr = mAudioSessionControl->SetIconPath(mIconPath.get(), nullptr);
  if (FAILED(hr)) {
    return;
  }

  LOGD("AudioSession started.");
  scopeExit.release();
}

void AudioSession::Stop(bool aShouldRestart) {
  // We usually use this on MTA threads but we shut down after
  // xpcom-shutdown-threads, so we don't have any easily available.
  // An MTA object is thread-safe by definition and is therefore considered
  // generally safe to use in the STA without an agile reference.
  MOZ_ASSERT(mscom::IsCurrentThreadMTA() ||
             (!aShouldRestart && NS_IsMainThread()));
  if (!mAudioSessionControl) {
    return;
  }

  LOGD("AudioSession stopping");

  // Decrements refcount of 'this' but we are holding a static one in sService.
  mAudioSessionControl->UnregisterAudioSessionNotification(this);

  if (!aShouldRestart) {
    // If we are shutting down then there is no audio playing so we can just
    // release the control now.
    mAudioSessionControl = nullptr;
    return;
  }

  LOGD("Attempting to restart AudioSession.");

  // Deleting the IAudioSessionControl COM object requires the STA/main thread.
  // Audio code may concurrently be running on the main thread and it may
  // block waiting for this to complete, creating deadlock.  So we destroy the
  // IAudioSessionControl on the main thread instead.  We marshall the object
  // to the main thread's apartment as an AgileReference for completeness,
  // since it was created from an MTA thread.
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());
  mscom::AgileReference agileAsc(mAudioSessionControl);
  mAudioSessionControl = nullptr;
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "FreeIAudioSessionControl", [agileAsc = std::move(agileAsc)]() mutable {
        // Now release the AgileReference which holds our only reference to the
        // IAudioSessionControl, then restart (i.e. create a new one).
        agileAsc = nullptr;
        NS_DispatchBackgroundTask(NS_NewCancelableRunnableFunction(
            "RestartAudioSession", [] { AudioSession::MaybeRestart(); }));
      }));
}

}  // namespace widget
}  // namespace mozilla
