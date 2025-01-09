/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * MozContainerWayland is a wrapper over MozContainer which manages
 * WaylandSurface for nsWindow.
 *
 * The widget scheme looks like:
 *
 *   ---------------------------------------------------------
 *  |  mShell Gtk widget (contains wl_surface owned by Gtk+)  |
 *  |                                                         |
 *  |  ---------------------------------------------------    |
 *  | | mContainer (contains wl_surface owned by Gtk+)    |   |
 *  | |                                                   |   |
 *  | |  ---------------------------------------------    |   |
 *  | | | wl_subsurface (owned by WaylandSurface)     |   |   |
 *  | | |                                             |   |   |
 *  | | |                                             |   |   |
 *  | | |                                             |   |   |
 *  | |  ---------------------------------------------    |   |
 *  |  ---------------------------------------------------    |
 *   ---------------------------------------------------------
 *
 *  We draw to wl_subsurface managed by WaylandSurface/MozContainerWayland.
 *  We need to wait until wl_surface of mContainer is created
 *  and then we create and attach our wl_subsurface to it.
 *
 *  First wl_subsurface creation has these steps:
 *
 *  1) moz_container_wayland_size_allocate() handler is called when
 *     mContainer size/position is known.
 *     It calls moz_container_wayland_surface_create_locked(), registers
 *     a frame callback handler
 *     moz_container_wayland_frame_callback_handler().
 *
 *  2) moz_container_wayland_frame_callback_handler() is called
 *     when wl_surface owned by mozContainer is ready.
 *     We call initial_draw_cbs() handler and we can create our wl_subsurface
 *     on top of wl_surface owned by mozContainer.
 *
 *  When MozContainer hides/show again, moz_container_wayland_size_allocate()
 *  handler may not be called as MozContainer size is set. So after first
 *  show/hide sequence use moz_container_wayland_map_event() to create
 *  wl_subsurface of MozContainer.
 */

#include "MozContainer.h"

#include <dlfcn.h>
#include <glib.h>
#include <stdio.h>
#include <wayland-egl.h>

#include "mozilla/gfx/gfxVars.h"
#include "mozilla/StaticPrefs_widget.h"
#include "nsGtkUtils.h"
#include "nsWaylandDisplay.h"
#include "base/task.h"

#undef LOGWAYLAND
#undef LOGCONTAINER
#ifdef MOZ_LOGGING
#  include "mozilla/Logging.h"
#  include "nsTArray.h"
#  include "Units.h"
#  include "nsWindow.h"
extern mozilla::LazyLogModule gWidgetWaylandLog;
extern mozilla::LazyLogModule gWidgetLog;
#  define LOGWAYLAND(...) \
    MOZ_LOG(gWidgetWaylandLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
#  define LOGCONTAINER(...) \
    MOZ_LOG(gWidgetLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
#else
#  define LOGWAYLAND(...)
#  define LOGCONTAINER(...)
#endif /* MOZ_LOGGING */

using namespace mozilla;
using namespace mozilla::widget;

static bool moz_container_wayland_ensure_surface(
    MozContainer* container, gfx::IntPoint* aPosition = nullptr);

// Invalidate gtk wl_surface to commit changes to wl_subsurface.
// wl_subsurface changes are effective when parent surface is commited.
static void moz_container_wayland_invalidate(MozContainer* container) {
  LOGWAYLAND("moz_container_wayland_invalidate [%p]\n",
             (void*)moz_container_get_nsWindow(container));

  GdkWindow* window = gtk_widget_get_window(GTK_WIDGET(container));
  if (!window) {
    LOGWAYLAND("    Failed - missing GdkWindow!\n");
    return;
  }
  gdk_window_invalidate_rect(window, nullptr, true);
}

// This is called from layout/compositor code only with
// size equal to GL rendering context.

// Return false if scale factor doesn't match buffer size.
// We need to skip painting in such case do avoid Wayland compositor freaking.
bool moz_container_wayland_egl_window_set_size(MozContainer* container,
                                               nsIntSize aScaledSize) {
  return MOZ_WL_SURFACE(container)->SetEGLWindowSize(aScaledSize);
}

void moz_container_wayland_add_or_fire_initial_draw_callback(
    MozContainer* container, const std::function<void(void)>& initial_draw_cb) {
  MOZ_WL_SURFACE(container)->AddOrFireReadyToDrawCallback(initial_draw_cb);
}

void moz_container_wayland_unmap(GtkWidget* widget) {
  g_return_if_fail(IS_MOZ_CONTAINER(widget));

  // Unmap MozContainer first so we can remove our resources
  moz_container_unmap(widget);

  LOGCONTAINER("%s [%p]\n", __FUNCTION__,
               (void*)moz_container_get_nsWindow(MOZ_CONTAINER(widget)));

  WaylandSurface* surface = MOZ_WL_SURFACE(MOZ_CONTAINER(widget));
  // MozContainer map/unmap is processed on main thread only
  // so we don't need to lock WaylandSurface here.
  if (surface->IsMapped()) {
    surface->RunUnmapCallback();
  }

  WaylandSurfaceLock lock(surface);
  if (surface->IsPendingGdkCleanup()) {
    surface->GdkCleanUpLocked(lock);
  }
  surface->UnmapLocked(lock);
}

gboolean moz_container_wayland_map_event(GtkWidget* widget,
                                         GdkEventAny* event) {
  LOGCONTAINER("%s [%p]\n", __FUNCTION__,
               (void*)moz_container_get_nsWindow(MOZ_CONTAINER(widget)));

  // Return early if we're not mapped. Gtk may send bogus map_event signal
  // to unmapped widgets (see Bug 1875369).
  if (!gtk_widget_get_mapped(widget)) {
    return false;
  }

  // Make sure we're on main thread as we can't lock mozContainer here
  // due to moz_container_wayland_add_or_fire_initial_draw_callback() call
  // below.
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());

  // Set waiting_to_show flag. It means the mozcontainer is cofigured/mapped
  // and it's supposed to be visible. *But* it's really visible when we get
  // moz_container_wayland_add_or_fire_initial_draw_callback() which means
  // wayland compositor makes it live.
  MOZ_WL_CONTAINER(widget)->waiting_to_show = true;
  MozContainer* container = MOZ_CONTAINER(widget);
  MOZ_WL_SURFACE(container)->AddOrFireReadyToDrawCallback(
      [container]() -> void {
        LOGCONTAINER(
            "[%p] moz_container_wayland_add_or_fire_initial_draw_callback set "
            "visible",
            moz_container_get_nsWindow(container));
        moz_container_wayland_clear_waiting_to_show_flag(container);
      });

  // Don't create wl_subsurface in map_event when it's already created or
  // if we create it for the first time.
  if (MOZ_WL_SURFACE(container)->IsMapped() ||
      MOZ_WL_CONTAINER(container)->before_first_size_alloc) {
    return false;
  }

  return moz_container_wayland_ensure_surface(container);
}

void moz_container_wayland_size_allocate(GtkWidget* widget,
                                         GtkAllocation* allocation) {
  GtkAllocation tmp_allocation;

  g_return_if_fail(IS_MOZ_CONTAINER(widget));

  LOGCONTAINER("moz_container_wayland_size_allocate [%p] %d,%d -> %d x %d\n",
               (void*)moz_container_get_nsWindow(MOZ_CONTAINER(widget)),
               allocation->x, allocation->y, allocation->width,
               allocation->height);

  /* short circuit if you can */
  gtk_widget_get_allocation(widget, &tmp_allocation);
  if (tmp_allocation.x == allocation->x && tmp_allocation.y == allocation->y &&
      tmp_allocation.width == allocation->width &&
      tmp_allocation.height == allocation->height) {
    return;
  }
  gtk_widget_set_allocation(widget, allocation);

  if (gtk_widget_get_has_window(widget) && gtk_widget_get_realized(widget)) {
    gdk_window_move_resize(gtk_widget_get_window(widget), allocation->x,
                           allocation->y, allocation->width,
                           allocation->height);
    // We need to position our subsurface according to GdkWindow
    // when offset changes (GdkWindow is maximized for instance).
    // see gtk-clutter-embed.c for reference.
    gfx::IntPoint position(allocation->x, allocation->y);
    moz_container_wayland_ensure_surface(MOZ_CONTAINER(widget), &position);
    MOZ_WL_CONTAINER(widget)->before_first_size_alloc = false;
  }
}

static bool moz_container_wayland_ensure_surface(MozContainer* container,
                                                 gfx::IntPoint* aPosition) {
  WaylandSurface* surface = MOZ_WL_SURFACE(container);
  WaylandSurfaceLock lock(surface);

  // We're already mapped, only move surface and quit.
  if (surface->IsMapped()) {
    if (aPosition) {
      surface->MoveLocked(lock, *aPosition);
    }
    moz_container_wayland_invalidate(container);
    return true;
  }

  LOGWAYLAND("%s [%p]\n", __FUNCTION__,
             (void*)moz_container_get_nsWindow(container));

  GdkWindow* gdkWindow = gtk_widget_get_window(GTK_WIDGET(container));
  MOZ_DIAGNOSTIC_ASSERT(gdkWindow);

  wl_surface* parentSurface = gdk_wayland_window_get_wl_surface(gdkWindow);
  if (!parentSurface) {
    LOGWAYLAND("    Failed - missing parent surface!");
    return false;
  }
  LOGWAYLAND("    gtk wl_surface %p ID %d\n", (void*)parentSurface,
             wl_proxy_get_id((struct wl_proxy*)parentSurface));

  // Try to guess subsurface offset to avoid potential flickering.
  gfx::IntPoint subsurfacePosition;
  if (aPosition) {
    subsurfacePosition = *aPosition;
  } else {
    int x = 0, y = 0;
    moz_container_get_nsWindow(container)->GetCSDDecorationOffset(&x, &y);
    subsurfacePosition = gfx::IntPoint(x, y);
  }

  if (!surface->MapLocked(lock, parentSurface, subsurfacePosition,
                          MOZ_WL_CONTAINER(container)->commit_to_parent)) {
    return false;
  }

  surface->AddOpaqueSurfaceHandlerLocked(lock, gdkWindow,
                                         /* aRegisterCommitHandler */ true);

  nsWindow* window = moz_container_get_nsWindow(container);
  MOZ_RELEASE_ASSERT(window);

  GtkWindow* parent =
      gtk_window_get_transient_for(GTK_WINDOW(window->GetGtkWidget()));
  if (parent) {
    nsWindow* parentWindow =
        static_cast<nsWindow*>(g_object_get_data(G_OBJECT(parent), "nsWindow"));
    MOZ_DIAGNOSTIC_ASSERT(parentWindow);
    surface->SetParentLocked(lock,
                             MOZ_WL_SURFACE(parentWindow->GetMozContainer()));
  }

  bool fractionalScale = false;
  if (StaticPrefs::widget_wayland_fractional_scale_enabled_AtStartup()) {
    fractionalScale = surface->EnableFractionalScaleLocked(
        lock,
        [win = RefPtr{window}]() {
          win->RefreshScale(/* aRefreshScreen */ true);
        },
        /* aManageViewport */ true);
  }

  if (!fractionalScale) {
    surface->EnableCeiledScaleLocked(lock);
  }

  if (MOZ_WL_CONTAINER(container)->opaque_region_needs_updates) {
    surface->SetOpaqueRegionLocked(lock,
                                   window->GetOpaqueRegion().ToUnknownRegion());
  }
  surface->DisableUserInputLocked(lock);

  // Commit eplicitly now as moz_container_wayland_invalidate() initiated
  // widget repaint
  surface->CommitLocked(lock);

  moz_container_wayland_invalidate(container);
  return true;
}

struct wl_egl_window* moz_container_wayland_get_egl_window(
    MozContainer* container) {
  LOGCONTAINER("%s [%p] mapped %d eglwindow %d", __FUNCTION__,
               (void*)moz_container_get_nsWindow(container),
               MOZ_WL_SURFACE(container)->IsMapped(),
               MOZ_WL_SURFACE(container)->HasEGLWindow());

  if (!MOZ_WL_SURFACE(container)->IsMapped()) {
    return nullptr;
  }

  // TODO: Get size from bounds instead of GdkWindow?
  // We may be in rendering/compositor thread here.
  GdkWindow* window = gtk_widget_get_window(GTK_WIDGET(container));
  nsIntSize unscaledSize(gdk_window_get_width(window),
                         gdk_window_get_height(window));
  return MOZ_WL_SURFACE(container)->GetEGLWindow(unscaledSize);
}

gboolean moz_container_wayland_has_egl_window(MozContainer* container) {
  return MOZ_WL_SURFACE(container)->HasEGLWindow();
}

void moz_container_wayland_update_opaque_region(MozContainer* container) {
  MOZ_WL_CONTAINER(container)->opaque_region_needs_updates = true;

  // When GL compositor / WebRender is used,
  // moz_container_wayland_get_egl_window() is called only once when window
  // is created or resized so update opaque region now.
  if (MOZ_WL_SURFACE(container)->HasEGLWindow()) {
    MOZ_WL_CONTAINER(container)->opaque_region_needs_updates = false;
    nsWindow* window = moz_container_get_nsWindow(container);
    MOZ_WL_SURFACE(container)->SetOpaqueRegion(
        window->GetOpaqueRegion().ToUnknownRegion());
  }
}

gboolean moz_container_wayland_can_draw(MozContainer* container) {
  return MOZ_WL_SURFACE(container)->IsReadyToDraw();
}

double moz_container_wayland_get_scale(MozContainer* container) {
  nsWindow* window = moz_container_get_nsWindow(container);
  return window ? window->FractionalScaleFactor() : 1.0;
}

void moz_container_wayland_set_commit_to_parent(MozContainer* container) {
  MOZ_DIAGNOSTIC_ASSERT(!MOZ_WL_SURFACE(container)->IsMapped());
  MOZ_WL_CONTAINER(container)->commit_to_parent = true;
}

bool moz_container_wayland_is_commiting_to_parent(MozContainer* container) {
  return MOZ_WL_CONTAINER(container)->commit_to_parent;
}

bool moz_container_wayland_is_waiting_to_show(MozContainer* container) {
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
  return MOZ_WL_CONTAINER(container)->waiting_to_show;
}

void moz_container_wayland_clear_waiting_to_show_flag(MozContainer* container) {
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
  MOZ_WL_CONTAINER(container)->waiting_to_show = false;
}
