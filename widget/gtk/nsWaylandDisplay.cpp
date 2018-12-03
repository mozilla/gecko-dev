/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWaylandDisplay.h"

#include "base/message_loop.h"  // for MessageLoop
#include "base/task.h"          // for NewRunnableMethod, etc
#include "mozilla/StaticMutex.h"

namespace mozilla {
namespace widget {

#define MAX_DISPLAY_CONNECTIONS 2

static nsWaylandDisplay *gWaylandDisplays[MAX_DISPLAY_CONNECTIONS];
static StaticMutex gWaylandDisplaysMutex;

// Each thread which is using wayland connection (wl_display) has to operate
// its own wl_event_queue. Main Firefox thread wl_event_queue is handled
// by Gtk main loop, other threads/wl_event_queue has to be handled by us.
//
// nsWaylandDisplay is our interface to wayland compositor. It provides wayland
// global objects as we need (wl_display, wl_shm) and operates wl_event_queue on
// compositor (not the main) thread.
static void WaylandDisplayLoop(wl_display *aDisplay);

// Get WaylandDisplay for given wl_display and actual calling thread.
static nsWaylandDisplay *WaylandDisplayGetLocked(wl_display *aDisplay,
                                                 const StaticMutexAutoLock &) {
  for (auto &display : gWaylandDisplays) {
    if (display && display->Matches(aDisplay)) {
      NS_ADDREF(display);
      return display;
    }
  }

  for (auto &display : gWaylandDisplays) {
    if (display == nullptr) {
      display = new nsWaylandDisplay(aDisplay);
      NS_ADDREF(display);
      return display;
    }
  }

  MOZ_CRASH("There's too many wayland display conections!");
  return nullptr;
}

nsWaylandDisplay *WaylandDisplayGet(GdkDisplay *aGdkDisplay) {
  if (!aGdkDisplay) {
    aGdkDisplay = gdk_display_get_default();
  }

  // Available as of GTK 3.8+
  static auto sGdkWaylandDisplayGetWlDisplay = (wl_display * (*)(GdkDisplay *))
      dlsym(RTLD_DEFAULT, "gdk_wayland_display_get_wl_display");

  wl_display *display = sGdkWaylandDisplayGetWlDisplay(aGdkDisplay);

  StaticMutexAutoLock lock(gWaylandDisplaysMutex);
  return WaylandDisplayGetLocked(display, lock);
}

static bool WaylandDisplayReleaseLocked(nsWaylandDisplay *aDisplay,
                                        const StaticMutexAutoLock &) {
  for (auto &display : gWaylandDisplays) {
    if (display == aDisplay) {
      int rc = display->Release();
      if (rc == 0) {
        display = nullptr;
      }
      return true;
    }
  }
  MOZ_ASSERT(false, "Missing nsWaylandDisplay for this thread!");
  return false;
}

void WaylandDisplayRelease(nsWaylandDisplay *aDisplay) {
  StaticMutexAutoLock lock(gWaylandDisplaysMutex);
  WaylandDisplayReleaseLocked(aDisplay, lock);
}

static void WaylandDisplayLoopLocked(wl_display *aDisplay,
                                     const StaticMutexAutoLock &) {
  for (auto &display : gWaylandDisplays) {
    if (display && display->Matches(aDisplay)) {
      if (display->DisplayLoop()) {
        MessageLoop::current()->PostDelayedTask(
            NewRunnableFunction("WaylandDisplayLoop", &WaylandDisplayLoop,
                                aDisplay),
            EVENT_LOOP_DELAY);
      }
      break;
    }
  }
}

static void WaylandDisplayLoop(wl_display *aDisplay) {
  MOZ_ASSERT(!NS_IsMainThread());
  StaticMutexAutoLock lock(gWaylandDisplaysMutex);
  WaylandDisplayLoopLocked(aDisplay, lock);
}

void nsWaylandDisplay::SetShm(wl_shm *aShm) { mShm = aShm; }

void nsWaylandDisplay::SetSubcompositor(wl_subcompositor *aSubcompositor) {
  mSubcompositor = aSubcompositor;
}

void nsWaylandDisplay::SetDataDeviceManager(
    wl_data_device_manager *aDataDeviceManager) {
  mDataDeviceManager = aDataDeviceManager;
}

void nsWaylandDisplay::SetSeat(wl_seat *aSeat) { mSeat = aSeat; }

void nsWaylandDisplay::SetPrimarySelectionDeviceManager(
    gtk_primary_selection_device_manager *aPrimarySelectionDeviceManager) {
  mPrimarySelectionDeviceManager = aPrimarySelectionDeviceManager;
}

static void global_registry_handler(void *data, wl_registry *registry,
                                    uint32_t id, const char *interface,
                                    uint32_t version) {
  auto display = reinterpret_cast<nsWaylandDisplay *>(data);

  if (strcmp(interface, "wl_shm") == 0) {
    auto shm = static_cast<wl_shm *>(
        wl_registry_bind(registry, id, &wl_shm_interface, 1));
    wl_proxy_set_queue((struct wl_proxy *)shm, display->GetEventQueue());
    display->SetShm(shm);
  } else if (strcmp(interface, "wl_data_device_manager") == 0) {
    int data_device_manager_version = MIN(version, 3);
    auto data_device_manager = static_cast<wl_data_device_manager *>(
        wl_registry_bind(registry, id, &wl_data_device_manager_interface,
                         data_device_manager_version));
    wl_proxy_set_queue((struct wl_proxy *)data_device_manager,
                       display->GetEventQueue());
    display->SetDataDeviceManager(data_device_manager);
  } else if (strcmp(interface, "wl_seat") == 0) {
    auto seat = static_cast<wl_seat *>(
        wl_registry_bind(registry, id, &wl_seat_interface, 1));
    wl_proxy_set_queue((struct wl_proxy *)seat, display->GetEventQueue());
    display->SetSeat(seat);
  } else if (strcmp(interface, "gtk_primary_selection_device_manager") == 0) {
    auto primary_selection_device_manager =
        static_cast<gtk_primary_selection_device_manager *>(wl_registry_bind(
            registry, id, &gtk_primary_selection_device_manager_interface, 1));
    wl_proxy_set_queue((struct wl_proxy *)primary_selection_device_manager,
                       display->GetEventQueue());
    display->SetPrimarySelectionDeviceManager(primary_selection_device_manager);
  } else if (strcmp(interface, "wl_subcompositor") == 0) {
    auto subcompositor = static_cast<wl_subcompositor *>(
        wl_registry_bind(registry, id, &wl_subcompositor_interface, 1));
    wl_proxy_set_queue((struct wl_proxy *)subcompositor,
                       display->GetEventQueue());
    display->SetSubcompositor(subcompositor);
  }
}

static void global_registry_remover(void *data, wl_registry *registry,
                                    uint32_t id) {}

static const struct wl_registry_listener registry_listener = {
    global_registry_handler, global_registry_remover};

bool nsWaylandDisplay::DisplayLoop() {
  wl_display_dispatch_queue_pending(mDisplay, mEventQueue);
  return true;
}

bool nsWaylandDisplay::Matches(wl_display *aDisplay) {
  return mThreadId == PR_GetCurrentThread() && aDisplay == mDisplay;
}

NS_IMPL_ISUPPORTS(nsWaylandDisplay, nsISupports);

nsWaylandDisplay::nsWaylandDisplay(wl_display *aDisplay)
    : mThreadId(PR_GetCurrentThread()),
      mDisplay(aDisplay),
      mEventQueue(nullptr),
      mDataDeviceManager(nullptr),
      mSubcompositor(nullptr),
      mSeat(nullptr),
      mShm(nullptr),
      mPrimarySelectionDeviceManager(nullptr) {
  wl_registry *registry = wl_display_get_registry(mDisplay);
  wl_registry_add_listener(registry, &registry_listener, this);

  if (NS_IsMainThread()) {
    // Use default event queue in main thread operated by Gtk+.
    mEventQueue = nullptr;
    wl_display_roundtrip(mDisplay);
    wl_display_roundtrip(mDisplay);
  } else {
    mEventQueue = wl_display_create_queue(mDisplay);
    MessageLoop::current()->PostTask(NewRunnableFunction(
        "WaylandDisplayLoop", &WaylandDisplayLoop, mDisplay));
    wl_proxy_set_queue((struct wl_proxy *)registry, mEventQueue);
    wl_display_roundtrip_queue(mDisplay, mEventQueue);
    wl_display_roundtrip_queue(mDisplay, mEventQueue);
  }
}

nsWaylandDisplay::~nsWaylandDisplay() {
  MOZ_ASSERT(mThreadId == PR_GetCurrentThread());
  // Owned by Gtk+, we don't need to release
  mDisplay = nullptr;

  if (mEventQueue) {
    wl_event_queue_destroy(mEventQueue);
    mEventQueue = nullptr;
  }
}

}  // namespace widget
}  // namespace mozilla
