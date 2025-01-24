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

  LayoutDeviceIntSize GetSize() const { return mSize; };
  bool IsMatchingSize(const LayoutDeviceIntSize& aSize) const {
    return aSize == mSize;
  }

  bool IsAttached() { return mIsAttachedToCompositor; }

  bool IsAttachedToSurface(WaylandSurface* aWaylandSurface);

  // Lend wl_buffer to WaylandSurface to attach.
  wl_buffer* BorrowBuffer(WaylandSurfaceLock& aSurfaceLock);

  // Return lended buffer, called by WaylandSurface when it's unmapped or
  // a different buffer is attached.
  void ReturnBuffer(WaylandSurfaceLock& aSurfaceLock);

  // Called by Wayland compostor when buffer is detached by
  // Wayland compostor (free to reuse).
  void BufferDetachedCallbackHandler(wl_buffer* aBuffer);

  // Called by Wayland compostor when buffer is deleted by
  // Wayland compostor.
  void BufferDeletedCallbackHandler();

 protected:
  explicit WaylandBuffer(const LayoutDeviceIntSize& aSize);
  virtual ~WaylandBuffer() = default;

  // Create and return wl_buffer for underlying memory buffer if it's missing.
  virtual bool CreateWlBuffer() = 0;

  // Delete wl_buffer. It only releases Wayland interface over underlying
  // memory, doesn't affect actual buffer content but only connection
  // to Wayland compositor.
  void DeleteWlBuffer();

  // We need to protect buffer release sequence as it can happen
  // from Main thread (Wayland compositor) and Rendering thread.
  mozilla::Mutex mMutex{"WaylandBuffer"};

  // wl_buffer delete is not atomic, we need to wait until it's finished.
  wl_callback* mBufferDeleteSyncCallback MOZ_GUARDED_BY(mMutex) = nullptr;

  // wl_buffer is a wayland object that encapsulates the shared/dmabuf memory
  // and passes it to wayland compositor by wl_surface object.
  wl_buffer* mWLBuffer MOZ_GUARDED_BY(mMutex) = nullptr;

  // Wayland buffer is tied to WaylandSurface.
  // We keep reference to WaylandSurface until WaylandSurface returns the
  // buffer.
  RefPtr<WaylandSurface> mAttachedToSurface MOZ_GUARDED_BY(mMutex);

  LayoutDeviceIntSize mSize;

  // Indicates that wl_buffer is actively used by Wayland compositor.
  // We can't delete such wl_buffer.
  mozilla::Atomic<bool, mozilla::Relaxed> mIsAttachedToCompositor{false};

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
  size_t GetBufferAge() const { return mBufferAge; };
  RefPtr<WaylandShmPool> GetShmPool() const { return mShmPool; }

  void IncrementBufferAge() { mBufferAge++; };
  void ResetBufferAge() { mBufferAge = 0; };

#ifdef MOZ_LOGGING
  void DumpToFile(const char* aHint);
#endif

 protected:
  bool CreateWlBuffer() override;

 private:
  explicit WaylandBufferSHM(const LayoutDeviceIntSize& aSize);
  ~WaylandBufferSHM() override;

  // WaylandShmPoolMB provides actual shared memory we draw into
  RefPtr<WaylandShmPool> mShmPool;

  size_t mBufferAge = 0;

#ifdef MOZ_LOGGING
  static int mDumpSerial;
  static char* mDumpDir;
#endif
};

class WaylandBufferDMABUF final : public WaylandBuffer {
 public:
  static already_AddRefed<WaylandBufferDMABUF> CreateRGBA(
      const LayoutDeviceIntSize& aSize, gl::GLContext* aGL,
      RefPtr<DRMFormat> aFormat);
  static already_AddRefed<WaylandBufferDMABUF> CreateExternal(
      RefPtr<DMABufSurface> aSurface);

  GLuint GetTexture() override { return mDMABufSurface->GetTexture(); };
  void DestroyGLResources() override { mDMABufSurface->ReleaseTextures(); };
  gfx::SurfaceFormat GetSurfaceFormat() override {
    return mDMABufSurface->GetFormat();
  }

 protected:
  bool CreateWlBuffer() override;

 private:
  explicit WaylandBufferDMABUF(const LayoutDeviceIntSize& aSize);
  ~WaylandBufferDMABUF();

  RefPtr<DMABufSurface> mDMABufSurface;
};

}  // namespace mozilla::widget

#endif  // _MOZILLA_WIDGET_GTK_WAYLAND_BUFFER_H
