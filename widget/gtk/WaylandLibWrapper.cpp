/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WaylandLibWrapper.h"
#include "mozilla/PodOperations.h"
#include "mozilla/Types.h"
#include "prlink.h"

struct WaylandLibWrapper MozWaylandWrapper;

WaylandLibWrapper::WaylandLibWrapper()
{
  PRLibSpec lspec;
  lspec.type = PR_LibSpec_Pathname;
  lspec.value.pathname = "libwayland-client.so.0";
  mWaylandLib = PR_LoadLibraryWithFlags(lspec, PR_LD_NOW | PR_LD_LOCAL);
  if (!mWaylandLib) {
   Unlink();
   return;
  }

#define WAY_FUNC(func)                                                    \
   if (!(func = (decltype(func))PR_FindSymbol(mWaylandLib, "wl_"#func))) {\
     NS_WARNING("Couldn't load function " # func);                        \
     Unlink();                                                            \
     return ;                                                             \
   }                                                                      \

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
}

void
WaylandLibWrapper::Unlink()
{
 if (mWaylandLib) {
   PR_UnloadLibrary(mWaylandLib);
 }
 PodZero(this);
}
