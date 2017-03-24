/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __WaylandLibWrapper_h__
#define __WaylandLibWrapper_h__

#include "mozilla/Types.h"

struct PRLibrary;

struct WaylandLibWrapper
{
  WaylandLibWrapper();

  // The libraries are not unloaded in the destructor,
  // it would only run on shutdown anyway.
  ~WaylandLibWrapper() = default;

  struct wl_interface *registry_interface;
  struct wl_interface *surface_interface;
  struct wl_interface *subcompositor_interface;

  int (*display_roundtrip_queue)(struct wl_display *display,
      struct wl_event_queue *queue);
  int (*display_roundtrip)(struct wl_display *display);
  int (*proxy_add_listener)(struct wl_proxy *proxy,
      void (**implementation)(void), void *data);
  void (*proxy_marshal)(struct wl_proxy *p, uint32_t opcode, ...);
  struct wl_proxy * (*proxy_marshal_constructor)(struct wl_proxy *proxy,
      uint32_t opcode, const struct wl_interface *interface, ...);
  struct wl_proxy * (*proxy_marshal_constructor_versioned)(struct wl_proxy *proxy,
      uint32_t opcode, const struct wl_interface *interface, uint32_t version, ...);
  void (*proxy_destroy)(struct wl_proxy *proxy);

private:
  // Reset the wrapper and unlink all attached libraries.
  void Unlink();

  PRLibrary* mWaylandLib;
};

extern struct WaylandLibWrapper MozWaylandWrapper;

// Redefine our wrapped code
#define wl_registry_interface       (MozWaylandWrapper.registry_interface)
#define wl_surface_interface        (MozWaylandWrapper.surface_interface)
#define wl_subcompositor_interface  (MozWaylandWrapper.subcompositor_interface)

#define wl_display_roundtrip_queue  (MozWaylandWrapper.display_roundtrip_queue)
#define wl_display_roundtrip        (MozWaylandWrapper.display_roundtrip)
#define wl_proxy_add_listener       (MozWaylandWrapper.proxy_add_listener)
#define wl_proxy_marshal            (MozWaylandWrapper.proxy_marshal)
#define wl_proxy_marshal_constructor (MozWaylandWrapper.proxy_marshal_constructor)
#define wl_proxy_marshal_constructor_versioned \
                                    (MozWaylandWrapper.proxy_marshal_constructor_versioned)
#define wl_proxy_destroy            (MozWaylandWrapper.proxy_destroy)

#endif // WaylandLibWrapper
