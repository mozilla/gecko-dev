/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayersLogging.h"
#include <stdint.h>                     // for uint8_t
#include "gfxColor.h"                   // for gfxRGBA
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4, Matrix
#include "mozilla/gfx/Point.h"          // for IntSize
#include "nsDebug.h"                    // for NS_ERROR
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRect.h"                     // for mozilla::gfx::IntRect

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

void
AppendToString(std::stringstream& aStream, const void* p,
               const char* pfx, const char* sfx)
{
  aStream << pfx;
  aStream << nsPrintfCString("%p", p).get();
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, const GraphicsFilter& f,
               const char* pfx, const char* sfx)
{
  aStream << pfx;
  switch (f) {
  case GraphicsFilter::FILTER_FAST:      aStream << "fast"; break;
  case GraphicsFilter::FILTER_GOOD:      aStream << "good"; break;
  case GraphicsFilter::FILTER_BEST:      aStream << "best"; break;
  case GraphicsFilter::FILTER_NEAREST:   aStream << "nearest"; break;
  case GraphicsFilter::FILTER_BILINEAR:  aStream << "bilinear"; break;
  case GraphicsFilter::FILTER_GAUSSIAN:  aStream << "gaussian"; break;
  default:
    NS_ERROR("unknown filter type");
    aStream << "???";
  }
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, FrameMetrics::ViewID n,
               const char* pfx, const char* sfx)
{
  aStream << pfx;
  aStream << n;
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, const gfxRGBA& c,
               const char* pfx, const char* sfx)
{
  aStream << pfx;
  aStream << nsPrintfCString(
    "rgba(%d, %d, %d, %g)",
    uint8_t(c.r*255.0), uint8_t(c.g*255.0), uint8_t(c.b*255.0), c.a).get();
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, const nsPoint& p,
               const char* pfx, const char* sfx)
{
  aStream << pfx;
  aStream << nsPrintfCString("(x=%d, y=%d)", p.x, p.y).get();
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, const nsRect& r,
               const char* pfx, const char* sfx)
{
  aStream << pfx;
  aStream << nsPrintfCString(
    "(x=%d, y=%d, w=%d, h=%d)",
    r.x, r.y, r.width, r.height).get();
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, const nsIntPoint& p,
               const char* pfx, const char* sfx)
{
  aStream << pfx;
  aStream << nsPrintfCString("(x=%d, y=%d)", p.x, p.y).get();
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, const IntRect& r,
               const char* pfx, const char* sfx)
{
  aStream << pfx;
  aStream << nsPrintfCString(
    "(x=%d, y=%d, w=%d, h=%d)",
    r.x, r.y, r.width, r.height).get();
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, const nsRegion& r,
               const char* pfx, const char* sfx)
{
  aStream << pfx;

  nsRegionRectIterator it(r);
  aStream << "< ";
  while (const nsRect* sr = it.Next()) {
    AppendToString(aStream, *sr);
    aStream << "; ";
  }
  aStream << ">";

  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, const nsIntRegion& r,
               const char* pfx, const char* sfx)
{
  aStream << pfx;

  nsIntRegionRectIterator it(r);
  aStream << "< ";
  while (const IntRect* sr = it.Next()) {
    AppendToString(aStream, *sr);
    aStream << "; ";
  }
  aStream << ">";

  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, const EventRegions& e,
               const char* pfx, const char* sfx)
{
  aStream << pfx << "{";
  if (!e.mHitRegion.IsEmpty()) {
    AppendToString(aStream, e.mHitRegion, " hitregion=", "");
  }
  if (!e.mDispatchToContentHitRegion.IsEmpty()) {
    AppendToString(aStream, e.mDispatchToContentHitRegion, " dispatchtocontentregion=", "");
  }
  if (!e.mNoActionRegion.IsEmpty()) {
    AppendToString(aStream, e.mNoActionRegion, " NoActionRegion=","");
  }
  if (!e.mHorizontalPanRegion.IsEmpty()) {
    AppendToString(aStream, e.mHorizontalPanRegion, " HorizontalPanRegion=", "");
  }
  if (!e.mVerticalPanRegion.IsEmpty()) {
    AppendToString(aStream, e.mVerticalPanRegion, " VerticalPanRegion=", "");
  }
  aStream << "}" << sfx;
}

void
AppendToString(std::stringstream& aStream, const FrameMetrics& m,
               const char* pfx, const char* sfx, bool detailed)
{
  aStream << pfx;
  AppendToString(aStream, m.GetCompositionBounds(), "{ [cb=");
  AppendToString(aStream, m.GetScrollableRect(), "] [sr=");
  AppendToString(aStream, m.GetScrollOffset(), "] [s=");
  if (m.GetDoSmoothScroll()) {
    AppendToString(aStream, m.GetSmoothScrollOffset(), "] [ss=");
  }
  AppendToString(aStream, m.GetDisplayPort(), "] [dp=");
  AppendToString(aStream, m.GetCriticalDisplayPort(), "] [cdp=");
  AppendToString(aStream, m.GetBackgroundColor(), "] [color=");
  if (!detailed) {
    AppendToString(aStream, m.GetScrollId(), "] [scrollId=");
    if (m.GetScrollParentId() != FrameMetrics::NULL_SCROLL_ID) {
      AppendToString(aStream, m.GetScrollParentId(), "] [scrollParent=");
    }
    if (m.IsRootContent()) {
      aStream << "] [rcd";
    }
    if (m.HasClipRect()) {
      AppendToString(aStream, m.ClipRect(), "] [clip=");
    }
    AppendToString(aStream, m.GetZoom(), "] [z=", "] }");
  } else {
    AppendToString(aStream, m.GetDisplayPortMargins(), " [dpm=");
    aStream << nsPrintfCString("] um=%d", m.GetUseDisplayPortMargins()).get();
    AppendToString(aStream, m.GetRootCompositionSize(), "] [rcs=");
    AppendToString(aStream, m.GetViewport(), "] [v=");
    aStream << nsPrintfCString("] [z=(ld=%.3f r=%.3f",
            m.GetDevPixelsPerCSSPixel().scale,
            m.GetPresShellResolution()).get();
    AppendToString(aStream, m.GetCumulativeResolution(), " cr=");
    AppendToString(aStream, m.GetZoom(), " z=");
    AppendToString(aStream, m.GetExtraResolution(), " er=");
    aStream << nsPrintfCString(")] [u=(%d %d %lu)",
            m.GetScrollOffsetUpdated(), m.GetDoSmoothScroll(),
            m.GetScrollGeneration()).get();
    AppendToString(aStream, m.GetScrollParentId(), "] [p=");
    aStream << nsPrintfCString("] [i=(%ld %lld %d)] }",
            m.GetPresShellId(), m.GetScrollId(), m.IsRootContent()).get();
  }
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, const ScrollableLayerGuid& s,
               const char* pfx, const char* sfx)
{
  aStream << pfx
          << nsPrintfCString("{ l=%" PRIu64 ", p=%u, v=%" PRIu64 " }", s.mLayersId, s.mPresShellId, s.mScrollId).get()
          << sfx;
}

void
AppendToString(std::stringstream& aStream, const ZoomConstraints& z,
               const char* pfx, const char* sfx)
{
  aStream << pfx
          << nsPrintfCString("{ z=%d dt=%d min=%f max=%f }", z.mAllowZoom, z.mAllowDoubleTapZoom, z.mMinZoom.scale, z.mMaxZoom.scale).get()
          << sfx;
}

void
AppendToString(std::stringstream& aStream, const Matrix& m,
               const char* pfx, const char* sfx)
{
  aStream << pfx;
  if (m.IsIdentity()) {
    aStream << "[ I ]";
  } else {
    aStream << nsPrintfCString(
      "[ %g %g; %g %g; %g %g; ]",
      m._11, m._12, m._21, m._22, m._31, m._32).get();
  }
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, const Matrix4x4& m,
               const char* pfx, const char* sfx)
{
  if (m.Is2D()) {
    Matrix matrix = m.As2D();
    AppendToString(aStream, matrix, pfx, sfx);
    return;
  }

  aStream << pfx;
  aStream << nsPrintfCString(
    "[ %g %g %g %g; %g %g %g %g; %g %g %g %g; %g %g %g %g; ]",
    m._11, m._12, m._13, m._14,
    m._21, m._22, m._23, m._24,
    m._31, m._32, m._33, m._34,
    m._41, m._42, m._43, m._44).get();
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, const Matrix5x4& m,
               const char* pfx, const char* sfx)
{
  aStream << pfx;
  aStream << nsPrintfCString(
    "[ %g %g %g %g; %g %g %g %g; %g %g %g %g; %g %g %g %g; %g %g %g %g]",
    m._11, m._12, m._13, m._14,
    m._21, m._22, m._23, m._24,
    m._31, m._32, m._33, m._34,
    m._41, m._42, m._43, m._44,
    m._51, m._52, m._53, m._54).get();
  aStream << sfx;
}


void
AppendToString(std::stringstream& aStream, const Filter filter,
               const char* pfx, const char* sfx)
{
  aStream << pfx;

  switch (filter) {
    case Filter::GOOD: aStream << "Filter::GOOD"; break;
    case Filter::LINEAR: aStream << "Filter::LINEAR"; break;
    case Filter::POINT: aStream << "Filter::POINT"; break;
  }
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, TextureFlags flags,
               const char* pfx, const char* sfx)
{
  aStream << pfx;
  if (flags == TextureFlags::NO_FLAGS) {
    aStream << "NoFlags";
  } else {

#define AppendFlag(test) \
{ \
  if (!!(flags & test)) { \
    if (previous) { \
      aStream << "|"; \
    } \
    aStream << #test; \
    previous = true; \
  } \
}
    bool previous = false;
    AppendFlag(TextureFlags::USE_NEAREST_FILTER);
    AppendFlag(TextureFlags::ORIGIN_BOTTOM_LEFT);
    AppendFlag(TextureFlags::DISALLOW_BIGIMAGE);

#undef AppendFlag
  }
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, mozilla::gfx::SurfaceFormat format,
               const char* pfx, const char* sfx)
{
  aStream << pfx;
  switch (format) {
  case SurfaceFormat::B8G8R8A8:  aStream << "SurfaceFormat::B8G8R8A8"; break;
  case SurfaceFormat::B8G8R8X8:  aStream << "SurfaceFormat::B8G8R8X8"; break;
  case SurfaceFormat::R8G8B8A8:  aStream << "SurfaceFormat::R8G8B8A8"; break;
  case SurfaceFormat::R8G8B8X8:  aStream << "SurfaceFormat::R8G8B8X8"; break;
  case SurfaceFormat::R5G6B5:    aStream << "SurfaceFormat::R5G6B5"; break;
  case SurfaceFormat::A8:        aStream << "SurfaceFormat::A8"; break;
  case SurfaceFormat::YUV:       aStream << "SurfaceFormat::YUV"; break;
  case SurfaceFormat::UNKNOWN:   aStream << "SurfaceFormat::UNKNOWN"; break;
  }

  aStream << sfx;
}

} // namespace
} // namespace

void
print_stderr(std::stringstream& aStr)
{
#if defined(ANDROID)
  // On Android logcat output is truncated to 1024 chars per line, and
  // we usually use std::stringstream to build up giant multi-line gobs
  // of output. So to avoid the truncation we find the newlines and
  // print the lines individually.
  std::string line;
  while (std::getline(aStr, line)) {
    printf_stderr("%s\n", line.c_str());
  }
#else
  printf_stderr("%s", aStr.str().c_str());
#endif
}

void
fprint_stderr(FILE* aFile, std::stringstream& aStr)
{
  if (aFile == stderr) {
    print_stderr(aStr);
  } else {
    fprintf_stderr(aFile, "%s", aStr.str().c_str());
  }
}
