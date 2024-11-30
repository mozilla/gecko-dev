/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __MOZ_WAYLAND_DISPLAY_H__
#define __MOZ_WAYLAND_DISPLAY_H__

#include "DMABufLibWrapper.h"

#include "mozilla/widget/mozwayland.h"
#include "mozilla/widget/gbm.h"
#include "mozilla/widget/fractional-scale-v1-client-protocol.h"
#include "mozilla/widget/idle-inhibit-unstable-v1-client-protocol.h"
#include "mozilla/widget/relative-pointer-unstable-v1-client-protocol.h"
#include "mozilla/widget/pointer-constraints-unstable-v1-client-protocol.h"
#include "mozilla/widget/pointer-gestures-unstable-v1-client-protocol.h"
#include "mozilla/widget/linux-dmabuf-unstable-v1-client-protocol.h"
#include "mozilla/widget/viewporter-client-protocol.h"
#include "mozilla/widget/xdg-activation-v1-client-protocol.h"
#include "mozilla/widget/xdg-dbus-annotation-v1-client-protocol.h"
#include "mozilla/widget/xdg-output-unstable-v1-client-protocol.h"
#include "mozilla/widget/xx-color-management-v4.h"

namespace mozilla::widget {

constexpr const int sColorTransfersNum =
    XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_HLG + 1;
constexpr const int sColorPrimariesNum =
    XX_COLOR_MANAGER_V4_PRIMARIES_ADOBE_RGB + 1;

// Our general connection to Wayland display server,
// holds our display connection and runs event loop.
// We have a global nsWaylandDisplay object for each thread.
class nsWaylandDisplay {
 public:
  // Create nsWaylandDisplay object on top of native Wayland wl_display
  // connection.
  explicit nsWaylandDisplay(wl_display* aDisplay);

  wl_display* GetDisplay() { return mDisplay; };
  wl_compositor* GetCompositor() { return mCompositor; };
  wl_subcompositor* GetSubcompositor() { return mSubcompositor; };
  wl_shm* GetShm() { return mShm; };
  zwp_idle_inhibit_manager_v1* GetIdleInhibitManager() {
    return mIdleInhibitManager;
  }
  wp_viewporter* GetViewporter() { return mViewporter; };
  zwp_relative_pointer_manager_v1* GetRelativePointerManager() {
    return mRelativePointerManager;
  }
  zwp_pointer_constraints_v1* GetPointerConstraints() {
    return mPointerConstraints;
  }
  zwp_linux_dmabuf_v1* GetDmabuf() { return mDmabuf; };
  xdg_activation_v1* GetXdgActivation() { return mXdgActivation; };
  xdg_dbus_annotation_manager_v1* GetXdgDbusAnnotationManager() {
    return mXdgDbusAnnotationManager;
  }
  wp_fractional_scale_manager_v1* GetFractionalScaleManager() {
    return mFractionalScaleManager;
  }
  bool IsPrimarySelectionEnabled() { return mIsPrimarySelectionEnabled; }

  wl_pointer* GetPointer() { return mPointer; }
  void SetPointer(wl_pointer* aPointer);
  void RemovePointer();

  void SetShm(wl_shm* aShm);

  void SetKeyboard(wl_keyboard* aKeyboard);
  wl_keyboard* GetKeyboard() { return mKeyboard; }
  void ClearKeyboard();

  void SetSeat(wl_seat* aSeat, int aSeatId);
  wl_seat* GetSeat() { return mSeat; }
  void RemoveSeat(int aSeatId);

  void SetCompositor(wl_compositor* aCompositor);
  void SetSubcompositor(wl_subcompositor* aSubcompositor);
  void SetDataDeviceManager(wl_data_device_manager* aDataDeviceManager);
  void SetIdleInhibitManager(zwp_idle_inhibit_manager_v1* aIdleInhibitManager);
  void SetViewporter(wp_viewporter* aViewporter);
  void SetRelativePointerManager(
      zwp_relative_pointer_manager_v1* aRelativePointerManager);
  void SetPointerConstraints(zwp_pointer_constraints_v1* aPointerConstraints);
  void SetPointerGestures(zwp_pointer_gestures_v1* aPointerGestures);
  void SetDmabuf(zwp_linux_dmabuf_v1* aDmabuf);
  void SetXdgActivation(xdg_activation_v1* aXdgActivation);
  void SetXdgDbusAnnotationManager(
      xdg_dbus_annotation_manager_v1* aXdgDbusAnnotationManager);
  void SetFractionalScaleManager(wp_fractional_scale_manager_v1* aManager) {
    mFractionalScaleManager = aManager;
  }
  void EnablePrimarySelection() { mIsPrimarySelectionEnabled = true; }

  void SetColorManager(xx_color_manager_v4* aColorManager);
  xx_color_manager_v4* GetColorManager() const { return mColorManager; }
  void SetCMSupportedFeature(uint32_t aFeature);
  void SetCMSupportedTFNamed(uint32_t aTF);
  void SetCMSupportedPrimariesNamed(uint32_t aPrimaries);
  bool IsHDREnabled() const {
    return mColorManagerSupportedFeature.mParametric;
  }

  ~nsWaylandDisplay();

 private:
  PRThread* mThreadId = nullptr;
  wl_registry* mRegistry = nullptr;
  wl_display* mDisplay = nullptr;
  wl_compositor* mCompositor = nullptr;
  wl_subcompositor* mSubcompositor = nullptr;
  wl_shm* mShm = nullptr;
  wl_seat* mSeat = nullptr;
  int mSeatId = -1;
  wl_keyboard* mKeyboard = nullptr;
  wl_pointer* mPointer = nullptr;
  zwp_idle_inhibit_manager_v1* mIdleInhibitManager = nullptr;
  zwp_relative_pointer_manager_v1* mRelativePointerManager = nullptr;
  zwp_pointer_constraints_v1* mPointerConstraints = nullptr;
  zwp_pointer_gestures_v1* mPointerGestures = nullptr;
  zwp_pointer_gesture_hold_v1* mPointerGestureHold = nullptr;
  wp_viewporter* mViewporter = nullptr;
  zwp_linux_dmabuf_v1* mDmabuf = nullptr;
  xdg_activation_v1* mXdgActivation = nullptr;
  xdg_dbus_annotation_manager_v1* mXdgDbusAnnotationManager = nullptr;
  wp_fractional_scale_manager_v1* mFractionalScaleManager = nullptr;
  xx_color_manager_v4* mColorManager = nullptr;

  struct ColorManagerSupportedFeature {
    bool mICC = false;
    bool mParametric = false;
    bool mPrimaries = false;
    bool mFTPower = false;
    bool mLuminances = false;
    bool mDisplayPrimaries = false;
  } mColorManagerSupportedFeature;

  int mSupportedTransfer[sColorTransfersNum] = {};
  int mSupportedPrimaries[sColorPrimariesNum] = {};

  bool mExplicitSync = false;
  bool mIsPrimarySelectionEnabled = false;
};

wl_display* WaylandDisplayGetWLDisplay();
nsWaylandDisplay* WaylandDisplayGet();
void WaylandDisplayRelease();
void WlCompositorCrashHandler();

}  // namespace mozilla::widget

template <class T>
static inline T* WaylandRegistryBind(struct wl_registry* wl_registry,
                                     uint32_t name,
                                     const struct wl_interface* interface,
                                     uint32_t version) {
  struct wl_proxy* id;

  // When libwayland-client does not provide this symbol, it will be
  // linked to the fallback in libmozwayland, which returns NULL.
  id = wl_proxy_marshal_constructor_versioned(
      (struct wl_proxy*)wl_registry, WL_REGISTRY_BIND, interface, version, name,
      interface->name, version, nullptr);

  if (id == nullptr) {
    id = wl_proxy_marshal_constructor((struct wl_proxy*)wl_registry,
                                      WL_REGISTRY_BIND, interface, name,
                                      interface->name, version, nullptr);
  }

  return reinterpret_cast<T*>(id);
}

#endif  // __MOZ_WAYLAND_DISPLAY_H__
