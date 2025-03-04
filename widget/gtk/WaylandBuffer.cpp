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
#include "mozilla/ipc/SharedMemoryHandle.h"
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

  auto handle = ipc::shared_memory::Create(aSize);
  if (!handle) {
    NS_WARNING("WaylandShmPool: Unable to allocate shared memory!");
    return nullptr;
  }

  shmPool->mShmHandle = handle.Clone();
  shmPool->mShmPool =
      wl_shm_create_pool(aWaylandDisplay->GetShm(),
                         handle.Clone().TakePlatformHandle().get(), aSize);
  if (!shmPool->mShmPool) {
    NS_WARNING("WaylandShmPool: Unable to allocate shared memory pool!");
    return nullptr;
  }

  return shmPool;
}

void* WaylandShmPool::GetImageData() {
  if (!mShm) {
    mShm = mShmHandle.Map();
    if (!mShm) {
      NS_WARNING("WaylandShmPool: Failed to map Shm!");
      return nullptr;
    }
  }
  return mShm.Address();
}

WaylandShmPool::~WaylandShmPool() {
  MozClearPointer(mShmPool, wl_shm_pool_destroy);
}

WaylandBuffer::WaylandBuffer(const LayoutDeviceIntSize& aSize) : mSize(aSize) {}

bool WaylandBuffer::IsAttachedToSurface(WaylandSurface* aWaylandSurface) {
  return mAttachedToSurface == aWaylandSurface;
}

wl_buffer* WaylandBuffer::BorrowBuffer(WaylandSurfaceLock& aSurfaceLock) {
  LOGWAYLAND(
      "WaylandBuffer::BorrowBuffer() [%p] WaylandSurface [%p] wl_buffer [%p]",
      (void*)this,
      mAttachedToSurface ? mAttachedToSurface->GetLoggingWidget() : nullptr,
      mWLBuffer);

  MOZ_RELEASE_ASSERT(!mAttachedToSurface && !mIsAttachedToCompositor,
                     "We're already attached!");
  MOZ_DIAGNOSTIC_ASSERT(!mBufferDeleteSyncCallback, "We're already deleted!?");

  if (!CreateWlBuffer()) {
    return nullptr;
  }

  mAttachedToSurface = aSurfaceLock.GetWaylandSurface();

  LOGWAYLAND(
      "WaylandBuffer::BorrowBuffer() [%p] WaylandSurface [%p] wl_buffer [%p]",
      (void*)this,
      mAttachedToSurface ? mAttachedToSurface->GetLoggingWidget() : nullptr,
      mWLBuffer);

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

void WaylandBuffer::ReturnBufferDetached(WaylandSurfaceLock& aSurfaceLock) {
  LOGWAYLAND("WaylandBuffer::ReturnBufferDetached() [%p] WaylandSurface [%p]",
             (void*)this, mAttachedToSurface.get());
  MOZ_DIAGNOSTIC_ASSERT(aSurfaceLock.GetWaylandSurface() == mAttachedToSurface);
  DeleteWlBuffer();
  mIsAttachedToCompositor = false;
  mAttachedToSurface = nullptr;
}

struct SurfaceAndBuffer {
  SurfaceAndBuffer(WaylandSurface* aSurface, WaylandBuffer* aBuffer)
      : mSurface(aSurface), mBuffer(aBuffer) {};

  RefPtr<WaylandSurface> mSurface;
  RefPtr<WaylandBuffer> mBuffer;
};

static void BufferDeleteSyncFinished(void* aData, struct wl_callback* callback,
                                     uint32_t time) {
  UniquePtr<SurfaceAndBuffer> ref(static_cast<SurfaceAndBuffer*>(aData));
  LOGWAYLAND(
      "BufferDeleteSyncFinished() WaylandSurface [%p] WaylandBuffer [%p]",
      ref->mSurface.get(), ref->mBuffer.get());

  ref->mBuffer->ClearSyncHandler();
  ref->mSurface->BufferFreeCallbackHandler(ref->mBuffer->GetWlBufferID(),
                                           /* aWlBufferDelete */ true);
}

static const struct wl_callback_listener sBufferDeleteSyncListener = {
    .done = BufferDeleteSyncFinished,
};

void WaylandBuffer::ClearSyncHandler() {
  AssertIsOnMainThread();
  MOZ_DIAGNOSTIC_ASSERT(!mWLBuffer);
  mBufferDeleteSyncCallback = nullptr;
}

void WaylandBuffer::ReturnBufferAttached(WaylandSurfaceLock& aSurfaceLock) {
  LOGWAYLAND("WaylandBuffer::ReturnBufferAttached() [%p] WaylandSurface [%p]",
             (void*)this, mAttachedToSurface.get());

  MOZ_DIAGNOSTIC_ASSERT(aSurfaceLock.GetWaylandSurface() == mAttachedToSurface);
  MOZ_DIAGNOSTIC_ASSERT(mIsAttachedToCompositor,
                        "WaylandBuffer is not attached to compostor!");

  // It's possible that ReturnBufferAttached() is called twice for the same
  // WaylandBuffer, may happens if WaylandSurface is
  // unmapped -> mapped -> unmapped quickly so the mBufferDeleteSyncCallback
  // from the first unmap is not finished yet.
  if (mBufferDeleteSyncCallback) {
    MOZ_DIAGNOSTIC_ASSERT(!mWLBuffer, "We should not have wl_buffer!");
    return;
  }

  MOZ_DIAGNOSTIC_ASSERT(mWLBuffer, "Missing wl_buffer!");

  // Delete wl_buffer now and use wl_display_sync() to make sure
  // it's really deleted.
  DeleteWlBuffer();

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
  wl_callback_add_listener(mBufferDeleteSyncCallback,
                           &sBufferDeleteSyncListener,
                           new SurfaceAndBuffer(mAttachedToSurface, this));
  return;
}

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
  mWLBuffer = wl_shm_pool_create_buffer(mShmPool->GetShmPool(), 0, mSize.width,
                                        mSize.height, mSize.width * BUFFER_BPP,
                                        WL_SHM_FORMAT_ARGB8888);
  mWLBufferID = reinterpret_cast<uintptr_t>(mWLBuffer);
  LOGWAYLAND("WaylandBufferSHM::CreateWlBuffer() [%p] wl_buffer [%p]",
             (void*)this, mWLBuffer);

  return !!mWLBuffer;
}

WaylandBufferSHM::WaylandBufferSHM(const LayoutDeviceIntSize& aSize)
    : WaylandBuffer(aSize) {
  LOGWAYLAND("WaylandBufferSHM::WaylandBufferSHM() [%p]\n", (void*)this);
}

WaylandBufferSHM::~WaylandBufferSHM() {
  LOGWAYLAND("WaylandBufferSHM::~WaylandBufferSHM() [%p]\n", (void*)this);
  MOZ_RELEASE_ASSERT(!mBufferDeleteSyncCallback);
  MOZ_RELEASE_ASSERT(!IsAttached());
  // We can delete wl_buffer as it not attached.
  DeleteWlBuffer();
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
    return mWLBuffer;
  }

  mWLBuffer = mDMABufSurface->CreateWlBuffer();
  mWLBufferID = reinterpret_cast<uintptr_t>(mWLBuffer);

  LOGWAYLAND("WaylandBufferDMABUF::CreateWlBuffer() [%p] UID %d wl_buffer [%p]",
             (void*)this, mDMABufSurface->GetUID(), mWLBuffer);

  return !!mWLBuffer;
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
  // We can delete wl_buffer as it not attached.
  DeleteWlBuffer();
}

}  // namespace mozilla::widget
