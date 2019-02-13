/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CameraPreferences.h"
#include "CameraCommon.h"
#include "DOMCameraManager.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Monitor.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Preferences.h"
#ifdef MOZ_WIDGET_GONK
#include "mozilla/Services.h"
#include "nsIObserverService.h"
#endif

using namespace mozilla;

/* statics */
static StaticAutoPtr<Monitor> sPrefMonitor;

StaticAutoPtr<nsCString> CameraPreferences::sPrefTestEnabled;
StaticAutoPtr<nsCString> CameraPreferences::sPrefHardwareTest;
StaticAutoPtr<nsCString> CameraPreferences::sPrefGonkParameters;

nsresult CameraPreferences::sPrefCameraControlMethodErrorOverride = NS_OK;
nsresult CameraPreferences::sPrefCameraControlAsyncErrorOverride = NS_OK;

uint32_t CameraPreferences::sPrefCameraControlLowMemoryThresholdMB = 0;

bool CameraPreferences::sPrefCameraParametersIsLowMemory = false;

bool CameraPreferences::sPrefCameraParametersPermission = false;

#ifdef MOZ_WIDGET_GONK
StaticRefPtr<CameraPreferences> CameraPreferences::sObserver;

NS_IMPL_ISUPPORTS(CameraPreferences, nsIObserver);
#endif

/* static */
nsresult
CameraPreferences::UpdatePref(const char* aPref, nsresult& aVal)
{
  uint32_t val;
  nsresult rv = Preferences::GetUint(aPref, &val);
  if (NS_SUCCEEDED(rv)) {
    aVal = static_cast<nsresult>(val);
  } else if(rv == NS_ERROR_UNEXPECTED) {
    // Preference does not exist
    rv = NS_OK;
    aVal = NS_OK;
  }
  return rv;
}

/* static */
nsresult
CameraPreferences::UpdatePref(const char* aPref, uint32_t& aVal)
{
  uint32_t val;
  nsresult rv = Preferences::GetUint(aPref, &val);
  if (NS_SUCCEEDED(rv)) {
    aVal = val;
  } else if(rv == NS_ERROR_UNEXPECTED) {
    // Preference does not exist
    rv = NS_OK;
    aVal = 0;
  }
  return rv;
}

/* static */
nsresult
CameraPreferences::UpdatePref(const char* aPref, nsACString& aVal)
{
  nsCString val;
  nsresult rv = Preferences::GetCString(aPref, &val);
  if (NS_SUCCEEDED(rv)) {
    aVal = val;
  } else if(rv == NS_ERROR_UNEXPECTED) {
    // Preference does not exist
    rv = NS_OK;
    aVal.Truncate();
  }
  return rv;
}

/* static */
nsresult
CameraPreferences::UpdatePref(const char* aPref, bool& aVal)
{
  bool val;
  nsresult rv = Preferences::GetBool(aPref, &val);
  if (NS_SUCCEEDED(rv)) {
    aVal = val;
  } else if(rv == NS_ERROR_UNEXPECTED) {
    // Preference does not exist
    rv = NS_OK;
    aVal = false;
  }
  return rv;
}

/* static */
CameraPreferences::Pref CameraPreferences::sPrefs[] = {
  {
    "camera.control.test.enabled",
    kPrefValueIsCString,
    { &sPrefTestEnabled }
  },
  {
    "camera.control.test.hardware",
    kPrefValueIsCString,
    { &sPrefHardwareTest }
  },
  {
    "camera.control.test.permission",
    kPrefValueIsBoolean,
    { &sPrefCameraParametersPermission }
  },
#ifdef MOZ_B2G
  {
    "camera.control.test.hardware.gonk.parameters",
    kPrefValueIsCString,
    { &sPrefGonkParameters }
  },
#endif
  {
    "camera.control.test.method.error",
    kPrefValueIsNsResult,
    { &sPrefCameraControlMethodErrorOverride }
  },
  {
    "camera.control.test.async.error",
    kPrefValueIsNsResult,
    { &sPrefCameraControlAsyncErrorOverride }
  },
  {
    "camera.control.test.is_low_memory",
    kPrefValueIsBoolean,
    { &sPrefCameraParametersIsLowMemory }
  },
  {
    "camera.control.low_memory_thresholdMB",
    kPrefValueIsUint32,
    { &sPrefCameraControlLowMemoryThresholdMB }
  },
};

/* static */
uint32_t
CameraPreferences::PrefToIndex(const char* aPref)
{
  for (uint32_t i = 0; i < ArrayLength(sPrefs); ++i) {
    if (strcmp(aPref, sPrefs[i].mPref) == 0) {
      return i;
    }
  }
  return kPrefNotFound;
}

/* static */
void
CameraPreferences::PreferenceChanged(const char* aPref, void* aClosure)
{
  MonitorAutoLock mon(*sPrefMonitor);

  uint32_t i = PrefToIndex(aPref);
  if (i == kPrefNotFound) {
    DOM_CAMERA_LOGE("Preference '%s' is not tracked by CameraPreferences\n", aPref);
    return;
  }

  Pref& p = sPrefs[i];
  nsresult rv;
  switch (p.mValueType) {
    case kPrefValueIsNsResult:
      {
        nsresult& v = *p.mValue.mAsNsResult;
        rv = UpdatePref(aPref, v);
        if (NS_SUCCEEDED(rv)) {
          DOM_CAMERA_LOGI("Preference '%s' has changed, 0x%x\n", aPref, v);
        }
      }
      break;

    case kPrefValueIsUint32:
      {
        uint32_t& v = *p.mValue.mAsUint32;
        rv = UpdatePref(aPref, v);
        if (NS_SUCCEEDED(rv)) {
          DOM_CAMERA_LOGI("Preference '%s' has changed, %u\n", aPref, v);
        }
      }
      break;

    case kPrefValueIsCString:
      {
        nsCString& v = **p.mValue.mAsCString;
        rv = UpdatePref(aPref, v);
        if (NS_SUCCEEDED(rv)) {
          DOM_CAMERA_LOGI("Preference '%s' has changed, '%s'\n", aPref, v.get());
        }
      }
      break;

    case kPrefValueIsBoolean:
      {
        bool& v = *p.mValue.mAsBoolean;
        rv = UpdatePref(aPref, v);
        if (NS_SUCCEEDED(rv)) {
          DOM_CAMERA_LOGI("Preference '%s' has changed, %s\n",
            aPref, v ? "true" : "false");
        }
      }
      break;

    default:
      MOZ_ASSERT_UNREACHABLE("Unhandled preference value type!");
      return;
  }

  if (NS_FAILED(rv)) {
    DOM_CAMERA_LOGE("Failed to get pref '%s' (0x%x)\n", aPref, rv);
  }
}

/* static */
bool
CameraPreferences::Initialize()
{
  DOM_CAMERA_LOGI("Initializing camera preference callbacks\n");

  nsresult rv;

#ifdef MOZ_WIDGET_GONK
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    sObserver = new CameraPreferences();
    rv = obs->AddObserver(sObserver, "init-camera-hw", false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      sObserver = nullptr;
    }
  } else {
    DOM_CAMERA_LOGE("Could not get observer service\n");
  }
#endif

  sPrefMonitor = new Monitor("CameraPreferences.sPrefMonitor");

  sPrefTestEnabled = new nsCString();
  sPrefHardwareTest = new nsCString();
  sPrefGonkParameters = new nsCString();

  for (uint32_t i = 0; i < ArrayLength(sPrefs); ++i) {
    rv = Preferences::RegisterCallbackAndCall(CameraPreferences::PreferenceChanged,
                                              sPrefs[i].mPref);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }
  }

  DOM_CAMERA_LOGI("Camera preferences initialized\n");
  return true;
}

/* static */
void
CameraPreferences::Shutdown()
{
  DOM_CAMERA_LOGI("Shutting down camera preference callbacks\n");

  for (uint32_t i = 0; i < ArrayLength(sPrefs); ++i) {
    Preferences::UnregisterCallback(CameraPreferences::PreferenceChanged,
                                    sPrefs[i].mPref);
  }

  sPrefTestEnabled = nullptr;
  sPrefHardwareTest = nullptr;
  sPrefGonkParameters = nullptr;
  sPrefMonitor = nullptr;

#ifdef MOZ_WIDGET_GONK
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    nsresult rv = obs->RemoveObserver(sObserver , "init-camera-hw");
    if (NS_FAILED(rv)) {
      DOM_CAMERA_LOGE("Failed to remove CameraPreferences observer (0x%x)\n", rv);
    }
    sObserver = nullptr;
  } else {
    DOM_CAMERA_LOGE("Could not get observer service\n");
  }
#endif

  DOM_CAMERA_LOGI("Camera preferences shut down\n");
}

#ifdef MOZ_WIDGET_GONK
nsresult
CameraPreferences::PreinitCameraHardware()
{
  nsDOMCameraManager::PreinitCameraHardware();
  return NS_OK;
}

NS_IMETHODIMP
CameraPreferences::Observe(nsISupports* aSubject, const char* aTopic, const char16_t* aData)
{
  if (strcmp(aTopic, "init-camera-hw") == 0) {
    return PreinitCameraHardware();
  }

  DOM_CAMERA_LOGE("Got unhandled topic '%s'\n", aTopic);
  return NS_OK;
}
#endif

/* static */
bool
CameraPreferences::GetPref(const char* aPref, nsACString& aVal)
{
  MOZ_ASSERT(sPrefMonitor, "sPrefMonitor missing in CameraPreferences::GetPref()");
  MonitorAutoLock mon(*sPrefMonitor);

  uint32_t i = PrefToIndex(aPref);
  if (i == kPrefNotFound || i >= ArrayLength(sPrefs)) {
    DOM_CAMERA_LOGW("Preference '%s' is not tracked by CameraPreferences\n", aPref);
    return false;
  }
  if (sPrefs[i].mValueType != kPrefValueIsCString) {
    DOM_CAMERA_LOGW("Preference '%s' is not a string type\n", aPref);
    return false;
  }

  StaticAutoPtr<nsCString>* s = sPrefs[i].mValue.mAsCString;
  if (!*s) {
    DOM_CAMERA_LOGE("Preference '%s' cache is not initialized\n", aPref);
    return false;
  }
  if ((*s)->IsEmpty()) {
    DOM_CAMERA_LOGI("Preference '%s' is not set\n", aPref);
    return false;
  }

  DOM_CAMERA_LOGI("Preference '%s', got '%s'\n", aPref, (*s)->get());
  aVal = **s;
  return true;
}

/* static */
bool
CameraPreferences::GetPref(const char* aPref, nsresult& aVal)
{
  MOZ_ASSERT(sPrefMonitor, "sPrefMonitor missing in CameraPreferences::GetPref()");
  MonitorAutoLock mon(*sPrefMonitor);

  uint32_t i = PrefToIndex(aPref);
  if (i == kPrefNotFound || i >= ArrayLength(sPrefs)) {
    DOM_CAMERA_LOGW("Preference '%s' is not tracked by CameraPreferences\n", aPref);
    return false;
  }
  if (sPrefs[i].mValueType != kPrefValueIsNsResult) {
    DOM_CAMERA_LOGW("Preference '%s' is not an nsresult type\n", aPref);
    return false;
  }

  nsresult v = *sPrefs[i].mValue.mAsNsResult;
  if (v == NS_OK) {
    DOM_CAMERA_LOGW("Preference '%s' is not set\n", aPref);
    return false;
  }

  DOM_CAMERA_LOGI("Preference '%s', got 0x%x\n", aPref, v);
  aVal = v;
  return true;
}

/* static */
bool
CameraPreferences::GetPref(const char* aPref, uint32_t& aVal)
{
  MOZ_ASSERT(sPrefMonitor, "sPrefMonitor missing in CameraPreferences::GetPref()");
  MonitorAutoLock mon(*sPrefMonitor);

  uint32_t i = PrefToIndex(aPref);
  if (i == kPrefNotFound || i >= ArrayLength(sPrefs)) {
    DOM_CAMERA_LOGW("Preference '%s' is not tracked by CameraPreferences\n", aPref);
    return false;
  }
  if (sPrefs[i].mValueType != kPrefValueIsUint32) {
    DOM_CAMERA_LOGW("Preference '%s' is not a uint32_t type\n", aPref);
    return false;
  }

  uint32_t v = *sPrefs[i].mValue.mAsUint32;
  DOM_CAMERA_LOGI("Preference '%s', got %u\n", aPref, v);
  aVal = v;
  return true;
}

/* static */
bool
CameraPreferences::GetPref(const char* aPref, bool& aVal)
{
  MOZ_ASSERT(sPrefMonitor, "sPrefMonitor missing in CameraPreferences::GetPref()");
  MonitorAutoLock mon(*sPrefMonitor);

  uint32_t i = PrefToIndex(aPref);
  if (i == kPrefNotFound || i >= ArrayLength(sPrefs)) {
    DOM_CAMERA_LOGW("Preference '%s' is not tracked by CameraPreferences\n", aPref);
    return false;
  }
  if (sPrefs[i].mValueType != kPrefValueIsBoolean) {
    DOM_CAMERA_LOGW("Preference '%s' is not a boolean type\n", aPref);
    return false;
  }

  bool v = *sPrefs[i].mValue.mAsBoolean;
  DOM_CAMERA_LOGI("Preference '%s', got %s\n", aPref, v ? "true" : "false");
  aVal = v;
  return true;
}
