/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WindowSurfaceCairo.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Tools.h"
#include "mozilla/gfx/gfxVars.h"
#include "gfxPlatform.h"
#include "gfx2DGlue.h"
#include "nsWindow.h"

namespace mozilla {
namespace widget {

using namespace mozilla::gfx;

WindowSurfaceCairo::WindowSurfaceCairo(nsWindow* aWidget) : mWidget(aWidget) {}
WindowSurfaceCairo::~WindowSurfaceCairo() {}

already_AddRefed<gfx::DrawTarget> WindowSurfaceCairo::Lock(
    const LayoutDeviceIntRegion& aRegion) {
  gfx::IntRect bounds = aRegion.GetBounds().ToUnknownRect();
  gfx::IntSize size(bounds.XMost(), bounds.YMost());

  if (!mImageSurface || mImageSurface->CairoStatus() ||
      !(size <= mImageSurface->GetSize())) {
    mImageSurface =
        new gfxImageSurface(size, gfx::SurfaceFormat::A8R8G8B8_UINT32);
    if (mImageSurface->CairoStatus()) {
      return nullptr;
    }
  }

  gfxImageFormat format = mImageSurface->Format();
  // Cairo prefers compositing to BGRX instead of BGRA where possible.
  // Cairo/pixman lacks some fast paths for compositing BGRX onto BGRA, so
  // just report it as BGRX directly in that case.
  // Otherwise, for Skia, report it as BGRA to the compositor. The alpha
  // channel will be discarded when we put the image.
  if (format == gfx::SurfaceFormat::X8R8G8B8_UINT32) {
    gfx::BackendType backend = gfxVars::ContentBackend();
    if (!gfx::Factory::DoesBackendSupportDataDrawtarget(backend)) {
      backend = gfx::BackendType::SKIA;
    }
    if (backend != gfx::BackendType::CAIRO) {
      format = gfx::SurfaceFormat::A8R8G8B8_UINT32;
    }
  }

  return gfxPlatform::CreateDrawTargetForData(
      mImageSurface->Data(), mImageSurface->GetSize(), mImageSurface->Stride(),
      ImageFormatToSurfaceFormat(format));
}

void WindowSurfaceCairo::Commit(const LayoutDeviceIntRegion& aInvalidRegion) {
  if (!mImageSurface) {
    return;
  }

  // We need to paint in main thread to GtkWidget.
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "WindowSurfaceCairo::Commit", [widget = RefPtr{mWidget}, aInvalidRegion,
                                     surface = RefPtr{mImageSurface}]() {
        widget->SetDragPopupSurface(surface, aInvalidRegion);
      }));

  mImageSurface = nullptr;
}

#if 0
void WindowSurfaceCairo::DumpToFile() {
  static int dumpSerial = 0;

  cairo_surface_t* surface = nullptr;
  auto unmap = MakeScopeExit([&] {
    if (surface) {
      cairo_surface_destroy(surface);
    }
  });
  auto size = mImageSurface->GetSize();
  surface = cairo_image_surface_create_for_data(
      (unsigned char*)mImageSurface->Data(), CAIRO_FORMAT_ARGB32,
      size.width, size.height, 4 * size.width);
  if (cairo_surface_status(surface) == CAIRO_STATUS_SUCCESS) {
    nsCString filename;
    filename.Append(nsPrintfCString("firefox-dnd-%.5d.png", dumpSerial++));
    cairo_surface_write_to_png(surface, filename.get());
  }
}
#endif

}  // namespace widget
}  // namespace mozilla
