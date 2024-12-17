/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WaylandSurface.h"
#include "WaylandBuffer.h"
#include <wayland-egl.h>

/*
  TODO:
    - Fractional popups has wrong location / are small
    - Use set zero size Viewport skip rendering of empty attached base surface
    - Remove compostor pauses from nsWindow
    - Get initial scale from monitor (if parent is missing?)
    - Layer Rendering - do we need emulated frame callback to start rendering?
*/

#undef LOG
#undef LOGVERBOSE

#ifdef MOZ_LOGGING
#  undef LOGWAYLAND

#  include "mozilla/Logging.h"
#  include "nsTArray.h"
#  include "Units.h"
#  include "nsWindow.h"
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
#  define LOGVERBOSE(...)
#  define LOGS(...)
#  define LOGS_VERBOSE(...)
#endif /* MOZ_LOGGING */

using namespace mozilla;
using namespace mozilla::widget;

namespace mozilla::widget {

bool WaylandSurface::sForceEmulateFrameCallback = false;

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
  static std::once_flag onceFlag;
  std::call_once(onceFlag, [&] {
    sForceEmulateFrameCallback =
        StaticPrefs::widget_wayland_emulate_frame_callback_AtStartup();
  });
  LOGWAYLAND("WaylandSurface::WaylandSurface(), parent [%p] size [%d x %d]",
             mParent ? mParent->GetLoggingWidget() : nullptr, mSizeScaled.width,
             mSizeScaled.height);
}

WaylandSurface::~WaylandSurface() {
  LOGWAYLAND("WaylandSurface::~WaylandSurface()");
  MOZ_RELEASE_ASSERT(!mIsMapped, "We can't release mapped WaylandSurface!");
  MOZ_RELEASE_ASSERT(!mSurfaceLock, "We can't release locked WaylandSurface!");
  MOZ_RELEASE_ASSERT(mAttachedBuffers.Length() == 0,
                     "We can't release surface with buffer attached!");
  MOZ_RELEASE_ASSERT(!mFrameCheckTimerID,
                     "We can't release WaylandSurface with active timer");
}

void WaylandSurface::InitialFrameCallbackHandler(struct wl_callback* callback) {
  LOGWAYLAND(
      "WaylandSurface::InitialFrameCallbackHandler() "
      "mInitialFrameCallback %p mIsReadyToDraw %d initial_draw callback %zd\n",
      (void*)mInitialFrameCallback, (bool)mIsReadyToDraw,
      mInitialDrawCallbacks.size());

  // We're supposed to run on main thread only.
  AssertIsOnMainThread();

  // mInitialFrameCallback/callback can be nullptr when redering directly
  // to GtkWidget and InitialFrameCallbackHandler is called by us from main
  // thread by WaylandSurface::Map().
  MOZ_RELEASE_ASSERT(mInitialFrameCallback == callback);

  std::vector<std::function<void(void)>> cbs;
  {
    WaylandSurfaceLock lock(this);
    MozClearPointer(mInitialFrameCallback, wl_callback_destroy);
    // It's possible that we're already unmapped so quit in such case.
    if (!mSurface) {
      LOGWAYLAND("  WaylandSurface is unmapped, quit.");
      if (!mInitialDrawCallbacks.empty()) {
        NS_WARNING("Unmapping WaylandSurface with active draw callback!");
        mInitialDrawCallbacks.clear();
      }
      return;
    }
    if (mIsReadyToDraw) {
      return;
    }
    mIsReadyToDraw = true;
    cbs = std::move(mInitialDrawCallbacks);
  }

  // We can't call the callbacks under lock
#ifdef MOZ_LOGGING
  int callbackNum = 0;
#endif
  for (auto const& cb : cbs) {
    LOGWAYLAND("  initial callback fire  [%d]", callbackNum++);
    cb();
  }

  // If there's any frame callback waiting, register the handler now to fire
  // them
  if (!mPersistentFrameCallbackHandlers.empty() ||
      !mOneTimeFrameCallbackHandlers.empty()) {
    LOGWAYLAND("  initial callback: Register regular frame callback");
    WaylandSurfaceLock lock(this);
    RequestFrameCallbackLocked(lock);
  }
}

static void InitialFrameCallbackHandler(void* aWaylandSurface,
                                        struct wl_callback* callback,
                                        uint32_t time) {
  auto* waylandSurface = static_cast<WaylandSurface*>(aWaylandSurface);
  waylandSurface->InitialFrameCallbackHandler(callback);
}

static const struct wl_callback_listener sWaylandSurfaceInitialFrameListener = {
    ::InitialFrameCallbackHandler};

void WaylandSurface::AddInitialDrawCallbackLocked(
    const WaylandSurfaceLock& aProofOfLock,
    const std::function<void(void)>& aInitialDrawCB) {
  LOGVERBOSE("WaylandSurface::AddInitialDrawCallbackLocked()");
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);

  if (mIsReadyToDraw && !mSurface) {
    NS_WARNING(
        "WaylandSurface::AddInitialDrawCallbackLocked():"
        " ready to draw without wayland surface!");
  }
  MOZ_DIAGNOSTIC_ASSERT(!mIsReadyToDraw || !mSurface);
  mInitialDrawCallbacks.push_back(aInitialDrawCB);
}

void WaylandSurface::AddOrFireInitialDrawCallback(
    const std::function<void(void)>& aInitialDrawCB) {
  {
    WaylandSurfaceLock lock(this);
    if (mIsReadyToDraw && !mSurface) {
      NS_WARNING(
          "WaylandSurface::AddOrFireInitialDrawCallback(): ready to draw "
          "without wayland surface!");
    }
    if (!mIsReadyToDraw || !mSurface) {
      LOGVERBOSE(
          "WaylandSurface::AddOrFireInitialDrawCallback() callback stored");
      mInitialDrawCallbacks.push_back(aInitialDrawCB);
      return;
    }
  }

  LOGWAYLAND("WaylandSurface::AddOrFireInitialDrawCallback() callback fire");

  // We're ready to draw and we have a surface to draw into.
  aInitialDrawCB();
}

void WaylandSurface::ClearInitialDrawCallbacksLocked(
    const WaylandSurfaceLock& aProofOfLock) {
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);
  MozClearPointer(mInitialFrameCallback, wl_callback_destroy);
  mInitialDrawCallbacks.clear();
}

void WaylandSurface::ClearInitialDrawCallbacks() {
  WaylandSurfaceLock lock(this);
  ClearInitialDrawCallbacksLocked(lock);
}

void WaylandSurface::FrameCallbackHandler(struct wl_callback* aCallback,
                                          uint32_t aTime,
                                          bool aRoutedFromChildSurface) {
  // aCallback is nullptr if it's emulated or called from external source
  MOZ_DIAGNOSTIC_ASSERT(aCallback == nullptr || mFrameCallback == aCallback);

  // We're supposed to run on main thread only.
  AssertIsOnMainThread();

  bool emulatedCallback = !aCallback && !aTime;

  std::vector<FrameCallback> cbs;
  {
    WaylandSurfaceLock lock(this);
    LOGVERBOSE(
        "WaylandSurface::FrameCallbackHandler() one-time %zd "
        "persistent %zd emulated %d routed %d",
        mOneTimeFrameCallbackHandlers.size(),
        mPersistentFrameCallbackHandlers.size(), emulatedCallback,
        aRoutedFromChildSurface);

    // Frame callback is not emulated and comes from this surface,
    // so we don't need to run emulation any more.
    if (!emulatedCallback && !aRoutedFromChildSurface) {
      MOZ_DIAGNOSTIC_ASSERT(!sForceEmulateFrameCallback);
      mEmulateFrameCallback = false;
    }
    // Don't run emulated callbacks on hidden surfaces
    if (emulatedCallback && !mIsMapped) {
      return;
    }
    if (aCallback) {
      ClearFrameCallbackLocked(lock);
    }

    if (emulatedCallback) {
      mOneTimeFrameCallbackHandlers.erase(
          std::remove_if(mOneTimeFrameCallbackHandlers.begin(),
                         mOneTimeFrameCallbackHandlers.end(),
                         [&](FrameCallback const& cb) {
                           if (cb.mEmulated) {
                             cbs.push_back(cb);
                           }
                           return cb.mEmulated;
                         }),
          mOneTimeFrameCallbackHandlers.end());
    } else {
      cbs = std::move(mOneTimeFrameCallbackHandlers);
    }
    std::copy(mPersistentFrameCallbackHandlers.begin(),
              mPersistentFrameCallbackHandlers.end(), back_inserter(cbs));
  }

  // We can't run the callbacks under WaylandSurfaceLock
#ifdef MOZ_LOGGING
  int callbackNum = 0;
#endif
  for (auto const& cb : cbs) {
    LOGVERBOSE("  frame callback fire [%d]", callbackNum++);
    const auto& [callback, runEmulated] = cb;
    if (emulatedCallback && !runEmulated) {
      continue;
    }
    callback(aCallback, aTime);
  }

  // Fire frame callback again if there's any pending frame callback
  if (!mPersistentFrameCallbackHandlers.empty() ||
      !mOneTimeFrameCallbackHandlers.empty()) {
    WaylandSurfaceLock lock(this);
    RequestFrameCallbackLocked(lock);
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
      "WaylandSurface::RequestFrameCallbackLocked(), mapped %d emulate %d "
      "frame "
      "%d",
      !!mIsMapped, mEmulateFrameCallback, !!mFrameCallback);
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);
  if (!mIsMapped) {
    // Frame callback will be added by initial frame callback after Map
    // or we're unmapped.
    return;
  }
  MOZ_DIAGNOSTIC_ASSERT(mSurface, "Missing mapped surface!");
  if (!mFrameCallback && !sForceEmulateFrameCallback) {
    mFrameCallback = wl_surface_frame(mSurface);
    wl_callback_add_listener(mFrameCallback, &sWaylandSurfaceFrameListener,
                             this);
    mSurfaceNeedsCommit = true;
  }

  if (mEmulateFrameCallback && !mFrameCheckTimerID) {
    LOGVERBOSE(
        "WaylandSurface::RequestFrameCallbackLocked() emulated, schedule "
        "next check");
    // Frame callback needs to be run from main thread
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "WaylandSurface::RequestFrameCallbackLocked",
        [this, self = RefPtr{this}]() {
          MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
          WaylandSurfaceLock lock(this);
          if (mIsMapped && !mFrameCheckTimerID) {
            mFrameCheckTimerID = g_timeout_add(
                sFrameCheckTimeoutMs,
                [](void* data) -> gint {
                  RefPtr surface = static_cast<WaylandSurface*>(data);
                  LOGS_VERBOSE("[%p]: WaylandSurface::Fallback frame callback",
                               surface->GetLoggingWidget());
                  // Clear timer ID as we're going to remove this timer
                  surface->mFrameCheckTimerID = 0;
                  surface->FrameCallbackHandler(
                      nullptr, 0, /* aEmulateFrameCallback */ false);
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

void WaylandSurface::AddOneTimeFrameCallbackLocked(
    const WaylandSurfaceLock& aProofOfLock,
    const std::function<void(wl_callback*, uint32_t)>& aFrameCallbackHandler,
    bool aEmulateFrameCallback) {
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);
  LOGWAYLAND("WaylandSurface::AddOneTimeFrameCallbackLocked()");

  mOneTimeFrameCallbackHandlers.push_back(
      FrameCallback{aFrameCallbackHandler, aEmulateFrameCallback});
  RequestFrameCallbackLocked(aProofOfLock);
}

void WaylandSurface::AddPersistentFrameCallbackLocked(
    const WaylandSurfaceLock& aProofOfLock,
    const std::function<void(wl_callback*, uint32_t)>& aFrameCallbackHandler,
    bool aEmulateFrameCallback) {
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);
  LOGWAYLAND("WaylandSurface::AddPersistentFrameCallbackLocked()");

  mPersistentFrameCallbackHandlers.push_back(
      FrameCallback{aFrameCallbackHandler, aEmulateFrameCallback});
  RequestFrameCallbackLocked(aProofOfLock);
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

bool WaylandSurface::MapLocked(const WaylandSurfaceLock& aProofOfLock,
                               GdkWindow* aGdkWindow,
                               wl_surface* aParentWLSurface,
                               WaylandSurfaceLock* aParentWaylandSurfaceLock,
                               gfx::IntPoint aSubsurfacePosition,
                               bool aCommitToParent, bool aSubsurfaceDesync,
                               bool aRegisterAsOpaqueSurface) {
  LOGWAYLAND("WaylandSurface::MapLocked()");
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);

  // We're already mapped, so just update subsurface position.
  if (mIsMapped) {
    if (mCommitToParentSurface) {
      return true;
    }
    wl_surface* newParentSurface =
        aParentWLSurface
            ? aParentWLSurface
            : aParentWaylandSurfaceLock->GetWaylandSurface()->mSurface;
    MOZ_RELEASE_ASSERT(mParentSurface == newParentSurface);
    MoveLocked(aProofOfLock, aSubsurfacePosition);
    return true;
  }

  MOZ_DIAGNOSTIC_ASSERT(!(aParentWLSurface && aParentWaylandSurfaceLock),
                        "Only one parent can be used.");
  MOZ_DIAGNOSTIC_ASSERT(aGdkWindow,
                        "Missing parent GdkWindow, parent is unmapped?");
  MOZ_DIAGNOSTIC_ASSERT(!mSurface && !mSubsurface, "Already mapped?");

  mGdkWindow = aGdkWindow;

  if (aParentWLSurface) {
    mParentSurface = aParentWLSurface;
  } else {
    MOZ_DIAGNOSTIC_ASSERT(!mParentSurface, "Already mapped?");
    mParent = aParentWaylandSurfaceLock->GetWaylandSurface();
    MOZ_DIAGNOSTIC_ASSERT(mParent->IsMapped(), "Parent surface is not mapped?");
    mParentSurface = mParent->mSurface;
  }

  mCommitToParentSurface = aCommitToParent;
  mSubsurfacePosition = aSubsurfacePosition;

  if (mCommitToParentSurface) {
    LOGWAYLAND("    commit to parent");
    mIsMapped = true;
    mSurface = mParentSurface;
    NS_DispatchToCurrentThread(NS_NewRunnableFunction(
        "InitialFrameCallbackHandler", [self = RefPtr{this}]() {
          self->InitialFrameCallbackHandler(nullptr);
        }));
    return true;
  }

  // Available as of GTK 3.8+
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

  mInitialFrameCallback = wl_surface_frame(mParentSurface);
  wl_callback_add_listener(mInitialFrameCallback,
                           &sWaylandSurfaceInitialFrameListener, this);
  LOGWAYLAND("    created frame callback ID %d\n",
             wl_proxy_get_id((struct wl_proxy*)mInitialFrameCallback));

  CommitLocked(aProofOfLock, /* aForceCommit */ true,
               /* aForceDisplayFlush */ true);

  if (aRegisterAsOpaqueSurface) {
    MOZ_DIAGNOSTIC_ASSERT(mGdkWindow);
    AddOpaqueSurfaceHandlerLocked(aProofOfLock);
  }

  mIsMapped = true;

  LOGWAYLAND("    created surface %p ID %d", (void*)mSurface,
             wl_proxy_get_id((struct wl_proxy*)mSurface));
  return true;
}

bool WaylandSurface::MapLocked(const WaylandSurfaceLock& aProofOfLock,
                               GdkWindow* aGdkWindow,
                               wl_surface* aParentWLSurface,
                               gfx::IntPoint aSubsurfacePosition,
                               bool aCommitToParent) {
  return MapLocked(aProofOfLock, aGdkWindow, aParentWLSurface, nullptr,
                   aSubsurfacePosition, aCommitToParent,
                   /* aSubsurfaceDesync */ true,
                   /* aRegisterAsOpaqueSurface */ true);
}

bool WaylandSurface::MapLocked(const WaylandSurfaceLock& aProofOfLock,
                               GdkWindow* aGdkWindow,
                               WaylandSurfaceLock* aParentWaylandSurfaceLock,
                               gfx::IntPoint aSubsurfacePosition) {
  return MapLocked(aProofOfLock, aGdkWindow, nullptr, aParentWaylandSurfaceLock,
                   aSubsurfacePosition,
                   /* aCommitToParent */ false,
                   /* aSubsurfaceDesync */ true,
                   /* aRegisterAsOpaqueSurface */ false);
}

void WaylandSurface::Unmap() {
  if (!mIsMapped) {
    return;
  }

  WaylandSurfaceLock lock(this);
  LOGWAYLAND("WaylandSurface::Unmap()");
  mIsMapped = false;

  // If mCommitToParentSurface is set, mSurface may be already deleted as
  // unamp/hide Gtk handler is called before us and we can't do anything
  // with it (at least I don't know how to override it).
  // So make it cleat and don't use it.
  // It doesn't matter much as we use direct rendering for D&D popups only.
  if (mCommitToParentSurface) {
    mSurface = nullptr;
  }

  ClearInitialDrawCallbacksLocked(lock);
  RemoveOpaqueSurfaceHandlerLocked(lock);
  ClearFrameCallbackLocked(lock);
  ClearScaleLocked(lock);

  MozClearPointer(mViewport, wp_viewport_destroy);
  mViewportDestinationSize = gfx::IntSize(-1, -1);
  mViewportSourceRect = gfx::Rect(-1, -1, -1, -1);

  wl_egl_window* tmp = nullptr;
  mEGLWindow.exchange(tmp);
  MozClearPointer(tmp, wl_egl_window_destroy);
  MozClearPointer(mFractionalScaleListener, wp_fractional_scale_v1_destroy);
  MozClearPointer(mSubsurface, wl_subsurface_destroy);
  MozClearPointer(mSurface, wl_surface_destroy);

  // Remove references to WaylandBuffers attached to mSurface.
  // We may not get buffer release callback when wl_surface is destroyed.
  // Also compositors may fail to send wl_buffer release callback sometime
  // which leads to memleaks.
  mAttachedBuffers.Clear();

  mGdkWindow = nullptr;
  mIsReadyToDraw = false;
  mEmulateFrameCallback = true;
  MozClearHandleID(mFrameCheckTimerID, g_source_remove);
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
                                gfx::IntPoint& aPosition) {
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);
  MOZ_DIAGNOSTIC_ASSERT(mIsMapped);

  if (mSubsurfacePosition == aPosition || mCommitToParentSurface) {
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
  // Region should be in surface-logical coordinates, so we need to divide by
  // the buffer scale. We use round-in in order to be safe with subpixels.
  UnknownScaleFactor scale(GetScale());
  wl_region* region =
      wl_compositor_create_region(WaylandDisplayGet()->GetCompositor());
  for (auto iter = aRegion.RectIter(); !iter.Done(); iter.Next()) {
    const auto& rect = gfx::RoundedIn(iter.Get().ToUnknownRect() / scale);
    wl_region_add(region, rect.x, rect.y, rect.Width(), rect.Height());
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
  wl_region* region =
      wl_compositor_create_region(WaylandDisplayGet()->GetCompositor());
  wl_region_add(region, 0, 0, INT32_MAX, INT32_MAX);
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

  mScaleType = ScaleType::Disabled;

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
    mViewportDestinationSize = aDestSize;
    if (!mViewportDestinationSize.width || !mViewportDestinationSize.height) {
      mViewportDestinationSize.width = mViewportDestinationSize.height = -1;
    }
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

void WaylandSurface::OpaqueSurfaceHandler(GdkFrameClock* aClock, void* aData) {
  auto* waylandSurface = static_cast<WaylandSurface*>(aData);
  if (waylandSurface->IsMapped()) {
    LOGS("[%p]: WaylandSurface::OpaqueSurfaceHandler()",
         waylandSurface->mLoggingWidget);
    WaylandSurfaceLock lock(waylandSurface);
    waylandSurface->CommitLocked(lock, /* aForceCommit */ true);
  }
}

bool WaylandSurface::AddOpaqueSurfaceHandlerLocked(
    const WaylandSurfaceLock& aProofOfLock, bool aRegisterCommitHandler) {
  if (!IsOpaqueRegionEnabled() || mIsOpaqueSurfaceHandlerSet) {
    return false;
  }

  LOGWAYLAND(
      "WaylandSurface::AddOpaqueSurfaceHandlerLocked() aRegisterCommitHandler "
      "%d",
      aRegisterCommitHandler);

  AssertIsOnMainThread();
  sGdkWaylandWindowAddCallbackSurface(mGdkWindow, mSurface);
  mIsOpaqueSurfaceHandlerSet = true;

  if (aRegisterCommitHandler) {
    MOZ_DIAGNOSTIC_ASSERT(!mGdkAfterPaintId);
    mGdkAfterPaintId = g_signal_connect_after(
        gdk_window_get_frame_clock(mGdkWindow), "after-paint",
        G_CALLBACK(WaylandSurface::OpaqueSurfaceHandler), this);
  }
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

  auto scale = GetScale();
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

  auto scale = GetScale();
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

  if (mCommitToParentSurface) {
    // When committing to parent surface we must use wl_surface_damage().
    // A parent surface is created as v.3 object which does not support
    // wl_surface_damage_buffer().
    wl_surface_damage(mSurface, 0, 0, INT32_MAX, INT32_MAX);
  } else {
    for (auto iter = aInvalidRegion.RectIter(); !iter.Done(); iter.Next()) {
      gfx::IntRect r = iter.Get();
      wl_surface_damage_buffer(mSurface, r.x, r.y, r.width, r.height);
    }
  }
  mSurfaceNeedsCommit = true;
}

void WaylandSurface::InvalidateLocked(const WaylandSurfaceLock& aProofOfLock) {
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);
  MOZ_DIAGNOSTIC_ASSERT(mSurface);

  wl_surface_damage(mSurface, 0, 0, INT32_MAX, INT32_MAX);
  mSurfaceNeedsCommit = true;
}

bool WaylandSurface::CheckAndRemoveWaylandBuffer(WaylandBuffer* aWaylandBuffer,
                                                 bool aRemove) {
  for (size_t i = 0; i < mAttachedBuffers.Length(); i++) {
    if (mAttachedBuffers[i] == aWaylandBuffer) {
      if (aRemove) {
        MOZ_DIAGNOSTIC_ASSERT(!mAttachedBuffers[i]->IsAttached(),
                              "Removing attached buffer!");
        mAttachedBuffers.RemoveElementAt(i);
      }
      return true;
    }
  }
  return false;
}

bool WaylandSurface::AttachLocked(const WaylandSurfaceLock& aProofOfLock,
                                  RefPtr<WaylandBuffer> aWaylandBuffer) {
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);
  MOZ_DIAGNOSTIC_ASSERT(mSurface);

  wl_buffer* buffer = aWaylandBuffer->BorrowBuffer(this);
  if (!buffer) {
    LOGWAYLAND("WaylandSurface::AttachLocked() failed, BorrowBuffer() failed");
    return false;
  }

  auto scale = GetScale();
  MOZ_DIAGNOSTIC_ASSERT(scale != sNoScale);

  LayoutDeviceIntSize bufferSize = aWaylandBuffer->GetSize();
  // TODO: rounding?
  SetSizeLocked(aProofOfLock, gfx::IntSize(bufferSize.width, bufferSize.height),
                gfx::IntSize((int)round(bufferSize.width / scale),
                             (int)round(bufferSize.height / scale)));
  LOGWAYLAND(
      "WaylandSurface::AttachLocked() WaylandBuffer [%p] size [%d x %d] "
      "fractional scale %f",
      aWaylandBuffer.get(), bufferSize.width, bufferSize.height, scale);

  // Take reference to buffer until it's released by compositor
  MOZ_DIAGNOSTIC_ASSERT(
      !CheckAndRemoveWaylandBuffer(aWaylandBuffer, /* aRemove */ false),
      "Wayland buffer is already attached?");
  mAttachedBuffers.AppendElement(std::move(aWaylandBuffer));
  if (mCommitToParentSurface) {
    wl_surface_set_buffer_scale(mSurface, 1);
  }
  wl_surface_attach(mSurface, buffer, 0, 0);
  mSurfaceNeedsCommit = true;

  mEmulateFrameCallback = sForceEmulateFrameCallback;
  return true;
}

void WaylandSurface::RemoveAttachedBufferLocked(
    const WaylandSurfaceLock& aProofOfLock) {
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);
  MOZ_DIAGNOSTIC_ASSERT(mSurface);

  LOGWAYLAND("WaylandSurface::RemoveAttachedBufferLocked()");

  // Set our size according to attached buffer
  SetSizeLocked(aProofOfLock, gfx::IntSize(0, 0), gfx::IntSize(0, 0));
  wl_surface_attach(mSurface, nullptr, 0, 0);
  mSurfaceNeedsCommit = true;

  mEmulateFrameCallback = true;
}

void WaylandSurface::DetachedLocked(const WaylandSurfaceLock& aProofOfLock,
                                    RefPtr<WaylandBuffer> aWaylandBuffer) {
  LOGWAYLAND("WaylandSurface::DetachedLocked() WaylandBuffer [%p]",
             aWaylandBuffer.get());
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);

  bool removed =
      CheckAndRemoveWaylandBuffer(aWaylandBuffer, /* aRemove */ true);
  MOZ_DIAGNOSTIC_ASSERT(removed, "Wayland buffer is not attached?");
}

// Place this WaylandSurface above aLowerSurface
void WaylandSurface::PlaceAboveLocked(const WaylandSurfaceLock& aProofOfLock,
                                      WaylandSurfaceLock& aLowerSurfaceLock) {
  LOGWAYLAND("WaylandSurface::PlaceAboveLocked() aLowerSurface [%p]",
             aLowerSurfaceLock.GetWaylandSurface());
  MOZ_DIAGNOSTIC_ASSERT(&aProofOfLock == mSurfaceLock);
  MOZ_DIAGNOSTIC_ASSERT(mSubsurface);

  // WaylandSurface is reffed by WaylandSurfaceLock
  WaylandSurface* lowerSurface = aLowerSurfaceLock.GetWaylandSurface();
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

void WaylandSurface::SetParentLocked(const WaylandSurfaceLock& aProofOfLock,
                                     RefPtr<WaylandSurface> aParent) {
  mParent = aParent;
}

}  // namespace mozilla::widget
