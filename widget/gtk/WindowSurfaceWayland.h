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

// Image surface which holds actual drawing to back buffer,
// it is commited to BackBufferWayland
class ImageBuffer {
friend class BackBufferWayland;
public:
  ImageBuffer();
  ~ImageBuffer();

  already_AddRefed<gfx::DrawTarget> Lock(const LayoutDeviceIntRegion& aRegion);
  unsigned char* GetData() { return mBufferData; };

private:
  unsigned char*     mBufferData;
  int                mBufferAllocated;
  int                mWidth;
  int                mHeight;
};

// Holds actual graphics data for wl_surface
class BackBufferWayland {
public:
  BackBufferWayland(int aWidth, int aHeight);
  ~BackBufferWayland();

  void CopyRectangle(ImageBuffer *aImage,
                     const mozilla::LayoutDeviceIntRect &rect);
  
  void Attach(wl_surface* aSurface);
  void Detach();
  bool IsAttached() { return mAttached; }

  bool Resize(int aWidth, int aHeight);
  bool Sync(class BackBufferWayland* aSourceBuffer);
  
  bool MatchSize(int aWidth, int aHeight) 
  {
    return aWidth == mWidth && aHeight == mHeight;
  }
  bool MatchSize(class BackBufferWayland *aBuffer) 
  {
    return aBuffer->mWidth == mWidth && aBuffer->mHeight == mHeight; 
  }
 
  bool MatchAllocatedSize(int aSize)
  {
    return aSize <= mAllocatedSize; 
  }
  
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
  bool               mAttached;
};

// WindowSurfaceWayland is an abstraction for wl_surface
// and related management
class WindowSurfaceWayland : public WindowSurface {
public:
  WindowSurfaceWayland(nsWindow *aWidget, wl_display *aDisplay, wl_surface *aSurface);
  ~WindowSurfaceWayland();

  already_AddRefed<gfx::DrawTarget> Lock(const LayoutDeviceIntRegion& aRegion) override;
  void                      Commit(const LayoutDeviceIntRegion& aInvalidRegion) final;
  void                      Draw();

  static void               SetShm(wl_shm* aShm) { mShm = aShm; };
  static wl_shm*            GetShm() { return(mShm); };
  static wl_event_queue*    GetQueue() { return mQueue; };
  static wl_display*        GetDisplay() { return mDisplay; };
  static void               SetWaylandPixelFormat(uint32_t format);
  static gfx::SurfaceFormat GetSurfaceFormat() { return mFormat; };

private:
  BackBufferWayland*        GetBufferToDraw(int aWidth, int aHeight);
  void                      Init();

  static bool               mIsAvailable;
  static bool               mInitialized;
  static gfx::SurfaceFormat mFormat;
  static wl_shm*            mShm;
  static wl_event_queue*    mQueue;
  static GThread*           mThread;
  static wl_display*        mDisplay;

  nsWindow*                 mWidget;
  
  // The surface size is dynamically allocated by Commit() call,
  // we store the latest size request here to optimize 
  // buffer usage and our gfx operations
  wl_surface*               mSurface;
  int                       mWidth;
  int                       mHeight;

  ImageBuffer               mImageBuffer;
  
  BackBufferWayland*        mFrontBuffer;
  BackBufferWayland*        mBackBuffer;
  wl_callback*              mFrameCallback;
  bool                      mDelayedCommit;
};

}  // namespace widget
}  // namespace mozilla

#endif // _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_WAYLAND_H
