/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//  * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/process.h"
#include "GLContext.h"
#include "gfx2DGlue.h"
#include <ui/GraphicBuffer.h>
#include "GrallocImages.h"  // for GrallocImage
#include "mozilla/layers/GrallocTextureHost.h"
#include "mozilla/layers/CompositorOGL.h"
#include "mozilla/layers/SharedBufferManagerParent.h"
#include "EGLImageHelpers.h"
#include "GLReadTexImageHelper.h"

namespace mozilla {
namespace layers {

using namespace android;

static gfx::SurfaceFormat
SurfaceFormatForAndroidPixelFormat(android::PixelFormat aFormat,
                                   TextureFlags aFlags)
{
  bool swapRB = bool(aFlags & TextureFlags::RB_SWAPPED);
  switch (aFormat) {
  case android::PIXEL_FORMAT_BGRA_8888:
    return swapRB ? gfx::SurfaceFormat::R8G8B8A8 : gfx::SurfaceFormat::B8G8R8A8;
  case android::PIXEL_FORMAT_RGBA_8888:
    return swapRB ? gfx::SurfaceFormat::B8G8R8A8 : gfx::SurfaceFormat::R8G8B8A8;
  case android::PIXEL_FORMAT_RGBX_8888:
    return swapRB ? gfx::SurfaceFormat::B8G8R8X8 : gfx::SurfaceFormat::R8G8B8X8;
  case android::PIXEL_FORMAT_RGB_565:
    return gfx::SurfaceFormat::R5G6B5;
  case HAL_PIXEL_FORMAT_YCbCr_422_SP:
  case HAL_PIXEL_FORMAT_YCrCb_420_SP:
  case HAL_PIXEL_FORMAT_YCbCr_422_I:
  case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED:
  case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS:
  case HAL_PIXEL_FORMAT_YV12:
    return gfx::SurfaceFormat::R8G8B8A8; // yup, use SurfaceFormat::R8G8B8A8 even though it's a YUV texture. This is an external texture.
  default:
    if (aFormat >= 0x100 && aFormat <= 0x1FF) {
      // Reserved range for HAL specific formats.
      return gfx::SurfaceFormat::R8G8B8A8;
    } else {
      // This is not super-unreachable, there's a bunch of hypothetical pixel
      // formats we don't deal with.
      // We only want to abort in debug builds here, since if we crash here
      // we'll take down the compositor process and thus the phone. This seems
      // like undesirable behaviour. We'd rather have a subtle artifact.
      printf_stderr(" xxxxx unknow android format %i\n", (int)aFormat);
      MOZ_ASSERT(false, "Unknown Android pixel format.");
      return gfx::SurfaceFormat::UNKNOWN;
    }
  }
}

static GLenum
TextureTargetForAndroidPixelFormat(android::PixelFormat aFormat)
{
  switch (aFormat) {
  case HAL_PIXEL_FORMAT_YCbCr_422_SP:
  case HAL_PIXEL_FORMAT_YCrCb_420_SP:
  case HAL_PIXEL_FORMAT_YCbCr_422_I:
  case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED:
  case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS:
  case HAL_PIXEL_FORMAT_YV12:
    return LOCAL_GL_TEXTURE_EXTERNAL;
  case android::PIXEL_FORMAT_BGRA_8888:
  case android::PIXEL_FORMAT_RGBA_8888:
  case android::PIXEL_FORMAT_RGBX_8888:
  case android::PIXEL_FORMAT_RGB_565:
    return LOCAL_GL_TEXTURE_2D;
  default:
    if (aFormat >= 0x100 && aFormat <= 0x1FF) {
      // Reserved range for HAL specific formats.
      return LOCAL_GL_TEXTURE_EXTERNAL;
    } else {
      // This is not super-unreachable, there's a bunch of hypothetical pixel
      // formats we don't deal with.
      // We only want to abort in debug builds here, since if we crash here
      // we'll take down the compositor process and thus the phone. This seems
      // like undesirable behaviour. We'd rather have a subtle artifact.
      MOZ_ASSERT(false, "Unknown Android pixel format.");
      return LOCAL_GL_TEXTURE_EXTERNAL;
    }
  }
}

GrallocTextureSourceOGL::GrallocTextureSourceOGL(CompositorOGL* aCompositor,
                                                 GrallocTextureHostOGL* aTextureHost,
                                                 android::GraphicBuffer* aGraphicBuffer,
                                                 gfx::SurfaceFormat aFormat)
  : mCompositor(aCompositor)
  , mTextureHost(aTextureHost)
  , mGraphicBuffer(aGraphicBuffer)
  , mEGLImage(0)
  , mFormat(aFormat)
  , mNeedsReset(true)
{
  MOZ_ASSERT(mGraphicBuffer.get());
}

GrallocTextureSourceOGL::~GrallocTextureSourceOGL()
{
  DeallocateDeviceData();
  mCompositor = nullptr;
}

void
GrallocTextureSourceOGL::BindTexture(GLenum aTextureUnit, gfx::Filter aFilter)
{
  /*
   * The job of this function is to ensure that the texture is tied to the
   * android::GraphicBuffer, so that texturing will source the GraphicBuffer.
   *
   * To this effect we create an EGLImage wrapping this GraphicBuffer,
   * using EGLImageCreateFromNativeBuffer, and then we tie this EGLImage to our
   * texture using fEGLImageTargetTexture2D.
   */
  MOZ_ASSERT(gl());
  if (!IsValid()) {
    return;
  }
  gl()->MakeCurrent();

  GLuint tex = GetGLTexture();
  GLuint textureTarget = GetTextureTarget();

  gl()->fActiveTexture(aTextureUnit);
  gl()->fBindTexture(textureTarget, tex);

  if (mCompositableBackendData) {
    // There are two paths for locking/unlocking - if mCompositableBackendData is
    // set, we use the texture on there, otherwise we use
    // CompositorBackendSpecificData from the compositor and bind the EGLImage
    // only in Lock().
    if (!mEGLImage) {
      mEGLImage = EGLImageCreateFromNativeBuffer(gl(), mGraphicBuffer->getNativeBuffer());
    }
    BindEGLImage();
  }

  ApplyFilterToBoundTexture(gl(), aFilter, textureTarget);

#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
  if (mTextureHost) {
    // Wait until it's ready.
    mTextureHost->WaitAcquireFenceSyncComplete();
  }
#endif
}

void GrallocTextureSourceOGL::Lock()
{
  if (mCompositableBackendData) return;

  MOZ_ASSERT(IsValid());

  mTexture = mCompositor->GetTemporaryTexture(GetTextureTarget(), LOCAL_GL_TEXTURE0);

  GLuint textureTarget = GetTextureTarget();

  gl()->MakeCurrent();
  gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
  gl()->fBindTexture(textureTarget, mTexture);
  if (!mEGLImage) {
    mEGLImage = EGLImageCreateFromNativeBuffer(gl(), mGraphicBuffer->getNativeBuffer());
  }
  gl()->fEGLImageTargetTexture2D(textureTarget, mEGLImage);
}

bool
GrallocTextureSourceOGL::IsValid() const
{
  return !!gl() && !!mGraphicBuffer.get() && (!!mCompositor || !!mCompositableBackendData);
}

gl::GLContext*
GrallocTextureSourceOGL::gl() const
{
  return mCompositor ? mCompositor->gl() : nullptr;
}

void
GrallocTextureSourceOGL::SetCompositor(Compositor* aCompositor)
{
  if (mCompositor && !aCompositor) {
    DeallocateDeviceData();
  }
  mCompositor = static_cast<CompositorOGL*>(aCompositor);
}


GLenum
GrallocTextureSourceOGL::GetTextureTarget() const
{
  MOZ_ASSERT(gl());
  MOZ_ASSERT(mGraphicBuffer.get());

  if (!gl() || !mGraphicBuffer.get()) {
    return LOCAL_GL_TEXTURE_EXTERNAL;
  }

  // SGX has a quirk that only TEXTURE_EXTERNAL works and any other value will
  // result in black pixels when trying to draw from bound textures.
  // Unfortunately, using TEXTURE_EXTERNAL on Adreno has a terrible effect on
  // performance.
  // See Bug 950050.
  if (gl()->Renderer() == gl::GLRenderer::SGX530 ||
      gl()->Renderer() == gl::GLRenderer::SGX540) {
    return LOCAL_GL_TEXTURE_EXTERNAL;
  }

  return TextureTargetForAndroidPixelFormat(mGraphicBuffer->getPixelFormat());
}

void
GrallocTextureSourceOGL::SetCompositableBackendSpecificData(CompositableBackendSpecificData* aBackendData)
{
  if (!aBackendData) {
    DeallocateDeviceData();
    // Update mCompositableBackendData after calling DeallocateDeviceData().
    mCompositableBackendData = nullptr;
    return;
  }

  if (mCompositableBackendData != aBackendData) {
    mNeedsReset = true;
  }

  if (!mNeedsReset) {
    // Update binding to the EGLImage
    gl()->MakeCurrent();
    GLuint tex = GetGLTexture();
    GLuint textureTarget = GetTextureTarget();
    gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
    gl()->fBindTexture(textureTarget, tex);
    BindEGLImage();
    return;
  }

  if (!mCompositor) {
    mCompositableBackendData = aBackendData;
    return;
  }

  // delete old EGLImage
  DeallocateDeviceData();

  // Update mCompositableBackendData after calling DeallocateDeviceData().
  mCompositableBackendData = aBackendData;

  gl()->MakeCurrent();
  GLuint tex = GetGLTexture();
  GLuint textureTarget = GetTextureTarget();

  gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
  gl()->fBindTexture(textureTarget, tex);
  // create new EGLImage
  mEGLImage = EGLImageCreateFromNativeBuffer(gl(), mGraphicBuffer->getNativeBuffer());
  BindEGLImage();
  mNeedsReset = false;
}

gfx::IntSize
GrallocTextureSourceOGL::GetSize() const
{
  if (!IsValid()) {
    NS_WARNING("Trying to access the size of an invalid GrallocTextureSourceOGL");
    return gfx::IntSize(0, 0);
  }
  return gfx::IntSize(mGraphicBuffer->getWidth(), mGraphicBuffer->getHeight());
}

void
GrallocTextureSourceOGL::DeallocateDeviceData()
{
  if (mEGLImage) {
    MOZ_ASSERT(gl());
    gl()->MakeCurrent();
    if (mCompositableBackendData) {
      CompositableDataGonkOGL* backend = static_cast<CompositableDataGonkOGL*>(mCompositableBackendData.get());
      backend->ClearBoundEGLImage(mEGLImage);
    }
    EGLImageDestroy(gl(), mEGLImage);
    mEGLImage = EGL_NO_IMAGE;
  }
}

GrallocTextureHostOGL::GrallocTextureHostOGL(TextureFlags aFlags,
                                             const NewSurfaceDescriptorGralloc& aDescriptor)
  : TextureHost(aFlags)
{
  gfx::SurfaceFormat format = gfx::SurfaceFormat::UNKNOWN;
  mGrallocHandle = aDescriptor;

  android::GraphicBuffer* graphicBuffer = GetGraphicBufferFromDesc(mGrallocHandle).get();
  MOZ_ASSERT(graphicBuffer);

  mSize = aDescriptor.size();
  if (graphicBuffer) {
    format =
      SurfaceFormatForAndroidPixelFormat(graphicBuffer->getPixelFormat(),
                                         aFlags & TextureFlags::RB_SWAPPED);
  } else {
    NS_WARNING("gralloc buffer is nullptr");
  }
  mTextureSource = new GrallocTextureSourceOGL(nullptr,
                                               this,
                                               graphicBuffer,
                                               format);
}

GrallocTextureHostOGL::~GrallocTextureHostOGL()
{
  mTextureSource = nullptr;
}

void
GrallocTextureHostOGL::SetCompositor(Compositor* aCompositor)
{
  mTextureSource->SetCompositor(static_cast<CompositorOGL*>(aCompositor));
}

bool
GrallocTextureHostOGL::Lock()
{
  if (IsValid()) {
    mTextureSource->Lock();
    return true;
  }
  return false;
}

void
GrallocTextureHostOGL::Unlock()
{
  // Unlock is done internally by binding the texture to another gralloc buffer
}

bool
GrallocTextureHostOGL::IsValid() const
{
  return mTextureSource->IsValid();
}

gfx::SurfaceFormat
GrallocTextureHostOGL::GetFormat() const
{
  return mTextureSource->GetFormat();
}

void
GrallocTextureHostOGL::DeallocateSharedData()
{
  if (mTextureSource) {
    mTextureSource->ForgetBuffer();
  }
  if (mGrallocHandle.buffer().type() != SurfaceDescriptor::Tnull_t) {
    MaybeMagicGrallocBufferHandle handle = mGrallocHandle.buffer();
    base::ProcessId owner;
    if (handle.type() == MaybeMagicGrallocBufferHandle::TGrallocBufferRef) {
      owner = handle.get_GrallocBufferRef().mOwner;
    }
    else {
      owner = handle.get_MagicGrallocBufferHandle().mRef.mOwner;
    }

    SharedBufferManagerParent::GetInstance(owner)->DropGrallocBuffer(mGrallocHandle);
  }
}

void
GrallocTextureHostOGL::ForgetSharedData()
{
  if (mTextureSource) {
    mTextureSource->ForgetBuffer();
  }
}

void
GrallocTextureHostOGL::DeallocateDeviceData()
{
  mTextureSource->DeallocateDeviceData();
}

LayerRenderState
GrallocTextureHostOGL::GetRenderState()
{
  if (IsValid()) {
    LayerRenderStateFlags flags = LayerRenderStateFlags::LAYER_RENDER_STATE_DEFAULT;
    if (mFlags & TextureFlags::NEEDS_Y_FLIP) {
      flags |= LayerRenderStateFlags::Y_FLIPPED;
    }
    if (mFlags & TextureFlags::RB_SWAPPED) {
      flags |= LayerRenderStateFlags::FORMAT_RB_SWAP;
    }
    return LayerRenderState(mTextureSource->mGraphicBuffer.get(),
                            gfx::ThebesIntSize(mSize),
                            flags,
                            this);
  }

  return LayerRenderState();
}

TemporaryRef<gfx::DataSourceSurface>
GrallocTextureHostOGL::GetAsSurface() {
  return mTextureSource ? mTextureSource->GetAsSurface()
                        : nullptr;
}

TemporaryRef<gfx::DataSourceSurface>
GrallocTextureSourceOGL::GetAsSurface() {
  if (!IsValid()) {
    return nullptr;
  }
  gl()->MakeCurrent();

  GLuint tex = GetGLTexture();
  gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
  gl()->fBindTexture(GetTextureTarget(), tex);
  if (!mEGLImage) {
    mEGLImage = EGLImageCreateFromNativeBuffer(gl(), mGraphicBuffer->getNativeBuffer());
  }
  BindEGLImage();

  RefPtr<gfx::DataSourceSurface> surf =
    IsValid() ? ReadBackSurface(gl(), tex, false, GetFormat())
              : nullptr;

  gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
  return surf.forget();
}

GLuint
GrallocTextureSourceOGL::GetGLTexture()
{
  if (mCompositableBackendData) {
    mCompositableBackendData->SetCompositor(mCompositor);
    return static_cast<CompositableDataGonkOGL*>(mCompositableBackendData.get())->GetTexture();
  }

  return mTexture;
}

void
GrallocTextureSourceOGL::BindEGLImage()
{
  if (mCompositableBackendData) {
    CompositableDataGonkOGL* backend = static_cast<CompositableDataGonkOGL*>(mCompositableBackendData.get());
    backend->BindEGLImage(GetTextureTarget(), mEGLImage);
  } else {
    gl()->fEGLImageTargetTexture2D(GetTextureTarget(), mEGLImage);
  }
}

void
GrallocTextureHostOGL::SetCompositableBackendSpecificData(CompositableBackendSpecificData* aBackendData)
{
  mCompositableBackendData = aBackendData;
  if (mTextureSource) {
    mTextureSource->SetCompositableBackendSpecificData(aBackendData);
  }
}

} // namepsace layers
} // namepsace mozilla
