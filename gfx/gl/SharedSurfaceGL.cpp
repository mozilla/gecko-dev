/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSurfaceGL.h"
#include "GLContext.h"
#include "GLBlitHelper.h"
#include "ScopedGLHelpers.h"
#include "mozilla/gfx/2D.h"
#include "GLReadTexImageHelper.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace gl {

// |src| must begin and end locked, though we may
// temporarily unlock it if we need to.
void
SharedSurface_GL::ProdCopy(SharedSurface_GL* src, SharedSurface_GL* dest,
                           SurfaceFactory_GL* factory)
{
    GLContext* gl = src->GL();

    gl->MakeCurrent();

    if (src->AttachType() == AttachmentType::Screen &&
        dest->AttachType() == AttachmentType::Screen)
    {
        // Here, we actually need to blit through a temp surface, so let's make one.
        nsAutoPtr<SharedSurface_GLTexture> tempSurf(
            SharedSurface_GLTexture::Create(gl, gl,
                                            factory->Formats(),
                                            src->Size(),
                                            factory->Caps().alpha));

        ProdCopy(src, tempSurf, factory);
        ProdCopy(tempSurf, dest, factory);
        return;
    }

    if (src->AttachType() == AttachmentType::Screen) {
        SharedSurface_GL* origLocked = gl->GetLockedSurface();
        bool srcNeedsUnlock = false;
        bool origNeedsRelock = false;
        if (origLocked != src) {
            if (origLocked) {
                origLocked->UnlockProd();
                origNeedsRelock = true;
            }

            src->LockProd();
            srcNeedsUnlock = true;
        }

        if (dest->AttachType() == AttachmentType::GLTexture) {
            GLuint destTex = dest->ProdTexture();
            GLenum destTarget = dest->ProdTextureTarget();

            gl->BlitHelper()->BlitFramebufferToTexture(0, destTex, src->Size(), dest->Size(), destTarget);
        } else if (dest->AttachType() == AttachmentType::GLRenderbuffer) {
            GLuint destRB = dest->ProdRenderbuffer();
            ScopedFramebufferForRenderbuffer destWrapper(gl, destRB);

            gl->BlitHelper()->BlitFramebufferToFramebuffer(0, destWrapper.FB(),
                                                           src->Size(), dest->Size());
        } else {
            MOZ_CRASH("Unhandled dest->AttachType().");
        }

        if (srcNeedsUnlock)
            src->UnlockProd();

        if (origNeedsRelock)
            origLocked->LockProd();

        return;
    }

    if (dest->AttachType() == AttachmentType::Screen) {
        SharedSurface_GL* origLocked = gl->GetLockedSurface();
        bool destNeedsUnlock = false;
        bool origNeedsRelock = false;
        if (origLocked != dest) {
            if (origLocked) {
                origLocked->UnlockProd();
                origNeedsRelock = true;
            }

            dest->LockProd();
            destNeedsUnlock = true;
        }

        if (src->AttachType() == AttachmentType::GLTexture) {
            GLuint srcTex = src->ProdTexture();
            GLenum srcTarget = src->ProdTextureTarget();

            gl->BlitHelper()->BlitTextureToFramebuffer(srcTex, 0, src->Size(), dest->Size(), srcTarget);
        } else if (src->AttachType() == AttachmentType::GLRenderbuffer) {
            GLuint srcRB = src->ProdRenderbuffer();
            ScopedFramebufferForRenderbuffer srcWrapper(gl, srcRB);

            gl->BlitHelper()->BlitFramebufferToFramebuffer(srcWrapper.FB(), 0,
                                                           src->Size(), dest->Size());
        } else {
            MOZ_CRASH("Unhandled src->AttachType().");
        }

        if (destNeedsUnlock)
            dest->UnlockProd();

        if (origNeedsRelock)
            origLocked->LockProd();

        return;
    }

    // Alright, done with cases involving Screen types.
    // Only {src,dest}x{texture,renderbuffer} left.

    if (src->AttachType() == AttachmentType::GLTexture) {
        GLuint srcTex = src->ProdTexture();
        GLenum srcTarget = src->ProdTextureTarget();

        if (dest->AttachType() == AttachmentType::GLTexture) {
            GLuint destTex = dest->ProdTexture();
            GLenum destTarget = dest->ProdTextureTarget();

            gl->BlitHelper()->BlitTextureToTexture(srcTex, destTex,
                                                   src->Size(), dest->Size(),
                                                   srcTarget, destTarget);

            return;
        }

        if (dest->AttachType() == AttachmentType::GLRenderbuffer) {
            GLuint destRB = dest->ProdRenderbuffer();
            ScopedFramebufferForRenderbuffer destWrapper(gl, destRB);

            gl->BlitHelper()->BlitTextureToFramebuffer(srcTex, destWrapper.FB(),
                                                       src->Size(), dest->Size(), srcTarget);

            return;
        }

        MOZ_CRASH("Unhandled dest->AttachType().");
    }

    if (src->AttachType() == AttachmentType::GLRenderbuffer) {
        GLuint srcRB = src->ProdRenderbuffer();
        ScopedFramebufferForRenderbuffer srcWrapper(gl, srcRB);

        if (dest->AttachType() == AttachmentType::GLTexture) {
            GLuint destTex = dest->ProdTexture();
            GLenum destTarget = dest->ProdTextureTarget();

            gl->BlitHelper()->BlitFramebufferToTexture(srcWrapper.FB(), destTex,
                                                       src->Size(), dest->Size(), destTarget);

            return;
        }

        if (dest->AttachType() == AttachmentType::GLRenderbuffer) {
            GLuint destRB = dest->ProdRenderbuffer();
            ScopedFramebufferForRenderbuffer destWrapper(gl, destRB);

            gl->BlitHelper()->BlitFramebufferToFramebuffer(srcWrapper.FB(), destWrapper.FB(),
                                                           src->Size(), dest->Size());

            return;
        }

        MOZ_CRASH("Unhandled dest->AttachType().");
    }

    MOZ_CRASH("Unhandled src->AttachType().");
}

void
SharedSurface_GL::LockProd()
{
    MOZ_ASSERT(!mIsLocked);

    LockProdImpl();

    mGL->LockSurface(this);
    mIsLocked = true;
}

void
SharedSurface_GL::UnlockProd()
{
    if (!mIsLocked)
        return;

    UnlockProdImpl();

    mGL->UnlockSurface(this);
    mIsLocked = false;
}


SurfaceFactory_GL::SurfaceFactory_GL(GLContext* gl,
                                     SharedSurfaceType type,
                                     const SurfaceCaps& caps)
    : SurfaceFactory(type, caps)
    , mGL(gl)
    , mFormats(gl->ChooseGLFormats(caps))
{
    ChooseBufferBits(caps, mDrawCaps, mReadCaps);
}

void
SurfaceFactory_GL::ChooseBufferBits(const SurfaceCaps& caps,
                                    SurfaceCaps& drawCaps,
                                    SurfaceCaps& readCaps) const
{
    SurfaceCaps screenCaps;

    screenCaps.color = caps.color;
    screenCaps.alpha = caps.alpha;
    screenCaps.bpp16 = caps.bpp16;

    screenCaps.depth = caps.depth;
    screenCaps.stencil = caps.stencil;

    screenCaps.antialias = caps.antialias;
    screenCaps.preserve = caps.preserve;

    if (caps.antialias) {
        drawCaps = screenCaps;
        readCaps.Clear();

        // Color caps need to be duplicated in readCaps.
        readCaps.color = caps.color;
        readCaps.alpha = caps.alpha;
        readCaps.bpp16 = caps.bpp16;
    } else {
        drawCaps.Clear();
        readCaps = screenCaps;
    }
}


SharedSurface_Basic*
SharedSurface_Basic::Create(GLContext* gl,
                            const GLFormats& formats,
                            const IntSize& size,
                            bool hasAlpha)
{
    gl->MakeCurrent();
    GLuint tex = CreateTexture(gl, formats.color_texInternalFormat,
                               formats.color_texFormat,
                               formats.color_texType,
                               size);

    SurfaceFormat format = SurfaceFormat::B8G8R8X8;
    switch (formats.color_texInternalFormat) {
    case LOCAL_GL_RGB:
    case LOCAL_GL_RGB8:
        if (formats.color_texType == LOCAL_GL_UNSIGNED_SHORT_5_6_5)
            format = SurfaceFormat::R5G6B5;
        else
            format = SurfaceFormat::B8G8R8X8;
        break;
    case LOCAL_GL_RGBA:
    case LOCAL_GL_RGBA8:
        format = SurfaceFormat::B8G8R8A8;
        break;
    default:
        MOZ_CRASH("Unhandled Tex format.");
    }
    return new SharedSurface_Basic(gl, size, hasAlpha, format, tex);
}

SharedSurface_Basic::SharedSurface_Basic(GLContext* gl,
                                         const IntSize& size,
                                         bool hasAlpha,
                                         SurfaceFormat format,
                                         GLuint tex)
    : SharedSurface_GL(SharedSurfaceType::Basic,
                       AttachmentType::GLTexture,
                       gl,
                       size,
                       hasAlpha)
    , mTex(tex), mFB(0)
{
    mGL->MakeCurrent();
    mGL->fGenFramebuffers(1, &mFB);

    ScopedBindFramebuffer autoFB(mGL, mFB);
    mGL->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER,
                              LOCAL_GL_COLOR_ATTACHMENT0,
                              LOCAL_GL_TEXTURE_2D,
                              mTex,
                              0);

    GLenum status = mGL->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
    if (status != LOCAL_GL_FRAMEBUFFER_COMPLETE) {
        mGL->fDeleteFramebuffers(1, &mFB);
        mFB = 0;
    }

    mData = Factory::CreateDataSourceSurfaceWithStride(size, format,
              GetAlignedStride<4>(size.width * BytesPerPixel(format)));
}

SharedSurface_Basic::~SharedSurface_Basic()
{
    if (!mGL->MakeCurrent())
        return;

    if (mFB)
        mGL->fDeleteFramebuffers(1, &mFB);

    mGL->fDeleteTextures(1, &mTex);
}

void
SharedSurface_Basic::Fence()
{
    mGL->MakeCurrent();
    ScopedBindFramebuffer autoFB(mGL, mFB);
    ReadPixelsIntoDataSurface(mGL, mData);
}



SharedSurface_GLTexture*
SharedSurface_GLTexture::Create(GLContext* prodGL,
                                GLContext* consGL,
                                const GLFormats& formats,
                                const gfx::IntSize& size,
                                bool hasAlpha,
                                GLuint texture)
{
    MOZ_ASSERT(prodGL);
    MOZ_ASSERT(!consGL || prodGL->SharesWith(consGL));

    prodGL->MakeCurrent();

    GLuint tex = texture;

    bool ownsTex = false;

    if (!tex) {
      tex = CreateTextureForOffscreen(prodGL, formats, size);
      ownsTex = true;
    }

    return new SharedSurface_GLTexture(prodGL, consGL, size, hasAlpha, tex, ownsTex);
}

SharedSurface_GLTexture::~SharedSurface_GLTexture()
{
    if (!mGL->MakeCurrent())
        return;

    if (mOwnsTex) {
        mGL->fDeleteTextures(1, &mTex);
    }

    if (mSync) {
        mGL->fDeleteSync(mSync);
    }
}

void
SharedSurface_GLTexture::Fence()
{
    MutexAutoLock lock(mMutex);
    mGL->MakeCurrent();

    if (mConsGL && mGL->IsExtensionSupported(GLContext::ARB_sync)) {
        if (mSync) {
            mGL->fDeleteSync(mSync);
            mSync = 0;
        }

        mSync = mGL->fFenceSync(LOCAL_GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        if (mSync) {
            mGL->fFlush();
            return;
        }
    }
    MOZ_ASSERT(!mSync);

    mGL->fFinish();
}

bool
SharedSurface_GLTexture::WaitSync()
{
    MutexAutoLock lock(mMutex);
    if (!mSync) {
        // We must have used glFinish instead of glFenceSync.
        return true;
    }

    mConsGL->MakeCurrent();
    MOZ_ASSERT(mConsGL->IsExtensionSupported(GLContext::ARB_sync));

    mConsGL->fWaitSync(mSync,
                       0,
                       LOCAL_GL_TIMEOUT_IGNORED);
    mConsGL->fDeleteSync(mSync);
    mSync = 0;

    return true;
}

GLuint
SharedSurface_GLTexture::ConsTexture(GLContext* consGL)
{
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(consGL);
    MOZ_ASSERT(mGL->SharesWith(consGL));
    MOZ_ASSERT_IF(mConsGL, consGL == mConsGL);

    mConsGL = consGL;

    return mTex;
}

} /* namespace gfx */
} /* namespace mozilla */
