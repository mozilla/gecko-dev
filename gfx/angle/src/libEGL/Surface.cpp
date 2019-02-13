//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Surface.cpp: Implements the egl::Surface class, representing a drawing surface
// such as the client area of a window, including any back buffers.
// Implements EGLSurface and related functionality. [EGL 1.4] section 2.2 page 3.

#include <tchar.h>

#include <algorithm>

#include "libEGL/Surface.h"

#include "common/debug.h"
#include "libGLESv2/Texture.h"
#include "libGLESv2/renderer/SwapChain.h"
#include "libGLESv2/main.h"

#include "libEGL/main.h"
#include "libEGL/Display.h"

#include "common/NativeWindow.h"

namespace egl
{

Surface::Surface(Display *display, const Config *config, EGLNativeWindowType window, EGLint fixedSize, EGLint width, EGLint height, EGLint postSubBufferSupported) 
    : mDisplay(display), mConfig(config), mNativeWindow(window), mPostSubBufferSupported(postSubBufferSupported)
{
    mRenderer = mDisplay->getRenderer();
    mSwapChain = NULL;
    mShareHandle = NULL;
    mTexture = NULL;
    mTextureFormat = EGL_NO_TEXTURE;
    mTextureTarget = EGL_NO_TEXTURE;

    mPixelAspectRatio = (EGLint)(1.0 * EGL_DISPLAY_SCALING);   // FIXME: Determine actual pixel aspect ratio
    mRenderBuffer = EGL_BACK_BUFFER;
    mSwapBehavior = EGL_BUFFER_PRESERVED;
    mSwapInterval = -1;
    mWidth = width;
    mHeight = height;
    setSwapInterval(1);
    mFixedSize = fixedSize;

    subclassWindow();
}

Surface::Surface(Display *display, const Config *config, HANDLE shareHandle, EGLint width, EGLint height, EGLenum textureFormat, EGLenum textureType)
    : mDisplay(display), mNativeWindow(NULL), mConfig(config), mShareHandle(shareHandle), mWidth(width), mHeight(height), mPostSubBufferSupported(EGL_FALSE)
{
    mRenderer = mDisplay->getRenderer();
    mSwapChain = NULL;
    mWindowSubclassed = false;
    mTexture = NULL;
    mTextureFormat = textureFormat;
    mTextureTarget = textureType;

    mPixelAspectRatio = (EGLint)(1.0 * EGL_DISPLAY_SCALING);   // FIXME: Determine actual pixel aspect ratio
    mRenderBuffer = EGL_BACK_BUFFER;
    mSwapBehavior = EGL_BUFFER_PRESERVED;
    mSwapInterval = -1;
    setSwapInterval(1);
    // This constructor is for offscreen surfaces, which are always fixed-size.
    mFixedSize = EGL_TRUE;
}

Surface::~Surface()
{
    unsubclassWindow();
    release();
}

bool Surface::initialize()
{
    if (mNativeWindow.getNativeWindow())
    {
        if (!mNativeWindow.initialize())
        {
            return false;
        }
    }

    if (!resetSwapChain())
      return false;

    return true;
}

void Surface::release()
{
    delete mSwapChain;
    mSwapChain = NULL;

    if (mTexture)
    {
        mTexture->releaseTexImage();
        mTexture = NULL;
    }
}

bool Surface::resetSwapChain()
{
    ASSERT(!mSwapChain);

    int width;
    int height;

    if (!mFixedSize)
    {
        RECT windowRect;
        if (!mNativeWindow.getClientRect(&windowRect))
        {
            ASSERT(false);

            ERR("Could not retrieve the window dimensions");
            return error(EGL_BAD_SURFACE, false);
        }

        width = windowRect.right - windowRect.left;
        height = windowRect.bottom - windowRect.top;
    }
    else
    {
        // non-window surface - size is determined at creation
        width = mWidth;
        height = mHeight;
    }

    mSwapChain = mRenderer->createSwapChain(mNativeWindow, mShareHandle,
                                            mConfig->mRenderTargetFormat,
                                            mConfig->mDepthStencilFormat);
    if (!mSwapChain)
    {
        return error(EGL_BAD_ALLOC, false);
    }

    if (!resetSwapChain(width, height))
    {
        delete mSwapChain;
        mSwapChain = NULL;
        return false;
    }

    return true;
}

bool Surface::resizeSwapChain(int backbufferWidth, int backbufferHeight)
{
    ASSERT(backbufferWidth >= 0 && backbufferHeight >= 0);
    ASSERT(mSwapChain);

    EGLint status = mSwapChain->resize(std::max(1, backbufferWidth), std::max(1, backbufferHeight));

    if (status == EGL_CONTEXT_LOST)
    {
        mDisplay->notifyDeviceLost();
        return false;
    }
    else if (status != EGL_SUCCESS)
    {
        return error(status, false);
    }

    mWidth = backbufferWidth;
    mHeight = backbufferHeight;

    return true;
}

bool Surface::resetSwapChain(int backbufferWidth, int backbufferHeight)
{
    ASSERT(backbufferWidth >= 0 && backbufferHeight >= 0);
    ASSERT(mSwapChain);

    EGLint status = mSwapChain->reset(std::max(1, backbufferWidth), std::max(1, backbufferHeight), mSwapInterval);

    if (status == EGL_CONTEXT_LOST)
    {
        mRenderer->notifyDeviceLost();
        return false;
    }
    else if (status != EGL_SUCCESS)
    {
        return error(status, false);
    }

    mWidth = backbufferWidth;
    mHeight = backbufferHeight;
    mSwapIntervalDirty = false;

    return true;
}

bool Surface::swapRect(EGLint x, EGLint y, EGLint width, EGLint height)
{
    if (!mSwapChain)
    {
        return true;
    }

    if (x + width > mWidth)
    {
        width = mWidth - x;
    }

    if (y + height > mHeight)
    {
        height = mHeight - y;
    }

    if (width == 0 || height == 0)
    {
        return true;
    }

    EGLint status = mSwapChain->swapRect(x, y, width, height);

    if (status == EGL_CONTEXT_LOST)
    {
        mRenderer->notifyDeviceLost();
        return false;
    }
    else if (status != EGL_SUCCESS)
    {
        return error(status, false);
    }

    checkForOutOfDateSwapChain();

    return true;
}

EGLNativeWindowType Surface::getWindowHandle()
{
    return mNativeWindow.getNativeWindow();
}

#if !defined(ANGLE_ENABLE_WINDOWS_STORE)
#define kSurfaceProperty _TEXT("Egl::SurfaceOwner")
#define kParentWndProc _TEXT("Egl::SurfaceParentWndProc")

static LRESULT CALLBACK SurfaceWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
  if (message == WM_SIZE)
  {
      Surface* surf = reinterpret_cast<Surface*>(GetProp(hwnd, kSurfaceProperty));
      if(surf)
      {
          surf->checkForOutOfDateSwapChain();
      }
  }
  WNDPROC prevWndFunc = reinterpret_cast<WNDPROC >(GetProp(hwnd, kParentWndProc));
  return CallWindowProc(prevWndFunc, hwnd, message, wparam, lparam);
}
#endif

void Surface::subclassWindow()
{
#if !defined(ANGLE_ENABLE_WINDOWS_STORE)
    HWND window = mNativeWindow.getNativeWindow();
    if (!window)
    {
        return;
    }

    DWORD processId;
    DWORD threadId = GetWindowThreadProcessId(window, &processId);
    if (processId != GetCurrentProcessId() || threadId != GetCurrentThreadId())
    {
        return;
    }

    SetLastError(0);
    LONG_PTR oldWndProc = SetWindowLongPtr(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(SurfaceWindowProc));
    if(oldWndProc == 0 && GetLastError() != ERROR_SUCCESS)
    {
        mWindowSubclassed = false;
        return;
    }

    SetProp(window, kSurfaceProperty, reinterpret_cast<HANDLE>(this));
    SetProp(window, kParentWndProc, reinterpret_cast<HANDLE>(oldWndProc));
    mWindowSubclassed = true;
#endif
}

void Surface::unsubclassWindow()
{
    if(!mWindowSubclassed)
    {
        return;
    }

#if !defined(ANGLE_ENABLE_WINDOWS_STORE)
    HWND window = mNativeWindow.getNativeWindow();
    if (!window)
    {
        return;
    }

    // un-subclass
    LONG_PTR parentWndFunc = reinterpret_cast<LONG_PTR>(GetProp(window, kParentWndProc));

    // Check the windowproc is still SurfaceWindowProc.
    // If this assert fails, then it is likely the application has subclassed the
    // hwnd as well and did not unsubclass before destroying its EGL context. The
    // application should be modified to either subclass before initializing the
    // EGL context, or to unsubclass before destroying the EGL context.
    if(parentWndFunc)
    {
        LONG_PTR prevWndFunc = SetWindowLongPtr(window, GWLP_WNDPROC, parentWndFunc);
        UNUSED_ASSERTION_VARIABLE(prevWndFunc);
        ASSERT(prevWndFunc == reinterpret_cast<LONG_PTR>(SurfaceWindowProc));
    }

    RemoveProp(window, kSurfaceProperty);
    RemoveProp(window, kParentWndProc);
#endif
    mWindowSubclassed = false;
}

bool Surface::checkForOutOfDateSwapChain()
{
    RECT client;
    int clientWidth = getWidth();
    int clientHeight = getHeight();
    bool sizeDirty = false;
    if (!mFixedSize && !mNativeWindow.isIconic())
    {
        // The window is automatically resized to 150x22 when it's minimized, but the swapchain shouldn't be resized
        // because that's not a useful size to render to.
        if (!mNativeWindow.getClientRect(&client))
        {
            ASSERT(false);
            return false;
        }

        // Grow the buffer now, if the window has grown. We need to grow now to avoid losing information.
        clientWidth = client.right - client.left;
        clientHeight = client.bottom - client.top;
        sizeDirty = clientWidth != getWidth() || clientHeight != getHeight();
    }

    bool wasDirty = (mSwapIntervalDirty || sizeDirty);

    if (mSwapIntervalDirty)
    {
        resetSwapChain(clientWidth, clientHeight);
    }
    else if (sizeDirty)
    {
        resizeSwapChain(clientWidth, clientHeight);
    }

    if (wasDirty)
    {
        if (static_cast<egl::Surface*>(getCurrentDrawSurface()) == this)
        {
            glMakeCurrent(glGetCurrentContext(), static_cast<egl::Display*>(getCurrentDisplay()), this);
        }

        return true;
    }

    return false;
}

bool Surface::swap()
{
    return swapRect(0, 0, mWidth, mHeight);
}

bool Surface::postSubBuffer(EGLint x, EGLint y, EGLint width, EGLint height)
{
    if (!mPostSubBufferSupported)
    {
        // Spec is not clear about how this should be handled.
        return true;
    }

    return swapRect(x, y, width, height);
}

EGLint Surface::isPostSubBufferSupported() const
{
    return mPostSubBufferSupported;
}

rx::SwapChain *Surface::getSwapChain() const
{
    return mSwapChain;
}

void Surface::setSwapInterval(EGLint interval)
{
    if (mSwapInterval == interval)
    {
        return;
    }

    mSwapInterval = interval;
    mSwapInterval = std::max(mSwapInterval, mRenderer->getMinSwapInterval());
    mSwapInterval = std::min(mSwapInterval, mRenderer->getMaxSwapInterval());

    mSwapIntervalDirty = true;
}

EGLint Surface::getConfigID() const
{
    return mConfig->mConfigID;
}

EGLint Surface::getWidth() const
{
    return mWidth;
}

EGLint Surface::getHeight() const
{
    return mHeight;
}

EGLint Surface::getPixelAspectRatio() const
{
    return mPixelAspectRatio;
}

EGLenum Surface::getRenderBuffer() const
{
    return mRenderBuffer;
}

EGLenum Surface::getSwapBehavior() const
{
    return mSwapBehavior;
}

EGLenum Surface::getTextureFormat() const
{
    return mTextureFormat;
}

EGLenum Surface::getTextureTarget() const
{
    return mTextureTarget;
}

void Surface::setBoundTexture(gl::Texture2D *texture)
{
    mTexture = texture;
}

gl::Texture2D *Surface::getBoundTexture() const
{
    return mTexture;
}

EGLint Surface::isFixedSize() const
{
    return mFixedSize;
}

EGLenum Surface::getFormat() const
{
    return mConfig->mRenderTargetFormat;
}
}
