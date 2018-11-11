/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContextTypes.h"
#include <windows.h>

struct PRLibrary;

namespace mozilla {
namespace gl {

class WGLLibrary
{
public:
    WGLLibrary()
      : mInitialized(false)
      , mOGLLibrary(nullptr)
      , mHasRobustness(false)
      , mHasDXInterop(false)
      , mHasDXInterop2(false)
      , mWindow (0)
      , mWindowDC(0)
      , mWindowGLContext(0)
      , mWindowPixelFormat(0)
    {}

    typedef HGLRC (GLAPIENTRY * PFNWGLCREATECONTEXTPROC) (HDC);
    PFNWGLCREATECONTEXTPROC fCreateContext;
    typedef BOOL (GLAPIENTRY * PFNWGLDELETECONTEXTPROC) (HGLRC);
    PFNWGLDELETECONTEXTPROC fDeleteContext;
    typedef BOOL (GLAPIENTRY * PFNWGLMAKECURRENTPROC) (HDC, HGLRC);
    PFNWGLMAKECURRENTPROC fMakeCurrent;
    typedef PROC (GLAPIENTRY * PFNWGLGETPROCADDRESSPROC) (LPCSTR);
    PFNWGLGETPROCADDRESSPROC fGetProcAddress;
    typedef HGLRC (GLAPIENTRY * PFNWGLGETCURRENTCONTEXT) (void);
    PFNWGLGETCURRENTCONTEXT fGetCurrentContext;
    typedef HDC (GLAPIENTRY * PFNWGLGETCURRENTDC) (void);
    PFNWGLGETCURRENTDC fGetCurrentDC;
    typedef BOOL (GLAPIENTRY * PFNWGLSHARELISTS) (HGLRC oldContext, HGLRC newContext);
    PFNWGLSHARELISTS fShareLists;

    typedef HANDLE (WINAPI * PFNWGLCREATEPBUFFERPROC) (HDC hDC, int iPixelFormat, int iWidth, int iHeight, const int* piAttribList);
    PFNWGLCREATEPBUFFERPROC fCreatePbuffer;
    typedef BOOL (WINAPI * PFNWGLDESTROYPBUFFERPROC) (HANDLE hPbuffer);
    PFNWGLDESTROYPBUFFERPROC fDestroyPbuffer;
    typedef HDC (WINAPI * PFNWGLGETPBUFFERDCPROC) (HANDLE hPbuffer);
    PFNWGLGETPBUFFERDCPROC fGetPbufferDC;

    typedef BOOL (WINAPI * PFNWGLBINDTEXIMAGEPROC) (HANDLE hPbuffer, int iBuffer);
    PFNWGLBINDTEXIMAGEPROC fBindTexImage;
    typedef BOOL (WINAPI * PFNWGLRELEASETEXIMAGEPROC) (HANDLE hPbuffer, int iBuffer);
    PFNWGLRELEASETEXIMAGEPROC fReleaseTexImage;

    typedef BOOL (WINAPI * PFNWGLCHOOSEPIXELFORMATPROC) (HDC hdc, const int* piAttribIList, const FLOAT* pfAttribFList, UINT nMaxFormats, int* piFormats, UINT* nNumFormats);
    PFNWGLCHOOSEPIXELFORMATPROC fChoosePixelFormat;
    typedef BOOL (WINAPI * PFNWGLGETPIXELFORMATATTRIBIVPROC) (HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, int* piAttributes, int* piValues);
    PFNWGLGETPIXELFORMATATTRIBIVPROC fGetPixelFormatAttribiv;

    typedef const char* (WINAPI * PFNWGLGETEXTENSIONSSTRINGPROC) (HDC hdc);
    PFNWGLGETEXTENSIONSSTRINGPROC fGetExtensionsString;

    typedef HGLRC (WINAPI * PFNWGLCREATECONTEXTATTRIBSPROC) (HDC hdc, HGLRC hShareContext, const int* attribList);
    PFNWGLCREATECONTEXTATTRIBSPROC fCreateContextAttribs;

    // WGL_NV_DX_interop:
    // BOOL wglDXSetResourceShareHandleNV(void* dxObject, HANDLE shareHandle);
    typedef BOOL (WINAPI * PFNWGLDXSETRESOURCESHAREHANDLEPROC) (void* dxObject, HANDLE shareHandle);
    PFNWGLDXSETRESOURCESHAREHANDLEPROC fDXSetResourceShareHandle;

    // HANDLE wglDXOpenDeviceNV(void* dxDevice);
    typedef HANDLE (WINAPI * PFNWGLDXOPENDEVICEPROC) (void* dxDevice);
    PFNWGLDXOPENDEVICEPROC fDXOpenDevice;

    // BOOL wglDXCloseDeviceNV(HANDLE hDevice);
    typedef BOOL (WINAPI * PFNWGLDXCLOSEDEVICEPROC) (HANDLE hDevice);
    PFNWGLDXCLOSEDEVICEPROC fDXCloseDevice;

    // HANDLE wglDXRegisterObjectNV(HANDLE hDevice, void* dxObject, GLuint name, GLenum type, GLenum access);
    typedef HANDLE (WINAPI * PFNWGLDXREGISTEROBJECTPROC) (HANDLE hDevice, void* dxObject, GLuint name, GLenum type, GLenum access);
    PFNWGLDXREGISTEROBJECTPROC fDXRegisterObject;

    // BOOL wglDXUnregisterObjectNV(HANDLE hDevice, HANDLE hObject);
    typedef BOOL (WINAPI * PFNWGLDXUNREGISTEROBJECT) (HANDLE hDevice, HANDLE hObject);
    PFNWGLDXUNREGISTEROBJECT fDXUnregisterObject;

    // BOOL wglDXObjectAccessNV(HANDLE hObject, GLenum access);
    typedef BOOL (WINAPI * PFNWGLDXOBJECTACCESSPROC) (HANDLE hObject, GLenum access);
    PFNWGLDXOBJECTACCESSPROC fDXObjectAccess;

    // BOOL wglDXLockObjectsNV(HANDLE hDevice, GLint count, HANDLE* hObjects);
    typedef BOOL (WINAPI * PFNWGLDXLOCKOBJECTSPROC) (HANDLE hDevice, GLint count, HANDLE* hObjects);
    PFNWGLDXLOCKOBJECTSPROC fDXLockObjects;

    // BOOL wglDXUnlockObjectsNV(HANDLE hDevice, GLint count, HANDLE* hObjects);
    typedef BOOL (WINAPI * PFNWGLDXUNLOCKOBJECTSPROC) (HANDLE hDevice, GLint count, HANDLE* hObjects);
    PFNWGLDXUNLOCKOBJECTSPROC fDXUnlockObjects;

    bool EnsureInitialized();
    HWND CreateDummyWindow(HDC* aWindowDC = nullptr);

    bool HasRobustness() const { return mHasRobustness; }
    bool HasDXInterop() const { return mHasDXInterop; }
    bool HasDXInterop2() const { return mHasDXInterop2; }
    bool IsInitialized() const { return mInitialized; }
    HWND GetWindow() const { return mWindow; }
    HDC GetWindowDC() const {return mWindowDC; }
    HGLRC GetWindowGLContext() const {return mWindowGLContext; }
    int GetWindowPixelFormat() const { return mWindowPixelFormat; }
    PRLibrary* GetOGLLibrary() { return mOGLLibrary; }

private:
    bool mInitialized;
    PRLibrary* mOGLLibrary;
    bool mHasRobustness;
    bool mHasDXInterop;
    bool mHasDXInterop2;

    HWND mWindow;
    HDC mWindowDC;
    HGLRC mWindowGLContext;
    int mWindowPixelFormat;

};

// a global WGLLibrary instance
extern WGLLibrary sWGLLib;

} /* namespace gl */
} /* namespace mozilla */
