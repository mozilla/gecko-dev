/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_WAYLAND_H
#define _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_WAYLAND_H

namespace mozilla {
namespace widget {

// We support only 32bpp formats
#define BUFFER_BPP 4

// Holds actual graphics data for wl_surface
class SurfaceBuffer {
public:
  SurfaceBuffer(int aWidth, int aHeight);
  ~SurfaceBuffer();

  void Attach(wl_surface* aSurface,
              const LayoutDeviceIntRegion& aInvalidRegion);
  void Detach();
  bool IsAttached() { return mAttached; }
  bool IsValid()    { return mBuffer && mBufferData; }

  bool Resize(int aWidth, int aHeight);
  bool Sync(class SurfaceBuffer* aSourceBuffer);
  
  already_AddRefed<gfx::DrawTarget> Lock();

private:
  bool CreateShmPool(int aSize);
  bool ResizeShmPool(int aSize);
  void ReleaseShmPool(void);

  void CreateBuffer(int aWidth, int aHeight);
  void ReleaseBuffer();

  wl_shm_pool*       mShmPool;
  int                mShmPoolFd;
  int                mAllocatedSize;
  wl_buffer*         mBuffer;
  void*              mBufferData;
  int                mWidth;
  int                mHeight;
  gfx::SurfaceFormat mFormat;
  bool               mAttached;
};

class WindowSurfaceWayland : public WindowSurface {
public:
  WindowSurfaceWayland(wl_display *aDisplay, wl_surface *aSurface);
  ~WindowSurfaceWayland();

  already_AddRefed<gfx::DrawTarget> Lock(const LayoutDeviceIntRegion& aRegion) override;
  void                      Commit(const LayoutDeviceIntRegion& aInvalidRegion) final;

  static void               SetShm(wl_shm* aShm) { mShm = aShm; };
  static wl_shm*            GetShm() { return(mShm); };
  static wl_event_queue*    GetQueue() { return mQueue; };
  static wl_display*        GetDisplay() { return mDisplay; };
  static void               SetWaylandPixelFormat(uint32_t format);

private:
  SurfaceBuffer*            SetBufferToDraw(int aWidth, int aHeight);
  void                      Init();

  static bool               mIsAvailable;
  static bool               mInitialized;
  static gfx::SurfaceFormat mFormat;
  static wl_shm*            mShm;
  static wl_event_queue*    mQueue;
  static GThread*           mThread;
  static wl_display*        mDisplay;
  wl_surface*               mSurface;
  SurfaceBuffer*            mBuffers[100];
  int                       mFrontBuffer;
  int                       mLastBuffer;
};

}  // namespace widget
}  // namespace mozilla

#endif // _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_WAYLAND_H
