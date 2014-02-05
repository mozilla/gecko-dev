/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "ImageContainer.h"
#include <string.h>                     // for memcpy, memset
#include "SharedTextureImage.h"         // for SharedTextureImage
#include "gfx2DGlue.h"
#include "gfxImageSurface.h"            // for gfxImageSurface
#include "gfxPlatform.h"                // for gfxPlatform
#include "gfxUtils.h"                   // for gfxUtils
#include "mozilla/RefPtr.h"             // for TemporaryRef
#include "mozilla/ipc/CrossProcessMutex.h"  // for CrossProcessMutex, etc
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/ImageBridgeChild.h"  // for ImageBridgeChild
#include "mozilla/layers/ImageClient.h"  // for ImageClient
#include "nsISupportsUtils.h"           // for NS_IF_ADDREF
#include "YCbCrUtils.h"                 // for YCbCr conversions
#ifdef MOZ_WIDGET_GONK
#include "GrallocImages.h"
#endif
#include "gfx2DGlue.h"

#ifdef XP_MACOSX
#include "mozilla/gfx/QuartzSupport.h"
#include "MacIOSurfaceImage.h"
#endif

#ifdef XP_WIN
#include "gfxD2DSurface.h"
#include "gfxWindowsPlatform.h"
#include <d3d10_1.h>
#include "d3d10/ImageLayerD3D10.h"
#include "D3D9SurfaceImage.h"
#endif

using namespace mozilla::ipc;
using namespace android;
using mozilla::gfx::DataSourceSurface;
using mozilla::gfx::SourceSurface;


namespace mozilla {
namespace layers {

class DataSourceSurface;
class SourceSurface;

Atomic<int32_t> Image::sSerialCounter(0);

TemporaryRef<gfx::SourceSurface>
Image::GetAsSourceSurface()
{
  nsRefPtr<gfxASurface> surface = DeprecatedGetAsSurface();
  return gfxPlatform::GetPlatform()->GetSourceSurfaceForSurface(nullptr, surface);
}

already_AddRefed<Image>
ImageFactory::CreateImage(ImageFormat aFormat,
                          const gfx::IntSize &,
                          BufferRecycleBin *aRecycleBin)
{
  nsRefPtr<Image> img;
#ifdef MOZ_WIDGET_GONK
  if (aFormat == ImageFormat::GRALLOC_PLANAR_YCBCR) {
    img = new GrallocImage();
    return img.forget();
  }
#endif
  if (aFormat == ImageFormat::PLANAR_YCBCR) {
    img = new PlanarYCbCrImage(aRecycleBin);
    return img.forget();
  }
  if (aFormat == ImageFormat::CAIRO_SURFACE) {
    img = new CairoImage();
    return img.forget();
  }
  if (aFormat == ImageFormat::SHARED_TEXTURE) {
    img = new SharedTextureImage();
    return img.forget();
  }
#ifdef XP_MACOSX
  if (aFormat == ImageFormat::MAC_IOSURFACE) {
    img = new MacIOSurfaceImage();
    return img.forget();
  }
#endif
#ifdef XP_WIN
  if (aFormat == ImageFormat::D3D9_RGB32_TEXTURE) {
    img = new D3D9SurfaceImage();
    return img.forget();
  }
#endif
  return nullptr;
}

BufferRecycleBin::BufferRecycleBin()
  : mLock("mozilla.layers.BufferRecycleBin.mLock")
{
}

void
BufferRecycleBin::RecycleBuffer(uint8_t* aBuffer, uint32_t aSize)
{
  MutexAutoLock lock(mLock);

  if (!mRecycledBuffers.IsEmpty() && aSize != mRecycledBufferSize) {
    mRecycledBuffers.Clear();
  }
  mRecycledBufferSize = aSize;
  mRecycledBuffers.AppendElement(aBuffer);
}

uint8_t*
BufferRecycleBin::GetBuffer(uint32_t aSize)
{
  MutexAutoLock lock(mLock);

  if (mRecycledBuffers.IsEmpty() || mRecycledBufferSize != aSize)
    return new uint8_t[aSize];

  uint32_t last = mRecycledBuffers.Length() - 1;
  uint8_t* result = mRecycledBuffers[last].forget();
  mRecycledBuffers.RemoveElementAt(last);
  return result;
}

ImageContainer::ImageContainer(int flag)
: mReentrantMonitor("ImageContainer.mReentrantMonitor"),
  mPaintCount(0),
  mPreviousImagePainted(false),
  mImageFactory(new ImageFactory()),
  mRecycleBin(new BufferRecycleBin()),
  mRemoteData(nullptr),
  mRemoteDataMutex(nullptr),
  mCompositionNotifySink(nullptr),
  mImageClient(nullptr)
{
  if (flag == ENABLE_ASYNC && ImageBridgeChild::IsCreated()) {
    // the refcount of this ImageClient is 1. we don't use a RefPtr here because the refcount
    // of this class must be done on the ImageBridge thread.
    if (gfxPlatform::GetPlatform()->UseDeprecatedTextures()) {
      mImageClient = ImageBridgeChild::GetSingleton()->CreateImageClient(BUFFER_IMAGE_BUFFERED).drop();
    } else {
      mImageClient = ImageBridgeChild::GetSingleton()->CreateImageClient(BUFFER_IMAGE_SINGLE).drop();
    }
    MOZ_ASSERT(mImageClient);
  }
}

ImageContainer::~ImageContainer()
{
  if (IsAsync()) {
    ImageBridgeChild::DispatchReleaseImageClient(mImageClient);
  }
}

already_AddRefed<Image>
ImageContainer::CreateImage(ImageFormat aFormat)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  if (mImageClient) {
    nsRefPtr<Image> img = mImageClient->CreateImage(aFormat);
    if (img) {
      return img.forget();
    }
  }
  return mImageFactory->CreateImage(aFormat, mScaleHint, mRecycleBin);
}

void
ImageContainer::SetCurrentImageInternal(Image *aImage)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  if (mRemoteData) {
    NS_ASSERTION(mRemoteDataMutex, "Should have remote data mutex when having remote data!");
    mRemoteDataMutex->Lock();
    // This is important since it ensures we won't change the active image
    // when we currently have a locked image that depends on mRemoteData.
  }

  mActiveImage = aImage;
  CurrentImageChanged();

  if (mRemoteData) {
    mRemoteDataMutex->Unlock();
  }
}

void
ImageContainer::ClearCurrentImage()
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  SetCurrentImageInternal(nullptr);
}

void
ImageContainer::SetCurrentImage(Image *aImage)
{
  if (!aImage) {
    ClearAllImages();
    return;
  }

  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  if (IsAsync()) {
    ImageBridgeChild::DispatchImageClientUpdate(mImageClient, this);
  }
  SetCurrentImageInternal(aImage);
}

 void
ImageContainer::ClearAllImages()
{
  if (IsAsync()) {
    // Let ImageClient release all TextureClients.
    ImageBridgeChild::FlushAllImages(mImageClient, this, false);
    return;
  }

  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  SetCurrentImageInternal(nullptr);
}

void
ImageContainer::ClearAllImagesExceptFront()
{
  if (IsAsync()) {
    // Let ImageClient release all TextureClients except front one.
    ImageBridgeChild::FlushAllImages(mImageClient, this, true);
  }
}

void
ImageContainer::SetCurrentImageInTransaction(Image *aImage)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  NS_ASSERTION(!mImageClient, "Should use async image transfer with ImageBridge.");

  SetCurrentImageInternal(aImage);
}

bool ImageContainer::IsAsync() const {
  return mImageClient != nullptr;
}

uint64_t ImageContainer::GetAsyncContainerID() const
{
  NS_ASSERTION(IsAsync(),"Shared image ID is only relevant to async ImageContainers");
  if (IsAsync()) {
    return mImageClient->GetAsyncID();
  } else {
    return 0; // zero is always an invalid AsyncID
  }
}

bool
ImageContainer::HasCurrentImage()
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  if (mRemoteData) {
    CrossProcessMutexAutoLock autoLock(*mRemoteDataMutex);
    
    EnsureActiveImage();

    return !!mActiveImage.get();
  }

  return !!mActiveImage.get();
}

already_AddRefed<Image>
ImageContainer::LockCurrentImage()
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  
  if (mRemoteData) {
    NS_ASSERTION(mRemoteDataMutex, "Should have remote data mutex when having remote data!");
    mRemoteDataMutex->Lock();
  }

  EnsureActiveImage();

  nsRefPtr<Image> retval = mActiveImage;
  return retval.forget();
}

already_AddRefed<gfxASurface>
ImageContainer::DeprecatedLockCurrentAsSurface(gfx::IntSize *aSize, Image** aCurrentImage)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  if (mRemoteData) {
    NS_ASSERTION(mRemoteDataMutex, "Should have remote data mutex when having remote data!");
    mRemoteDataMutex->Lock();

    EnsureActiveImage();

    if (aCurrentImage) {
      NS_IF_ADDREF(mActiveImage);
      *aCurrentImage = mActiveImage.get();
    }

    if (!mActiveImage) {
      return nullptr;
    } 

    if (mActiveImage->GetFormat() == ImageFormat::REMOTE_IMAGE_BITMAP) {
      nsRefPtr<gfxImageSurface> newSurf =
        new gfxImageSurface(mRemoteData->mBitmap.mData,
                            ThebesIntSize(mRemoteData->mSize),
                            mRemoteData->mBitmap.mStride,
                            mRemoteData->mFormat == RemoteImageData::BGRX32 ?
                                                   gfxImageFormat::ARGB32 :
                                                   gfxImageFormat::RGB24);

      *aSize = newSurf->GetSize().ToIntSize();
    
      return newSurf.forget();
    }

    *aSize = mActiveImage->GetSize();
    return mActiveImage->DeprecatedGetAsSurface();
  }

  if (aCurrentImage) {
    NS_IF_ADDREF(mActiveImage);
    *aCurrentImage = mActiveImage.get();
  }

  if (!mActiveImage) {
    return nullptr;
  }

  *aSize = mActiveImage->GetSize();
  return mActiveImage->DeprecatedGetAsSurface();
}

TemporaryRef<gfx::SourceSurface>
ImageContainer::LockCurrentAsSourceSurface(gfx::IntSize *aSize, Image** aCurrentImage)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  if (mRemoteData) {
    NS_ASSERTION(mRemoteDataMutex, "Should have remote data mutex when having remote data!");
    mRemoteDataMutex->Lock();

    EnsureActiveImage();

    if (aCurrentImage) {
      NS_IF_ADDREF(mActiveImage);
      *aCurrentImage = mActiveImage.get();
    }

    if (!mActiveImage) {
      return nullptr;
    }

    if (mActiveImage->GetFormat() == ImageFormat::REMOTE_IMAGE_BITMAP) {
      gfxImageFormat fmt = mRemoteData->mFormat == RemoteImageData::BGRX32
                           ? gfxImageFormat::ARGB32
                           : gfxImageFormat::RGB24;

      RefPtr<gfx::DataSourceSurface> newSurf
        = gfx::Factory::CreateWrappingDataSourceSurface(mRemoteData->mBitmap.mData,
                                                        mRemoteData->mBitmap.mStride,
                                                        mRemoteData->mSize,
                                                        gfx::ImageFormatToSurfaceFormat(fmt));
      *aSize = newSurf->GetSize();

      return newSurf;
    }

    *aSize = mActiveImage->GetSize();
    return mActiveImage->GetAsSourceSurface();
  }

  if (aCurrentImage) {
    NS_IF_ADDREF(mActiveImage);
    *aCurrentImage = mActiveImage.get();
  }

  if (!mActiveImage) {
    return nullptr;
  }

  *aSize = mActiveImage->GetSize();
  return mActiveImage->GetAsSourceSurface();
}

void
ImageContainer::UnlockCurrentImage()
{
  if (mRemoteData) {
    NS_ASSERTION(mRemoteDataMutex, "Should have remote data mutex when having remote data!");
    mRemoteDataMutex->Unlock();
  }
}

already_AddRefed<gfxASurface>
ImageContainer::DeprecatedGetCurrentAsSurface(gfx::IntSize *aSize)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  if (mRemoteData) {
    CrossProcessMutexAutoLock autoLock(*mRemoteDataMutex);
    EnsureActiveImage();

    if (!mActiveImage)
      return nullptr;
    *aSize = mRemoteData->mSize;
  } else {
    if (!mActiveImage)
      return nullptr;
    *aSize = mActiveImage->GetSize();
  }
  return mActiveImage->DeprecatedGetAsSurface();
}

TemporaryRef<gfx::SourceSurface>
ImageContainer::GetCurrentAsSourceSurface(gfx::IntSize *aSize)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  if (mRemoteData) {
    CrossProcessMutexAutoLock autoLock(*mRemoteDataMutex);
    EnsureActiveImage();

    if (!mActiveImage)
      return nullptr;
    *aSize = mRemoteData->mSize;
  } else {
    if (!mActiveImage)
      return nullptr;
    *aSize = mActiveImage->GetSize();
  }
  return mActiveImage->GetAsSourceSurface();
}

gfx::IntSize
ImageContainer::GetCurrentSize()
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  if (mRemoteData) {
    CrossProcessMutexAutoLock autoLock(*mRemoteDataMutex);

    // We don't need to ensure we have an active image here, as we need to
    // be in the mutex anyway, and this is easiest to return from there.
    return mRemoteData->mSize;
  }

  if (!mActiveImage) {
    return gfx::IntSize(0, 0);
  }

  return mActiveImage->GetSize();
}

void
ImageContainer::SetRemoteImageData(RemoteImageData *aData, CrossProcessMutex *aMutex)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  NS_ASSERTION(!mActiveImage || !aData, "No active image expected when SetRemoteImageData is called with non-NULL aData.");
  NS_ASSERTION(!mRemoteData || !aData, "No remote data expected when SetRemoteImageData is called with non-NULL aData.");

  mRemoteData = aData;

  if (aData) {
    memset(aData, 0, sizeof(RemoteImageData));
  } else {
    mActiveImage = nullptr;
  }

  mRemoteDataMutex = aMutex;
}

void
ImageContainer::EnsureActiveImage()
{
  if (mRemoteData) {
    if (mRemoteData->mWasUpdated) {
      mActiveImage = nullptr;
    }

    if (mRemoteData->mType == RemoteImageData::RAW_BITMAP &&
        mRemoteData->mBitmap.mData && !mActiveImage) {
      nsRefPtr<RemoteBitmapImage> newImg = new RemoteBitmapImage();
      
      newImg->mFormat = mRemoteData->mFormat;
      newImg->mData = mRemoteData->mBitmap.mData;
      newImg->mSize = mRemoteData->mSize;
      newImg->mStride = mRemoteData->mBitmap.mStride;
      mRemoteData->mWasUpdated = false;
              
      mActiveImage = newImg;
    }
#ifdef XP_WIN
    else if (mRemoteData->mType == RemoteImageData::DXGI_TEXTURE_HANDLE &&
             mRemoteData->mTextureHandle && !mActiveImage) {
      nsRefPtr<RemoteDXGITextureImage> newImg = new RemoteDXGITextureImage();
      newImg->mSize = mRemoteData->mSize;
      newImg->mHandle = mRemoteData->mTextureHandle;
      newImg->mFormat = mRemoteData->mFormat;
      mRemoteData->mWasUpdated = false;

      mActiveImage = newImg;
    }
#endif
  }
}


PlanarYCbCrImage::PlanarYCbCrImage(BufferRecycleBin *aRecycleBin)
  : Image(nullptr, ImageFormat::PLANAR_YCBCR)
  , mBufferSize(0)
  , mOffscreenFormat(gfxImageFormat::Unknown)
  , mRecycleBin(aRecycleBin)
{
}

PlanarYCbCrImage::~PlanarYCbCrImage()
{
  if (mBuffer) {
    mRecycleBin->RecycleBuffer(mBuffer.forget(), mBufferSize);
  }
}

uint8_t* 
PlanarYCbCrImage::AllocateBuffer(uint32_t aSize)
{
  return mRecycleBin->GetBuffer(aSize); 
}

static void
CopyPlane(uint8_t *aDst, const uint8_t *aSrc,
          const gfx::IntSize &aSize, int32_t aStride, int32_t aSkip)
{
  if (!aSkip) {
    // Fast path: planar input.
    memcpy(aDst, aSrc, aSize.height * aStride);
  } else {
    int32_t height = aSize.height;
    int32_t width = aSize.width;
    for (int y = 0; y < height; ++y) {
      const uint8_t *src = aSrc;
      uint8_t *dst = aDst;
      // Slow path
      for (int x = 0; x < width; ++x) {
        *dst++ = *src++;
        src += aSkip;
      }
      aSrc += aStride;
      aDst += aStride;
    }
  }
}

void
PlanarYCbCrImage::CopyData(const Data& aData)
{
  mData = aData;

  // update buffer size
  size_t size = mData.mCbCrStride * mData.mCbCrSize.height * 2 +
                mData.mYStride * mData.mYSize.height;

  // get new buffer
  mBuffer = AllocateBuffer(size);
  if (!mBuffer)
    return;

  // update buffer size
  mBufferSize = size;

  mData.mYChannel = mBuffer;
  mData.mCbChannel = mData.mYChannel + mData.mYStride * mData.mYSize.height;
  mData.mCrChannel = mData.mCbChannel + mData.mCbCrStride * mData.mCbCrSize.height;

  CopyPlane(mData.mYChannel, aData.mYChannel,
            mData.mYSize, mData.mYStride, mData.mYSkip);
  CopyPlane(mData.mCbChannel, aData.mCbChannel,
            mData.mCbCrSize, mData.mCbCrStride, mData.mCbSkip);
  CopyPlane(mData.mCrChannel, aData.mCrChannel,
            mData.mCbCrSize, mData.mCbCrStride, mData.mCrSkip);

  mSize = aData.mPicSize;
}

void
PlanarYCbCrImage::SetData(const Data &aData)
{
  CopyData(aData);
}

gfxImageFormat
PlanarYCbCrImage::GetOffscreenFormat()
{
  return mOffscreenFormat == gfxImageFormat::Unknown ?
    gfxPlatform::GetPlatform()->GetOffscreenFormat() :
    mOffscreenFormat;
}

void
PlanarYCbCrImage::SetDataNoCopy(const Data &aData)
{
  mData = aData;
  mSize = aData.mPicSize;
}

uint8_t*
PlanarYCbCrImage::AllocateAndGetNewBuffer(uint32_t aSize)
{
  // get new buffer
  mBuffer = AllocateBuffer(aSize);
  if (mBuffer) {
    // update buffer size
    mBufferSize = aSize;
  }
  return mBuffer;
}

already_AddRefed<gfxASurface>
PlanarYCbCrImage::DeprecatedGetAsSurface()
{
  if (mDeprecatedSurface) {
    nsRefPtr<gfxASurface> result = mDeprecatedSurface.get();
    return result.forget();
  }

  gfx::SurfaceFormat format = gfx::ImageFormatToSurfaceFormat(GetOffscreenFormat());
  gfx::IntSize size(mSize);
  gfx::GetYCbCrToRGBDestFormatAndSize(mData, format, size);
  if (size.width > PlanarYCbCrImage::MAX_DIMENSION ||
      size.height > PlanarYCbCrImage::MAX_DIMENSION) {
    NS_ERROR("Illegal image dest width or height");
    return nullptr;
  }

  nsRefPtr<gfxImageSurface> imageSurface =
    new gfxImageSurface(gfx::ThebesIntSize(mSize), gfx::SurfaceFormatToImageFormat(format));

  gfx::ConvertYCbCrToRGB(mData, format, mSize, imageSurface->Data(), imageSurface->Stride());

  mDeprecatedSurface = imageSurface;

  return imageSurface.forget();
}

TemporaryRef<gfx::SourceSurface>
PlanarYCbCrImage::GetAsSourceSurface()
{
  if (mSourceSurface) {
    return mSourceSurface.get();
  }

  gfx::IntSize size(mSize);
  gfx::SurfaceFormat format = gfx::ImageFormatToSurfaceFormat(GetOffscreenFormat());
  gfx::GetYCbCrToRGBDestFormatAndSize(mData, format, size);
  if (mSize.width > PlanarYCbCrImage::MAX_DIMENSION ||
      mSize.height > PlanarYCbCrImage::MAX_DIMENSION) {
    NS_ERROR("Illegal image dest width or height");
    return nullptr;
  }

  RefPtr<gfx::DataSourceSurface> surface = gfx::Factory::CreateDataSourceSurface(size, format);

  gfx::ConvertYCbCrToRGB(mData, format, size, surface->GetData(), surface->Stride());

  mSourceSurface = surface;

  return surface.forget();
}

already_AddRefed<gfxASurface>
RemoteBitmapImage::DeprecatedGetAsSurface()
{
  nsRefPtr<gfxImageSurface> newSurf =
    new gfxImageSurface(ThebesIntSize(mSize),
    mFormat == RemoteImageData::BGRX32 ? gfxImageFormat::RGB24 : gfxImageFormat::ARGB32);

  for (int y = 0; y < mSize.height; y++) {
    memcpy(newSurf->Data() + newSurf->Stride() * y,
           mData + mStride * y,
           mSize.width * 4);
  }

  return newSurf.forget();
}

TemporaryRef<gfx::SourceSurface>
RemoteBitmapImage::GetAsSourceSurface()
{
  gfx::SurfaceFormat fmt = mFormat == RemoteImageData::BGRX32
                         ? gfx::SurfaceFormat::B8G8R8X8
                         : gfx::SurfaceFormat::B8G8R8A8;
  RefPtr<gfx::DataSourceSurface> newSurf = gfx::Factory::CreateDataSourceSurface(mSize, fmt);

  for (int y = 0; y < mSize.height; y++) {
    memcpy(newSurf->GetData() + newSurf->Stride() * y,
           mData + mStride * y,
           mSize.width * 4);
  }

  return newSurf;
}

} // namespace
} // namespace
