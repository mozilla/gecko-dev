/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWaylandDisplay.h"

#include <dlfcn.h>

#include "base/message_loop.h"    // for MessageLoop
#include "base/task.h"            // for NewRunnableMethod, etc
#include "mozilla/gfx/Logging.h"  // for gfxCriticalNote
#include "mozilla/StaticMutex.h"
#include "mozilla/Array.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/Sprintf.h"
#include "WidgetUtilsGtk.h"
#include "nsGtkKeyUtils.h"
#include "nsWindow.h"

namespace mozilla::widget {

static nsWaylandDisplay* gWaylandDisplay;

void WaylandDisplayRelease() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread(),
                     "WaylandDisplay can be released in main thread only!");
  if (!gWaylandDisplay) {
    return;
  }
  delete gWaylandDisplay;
  gWaylandDisplay = nullptr;
}

wl_display* WaylandDisplayGetWLDisplay() {
  GdkDisplay* disp = gdk_display_get_default();
  if (!GdkIsWaylandDisplay(disp)) {
    return nullptr;
  }
  return gdk_wayland_display_get_wl_display(disp);
}

nsWaylandDisplay* WaylandDisplayGet() {
  if (!gWaylandDisplay) {
    MOZ_RELEASE_ASSERT(NS_IsMainThread(),
                       "WaylandDisplay can be created in main thread only!");
    wl_display* waylandDisplay = WaylandDisplayGetWLDisplay();
    if (!waylandDisplay) {
      return nullptr;
    }
    gWaylandDisplay = new nsWaylandDisplay(waylandDisplay);
  }
  return gWaylandDisplay;
}

void nsWaylandDisplay::SetShm(wl_shm* aShm) { mShm = aShm; }

struct PointerState {
  wl_surface* surface;

  nsWindow* GetWindow() {
    GdkWindow* window =
        static_cast<GdkWindow*>(wl_surface_get_user_data(surface));
    return window ? static_cast<nsWindow*>(
                        g_object_get_data(G_OBJECT(window), "nsWindow"))
                  : nullptr;
  }
} sPointerState;

static void gesture_hold_begin(void* data,
                               struct zwp_pointer_gesture_hold_v1* hold,
                               uint32_t serial, uint32_t time,
                               struct wl_surface* surface, uint32_t fingers) {
  RefPtr<nsWindow> window = sPointerState.GetWindow();
  if (!window) {
    return;
  }
  window->OnTouchpadHoldEvent(GDK_TOUCHPAD_GESTURE_PHASE_BEGIN, time, fingers);
}

static void gesture_hold_end(void* data,
                             struct zwp_pointer_gesture_hold_v1* hold,
                             uint32_t serial, uint32_t time,
                             int32_t cancelled) {
  RefPtr<nsWindow> window = sPointerState.GetWindow();
  if (!window) {
    return;
  }
  window->OnTouchpadHoldEvent(cancelled ? GDK_TOUCHPAD_GESTURE_PHASE_CANCEL
                                        : GDK_TOUCHPAD_GESTURE_PHASE_END,
                              time, 0);
}

static const struct zwp_pointer_gesture_hold_v1_listener gesture_hold_listener =
    {gesture_hold_begin, gesture_hold_end};

static void pointer_handle_enter(void* data, struct wl_pointer* pointer,
                                 uint32_t serial, struct wl_surface* surface,
                                 wl_fixed_t sx, wl_fixed_t sy) {
  sPointerState.surface = surface;
}

static void pointer_handle_leave(void* data, struct wl_pointer* pointer,
                                 uint32_t serial, struct wl_surface* surface) {
  sPointerState.surface = nullptr;
}

static void pointer_handle_motion(void* data, struct wl_pointer* pointer,
                                  uint32_t time, wl_fixed_t sx, wl_fixed_t sy) {
}

static void pointer_handle_button(void* data, struct wl_pointer* pointer,
                                  uint32_t serial, uint32_t time,
                                  uint32_t button, uint32_t state) {}

static void pointer_handle_axis(void* data, struct wl_pointer* pointer,
                                uint32_t time, uint32_t axis,
                                wl_fixed_t value) {}

static void pointer_handle_frame(void* data, struct wl_pointer* pointer) {}

static void pointer_handle_axis_source(
    void* data, struct wl_pointer* pointer,
    /*enum wl_pointer_axis_source */ uint32_t source) {}

static void pointer_handle_axis_stop(void* data, struct wl_pointer* pointer,
                                     uint32_t time, uint32_t axis) {}

static void pointer_handle_axis_discrete(void* data, struct wl_pointer* pointer,
                                         uint32_t axis, int32_t value) {}

static void pointer_handle_axis_value120(void* data, struct wl_pointer* pointer,
                                         uint32_t axis, int32_t value) {}

static const struct moz_wl_pointer_listener pointer_listener = {
    pointer_handle_enter,         pointer_handle_leave,
    pointer_handle_motion,        pointer_handle_button,
    pointer_handle_axis,          pointer_handle_frame,
    pointer_handle_axis_source,   pointer_handle_axis_stop,
    pointer_handle_axis_discrete, pointer_handle_axis_value120,
};

void nsWaylandDisplay::SetPointer(wl_pointer* aPointer) {
  if (!mPointerGestures || wl_proxy_get_version((struct wl_proxy*)aPointer) <
                               WL_POINTER_RELEASE_SINCE_VERSION) {
    return;
  }
  MOZ_DIAGNOSTIC_ASSERT(!mPointer);
  mPointer = aPointer;
  wl_pointer_add_listener(mPointer,
                          (const wl_pointer_listener*)&pointer_listener, this);

  mPointerGestureHold =
      zwp_pointer_gestures_v1_get_hold_gesture(mPointerGestures, mPointer);
  zwp_pointer_gesture_hold_v1_set_user_data(mPointerGestureHold, this);
  zwp_pointer_gesture_hold_v1_add_listener(mPointerGestureHold,
                                           &gesture_hold_listener, this);
}

void nsWaylandDisplay::RemovePointer() {
  wl_pointer_release(mPointer);
  mPointer = nullptr;
}

static void seat_handle_capabilities(void* data, struct wl_seat* seat,
                                     unsigned int caps) {
  auto* display = static_cast<nsWaylandDisplay*>(data);
  if (!display) {
    return;
  }

  if ((caps & WL_SEAT_CAPABILITY_POINTER) && !display->GetPointer()) {
    display->SetPointer(wl_seat_get_pointer(seat));
  } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && display->GetPointer()) {
    display->RemovePointer();
  }

  wl_keyboard* keyboard = display->GetKeyboard();
  if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !keyboard) {
    display->SetKeyboard(wl_seat_get_keyboard(seat));
  } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && keyboard) {
    display->ClearKeyboard();
  }
}

static void seat_handle_name(void* data, struct wl_seat* seat,
                             const char* name) {
  /* We don't care about the name. */
}

static const struct wl_seat_listener seat_listener = {
    seat_handle_capabilities,
    seat_handle_name,
};

void nsWaylandDisplay::SetSeat(wl_seat* aSeat, int aSeatId) {
  mSeat = aSeat;
  mSeatId = aSeatId;
  wl_seat_add_listener(aSeat, &seat_listener, this);
}

void nsWaylandDisplay::RemoveSeat(int aSeatId) {
  if (mSeatId == aSeatId) {
    mSeat = nullptr;
    mSeatId = -1;
  }
}

/* This keymap routine is derived from weston-2.0.0/clients/simple-im.c
 */
static void keyboard_handle_keymap(void* data, struct wl_keyboard* wl_keyboard,
                                   uint32_t format, int fd, uint32_t size) {
  KeymapWrapper::HandleKeymap(format, fd, size);
}

static void keyboard_handle_enter(void* data, struct wl_keyboard* keyboard,
                                  uint32_t serial, struct wl_surface* surface,
                                  struct wl_array* keys) {
  KeymapWrapper::SetFocusIn(surface, serial);
}

static void keyboard_handle_leave(void* data, struct wl_keyboard* keyboard,
                                  uint32_t serial, struct wl_surface* surface) {
  KeymapWrapper::SetFocusOut(surface);
}

static void keyboard_handle_key(void* data, struct wl_keyboard* keyboard,
                                uint32_t serial, uint32_t time, uint32_t key,
                                uint32_t state) {}
static void keyboard_handle_modifiers(void* data, struct wl_keyboard* keyboard,
                                      uint32_t serial, uint32_t mods_depressed,
                                      uint32_t mods_latched,
                                      uint32_t mods_locked, uint32_t group) {}
static void keyboard_handle_repeat_info(void* data,
                                        struct wl_keyboard* keyboard,
                                        int32_t rate, int32_t delay) {}

static const struct wl_keyboard_listener keyboard_listener = {
    keyboard_handle_keymap,    keyboard_handle_enter,
    keyboard_handle_leave,     keyboard_handle_key,
    keyboard_handle_modifiers, keyboard_handle_repeat_info};

void nsWaylandDisplay::SetKeyboard(wl_keyboard* aKeyboard) {
  MOZ_ASSERT(aKeyboard);
  MOZ_DIAGNOSTIC_ASSERT(!mKeyboard);
  mKeyboard = aKeyboard;
  wl_keyboard_add_listener(mKeyboard, &keyboard_listener, nullptr);
}

void nsWaylandDisplay::ClearKeyboard() {
  if (mKeyboard) {
    wl_keyboard_destroy(mKeyboard);
    mKeyboard = nullptr;
  }
}

void nsWaylandDisplay::SetCompositor(wl_compositor* aCompositor) {
  mCompositor = aCompositor;
}

void nsWaylandDisplay::SetSubcompositor(wl_subcompositor* aSubcompositor) {
  mSubcompositor = aSubcompositor;
}

void nsWaylandDisplay::SetIdleInhibitManager(
    zwp_idle_inhibit_manager_v1* aIdleInhibitManager) {
  mIdleInhibitManager = aIdleInhibitManager;
}

void nsWaylandDisplay::SetViewporter(wp_viewporter* aViewporter) {
  mViewporter = aViewporter;
}

void nsWaylandDisplay::SetRelativePointerManager(
    zwp_relative_pointer_manager_v1* aRelativePointerManager) {
  mRelativePointerManager = aRelativePointerManager;
}

void nsWaylandDisplay::SetPointerConstraints(
    zwp_pointer_constraints_v1* aPointerConstraints) {
  mPointerConstraints = aPointerConstraints;
}

void nsWaylandDisplay::SetPointerGestures(
    zwp_pointer_gestures_v1* aPointerGestures) {
  mPointerGestures = aPointerGestures;
}

void nsWaylandDisplay::SetDmabuf(zwp_linux_dmabuf_v1* aDmabuf) {
  mDmabuf = aDmabuf;
}

void nsWaylandDisplay::SetXdgActivation(xdg_activation_v1* aXdgActivation) {
  mXdgActivation = aXdgActivation;
}

void nsWaylandDisplay::SetXdgDbusAnnotationManager(
    xdg_dbus_annotation_manager_v1* aXdgDbusAnnotationManager) {
  mXdgDbusAnnotationManager = aXdgDbusAnnotationManager;
}

static void global_registry_handler(void* data, wl_registry* registry,
                                    uint32_t id, const char* interface,
                                    uint32_t version) {
  auto* display = static_cast<nsWaylandDisplay*>(data);
  if (!display) {
    return;
  }

  nsDependentCString iface(interface);
  if (iface.EqualsLiteral("wl_shm")) {
    auto* shm = WaylandRegistryBind<wl_shm>(registry, id, &wl_shm_interface, 1);
    display->SetShm(shm);
  } else if (iface.EqualsLiteral("zwp_idle_inhibit_manager_v1")) {
    auto* idle_inhibit_manager =
        WaylandRegistryBind<zwp_idle_inhibit_manager_v1>(
            registry, id, &zwp_idle_inhibit_manager_v1_interface, 1);
    display->SetIdleInhibitManager(idle_inhibit_manager);
  } else if (iface.EqualsLiteral("zwp_relative_pointer_manager_v1")) {
    auto* relative_pointer_manager =
        WaylandRegistryBind<zwp_relative_pointer_manager_v1>(
            registry, id, &zwp_relative_pointer_manager_v1_interface, 1);
    display->SetRelativePointerManager(relative_pointer_manager);
  } else if (iface.EqualsLiteral("zwp_pointer_constraints_v1")) {
    auto* pointer_constraints = WaylandRegistryBind<zwp_pointer_constraints_v1>(
        registry, id, &zwp_pointer_constraints_v1_interface, 1);
    display->SetPointerConstraints(pointer_constraints);
  } else if (iface.EqualsLiteral("wl_compositor")) {
    auto* compositor = WaylandRegistryBind<wl_compositor>(
        registry, id, &wl_compositor_interface,
        WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION);
    display->SetCompositor(compositor);
  } else if (iface.EqualsLiteral("wl_subcompositor")) {
    auto* subcompositor = WaylandRegistryBind<wl_subcompositor>(
        registry, id, &wl_subcompositor_interface, 1);
    display->SetSubcompositor(subcompositor);
  } else if (iface.EqualsLiteral("wp_viewporter")) {
    auto* viewporter = WaylandRegistryBind<wp_viewporter>(
        registry, id, &wp_viewporter_interface, 1);
    display->SetViewporter(viewporter);
  } else if (iface.EqualsLiteral("zwp_linux_dmabuf_v1") && version > 2) {
    auto* dmabuf = WaylandRegistryBind<zwp_linux_dmabuf_v1>(
        registry, id, &zwp_linux_dmabuf_v1_interface, 3);
    display->SetDmabuf(dmabuf);
  } else if (iface.EqualsLiteral("xdg_activation_v1")) {
    auto* activation = WaylandRegistryBind<xdg_activation_v1>(
        registry, id, &xdg_activation_v1_interface, 1);
    display->SetXdgActivation(activation);
  } else if (iface.EqualsLiteral("xdg_dbus_annotation_manager_v1")) {
    auto* annotationManager =
        WaylandRegistryBind<xdg_dbus_annotation_manager_v1>(
            registry, id, &xdg_dbus_annotation_manager_v1_interface, 1);
    display->SetXdgDbusAnnotationManager(annotationManager);
  } else if (iface.EqualsLiteral("wl_seat")) {
    auto* seat = WaylandRegistryBind<wl_seat>(registry, id, &wl_seat_interface,
                                              WL_POINTER_RELEASE_SINCE_VERSION);
    if (seat) {
      display->SetSeat(seat, id);
    }
  } else if (iface.EqualsLiteral("wp_fractional_scale_manager_v1")) {
    auto* manager = WaylandRegistryBind<wp_fractional_scale_manager_v1>(
        registry, id, &wp_fractional_scale_manager_v1_interface, 1);
    display->SetFractionalScaleManager(manager);
  } else if (iface.EqualsLiteral("gtk_primary_selection_device_manager") ||
             iface.EqualsLiteral("zwp_primary_selection_device_manager_v1")) {
    display->EnablePrimarySelection();
  } else if (iface.EqualsLiteral("zwp_pointer_gestures_v1")) {
    auto* gestures = WaylandRegistryBind<zwp_pointer_gestures_v1>(
        registry, id, &zwp_pointer_gestures_v1_interface,
        ZWP_POINTER_GESTURES_V1_GET_HOLD_GESTURE_SINCE_VERSION);
    if (gestures) {
      display->SetPointerGestures(gestures);
    }
  }
}

static void global_registry_remover(void* data, wl_registry* registry,
                                    uint32_t id) {
  auto* display = static_cast<nsWaylandDisplay*>(data);
  if (!display) {
    return;
  }
  display->RemoveSeat(id);
}

static const struct wl_registry_listener registry_listener = {
    global_registry_handler, global_registry_remover};

nsWaylandDisplay::~nsWaylandDisplay() = default;

static void WlLogHandler(const char* format, va_list args) {
  char error[1000];
  VsprintfLiteral(error, format, args);
  gfxCriticalNote << "Wayland protocol error: " << error;

  // See Bug 1826583 and Bug 1844653 for reference.
  // "warning: queue %p destroyed while proxies still attached" and variants
  // like "zwp_linux_dmabuf_feedback_v1@%d still attached" are exceptions on
  // Wayland and non-fatal. They are triggered in certain versions of Mesa or
  // the proprietary Nvidia driver and we don't want to crash because of them.
  if (strstr(error, "still attached")) {
    return;
  }

  MOZ_CRASH_UNSAFE(error);
}

nsWaylandDisplay::nsWaylandDisplay(wl_display* aDisplay)
    : mThreadId(PR_GetCurrentThread()), mDisplay(aDisplay) {
  // GTK sets the log handler on display creation, thus we overwrite it here
  // in a similar fashion
  wl_log_set_handler_client(WlLogHandler);

  mRegistry = wl_display_get_registry(mDisplay);
  wl_registry_add_listener(mRegistry, &registry_listener, this);
  wl_display_roundtrip(mDisplay);
  wl_display_roundtrip(mDisplay);

  // Check we have critical Wayland interfaces.
  // Missing ones indicates a compositor bug and we can't continue.
  MOZ_RELEASE_ASSERT(GetShm(), "We're missing shm interface!");
  MOZ_RELEASE_ASSERT(GetCompositor(), "We're missing compositor interface!");
  MOZ_RELEASE_ASSERT(GetSubcompositor(),
                     "We're missing subcompositor interface!");
}

}  // namespace mozilla::widget
