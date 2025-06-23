/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WaylandSurface.h"
#include "WaylandBuffer.h"
#include <wayland-egl.h>
#include "nsGtkUtils.h"
#include "mozilla/StaticPrefs_widget.h"
#include <dlfcn.h>
#include <fcntl.h>
#include "ScreenHelperGTK.h"
#include "DMABufFormats.h"
#include "mozilla/gfx/gfxVars.h"

#undef LOG
#ifdef MOZ_LOGGING
#  include "mozilla/Logging.h"
#  include "nsTArray.h"
#  include "Units.h"
#  include "nsWindow.h"
#  undef LOGWAYLAND
#  undef LOGVERBOSE
extern mozilla::LazyLogModule gWidgetWaylandLog;
extern mozilla::LazyLogModule gWidgetLog;
#  define LOGWAYLAND(str, ...)                           \
    MOZ_LOG(gWidgetWaylandLog, mozilla::LogLevel::Debug, \
            ("%s: " str, GetDebugTag().get(), ##__VA_ARGS__))
#  define LOGVERBOSE(str, ...)                             \
    MOZ_LOG(gWidgetWaylandLog, mozilla::LogLevel::Verbose, \
            ("%s: " str, GetDebugTag().get(), ##__VA_ARGS__))
#  define LOGS(...) \
    MOZ_LOG(gWidgetWaylandLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
#  define LOGS_VERBOSE(...) \
    MOZ_LOG(gWidgetWaylandLog, mozilla::LogLevel::Verbose, (__VA_ARGS__))
#else
#  define LOGWAYLAND(...)
#  undef LOGVERBOSE
#  define LOGVERBOSE(...)
#  define LOGS(...)
#  define LOGS_VERBOSE(...)
#endif /* MOZ_LOGGING */

using namespace mozilla;
using namespace mozilla::widget;

namespace mozilla::widget {

#ifdef MOZ_LOGGING
nsAutoCString WaylandSurface::GetDebugTag() const {
  nsAutoCString tag;
  tag.AppendPrintf("[%p]", mLoggingWidget);
  return tag;
}
#endif

void (*WaylandSurface::sGdkWaylandWindowAddCallbackSurface)(
    GdkWindow*, struct wl_surface*) = nullptr;
void (*WaylandSurface::sGdkWaylandWindowRemoveCallbackSurface)(
    GdkWindow*, struct wl_surface*) = nullptr;

bool WaylandSurface::IsOpaqueRegionEnabled() {
  static bool sIsOpaqueRegionEnabled = []() {
    if (!StaticPrefs::widget_wayland_opaque_region_enabled_AtStartup()) {
      return false;
    }
    sGdkWaylandWindowAddCallbackSurface =
        reinterpret_cast<void (*)(GdkWindow*, struct wl_surface*)>(dlsym(
            RTLD_DEFAULT, "gdk_wayland_window_add_frame_callback_surface"));
    sGdkWaylandWindowRemoveCallbackSurface =
        reinterpret_cast<void (*)(GdkWindow*, struct wl_surface*)>(dlsym(
            RTLD_DEFAULT, "gdk_wayland_window_remove_frame_callback_surface"));
    return sGdkWaylandWindowAddCallbackSurface &&
           sGdkWaylandWindowRemoveCallbackSurface;
  }();
  return sIsOpaqueRegionEnabled;
}

WaylandSurface::WaylandSurface(RefPtr<WaylandSurface> aParent,
                               gfx::IntSize aSize)
    : mSizeScaled(aSize), mParent(aParent) {
  LOGWAYLAND("WaylandSurface::WaylandSurface(), parent [%p] size [%d x %d]",
             mParent ? mParent->GetLoggingWidget() : nullptr, mSizeScaled.width,
             mSizeScaled.height);
}

WaylandSurface::~WaylandSurface() {
  LOGWAYLAND("WaylandSurface::~WaylandSurface()");
  MOZ_RELEASE_ASSERT(!mIsMapped, "We can't release mapped WaylandSurface!");
  MOZ_RELEASE_ASSERT(!mSurfaceLock, "We can't release locked WaylandSurface!");
  MOZ_RELEASE_ASSERT(mAttachedBuffers.Length() == 0,
                     "We can't release surface with buffers tracked!");
  MOZ_RELEASE_ASSERT(!mEmulatedFrameCallbackTimerID,
                     "We can't release WaylandSurface with active timer");
  MOZ_RELEASE_ASSERT(!mIsPendingGdkCleanup,
                     "We can't release WaylandSurface with Gdk resources!");
  MOZ_RELEASE_ASSERT(
      !mDMABufFormatRefreshCallback,
      "We can't release WaylandSurface with DMABufFormatRefreshCallback!");
  MOZ_RELEASE_ASSERT(!mGdkCommitCallback,
                     "We can't release WaylandSurface with GdkCommitCallback!");
  MOZ_RELEASE_ASSERT(!mUnmapCallback,
                     "We can't release WaylandSurface with numap callback!");
}

void WaylandSurface::ReadyToDrawFrameCallbackHandler(
    struct wl_callback* callback) {
  LOGWAYLAND(
      "WaylandSurface::ReadyToDrawFrameCallbackHandler() "
      "mReadyToDrawFrameCallback %p mIsReadyToDraw %d initial_draw callback "
      "%zd\n",
      (void*)mReadyToDrawFrameCallback, (bool)mIsReadyToDraw,
      mReadyToDrawCallbacks.size());

  // We're supposed to run on main thread only.
  AssertIsOnMainThread();

  // mReadyToDrawFrameCallback/callback can be nullptr when redering directly
  // to GtkWidget and ReadyToDrawFrameCallbackHandler is called by us from main
  // thread by WaylandSurface::Map().
  MOZ_RELEASE_ASSERT(mReadyToDrawFrameCallback == callback);

  std::vector<std::function<void(void)>> cbs;
  {
    WaylandSurfaceLock lock(this);
    MozClearPointer(mReadyToDrawFrameCallback, wl_callback_destroy);
    // It's possible that we're already unmapped so quit in such case.
    if (!mSurface) {
      LOGWAYLAND("  WaylandSurface is unmapped, quit.");
      if (!mReadyToDrawCallbacks.empty()) {
        NS_WARNING("Unmapping WaylandSurface with active draw callback!");
        mReadyToDrawCallbacks.clear();
      }
      return;
    }
    if (mIsReadyToDraw) {
      return;
    }
    mIsReadyToDraw = true;
    cbs = std::move(mReadyToDrawCallbacks);

    RequestFrameCallbackLocked(lock);
  }

  // We can't call the callbacks under lock
#ifdef MOZ_LOGGING
  int callbackNum = 0;
#endif
  for (auto const& cb : cbs) {
    LOGWAYLAND("  initial callback fire  [%d]", callbackNum++);
    cb();
  }
}

static void ReadyToDrawFrameCallbackHandler(void* aWaylandSurface,
                                            struct wl_callback* callback,
                                            uint32_t time) {
  auto* waylandSurface = static_cast<WaylandSurface*>(aWaylandSurface);
  waylandSurface->ReadyToDrawFrameCallbackHandler(callback);
}

static const struct wl_callback_listener
    sWaylandSurfaceReadyToDrawFrameListener = {
        ::ReadyToDrawFrameCallbackHandler};

void WaylandSurface::AddReadyToDrawCallbackLocked(
    const WaylandSurfaceLock& aProofOfLock,
    const std::function<void(void)>& aDrawCB) {
  LOGVERBOSE("WaylandSurface::AddReadyToDrawCallbackLocked()");
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);

  if (mIsReadyToDraw && !mSurface) {
    NS_WARNING(
        "WaylandSurface::AddReadyToDrawCallbackLocked():"
        " ready to draw without wayland surface!");
  }
  MOZ_DIAGNOSTIC_ASSERT(!mIsReadyToDraw || !mSurface);
  mReadyToDrawCallbacks.push_back(aDrawCB);
}

void WaylandSurface::AddOrFireReadyToDrawCallback(
    const std::function<void(void)>& aDrawCB) {
  {
    WaylandSurfaceLock lock(this);
    if (mIsReadyToDraw && !mSurface) {
      NS_WARNING(
          "WaylandSurface::AddOrFireReadyToDrawCallback(): ready to draw "
          "without wayland surface!");
    }
    if (!mIsReadyToDraw || !mSurface) {
      LOGVERBOSE(
          "WaylandSurface::AddOrFireReadyToDrawCallback() callback stored");
      mReadyToDrawCallbacks.push_back(aDrawCB);
      return;
    }
  }

  LOGWAYLAND("WaylandSurface::AddOrFireReadyToDrawCallback() callback fire");

  // We're ready to draw and we have a surface to draw into.
  aDrawCB();
}

void WaylandSurface::ClearReadyToDrawCallbacksLocked(
    const WaylandSurfaceLock& aProofOfLock) {
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);
  MozClearPointer(mReadyToDrawFrameCallback, wl_callback_destroy);
  mReadyToDrawCallbacks.clear();
}

void WaylandSurface::ClearReadyToDrawCallbacks() {
  WaylandSurfaceLock lock(this);
  ClearReadyToDrawCallbacksLocked(lock);
}

bool WaylandSurface::HasEmulatedFrameCallbackLocked(
    const WaylandSurfaceLock& aProofOfLock) const {
  return mFrameCallbackHandler.IsSet() && mFrameCallbackHandler.mEmulated;
}

void WaylandSurface::FrameCallbackHandler(struct wl_callback* aCallback,
                                          uint32_t aTime,
                                          bool aRoutedFromChildSurface) {
  // We're supposed to run on main thread only.
  AssertIsOnMainThread();

  bool emulatedCallback = !aCallback && !aTime;

  FrameCallback cb;
  {
    WaylandSurfaceLock lock(this);

    // Don't run emulated callbacks on hidden surfaces
    if ((emulatedCallback || aRoutedFromChildSurface) && !mIsReadyToDraw) {
      return;
    }

    LOGVERBOSE(
        "WaylandSurface::FrameCallbackHandler() "
        "set %d emulated %d routed %d",
        mFrameCallbackHandler.IsSet(), emulatedCallback,
        aRoutedFromChildSurface);

    // It's possible to get regular frame callback right after unmap
    // if frame callbacks was already in event queue so ignore it.
    if (!emulatedCallback && !aRoutedFromChildSurface && !mFrameCallback) {
      MOZ_DIAGNOSTIC_ASSERT(!mIsMapped);
      return;
    }

    MOZ_DIAGNOSTIC_ASSERT(aCallback == nullptr || mFrameCallback == aCallback);

    if (aCallback) {
      ClearFrameCallbackLocked(lock);
    }

    // We're getting regular frame callback from this surface so we must
    // have buffer attached.
    if (!emulatedCallback && !aRoutedFromChildSurface) {
      mBufferAttached = true;
    }

    cb = mFrameCallbackHandler;

    // Fire frame callback again if there's any pending frame callback
    RequestFrameCallbackLocked(lock);
  }

  // We can't run the callbacks under WaylandSurfaceLock
  LOGVERBOSE("  frame callback fire");
  if (emulatedCallback && !cb.mEmulated) {
    return;
  }
  if (cb.IsSet()) {
    cb.mCb(aCallback, aTime);
  }
}

static void FrameCallbackHandler(void* aWaylandSurface,
                                 struct wl_callback* aCallback,
                                 uint32_t aTime) {
  RefPtr waylandSurface = static_cast<WaylandSurface*>(aWaylandSurface);
  waylandSurface->FrameCallbackHandler(aCallback, aTime,
                                       /* aRoutedFromChildSurface */ false);
}

static const struct wl_callback_listener sWaylandSurfaceFrameListener = {
    ::FrameCallbackHandler};

void WaylandSurface::RequestFrameCallbackLocked(
    const WaylandSurfaceLock& aProofOfLock) {
  LOGVERBOSE(
      "WaylandSurface::RequestFrameCallbackLocked(), enabled %d mapped %d "
      " mFrameCallback %d",
      mFrameCallbackEnabled, !!mIsMapped, !!mFrameCallback);

  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);

  // Frame callback will be added by Map.
  if (!mIsMapped || !mFrameCallbackEnabled || !mFrameCallbackHandler.IsSet()) {
    return;
  }

  MOZ_DIAGNOSTIC_ASSERT(mSurface, "Missing mapped surface!");

  if (!mFrameCallback) {
    mFrameCallback = wl_surface_frame(mSurface);
    wl_callback_add_listener(mFrameCallback, &sWaylandSurfaceFrameListener,
                             this);
    mSurfaceNeedsCommit = true;
  }

  // Request frame callback emulation if:
  // - we have registered any emulated frame callbacks
  // - we don't have buffer attached so we can't get regular frame callback
  // - emulated frame callback is not already pending
  if (HasEmulatedFrameCallbackLocked(aProofOfLock) && !mBufferAttached &&
      !mEmulatedFrameCallbackTimerID) {
    LOGVERBOSE(
        "WaylandSurface::RequestFrameCallbackLocked() emulated, schedule "
        "next check");
    // Frame callback needs to be run from main thread
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "WaylandSurface::RequestFrameCallbackLocked",
        [this, self = RefPtr{this}]() {
          MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
          WaylandSurfaceLock lock(this);
          if (mIsMapped && !mEmulatedFrameCallbackTimerID) {
            mIsPendingGdkCleanup = true;
            mEmulatedFrameCallbackTimerID = g_timeout_add(
                sEmulatedFrameCallbackTimeoutMs,
                [](void* data) -> gint {
                  RefPtr surface = static_cast<WaylandSurface*>(data);
                  LOGS_VERBOSE("[%p]: WaylandSurface emulated frame callbacks",
                               surface->GetLoggingWidget());
                  // Clear timer ID as we're going to remove this timer
                  surface->mEmulatedFrameCallbackTimerID = 0;

                  if (!surface->mGdkAfterPaintId &&
                      !surface->mIsOpaqueSurfaceHandlerSet) {
                    surface->mIsPendingGdkCleanup = false;
                  }

                  surface->FrameCallbackHandler(
                      nullptr, 0, /* aRoutedFromChildSurface */ false);
                  return G_SOURCE_REMOVE;
                },
                this);
          }
        }));
  }
}

void WaylandSurface::ClearFrameCallbackLocked(
    const WaylandSurfaceLock& aProofOfLock) {
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);
  MozClearPointer(mFrameCallback, wl_callback_destroy);
}

void WaylandSurface::SetFrameCallbackLocked(
    const WaylandSurfaceLock& aProofOfLock,
    const std::function<void(wl_callback*, uint32_t)>& aFrameCallbackHandler,
    bool aEmulateFrameCallback) {
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);
  MOZ_DIAGNOSTIC_ASSERT(!mFrameCallbackHandler.IsSet());

  LOGWAYLAND("WaylandSurface::SetFrameCallbackLocked()");

  mFrameCallbackHandler =
      FrameCallback{aFrameCallbackHandler, aEmulateFrameCallback};
  RequestFrameCallbackLocked(aProofOfLock);
}

void WaylandSurface::SetFrameCallbackStateLocked(
    const WaylandSurfaceLock& aProofOfLock, bool aEnabled) {
  LOGWAYLAND("WaylandSurface::SetFrameCallbackState() state %d", aEnabled);
  if (mFrameCallbackEnabled == aEnabled) {
    return;
  }
  mFrameCallbackEnabled = aEnabled;

  // If there's any frame callback waiting, register the handler.
  if (mFrameCallbackEnabled) {
    RequestFrameCallbackLocked(aProofOfLock);
  } else {
    ClearFrameCallbackLocked(aProofOfLock);
  }
  if (mFrameCallbackStateHandler) {
    mFrameCallbackStateHandler(aEnabled);
  }
}

void WaylandSurface::SetFrameCallbackStateHandlerLocked(
    const WaylandSurfaceLock& aProofOfLock,
    const std::function<void(bool)>& aFrameCallbackStateHandler) {
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);
  mFrameCallbackStateHandler = aFrameCallbackStateHandler;
}

bool WaylandSurface::CreateViewportLocked(
    const WaylandSurfaceLock& aProofOfLock, bool aFollowsSizeChanges) {
  LOGWAYLAND("WaylandSurface::CreateViewportLocked() follow size %d",
             aFollowsSizeChanges);
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);
  MOZ_DIAGNOSTIC_ASSERT(mIsMapped);
  MOZ_DIAGNOSTIC_ASSERT(!mViewport);

  auto* viewPorter = WaylandDisplayGet()->GetViewporter();
  if (viewPorter) {
    mViewport = wp_viewporter_get_viewport(viewPorter, mSurface);
  }
  if (!mViewport) {
    LOGWAYLAND(
        "WaylandSurface::CreateViewportLocked(): Failed to get "
        "WaylandViewport!");
    return false;
  }
  mSurfaceNeedsCommit = true;
  mViewportFollowsSizeChanges = aFollowsSizeChanges;
  return !!mViewport;
}

void WaylandSurface::EnableDMABufFormatsLocked(
    const WaylandSurfaceLock& aProofOfLock,
    const std::function<void(DMABufFormats*)>& aFormatRefreshCB) {
  // Ignore DMABuf feedback requests if we export dmabuf surfaces
  // directly from EGLImage.
  if (gfx::gfxVars::UseDMABufSurfaceExport()) {
    return;
  }

  mUseDMABufFormats = true;
  mDMABufFormatRefreshCallback = aFormatRefreshCB;

  // We'll set up on Map
  if (!mIsMapped) {
    return;
  }

  mFormats = CreateDMABufFeedbackFormats(mSurface, aFormatRefreshCB);
  if (!mFormats) {
    LOGWAYLAND(
        "WaylandSurface::SetDMABufFormatsLocked(): Failed to get DMABuf "
        "formats!");
  }
}

void WaylandSurface::DisableDMABufFormatsLocked(
    const WaylandSurfaceLock& aProofOfLock) {
  mUseDMABufFormats = false;
  mDMABufFormatRefreshCallback = nullptr;
  mFormats = nullptr;
}

bool WaylandSurface::MapLocked(const WaylandSurfaceLock& aProofOfLock,
                               wl_surface* aParentWLSurface,
                               WaylandSurfaceLock* aParentWaylandSurfaceLock,
                               gfx::IntPoint aSubsurfacePosition,
                               bool aSubsurfaceDesync,
                               bool aUseReadyToDrawCallback) {
  LOGWAYLAND("WaylandSurface::MapLocked()");
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);
  MOZ_DIAGNOSTIC_ASSERT(!mIsMapped, "Already mapped?");
  MOZ_DIAGNOSTIC_ASSERT(!(aParentWLSurface && aParentWaylandSurfaceLock),
                        "Only one parent can be used.");
  MOZ_DIAGNOSTIC_ASSERT(!mSurface && !mSubsurface, "Already mapped?");

  if (aParentWLSurface) {
    mParentSurface = aParentWLSurface;
  } else {
    MOZ_DIAGNOSTIC_ASSERT(!mParentSurface, "Already mapped?");
    mParent = aParentWaylandSurfaceLock->GetWaylandSurface();
    MOZ_DIAGNOSTIC_ASSERT(mParent->IsMapped(), "Parent surface is not mapped?");
    mParentSurface = mParent->mSurface;
  }

  mSubsurfacePosition = aSubsurfacePosition;

  // Created wl_surface is without buffer attached
  mBufferAttached = false;
  struct wl_compositor* compositor = WaylandDisplayGet()->GetCompositor();
  mSurface = wl_compositor_create_surface(compositor);
  if (!mSurface) {
    LOGWAYLAND("    Failed - can't create surface!");
    return false;
  }

  mSubsurface = wl_subcompositor_get_subsurface(
      WaylandDisplayGet()->GetSubcompositor(), mSurface, mParentSurface);
  if (!mSubsurface) {
    MozClearPointer(mSurface, wl_surface_destroy);
    LOGWAYLAND("    Failed - can't create sub-surface!");
    return false;
  }
  if (aSubsurfaceDesync) {
    wl_subsurface_set_desync(mSubsurface);
  }
  wl_subsurface_set_position(mSubsurface, mSubsurfacePosition.x,
                             mSubsurfacePosition.y);

  if (aUseReadyToDrawCallback) {
    mReadyToDrawFrameCallback = wl_surface_frame(mParentSurface);
    wl_callback_add_listener(mReadyToDrawFrameCallback,
                             &sWaylandSurfaceReadyToDrawFrameListener, this);
    LOGWAYLAND("    created ready to draw frame callback ID %d\n",
               wl_proxy_get_id((struct wl_proxy*)mReadyToDrawFrameCallback));
  }

  LOGWAYLAND("  register frame callback");
  RequestFrameCallbackLocked(aProofOfLock);

  CommitLocked(aProofOfLock, /* aForceCommit */ true,
               /* aForceDisplayFlush */ true);

  mIsMapped = true;

  if (mUseDMABufFormats) {
    EnableDMABufFormatsLocked(aProofOfLock, mDMABufFormatRefreshCallback);
  }

  LOGWAYLAND("    created surface %p ID %d", (void*)mSurface,
             wl_proxy_get_id((struct wl_proxy*)mSurface));
  return true;
}

bool WaylandSurface::MapLocked(const WaylandSurfaceLock& aProofOfLock,
                               wl_surface* aParentWLSurface,
                               gfx::IntPoint aSubsurfacePosition) {
  return MapLocked(aProofOfLock, aParentWLSurface, nullptr, aSubsurfacePosition,
                   /* aSubsurfaceDesync */ true);
}

bool WaylandSurface::MapLocked(const WaylandSurfaceLock& aProofOfLock,
                               WaylandSurfaceLock* aParentWaylandSurfaceLock,
                               gfx::IntPoint aSubsurfacePosition) {
  return MapLocked(aProofOfLock, nullptr, aParentWaylandSurfaceLock,
                   aSubsurfacePosition,
                   /* aSubsurfaceDesync */ false,
                   /* aUseReadyToDrawCallback */ false);
}

void WaylandSurface::SetUnmapCallbackLocked(
    const WaylandSurfaceLock& aProofOfLock,
    const std::function<void(void)>& aUnmapCB) {
  mUnmapCallback = aUnmapCB;
}

void WaylandSurface::ClearUnmapCallbackLocked(
    const WaylandSurfaceLock& aProofOfLock) {
  mUnmapCallback = nullptr;
}

void WaylandSurface::RunUnmapCallback() {
  AssertIsOnMainThread();
  MOZ_DIAGNOSTIC_ASSERT(
      mIsMapped, "RunUnmapCallback is supposed to run before surface unmap!");
  if (mUnmapCallback) {
    mUnmapCallback();
  }
}

void WaylandSurface::GdkCleanUpLocked(const WaylandSurfaceLock& aProofOfLock) {
  LOGWAYLAND("WaylandSurface::GdkCleanUp()");
  AssertIsOnMainThread();
  MOZ_DIAGNOSTIC_ASSERT(mSurface);
  if (mGdkWindow) {
    RemoveOpaqueSurfaceHandlerLocked(aProofOfLock);
    mGdkWindow = nullptr;
  }
  MozClearHandleID(mEmulatedFrameCallbackTimerID, g_source_remove);

  mIsPendingGdkCleanup = false;
  if (!mIsMapped) {
    MozClearPointer(mSurface, wl_surface_destroy);
  }
}

void WaylandSurface::UnmapLocked(WaylandSurfaceLock& aSurfaceLock) {
  if (!mIsMapped) {
    return;
  }
  mIsMapped = false;

  LOGWAYLAND("WaylandSurface::UnmapLocked()");

  ClearReadyToDrawCallbacksLocked(aSurfaceLock);
  ClearFrameCallbackLocked(aSurfaceLock);
  ClearScaleLocked(aSurfaceLock);

  MozClearPointer(mViewport, wp_viewport_destroy);
  mViewportDestinationSize = gfx::IntSize(-1, -1);
  mViewportSourceRect = gfx::Rect(-1, -1, -1, -1);

  wl_egl_window* tmp = nullptr;
  mEGLWindow.exchange(tmp);
  MozClearPointer(tmp, wl_egl_window_destroy);
  MozClearPointer(mFractionalScaleListener, wp_fractional_scale_v1_destroy);
  MozClearPointer(mSubsurface, wl_subsurface_destroy);
  MozClearPointer(mColorSurface, wp_color_management_surface_v1_destroy);
  MozClearPointer(mImageDescription, wp_image_description_v1_destroy);
  mFrameCallbackHandler.Clear();
  mParentSurface = nullptr;
  mFormats = nullptr;

  // We can't release mSurface if it's used by Gdk for frame callback routing.
  if (!mIsPendingGdkCleanup) {
    MozClearPointer(mSurface, wl_surface_destroy);
  }

  mIsReadyToDraw = false;
  mBufferAttached = false;

  // Remove references to WaylandBuffers attached to mSurface,
  // we don't want to get any buffer release callback when we're unmapped.
  ReleaseAllWaylandBuffersLocked(aSurfaceLock);
}

void WaylandSurface::Commit(WaylandSurfaceLock* aProofOfLock, bool aForceCommit,
                            bool aForceDisplayFlush) {
  MOZ_DIAGNOSTIC_ASSERT(aProofOfLock == mSurfaceLock);

  // mSurface may be already deleted, see WaylandSurface::Unmap();
  if (mSurface && (aForceCommit || mSurfaceNeedsCommit)) {
    LOGVERBOSE(
        "WaylandSurface::Commit() needs commit %d, force commit %d flush %d",
        mSurfaceNeedsCommit, aForceCommit, aForceDisplayFlush);
    mSurfaceNeedsCommit = false;
    wl_surface_commit(mSurface);
    if (aForceDisplayFlush) {
      wl_display_flush(WaylandDisplayGet()->GetDisplay());
    }
  }
}

void WaylandSurface::CommitLocked(const WaylandSurfaceLock& aProofOfLock,
                                  bool aForceCommit, bool aForceDisplayFlush) {
  Commit((WaylandSurfaceLock*)&aProofOfLock, aForceCommit, aForceDisplayFlush);
}

void WaylandSurface::MoveLocked(const WaylandSurfaceLock& aProofOfLock,
                                gfx::IntPoint aPosition) {
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);
  MOZ_DIAGNOSTIC_ASSERT(mIsMapped);

  if (mSubsurfacePosition == aPosition) {
    return;
  }

  MOZ_DIAGNOSTIC_ASSERT(mSubsurface);
  LOGWAYLAND("WaylandSurface::MoveLocked() [%d,%d]", (int)aPosition.x,
             (int)aPosition.y);
  mSubsurfacePosition = aPosition;
  wl_subsurface_set_position(mSubsurface, aPosition.x, aPosition.y);
  mSurfaceNeedsCommit = true;
}

// Route input to parent wl_surface owned by Gtk+ so we get input
// events from Gtk+.
bool WaylandSurface::DisableUserInputLocked(
    const WaylandSurfaceLock& aProofOfLock) {
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);
  wl_region* region =
      wl_compositor_create_region(WaylandDisplayGet()->GetCompositor());
  wl_surface_set_input_region(mSurface, region);
  wl_region_destroy(region);
  mSurfaceNeedsCommit = true;
  return true;
}

void WaylandSurface::SetOpaqueRegionLocked(
    const WaylandSurfaceLock& aProofOfLock, const gfx::IntRegion& aRegion) {
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);
  if (!mSurface || !IsOpaqueRegionEnabled()) {
    return;
  }

  LOGVERBOSE("WaylandSurface::SetOpaqueRegionLocked()");

  // Region should be in surface-logical coordinates, so we need to divide by
  // the buffer scale. We use round-in in order to be safe with subpixels.
  UnknownScaleFactor scale(GetScaleSafe());
  wl_region* region =
      wl_compositor_create_region(WaylandDisplayGet()->GetCompositor());
  for (auto iter = aRegion.RectIter(); !iter.Done(); iter.Next()) {
    const auto& rect = gfx::RoundedIn(iter.Get().ToUnknownRect() / scale);
    wl_region_add(region, rect.x, rect.y, rect.Width(), rect.Height());
    LOGVERBOSE("  region [%d, %d] -> [%d x %d]", rect.x, rect.y, rect.Width(),
               rect.Height());
  }
  wl_surface_set_opaque_region(mSurface, region);
  wl_region_destroy(region);
  mSurfaceNeedsCommit = true;
}

void WaylandSurface::SetOpaqueRegion(const gfx::IntRegion& aRegion) {
  WaylandSurfaceLock lock(this);
  SetOpaqueRegionLocked(lock, aRegion);
}

void WaylandSurface::SetOpaqueLocked(const WaylandSurfaceLock& aProofOfLock) {
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);
  if (!mSurface || !IsOpaqueRegionEnabled()) {
    return;
  }
  LOGVERBOSE("WaylandSurface::SetOpaqueLocked()");
  wl_region* region =
      wl_compositor_create_region(WaylandDisplayGet()->GetCompositor());
  wl_region_add(region, 0, 0, INT32_MAX, INT32_MAX);
  wl_surface_set_opaque_region(mSurface, region);
  wl_region_destroy(region);
  mSurfaceNeedsCommit = true;
}

void WaylandSurface::ClearOpaqueRegionLocked(
    const WaylandSurfaceLock& aProofOfLock) {
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);
  if (!mSurface) {
    return;
  }
  LOGVERBOSE("WaylandSurface::ClearOpaqueLocked()");
  wl_region* region =
      wl_compositor_create_region(WaylandDisplayGet()->GetCompositor());
  wl_surface_set_opaque_region(mSurface, region);
  wl_region_destroy(region);
  mSurfaceNeedsCommit = true;
}

void WaylandSurface::FractionalScaleHandler(void* data,
                                            struct wp_fractional_scale_v1* info,
                                            uint32_t wire_scale) {
  AssertIsOnMainThread();

  WaylandSurface* waylandSurface = static_cast<WaylandSurface*>(data);
  waylandSurface->mScreenScale = wire_scale / 120.0;

  LOGS("[%p]: WaylandSurface::FractionalScaleHandler() scale: %f\n",
       waylandSurface->mLoggingWidget, (double)waylandSurface->mScreenScale);

  waylandSurface->mFractionalScaleCallback();
}

static const struct wp_fractional_scale_v1_listener fractional_scale_listener =
    {
        .preferred_scale = WaylandSurface::FractionalScaleHandler,
};

bool WaylandSurface::EnableFractionalScaleLocked(
    const WaylandSurfaceLock& aProofOfLock,
    std::function<void(void)> aFractionalScaleCallback, bool aManageViewport) {
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);

  MOZ_DIAGNOSTIC_ASSERT(!mFractionalScaleListener);
  auto* manager = WaylandDisplayGet()->GetFractionalScaleManager();
  if (!manager) {
    LOGWAYLAND(
        "WaylandSurface::SetupFractionalScale(): Failed to get "
        "FractionalScaleManager");
    return false;
  }
  mFractionalScaleListener =
      wp_fractional_scale_manager_v1_get_fractional_scale(manager, mSurface);
  wp_fractional_scale_v1_add_listener(mFractionalScaleListener,
                                      &fractional_scale_listener, this);

  // Create Viewport with aFollowsSizeChanges enabled,
  // regular rendering uses Viewport for fraction scale only.
  if (aManageViewport &&
      !CreateViewportLocked(aProofOfLock, /* aFollowsSizeChanges */ true)) {
    return false;
  }
  mFractionalScaleCallback = std::move(aFractionalScaleCallback);

  // Init scale to default values and load ceiled screen scale from GdkWindow.
  // We use it as fallback before we get mScreenScale from system.
  mScaleType = ScaleType::Fractional;

  LOGWAYLAND("WaylandSurface::SetupFractionalScale()");
  return true;
}

bool WaylandSurface::EnableCeiledScaleLocked(
    const WaylandSurfaceLock& aProofOfLock) {
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);

  if (!CreateViewportLocked(aProofOfLock, /* aFollowsSizeChanges */ true)) {
    return false;
  }

  mScaleType = ScaleType::Ceiled;

  LOGWAYLAND("WaylandSurface::EnableCeiledScaleLocked()");
  return true;
}

void WaylandSurface::ClearScaleLocked(const WaylandSurfaceLock& aProofOfLock) {
  LOGWAYLAND("WaylandSurface::ClearScaleLocked()");
  mFractionalScaleCallback = []() {};
  mScreenScale = sNoScale;
}

void WaylandSurface::SetCeiledScaleLocked(
    const WaylandSurfaceLock& aProofOfLock, int aScreenCeiledScale) {
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);

  // Allow to set scale for unmapped surfaces unconditionally.
  // It sets initial scale factor so we have something to work with.
  if (!mIsMapped || IsCeiledScaleLocked(aProofOfLock)) {
    // mScreenScale = (double)aScreenCeiledScale;
    mScreenScale = aScreenCeiledScale;
    LOGWAYLAND("WaylandSurface::SetCeiledScaleLocked() scale %f",
               (double)mScreenScale);
  }
}

void WaylandSurface::SetSizeLocked(const WaylandSurfaceLock& aProofOfLock,
                                   gfx::IntSize aSizeScaled,
                                   gfx::IntSize aSizeUnscaled) {
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);
  LOGVERBOSE(
      "WaylandSurface::SetSizeLocked(): Size [%d x %d] unscaled size [%d x %d]",
      aSizeScaled.width, aSizeScaled.height, aSizeUnscaled.width,
      aSizeUnscaled.height);
  mSizeScaled = aSizeScaled;
  if (mViewportFollowsSizeChanges) {
    SetViewPortDestLocked(aProofOfLock, aSizeUnscaled);
  }
}

void WaylandSurface::SetViewPortDestLocked(
    const WaylandSurfaceLock& aProofOfLock, gfx::IntSize aDestSize) {
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);
  if (mViewport) {
    if (mViewportDestinationSize == aDestSize) {
      return;
    }
    LOGWAYLAND("WaylandSurface::SetViewPortDestLocked(): Size [%d x %d]",
               aDestSize.width, aDestSize.height);
    if (aDestSize.width < 1 || aDestSize.height < 1) {
      NS_WARNING("WaylandSurface::SetViewPortDestLocked(): Wrong coordinates!");
      aDestSize.width = aDestSize.height = -1;
    }
    mViewportDestinationSize = aDestSize;
    wp_viewport_set_destination(mViewport, mViewportDestinationSize.width,
                                mViewportDestinationSize.height);
    mSurfaceNeedsCommit = true;
  }
}

void WaylandSurface::SetViewPortSourceRectLocked(
    const WaylandSurfaceLock& aProofOfLock, gfx::Rect aRect) {
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);

  if (!mViewport || mViewportSourceRect == aRect) {
    return;
  }

  LOGWAYLAND(
      "WaylandSurface::SetViewPortSourceRectLocked(): [%f, %f] -> [%f x %f]",
      aRect.x, aRect.y, aRect.width, aRect.height);

  // Don't throw protocol error with bad coords
  if (aRect.x < 0 || aRect.y < 0 || aRect.width < 1 || aRect.height < 1) {
    NS_WARNING(
        "WaylandSurface::SetViewPortSourceRectLocked(): Wrong coordinates!");
    aRect.x = aRect.y = aRect.width = aRect.height = -1;
  }

  mViewportSourceRect = aRect;
  wp_viewport_set_source(
      mViewport, wl_fixed_from_double(aRect.x), wl_fixed_from_double(aRect.y),
      wl_fixed_from_double(aRect.width), wl_fixed_from_double(aRect.height));
  mSurfaceNeedsCommit = true;
}

wl_surface* WaylandSurface::Lock(WaylandSurfaceLock* aWaylandSurfaceLock)
    // Disable thread safety analysis, it reports:
    // mutex 'mMutex' is still held at the end of function
    // which we want.
    MOZ_NO_THREAD_SAFETY_ANALYSIS {
  mMutex.Lock();
  MOZ_DIAGNOSTIC_ASSERT(!mSurfaceLock);
  mSurfaceLock = aWaylandSurfaceLock;
  return mIsReadyToDraw ? mSurface : nullptr;
}

void WaylandSurface::Unlock(struct wl_surface** aSurface,
                            WaylandSurfaceLock* aWaylandSurfaceLock) {
  MOZ_DIAGNOSTIC_ASSERT(*aSurface == nullptr || mSurface == nullptr ||
                        *aSurface == mSurface);
  MOZ_DIAGNOSTIC_ASSERT(mSurfaceLock == aWaylandSurfaceLock);
  mMutex.AssertCurrentThreadOwns();
  if (*aSurface) {
    *aSurface = nullptr;
  }
  mSurfaceLock = nullptr;
  mMutex.Unlock();
}

void WaylandSurface::SetGdkCommitCallbackLocked(
    const WaylandSurfaceLock& aProofOfLock,
    const std::function<void(void)>& aGdkCommitCB) {
  mGdkCommitCallback = aGdkCommitCB;
}

void WaylandSurface::ClearGdkCommitCallbackLocked(
    const WaylandSurfaceLock& aProofOfLock) {
  mGdkCommitCallback = nullptr;
}

void WaylandSurface::AfterPaintHandler(GdkFrameClock* aClock, void* aData) {
  auto* waylandSurface = static_cast<WaylandSurface*>(aData);
  if (waylandSurface->IsMapped()) {
    if (waylandSurface->mGdkCommitCallback) {
      waylandSurface->mGdkCommitCallback();
    }
    LOGS("[%p]: WaylandSurface::AfterPaintHandler()",
         waylandSurface->mLoggingWidget);
    WaylandSurfaceLock lock(waylandSurface);
    waylandSurface->CommitLocked(lock, /* aForceCommit */ true);
  }
}

bool WaylandSurface::AddOpaqueSurfaceHandlerLocked(
    const WaylandSurfaceLock& aProofOfLock, GdkWindow* aGdkWindow,
    bool aRegisterCommitHandler) {
  if (!IsOpaqueRegionEnabled() || mIsOpaqueSurfaceHandlerSet) {
    return false;
  }

  LOGWAYLAND(
      "WaylandSurface::AddOpaqueSurfaceHandlerLocked() "
      "aRegisterCommitHandler %d",
      aRegisterCommitHandler);
  AssertIsOnMainThread();

  mGdkWindow = aGdkWindow;
  sGdkWaylandWindowAddCallbackSurface(mGdkWindow, mSurface);
  mIsOpaqueSurfaceHandlerSet = true;

  if (aRegisterCommitHandler) {
    MOZ_DIAGNOSTIC_ASSERT(!mGdkAfterPaintId);
    mGdkAfterPaintId = g_signal_connect_after(
        gdk_window_get_frame_clock(mGdkWindow), "after-paint",
        G_CALLBACK(WaylandSurface::AfterPaintHandler), this);
  }

  mIsPendingGdkCleanup = true;
  return true;
}

bool WaylandSurface::RemoveOpaqueSurfaceHandlerLocked(
    const WaylandSurfaceLock& aProofOfLock) {
  if (!IsOpaqueRegionEnabled() || !mIsOpaqueSurfaceHandlerSet) {
    return false;
  }
  AssertIsOnMainThread();
  if (mSurface) {
    LOGWAYLAND("WaylandSurface::RemoveOpaqueSurfaceHandlerLocked()");
    sGdkWaylandWindowRemoveCallbackSurface(mGdkWindow, mSurface);
    mIsOpaqueSurfaceHandlerSet = false;
  }
  if (mGdkAfterPaintId) {
    GdkFrameClock* frameClock = gdk_window_get_frame_clock(mGdkWindow);
    // If we're already unmapped frameClock is nullptr
    if (frameClock) {
      g_signal_handler_disconnect(frameClock, mGdkAfterPaintId);
    }
    mGdkAfterPaintId = 0;
  }
  return true;
}

wl_egl_window* WaylandSurface::GetEGLWindow(nsIntSize aUnscaledSize) {
  LOGWAYLAND("WaylandSurface::GetEGLWindow() eglwindow %p", (void*)mEGLWindow);

  WaylandSurfaceLock lock(this);
  if (!mSurface || !mIsReadyToDraw) {
    LOGWAYLAND("  quit, mSurface %p mIsReadyToDraw %d", mSurface,
               (bool)mIsReadyToDraw);
    return nullptr;
  }

  auto scale = GetScaleSafe();
  // TODO: Correct size rounding
  nsIntSize scaledSize((int)floor(aUnscaledSize.width * scale),
                       (int)floor(aUnscaledSize.height * scale));
  if (!mEGLWindow) {
    mEGLWindow =
        wl_egl_window_create(mSurface, scaledSize.width, scaledSize.height);
    LOGWAYLAND(
        "WaylandSurface::GetEGLWindow() created eglwindow [%p] size %d x %d",
        (void*)mEGLWindow, scaledSize.width, scaledSize.height);
  } else {
    LOGWAYLAND("WaylandSurface::GetEGLWindow() resized to %d x %d",
               scaledSize.width, scaledSize.height);
    wl_egl_window_resize(mEGLWindow, scaledSize.width, scaledSize.height, 0, 0);
  }

  if (mEGLWindow) {
    SetSizeLocked(lock, scaledSize, aUnscaledSize);
  }

  return mEGLWindow;
}

// Return false if scale factor doesn't match buffer size.
// We need to skip painting in such case do avoid Wayland compositor freaking.
bool WaylandSurface::SetEGLWindowSize(nsIntSize aScaledSize) {
  WaylandSurfaceLock lock(this);

  // We may be called after unmap so we're missing egl window completelly.
  // In such case don't return false which would block compositor.
  // We return true here and don't block flush WebRender queue.
  // We'll be repainted if our window become visible again anyway.
  if (!mEGLWindow) {
    return true;
  }

  auto scale = GetScaleSafe();
  // TODO: Avoid precision lost here? Load coordinates from window?
  nsIntSize unscaledSize((int)round(aScaledSize.width / scale),
                         (int)round(aScaledSize.height / scale));

  LOGVERBOSE(
      "WaylandSurface::SetEGLWindowSize() scaled [%d x %d] unscaled [%d x %d] "
      "scale %f",
      aScaledSize.width, aScaledSize.height, unscaledSize.width,
      unscaledSize.height, scale);

  wl_egl_window_resize(mEGLWindow, aScaledSize.width, aScaledSize.height, 0, 0);
  SetSizeLocked(lock, aScaledSize, unscaledSize);
  return true;
}

void WaylandSurface::InvalidateRegionLocked(
    const WaylandSurfaceLock& aProofOfLock,
    const gfx::IntRegion& aInvalidRegion) {
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);
  MOZ_DIAGNOSTIC_ASSERT(mSurface);

  for (auto iter = aInvalidRegion.RectIter(); !iter.Done(); iter.Next()) {
    gfx::IntRect r = iter.Get();
    wl_surface_damage_buffer(mSurface, r.x, r.y, r.width, r.height);
  }
  mSurfaceNeedsCommit = true;
}

void WaylandSurface::InvalidateLocked(const WaylandSurfaceLock& aProofOfLock) {
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);
  MOZ_DIAGNOSTIC_ASSERT(mSurface);

  wl_surface_damage_buffer(mSurface, 0, 0, INT32_MAX, INT32_MAX);
  mSurfaceNeedsCommit = true;
}

void WaylandSurface::ReleaseAllWaylandBuffersLocked(
    WaylandSurfaceLock& aSurfaceLock) {
  LOGWAYLAND("WaylandSurface::ReleaseAllWaylandBuffersLocked(), buffers num %d",
             (int)mAttachedBuffers.Length());
  MOZ_DIAGNOSTIC_ASSERT(!mIsMapped);
  for (auto& buffer : mAttachedBuffers) {
    buffer->ReturnBufferAttached(aSurfaceLock);
  }
}

// BufferFreeCallbackHandler is called when WaylandBuffer is detached by
// compositor or we delete it explicitly. The two events can happen in any
// order.
void WaylandSurface::BufferFreeCallbackHandler(uintptr_t aWlBufferID,
                                               bool aWlBufferDelete) {
  LOGWAYLAND("WaylandSurface::BufferFreeCallbackHandler() wl_buffer [%" PRIxPTR
             "] buffer %s",
             aWlBufferID, aWlBufferDelete ? "delete" : "detach");
  WaylandSurfaceLock lock(this);

  // BufferFreeCallbackHandler() should be caled by Wayland compostor
  // on main thread only.
  AssertIsOnMainThread();

  for (size_t i = 0; i < mAttachedBuffers.Length(); i++) {
    if (mAttachedBuffers[i]->Matches(aWlBufferID)) {
      mAttachedBuffers[i]->ReturnBufferDetached(lock);
      mAttachedBuffers.RemoveElementAt(i);
      return;
    }
  }

  // It's possible that buffer was already freed by previous detach call
  // and we're on synced delete now. In such case just quit.
  // Reversed order (delete, detach) is not possible - we can't get detach
  // for deleted buffers.
  MOZ_DIAGNOSTIC_ASSERT(
      aWlBufferDelete,
      "Wayland compositor detach call after wl_buffer delete?");
}

static void BufferDetachedCallbackHandler(void* aData, wl_buffer* aBuffer) {
  LOGS(
      "BufferDetachedCallbackHandler() WaylandSurface [%p] received wl_buffer "
      "[%p]",
      aData, aBuffer);
  RefPtr surface = static_cast<WaylandSurface*>(aData);
  // surface may be nullptr if detached wl_buffer is no longer connected to
  // WaylandBuffer.
  if (!surface) {
    return;
  }
  surface->BufferFreeCallbackHandler(reinterpret_cast<uintptr_t>(aBuffer),
                                     /* aWlBufferDelete */ false);
}

static const struct wl_buffer_listener sBufferDetachListener = {
    BufferDetachedCallbackHandler};

bool WaylandSurface::AttachLocked(const WaylandSurfaceLock& aSurfaceLock,
                                  RefPtr<WaylandBuffer> aWaylandBuffer) {
  MOZ_DIAGNOSTIC_ASSERT(&aSurfaceLock == mSurfaceLock);
  MOZ_DIAGNOSTIC_ASSERT(mSurface);

  auto scale = GetScaleSafe();
  LayoutDeviceIntSize bufferSize = aWaylandBuffer->GetSize();
  // TODO: rounding?
  SetSizeLocked(aSurfaceLock, gfx::IntSize(bufferSize.width, bufferSize.height),
                gfx::IntSize((int)round(bufferSize.width / scale),
                             (int)round(bufferSize.height / scale)));

  wl_buffer* buffer = aWaylandBuffer->BorrowBuffer(aSurfaceLock);
  if (!buffer) {
    LOGWAYLAND("WaylandSurface::AttachLocked() failed, BorrowBuffer() failed");
    return false;
  }

  LOGWAYLAND(
      "WaylandSurface::AttachLocked() WaylandBuffer [%p] wl_buffer [%p] size "
      "[%d x %d] "
      "fractional scale %f",
      aWaylandBuffer.get(), buffer, bufferSize.width, bufferSize.height, scale);

  // We don't take reference to this. Some compositors doesn't send
  // buffer release callback and we may leak WaylandSurface then.
  // Rather we destroy wl_buffer at end which makes sure no release callback
  // comes after WaylandSurface release.
  if (wl_proxy_get_listener((wl_proxy*)buffer)) {
    // Listener is already set, update only WaylandBuffer ref.
    wl_proxy_set_user_data((wl_proxy*)buffer, this);
  } else {
    if (wl_buffer_add_listener(buffer, &sBufferDetachListener, this) < 0) {
      LOGWAYLAND(
          "WaylandSurface::AttachLocked() failed to attach buffer listener");
      aWaylandBuffer->ReturnBufferDetached(aSurfaceLock);
      return false;
    }
  }

  if (!mAttachedBuffers.Contains(aWaylandBuffer)) {
    mAttachedBuffers.AppendElement(aWaylandBuffer);
  }

  wl_surface_attach(mSurface, buffer, 0, 0);
  aWaylandBuffer->SetAttachedLocked(aSurfaceLock);
  mBufferAttached = true;
  mSurfaceNeedsCommit = true;
  return true;
}

void WaylandSurface::RemoveAttachedBufferLocked(
    const WaylandSurfaceLock& aSurfaceLock) {
  MOZ_DIAGNOSTIC_ASSERT(&aSurfaceLock == mSurfaceLock);
  MOZ_DIAGNOSTIC_ASSERT(mSurface);

  LOGWAYLAND("WaylandSurface::RemoveAttachedBufferLocked()");

  // Set our size according to attached buffer
  SetSizeLocked(aSurfaceLock, gfx::IntSize(0, 0), gfx::IntSize(0, 0));
  wl_surface_attach(mSurface, nullptr, 0, 0);
  mSurfaceNeedsCommit = true;
  mBufferAttached = false;
}

// Place this WaylandSurface above aLowerSurface
void WaylandSurface::PlaceAboveLocked(const WaylandSurfaceLock& aProofOfLock,
                                      WaylandSurfaceLock& aLowerSurfaceLock) {
  LOGVERBOSE("WaylandSurface::PlaceAboveLocked() aLowerSurface [%p]",
             aLowerSurfaceLock.GetWaylandSurface());
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);
  MOZ_DIAGNOSTIC_ASSERT(mSubsurface);

  // WaylandSurface is reffed by WaylandSurfaceLock
  WaylandSurface* lowerSurface = aLowerSurfaceLock.GetWaylandSurface();

  // lowerSurface has to be sibling or child of this
  MOZ_DIAGNOSTIC_ASSERT(lowerSurface->mParent == mParent ||
                        lowerSurface->mParent == this);

  // It's possible that lowerSurface becomed unmapped. In such rare case
  // just skip the operation, we may be deleted anyway.
  if (lowerSurface->mSurface) {
    wl_subsurface_place_above(mSubsurface, lowerSurface->mSurface);
  }
  mSurfaceNeedsCommit = true;
}

void WaylandSurface::SetTransformFlippedLocked(
    const WaylandSurfaceLock& aProofOfLock, bool aFlippedX, bool aFlippedY) {
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);
  if (aFlippedX == mBufferTransformFlippedX &&
      aFlippedY == mBufferTransformFlippedY) {
    return;
  }

  MOZ_RELEASE_ASSERT(mSurface);

  mBufferTransformFlippedX = aFlippedX;
  mBufferTransformFlippedY = aFlippedY;

  if (mBufferTransformFlippedY) {
    if (mBufferTransformFlippedX) {
      wl_surface_set_buffer_transform(mSurface, WL_OUTPUT_TRANSFORM_180);
    } else {
      wl_surface_set_buffer_transform(mSurface,
                                      WL_OUTPUT_TRANSFORM_FLIPPED_180);
    }
  } else {
    if (mBufferTransformFlippedX) {
      wl_surface_set_buffer_transform(mSurface, WL_OUTPUT_TRANSFORM_FLIPPED);
    } else {
      wl_surface_set_buffer_transform(mSurface, WL_OUTPUT_TRANSFORM_NORMAL);
    }
  }
}

GdkWindow* WaylandSurface::GetGdkWindow() const {
  // Gdk/Gtk code is used on main thread only
  AssertIsOnMainThread();
  return mGdkWindow;
}

double WaylandSurface::GetScale() {
  if (mScreenScale != sNoScale) {
    LOGVERBOSE("WaylandSurface::GetScale() fractional scale %f",
               (double)mScreenScale);
    return mScreenScale;
  }

  // We don't have scale yet - query parent surface if there's any.
  if (mParent) {
    auto scale = mParent->GetScale();
    LOGVERBOSE("WaylandSurface::GetScale() parent scale %f", scale);
    return scale;
  }

  LOGVERBOSE("WaylandSurface::GetScale() no scale available");
  return sNoScale;
}

double WaylandSurface::GetScaleSafe() {
  double scale = GetScale();
  if (scale != sNoScale) {
    return scale;
  }

  // Return default scale for now - we'll repaint after show anyway.
  return 1;
}

void WaylandSurface::SetParentLocked(const WaylandSurfaceLock& aProofOfLock,
                                     RefPtr<WaylandSurface> aParent) {
  mParent = aParent;
}

void WaylandSurface::ImageDescriptionFailed(
    void* aData, struct wp_image_description_v1* aImageDescription,
    uint32_t aCause, const char* aMsg) {
  RefPtr waylandSurface =
      already_AddRefed(reinterpret_cast<WaylandSurface*>(aData));
  WaylandSurfaceLock lock(waylandSurface);
  waylandSurface->mHDRSet = false;
  LOGS("[%p] WaylandSurface::ImageDescriptionFailed()",
       waylandSurface->mLoggingWidget);
}

void WaylandSurface::ImageDescriptionReady(
    void* aData, struct wp_image_description_v1* aImageDescription,
    uint32_t aIdentity) {
  RefPtr waylandSurface = dont_AddRef(static_cast<WaylandSurface*>(aData));
  WaylandSurfaceLock lock(waylandSurface);
  wp_color_management_surface_v1_set_image_description(
      waylandSurface->mColorSurface, waylandSurface->mImageDescription, 0);
  waylandSurface->mHDRSet = true;
  LOGS("[%p] WaylandSurface::ImageDescriptionReady()",
       waylandSurface->mLoggingWidget);
}

static const struct wp_image_description_v1_listener
    image_description_listener = {
        WaylandSurface::ImageDescriptionFailed,
        WaylandSurface::ImageDescriptionReady,
};

bool WaylandSurface::EnableColorManagementLocked(
    const WaylandSurfaceLock& aProofOfLock) {
  MOZ_DIAGNOSTIC_ASSERT(mIsMapped);
  MOZ_DIAGNOSTIC_ASSERT(!mColorSurface);

  auto* colorManager = WaylandDisplayGet()->GetColorManager();
  if (!colorManager || !WaylandDisplayGet()->IsHDREnabled()) {
    return false;
  }

  LOGWAYLAND("WaylandSurface::EnableColorManagementLocked()");

  mColorSurface = wp_color_manager_v1_get_surface(colorManager, mSurface);

  auto* params = wp_color_manager_v1_create_parametric_creator(colorManager);
  wp_image_description_creator_params_v1_set_primaries_named(
      params, WP_COLOR_MANAGER_V1_PRIMARIES_BT2020);
  wp_image_description_creator_params_v1_set_tf_named(
      params, WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ);
  mImageDescription = wp_image_description_creator_params_v1_create(params);
  // wp_image_description_creator_params_v1_create() consumes params
  params = nullptr;

  // AddRef this to keep it live until callback
  AddRef();
  wp_image_description_v1_add_listener(mImageDescription,
                                       &image_description_listener, this);

  return true;
}

void WaylandSurface::AssertCurrentThreadOwnsMutex() {
  mMutex.AssertCurrentThreadOwns();
}

}  // namespace mozilla::widget
