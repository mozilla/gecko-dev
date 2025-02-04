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

wl_buffer* WaylandBuffer::BorrowBuffer(RefPtr<WaylandSurface> aWaylandSurface) {
  MOZ_RELEASE_ASSERT(!mSurface, "We're already attached!");

  if (CreateWlBuffer()) {
    mSurface = std::move(aWaylandSurface);
  }

  LOGWAYLAND(
      "WaylandBuffer::BorrowBuffer() [%p] WaylandSurface [%p] wl_buffer [%p]",
      (void*)this, mSurface ? mSurface->GetLoggingWidget() : nullptr,
      mWLBuffer);

  MOZ_DIAGNOSTIC_ASSERT(!IsWaitingToBufferDelete(), "We're already deleted!");
  return mWLBuffer;
}

void WaylandBuffer::DeleteWlBuffer() {
  if (!mWLBuffer) {
    return;
  }
  LOGWAYLAND("WaylandBuffer::DeleteWlBuffer() [%p] wl_buffer [%p]\n",
             (void*)this, mWLBuffer);
  MozClearPointer(mWLBuffer, wl_buffer_destroy);
}

static void BufferDeleteSyncFinished(void* aData, struct wl_callback* callback,
                                     uint32_t time) {
  LOGWAYLAND("BufferDeleteSyncFinished() [%p]", aData);
  RefPtr buffer = already_AddRefed(static_cast<WaylandBuffer*>(aData));
  // wl_buffer should be already deleted on our side.
  buffer->BufferDetachedCallbackHandler(nullptr, /* aWlBufferDeleted */ true);
}

static const struct wl_callback_listener sBufferDeleteSyncListener = {
    .done = BufferDeleteSyncFinished,
};

void WaylandBuffer::ReturnBuffer(RefPtr<WaylandSurface> aWaylandSurface) {
  LOGWAYLAND("WaylandBuffer::ReturnBuffer() [%p] WaylandSurface [%p]",
             (void*)this, mSurface.get());

  MutexAutoLock lock(mBufferReleaseMutex);
  MOZ_RELEASE_ASSERT(aWaylandSurface == mSurface || !mSurface);

  if (mBufferDeleteSyncCallback) {
    MOZ_DIAGNOSTIC_ASSERT(!HasWlBuffer());
    return;
  }

  DeleteWlBuffer();

  // We're already detached from WaylandSurface
  if (!mSurface) {
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

void WaylandBuffer::BufferDetachedCallbackHandler(wl_buffer* aBuffer,
                                                  bool aWlBufferDeleted) {
  LOGWAYLAND(
      "WaylandBuffer::BufferDetachedCallbackHandler() [%p] WaylandSurface [%p] "
      "aBuffer [%p] aWlBufferDeleted %d GetWlBuffer() [%p]",
      (void*)this, mSurface ? mSurface->GetLoggingWidget() : nullptr, aBuffer,
      aWlBufferDeleted, GetWlBuffer());

  // BufferDetachedCallbackHandler() should be caled by Wayland compostor
  // on main thread only.
  AssertIsOnMainThread();

  // aWlBufferDeleted means wl_buffer should be nullptr
  MOZ_DIAGNOSTIC_ASSERT(!aBuffer == aWlBufferDeleted);

  RefPtr<WaylandSurface> surface;

  // Don't take mBufferReleaseMutex and mSurface locks together,
  // may lead to deadlock.
  {
    MutexAutoLock lock(mBufferReleaseMutex);

    // We should release correct buffer.
    // If GetWlBuffer() is nullptr (deleted) we should have valid delete
    // callback.
    MOZ_DIAGNOSTIC_ASSERT(aBuffer == GetWlBuffer() ||
                          (!GetWlBuffer() && mBufferDeleteSyncCallback));

    if (aWlBufferDeleted) {
      MOZ_DIAGNOSTIC_ASSERT(mBufferDeleteSyncCallback);
      mBufferDeleteSyncCallback = nullptr;
    }

    // We might be unreffed by previous BufferDetachedCallbackHandler() callback
    // as it's called for both wl_buffer delete and wl_buffer detach events.
    if (!mSurface) {
      return;
    }

    // Clear surface reference so WaylandBuffer is marked as not attached now.
    surface = std::move(mSurface);
  }

  // Notify WaylandSurface we're detached by Wayland compositor
  // so it can clear reference to us.
  WaylandSurfaceLock surfaceLock(surface);
  surface->DetachedByWaylandCompositorLocked(surfaceLock, this);
}

static void BufferDetachedCallbackHandler(void* aData, wl_buffer* aBuffer) {
  LOGWAYLAND("BufferDetachedCallbackHandler() [%p] received wl_buffer [%p]",
             aData, aBuffer);
  RefPtr<WaylandBuffer> buffer = static_cast<WaylandBuffer*>(aData);
  buffer->BufferDetachedCallbackHandler(aBuffer, /* aWlBufferDeleted */ false);
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
  MOZ_DIAGNOSTIC_ASSERT(!IsWaitingToBufferDelete());
  MOZ_DIAGNOSTIC_ASSERT(!IsAttached());
  if (!IsAttached()) {
    DeleteWlBuffer();
  }
  MOZ_DIAGNOSTIC_ASSERT(!HasWlBuffer());
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
  MOZ_DIAGNOSTIC_ASSERT(!IsWaitingToBufferDelete());
  MOZ_DIAGNOSTIC_ASSERT(!IsAttached());
  if (!IsAttached()) {
    DeleteWlBuffer();
  }
  MOZ_DIAGNOSTIC_ASSERT(!HasWlBuffer());
}

}  // namespace mozilla::widget
