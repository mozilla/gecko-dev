/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WindowGfx_h__
#define WindowGfx_h__

/*
 * nsWindowGfx - Painting and aceleration.
 */

#include "nsWindow.h"
#include <imgIContainer.h>

class nsISVGPaintContext;

class nsWindowGfx {
 public:
  enum IconSizeType { kSmallIcon, kRegularIcon };
  static mozilla::LayoutDeviceIntSize GetIconMetrics(IconSizeType aSizeType);

  /**
   * Renders an imgIContainer to a HICON.
   * aContainer - the image to render.
   * aSVGContext - Optional CSS context properties to apply. Ignored if the
   *               container is not an SVG image.
   * aIsCursor - true if this icon will be used as a mouse cursor.
   * aHotSpot - the position of the hot spot for a mouse cursor.
   * aScaledSize - the size of the icon to generate.
   * aIcon - Out parameter for the returned HICON. Required.
   */
  static nsresult CreateIcon(imgIContainer* aContainer,
                             nsISVGPaintContext* aSVGContext, bool aIsCursor,
                             mozilla::LayoutDeviceIntPoint aHotspot,
                             mozilla::LayoutDeviceIntSize aScaledSize,
                             HICON* aIcon);

 private:
  /**
   * Cursor helpers
   */
  static uint8_t* Data32BitTo1Bit(uint8_t* aImageData, uint32_t aWidth,
                                  uint32_t aHeight);
  static HBITMAP DataToBitmap(uint8_t* aImageData, uint32_t aWidth,
                              uint32_t aHeight, uint32_t aDepth);
};

#endif  // WindowGfx_h__
