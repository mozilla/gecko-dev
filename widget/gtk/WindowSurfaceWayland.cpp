/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
TODO:
X11CompositorWidget - update
nsWindow::GetCompositorWidgetInitData
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
- GdkWidnow - show/hide -> callback, get surface and frame callback
- optimization - give backbuffer directly when requested whole area in Lock()
- how big is rectangle owerlap in WindowBackBuffer::CopyRectangle()?
(firefox:15155): Gdk-WARNING **: Tried to map a popup with a non-top most parent
  - it was ok in X11
- try to draw (first commit) when vblank comes like Gtk does, not when gecko calls it
- why we can't draw directly to back-buffer? Why do we need the image buffer??
*/

#include "WindowSurfaceWayland.h"

#include "base/message_loop.h"          // for MessageLoop
#include "base/task.h"                  // for NewRunnableMethod, etc
#include "nsPrintfCString.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Tools.h"
#include "gfxPlatform.h"
#include "mozcontainer.h"

#include <gdk/gdkwayland.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <assert.h>
#include <poll.h>

namespace mozilla {
namespace widget {

static WaylandDisplay *gWaylandDisplay = nullptr;

void
WaylandDisplayInit(wl_display *aDisplay)
{
    if (!gWaylandDisplay) {
        gWaylandDisplay = new WaylandDisplay(aDisplay);
    }
}

void
WaylandDisplay::SetWaylandPixelFormat(uint32_t format)
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
  auto interface = reinterpret_cast<WaylandDisplay *>(data);
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
    auto interface = reinterpret_cast<WaylandDisplay *>(data);
    auto shm = static_cast<wl_shm*>(
        wl_registry_bind(registry, id, &wl_shm_interface, 1));
    wl_proxy_set_queue((struct wl_proxy *)shm, interface->GetEventQueue());
    wl_shm_add_listener(shm, &shm_listener, data);
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

//Call as timer?
//Integrate to compositor loop?
void
WaylandDisplay::DisplayLoop()
{
  wl_display_roundtrip_queue(mDisplay, mEventQueue);
}

static void
RunDisplayLoop(WaylandDisplay *aWaylandDisplay)
{
  aWaylandDisplay->DisplayLoop();
  MessageLoop::current()->PostTask(
      NewRunnableFunction(&RunDisplayLoop, aWaylandDisplay));
}

WaylandDisplay::WaylandDisplay(wl_display *aDisplay)
  : mDisplay(aDisplay)
{
  mEventQueue = wl_display_create_queue(mDisplay);

  // wl_shm and wl_subcompositor are not provided by Gtk so we need
  // to query wayland directly
  wl_registry* registry = wl_display_get_registry(mDisplay);
  wl_proxy_set_queue((struct wl_proxy *)registry, mEventQueue);
  wl_registry_add_listener(registry, &registry_listener, this);

  // We need two roundtrips here to get the registry info
  wl_display_dispatch_queue(mDisplay, mEventQueue);
  wl_display_roundtrip_queue(mDisplay, mEventQueue);
  wl_display_roundtrip_queue(mDisplay, mEventQueue);

  // We must have a valid pixel format
  MOZ_RELEASE_ASSERT(mFormat != gfx::SurfaceFormat::UNKNOWN,
                     "We don't have any pixel format!");

  MessageLoop::current()->PostTask(NewRunnableFunction(&RunDisplayLoop, this));
}

int
WaylandShmPool::CreateTemporaryFile(int aSize)
{
  const char* tmppath = getenv("XDG_RUNTIME_DIR");
  MOZ_RELEASE_ASSERT(tmppath, "Missing XDG_RUNTIME_DIR env variable.");

  nsPrintfCString tmpname("%s/weston-shared-XXXXXX", tmppath);

  char* filename;
  int fd = -1;

  if (tmpname.GetMutableData(&filename)) {
      fd = mkstemp(filename);
      if (fd >= 0) {
          int flags = fcntl(fd, F_GETFD);
          if (flags >= 0) {
              fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
          }
      }
  }

  if (fd >= 0) {
      unlink(tmpname.get());
  } else {
      printf_stderr("Unable to create mapping file %s\n", filename);
      MOZ_CRASH();
  }

#ifdef HAVE_POSIX_FALLOCATE
  int ret = posix_fallocate(fd, 0, aSize);
#else
  int ret = ftruncate(fd, aSize);
#endif
  MOZ_RELEASE_ASSERT(ret == 0, "Mapping file allocation failed.");

  return fd;
}

WaylandShmPool::WaylandShmPool(int aSize)
{
  mAllocatedSize = aSize;

  mShmPoolFd = CreateTemporaryFile(mAllocatedSize);
  mImageData = mmap(nullptr, mAllocatedSize,
                     PROT_READ | PROT_WRITE, MAP_SHARED, mShmPoolFd, 0);
  MOZ_RELEASE_ASSERT(mImageData != MAP_FAILED,
                     "Unable to map drawing surface!");

  mShmPool = wl_shm_create_pool(gWaylandDisplay->GetShm(),
                                mShmPoolFd, mAllocatedSize);
  wl_proxy_set_queue((struct wl_proxy *)mShmPool,
                     gWaylandDisplay->GetEventQueue());
}

bool
WaylandShmPool::Resize(int aSize)
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

  munmap(mImageData, mAllocatedSize);

  mImageData = mmap(nullptr, aSize,
                     PROT_READ | PROT_WRITE, MAP_SHARED, mShmPoolFd, 0);
  if (mImageData == MAP_FAILED)
    return false;

  mAllocatedSize = aSize;
  return true;
}

WaylandShmPool::~WaylandShmPool()
{
  munmap(mImageData, mAllocatedSize);
  wl_shm_pool_destroy(mShmPool);
  close(mShmPoolFd);
}

WaylandDisplay::~WaylandDisplay()
{
}

ImageBuffer::ImageBuffer()
  : mImageData(nullptr)
  , mBufferAllocated(0)
  , mWidth(0)
  , mHeight(0)
{
}

ImageBuffer::~ImageBuffer()
{
  if (mImageData)
    free(mImageData);
}

already_AddRefed<gfx::DrawTarget>
ImageBuffer::Lock(const LayoutDeviceIntRegion& aRegion)
{
  gfx::IntRect bounds = aRegion.GetBounds().ToUnknownRect();
  gfx::IntSize imageSize(bounds.XMost(), bounds.YMost());

  // TODO - use widget bounds?
  // LayoutDeviceIntRect rect = mWidget->GetBounds();

  int newSize = imageSize.width * imageSize.height * BUFFER_BPP;
  if (!mImageData || mBufferAllocated < newSize) {
    if (mImageData) {
      free(mImageData);
    }

    mImageData = (unsigned char*)malloc(newSize);
    if (!mImageData)
      return nullptr;

    mBufferAllocated = newSize;
  }

  mWidth = imageSize.width;
  mHeight = imageSize.height;

  return gfxPlatform::CreateDrawTargetForData(mImageData, imageSize,
    BUFFER_BPP * mWidth, gWaylandDisplay->GetSurfaceFormat());
}

static void
buffer_release(void *data, wl_buffer *buffer)
{
  auto surface = reinterpret_cast<WindowBackBuffer*>(data);
  surface->Detach();
}

static const struct wl_buffer_listener buffer_listener = {
  buffer_release
};

void WindowBackBuffer::Create(int aWidth, int aHeight)
{
  MOZ_ASSERT(!IsAttached(), "We can't resize attached buffers.");

  int newBufferSize = aWidth*aHeight*BUFFER_BPP;
  mShmPool.Resize(newBufferSize);

  mWaylandBuffer = wl_shm_pool_create_buffer(mShmPool.GetShmPool(), 0,
                                            aWidth, aHeight, aWidth*BUFFER_BPP,
                                            WL_SHM_FORMAT_ARGB8888);
  wl_proxy_set_queue((struct wl_proxy *)mWaylandBuffer,
                     gWaylandDisplay->GetEventQueue());
  wl_buffer_add_listener(mWaylandBuffer, &buffer_listener, this);

  mWidth = aWidth;
  mHeight = aHeight;
}

void WindowBackBuffer::Release()
{
  wl_buffer_destroy(mWaylandBuffer);
  mWidth = mHeight = 0;
}

WindowBackBuffer::WindowBackBuffer(int aWidth, int aHeight)
 : mShmPool(aWidth*aHeight*BUFFER_BPP)
  ,mWaylandBuffer(nullptr)
  ,mWidth(aWidth)
  ,mHeight(aHeight)
  ,mAttached(false)
{
  Create(aWidth, aHeight);
}

WindowBackBuffer::~WindowBackBuffer()
{
  Release();
}

bool
WindowBackBuffer::Resize(int aWidth, int aHeight)
{
  if (aWidth == mWidth && aHeight == mHeight)
    return true;

  Release();
  Create(aWidth, aHeight);

  return (mWaylandBuffer != nullptr);
}

// Update back buffer with image data from ImageBuffer
void
WindowBackBuffer::CopyRectangle(ImageBuffer *aImage,
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
    memcpy((unsigned char *)mShmPool.GetImageData() + start,
            aImage->GetImageData() + ((y * aImage->GetWidth()) + r.x) * BUFFER_BPP,
            lenght);
  }
}

void
WindowBackBuffer::Attach(wl_surface* aSurface)
{
  // Taken from Hybris project:
  // Some compositors, namely Weston, queue buffer release events instead
  // of sending them immediately.  If a frame event is used, this should
  // not be a problem.  Without a frame event, we need to send a sync
  // request to ensure that they get flushed.
  //wl_callback_destroy(wl_display_sync(WindowSurfaceWayland::GetDisplay()));

  wl_surface_attach(aSurface, mWaylandBuffer, 0, 0);
  wl_surface_commit(aSurface);
  wl_display_flush(gWaylandDisplay->GetDisplay());
  mAttached = true;
}

void
WindowBackBuffer::Detach()
{
  mAttached = false;
}

bool WindowBackBuffer::Sync(class WindowBackBuffer* aSourceBuffer)
{
  bool bufferSizeMatches = MatchSize(aSourceBuffer);
  if (!bufferSizeMatches) {
    Resize(aSourceBuffer->mWidth, aSourceBuffer->mHeight);
  }

  memcpy(mShmPool.GetImageData(), aSourceBuffer->mShmPool.GetImageData(),
         aSourceBuffer->mWidth * aSourceBuffer->mHeight * BUFFER_BPP);
  return true;
}

static void
frame_callback_handler(void *data, struct wl_callback *callback, uint32_t time)
{
    auto surface = reinterpret_cast<WindowSurfaceWayland*>(data);
    surface->FrameCallbackHandler();
}

static const struct wl_callback_listener frame_listener = {
    frame_callback_handler
};

WindowSurfaceWayland::WindowSurfaceWayland(nsWindow *aWidget,
                                           wl_display *aDisplay,
                                           wl_surface *aSurface)
  : mWidget(aWidget)
  , mSurface(aSurface)
  , mFrontBuffer(nullptr)
  , mBackBuffer(nullptr)
  , mFrameCallback(nullptr)
  , mDelayedCommit(false)
  , mFullScreenDamage(false)
{
  MOZ_RELEASE_ASSERT(mSurface != nullptr,
                    "We can't do anything useful without valid wl_surface.");
  // Ensure we have valid display connection
  WaylandDisplayInit(aDisplay);

  // Make sure the drawing surface is handled by our event loop
  // and not the default (Gdk) one to draw out of main thread.
  wl_proxy_set_queue((struct wl_proxy *)mSurface,
                     gWaylandDisplay->GetEventQueue());
}

WindowSurfaceWayland::~WindowSurfaceWayland()
{
  delete mFrontBuffer;
  delete mBackBuffer;

  if (mFrameCallback) {
    wl_callback_destroy(mFrameCallback);
  }
}

WindowBackBuffer*
WindowSurfaceWayland::GetBufferToDraw(int aWidth, int aHeight)
{
  if (!mFrontBuffer) {
    mFrontBuffer = new WindowBackBuffer(aWidth, aHeight);
    mBackBuffer = new WindowBackBuffer(aWidth, aHeight);
    return mFrontBuffer;
  }

  if (!mFrontBuffer->IsAttached()) {
    if (!mFrontBuffer->MatchSize(aWidth, aHeight)) {
      mFrontBuffer->Resize(aWidth, aHeight);
    }
    return mFrontBuffer;
  }

  // Front buffer is used by compositor, draw to back buffer
  if (mBackBuffer->IsAttached()) {
    NS_WARNING("No drawing buffer available");
    return nullptr;
  }

  NS_ASSERTION(!mDelayedCommit,
               "Uncommitted buffer switch, screen artifacts ahead.");

  WindowBackBuffer *tmp = mFrontBuffer;
  mFrontBuffer = mBackBuffer;
  mBackBuffer = tmp;

  if (mBackBuffer->MatchSize(aWidth, aHeight)) {
    // Former front buffer has the same size as a requested one.
    // Gecko may expect a content already drawn on screen so copy
    // existing data to the new buffer.
    mFrontBuffer->Sync(mBackBuffer);
    // When buffer switches we need to damage whole screen
    // (https://bugzilla.redhat.com/show_bug.cgi?id=1418260)
    mFullScreenDamage = true;
  } else {
    // Former buffer has different size from the new request. Only resize
    // the new buffer and leave geck to render new whole content.
    mFrontBuffer->Resize(aWidth, aHeight);
  }

  return mFrontBuffer;
}

already_AddRefed<gfx::DrawTarget>
WindowSurfaceWayland::Lock(const LayoutDeviceIntRegion& aRegion)
{
  // We can use backbuffer directly when:
  // 1) Front buffer is not used by compositor
  // 2) We're asked for full screen area
  // 3) No pre/after drawing - resize drop fullscreen, delete fullscreen flag between comits
/*
  if (!mFrontBuffer) {
    LayoutDeviceIntRect rect = mWidget->GetBounds();
    (void)GetBufferToDraw(rect.width, rect.height);
  }

  if (!mFrontBuffer->IsAttached()) {
    LayoutDeviceIntRect rect = mWidget->GetBounds();
    mFullScreen = false;
    for (auto iter = aRegion.RectIter(); !iter.Done(); iter.Next()) {
      const mozilla::LayoutDeviceIntRect &r = iter.Get();
      if (r.x == 0 && r.y == 0 &&
          r.width == rect.width && r.height == rect.height) {
        fprintf(stderr, "************* Fulscreen %d x %d\n", r.width, r.height);
        mFullScreen = true;
        break;
      }
    }
  }
*/
  // TODO -> compare with bound size
  // and provide back buffer direcly when possible
  // (no data in img buffer and size match)
  // and we also don't need to switch buffers
  return mImageBuffer.Lock(aRegion);
}

void
WindowSurfaceWayland::Commit(const LayoutDeviceIntRegion& aInvalidRegion)
{
  LayoutDeviceIntRect rect = mWidget->GetBounds();
  WindowBackBuffer* buffer = GetBufferToDraw(rect.width,
                                             rect.height);
  NS_ASSERTION(buffer, "We don't have any buffer to draw to!");
  if (!buffer) {
    return;
  }

  // TODO - Do we want to fix redundat copy of overlapping areas?
  for (auto iter = aInvalidRegion.RectIter(); !iter.Done(); iter.Next()) {
    const mozilla::LayoutDeviceIntRect &r = iter.Get();
    buffer->CopyRectangle(&mImageBuffer, r);
    if (!mFullScreenDamage)
      wl_surface_damage(mSurface, r.x, r.y, r.width, r.height);
  }

  if (mFullScreenDamage) {
    wl_surface_damage(mSurface, 0, 0, rect.width, rect.height);
    mFullScreenDamage = false;
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
WindowSurfaceWayland::FrameCallbackHandler()
{
  if (mFrameCallback) {
      wl_callback_destroy(mFrameCallback);
      mFrameCallback = nullptr;
  }

  if (mDelayedCommit) {
    // Send pending surface to compositor and register frame callback
    // for possible subsequent drawing.
    mFrameCallback = wl_surface_frame(mSurface);
    wl_callback_add_listener(mFrameCallback, &frame_listener, this);

    mFrontBuffer->Attach(mSurface);
    mDelayedCommit = false;
  }
}

}  // namespace widget
}  // namespace mozilla
