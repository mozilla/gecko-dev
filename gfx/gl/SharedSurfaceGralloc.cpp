/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Preferences.h"

#include "SharedSurfaceGralloc.h"

#include "GLContext.h"
#include "SharedSurfaceGL.h"
#include "SurfaceFactory.h"
#include "GLLibraryEGL.h"
#include "mozilla/layers/GrallocTextureClient.h"
#include "mozilla/layers/ShadowLayers.h"

#include "ui/GraphicBuffer.h"
#include "../layers/ipc/ShadowLayers.h"
#include "ScopedGLHelpers.h"

#include "gfxPlatform.h"
#include "gfx2DGlue.h"
#include "gfxPrefs.h"

#define DEBUG_GRALLOC
#ifdef DEBUG_GRALLOC
#define DEBUG_PRINT(...) do { printf_stderr(__VA_ARGS__); } while (0)
#else
#define DEBUG_PRINT(...) do { } while (0)
#endif

namespace mozilla {
namespace gl {

using namespace mozilla::gfx;
using namespace mozilla::layers;
using namespace android;

SurfaceFactory_Gralloc::SurfaceFactory_Gralloc(GLContext* prodGL,
                                               const SurfaceCaps& caps,
                                               layers::ISurfaceAllocator* allocator)
    : SurfaceFactory_GL(prodGL, SharedSurfaceType::Gralloc, caps)
{
    if (caps.surfaceAllocator) {
        allocator = caps.surfaceAllocator;
    }

    MOZ_ASSERT(allocator);

    mAllocator = allocator;
}

SharedSurface_Gralloc*
SharedSurface_Gralloc::Create(GLContext* prodGL,
                              const GLFormats& formats,
                              const gfx::IntSize& size,
                              bool hasAlpha,
                              ISurfaceAllocator* allocator)
{
    GLLibraryEGL* egl = &sEGLLibrary;
    MOZ_ASSERT(egl);

    DEBUG_PRINT("SharedSurface_Gralloc::Create -------\n");

    if (!HasExtensions(egl, prodGL))
        return nullptr;

    gfxContentType type = hasAlpha ? gfxContentType::COLOR_ALPHA
                                   : gfxContentType::COLOR;

    gfxImageFormat format
      = gfxPlatform::GetPlatform()->OptimalFormatForContent(type);

    RefPtr<GrallocTextureClientOGL> grallocTC =
      new GrallocTextureClientOGL(
          allocator,
          gfx::ImageFormatToSurfaceFormat(format),
          gfx::BackendType::NONE, // we don't need to use it with a DrawTarget
          layers::TextureFlags::DEFAULT);

    if (!grallocTC->AllocateForGLRendering(size)) {
      return nullptr;
    }

    sp<GraphicBuffer> buffer = grallocTC->GetGraphicBuffer();

    EGLDisplay display = egl->Display();
    EGLClientBuffer clientBuffer = buffer->getNativeBuffer();
    EGLint attrs[] = {
        LOCAL_EGL_NONE, LOCAL_EGL_NONE
    };
    EGLImage image = egl->fCreateImage(display,
                                       EGL_NO_CONTEXT,
                                       LOCAL_EGL_NATIVE_BUFFER_ANDROID,
                                       clientBuffer, attrs);
    if (!image) {
        return nullptr;
    }

    prodGL->MakeCurrent();
    GLuint prodTex = 0;
    prodGL->fGenTextures(1, &prodTex);
    ScopedBindTexture autoTex(prodGL, prodTex);

    prodGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
    prodGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
    prodGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
    prodGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);

    prodGL->fEGLImageTargetTexture2D(LOCAL_GL_TEXTURE_2D, image);

    egl->fDestroyImage(display, image);

    SharedSurface_Gralloc *surf = new SharedSurface_Gralloc(prodGL, size, hasAlpha, egl, allocator, grallocTC, prodTex);

    DEBUG_PRINT("SharedSurface_Gralloc::Create: success -- surface %p, GraphicBuffer %p.\n", surf, buffer.get());

    return surf;
}


bool
SharedSurface_Gralloc::HasExtensions(GLLibraryEGL* egl, GLContext* gl)
{
    return egl->HasKHRImageBase() &&
           gl->IsExtensionSupported(GLContext::OES_EGL_image);
}

SharedSurface_Gralloc::~SharedSurface_Gralloc()
{

    DEBUG_PRINT("[SharedSurface_Gralloc %p] destroyed\n", this);

    mGL->MakeCurrent();
    mGL->fDeleteTextures(1, &mProdTex);

    if (mSync) {
        MOZ_ALWAYS_TRUE( mEGL->fDestroySync(mEGL->Display(), mSync) );
        mSync = 0;
    }
}

void
SharedSurface_Gralloc::Fence()
{
    if (mSync) {
        MOZ_ALWAYS_TRUE( mEGL->fDestroySync(mEGL->Display(), mSync) );
        mSync = 0;
    }

    // When Android native fences are available, try
    // them first since they're more likely to work.
    // Android native fences are also likely to perform better.
    if (mEGL->IsExtensionSupported(GLLibraryEGL::ANDROID_native_fence_sync)) {
        mGL->MakeCurrent();
        EGLSync sync = mEGL->fCreateSync(mEGL->Display(),
                                         LOCAL_EGL_SYNC_NATIVE_FENCE_ANDROID,
                                         nullptr);
        if (sync) {
            mGL->fFlush();
#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
            int fenceFd = mEGL->fDupNativeFenceFDANDROID(mEGL->Display(), sync);
            if (fenceFd != -1) {
                mEGL->fDestroySync(mEGL->Display(), sync);
                android::sp<android::Fence> fence(new android::Fence(fenceFd));
                FenceHandle handle = FenceHandle(fence);
                mTextureClient->SetAcquireFenceHandle(handle);
            } else {
                mSync = sync;
            }
#else
            mSync = sync;
#endif
            return;
        }
    }

    if (mEGL->IsExtensionSupported(GLLibraryEGL::KHR_fence_sync)) {
        mGL->MakeCurrent();
        mSync = mEGL->fCreateSync(mEGL->Display(),
                                  LOCAL_EGL_SYNC_FENCE,
                                  nullptr);
        if (mSync) {
            mGL->fFlush();
            return;
        }
    }

    // We should be able to rely on genlock write locks/read locks.
    // But they're broken on some configs, and even a glFinish doesn't
    // work.  glReadPixels seems to, though.
    if (gfxPrefs::GrallocFenceWithReadPixels()) {
        mGL->MakeCurrent();
        ScopedDeleteArray<char> buf(new char[4]);
        mGL->fReadPixels(0, 0, 1, 1, LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE, buf);
    }
}

bool
SharedSurface_Gralloc::WaitSync()
{
    if (!mSync) {
        // We must not be needed.
        return true;
    }
    MOZ_ASSERT(mEGL->IsExtensionSupported(GLLibraryEGL::KHR_fence_sync));

    EGLint status = mEGL->fClientWaitSync(mEGL->Display(),
                                          mSync,
                                          0,
                                          LOCAL_EGL_FOREVER);

    if (status != LOCAL_EGL_CONDITION_SATISFIED) {
        return false;
    }

    MOZ_ALWAYS_TRUE( mEGL->fDestroySync(mEGL->Display(), mSync) );
    mSync = 0;

    return true;
}

void
SharedSurface_Gralloc::WaitForBufferOwnership()
{
    mTextureClient->WaitForBufferOwnership();
}

}
}
