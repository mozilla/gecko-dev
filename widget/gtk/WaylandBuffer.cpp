/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WaylandBuffer.h"
#include "WaylandSurface.h"
#include "WaylandSurfaceLock.h"

#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>

#include "gfx2DGlue.h"
#include "gfxPlatform.h"
#include "mozilla/WidgetUtilsGtk.h"
#include "mozilla/gfx/Tools.h"
#include "nsGtkUtils.h"
#include "nsPrintfCString.h"
#include "prenv.h"  // For PR_GetEnv

#ifdef MOZ_LOGGING
#  include "mozilla/Logging.h"
#  include "mozilla/ScopeExit.h"
#  include "Units.h"
extern mozilla::LazyLogModule gWidgetWaylandLog;
#  define LOGWAYLAND(...) \
    MOZ_LOG(gWidgetWaylandLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
#else
#  define LOGWAYLAND(...)
#endif /* MOZ_LOGGING */

using namespace mozilla::gl;

namespace mozilla::widget {

#define BUFFER_BPP 4

#ifdef MOZ_LOGGING
MOZ_RUNINIT int WaylandBufferSHM::mDumpSerial =
    PR_GetEnv("MOZ_WAYLAND_DUMP_WL_BUFFERS") ? 1 : 0;
MOZ_RUNINIT char* WaylandBufferSHM::mDumpDir =
    PR_GetEnv("MOZ_WAYLAND_DUMP_DIR");
#endif

/* static */
RefPtr<WaylandShmPool> WaylandShmPool::Create(nsWaylandDisplay* aWaylandDisplay,
                                              int aSize) {
  if (!aWaylandDisplay->GetShm()) {
    NS_WARNING("WaylandShmPool: Missing Wayland shm interface!");
    return nullptr;
  }

  RefPtr<WaylandShmPool> shmPool = new WaylandShmPool();

  shmPool->mShm = MakeRefPtr<ipc::SharedMemory>();
  if (!shmPool->mShm->Create(aSize)) {
    NS_WARNING("WaylandShmPool: Unable to allocate shared memory!");
    return nullptr;
  }

  shmPool->mSize = aSize;
  shmPool->mShmPool = wl_shm_create_pool(
      aWaylandDisplay->GetShm(), shmPool->mShm->CloneHandle().get(), aSize);
  if (!shmPool->mShmPool) {
    NS_WARNING("WaylandShmPool: Unable to allocate shared memory pool!");
    return nullptr;
  }

  return shmPool;
}

void* WaylandShmPool::GetImageData() {
  if (mImageData) {
    return mImageData;
  }
  if (!mShm->Map(mSize)) {
    NS_WARNING("WaylandShmPool: Failed to map Shm!");
    return nullptr;
  }
  mImageData = mShm->Memory();
  return mImageData;
}

WaylandShmPool::~WaylandShmPool() {
  MozClearPointer(mShmPool, wl_shm_pool_destroy);
}

WaylandBuffer::WaylandBuffer(const LayoutDeviceIntSize& aSize) : mSize(aSize) {}

bool WaylandBuffer::IsAttachedToSurface(WaylandSurface* aWaylandSurface) {
  MutexAutoLock lock(mMutex);
  return mAttachedToSurface == aWaylandSurface;
}

wl_buffer* WaylandBuffer::BorrowBuffer(WaylandSurfaceLock& aSurfaceLock) {
  MutexAutoLock lock(mMutex);

  LOGWAYLAND(
      "WaylandBuffer::BorrowBuffer() [%p] WaylandSurface [%p] wl_buffer [%p]",
      (void*)this,
      mAttachedToSurface ? mAttachedToSurface->GetLoggingWidget() : nullptr,
      mWLBuffer);

  MOZ_RELEASE_ASSERT(!mAttachedToSurface, "We're already attached!");
  MOZ_DIAGNOSTIC_ASSERT(!mBufferDeleteSyncCallback, "We're already deleted!?");

  if (CreateWlBuffer()) {
    mAttachedToSurface = aSurfaceLock.GetWaylandSurface();
  }

  LOGWAYLAND(
      "WaylandBuffer::BorrowBuffer() [%p] WaylandSurface [%p] wl_buffer [%p]",
      (void*)this,
      mAttachedToSurface ? mAttachedToSurface->GetLoggingWidget() : nullptr,
      mWLBuffer);

  return mWLBuffer;
}

void WaylandBuffer::DeleteWlBuffer() {
  mMutex.AssertCurrentThreadOwns();

  if (!mWLBuffer) {
    return;
  }
  LOGWAYLAND("WaylandBuffer::DeleteWlBuffer() [%p] wl_buffer [%p]\n",
             (void*)this, mWLBuffer);
  MozClearPointer(mWLBuffer, wl_buffer_destroy);
}

void WaylandBuffer::BufferDeletedCallbackHandler() {
  RefPtr<WaylandSurface> surface;

  // Don't take mBufferReleaseMutex and mSurface locks together,
  // may lead to deadlock.
  {
    MutexAutoLock lock(mMutex);

    // BufferDeletedCallbackHandler() should be caled by Wayland compostor
    // on main thread only.
    AssertIsOnMainThread();

    LOGWAYLAND(
        "WaylandBuffer::BufferDeletedCallbackHandler() [%p] WaylandSurface "
        "[%p]",
        (void*)this,
        mAttachedToSurface ? mAttachedToSurface->GetLoggingWidget() : nullptr);

    MOZ_DIAGNOSTIC_ASSERT(mBufferDeleteSyncCallback);
    MOZ_DIAGNOSTIC_ASSERT(mAttachedToSurface);
    mBufferDeleteSyncCallback = nullptr;

    // Clear surface reference so WaylandBuffer is marked as not attached now.
    surface = std::move(mAttachedToSurface);
  }

  // Notify WaylandSurface we're detached by Wayland compositor
  // so it can clear reference to us.
  WaylandSurfaceLock surfaceLock(surface);
  surface->UntrackWaylandBufferLocked(surfaceLock, this);
}

static void BufferDeleteSyncFinished(void* aData, struct wl_callback* callback,
                                     uint32_t time) {
  LOGWAYLAND("BufferDeleteSyncFinished() [%p]", aData);
  RefPtr buffer = already_AddRefed(static_cast<WaylandBuffer*>(aData));
  // wl_buffer should be already deleted on our side.
  buffer->BufferDeletedCallbackHandler();
}

static const struct wl_callback_listener sBufferDeleteSyncListener = {
    .done = BufferDeleteSyncFinished,
};

void WaylandBuffer::ReturnBuffer(WaylandSurfaceLock& aSurfaceLock) {
  MutexAutoLock lock(mMutex);

  LOGWAYLAND("WaylandBuffer::ReturnBuffer() [%p] WaylandSurface [%p]",
             (void*)this, mAttachedToSurface.get());

  MOZ_RELEASE_ASSERT(aSurfaceLock.GetWaylandSurface() == mAttachedToSurface ||
                     !mAttachedToSurface);
  MOZ_DIAGNOSTIC_ASSERT(!mBufferDeleteSyncCallback,
                        "Buffer is already returned?");
  MOZ_DIAGNOSTIC_ASSERT(mWLBuffer);

  DeleteWlBuffer();

  if (!mIsAttachedToCompositor) {
    // We don't need to run synced delete - wl_buffer it already detached
    // from compositor so we can't get any wl_buffer_release event
    // and we're free to release this.
    RefPtr surface = std::move(mAttachedToSurface);
    surface->UntrackWaylandBufferLocked(aSurfaceLock, this);
    return;
  }

  // There are various Wayland queues processed for every thread.
  // It's possible that wl_buffer release event is pending in any
  // queue while we already asked for wl_buffer delete.
  // We need to finish wl_buffer removal when all events from this
  // point are processed so we use sync callback.
  //
  // When wl_display_sync comes back to us (from main thread)
  // we know all events are processed and there isn't any
  // wl_buffer operation pending so we can safely release WaylandSurface
  // and WaylandBuffer objects.
  mBufferDeleteSyncCallback = wl_display_sync(WaylandDisplayGetWLDisplay());
  // Addref this to keep it live until sync,
  // we're unref it at sBufferDeleteSyncListener
  AddRef();
  wl_callback_add_listener(mBufferDeleteSyncCallback,
                           &sBufferDeleteSyncListener, this);
}

void WaylandBuffer::BufferDetachedCallbackHandler(wl_buffer* aBuffer) {
  MutexAutoLock lock(mMutex);

  LOGWAYLAND("WaylandBuffer::BufferDetachedCallbackHandler() [%p] aBuffer [%p]",
             (void*)this, aBuffer);

  MOZ_DIAGNOSTIC_ASSERT(aBuffer == mWLBuffer || !mWLBuffer,
                        "Different buffer released?");

  // BufferDetachedCallbackHandler() should be caled by Wayland compostor
  // on main thread only.
  AssertIsOnMainThread();
  mIsAttachedToCompositor = false;
}

static void BufferDetachedCallbackHandler(void* aData, wl_buffer* aBuffer) {
  LOGWAYLAND("BufferDetachedCallbackHandler() [%p] received wl_buffer [%p]",
             aData, aBuffer);
  RefPtr<WaylandBuffer> buffer = static_cast<WaylandBuffer*>(aData);
  buffer->BufferDetachedCallbackHandler(aBuffer);
}

static const struct wl_buffer_listener sBufferDetachListener = {
    BufferDetachedCallbackHandler};

/* static */
RefPtr<WaylandBufferSHM> WaylandBufferSHM::Create(
    const LayoutDeviceIntSize& aSize) {
  RefPtr<WaylandBufferSHM> buffer = new WaylandBufferSHM(aSize);
  nsWaylandDisplay* waylandDisplay = WaylandDisplayGet();

  LOGWAYLAND("WaylandBufferSHM::Create() [%p] [%d x %d]", (void*)buffer,
             aSize.width, aSize.height);

  int size = aSize.width * aSize.height * BUFFER_BPP;
  buffer->mShmPool = WaylandShmPool::Create(waylandDisplay, size);
  if (!buffer->mShmPool) {
    LOGWAYLAND("  failed to create shmPool");
    return nullptr;
  }

  LOGWAYLAND("  created [%p] WaylandDisplay [%p]\n", buffer.get(),
             waylandDisplay);

  return buffer;
}

bool WaylandBufferSHM::CreateWlBuffer() {
  mMutex.AssertCurrentThreadOwns();

  if (mWLBuffer) {
    return true;
  }
  LOGWAYLAND("WaylandBufferSHM::CreateWlBuffer() [%p]", (void*)this);

  mWLBuffer = wl_shm_pool_create_buffer(mShmPool->GetShmPool(), 0, mSize.width,
                                        mSize.height, mSize.width * BUFFER_BPP,
                                        WL_SHM_FORMAT_ARGB8888);
  if (!mWLBuffer) {
    LOGWAYLAND("  failed to create wl_buffer");
    return false;
  }

  if (wl_buffer_add_listener(mWLBuffer, &sBufferDetachListener, this) < 0) {
    LOGWAYLAND("  failed to attach listener");
    return false;
  }

  return true;
}

WaylandBufferSHM::WaylandBufferSHM(const LayoutDeviceIntSize& aSize)
    : WaylandBuffer(aSize) {
  LOGWAYLAND("WaylandBufferSHM::WaylandBufferSHM() [%p]\n", (void*)this);
}

WaylandBufferSHM::~WaylandBufferSHM() {
  LOGWAYLAND("WaylandBufferSHM::~WaylandBufferSHM() [%p]\n", (void*)this);
  MOZ_RELEASE_ASSERT(!mBufferDeleteSyncCallback);
  MOZ_RELEASE_ASSERT(!IsAttached());
  MOZ_DIAGNOSTIC_ASSERT(!mWLBuffer);
}

already_AddRefed<gfx::DrawTarget> WaylandBufferSHM::Lock() {
  LOGWAYLAND("WaylandBufferSHM::lock() [%p]\n", (void*)this);
  return gfxPlatform::CreateDrawTargetForData(
      static_cast<unsigned char*>(mShmPool->GetImageData()),
      mSize.ToUnknownSize(), BUFFER_BPP * mSize.width, GetSurfaceFormat());
}

void WaylandBufferSHM::Clear() {
  LOGWAYLAND("WaylandBufferSHM::Clear() [%p]\n", (void*)this);
  memset(mShmPool->GetImageData(), 0xff,
         mSize.height * mSize.width * BUFFER_BPP);
}

#ifdef MOZ_LOGGING
void WaylandBufferSHM::DumpToFile(const char* aHint) {
  if (!mDumpSerial) {
    return;
  }

  cairo_surface_t* surface = nullptr;
  auto unmap = MakeScopeExit([&] {
    if (surface) {
      cairo_surface_destroy(surface);
    }
  });
  surface = cairo_image_surface_create_for_data(
      (unsigned char*)mShmPool->GetImageData(), CAIRO_FORMAT_ARGB32,
      mSize.width, mSize.height, BUFFER_BPP * mSize.width);
  if (cairo_surface_status(surface) == CAIRO_STATUS_SUCCESS) {
    nsCString filename;
    if (mDumpDir) {
      filename.Append(mDumpDir);
      filename.Append('/');
    }
    filename.Append(
        nsPrintfCString("firefox-wl-buffer-%.5d-%s.png", mDumpSerial++, aHint));
    cairo_surface_write_to_png(surface, filename.get());
    LOGWAYLAND("Dumped wl_buffer to %s\n", filename.get());
  }
}
#endif

/* static */
already_AddRefed<WaylandBufferDMABUF> WaylandBufferDMABUF::CreateRGBA(
    const LayoutDeviceIntSize& aSize, GLContext* aGL,
    RefPtr<DRMFormat> aFormat) {
  RefPtr<WaylandBufferDMABUF> buffer = new WaylandBufferDMABUF(aSize);

  buffer->mDMABufSurface = DMABufSurfaceRGBA::CreateDMABufSurface(
      aSize.width, aSize.height, aFormat,
      DMABUF_SCANOUT | DMABUF_USE_MODIFIERS);
  if (!buffer->mDMABufSurface || !buffer->mDMABufSurface->CreateTexture(aGL)) {
    LOGWAYLAND("  failed to create texture");
    return nullptr;
  }

  LOGWAYLAND("WaylandBufferDMABUF::CreateRGBA() [%p] UID %d [%d x %d]",
             (void*)buffer, buffer->mDMABufSurface->GetUID(), aSize.width,
             aSize.height);
  return buffer.forget();
}

/* static */
already_AddRefed<WaylandBufferDMABUF> WaylandBufferDMABUF::CreateExternal(
    RefPtr<DMABufSurface> aSurface) {
  const auto size =
      LayoutDeviceIntSize(aSurface->GetWidth(), aSurface->GetWidth());
  RefPtr<WaylandBufferDMABUF> buffer = new WaylandBufferDMABUF(size);

  LOGWAYLAND("WaylandBufferDMABUF::CreateExternal() [%p] UID %d [%d x %d]",
             (void*)buffer, aSurface->GetUID(), size.width, size.height);

  buffer->mDMABufSurface = aSurface;
  return buffer.forget();
}

bool WaylandBufferDMABUF::CreateWlBuffer() {
  mMutex.AssertCurrentThreadOwns();

  MOZ_DIAGNOSTIC_ASSERT(mDMABufSurface);
  if (mWLBuffer) {
    return true;
  }

  LOGWAYLAND("WaylandBufferDMABUF::CreateWlBuffer() [%p] UID %d", (void*)this,
             mDMABufSurface->GetUID());

  mWLBuffer = mDMABufSurface->CreateWlBuffer();
  if (!mWLBuffer) {
    LOGWAYLAND("  failed to create wl_buffer");
    return false;
  }

  if (wl_buffer_add_listener(mWLBuffer, &sBufferDetachListener, this) < 0) {
    LOGWAYLAND("  failed to attach listener!");
    return false;
  }

  return true;
}

WaylandBufferDMABUF::WaylandBufferDMABUF(const LayoutDeviceIntSize& aSize)
    : WaylandBuffer(aSize) {
  LOGWAYLAND("WaylandBufferDMABUF::WaylandBufferDMABUF [%p]\n", (void*)this);
}

WaylandBufferDMABUF::~WaylandBufferDMABUF() {
  LOGWAYLAND("WaylandBufferDMABUF::~WaylandBufferDMABUF [%p] UID %d\n",
             (void*)this, mDMABufSurface ? mDMABufSurface->GetUID() : -1);
  MOZ_RELEASE_ASSERT(!mBufferDeleteSyncCallback);
  MOZ_RELEASE_ASSERT(!IsAttached());
  MOZ_DIAGNOSTIC_ASSERT(!mWLBuffer);
}

}  // namespace mozilla::widget
