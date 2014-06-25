/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYERSLOGGING_H
#define GFX_LAYERSLOGGING_H

#include "FrameMetrics.h"               // for FrameMetrics, etc
#include "GraphicsFilter.h"             // for GraphicsFilter
#include "mozilla/gfx/Point.h"          // for IntSize, etc
#include "mozilla/gfx/Types.h"          // for Filter, SurfaceFormat
#include "mozilla/layers/CompositorTypes.h"  // for TextureFlags
#include "nsAString.h"
#include "nsPrintfCString.h"            // for nsPrintfCString
#include "nsRegion.h"                   // for nsIntRegion
#include "nscore.h"                     // for nsACString, etc

struct gfxRGBA;
struct nsIntPoint;
struct nsIntRect;
struct nsIntSize;

namespace mozilla {
namespace gfx {
class Matrix4x4;
template <class units> struct RectTyped;
}

namespace layers {

void
AppendToString(std::stringstream& aStream, const void* p,
               const char* pfx="", const char* sfx="");

void
AppendToString(std::stringstream& aStream, const GraphicsFilter& f,
               const char* pfx="", const char* sfx="");

void
AppendToString(std::stringstream& aStream, FrameMetrics::ViewID n,
               const char* pfx="", const char* sfx="");

void
AppendToString(std::stringstream& aStream, const gfxRGBA& c,
               const char* pfx="", const char* sfx="");

void
AppendToString(std::stringstream& aStream, const nsIntPoint& p,
               const char* pfx="", const char* sfx="");

template<class T>
void
AppendToString(std::stringstream& aStream, const mozilla::gfx::PointTyped<T>& p,
               const char* pfx="", const char* sfx="")
{
  aStream << pfx;
  aStream << nsPrintfCString("(x=%f, y=%f)", p.x, p.y).get();
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, const nsIntRect& r,
               const char* pfx="", const char* sfx="");

template<class T>
void
AppendToString(std::stringstream& aStream, const mozilla::gfx::RectTyped<T>& r,
               const char* pfx="", const char* sfx="")
{
  aStream << pfx;
  aStream << nsPrintfCString(
    "(x=%f, y=%f, w=%f, h=%f)",
    r.x, r.y, r.width, r.height).get();
  aStream << sfx;
}

template<class T>
void
AppendToString(std::stringstream& aStream, const mozilla::gfx::IntRectTyped<T>& r,
               const char* pfx="", const char* sfx="")
{
  aStream << pfx;
  aStream << nsPrintfCString(
    "(x=%d, y=%d, w=%d, h=%d)",
    r.x, r.y, r.width, r.height).get();
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, const nsIntRegion& r,
               const char* pfx="", const char* sfx="");

void
AppendToString(std::stringstream& aStream, const nsIntSize& sz,
               const char* pfx="", const char* sfx="");

void
AppendToString(std::stringstream& aStream, const FrameMetrics& m,
               const char* pfx="", const char* sfx="");

void
AppendToString(std::stringstream& aStream, const mozilla::gfx::IntSize& size,
               const char* pfx="", const char* sfx="");

void
AppendToString(std::stringstream& aStream, const mozilla::gfx::Matrix4x4& m,
               const char* pfx="", const char* sfx="");

void
AppendToString(std::stringstream& aStream, const mozilla::gfx::Filter filter,
               const char* pfx="", const char* sfx="");

void
AppendToString(std::stringstream& aStream, mozilla::layers::TextureFlags flags,
               const char* pfx="", const char* sfx="");

void
AppendToString(std::stringstream& aStream, mozilla::gfx::SurfaceFormat format,
               const char* pfx="", const char* sfx="");

} // namespace
} // namespace

#endif /* GFX_LAYERSLOGGING_H */
