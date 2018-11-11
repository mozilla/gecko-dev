/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContextProvider.h"
#include "GLContextWGL.h"
#include "GLLibraryLoader.h"
#include "nsDebug.h"
#include "nsIWidget.h"
#include "gfxPlatform.h"
#include "gfxWindowsSurface.h"

#include "gfxCrashReporterUtils.h"

#include "prenv.h"

#include "mozilla/Preferences.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/widget/CompositorWidget.h"

namespace mozilla {
namespace gl {

using namespace mozilla::gfx;
using namespace mozilla::widget;

WGLLibrary sWGLLib;

HWND
WGLLibrary::CreateDummyWindow(HDC* aWindowDC)
{
    WNDCLASSW wc;
    if (!GetClassInfoW(GetModuleHandle(nullptr), L"GLContextWGLClass", &wc)) {
        ZeroMemory(&wc, sizeof(WNDCLASSW));
        wc.style = CS_OWNDC;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpfnWndProc = DefWindowProc;
        wc.lpszClassName = L"GLContextWGLClass";
        if (!RegisterClassW(&wc)) {
            NS_WARNING("Failed to register GLContextWGLClass?!");
            // er. failed to register our class?
            return nullptr;
        }
    }

    HWND win = CreateWindowW(L"GLContextWGLClass", L"GLContextWGL", 0,
                             0, 0, 16, 16,
                             nullptr, nullptr, GetModuleHandle(nullptr),
                             nullptr);
    NS_ENSURE_TRUE(win, nullptr);

    HDC dc = GetDC(win);
    NS_ENSURE_TRUE(dc, nullptr);

    if (mWindowPixelFormat == 0) {
        PIXELFORMATDESCRIPTOR pfd;
        ZeroMemory(&pfd, sizeof(PIXELFORMATDESCRIPTOR));
        pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 24;
        pfd.cRedBits = 8;
        pfd.cGreenBits = 8;
        pfd.cBlueBits = 8;
        pfd.cAlphaBits = 8;
        pfd.cDepthBits = 0;
        pfd.iLayerType = PFD_MAIN_PLANE;

        mWindowPixelFormat = ChoosePixelFormat(dc, &pfd);
    }

    if (!mWindowPixelFormat ||
        !SetPixelFormat(dc, mWindowPixelFormat, nullptr))
    {
        NS_WARNING("SetPixelFormat failed!");
        DestroyWindow(win);
        return nullptr;
    }

    if (aWindowDC) {
        *aWindowDC = dc;
    }

    return win;
}

static inline bool
HasExtension(const char* aExtensions, const char* aRequiredExtension)
{
    return GLContext::ListHasExtension(
        reinterpret_cast<const GLubyte*>(aExtensions), aRequiredExtension);
}

bool
WGLLibrary::EnsureInitialized()
{
    if (mInitialized)
        return true;

    mozilla::ScopedGfxFeatureReporter reporter("WGL");

    std::string libGLFilename = "Opengl32.dll";
    // SU_SPIES_DIRECTORY is for AMD CodeXL/gDEBugger
    if (PR_GetEnv("SU_SPIES_DIRECTORY")) {
        libGLFilename = std::string(PR_GetEnv("SU_SPIES_DIRECTORY")) + "\\opengl32.dll";
    }

    if (!mOGLLibrary) {
        mOGLLibrary = PR_LoadLibrary(libGLFilename.c_str());
        if (!mOGLLibrary) {
            NS_WARNING("Couldn't load OpenGL library.");
            return false;
        }
    }

    const GLLibraryLoader::SymLoadStruct earlySymbols[] = {
        { (PRFuncPtr*) &fCreateContext, { "wglCreateContext", nullptr } },
        { (PRFuncPtr*) &fMakeCurrent, { "wglMakeCurrent", nullptr } },
        { (PRFuncPtr*) &fGetProcAddress, { "wglGetProcAddress", nullptr } },
        { (PRFuncPtr*) &fDeleteContext, { "wglDeleteContext", nullptr } },
        { (PRFuncPtr*) &fGetCurrentContext, { "wglGetCurrentContext", nullptr } },
        { (PRFuncPtr*) &fGetCurrentDC, { "wglGetCurrentDC", nullptr } },
        { nullptr, { nullptr } }
    };

    if (!GLLibraryLoader::LoadSymbols(mOGLLibrary, &earlySymbols[0])) {
        NS_WARNING("Couldn't find required entry points in OpenGL DLL (early init)");
        return false;
    }

    // This is ridiculous -- we have to actually create a context to
    // get the OpenGL ICD to load.
    mWindow = CreateDummyWindow(&mWindowDC);
    NS_ENSURE_TRUE(mWindow, false);

    // create rendering context
    mWindowGLContext = fCreateContext(mWindowDC);
    NS_ENSURE_TRUE(mWindowGLContext, false);

    if (!fMakeCurrent(mWindowDC, mWindowGLContext)) {
        NS_WARNING("wglMakeCurrent failed");
        return false;
    }

    const auto curCtx = fGetCurrentContext();
    const auto curDC = fGetCurrentDC();

    const auto lookupFunc = (GLLibraryLoader::PlatformLookupFunction)fGetProcAddress;

    // Now we can grab all the other symbols that we couldn't without having
    // a context current.
    const GLLibraryLoader::SymLoadStruct pbufferSymbols[] = {
        { (PRFuncPtr*) &fCreatePbuffer, { "wglCreatePbufferARB", "wglCreatePbufferEXT", nullptr } },
        { (PRFuncPtr*) &fDestroyPbuffer, { "wglDestroyPbufferARB", "wglDestroyPbufferEXT", nullptr } },
        { (PRFuncPtr*) &fGetPbufferDC, { "wglGetPbufferDCARB", "wglGetPbufferDCEXT", nullptr } },
        { (PRFuncPtr*) &fBindTexImage, { "wglBindTexImageARB", "wglBindTexImageEXT", nullptr } },
        { (PRFuncPtr*) &fReleaseTexImage, { "wglReleaseTexImageARB", "wglReleaseTexImageEXT", nullptr } },
        { nullptr, { nullptr } }
    };

    const GLLibraryLoader::SymLoadStruct pixFmtSymbols[] = {
        { (PRFuncPtr*) &fChoosePixelFormat, { "wglChoosePixelFormatARB", "wglChoosePixelFormatEXT", nullptr } },
        { (PRFuncPtr*) &fGetPixelFormatAttribiv, { "wglGetPixelFormatAttribivARB", "wglGetPixelFormatAttribivEXT", nullptr } },
        { nullptr, { nullptr } }
    };

    if (!GLLibraryLoader::LoadSymbols(mOGLLibrary, &pbufferSymbols[0], lookupFunc)) {
        // this isn't an error, just means that pbuffers aren't supported
        fCreatePbuffer = nullptr;
    }

    if (!GLLibraryLoader::LoadSymbols(mOGLLibrary, &pixFmtSymbols[0], lookupFunc)) {
        // this isn't an error, just means that we don't have the pixel format extension
        fChoosePixelFormat = nullptr;
    }

    const GLLibraryLoader::SymLoadStruct extensionsSymbols[] = {
        { (PRFuncPtr*) &fGetExtensionsString, { "wglGetExtensionsStringARB", nullptr} },
        { nullptr, { nullptr } }
    };

    const GLLibraryLoader::SymLoadStruct robustnessSymbols[] = {
        { (PRFuncPtr*) &fCreateContextAttribs, { "wglCreateContextAttribsARB", nullptr} },
        { nullptr, { nullptr } }
    };

    const GLLibraryLoader::SymLoadStruct dxInteropSymbols[] = {
        { (PRFuncPtr*)&fDXSetResourceShareHandle,{ "wglDXSetResourceShareHandleNV", nullptr } },
        { (PRFuncPtr*)&fDXOpenDevice,            { "wglDXOpenDeviceNV",             nullptr } },
        { (PRFuncPtr*)&fDXCloseDevice,           { "wglDXCloseDeviceNV",            nullptr } },
        { (PRFuncPtr*)&fDXRegisterObject,        { "wglDXRegisterObjectNV",         nullptr } },
        { (PRFuncPtr*)&fDXUnregisterObject,      { "wglDXUnregisterObjectNV",       nullptr } },
        { (PRFuncPtr*)&fDXObjectAccess,          { "wglDXObjectAccessNV",           nullptr } },
        { (PRFuncPtr*)&fDXLockObjects,           { "wglDXLockObjectsNV",            nullptr } },
        { (PRFuncPtr*)&fDXUnlockObjects,         { "wglDXUnlockObjectsNV",          nullptr } },
        { nullptr, { nullptr } }
    };

    if (GLLibraryLoader::LoadSymbols(mOGLLibrary, &extensionsSymbols[0], lookupFunc)) {
        const char* extString = fGetExtensionsString(mWindowDC);
        MOZ_ASSERT(extString);
        MOZ_ASSERT(HasExtension(extString, "WGL_ARB_extensions_string"));

        if (HasExtension(extString, "WGL_ARB_create_context")) {
            if (GLLibraryLoader::LoadSymbols(mOGLLibrary, &robustnessSymbols[0], lookupFunc)) {
                if (HasExtension(extString, "WGL_ARB_create_context_robustness")) {
                    mHasRobustness = true;
                }
            } else {
                NS_ERROR("WGL supports ARB_create_context without supplying its functions.");
                fCreateContextAttribs = nullptr;
            }
        }

        ////

        mHasDXInterop = HasExtension(extString, "WGL_NV_DX_interop");
        mHasDXInterop2 = HasExtension(extString, "WGL_NV_DX_interop2");

        nsCString blocklistId;
        if (gfxUtils::IsFeatureBlacklisted(nullptr, nsIGfxInfo::FEATURE_DX_INTEROP2,
                                           &blocklistId) &&
            !gfxPrefs::IgnoreDXInterop2Blacklist())
        {
            mHasDXInterop2 = false;
        }

        if (mHasDXInterop || mHasDXInterop2) {
            if (!GLLibraryLoader::LoadSymbols(mOGLLibrary, &dxInteropSymbols[0],
                                              lookupFunc))
            {
                NS_ERROR("WGL supports NV_DX_interop(2) without supplying its functions.");
                fDXSetResourceShareHandle = nullptr;
                fDXOpenDevice = nullptr;
                fDXCloseDevice = nullptr;
                fDXRegisterObject = nullptr;
                fDXUnregisterObject = nullptr;
                fDXObjectAccess = nullptr;
                fDXLockObjects = nullptr;
                fDXUnlockObjects = nullptr;

                mHasDXInterop = false;
                mHasDXInterop2 = false;
            }
        }
    }

    // reset back to the previous context, just in case
    fMakeCurrent(curDC, curCtx);

    if (mHasRobustness) {
        fDeleteContext(mWindowGLContext);

        const int attribs[] = {
            LOCAL_WGL_CONTEXT_FLAGS_ARB, LOCAL_WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB,
            LOCAL_WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB, LOCAL_WGL_LOSE_CONTEXT_ON_RESET_ARB,
            0
        };

        mWindowGLContext = fCreateContextAttribs(mWindowDC, nullptr, attribs);
        if (!mWindowGLContext) {
            mHasRobustness = false;
            mWindowGLContext = fCreateContext(mWindowDC);
        }
    }

    mInitialized = true;

    reporter.SetSuccessful();
    return true;
}

GLContextWGL::GLContextWGL(CreateContextFlags flags, const SurfaceCaps& caps,
                           bool isOffscreen, HDC aDC, HGLRC aContext, HWND aWindow)
    : GLContext(flags, caps, nullptr, isOffscreen),
      mDC(aDC),
      mContext(aContext),
      mWnd(aWindow),
      mPBuffer(nullptr),
      mPixelFormat(0),
      mIsDoubleBuffered(false)
{
    // See 899855
    SetProfileVersion(ContextProfile::OpenGLCompatibility, 200);
}

GLContextWGL::GLContextWGL(CreateContextFlags flags, const SurfaceCaps& caps,
                           bool isOffscreen, HANDLE aPbuffer, HDC aDC, HGLRC aContext,
                           int aPixelFormat)
    : GLContext(flags, caps, nullptr, isOffscreen),
      mDC(aDC),
      mContext(aContext),
      mWnd(nullptr),
      mPBuffer(aPbuffer),
      mPixelFormat(aPixelFormat),
      mIsDoubleBuffered(false)
{
    // See 899855
    SetProfileVersion(ContextProfile::OpenGLCompatibility, 200);
}

GLContextWGL::~GLContextWGL()
{
    MarkDestroyed();

    sWGLLib.fDeleteContext(mContext);

    if (mPBuffer)
        sWGLLib.fDestroyPbuffer(mPBuffer);
    if (mWnd)
        DestroyWindow(mWnd);
}

bool
GLContextWGL::Init()
{
    if (!mDC || !mContext)
        return false;

    // see bug 929506 comment 29. wglGetProcAddress requires a current context.
    if (!sWGLLib.fMakeCurrent(mDC, mContext))
        return false;

    SetupLookupFunction();
    if (!InitWithPrefix("gl", true))
        return false;

    return true;
}

bool
GLContextWGL::MakeCurrentImpl(bool aForce)
{
    BOOL succeeded = true;

    // wglGetCurrentContext seems to just pull the HGLRC out
    // of its TLS slot, so no need to do our own tls slot.
    // You would think that wglMakeCurrent would avoid doing
    // work if mContext was already current, but not so much..
    if (aForce || sWGLLib.fGetCurrentContext() != mContext) {
        succeeded = sWGLLib.fMakeCurrent(mDC, mContext);
        NS_ASSERTION(succeeded, "Failed to make GL context current!");
    }

    return succeeded;
}

bool
GLContextWGL::IsCurrent()
{
    return sWGLLib.fGetCurrentContext() == mContext;
}

void
GLContextWGL::SetIsDoubleBuffered(bool aIsDB)
{
    mIsDoubleBuffered = aIsDB;
}

bool
GLContextWGL::IsDoubleBuffered() const
{
    return mIsDoubleBuffered;
}

bool
GLContextWGL::SupportsRobustness() const
{
    return sWGLLib.HasRobustness();
}

bool
GLContextWGL::SwapBuffers() {
    if (!mIsDoubleBuffered)
        return false;
    return ::SwapBuffers(mDC);
}

bool
GLContextWGL::SetupLookupFunction()
{
    // Make sure that we have a ref to the OGL library;
    // when run under CodeXL, wglGetProcAddress won't return
    // the right thing for some core functions.
    MOZ_ASSERT(mLibrary == nullptr);

    mLibrary = sWGLLib.GetOGLLibrary();
    mLookupFunc = (PlatformLookupFunction)sWGLLib.fGetProcAddress;
    return true;
}

static bool
GetMaxSize(HDC hDC, int format, IntSize& size)
{
    int query[] = {LOCAL_WGL_MAX_PBUFFER_WIDTH_ARB, LOCAL_WGL_MAX_PBUFFER_HEIGHT_ARB};
    int result[2];

    // (HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, int* piAttributes, int* piValues)
    if (!sWGLLib.fGetPixelFormatAttribiv(hDC, format, 0, 2, query, result))
        return false;

    size.width = result[0];
    size.height = result[1];
    return true;
}

static bool
IsValidSizeForFormat(HDC hDC, int format,
                     const IntSize& requested)
{
    IntSize max;
    if (!GetMaxSize(hDC, format, max))
        return true;

    if (requested.width > max.width)
        return false;
    if (requested.height > max.height)
        return false;

    return true;
}

static GLContextWGL*
GetGlobalContextWGL()
{
    return static_cast<GLContextWGL*>(GLContextProviderWGL::GetGlobalContext());
}

already_AddRefed<GLContext>
GLContextProviderWGL::CreateWrappingExisting(void*, void*)
{
    return nullptr;
}

already_AddRefed<GLContext>
GLContextProviderWGL::CreateForCompositorWidget(CompositorWidget* aCompositorWidget, bool aForceAccelerated)
{
    return CreateForWindow(aCompositorWidget->RealWidget(), aForceAccelerated);
}

already_AddRefed<GLContext>
GLContextProviderWGL::CreateForWindow(nsIWidget* aWidget, bool aForceAccelerated)
{
    if (!sWGLLib.EnsureInitialized()) {
        return nullptr;
    }

    /**
       * We need to make sure we call SetPixelFormat -after- calling
       * EnsureInitialized, otherwise it can load/unload the dll and
       * wglCreateContext will fail.
       */

    HDC dc = (HDC)aWidget->GetNativeData(NS_NATIVE_GRAPHIC);

    SetPixelFormat(dc, sWGLLib.GetWindowPixelFormat(), nullptr);
    HGLRC context;

    if (sWGLLib.HasRobustness()) {
        int attribs[] = {
            LOCAL_WGL_CONTEXT_FLAGS_ARB, LOCAL_WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB,
            LOCAL_WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB, LOCAL_WGL_LOSE_CONTEXT_ON_RESET_ARB,
            0
        };

        context = sWGLLib.fCreateContextAttribs(dc, nullptr, attribs);
    } else {
        context = sWGLLib.fCreateContext(dc);
    }

    if (!context) {
        return nullptr;
    }

    SurfaceCaps caps = SurfaceCaps::ForRGBA();
    RefPtr<GLContextWGL> glContext = new GLContextWGL(CreateContextFlags::NONE, caps,
                                                      false, dc, context);
    if (!glContext->Init()) {
        return nullptr;
    }

    glContext->SetIsDoubleBuffered(true);

    return glContext.forget();
}

static already_AddRefed<GLContextWGL>
CreatePBufferOffscreenContext(CreateContextFlags flags, const IntSize& aSize)
{
    WGLLibrary& wgl = sWGLLib;

    const int pfAttribs[] = {
        LOCAL_WGL_SUPPORT_OPENGL_ARB, LOCAL_GL_TRUE,
        LOCAL_WGL_ACCELERATION_ARB, LOCAL_WGL_FULL_ACCELERATION_ARB,

        LOCAL_WGL_DRAW_TO_PBUFFER_ARB, LOCAL_GL_TRUE,
        LOCAL_WGL_DOUBLE_BUFFER_ARB, LOCAL_GL_FALSE,
        LOCAL_WGL_STEREO_ARB, LOCAL_GL_FALSE,

        0
    };

    // We only need one!
    static const uint32_t kMaxFormats = 1024;
    int formats[kMaxFormats];
    uint32_t foundFormats;
    HDC windowDC = wgl.GetWindowDC();
    if (!wgl.fChoosePixelFormat(windowDC, pfAttribs, nullptr, kMaxFormats, formats,
                                &foundFormats)
        || foundFormats == 0)
    {
        return nullptr;
    }

    // We don't care; just pick the first one.
    int chosenFormat = formats[0];
    if (!IsValidSizeForFormat(windowDC, chosenFormat, aSize))
        return nullptr;

    const int pbAttribs[] = { 0 };
    HANDLE pbuffer = wgl.fCreatePbuffer(windowDC, chosenFormat, aSize.width, aSize.height,
                                        pbAttribs);
    if (!pbuffer) {
        return nullptr;
    }

    HDC pbdc = wgl.fGetPbufferDC(pbuffer);
    NS_ASSERTION(pbdc, "expected a dc");

    HGLRC context;
    if (wgl.HasRobustness()) {
        const int attribs[] = {
            LOCAL_WGL_CONTEXT_FLAGS_ARB, LOCAL_WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB,
            LOCAL_WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB, LOCAL_WGL_LOSE_CONTEXT_ON_RESET_ARB,
            0
        };
        context = wgl.fCreateContextAttribs(pbdc, nullptr, attribs);
    } else {
        context = wgl.fCreateContext(pbdc);
    }

    if (!context) {
        wgl.fDestroyPbuffer(pbuffer);
        return nullptr;
    }

    SurfaceCaps dummyCaps = SurfaceCaps::Any();
    RefPtr<GLContextWGL> glContext = new GLContextWGL(flags, dummyCaps, true, pbuffer,
                                                      pbdc, context, chosenFormat);
    return glContext.forget();
}

static already_AddRefed<GLContextWGL>
CreateWindowOffscreenContext()
{
    HDC dc;
    HWND win = sWGLLib.CreateDummyWindow(&dc);
    if (!win) {
        return nullptr;
    }

    HGLRC context = sWGLLib.fCreateContext(dc);
    if (sWGLLib.HasRobustness()) {
        int attribs[] = {
            LOCAL_WGL_CONTEXT_FLAGS_ARB, LOCAL_WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB,
            LOCAL_WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB, LOCAL_WGL_LOSE_CONTEXT_ON_RESET_ARB,
            0
        };

        context = sWGLLib.fCreateContextAttribs(dc, nullptr, attribs);
    } else {
        context = sWGLLib.fCreateContext(dc);
    }

    if (!context) {
        return nullptr;
    }

    SurfaceCaps caps = SurfaceCaps::ForRGBA();
    RefPtr<GLContextWGL> glContext = new GLContextWGL(CreateContextFlags::NONE, caps,
                                                      true, dc, context, win);
    return glContext.forget();
}

/*static*/ already_AddRefed<GLContext>
GLContextProviderWGL::CreateHeadless(CreateContextFlags flags,
                                     nsACString* const out_failureId)
{
    if (!sWGLLib.EnsureInitialized()) {
        return nullptr;
    }

    RefPtr<GLContextWGL> glContext;

    // Always try to create a pbuffer context first, because we
    // want the context isolation.
    if (sWGLLib.fCreatePbuffer &&
        sWGLLib.fChoosePixelFormat)
    {
        IntSize dummySize = IntSize(16, 16);
        glContext = CreatePBufferOffscreenContext(flags, dummySize);
    }

    // If it failed, then create a window context and use a FBO.
    if (!glContext) {
        glContext = CreateWindowOffscreenContext();
    }

    if (!glContext ||
        !glContext->Init())
    {
        return nullptr;
    }

    RefPtr<GLContext> retGL = glContext.get();
    return retGL.forget();
}

/*static*/ already_AddRefed<GLContext>
GLContextProviderWGL::CreateOffscreen(const IntSize& size,
                                      const SurfaceCaps& minCaps,
                                      CreateContextFlags flags,
                                      nsACString* const out_failureId)
{
    RefPtr<GLContext> gl = CreateHeadless(flags, out_failureId);
    if (!gl)
        return nullptr;

    if (!gl->InitOffscreen(size, minCaps)) {
        *out_failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_WGL_INIT");
        return nullptr;
    }

    return gl.forget();
}

/*static*/ GLContext*
GLContextProviderWGL::GetGlobalContext()
{
    return nullptr;
}

/*static*/ void
GLContextProviderWGL::Shutdown()
{
}

} /* namespace gl */
} /* namespace mozilla */
