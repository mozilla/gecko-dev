/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Derived from Weston project,
 * https://github.com/wayland-project/weston/blob/master/clients/simple-shm.c
 */
/*
- make it work single thread, no e10s
- simple multi-thread app
- abort() on exhausted buffer pool

TODO:
moz-container -> display check
X11CompositorWidget - update
nsWindow::GetCompositorWidgetInitData
GDK_WINDOWING_X11 - remove
#ifdef GDK_WINDOWING_WAYLAND + display test
- ensure we always draw to container
- surface cleaning/realocation
- can we redraw on allocate?
- create subsurface on show
- share fd/pool with more buffers?
- resize (pool size) optimization
- pool of available buffers?
- call wayland display/queue events right after attach&co?
*/
#include <assert.h>
#include <poll.h>

#include "WindowSurfaceWayland.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Tools.h"
#include "gfxPlatform.h"
#include "os-compatibility.h"

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif
#include <sys/mman.h>
#include <fcntl.h>

namespace mozilla {
namespace widget {

bool                WindowSurfaceWayland::mIsAvailable;
bool                WindowSurfaceWayland::mInitialized;
gfx::SurfaceFormat  WindowSurfaceWayland::mFormat = gfx::SurfaceFormat::UNKNOWN;
wl_shm*             WindowSurfaceWayland::mShm;
wl_event_queue*     WindowSurfaceWayland::mQueue;
GThread*            WindowSurfaceWayland::mThread;
wl_display*         WindowSurfaceWayland::mDisplay;

bool SurfaceBuffer::CreateShmPool(int aSize)
{
  mAllocatedSize = aSize;

  mShmPoolFd = os_create_anonymous_file(mAllocatedSize);
  if (mShmPoolFd < 0)
    return false;

  mBufferData = mmap(nullptr, mAllocatedSize,
                     PROT_READ | PROT_WRITE, MAP_SHARED, mShmPoolFd, 0);
  if (mBufferData == MAP_FAILED) {
    close(mShmPoolFd);
    mShmPoolFd = 0;
    return false;
  }

  mShmPool = wl_shm_create_pool(WindowSurfaceWayland::GetShm(),
                                mShmPoolFd, mAllocatedSize);
  wl_proxy_set_queue((struct wl_proxy *)mShmPool,
                     WindowSurfaceWayland::GetQueue());
                                
  return true;
}

bool SurfaceBuffer::ResizeShmPool(int aSize)
{
  // We do size increase only
  if (aSize <= mAllocatedSize)
    return true;

  if (ftruncate(mShmPoolFd, aSize) < 0)
    return false;

#ifdef HAVE_POSIX_FALLOCATE
  errno = posix_fallocate(mShmPoolFd, 0, aSize);
  if (errno != 0)
    return false;
#endif

  wl_shm_pool_resize(mShmPool, aSize);

  munmap(mBufferData, mAllocatedSize);

  mBufferData = mmap(nullptr, aSize,
                     PROT_READ | PROT_WRITE, MAP_SHARED, mShmPoolFd, 0);
  if (mBufferData == MAP_FAILED)
    return false;

  mAllocatedSize = aSize;
  return true;
}

void SurfaceBuffer::ReleaseShmPool()
{
  munmap(mBufferData, mAllocatedSize);
  wl_shm_pool_destroy(mShmPool);
  close(mShmPoolFd);

  mBufferData = nullptr;
  mAllocatedSize = 0;
}

static void
buffer_release(void *data, wl_buffer *buffer)
{  
  auto surface = reinterpret_cast<SurfaceBuffer*>(data);
  surface->Detach();
}

static const struct wl_buffer_listener buffer_listener = {
  buffer_release
};

void SurfaceBuffer::CreateBuffer(int aWidth, int aHeight)
{
  mBuffer = wl_shm_pool_create_buffer(mShmPool, 0,
                              			  aWidth, aHeight, aWidth*BUFFER_BPP,
                              			  WL_SHM_FORMAT_ARGB8888);
  wl_proxy_set_queue((struct wl_proxy *)mBuffer,
                     WindowSurfaceWayland::GetQueue());
  wl_buffer_add_listener(mBuffer, &buffer_listener, this);

  mWidth = aWidth;
  mHeight = aHeight;
}

void SurfaceBuffer::ReleaseBuffer()
{
  wl_buffer_destroy(mBuffer);
  mWidth = mHeight = 0;
}

SurfaceBuffer::SurfaceBuffer(int aWidth, int aHeight)
 : mShmPool(nullptr)
  ,mShmPoolFd(0)
  ,mAllocatedSize(0)
  ,mBuffer(nullptr)
  ,mBufferData(nullptr)
  ,mWidth(0)
  ,mHeight(0)
  ,mFormat(gfx::SurfaceFormat::B8G8R8A8)
  ,mAttached(false)
{
  if(CreateShmPool(aWidth*aHeight*BUFFER_BPP)) {
    CreateBuffer(aWidth, aHeight);
  } else
    assert(0);
}

SurfaceBuffer::~SurfaceBuffer()
{
  ReleaseBuffer();
  ReleaseShmPool();
}

bool SurfaceBuffer::Resize(int aWidth, int aHeight)
{
  if (aWidth == mWidth && aHeight == mHeight)
    return true;

  ReleaseBuffer();

  int newSize = aWidth*aHeight*BUFFER_BPP;
  if (newSize > mAllocatedSize)
    ResizeShmPool(newSize);

  CreateBuffer(aWidth, aHeight);
  return (mBuffer != nullptr);
}

already_AddRefed<gfx::DrawTarget>
SurfaceBuffer::Lock()
{
  unsigned char* data = static_cast<unsigned char*>(mBufferData);
  return gfxPlatform::CreateDrawTargetForData(data,
                                              gfx::IntSize(mWidth, mHeight),
                                              mWidth*BUFFER_BPP,
                                              mFormat);
}

void
SurfaceBuffer::Attach(wl_surface* aSurface,
                      const LayoutDeviceIntRegion& aInvalidRegion)
{
  gfx::IntRect bounds = aInvalidRegion.GetBounds().ToUnknownRect();
  gfx::IntSize size(bounds.XMost(), bounds.YMost());

  fprintf(stderr, "Commit %d,%d -> %d x %d\n", bounds.x, bounds.y, size.width, size.height);
  wl_surface_damage(aSurface, bounds.x, bounds.y, size.width, size.height);
  wl_surface_attach(aSurface, mBuffer, 0, 0);
  wl_surface_commit(aSurface);

  // Taken from Hybris project:
  // Some compositors, namely Weston, queue buffer release events instead
  // of sending them immediately.  If a frame event is used, this should
  // not be a problem.  Without a frame event, we need to send a sync
  // request to ensure that they get flushed.    
  //wl_callback_destroy(wl_display_sync(WindowSurfaceWayland::GetDisplay()));
  
  wl_display_flush(WindowSurfaceWayland::GetDisplay());

  mAttached = true;
}

void
SurfaceBuffer::Detach()
{
  mAttached = false;
}

bool SurfaceBuffer::Sync(class SurfaceBuffer* aSourceBuffer)
{
  if (mWidth != aSourceBuffer->mWidth || mHeight != aSourceBuffer->mHeight)
    return false;

  int bufferSize = mWidth*mHeight*BUFFER_BPP;
  memcpy(mBufferData, aSourceBuffer->mBufferData, bufferSize);
  return true;
}

void
WindowSurfaceWayland::SetWaylandPixelFormat(uint32_t format)
{
  switch (format) {
    case WL_SHM_FORMAT_ARGB8888:
      mFormat = gfx::SurfaceFormat::B8G8R8A8;
      break;
    case WL_SHM_FORMAT_XRGB8888:
      // TODO - We can use non-alpha formats when we need that
    default:
      break;
  }
}

static void
shm_format(void *data, wl_shm *wl_shm, uint32_t format)
{
  auto interface = reinterpret_cast<WindowSurfaceWayland *>(data);
  interface->SetWaylandPixelFormat(format);
 }

struct wl_shm_listener shm_listener = {
	shm_format
};

static void
global_registry_handler(void *data, wl_registry *registry, uint32_t id,
	                      const char *interface, uint32_t version)
{
  if (strcmp(interface, "wl_shm") == 0) {
    auto shm = static_cast<wl_shm*>(
        wl_registry_bind(registry, id, &wl_shm_interface, 1));
    wl_proxy_set_queue((struct wl_proxy *)shm, WindowSurfaceWayland::GetQueue());
    wl_shm_add_listener(shm, &shm_listener, NULL);
    auto interface = reinterpret_cast<WindowSurfaceWayland *>(data);
    interface->SetShm(shm);
  }
}

static void
global_registry_remover(void *data, wl_registry *registry, uint32_t id)
{
}

static const struct wl_registry_listener registry_listener = {
  global_registry_handler,
  global_registry_remover
};

static gpointer
gst_wl_display_thread_run (gpointer data)
{
  struct pollfd fds;
  fds.fd = wl_display_get_fd (WindowSurfaceWayland::GetDisplay());
  fds.events = POLLIN;

  /* main loop */
  while (1) {
    fprintf(stderr, "***** Loop Enter 1.\n");
    while (wl_display_prepare_read_queue (WindowSurfaceWayland::GetDisplay(),
                                          WindowSurfaceWayland::GetQueue()) < 0) {
      wl_display_dispatch_queue_pending (WindowSurfaceWayland::GetDisplay(),
                                         WindowSurfaceWayland::GetQueue());
    }
    wl_display_flush (WindowSurfaceWayland::GetDisplay());

    fprintf(stderr, "***** Loop Enter 2.\n");
    int ret = poll(&fds, 1, -1);
    fprintf(stderr, "***** Loop Enter 3.\n");
    if (ret == -1) {
      wl_display_cancel_read(WindowSurfaceWayland::GetDisplay());
      fprintf(stderr, "***** Loop error!!\n");
      break;
    }
    wl_display_read_events(WindowSurfaceWayland::GetDisplay());
    wl_display_dispatch_queue_pending (WindowSurfaceWayland::GetDisplay(), 
                                       WindowSurfaceWayland::GetQueue());
  }

  return NULL;
}

extern "C" {
struct wl_event_queue* moz_container_get_wl_queue();
}

void
WindowSurfaceWayland::Init()
{
  // Try to initialize only once
  if (mInitialized)
    return;
  mInitialized = true;
  
  mQueue = moz_container_get_wl_queue();

  // wl_shm and wl_subcompositor are not provided by Gtk so we need
  // to query wayland directly
  wl_registry* registry = wl_display_get_registry(mDisplay);
  wl_proxy_set_queue((struct wl_proxy *)registry, mQueue);
  wl_registry_add_listener(registry,
                           &registry_listener, nullptr);
  wl_display_dispatch_queue(mDisplay, mQueue);
  wl_display_roundtrip_queue(mDisplay, mQueue);

  // We should have a valid pixel format now
  mIsAvailable = (mFormat != gfx::SurfaceFormat::UNKNOWN);
  NS_ASSERTION(mIsAvailable, "We don't have any pixel format!");

  GError *err = nullptr;
  mThread = g_thread_try_new ("GstWlDisplay", gst_wl_display_thread_run,
                              this, &err);
}

WindowSurfaceWayland::WindowSurfaceWayland(wl_display *aDisplay,
                                           wl_surface *aSurface)
  : mSurface(aSurface)
  , mBuffers{nullptr, nullptr}
  , mFrontBuffer(0)
  , mLastBuffer(0)
{
  NS_ASSERTION(mSurface != nullptr,
               "Missing Wayland surfaces to draw to!");

  mDisplay = aDisplay;
  Init();
  wl_proxy_set_queue((struct wl_proxy *)mSurface, mQueue);
}

WindowSurfaceWayland::~WindowSurfaceWayland()
{
  // TODO - free registry, buffers etc.
}

SurfaceBuffer*
WindowSurfaceWayland::SetBufferToDraw(int aWidth, int aHeight)
{
  int i;
  for (i = 0; i < 100; i++) {
    if (!mBuffers[i] || !mBuffers[i]->IsAttached()) {
      mFrontBuffer = i;
      break;
    }
  }
    
  assert(i != 100);
  
  if (mBuffers[mFrontBuffer] != nullptr) {
    mBuffers[mFrontBuffer]->Resize(aWidth, aHeight);
  } else {
    mBuffers[mFrontBuffer] = new SurfaceBuffer(aWidth, aHeight);    
  }

  fprintf(stderr, "WindowSurfaceWayland %p Take Buffer[%d] %d x %d\n", this, mFrontBuffer, aWidth, aHeight);

  /*
  // Sync last and front buffer
  if (mFrontBuffer != mLastBuffer) {  
    mBuffers[mFrontBuffer]->Sync(mBuffers[mLastBuffer]);
    mLastBuffer = mFrontBuffer;
  }
  */

  return mBuffers[mFrontBuffer]->IsValid() ? mBuffers[mFrontBuffer] : nullptr;
}

already_AddRefed<gfx::DrawTarget>
WindowSurfaceWayland::Lock(const LayoutDeviceIntRegion& aRegion)
{
  gfx::IntRect bounds = aRegion.GetBounds().ToUnknownRect();
  gfx::IntSize size(bounds.XMost(), bounds.YMost());

  SurfaceBuffer* buffer = SetBufferToDraw(size.width, size.height);
  if (!buffer)
    return nullptr;

  return buffer->Lock();
}

void
WindowSurfaceWayland::Commit(const LayoutDeviceIntRegion& aInvalidRegion)
{
  MOZ_ASSERT(mBuffers[mFrontBuffer], "Attempted to commit invalid surface!");  
  mBuffers[mFrontBuffer]->Attach(mSurface, aInvalidRegion);
}

}  // namespace widget
}  // namespace mozilla
