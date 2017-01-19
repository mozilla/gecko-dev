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
- optimization -> use Image bufer when update area is smaller that whole window
- is bounds.x bounds.y non-zero??
- buffer sync - can be undamaged part unsynced?
- GdkWidnow - show/hide -> callback, get surface and frame callback
- optimization - give backbuffer directly when requested whole area in Lock()
- how big is rectangle owerlap in BackBufferWayland::CopyRectangle()?
(firefox:15155): Gdk-WARNING **: Tried to map a popup with a non-top most parent
  - it was ok in X11
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

ImageBuffer::ImageBuffer()
  : mBufferData(nullptr)
  , mBufferAllocated(0)
  , mWidth(0)
  , mHeight(0)
{
}

ImageBuffer::~ImageBuffer()
{
  if (mBufferData)
    free(mBufferData);
}

already_AddRefed<gfx::DrawTarget>
ImageBuffer::Lock(const LayoutDeviceIntRegion& aRegion)
{    
  gfx::IntRect bounds = aRegion.GetBounds().ToUnknownRect();
  gfx::IntSize imageSize(bounds.XMost(), bounds.YMost());

  // We use the same trick as nsShmImage::CreateDrawTarget() does:
  // Due to bug 1205045, we must avoid making GTK calls off the main thread
  // to query window size.
  // Instead we just track the largest offset within the image we are
  // drawing to and grow the image to accomodate it. Since usually
  // the entire window is invalidated on the first paint to it,
  // this should grow the image to the necessary size quickly without
  // many intermediate reallocations.
  int newSize = imageSize.width * imageSize.height * BUFFER_BPP;
  if (!mBufferData || mBufferAllocated < newSize) {
    if (mBufferData) {
      free(mBufferData);
    }
    
    mBufferData = (unsigned char*)malloc(newSize);
    if (!mBufferData)
      return nullptr;
    
    mBufferAllocated = newSize;
  }

  mWidth = imageSize.width;
  mHeight = imageSize.height;

  return gfxPlatform::CreateDrawTargetForData(mBufferData, imageSize,
    BUFFER_BPP * mWidth, WindowSurfaceWayland::GetSurfaceFormat());
}

bool BackBufferWayland::CreateShmPool(int aSize)
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

bool BackBufferWayland::ResizeShmPool(int aSize)
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

void BackBufferWayland::ReleaseShmPool()
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
  auto surface = reinterpret_cast<BackBufferWayland*>(data);
  surface->Detach();
}

static const struct wl_buffer_listener buffer_listener = {
  buffer_release
};

void BackBufferWayland::CreateBuffer(int aWidth, int aHeight)
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

void BackBufferWayland::ReleaseBuffer()
{
  wl_buffer_destroy(mBuffer);
  mWidth = mHeight = 0;
}

BackBufferWayland::BackBufferWayland(int aWidth, int aHeight)
 : mShmPool(nullptr)
  ,mShmPoolFd(0)
  ,mAllocatedSize(0)
  ,mBuffer(nullptr)
  ,mBufferData(nullptr)
  ,mWidth(aWidth)
  ,mHeight(aHeight)
  ,mAttached(false)
{
  if(CreateShmPool(aWidth*aHeight*BUFFER_BPP)) {
    CreateBuffer(aWidth, aHeight);
  } else
    assert(0);
}

BackBufferWayland::~BackBufferWayland()
{
  ReleaseBuffer();
  ReleaseShmPool();
}

bool
BackBufferWayland::Resize(int aWidth, int aHeight)
{
  if (aWidth == mWidth && aHeight == mHeight)
    return true;

  fprintf(stderr, "BackBufferResize, %dx%d -> %dx%d",
          mWidth, mHeight, aWidth, aHeight);
  ReleaseBuffer();

  int newSize = aWidth*aHeight*BUFFER_BPP;
  if (newSize > mAllocatedSize)
    ResizeShmPool(newSize);

  CreateBuffer(aWidth, aHeight);
  return (mBuffer != nullptr);
}

// Update back buffer with image data from ImageBuffer
void
BackBufferWayland::CopyRectangle(ImageBuffer *aImage,
                                 const mozilla::LayoutDeviceIntRect &rect)
{
  mozilla::LayoutDeviceIntRect r = rect;

  if (r.x + r.width > mWidth)
    r.width = mWidth - r.x;
  if (r.y + r.height > mHeight)
    r.height = mHeight - r.y;

  for (int y = r.y; y < r.y + r.height; y++) {
    int start = (y * mWidth + r.x) * BUFFER_BPP;
    int lenght = r.width * BUFFER_BPP;
    memcpy((unsigned char *)mBufferData + start,
            aImage->GetData() + ((y * aImage->mWidth) + r.x) * BUFFER_BPP,
            lenght);
  }
}

void
BackBufferWayland::Attach(wl_surface* aSurface)
{
  // Taken from Hybris project:
  // Some compositors, namely Weston, queue buffer release events instead
  // of sending them immediately.  If a frame event is used, this should
  // not be a problem.  Without a frame event, we need to send a sync
  // request to ensure that they get flushed.    
  //wl_callback_destroy(wl_display_sync(WindowSurfaceWayland::GetDisplay()));
  
  wl_surface_attach(aSurface, mBuffer, 0, 0);
  wl_surface_commit(aSurface);
  wl_display_flush(WindowSurfaceWayland::GetDisplay());
  mAttached = true;
}

void
BackBufferWayland::Detach()
{
  mAttached = false;
}

bool BackBufferWayland::Sync(class BackBufferWayland* aSourceBuffer)
{
  bool bufferSizeMatches = MatchSize(aSourceBuffer);
  if (!bufferSizeMatches) {
    Resize(aSourceBuffer->mWidth, aSourceBuffer->mHeight);
  }

  memcpy(mBufferData, aSourceBuffer->mBufferData,
         aSourceBuffer->mWidth * aSourceBuffer->mHeight * BUFFER_BPP);
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
    while (wl_display_prepare_read_queue (WindowSurfaceWayland::GetDisplay(),
                                          WindowSurfaceWayland::GetQueue()) < 0) {
      wl_display_dispatch_queue_pending (WindowSurfaceWayland::GetDisplay(),
                                         WindowSurfaceWayland::GetQueue());
    }
    wl_display_flush (WindowSurfaceWayland::GetDisplay());

    int ret = poll(&fds, 1, -1);
    if (ret == -1) {
      wl_display_cancel_read(WindowSurfaceWayland::GetDisplay());
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

static void
redraw(void *data, struct wl_callback *callback, uint32_t time)
{
    auto surface = reinterpret_cast<WindowSurfaceWayland*>(data);
    surface->Draw();
}

static const struct wl_callback_listener frame_listener = {
    redraw
};

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

  // We need two roundtrips here to get the registry info
  wl_display_dispatch_queue(mDisplay, mQueue);
  wl_display_roundtrip_queue(mDisplay, mQueue);
  wl_display_roundtrip_queue(mDisplay, mQueue);

  // We should have a valid pixel format now
  mIsAvailable = (mFormat != gfx::SurfaceFormat::UNKNOWN);
  NS_ASSERTION(mIsAvailable, "We don't have any pixel format!");
  
  assert(mFormat != gfx::SurfaceFormat::UNKNOWN);

  GError *err = nullptr;
  mThread = g_thread_try_new ("GstWlDisplay", gst_wl_display_thread_run,
                              this, &err);
}

WindowSurfaceWayland::WindowSurfaceWayland(nsWindow *aWidget,
                                           wl_display *aDisplay,
                                           wl_surface *aSurface)
  : mWidget(aWidget)
  , mSurface(aSurface)
  , mFrontBuffer(nullptr)
  , mBackBuffer(nullptr)
  , mFrameCallback(nullptr)
  , mDelayedCommit(false)
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

BackBufferWayland*
WindowSurfaceWayland::GetBufferToDraw(int aWidth, int aHeight)
{
  if (!mFrontBuffer) {
    mFrontBuffer = new BackBufferWayland(aWidth, aHeight);
    mBackBuffer = new BackBufferWayland(aWidth, aHeight);  
  } else {
    if (mFrontBuffer->IsAttached()) {
      if (mBackBuffer->IsAttached()) {
        NS_ASSERTION(!mBackBuffer->IsAttached(), "We don't have any buffer to draw to!");
        return nullptr;
      }
      
      BackBufferWayland *tmp = mFrontBuffer;
      mFrontBuffer = mBackBuffer;
      mBackBuffer = tmp;

      mFrontBuffer->Sync(mBackBuffer);      
    }

    if (!mFrontBuffer->MatchSize(aWidth, aHeight)) {
      mFrontBuffer->Resize(aWidth, aHeight);
    }
  }

  return mFrontBuffer;
}

already_AddRefed<gfx::DrawTarget>
WindowSurfaceWayland::Lock(const LayoutDeviceIntRegion& aRegion)
{
  return mImageBuffer.Lock(aRegion);
}

void
WindowSurfaceWayland::Commit(const LayoutDeviceIntRegion& aInvalidRegion)
{
  gfx::IntRect bounds = aInvalidRegion.GetBounds().ToUnknownRect();
  gfx::IntSize bufferSize(bounds.XMost(), bounds.YMost());

  LayoutDeviceIntRect rect = mWidget->GetBounds();
  BackBufferWayland* buffer = GetBufferToDraw(rect.width,
                                              rect.height);
  NS_ASSERTION(buffer, "We don't have any buffer to draw to!");
  if (!buffer)
    return;

  for (auto iter = aInvalidRegion.RectIter(); !iter.Done(); iter.Next()) {
    const mozilla::LayoutDeviceIntRect &r = iter.Get();
    buffer->CopyRectangle(&mImageBuffer, r);
    wl_surface_damage(mSurface, r.x, r.y, r.width, r.height);
  }

  if (mFrameCallback) {
    // Do nothing here - buffer will be commited to compositor
    // in next frame callback event.
    mDelayedCommit = true;
    return;
  } else  {
    mFrameCallback = wl_surface_frame(mSurface);
    wl_callback_add_listener(mFrameCallback, &frame_listener, this);

    // There's no pending frame callback so we can draw immediately
    // and create frame callback for possible subsequent drawing.
    mFrontBuffer->Attach(mSurface);
    mDelayedCommit = false;
  }
}

void
WindowSurfaceWayland::Draw()
{
  if (mFrameCallback) {
      wl_callback_destroy(mFrameCallback);
      mFrameCallback = nullptr;
  }

  if (mDelayedCommit) {
    mFrameCallback = wl_surface_frame(mSurface);
    wl_callback_add_listener(mFrameCallback, &frame_listener, this);

    mFrontBuffer->Attach(mSurface);
    mDelayedCommit = false;
  }
}

}  // namespace widget
}  // namespace mozilla
