/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __MOZ_CONTAINER_WAYLAND_H__
#define __MOZ_CONTAINER_WAYLAND_H__

#include <gtk/gtk.h>
#include <functional>
#include <vector>
#include "mozilla/Mutex.h"
#include "WindowSurface.h"
#include "WaylandSurface.h"

/*
 * MozContainer
 *
 * This class serves three purposes in the nsIWidget implementation.
 *
 *   - It provides objects to receive signals from GTK for events on native
 *     windows.
 *
 *   - It provides GdkWindow to draw content on Wayland or when Gtk+ renders
 *     client side decorations to mShell.
 */

/* Workaround for bug at wayland-util.h,
 * present in wayland-devel < 1.12
 */
struct wl_surface;
struct wl_subsurface;

struct _MozContainer;
struct _MozContainerClass;
typedef struct _MozContainer MozContainer;
typedef struct _MozContainerClass MozContainerClass;

struct MozContainerWayland {
  explicit MozContainerWayland(RefPtr<mozilla::widget::WaylandSurface> aSurface)
      : mSurface(aSurface) {}

  RefPtr<mozilla::widget::WaylandSurface> mSurface;
  gboolean commit_to_parent = false;
  gboolean opaque_region_needs_updates = false;
  gboolean before_first_size_alloc = false;
  gboolean waiting_to_show = false;
};

void moz_container_wayland_map(GtkWidget*);
gboolean moz_container_wayland_map_event(GtkWidget*, GdkEventAny*);
void moz_container_wayland_size_allocate(GtkWidget*, GtkAllocation*);
void moz_container_wayland_unmap(GtkWidget*);

struct wl_egl_window* moz_container_wayland_get_egl_window(
    MozContainer* container);

gboolean moz_container_wayland_has_egl_window(MozContainer* container);
bool moz_container_wayland_egl_window_set_size(MozContainer* container,
                                               nsIntSize aScaledSize);
void moz_container_wayland_add_or_fire_initial_draw_callback(
    MozContainer* container, const std::function<void(void)>& initial_draw_cb);

wl_surface* moz_gtk_widget_get_wl_surface(GtkWidget* aWidget);
void moz_container_wayland_update_opaque_region(MozContainer* container);
gboolean moz_container_wayland_can_draw(MozContainer* container);
double moz_container_wayland_get_scale(MozContainer* container);
void moz_container_wayland_set_commit_to_parent(MozContainer* container);
bool moz_container_wayland_is_commiting_to_parent(MozContainer* container);
bool moz_container_wayland_is_waiting_to_show(MozContainer* container);
void moz_container_wayland_clear_waiting_to_show_flag(MozContainer* container);

#endif /* __MOZ_CONTAINER_WAYLAND_H__ */
