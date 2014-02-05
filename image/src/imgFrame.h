/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef imgFrame_h
#define imgFrame_h

#include "mozilla/MemoryReporting.h"
#include "mozilla/Mutex.h"
#include "nsRect.h"
#include "nsPoint.h"
#include "nsSize.h"
#include "gfxPattern.h"
#include "gfxDrawable.h"
#include "gfxImageSurface.h"
#if defined(XP_WIN)
#include "gfxWindowsSurface.h"
#elif defined(XP_MACOSX)
#include "gfxQuartzImageSurface.h"
#endif
#include "nsAutoPtr.h"
#include "imgIContainer.h"
#include "gfxColor.h"

class imgFrame
{
public:
  imgFrame();
  ~imgFrame();

  nsresult Init(int32_t aX, int32_t aY, int32_t aWidth, int32_t aHeight, gfxImageFormat aFormat, uint8_t aPaletteDepth = 0);
  nsresult Optimize();

  void Draw(gfxContext *aContext, GraphicsFilter aFilter,
            const gfxMatrix &aUserSpaceToImageSpace, const gfxRect& aFill,
            const nsIntMargin &aPadding, const nsIntRect &aSubimage,
            uint32_t aImageFlags = imgIContainer::FLAG_NONE);

  nsresult ImageUpdated(const nsIntRect &aUpdateRect);
  bool GetIsDirty() const;

  nsIntRect GetRect() const;
  gfxImageFormat GetFormat() const;
  bool GetNeedsBackground() const;
  uint32_t GetImageBytesPerRow() const;
  uint32_t GetImageDataLength() const;
  bool GetIsPaletted() const;
  bool GetHasAlpha() const;
  void GetImageData(uint8_t **aData, uint32_t *length) const;
  uint8_t* GetImageData() const;
  void GetPaletteData(uint32_t **aPalette, uint32_t *length) const;
  uint32_t* GetPaletteData() const;

  int32_t GetRawTimeout() const;
  void SetRawTimeout(int32_t aTimeout);

  int32_t GetFrameDisposalMethod() const;
  void SetFrameDisposalMethod(int32_t aFrameDisposalMethod);
  int32_t GetBlendMethod() const;
  void SetBlendMethod(int32_t aBlendMethod);
  bool ImageComplete() const;

  void SetHasNoAlpha();
  void SetAsNonPremult(bool aIsNonPremult);

  bool GetCompositingFailed() const;
  void SetCompositingFailed(bool val);

  nsresult LockImageData();
  nsresult UnlockImageData();
  void ApplyDirtToSurfaces();

  nsresult GetSurface(gfxASurface **aSurface) const
  {
    *aSurface = ThebesSurface();
    NS_IF_ADDREF(*aSurface);
    return NS_OK;
  }

  nsresult GetPattern(gfxPattern **aPattern) const
  {
    if (mSinglePixel)
      *aPattern = new gfxPattern(mSinglePixelColor);
    else
      *aPattern = new gfxPattern(ThebesSurface());
    NS_ADDREF(*aPattern);
    return NS_OK;
  }

  gfxASurface* ThebesSurface() const
  {
    if (mOptSurface)
      return mOptSurface;
#if defined(XP_WIN)
    if (mWinSurface)
      return mWinSurface;
#elif defined(XP_MACOSX)
    if (mQuartzSurface)
      return mQuartzSurface;
#endif
    return mImageSurface;
  }

  size_t SizeOfExcludingThisWithComputedFallbackIfHeap(
           gfxMemoryLocation aLocation,
           mozilla::MallocSizeOf aMallocSizeOf) const;

  uint8_t GetPaletteDepth() const { return mPaletteDepth; }
  uint32_t PaletteDataLength() const {
    if (!mPaletteDepth)
      return 0;

    return ((1 << mPaletteDepth) * sizeof(uint32_t));
  }

private: // methods

  struct SurfaceWithFormat {
    nsRefPtr<gfxDrawable> mDrawable;
    gfxImageFormat mFormat;
    SurfaceWithFormat() {}
    SurfaceWithFormat(gfxDrawable* aDrawable, gfxImageFormat aFormat)
     : mDrawable(aDrawable), mFormat(aFormat) {}
    bool IsValid() { return !!mDrawable; }
  };

  SurfaceWithFormat SurfaceForDrawing(bool               aDoPadding,
                                      bool               aDoPartialDecode,
                                      bool               aDoTile,
                                      const nsIntMargin& aPadding,
                                      gfxMatrix&         aUserSpaceToImageSpace,
                                      gfxRect&           aFill,
                                      gfxRect&           aSubimage,
                                      gfxRect&           aSourceRect,
                                      gfxRect&           aImageRect);

private: // data
  nsRefPtr<gfxImageSurface> mImageSurface;
  nsRefPtr<gfxASurface> mOptSurface;
#if defined(XP_WIN)
  nsRefPtr<gfxWindowsSurface> mWinSurface;
#elif defined(XP_MACOSX)
  nsRefPtr<gfxQuartzImageSurface> mQuartzSurface;
#endif

  nsIntSize    mSize;
  nsIntPoint   mOffset;

  nsIntRect    mDecoded;

  mutable mozilla::Mutex mDirtyMutex;

  // The palette and image data for images that are paletted, since Cairo
  // doesn't support these images.
  // The paletted data comes first, then the image data itself.
  // Total length is PaletteDataLength() + GetImageDataLength().
  uint8_t*     mPalettedImageData;

  // Note that the data stored in gfxRGBA is *non-alpha-premultiplied*.
  gfxRGBA      mSinglePixelColor;

  int32_t      mTimeout; // -1 means display forever
  int32_t      mDisposalMethod;

  /** Indicates how many readers currently have locked this frame */
  int32_t mLockCount;

  gfxImageFormat mFormat;
  uint8_t      mPaletteDepth;
  int8_t       mBlendMethod;
  bool mSinglePixel;
  bool mFormatChanged;
  bool mCompositingFailed;
  bool mNonPremult;

  /** Have we called DiscardTracker::InformAllocation()? */
  bool mInformedDiscardTracker;

  bool mDirty;
};

namespace mozilla {
namespace image {
  // An RAII class to ensure it's easy to balance locks and unlocks on
  // imgFrames.
  class AutoFrameLocker
  {
  public:
    AutoFrameLocker(imgFrame* frame)
      : mFrame(frame)
      , mSucceeded(NS_SUCCEEDED(frame->LockImageData()))
    {}

    ~AutoFrameLocker()
    {
      if (mSucceeded) {
        mFrame->UnlockImageData();
      }
    }

    // Whether the lock request succeeded.
    bool Succeeded() { return mSucceeded; }

  private:
    imgFrame* mFrame;
    bool mSucceeded;
  };
}
}

#endif /* imgFrame_h */
