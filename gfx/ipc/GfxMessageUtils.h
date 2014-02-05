/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __GFXMESSAGEUTILS_H__
#define __GFXMESSAGEUTILS_H__

#include "base/process_util.h"
#include "chrome/common/ipc_message_utils.h"
#include "ipc/IPCMessageUtils.h"

#include <stdint.h>

#include "gfx3DMatrix.h"
#include "gfxColor.h"
#include "mozilla/gfx/Matrix.h"
#include "GraphicsFilter.h"
#include "gfxPoint.h"
#include "gfxRect.h"
#include "nsRect.h"
#include "nsRegion.h"
#include "gfxTypes.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/layers/CompositorTypes.h"
#include "FrameMetrics.h"

#ifdef _MSC_VER
#pragma warning( disable : 4800 )
#endif

namespace mozilla {

typedef gfxImageFormat PixelFormat;
#if defined(MOZ_HAVE_CXX11_STRONG_ENUMS)
typedef ::GraphicsFilter GraphicsFilterType;
#else
// If we don't have support for enum classes, then we need to use the actual
// enum type here instead of the simulated enum class.
typedef GraphicsFilter::Enum GraphicsFilterType;
#endif

} // namespace mozilla

namespace IPC {

template<>
struct ParamTraits<mozilla::gfx::Matrix>
{
  typedef mozilla::gfx::Matrix paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam._11);
    WriteParam(aMsg, aParam._12);
    WriteParam(aMsg, aParam._21);
    WriteParam(aMsg, aParam._22);
    WriteParam(aMsg, aParam._31);
    WriteParam(aMsg, aParam._32);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (ReadParam(aMsg, aIter, &aResult->_11) &&
        ReadParam(aMsg, aIter, &aResult->_12) &&
        ReadParam(aMsg, aIter, &aResult->_21) &&
        ReadParam(aMsg, aIter, &aResult->_22) &&
        ReadParam(aMsg, aIter, &aResult->_31) &&
        ReadParam(aMsg, aIter, &aResult->_32))
      return true;

    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"[[%g %g] [%g %g] [%g %g]]", aParam._11, aParam._12, aParam._21, aParam._22,
                                                            aParam._31, aParam._32));
  }
};

template<>
struct ParamTraits<mozilla::gfx::Matrix4x4>
{
  typedef mozilla::gfx::Matrix4x4 paramType;

  static void Write(Message* msg, const paramType& param)
  {
#define Wr(_f)  WriteParam(msg, param. _f)
    Wr(_11); Wr(_12); Wr(_13); Wr(_14);
    Wr(_21); Wr(_22); Wr(_23); Wr(_24);
    Wr(_31); Wr(_32); Wr(_33); Wr(_34);
    Wr(_41); Wr(_42); Wr(_43); Wr(_44);
#undef Wr
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
#define Rd(_f)  ReadParam(msg, iter, &result-> _f)
    return (Rd(_11) && Rd(_12) && Rd(_13) && Rd(_14) &&
            Rd(_21) && Rd(_22) && Rd(_23) && Rd(_24) &&
            Rd(_31) && Rd(_32) && Rd(_33) && Rd(_34) &&
            Rd(_41) && Rd(_42) && Rd(_43) && Rd(_44));
#undef Rd
  }
};

template<>
struct ParamTraits<gfxPoint>
{
  typedef gfxPoint paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.x);
    WriteParam(aMsg, aParam.y);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return (ReadParam(aMsg, aIter, &aResult->x) &&
            ReadParam(aMsg, aIter, &aResult->y));
 }
};

template<>
struct ParamTraits<gfxPoint3D>
{
  typedef gfxPoint3D paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.x);
    WriteParam(aMsg, aParam.y);
    WriteParam(aMsg, aParam.z);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return (ReadParam(aMsg, aIter, &aResult->x) &&
            ReadParam(aMsg, aIter, &aResult->y) &&
            ReadParam(aMsg, aIter, &aResult->z));
  }
};

template<>
struct ParamTraits<gfxSize>
{
  typedef gfxSize paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.width);
    WriteParam(aMsg, aParam.height);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (ReadParam(aMsg, aIter, &aResult->width) &&
        ReadParam(aMsg, aIter, &aResult->height))
      return true;

    return false;
  }
};

template<>
struct ParamTraits<gfxRect>
{
  typedef gfxRect paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.x);
    WriteParam(aMsg, aParam.y);
    WriteParam(aMsg, aParam.width);
    WriteParam(aMsg, aParam.height);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->x) &&
           ReadParam(aMsg, aIter, &aResult->y) &&
           ReadParam(aMsg, aIter, &aResult->width) &&
           ReadParam(aMsg, aIter, &aResult->height);
  }
};

template<>
struct ParamTraits<gfx3DMatrix>
{
  typedef gfx3DMatrix paramType;

  static void Write(Message* msg, const paramType& param)
  {
#define Wr(_f)  WriteParam(msg, param. _f)
    Wr(_11); Wr(_12); Wr(_13); Wr(_14);
    Wr(_21); Wr(_22); Wr(_23); Wr(_24);
    Wr(_31); Wr(_32); Wr(_33); Wr(_34);
    Wr(_41); Wr(_42); Wr(_43); Wr(_44);
#undef Wr
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
#define Rd(_f)  ReadParam(msg, iter, &result-> _f)
    return (Rd(_11) && Rd(_12) && Rd(_13) && Rd(_14) &&
            Rd(_21) && Rd(_22) && Rd(_23) && Rd(_24) &&
            Rd(_31) && Rd(_32) && Rd(_33) && Rd(_34) &&
            Rd(_41) && Rd(_42) && Rd(_43) && Rd(_44));
#undef Rd
  }
};

template <>
struct ParamTraits<gfxContentType>
  : public TypedEnumSerializer<gfxContentType,
                               gfxContentType::COLOR,
                               gfxContentType::SENTINEL>
{};

template <>
struct ParamTraits<gfxSurfaceType>
  : public TypedEnumSerializer<gfxSurfaceType,
                               gfxSurfaceType::Image,
                               gfxSurfaceType::Max>
{};

template <>
struct ParamTraits<mozilla::GraphicsFilterType>
  : public EnumSerializer<mozilla::GraphicsFilterType,
                          GraphicsFilter::FILTER_FAST,
                          GraphicsFilter::FILTER_SENTINEL>
{};

template <>
struct ParamTraits<mozilla::layers::LayersBackend>
  : public TypedEnumSerializer<mozilla::layers::LayersBackend,
                               mozilla::layers::LayersBackend::LAYERS_NONE,
                               mozilla::layers::LayersBackend::LAYERS_LAST>
{};

template <>
struct ParamTraits<mozilla::layers::ScaleMode>
  : public TypedEnumSerializer<mozilla::layers::ScaleMode,
                               mozilla::layers::ScaleMode::SCALE_NONE,
                               mozilla::layers::ScaleMode::SENTINEL>
{};

template <>
struct ParamTraits<gfxImageFormat>
  : public TypedEnumSerializer<gfxImageFormat,
                               gfxImageFormat::ARGB32,
                               gfxImageFormat::Unknown>
{};

/*
template <>
struct ParamTraits<mozilla::PixelFormat>
  : public EnumSerializer<mozilla::PixelFormat,
                          gfxImageFormat::ARGB32,
                          gfxImageFormat::Unknown>
{};
*/

template<>
struct ParamTraits<gfxRGBA>
{
  typedef gfxRGBA paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.r);
    WriteParam(msg, param.g);
    WriteParam(msg, param.b);
    WriteParam(msg, param.a);
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->r) &&
            ReadParam(msg, iter, &result->g) &&
            ReadParam(msg, iter, &result->b) &&
            ReadParam(msg, iter, &result->a));
  }
};

template<>
struct ParamTraits<nsPoint>
{
  typedef nsPoint paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.x);
    WriteParam(msg, param.y);
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->x) &&
            ReadParam(msg, iter, &result->y));
  }
};

template<>
struct ParamTraits<nsIntPoint>
{
  typedef nsIntPoint paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.x);
    WriteParam(msg, param.y);
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->x) &&
            ReadParam(msg, iter, &result->y));
  }
};

template<>
struct ParamTraits<mozilla::gfx::IntSize>
{
  typedef mozilla::gfx::IntSize paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.width);
    WriteParam(msg, param.height);
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->width) &&
            ReadParam(msg, iter, &result->height));
  }
};

template<>
struct ParamTraits<nsIntRect>
{
  typedef nsIntRect paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.x);
    WriteParam(msg, param.y);
    WriteParam(msg, param.width);
    WriteParam(msg, param.height);
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->x) &&
            ReadParam(msg, iter, &result->y) &&
            ReadParam(msg, iter, &result->width) &&
            ReadParam(msg, iter, &result->height));
  }
};

template<typename Region, typename Rect, typename Iter>
struct RegionParamTraits
{
  typedef Region paramType;

  static void Write(Message* msg, const paramType& param)
  {
    Iter it(param);
    while (const Rect* r = it.Next())
      WriteParam(msg, *r);
    // empty rects are sentinel values because nsRegions will never
    // contain them
    WriteParam(msg, Rect());
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    Rect rect;
    while (ReadParam(msg, iter, &rect)) {
      if (rect.IsEmpty())
        return true;
      result->Or(*result, rect);
    }
    return false;
  }
};

template<>
struct ParamTraits<nsIntRegion>
  : RegionParamTraits<nsIntRegion, nsIntRect, nsIntRegionRectIterator>
{};

template<>
struct ParamTraits<nsIntSize>
{
  typedef nsIntSize paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.width);
    WriteParam(msg, param.height);
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->width) &&
            ReadParam(msg, iter, &result->height));
  }
};

template<class T, class U>
struct ParamTraits< mozilla::gfx::ScaleFactor<T, U> >
{
  typedef mozilla::gfx::ScaleFactor<T, U> paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.scale);
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->scale));
  }
};

template<class T>
struct ParamTraits< mozilla::gfx::PointTyped<T> >
{
  typedef mozilla::gfx::PointTyped<T> paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.x);
    WriteParam(msg, param.y);
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->x) &&
            ReadParam(msg, iter, &result->y));
  }
};

template<class T>
struct ParamTraits< mozilla::gfx::IntPointTyped<T> >
{
  typedef mozilla::gfx::IntPointTyped<T> paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.x);
    WriteParam(msg, param.y);
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->x) &&
            ReadParam(msg, iter, &result->y));
  }
};

template<>
struct ParamTraits<mozilla::gfx::Size>
{
  typedef mozilla::gfx::Size paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.width);
    WriteParam(msg, param.height);
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->width) &&
            ReadParam(msg, iter, &result->height));
  }
};

template<class T>
struct ParamTraits< mozilla::gfx::RectTyped<T> >
{
  typedef mozilla::gfx::RectTyped<T> paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.x);
    WriteParam(msg, param.y);
    WriteParam(msg, param.width);
    WriteParam(msg, param.height);
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->x) &&
            ReadParam(msg, iter, &result->y) &&
            ReadParam(msg, iter, &result->width) &&
            ReadParam(msg, iter, &result->height));
  }
};

template<class T>
struct ParamTraits< mozilla::gfx::IntRectTyped<T> >
{
  typedef mozilla::gfx::IntRectTyped<T> paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.x);
    WriteParam(msg, param.y);
    WriteParam(msg, param.width);
    WriteParam(msg, param.height);
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->x) &&
            ReadParam(msg, iter, &result->y) &&
            ReadParam(msg, iter, &result->width) &&
            ReadParam(msg, iter, &result->height));
  }
};

template<>
struct ParamTraits<mozilla::gfx::Margin>
{
  typedef mozilla::gfx::Margin paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.top);
    WriteParam(msg, param.right);
    WriteParam(msg, param.bottom);
    WriteParam(msg, param.left);
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->top) &&
            ReadParam(msg, iter, &result->right) &&
            ReadParam(msg, iter, &result->bottom) &&
            ReadParam(msg, iter, &result->left));
  }
};

template<class T>
struct ParamTraits< mozilla::gfx::MarginTyped<T> >
{
  typedef mozilla::gfx::MarginTyped<T> paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.top);
    WriteParam(msg, param.right);
    WriteParam(msg, param.bottom);
    WriteParam(msg, param.left);
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->top) &&
            ReadParam(msg, iter, &result->right) &&
            ReadParam(msg, iter, &result->bottom) &&
            ReadParam(msg, iter, &result->left));
  }
};

template<>
struct ParamTraits<nsRect>
{
  typedef nsRect paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.x);
    WriteParam(msg, param.y);
    WriteParam(msg, param.width);
    WriteParam(msg, param.height);
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->x) &&
            ReadParam(msg, iter, &result->y) &&
            ReadParam(msg, iter, &result->width) &&
            ReadParam(msg, iter, &result->height));
  }
};

template<>
struct ParamTraits<nsRegion>
  : RegionParamTraits<nsRegion, nsRect, nsRegionRectIterator>
{};

template <>
struct ParamTraits<mozilla::layers::FrameMetrics>
{
  typedef mozilla::layers::FrameMetrics paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mScrollableRect);
    WriteParam(aMsg, aParam.mViewport);
    WriteParam(aMsg, aParam.mScrollOffset);
    WriteParam(aMsg, aParam.mDisplayPort);
    WriteParam(aMsg, aParam.mCriticalDisplayPort);
    WriteParam(aMsg, aParam.mCompositionBounds);
    WriteParam(aMsg, aParam.mScrollId);
    WriteParam(aMsg, aParam.mResolution);
    WriteParam(aMsg, aParam.mCumulativeResolution);
    WriteParam(aMsg, aParam.mZoom);
    WriteParam(aMsg, aParam.mDevPixelsPerCSSPixel);
    WriteParam(aMsg, aParam.mMayHaveTouchListeners);
    WriteParam(aMsg, aParam.mPresShellId);
    WriteParam(aMsg, aParam.mIsRoot);
    WriteParam(aMsg, aParam.mHasScrollgrab);
    WriteParam(aMsg, aParam.mUpdateScrollOffset);
    WriteParam(aMsg, aParam.mDisableScrollingX);
    WriteParam(aMsg, aParam.mDisableScrollingY);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return (ReadParam(aMsg, aIter, &aResult->mScrollableRect) &&
            ReadParam(aMsg, aIter, &aResult->mViewport) &&
            ReadParam(aMsg, aIter, &aResult->mScrollOffset) &&
            ReadParam(aMsg, aIter, &aResult->mDisplayPort) &&
            ReadParam(aMsg, aIter, &aResult->mCriticalDisplayPort) &&
            ReadParam(aMsg, aIter, &aResult->mCompositionBounds) &&
            ReadParam(aMsg, aIter, &aResult->mScrollId) &&
            ReadParam(aMsg, aIter, &aResult->mResolution) &&
            ReadParam(aMsg, aIter, &aResult->mCumulativeResolution) &&
            ReadParam(aMsg, aIter, &aResult->mZoom) &&
            ReadParam(aMsg, aIter, &aResult->mDevPixelsPerCSSPixel) &&
            ReadParam(aMsg, aIter, &aResult->mMayHaveTouchListeners) &&
            ReadParam(aMsg, aIter, &aResult->mPresShellId) &&
            ReadParam(aMsg, aIter, &aResult->mIsRoot) &&
            ReadParam(aMsg, aIter, &aResult->mHasScrollgrab) &&
            ReadParam(aMsg, aIter, &aResult->mUpdateScrollOffset) &&
            ReadParam(aMsg, aIter, &aResult->mDisableScrollingX) &&
            ReadParam(aMsg, aIter, &aResult->mDisableScrollingY));
  }
};

template<>
struct ParamTraits<mozilla::layers::TextureFactoryIdentifier>
{
  typedef mozilla::layers::TextureFactoryIdentifier paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mParentBackend);
    WriteParam(aMsg, aParam.mMaxTextureSize);
    WriteParam(aMsg, aParam.mSupportsTextureBlitting);
    WriteParam(aMsg, aParam.mSupportsPartialUploads);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mParentBackend) &&
           ReadParam(aMsg, aIter, &aResult->mMaxTextureSize) &&
           ReadParam(aMsg, aIter, &aResult->mSupportsTextureBlitting) &&
           ReadParam(aMsg, aIter, &aResult->mSupportsPartialUploads);
  }
};

template<>
struct ParamTraits<mozilla::layers::TextureInfo>
{
  typedef mozilla::layers::TextureInfo paramType;
  
  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mCompositableType);
    WriteParam(aMsg, aParam.mDeprecatedTextureHostFlags);
    WriteParam(aMsg, aParam.mTextureFlags);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mCompositableType) &&
           ReadParam(aMsg, aIter, &aResult->mDeprecatedTextureHostFlags) &&
           ReadParam(aMsg, aIter, &aResult->mTextureFlags);
  }
};

template <>
struct ParamTraits<mozilla::layers::CompositableType>
  : public EnumSerializer<mozilla::layers::CompositableType,
                          mozilla::layers::BUFFER_UNKNOWN,
                          mozilla::layers::BUFFER_COUNT>
{};

template <>
struct ParamTraits<mozilla::gfx::SurfaceFormat>
  : public TypedEnumSerializer<mozilla::gfx::SurfaceFormat,
                               mozilla::gfx::SurfaceFormat::B8G8R8A8,
                               mozilla::gfx::SurfaceFormat::UNKNOWN>
{};

template <>
struct ParamTraits<mozilla::layers::ScrollableLayerGuid>
{
  typedef mozilla::layers::ScrollableLayerGuid paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mLayersId);
    WriteParam(aMsg, aParam.mPresShellId);
    WriteParam(aMsg, aParam.mScrollId);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return (ReadParam(aMsg, aIter, &aResult->mLayersId) &&
            ReadParam(aMsg, aIter, &aResult->mPresShellId) &&
            ReadParam(aMsg, aIter, &aResult->mScrollId));
  }
};

template <>
struct ParamTraits<mozilla::layers::ZoomConstraints>
{
  typedef mozilla::layers::ZoomConstraints paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mAllowZoom);
    WriteParam(aMsg, aParam.mMinZoom);
    WriteParam(aMsg, aParam.mMaxZoom);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return (ReadParam(aMsg, aIter, &aResult->mAllowZoom) &&
            ReadParam(aMsg, aIter, &aResult->mMinZoom) &&
            ReadParam(aMsg, aIter, &aResult->mMaxZoom));
  }
};

template <>
struct ParamTraits<mozilla::layers::EventRegions>
{
  typedef mozilla::layers::EventRegions paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mHitRegion);
    WriteParam(aMsg, aParam.mDispatchToContentHitRegion);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return (ReadParam(aMsg, aIter, &aResult->mHitRegion) &&
            ReadParam(aMsg, aIter, &aResult->mDispatchToContentHitRegion));
  }
};

} /* namespace IPC */

#endif /* __GFXMESSAGEUTILS_H__ */
