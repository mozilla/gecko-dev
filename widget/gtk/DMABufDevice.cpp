/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DMABufDevice.h"
#include "DMABufFormats.h"
#ifdef MOZ_WAYLAND
#  include "nsWaylandDisplay.h"
#endif
#include "base/message_loop.h"    // for MessageLoop
#include "mozilla/gfx/Logging.h"  // for gfxCriticalNote
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/gfx/gfxVars.h"
#include "WidgetUtilsGtk.h"
#include "gfxConfig.h"
#include "nsIGfxInfo.h"
#include "GfxInfo.h"
#include "mozilla/Components.h"
#include "mozilla/ClearOnShutdown.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <mutex>
#include <unistd.h>

using namespace mozilla::gfx;

namespace mozilla {
namespace widget {

StaticMutex DMABufDeviceLock::sMutex;
DMABufDevice* DMABufDeviceLock::sDMABufDevice = nullptr;

bool sUseWebGLDmabufBackend = true;

#define GBMLIB_NAME "libgbm.so.1"
#define DRMLIB_NAME "libdrm.so.2"

bool GbmLib::sLoaded = false;
void* GbmLib::sGbmLibHandle = nullptr;
void* GbmLib::sXf86DrmLibHandle = nullptr;
CreateDeviceFunc GbmLib::sCreateDevice;
DestroyDeviceFunc GbmLib::sDestroyDevice;
CreateFunc GbmLib::sCreate;
CreateWithModifiersFunc GbmLib::sCreateWithModifiers;
CreateWithModifiers2Func GbmLib::sCreateWithModifiers2;
GetModifierFunc GbmLib::sGetModifier;
GetStrideFunc GbmLib::sGetStride;
GetFdFunc GbmLib::sGetFd;
DestroyFunc GbmLib::sDestroy;
MapFunc GbmLib::sMap;
UnmapFunc GbmLib::sUnmap;
GetPlaneCountFunc GbmLib::sGetPlaneCount;
GetHandleForPlaneFunc GbmLib::sGetHandleForPlane;
GetStrideForPlaneFunc GbmLib::sGetStrideForPlane;
GetOffsetFunc GbmLib::sGetOffset;
DeviceIsFormatSupportedFunc GbmLib::sDeviceIsFormatSupported;
DrmPrimeHandleToFDFunc GbmLib::sDrmPrimeHandleToFD;
CreateSurfaceFunc GbmLib::sCreateSurface;
DestroySurfaceFunc GbmLib::sDestroySurface;

bool GbmLib::IsLoaded() {
  return sCreateDevice != nullptr && sDestroyDevice != nullptr &&
         sCreate != nullptr && sCreateWithModifiers != nullptr &&
         sGetModifier != nullptr && sGetStride != nullptr &&
         sGetFd != nullptr && sDestroy != nullptr && sMap != nullptr &&
         sUnmap != nullptr && sGetPlaneCount != nullptr &&
         sGetHandleForPlane != nullptr && sGetStrideForPlane != nullptr &&
         sGetOffset != nullptr && sDeviceIsFormatSupported != nullptr &&
         sDrmPrimeHandleToFD != nullptr && sCreateSurface != nullptr &&
         sDestroySurface != nullptr;
}

bool GbmLib::Load() {
  static bool sTriedToLoad = false;
  if (sTriedToLoad) {
    return sLoaded;
  }

  sTriedToLoad = true;

  MOZ_ASSERT(!sGbmLibHandle);
  MOZ_ASSERT(!sLoaded);

  LOGDMABUF(("Loading DMABuf system library %s ...\n", GBMLIB_NAME));

  sGbmLibHandle = dlopen(GBMLIB_NAME, RTLD_LAZY | RTLD_LOCAL);
  if (!sGbmLibHandle) {
    LOGDMABUF(("Failed to load %s, dmabuf isn't available.\n", GBMLIB_NAME));
    return false;
  }

  sCreateDevice = (CreateDeviceFunc)dlsym(sGbmLibHandle, "gbm_create_device");
  sDestroyDevice =
      (DestroyDeviceFunc)dlsym(sGbmLibHandle, "gbm_device_destroy");
  sCreate = (CreateFunc)dlsym(sGbmLibHandle, "gbm_bo_create");
  sCreateWithModifiers = (CreateWithModifiersFunc)dlsym(
      sGbmLibHandle, "gbm_bo_create_with_modifiers");
  sCreateWithModifiers2 = (CreateWithModifiers2Func)dlsym(
      sGbmLibHandle, "gbm_bo_create_with_modifiers2");
  sGetModifier = (GetModifierFunc)dlsym(sGbmLibHandle, "gbm_bo_get_modifier");
  sGetStride = (GetStrideFunc)dlsym(sGbmLibHandle, "gbm_bo_get_stride");
  sGetFd = (GetFdFunc)dlsym(sGbmLibHandle, "gbm_bo_get_fd");
  sDestroy = (DestroyFunc)dlsym(sGbmLibHandle, "gbm_bo_destroy");
  sMap = (MapFunc)dlsym(sGbmLibHandle, "gbm_bo_map");
  sUnmap = (UnmapFunc)dlsym(sGbmLibHandle, "gbm_bo_unmap");
  sGetPlaneCount =
      (GetPlaneCountFunc)dlsym(sGbmLibHandle, "gbm_bo_get_plane_count");
  sGetHandleForPlane = (GetHandleForPlaneFunc)dlsym(
      sGbmLibHandle, "gbm_bo_get_handle_for_plane");
  sGetStrideForPlane = (GetStrideForPlaneFunc)dlsym(
      sGbmLibHandle, "gbm_bo_get_stride_for_plane");
  sGetOffset = (GetOffsetFunc)dlsym(sGbmLibHandle, "gbm_bo_get_offset");
  sDeviceIsFormatSupported = (DeviceIsFormatSupportedFunc)dlsym(
      sGbmLibHandle, "gbm_device_is_format_supported");
  sCreateSurface =
      (CreateSurfaceFunc)dlsym(sGbmLibHandle, "gbm_surface_create");
  sDestroySurface =
      (DestroySurfaceFunc)dlsym(sGbmLibHandle, "gbm_surface_destroy");

  sXf86DrmLibHandle = dlopen(DRMLIB_NAME, RTLD_LAZY | RTLD_LOCAL);
  if (!sXf86DrmLibHandle) {
    LOGDMABUF(("Failed to load %s, dmabuf isn't available.\n", DRMLIB_NAME));
    return false;
  }
  sDrmPrimeHandleToFD =
      (DrmPrimeHandleToFDFunc)dlsym(sXf86DrmLibHandle, "drmPrimeHandleToFD");
  sLoaded = IsLoaded();
  if (!sLoaded) {
    LOGDMABUF(("Failed to load all symbols from %s\n", GBMLIB_NAME));
  }
  return sLoaded;
}

DMABufDevice* DMABufDeviceLock::EnsureDMABufDevice() {
  sMutex.AssertCurrentThreadOwns();
  static std::once_flag onceFlag;
  std::call_once(onceFlag, [&] {
    sDMABufDevice = new DMABufDevice();
    if (sDMABufDevice->Init()) {
      LOGDMABUF(("EnsureDMABufDevice(): created DMABufDevice"));
    } else {
      nsCString failureId;
      Unused << sDMABufDevice->IsEnabled(failureId);
      LOGDMABUF(("EnsureDMABufDevice(): failed to init DMABufDevice: %s",
                 failureId.get()));
    }
  });

  MOZ_DIAGNOSTIC_ASSERT(sDMABufDevice, "Missing DMABufDevice!");
  return sDMABufDevice;
}

DMABufDeviceLock::DMABufDeviceLock() {
  LOGDMABUF(("DMABufDeviceLock::DMABufDeviceLock() [%p]", this));
  sMutex.Lock();

  mDMABufDevice = EnsureDMABufDevice();
  mGBMDevice = mDMABufDevice->GetDevice(this);
}

DMABufDeviceLock::~DMABufDeviceLock() {
  LOGDMABUF(("DMABufDeviceLock::~DMABufDeviceLock() [%p]", this));
  sMutex.AssertCurrentThreadOwns();
  MOZ_DIAGNOSTIC_ASSERT(mDMABufDevice);

  mGBMDevice = nullptr;
  mDMABufDevice = nullptr;

  sMutex.Unlock();
}

gbm_device* DMABufDevice::GetDevice(DMABufDeviceLock* aDMABufDeviceLock) {
  LOGDMABUF(("DMABufDevice::GetDevice() [%p]", this));
  if (mDRMFd == -1) {
    LOGDMABUF(("  mDRMFd is missing!"));
    return nullptr;
  }
  if (!mGbmDevice) {
    mGbmDevice = GbmLib::CreateDevice(mDRMFd);
    if (!mGbmDevice) {
      LOGDMABUF(("  GbmLib::CreateDevice() failed for fd %d", mDRMFd));
    }
  }
  return mGbmDevice;
}

int DMABufDevice::GetDmabufFD(uint32_t aGEMHandle) {
  int fd;
  return GbmLib::DrmPrimeHandleToFD(mDRMFd, aGEMHandle, 0, &fd) < 0 ? -1 : fd;
}

int DMABufDevice::OpenDRMFd() { return open(mDrmRenderNode.get(), O_RDWR); }

bool DMABufDevice::IsEnabled(nsACString& aFailureId) {
  if (mDRMFd == -1) {
    aFailureId = mFailureId;
  }
  return mDRMFd != -1;
}

DMABufDevice::~DMABufDevice() {
  LOGDMABUF(("DMABufDevice::~DMABufDevice() [%p] mGbmDevice [%p] mDRMFd [%d]",
             this, mGbmDevice, mDRMFd));
  if (mGbmDevice) {
    GbmLib::DestroyDevice(mGbmDevice);
    mGbmDevice = nullptr;
  }
  if (mDRMFd != -1) {
    close(mDRMFd);
    mDRMFd = -1;
  }
}

bool DMABufDevice::Init() {
  LOGDMABUF(("DMABufDevice::Init()"));

  if (!GbmLib::IsAvailable()) {
    LOGDMABUF(("GbmLib is not available!"));
    mFailureId = "FEATURE_FAILURE_NO_LIBGBM";
    return false;
  }

  // See Bug 1865747 for details.
  if (XRE_IsParentProcess()) {
    if (auto* gbmBackend = getenv("GBM_BACKEND")) {
      const nsCOMPtr<nsIGfxInfo> gfxInfo = components::GfxInfo::Service();
      nsAutoString vendorID;
      gfxInfo->GetAdapterVendorID(vendorID);
      if (vendorID != GfxDriverInfo::GetDeviceVendor(DeviceVendor::NVIDIA)) {
        if (strstr(gbmBackend, "nvidia")) {
          unsetenv("GBM_BACKEND");
        }
      }
    }
  }

  mDrmRenderNode = nsAutoCString(getenv("MOZ_DRM_DEVICE"));
  if (mDrmRenderNode.IsEmpty()) {
    mDrmRenderNode.Assign(gfx::gfxVars::DrmRenderDevice());
  }
  if (mDrmRenderNode.IsEmpty()) {
    LOGDMABUF(("We're missing DRM render device!\n"));
    mFailureId = "FEATURE_FAILURE_NO_DRM_DEVICE";
    return false;
  }

  LOGDMABUF(("Using DRM device %s", mDrmRenderNode.get()));
  mDRMFd = open(mDrmRenderNode.get(), O_RDWR);
  if (mDRMFd < 0) {
    LOGDMABUF(("Failed to open drm render node %s error %s\n",
               mDrmRenderNode.get(), strerror(errno)));
    mFailureId = "FEATURE_FAILURE_NO_DRM_DEVICE";
    return false;
  }

  LOGDMABUF(("DMABuf is enabled"));
  return true;
}

bool DMABufDevice::IsDMABufWebGLEnabled() {
  LOGDMABUF(
      ("DMABufDevice::IsDMABufWebGLEnabled: UseDMABuf %d "
       "sUseWebGLDmabufBackend %d "
       "UseDMABufWebGL %d\n",
       gfx::gfxVars::UseDMABuf(), sUseWebGLDmabufBackend,
       gfx::gfxVars::UseDMABufWebGL()));
  return gfx::gfxVars::UseDMABuf() && sUseWebGLDmabufBackend &&
         gfx::gfxVars::UseDMABufWebGL();
}

void DMABufDevice::DisableDMABufWebGL() { sUseWebGLDmabufBackend = false; }

}  // namespace widget
}  // namespace mozilla
