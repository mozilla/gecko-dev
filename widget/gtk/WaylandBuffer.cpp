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
gfx::SurfaceFormat WaylandBuffer::mFormat = gfx::SurfaceFormat::B8G8R8A8;

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
  wl_buffer* buffer = GetWlBuffer();
  if (buffer) {
    mSurface = std::move(aWaylandSurface);
  }
  LOGWAYLAND(
      "WaylandBuffer::BorrowBuffer() [%p] WaylandSurface [%p] wl_buffer [%p]",
      (void*)this, aWaylandSurface.get(), buffer);
  return buffer;
}

void WaylandBuffer::BufferReleaseCallbackHandler(wl_buffer* aBuffer) {
  LOGWAYLAND(
      "WaylandBuffer::BufferReleaseCallbackHandler() [%p] WaylandSurface [%p] "
      "received wl_buffer [%p]",
      (void*)this, mSurface.get(), aBuffer);

  WaylandSurfaceLock lock(mSurface);

  // We want WaylandBuffer::IsAttached() to return false at DetachedLocked()
  // so set WaylandBuffer::mSurface to nullptr now.
  RefPtr<WaylandSurface> surface = std::move(mSurface);
  surface->DetachedLocked(lock, this);
}

static void BufferReleaseCallbackHandler(void* aData, wl_buffer* aBuffer) {
  LOGWAYLAND("BufferReleaseCallbackHandler() [%p] received wl_buffer [%p]",
             aData, aBuffer);
  RefPtr<WaylandBuffer> buffer = static_cast<WaylandBuffer*>(aData);
  if (!buffer) {
    NS_WARNING(
        "WaylandBuffer::BufferReleaseCallbackHandler() called after wl_buffer "
        "release. Possible compositor bug?");
    return;
  }
  buffer->BufferReleaseCallbackHandler(aBuffer);
}

static const struct wl_buffer_listener sBufferListenerWaylandBuffer = {
    BufferReleaseCallbackHandler};

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

  buffer->mWLBuffer = wl_shm_pool_create_buffer(
      buffer->mShmPool->GetShmPool(), 0, aSize.width, aSize.height,
      aSize.width * BUFFER_BPP, WL_SHM_FORMAT_ARGB8888);
  if (!buffer->mWLBuffer) {
    LOGWAYLAND("  failed to create wl_buffer");
    return nullptr;
  }

  if (wl_buffer_add_listener(buffer->GetWlBuffer(),
                             &sBufferListenerWaylandBuffer, buffer.get()) < 0) {
    LOGWAYLAND("  failed to attach listener");
    return nullptr;
  }

  LOGWAYLAND("  created [%p] WaylandDisplay [%p]\n", buffer.get(),
             waylandDisplay);

  return buffer;
}

WaylandBufferSHM::WaylandBufferSHM(const LayoutDeviceIntSize& aSize)
    : WaylandBuffer(aSize) {
  LOGWAYLAND("WaylandBufferSHM::WaylandBufferSHM() [%p]\n", (void*)this);
}

WaylandBufferSHM::~WaylandBufferSHM() {
  LOGWAYLAND("WaylandBufferSHM::~WaylandBufferSHM() [%p]\n", (void*)this);
  ReleaseWlBuffer();
}

void WaylandBufferSHM::ReleaseWlBuffer() {
  LOGWAYLAND("WaylandBufferSHM::ReleaseWlBuffer() [%p] wl_buffer [%p]\n",
             (void*)this, mWLBuffer);
  // Set mWLBuffer user data explicitly, ensure BufferReleaseCallbackHandler()
  // will get nullptr aData reference.
  wl_proxy_set_user_data((struct wl_proxy*)mWLBuffer, nullptr);
  MozClearPointer(mWLBuffer, wl_buffer_destroy);
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
    const LayoutDeviceIntSize& aSize, GLContext* aGL) {
  RefPtr<WaylandBufferDMABUF> buffer = new WaylandBufferDMABUF(aSize);

  const auto flags =
      static_cast<DMABufSurfaceFlags>(DMABUF_TEXTURE | DMABUF_ALPHA);
  buffer->mDMABufSurface =
      DMABufSurfaceRGBA::CreateDMABufSurface(aSize.width, aSize.height, flags);
  if (!buffer->mDMABufSurface || !buffer->mDMABufSurface->CreateTexture(aGL)) {
    LOGWAYLAND("  failed to create texture");
    return nullptr;
  }

  LOGWAYLAND("WaylandBufferDMABUF::CreateRGBA() [%p] UID %d [%d x %d]",
             (void*)buffer, buffer->mDMABufSurface->GetUID(), aSize.width,
             aSize.height);

  if (!buffer->mDMABufSurface->CreateWlBuffer()) {
    LOGWAYLAND("  failed to create wl_buffer");
    return nullptr;
  }

  if (wl_buffer_add_listener(buffer->GetWlBuffer(),
                             &sBufferListenerWaylandBuffer, buffer.get()) < 0) {
    LOGWAYLAND("  failed to attach listener");
    return nullptr;
  }

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

  if (!buffer->mDMABufSurface->CreateWlBuffer()) {
    LOGWAYLAND("  failed to create wl_buffer");
    return nullptr;
  }

  if (wl_buffer_add_listener(buffer->GetWlBuffer(),
                             &sBufferListenerWaylandBuffer, buffer.get()) < 0) {
    LOGWAYLAND("  failed to attach listener!");
    return nullptr;
  }

  LOGWAYLAND("  wl_buffer [%p]", buffer->GetWlBuffer());
  return buffer.forget();
}

WaylandBufferDMABUF::WaylandBufferDMABUF(const LayoutDeviceIntSize& aSize)
    : WaylandBuffer(aSize) {
  LOGWAYLAND("WaylandBufferDMABUF::WaylandBufferDMABUF [%p]\n", (void*)this);
}

WaylandBufferDMABUF::~WaylandBufferDMABUF() {
  LOGWAYLAND("WaylandBufferDMABUF::~WaylandBufferDMABUF [%p]\n", (void*)this);
}

}  // namespace mozilla::widget
