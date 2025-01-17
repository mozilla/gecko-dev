/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_WIDGET_GTK_WAYLAND_BUFFER_H
#define _MOZILLA_WIDGET_GTK_WAYLAND_BUFFER_H

#include "DMABufSurface.h"
#include "GLContext.h"
#include "MozFramebuffer.h"
#include "mozilla/ipc/SharedMemory.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "nsTArray.h"
#include "nsWaylandDisplay.h"
#include "WaylandSurface.h"

namespace mozilla::widget {

// Allocates and owns shared memory for Wayland drawing surface
class WaylandShmPool {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WaylandShmPool);

  static RefPtr<WaylandShmPool> Create(nsWaylandDisplay* aWaylandDisplay,
                                       int aSize);

  wl_shm_pool* GetShmPool() { return mShmPool; };
  void* GetImageData();

 private:
  WaylandShmPool() = default;
  ~WaylandShmPool();

  wl_shm_pool* mShmPool = nullptr;
  void* mImageData = nullptr;
  RefPtr<ipc::SharedMemory> mShm;
  int mSize = 0;
};

class WaylandBuffer {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WaylandBuffer);

  virtual already_AddRefed<gfx::DrawTarget> Lock() { return nullptr; };
  virtual void* GetImageData() { return nullptr; }
  virtual GLuint GetTexture() { return 0; }
  virtual void DestroyGLResources() {};
  virtual gfx::SurfaceFormat GetSurfaceFormat() = 0;

  LayoutDeviceIntSize GetSize() { return mSize; };
  bool IsMatchingSize(const LayoutDeviceIntSize& aSize) {
    return aSize == mSize;
  }

  bool IsAttached() { return !!mSurface; }

  // Lend wl_buffer to WaylandSurface to attach.
  // We store reference to WaylandSurface unless we don't have
  // wl_buffer available.
  //
  // At also marks buffer as attached.
  wl_buffer* BorrowBuffer(RefPtr<WaylandSurface> aWaylandSurface);

  // Return lended buffer, called by aWaylandSurface.
  void ReturnBuffer(RefPtr<WaylandSurface> aWaylandSurface);

  // Called by Wayland compostor when buffer is released/deleted by
  // Wayland compostor.
  //
  // There are two cases how buffer can be detached:
  // 1) detach call from Wayland compostor, wl_buffer may be kept around.
  // 2) detach from WaylandSurface - internal wl_buffer is deleted,
  //    for instance on Unmap when wl_surface becomes invisible.
  void BufferDetachedCallbackHandler(wl_buffer* aBuffer, bool aWlBufferDeleted);

 protected:
  explicit WaylandBuffer(const LayoutDeviceIntSize& aSize);
  virtual ~WaylandBuffer() = default;

  // Create and return wl_buffer for underlying memory buffer.
  // CreateWlBuffer() can be called many times, if wl_buffer already
  // exist, recent buffer is returned.
  virtual wl_buffer* CreateWlBuffer() = 0;

  // Delete wl_buffer. It only releases Wayland interface over underlying
  // memory, doesn't affect actual buffer content but only connection
  // to Wayland compositor.
  virtual void DeleteWlBuffer() = 0;

  virtual wl_buffer* GetWlBuffer() = 0;
  bool HasWlBuffer() { return !!GetWlBuffer(); }

  bool IsWaitingToBufferDelete() const { return !!mBufferDeleteSyncCallback; }

  // We need to protect buffer release sequence as it can happen
  // from Main thread (Wayland compositor) and Rendering thread.
  mozilla::Mutex mBufferReleaseMutex{"WaylandBufferRelease"};

  // wl_buffer delete is not atomic, we need to wait until it's finished.
  wl_callback* mBufferDeleteSyncCallback = nullptr;

  LayoutDeviceIntSize mSize;
  // WaylandSurface where we're attached to.
  RefPtr<WaylandSurface> mSurface;
  static gfx::SurfaceFormat sFormat;
};

// Holds actual graphics data for wl_surface
class WaylandBufferSHM final : public WaylandBuffer {
 public:
  static RefPtr<WaylandBufferSHM> Create(const LayoutDeviceIntSize& aSize);

  void ReleaseWlBuffer();
  already_AddRefed<gfx::DrawTarget> Lock() override;
  void* GetImageData() override { return mShmPool->GetImageData(); }

  gfx::SurfaceFormat GetSurfaceFormat() override {
    return gfx::SurfaceFormat::B8G8R8A8;
  }

  void Clear();
  size_t GetBufferAge() { return mBufferAge; };
  RefPtr<WaylandShmPool> GetShmPool() { return mShmPool; }

  void IncrementBufferAge() { mBufferAge++; };
  void ResetBufferAge() { mBufferAge = 0; };

#ifdef MOZ_LOGGING
  void DumpToFile(const char* aHint);
#endif

 protected:
  wl_buffer* CreateWlBuffer() override;
  void DeleteWlBuffer() override;
  wl_buffer* GetWlBuffer() override;

 private:
  explicit WaylandBufferSHM(const LayoutDeviceIntSize& aSize);
  ~WaylandBufferSHM() override;

  // WaylandShmPoolMB provides actual shared memory we draw into
  RefPtr<WaylandShmPool> mShmPool;

  // wl_buffer is a wayland object that encapsulates the shared memory
  // and passes it to wayland compositor by wl_surface object.
  wl_buffer* mWLBuffer = nullptr;

  size_t mBufferAge = 0;

#ifdef MOZ_LOGGING
  static int mDumpSerial;
  static char* mDumpDir;
#endif
};

class WaylandBufferDMABUF final : public WaylandBuffer {
 public:
  static already_AddRefed<WaylandBufferDMABUF> CreateRGBA(
      const LayoutDeviceIntSize& aSize, gl::GLContext* aGL);
  static already_AddRefed<WaylandBufferDMABUF> CreateExternal(
      RefPtr<DMABufSurface> aSurface);

  GLuint GetTexture() override { return mDMABufSurface->GetTexture(); };
  void DestroyGLResources() override { mDMABufSurface->ReleaseTextures(); };
  gfx::SurfaceFormat GetSurfaceFormat() override {
    return mDMABufSurface->GetFormat();
  }

 protected:
  wl_buffer* CreateWlBuffer() override;
  void DeleteWlBuffer() override;
  wl_buffer* GetWlBuffer() override;

 private:
  explicit WaylandBufferDMABUF(const LayoutDeviceIntSize& aSize);
  ~WaylandBufferDMABUF();

  RefPtr<DMABufSurface> mDMABufSurface;
};

}  // namespace mozilla::widget

#endif  // _MOZILLA_WIDGET_GTK_WAYLAND_BUFFER_H
