/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WaylandSurfaceLock.h"
#ifdef MOZ_WAYLAND
#  include "WaylandSurface.h"
#endif
#include "MozContainer.h"
#include "WidgetUtilsGtk.h"

namespace mozilla::widget {

WaylandSurfaceLock::WaylandSurfaceLock(RefPtr<WaylandSurface> aWaylandSurface) {
#ifdef MOZ_WAYLAND
  mWaylandSurface = std::move(aWaylandSurface);
  if (GdkIsWaylandDisplay()) {
    MOZ_DIAGNOSTIC_ASSERT(mWaylandSurface);
    // mSurface can be nullptr if we lock hidden MozContainer and
    // that's correct, MozContainer is still locked.
    mSurface = mWaylandSurface->Lock(this);
  }
#endif
}

WaylandSurfaceLock::~WaylandSurfaceLock() {
#ifdef MOZ_WAYLAND
  if (GdkIsWaylandDisplay()) {
    mWaylandSurface->Commit(this, /* force commit */ false,
                            /* flush display */ false);
    mWaylandSurface->Unlock(&mSurface, this);
  }
#endif
}

WaylandSurface* WaylandSurfaceLock::GetWaylandSurface() {
#ifdef MOZ_WAYLAND
  return mWaylandSurface.get();
#else
  return nullptr;
#endif
}

}  // namespace mozilla::widget
