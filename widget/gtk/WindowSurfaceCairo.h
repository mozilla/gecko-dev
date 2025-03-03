/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_CAIRO_H
#define _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_CAIRO_H

#include "mozilla/widget/WindowSurface.h"
#include "mozilla/gfx/Types.h"
#include <glib.h>
#include "gfxImageSurface.h"

class nsWindow;

namespace mozilla {
namespace widget {

class WindowSurfaceCairo : public WindowSurface {
 public:
  explicit WindowSurfaceCairo(nsWindow* aWidget);
  ~WindowSurfaceCairo();

  already_AddRefed<gfx::DrawTarget> Lock(
      const LayoutDeviceIntRegion& aRegion) override;
  void Commit(const LayoutDeviceIntRegion& aInvalidRegion) override;
  bool IsFallback() const override { return true; }

 private:
  RefPtr<gfxImageSurface> mImageSurface;
  RefPtr<nsWindow> mWidget;
};

}  // namespace widget
}  // namespace mozilla

#endif  // _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_X11_IMAGE_H
