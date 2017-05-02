/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

static nsWaylandDisplay* gWaylandDisplay = nullptr;

static void
WaylandDisplayAddRef(wl_display *aDisplay)
{
  // We should run in Compositor thread
  MOZ_ASSERT(!NS_IsMainThread());
  if (!gWaylandDisplay) {
    gWaylandDisplay = new nsWaylandDisplay(aDisplay);
  } else {
    MOZ_ASSERT(gWaylandDisplay->GetDisplay() == aDisplay,
               "Unknown Wayland display!");
  }
  NS_ADDREF(gWaylandDisplay);
}

static void
WaylandDisplayRelease(void *aUnused)
{
  MOZ_ASSERT(!NS_IsMainThread());
  NS_IF_RELEASE(gWaylandDisplay);
}

static void
WaylandDisplayLoop(void *tmp)
{
  MOZ_ASSERT(!NS_IsMainThread());

  // Check we still have the display interface
  if (gWaylandDisplay && gWaylandDisplay->DisplayLoop()) {
    MessageLoop::current()->PostTask(
        NewRunnableFunction(&WaylandDisplayLoop, nullptr));
  }
}

void
nsWaylandDisplay::SetWaylandPixelFormat(uint32_t format)
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
  auto interface = reinterpret_cast<nsWaylandDisplay *>(data);
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
    auto interface = reinterpret_cast<nsWaylandDisplay *>(data);
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

bool
nsWaylandDisplay::DisplayLoop()
{
  /* NoteThis function may dispatch other events being received on the given
     queue. This function uses wl_display_dispatch_queue() internally.
     If you are using wl_display_read_events() from more threads,
     don't use this function (or make sure that calling wl_display_roundtrip_queue()
     doesn't interfere with calling wl_display_prepare_read() and
     wl_display_read_events()).
  */
  return wl_display_roundtrip_queue(mDisplay, mEventQueue) != -1;
}

NS_IMPL_ISUPPORTS(nsWaylandDisplay, nsISupports);

nsWaylandDisplay::nsWaylandDisplay(wl_display *aDisplay)
  : mDisplay(aDisplay)
{
  // We're supposed to run in Compositor thread
  MOZ_ASSERT(!NS_IsMainThread());

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

  // TODO - is that correct way how to run wayland event pump?
  MessageLoop::current()->PostTask(NewRunnableFunction(&WaylandDisplayLoop, nullptr));
}

nsWaylandDisplay::~nsWaylandDisplay()
{
  MOZ_ASSERT(!NS_IsMainThread());
  wl_event_queue_destroy(mEventQueue);
  mEventQueue = nullptr;
  mDisplay = nullptr;
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

WaylandShmPool::WaylandShmPool(bool aIsMainThread, int aSize)
{
  mAllocatedSize = aSize;

  mShmPoolFd = CreateTemporaryFile(mAllocatedSize);
  mImageData = mmap(nullptr, mAllocatedSize,
                     PROT_READ | PROT_WRITE, MAP_SHARED, mShmPoolFd, 0);
  MOZ_RELEASE_ASSERT(mImageData != MAP_FAILED,
                     "Unable to map drawing surface!");

  mShmPool = wl_shm_create_pool(gWaylandDisplay->GetShm(),
                                mShmPoolFd, mAllocatedSize);
  if (!aIsMainThread) {
    wl_proxy_set_queue((struct wl_proxy *)mShmPool,
                      gWaylandDisplay->GetEventQueue());
  }
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
  if (!mIsMainThread) {
    wl_proxy_set_queue((struct wl_proxy *)mWaylandBuffer,
                      gWaylandDisplay->GetEventQueue());
  }
  wl_buffer_add_listener(mWaylandBuffer, &buffer_listener, this);

  mWidth = aWidth;
  mHeight = aHeight;
}

void WindowBackBuffer::Release()
{
  wl_buffer_destroy(mWaylandBuffer);
  mWidth = mHeight = 0;
}

WindowBackBuffer::WindowBackBuffer(bool aIsMainThread, int aWidth, int aHeight)
 : mShmPool(aIsMainThread, aWidth*aHeight*BUFFER_BPP)
  ,mWaylandBuffer(nullptr)
  ,mWidth(aWidth)
  ,mHeight(aHeight)
  ,mAttached(false)
  ,mIsMainThread(aIsMainThread)
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

void
WindowBackBuffer::Attach(wl_surface* aSurface)
{
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

already_AddRefed<gfx::DrawTarget>
WindowBackBuffer::Lock(const LayoutDeviceIntRegion& aRegion)
{
  gfx::IntRect bounds = aRegion.GetBounds().ToUnknownRect();
  gfx::IntSize lockSize(bounds.XMost(), bounds.YMost());

  return gfxPlatform::CreateDrawTargetForData(static_cast<unsigned char*>(mShmPool.GetImageData()),
                                              lockSize,
                                              BUFFER_BPP * mWidth,
                                              gWaylandDisplay->GetSurfaceFormat());
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
  , mWaylandMessageLoop(nullptr)
  , mIsMainThread(NS_IsMainThread())
{
  MOZ_RELEASE_ASSERT(mSurface != nullptr,
                    "We can't do anything useful without valid wl_surface.");

  if (!mIsMainThread) {
    // Register and run wayland loop when running in compositor thread.
    mWaylandMessageLoop = MessageLoop::current();
    WaylandDisplayAddRef(aDisplay);
    wl_proxy_set_queue((struct wl_proxy *)mSurface,
                       gWaylandDisplay->GetEventQueue());
  }
}

WindowSurfaceWayland::~WindowSurfaceWayland()
{
  delete mFrontBuffer;
  delete mBackBuffer;

  if (mFrameCallback) {
    wl_callback_destroy(mFrameCallback);
  }

  if (!mIsMainThread) {
    // Release WaylandDisplay only for surfaces created in Compositor thread.
    mWaylandMessageLoop->PostTask(
      NewRunnableFunction(&WaylandDisplayRelease, nullptr));
  }
}

WindowBackBuffer*
WindowSurfaceWayland::GetBufferToDraw(int aWidth, int aHeight)
{
  if (!mFrontBuffer) {
    mFrontBuffer = new WindowBackBuffer(mIsMainThread, aWidth, aHeight);
    mBackBuffer = new WindowBackBuffer(mIsMainThread, aWidth, aHeight);
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

  MOZ_ASSERT(!mDelayedCommit,
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
  MOZ_ASSERT(mIsMainThread == NS_IsMainThread());

  // We allocate back buffer to widget size but return only
  // portion requested by aRegion.
  LayoutDeviceIntRect rect = mWidget->GetBounds();
  WindowBackBuffer* buffer = GetBufferToDraw(rect.width,
                                             rect.height);
  MOZ_ASSERT(buffer, "We don't have any buffer to draw to!");
  if (!buffer) {
    return nullptr;
  }

  return buffer->Lock(aRegion);
}

void
WindowSurfaceWayland::Commit(const LayoutDeviceIntRegion& aInvalidRegion)
{
  MOZ_ASSERT(mIsMainThread == NS_IsMainThread());

  for (auto iter = aInvalidRegion.RectIter(); !iter.Done(); iter.Next()) {
    const mozilla::LayoutDeviceIntRect &r = iter.Get();
    if (!mFullScreenDamage)
      wl_surface_damage(mSurface, r.x, r.y, r.width, r.height);
  }

  if (mFullScreenDamage) {
    LayoutDeviceIntRect rect = mWidget->GetBounds();
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
  MOZ_ASSERT(mIsMainThread == NS_IsMainThread());

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
