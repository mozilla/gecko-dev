/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// NOTE: No include guard.  This is meant to be included to generate different
// code based on how GFXINFO_FEATURE_STATUS is defined, possibly multiple times
// in a single translation unit.

/* clang-format off */

// There is an assumption that this is the first enum
GFXINFO_WINDOW_PROTOCOL(All, "")
GFXINFO_WINDOW_PROTOCOL(X11, "x11")
GFXINFO_WINDOW_PROTOCOL(XWayland, "xwayland")
GFXINFO_WINDOW_PROTOCOL(Wayland, "wayland")
GFXINFO_WINDOW_PROTOCOL(WaylandDRM, "wayland/drm")
// Wildcard for all Wayland variants, excluding XWayland.
GFXINFO_WINDOW_PROTOCOL(WaylandAll, "wayland/all")
// Wildcard for all X11 variants, including XWayland.
GFXINFO_WINDOW_PROTOCOL(X11All, "x11/all")
