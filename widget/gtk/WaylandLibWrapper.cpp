/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WaylandLibWrapper.h"
#include "mozilla/PodOperations.h"
#include "mozilla/Types.h"
#include "prlink.h"
#include  <gdk/gdk.h>

struct WaylandLibWrapper MozWaylandWrapper;

WaylandLibWrapper::WaylandLibWrapper()
{
#ifdef GDK_WINDOWING_WAYLAND
    PRLibSpec lspec;
    lspec.type = PR_LibSpec_Pathname;
    lspec.value.pathname = "libwayland-client.so.0";
    mWaylandLib = PR_LoadLibraryWithFlags(lspec, PR_LD_NOW | PR_LD_LOCAL);
    if (!mWaylandLib) {
        SetFallbackCalls();
        return;
    }

#define WAY_FUNC(func)                                                       \
    if (!(func = (decltype(func))PR_FindSymbol(mWaylandLib, "wl_"#func))) {  \
        NS_WARNING("Couldn't load function " # func);                        \
        PR_UnloadLibrary(mWaylandLib);                                       \
        SetFallbackCalls();                                                  \
        return;                                                              \
    }                                                                        \

    WAY_FUNC(registry_interface);
    WAY_FUNC(surface_interface);
    WAY_FUNC(subcompositor_interface);

    WAY_FUNC(display_roundtrip_queue);
    WAY_FUNC(display_roundtrip);
    WAY_FUNC(proxy_add_listener);
    WAY_FUNC(proxy_marshal);
    WAY_FUNC(proxy_marshal_constructor);
    WAY_FUNC(proxy_marshal_constructor_versioned);
    WAY_FUNC(proxy_destroy);
#undef WAY_FUNC
#else // GDK_WINDOWING_WAYLAND
    SetFallbackCalls();
#endif
}

static int
fallback_display_roundtrip_queue(struct wl_display *display,
                                 struct wl_event_queue *queue)
{
    return -1;
}
static int
fallback_display_roundtrip(struct wl_display *display)
{
    return -1;
}
static int
fallback_proxy_add_listener(struct wl_proxy *proxy,
                            void (**implementation)(void), void *data)
{
    return -1;
}
static void
fallback_proxy_marshal(struct wl_proxy *p, uint32_t opcode, ...)
{
    return;
}
static struct wl_proxy *
fallback_proxy_marshal_constructor(struct wl_proxy *proxy,
                                   uint32_t opcode,
                                   const struct wl_interface *interface, ...)
{
    return nullptr;
}
static struct wl_proxy *
fallback_proxy_marshal_constructor_versioned(struct wl_proxy *proxy,
    uint32_t opcode, const struct wl_interface *interface, uint32_t version, ...)
{
    return nullptr;
}
static void
fallback_proxy_destroy(struct wl_proxy *proxy)
{
    return;
}

void
WaylandLibWrapper::SetFallbackCalls()
{
#define WAY_FALLBACK(func)  \
    func = fallback_##func;

    // Set up fallback calls to avoid unexpected crashes
    WAY_FALLBACK(display_roundtrip_queue);
    WAY_FALLBACK(display_roundtrip);
    WAY_FALLBACK(proxy_add_listener);
    WAY_FALLBACK(proxy_marshal);
    WAY_FALLBACK(proxy_marshal_constructor);
    WAY_FALLBACK(proxy_marshal_constructor_versioned);
    WAY_FALLBACK(proxy_destroy);
#undef WAY_FALLBACK
}
