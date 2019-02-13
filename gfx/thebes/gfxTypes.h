/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_TYPES_H
#define GFX_TYPES_H

#include <stdint.h>

typedef struct _cairo_surface cairo_surface_t;
typedef struct _cairo_user_data_key cairo_user_data_key_t;

typedef void (*thebes_destroy_func_t) (void *data);

/**
 * Currently needs to be 'double' for Cairo compatibility. Could
 * become 'float', perhaps, in some configurations.
 */
typedef double gfxFloat;

/**
 * Priority of a line break opportunity.
 *
 * eNoBreak       The line has no break opportunities
 * eWordWrapBreak The line has a break opportunity only within a word. With
 *                word-wrap: break-word we will break at this point only if
 *                there are no other break opportunities in the line.
 * eNormalBreak   The line has a break opportunity determined by the standard
 *                line-breaking algorithm.
 *
 * Future expansion: split eNormalBreak into multiple priorities, e.g.
 *                    punctuation break and whitespace break (bug 389710).
 *                   As and when we implement it, text-wrap: unrestricted will
 *                    mean that priorities are ignored and all line-break
 *                    opportunities are equal.
 *
 * @see gfxTextRun::BreakAndMeasureText
 * @see nsLineLayout::NotifyOptionalBreakPosition
 */
enum class gfxBreakPriority {
  eNoBreak       = 0,
  eWordWrapBreak,
  eNormalBreak
};

/**
  * The format for an image surface. For all formats with alpha data, 0
  * means transparent, 1 or 255 means fully opaque.
  */
enum class gfxImageFormat {
  ARGB32, ///< ARGB data in native endianness, using premultiplied alpha
  RGB24,  ///< xRGB data in native endianness
  A8,     ///< Only an alpha channel
  A1,     ///< Packed transparency information (one byte refers to 8 pixels)
  RGB16_565,  ///< RGB_565 data in native endianness
  Unknown
};

enum class gfxSurfaceType {
  Image,
  PDF,
  PS,
  Xlib,
  Xcb,
  Glitz,           // unused, but needed for cairo parity
  Quartz,
  Win32,
  BeOS,
  DirectFB,        // unused, but needed for cairo parity
  SVG,
  OS2,
  Win32Printing,
  QuartzImage,
  Script,
  QPainter,
  Recording,
  VG,
  GL,
  DRM,
  Tee,
  XML,
  Skia,
  Subsurface,
  D2D,
  Max
};

enum class gfxContentType {
  COLOR       = 0x1000,
  ALPHA       = 0x2000,
  COLOR_ALPHA = 0x3000,
  SENTINEL    = 0xffff
};

/**
  * The memory used by a gfxASurface (as reported by KnownMemoryUsed()) can
  * either live in this process's heap, in this process but outside the
  * heap, or in another process altogether.
  */
enum class gfxMemoryLocation {
  IN_PROCESS_HEAP,
  IN_PROCESS_NONHEAP,
  OUT_OF_PROCESS
};

#endif /* GFX_TYPES_H */
