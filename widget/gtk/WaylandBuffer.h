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
#include "mozilla/ipc/SharedMemoryHandle.h"
#include "mozilla/ipc/SharedMemoryMapping.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "nsTArray.h"
#include "nsWaylandDisplay.h"
#include "WaylandSurface.h"

namespace mozilla::widget {

class WaylandBufferDMABUF;

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
  ipc::MutableSharedMemoryHandle mShmHandle;
  ipc::SharedMemoryMapping mShm;
};

class WaylandBuffer {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WaylandBuffer);

  virtual already_AddRefed<gfx::DrawTarget> Lock() { return nullptr; };
  virtual void* GetImageData() { return nullptr; }
  virtual GLuint GetTexture() { return 0; }
  virtual void DestroyGLResources() {};
  virtual gfx::SurfaceFormat GetSurfaceFormat() = 0;
  virtual WaylandBufferDMABUF* AsWaylandBufferDMABUF() { return nullptr; };

  LayoutDeviceIntSize GetSize() const { return mSize; };
  bool IsMatchingSize(const LayoutDeviceIntSize& aSize) const {
    return aSize == mSize;
  }

  bool IsAttached() const { return mIsAttachedToCompositor; }
  void SetAttachedLocked(const WaylandSurfaceLock& aSurfaceLock) {
    mIsAttachedToCompositor = true;
  }

  bool IsAttachedToSurface(WaylandSurface* aWaylandSurface);

  bool Matches(uintptr_t aWlBufferID) { return aWlBufferID == mWLBufferID; }
  uintptr_t GetWlBufferID() { return mWLBufferID; }

  // Lend wl_buffer to WaylandSurface to attach.
  wl_buffer* BorrowBuffer(const WaylandSurfaceLock& aSurfaceLock);

  // Return lended buffer.
  void ReturnBufferDetached(const WaylandSurfaceLock& aSurfaceLock);

  // Return lended buffer which is still used by Wayland compostor.
  void ReturnBufferAttached(WaylandSurfaceLock& aSurfaceLock);

  void ClearSyncHandler();

#ifdef MOZ_LOGGING
  virtual void DumpToFile(const char* aHint) = 0;
#endif

  // Create and move away wl_buffer and mark is as not managed.
  // From this point wl_buffer is not owned by WaylandBuffer.
  wl_buffer* CreateAndTakeWLBuffer();

  // Set wl_buffer from external source (WaylandBufferDMABUFHolder).
  void SetExternalWLBuffer(wl_buffer* aWLBuffer);

 protected:
  explicit WaylandBuffer(const LayoutDeviceIntSize& aSize);
  virtual ~WaylandBuffer() = default;

  // Create and return wl_buffer for underlying memory buffer if it's missing.
  virtual bool CreateWlBuffer() = 0;

  // Delete wl_buffer. It only releases Wayland interface over underlying
  // memory, doesn't affect actual buffer content but only connection
  // to Wayland compositor.
  void DeleteWlBuffer();

  // wl_buffer delete is not atomic, we need to wait until it's finished.
  wl_callback* mBufferDeleteSyncCallback = nullptr;

  // wl_buffer is a wayland object that encapsulates the shared/dmabuf memory
  // and passes it to wayland compositor by wl_surface object.
  wl_buffer* mWLBuffer = nullptr;
  uintptr_t mWLBufferID = 0;

  // Owns and manages WL buffer. If set to false, wl_buffer is managed by
  // someone else (for instance WaylandBufferDMABUFHolder)
  // and WaylandBuffer can't destroy it.
  bool mManagingWLBuffer = true;

  // Wayland buffer is tied to WaylandSurface.
  // We keep reference to WaylandSurface until WaylandSurface returns the
  // buffer.
  RefPtr<WaylandSurface> mAttachedToSurface;

  // Indicates that wl_buffer is actively used by Wayland compositor.
  // We can't delete such wl_buffer.
  mozilla::Atomic<bool, mozilla::Relaxed> mIsAttachedToCompositor{false};

  LayoutDeviceIntSize mSize;

  static gfx::SurfaceFormat sFormat;

#ifdef MOZ_LOGGING
  static int mDumpSerial;
  static char* mDumpDir;
#endif
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
  void DumpToFile(const char* aHint) override;
#endif

 protected:
  bool CreateWlBuffer() override;

 private:
  explicit WaylandBufferSHM(const LayoutDeviceIntSize& aSize);
  ~WaylandBufferSHM() override;

  // WaylandShmPoolMB provides actual shared memory we draw into
  RefPtr<WaylandShmPool> mShmPool;

  size_t mBufferAge = 0;
};

class WaylandBufferDMABUF final : public WaylandBuffer {
 public:
  static already_AddRefed<WaylandBufferDMABUF> CreateRGBA(
      const LayoutDeviceIntSize& aSize, gl::GLContext* aGL,
      RefPtr<DRMFormat> aFormat);
  static already_AddRefed<WaylandBufferDMABUF> CreateExternal(
      RefPtr<DMABufSurface> aSurface);

  WaylandBufferDMABUF* AsWaylandBufferDMABUF() override { return this; };

  GLuint GetTexture() override { return mDMABufSurface->GetTexture(); };
  void DestroyGLResources() override { mDMABufSurface->ReleaseTextures(); };
  gfx::SurfaceFormat GetSurfaceFormat() override {
    return mDMABufSurface->GetFormat();
  }
  DMABufSurface* GetSurface() { return mDMABufSurface; }

#ifdef MOZ_LOGGING
  void DumpToFile(const char* aHint) override;
#endif

 protected:
  bool CreateWlBuffer() override;

 private:
  explicit WaylandBufferDMABUF(const LayoutDeviceIntSize& aSize);
  ~WaylandBufferDMABUF();

  RefPtr<DMABufSurface> mDMABufSurface;
};

class WaylandBufferDMABUFHolder final {
 public:
  bool Matches(DMABufSurface* aSurface) const;

  wl_buffer* GetWLBuffer() { return mWLBuffer; }

  WaylandBufferDMABUFHolder(DMABufSurface* aSurface, wl_buffer* aWLBuffer);
  ~WaylandBufferDMABUFHolder();

 private:
  wl_buffer* mWLBuffer = nullptr;
  uint32_t mUID = 0;
  uint32_t mPID = 0;
};

}  // namespace mozilla::widget

#endif  // _MOZILLA_WIDGET_GTK_WAYLAND_BUFFER_H
