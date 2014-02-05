/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_IMAGETYPES_H
#define GFX_IMAGETYPES_H

#include "mozilla/TypedEnum.h"

namespace mozilla {

MOZ_BEGIN_ENUM_CLASS(ImageFormat)
  /**
   * The PLANAR_YCBCR format creates a PlanarYCbCrImage. All backends should
   * support this format, because the Ogg video decoder depends on it.
   * The maximum image width and height is 16384.
   */
  PLANAR_YCBCR,

  /**
   * The GRALLOC_PLANAR_YCBCR format creates a GrallocImage, a subtype of
   * PlanarYCbCrImage. It takes a PlanarYCbCrImage data or the raw gralloc
   * data and can be used as a texture by Gonk backend directly.
   */
  GRALLOC_PLANAR_YCBCR,

  /**
   * The SHARED_RGB format creates a DeprecatedSharedRGBImage, which stores RGB data in
   * shared memory. Some Android hardware video decoders require this format.
   * Currently only used on Android.
   */
  SHARED_RGB,

  /**
   * The CAIRO_SURFACE format creates a CairoImage. All backends should
   * support this format, because video rendering sometimes requires it.
   *
   * This format is useful even though a ThebesLayer could be used.
   * It makes it easy to render a cairo surface when another Image format
   * could be used. It can also avoid copying the surface data in some
   * cases.
   *
   * Images in CAIRO_SURFACE format should only be created and
   * manipulated on the main thread, since the underlying cairo surface
   * is main-thread-only.
   */
  CAIRO_SURFACE,

  /**
   * A MacIOSurface object.
   */
  MAC_IOSURFACE,

  /**
   * An bitmap image that can be shared with a remote process.
   */
  REMOTE_IMAGE_BITMAP,

  /**
   * A OpenGL texture that can be shared across threads or processes
   */
  SHARED_TEXTURE,

  /**
   * An DXGI shared surface handle that can be shared with a remote process.
   */
  REMOTE_IMAGE_DXGI_TEXTURE,

  /**
   * The D3D9_RGB32_TEXTURE format creates a D3D9SurfaceImage, and wraps a
   * IDirect3DTexture9 in RGB32 layout.
   */
  D3D9_RGB32_TEXTURE
MOZ_END_ENUM_CLASS(ImageFormat)

MOZ_BEGIN_ENUM_CLASS(StereoMode)
  MONO,
  LEFT_RIGHT,
  RIGHT_LEFT,
  BOTTOM_TOP,
  TOP_BOTTOM
MOZ_END_ENUM_CLASS(StereoMode)

} // namespace

#endif
