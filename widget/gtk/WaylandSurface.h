/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __MOZ_WAYLAND_SURFACE_H__
#define __MOZ_WAYLAND_SURFACE_H__

#include "nsWaylandDisplay.h"
#include "mozilla/Mutex.h"
#include "mozilla/Atomics.h"
#include "WaylandSurfaceLock.h"
#include "mozilla/GRefPtr.h"

/* Workaround for bug at wayland-util.h,
 * present in wayland-devel < 1.12
 */
struct wl_surface;
struct wl_subsurface;
struct wl_egl_window;

class MessageLoop;

namespace mozilla::widget {

class WaylandBuffer;

// WaylandSurface is a wrapper for Wayland rendering target
// which is wl_surface / wl_subsurface.
class WaylandSurface final {
  friend WaylandSurfaceLock;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WaylandSurface);

  WaylandSurface(RefPtr<WaylandSurface> aParent, gfx::IntSize aSize);

#ifdef MOZ_LOGGING
  nsAutoCString GetDebugTag() const;
  void* GetLoggingWidget() const { return mLoggingWidget; };
  void SetLoggingWidget(void* aWidget) { mLoggingWidget = aWidget; }
#endif

  void InitialFrameCallbackHandler(struct wl_callback* aCallback);
  void AddOrFireInitialDrawCallback(
      const std::function<void(void)>& aInitialDrawCB);
  void ClearInitialDrawCallbacks();

  void FrameCallbackHandler(struct wl_callback* aCallback, uint32_t aTime,
                            bool aRoutedFromChildSurface);
  // Run only once at most.
  void AddOneTimeFrameCallbackLocked(
      const WaylandSurfaceLock& aProofOfLock,
      const std::function<void(wl_callback*, uint32_t)>& aFrameCallbackHandler);

  // Run frame callback repeatedly. Callback is removed on Unmap.
  // If aEmulateFrameCallback is set to true and WaylandSurface is mapped and
  // ready to draw and we don't have buffer attached yet,
  // fire aFrameCallbackHandler without frame callback from
  // compositor in sFrameCheckTimeoutMs.
  void AddPersistentFrameCallbackLocked(
      const WaylandSurfaceLock& aProofOfLock,
      const std::function<void(wl_callback*, uint32_t)>& aFrameCallbackHandler,
      bool aEmulateFrameCallback = false);

  // Create and resize EGL window.
  // GetEGLWindow() takes unscaled window size as we derive size from GdkWindow.
  // It's scaled internally by WaylandSurface fractional scale.
  wl_egl_window* GetEGLWindow(nsIntSize aUnscaledSize);
  // SetEGLWindowSize() takes scaled size - it's called from rendering code
  // which uses scaled sizes.
  bool SetEGLWindowSize(nsIntSize aScaledSize);
  bool HasEGLWindow() { return !!mEGLWindow; }

  bool DoesCommitToParentSurface() { return mCommitToParentSurface; }

  // Read to draw means we got frame callback from parent surface
  // where we attached to.
  bool IsReadyToDraw() { return mIsReadyToDraw; }
  // Mapped means we have all internals created.
  bool IsMapped() { return mIsMapped; }

  bool IsOpaqueSurfaceHandlerSetLocked(const WaylandSurfaceLock& aProofOfLock) {
    return mIsOpaqueSurfaceHandlerSet;
  }

  // Mapped as direct surface of MozContainer
  bool MapLocked(const WaylandSurfaceLock& aProofOfLock, GdkWindow* aGdkWindow,
                 wl_surface* aParentWLSurface,
                 gfx::IntPoint aSubsurfacePosition, bool aCommitToParent);
  // Mapped as child of WaylandSurface (used by layers)
  bool MapLocked(const WaylandSurfaceLock& aProofOfLock, GdkWindow* aGdkWindow,
                 WaylandSurfaceLock* aParentWaylandSurfaceLock,
                 gfx::IntPoint aSubsurfacePosition);
  void Unmap();

  // Create Viewport to manage surface transformations.
  // aFollowsSizeChanges if set, Viewport destination size
  // is updated according to buffer size.
  bool CreateViewportLocked(const WaylandSurfaceLock& aProofOfLock,
                            bool aFollowsSizeChanges);

  void AddInitialDrawCallbackLocked(
      const WaylandSurfaceLock& aProofOfLock,
      const std::function<void(void)>& aInitialDrawCB);

  // Attach WaylandBuffer which shows WaylandBuffer content
  // on screen.
  bool AttachLocked(const WaylandSurfaceLock& aProofOfLock,
                    RefPtr<WaylandBuffer> aWaylandBuffer);
  // Notify WaylandSurface that WaylandBuffer wes released by Wayland compositor
  // and can be reused. We remove WaylandBuffer from 'tracked buffers'
  // internal list.
  void DetachedLocked(const WaylandSurfaceLock& aProofOfLock,
                      RefPtr<WaylandBuffer> aWaylandBuffer);

  // If there's any WaylandBuffer recently attached, detach it.
  // It makes the WaylandSurface invisible and it doesn't have any
  // content.
  void RemoveAttachedBufferLocked(const WaylandSurfaceLock& aProofOfLock);

  // CommitLocked() is needed to call after some of *Locked() method
  // to submit the action to Wayland compositor by wl_surface_commit().

  // It's possible to stack more *Locked() methods
  // together and do commit after the last one to do the changes in atomic way.

  // Need of commit is tracked by mSurfaceNeedsCommit flag and
  // if it's set, CommitLocked() is called when WaylandSurfaceLock is destroyed
  // and WaylandSurface is unlocked.
  void CommitLocked(const WaylandSurfaceLock& aProofOfLock,
                    bool aForceCommit = false, bool aForceDisplayFlush = false);

  // Place this WaylandSurface above aLowerSurface
  void PlaceAboveLocked(const WaylandSurfaceLock& aProofOfLock,
                        WaylandSurfaceLock& aLowerSurfaceLock);
  void MoveLocked(const WaylandSurfaceLock& aProofOfLock,
                  gfx::IntPoint& aPosition);
  void SetViewPortSourceRectLocked(const WaylandSurfaceLock& aProofOfLock,
                                   gfx::Rect aRect);
  void SetViewPortDestLocked(const WaylandSurfaceLock& aProofOfLock,
                             gfx::IntSize aDestSize);
  void SetTransformFlippedLocked(const WaylandSurfaceLock& aProofOfLock,
                                 bool aFlippedX, bool aFlippedY);

  void SetOpaqueRegion(const gfx::IntRegion& aRegion);
  void SetOpaqueRegionLocked(const WaylandSurfaceLock& aProofOfLock,
                             const gfx::IntRegion& aRegion);
  void SetOpaqueLocked(const WaylandSurfaceLock& aProofOfLock);

  bool DisableUserInputLocked(const WaylandSurfaceLock& aProofOfLock);
  void InvalidateRegionLocked(const WaylandSurfaceLock& aProofOfLock,
                              const gfx::IntRegion& aInvalidRegion);
  void InvalidateLocked(const WaylandSurfaceLock& aProofOfLock);

  bool EnableFractionalScaleLocked(
      const WaylandSurfaceLock& aProofOfLock,
      std::function<void(void)> aFractionalScaleCallback, bool aManageViewport);
  bool EnableCeiledScaleLocked(const WaylandSurfaceLock& aProofOfLock);

  bool IsFractionalScaleLocked(const WaylandSurfaceLock& aProofOfLock) {
    return mScaleType == ScaleType::Disabled;
  }
  bool IsCeiledScaleLocked(const WaylandSurfaceLock& aProofOfLock) {
    return mScaleType == ScaleType::Ceiled;
  }
  bool IsScaleEnabledLocked(const WaylandSurfaceLock& aProofOfLock) {
    return mScaleType != ScaleType::Disabled;
  }

  // Returns scale as float point number. If WaylandSurface is not mapped,
  // return fractional scale of parent surface.
  // Returns sNoScale is we can't get it.
  static constexpr const double sNoScale = -1;
  double GetScale();

  // Called when screen ceiled scale changed or set initial scale before we map
  // and paint the surface.
  void SetCeiledScaleLocked(const WaylandSurfaceLock& aProofOfLock,
                            int aScreenCeiledScale);

  // Called by wayland compositor when fractional scale is changed.
  static void FractionalScaleHandler(void* data,
                                     struct wp_fractional_scale_v1* info,
                                     uint32_t wire_scale);

  static void OpaqueSurfaceHandler(GdkFrameClock* aClock, void* aData);

  // See https://gitlab.gnome.org/GNOME/gtk/-/merge_requests/3111 why we use it.
  // If child surface covers whole area of parent surface,
  // Tell Gdk that this WaylandSurface may cover whole area of parent
  // surface. In such case
  bool AddOpaqueSurfaceHandlerLocked(const WaylandSurfaceLock& aProofOfLock,
                                     bool aRegisterCommitHandler = true);
  bool RemoveOpaqueSurfaceHandlerLocked(const WaylandSurfaceLock& aProofOfLock);

  GdkWindow* GetGdkWindow() const;

  static bool IsOpaqueRegionEnabled();

  void SetParentLocked(const WaylandSurfaceLock& aProofOfLock,
                       RefPtr<WaylandSurface> aParent);

 private:
  ~WaylandSurface();

  bool MapLocked(const WaylandSurfaceLock& aProofOfLock, GdkWindow* aGdkWindow,
                 wl_surface* aParentWLSurface,
                 WaylandSurfaceLock* aParentWaylandSurfaceLock,
                 gfx::IntPoint aSubsurfacePosition, bool aCommitToParent,
                 bool aSubsurfaceDesync, bool aRegisterAsOpaqueSurface);

  void SetSizeLocked(const WaylandSurfaceLock& aProofOfLock,
                     gfx::IntSize aSizeScaled, gfx::IntSize aUnscaledSize);

  wl_surface* Lock(WaylandSurfaceLock* aWaylandSurfaceLock);
  void Unlock(struct wl_surface** aSurface,
              WaylandSurfaceLock* aWaylandSurfaceLock);
  void Commit(WaylandSurfaceLock* aProofOfLock, bool aForceCommit,
              bool aForceDisplayFlush);

  bool CheckAndRemoveWaylandBuffer(WaylandBuffer* aWaylandBuffer, bool aRemove);

  void RequestFrameCallbackLocked(const WaylandSurfaceLock& aProofOfLock,
                                  bool aRequestEmulated);
  void ClearFrameCallbackLocked(const WaylandSurfaceLock& aProofOfLock);
  bool IsEmulatedFrameCallbackPending() const;

  void ClearInitialDrawCallbacksLocked(const WaylandSurfaceLock& aProofOfLock);

  void ClearScaleLocked(const WaylandSurfaceLock& aProofOfLock);

  // Weak ref to owning widget (nsWindow or NativeLayerWayland),
  // used for diagnostics/logging only.
  void* mLoggingWidget = nullptr;

  // TODO: Change to non-atomic and request lock?
  mozilla::Atomic<bool, mozilla::Relaxed> mIsMapped{false};
  mozilla::Atomic<bool, mozilla::Relaxed> mIsReadyToDraw{false};

  // Scaled surface size, ceiled or fractional.
  // This reflects real surface size which we paint.
  gfx::IntSize mSizeScaled;

  // Parent GdkWindow where we paint to, directly or via subsurface.
  RefPtr<GdkWindow> mGdkWindow;

  // Parent wl_surface owned by mGdkWindow. It's used when we're attached
  // directly to MozContainer.
  wl_surface* mParentSurface = nullptr;

  // Parent WaylandSurface.
  //
  // Layer rendering (compositor) uses mSurface directly attached to
  // wl_surface owned by mParent.
  //
  // For non-compositing rendering (old) mParent is WaylandSurface
  // owned by parent nsWindow.
  RefPtr<WaylandSurface> mParent;

  // wl_surface setup/states
  wl_surface* mSurface = nullptr;
  bool mSurfaceNeedsCommit = false;
  wl_subsurface* mSubsurface = nullptr;
  gfx::IntPoint mSubsurfacePosition{-1, -1};

  // Wayland buffers attached to this surface AND held by Wayland compositor.
  // There may be more than one buffer attached, for instance if
  // previous buffer is hold by compositor. We need to keep
  // there buffers live until compositor notify us that we
  // can release them.
  AutoTArray<RefPtr<WaylandBuffer>, 3> mAttachedBuffers;

  // Indicates mSurface has buffer attached so we can attach subsurface
  // to it and expect to get frame callbacks from Wayland compositor.
  // We set it at AttachLocked() or when we get first frame callback
  // (when EGL is used).
  bool mBufferAttached = false;

  // It's kind of special case here where mSurface equal to mParentSurface
  // so we directly paint to parent surface without subsurface.
  // It's used when Wayland compositor doesn't support subsurfaces like D&D
  // popups. This rendering setup is fragile and we want to use it as less as
  // possible because we usually don't have control over parent surface.
  // Calling code needs to make sure mParentSurface is valid and not
  // used by Gtk/GtkWidget for instance.
  bool mCommitToParentSurface = false;

  mozilla::Atomic<wl_egl_window*, mozilla::Relaxed> mEGLWindow{nullptr};

  bool mViewportFollowsSizeChanges = true;
  wp_viewport* mViewport = nullptr;
  gfx::Rect mViewportSourceRect{-1, -1, -1, -1};
  gfx::IntSize mViewportDestinationSize{-1, -1};

  // Surface flip state on X/Y asix
  bool mBufferTransformFlippedX = false;
  bool mBufferTransformFlippedY = false;

  // Frame callback registered to parent surface. When we get it we know
  // parent surface is ready and we can paint.
  wl_callback* mInitialFrameCallback = nullptr;
  std::vector<std::function<void(void)>> mInitialDrawCallbacks;

  // Frame callbacks of this surface
  wl_callback* mFrameCallback = nullptr;

  struct FrameCallback {
    std::function<void(wl_callback*, uint32_t)> mCb;
    bool mEmulated = false;
  };

  // Frame callback handlers called every frame
  std::vector<FrameCallback> mPersistentFrameCallbackHandlers;
  // Frame callback handlers called only once
  std::vector<FrameCallback> mOneTimeFrameCallbackHandlers;

  xx_color_management_surface_v4* mColorSurface = nullptr;
  xx_image_description_v4* mImageDescription = nullptr;
  xx_image_description_creator_params_v4* mImageCreatorParams = nullptr;

  // WaylandSurface is used from Compositor/Rendering/Main threads.
  mozilla::Mutex mMutex{"WaylandSurface"};
  WaylandSurfaceLock* mSurfaceLock = nullptr;

  // We may mark part of mSurface as opaque (non-transparent) if it's supported
  // by Gtk which allows compositor to skip painting of covered parts.
  bool mIsOpaqueSurfaceHandlerSet = false;
  gulong mGdkAfterPaintId = 0;
  static bool sIsOpaqueRegionEnabled;
  static void (*sGdkWaylandWindowAddCallbackSurface)(GdkWindow*,
                                                     struct wl_surface*);
  static void (*sGdkWaylandWindowRemoveCallbackSurface)(GdkWindow*,
                                                        struct wl_surface*);
  guint mFrameCheckTimerID = 0;
  constexpr static int sFrameCheckTimeoutMs = (int)(1000.0 / 60.0);

  // We use two scale systems in Firefox/Wayland. Ceiled (integer) scale and
  // fractional scale. Ceiled scale is easy to implement but comes with
  // rendering overhead while fractional rendering paints buffers with exact
  // scale.
  //
  // Fractional scale is used as rendering optimization.
  // For instance if 225% scale is used, ceiled scale is 3
  // and fractional 2.20.
  //
  // If we paint content with ceiled scale 3 and desktop uses scale 225%,
  // Wayland compositor downscales buffer to 2.20 on rendering
  // but we paint more pixels than neccessary (so we use name ceiled).
  //
  // Scale is used by wp_viewport. If a surface has a surface-local size
  // of 100 px by 50 px and wishes to submit buffers with a scale of 1.5,
  // then a buffer of 150px by 75 px should be used and the wp_viewport
  // destination rectangle should be 100 px by 50 px.
  // The wl_surface buffer scale should remain set to 1.
  //
  // For scale 2 (200%) we use surface size 200 x 100 px and set
  // viewport size to 100 x 50 px.
  //
  // We're getting fractional scale number with a small delay from
  // wp_fractional_scale_v1 after fist commit to surface.
  // Meanwhile we can use ceiled scale number instead of fractional one or
  // get fractional scale from parent window (if there's any).
  //
  enum class ScaleType {
    Disabled,
    Ceiled,
    Fractional,
  };

  ScaleType mScaleType = ScaleType::Disabled;

  // mScreenScale is set from main thread only but read from
  // different threads.
  mozilla::Atomic<double, mozilla::Relaxed> mScreenScale{sNoScale};

  wp_fractional_scale_v1* mFractionalScaleListener = nullptr;

  // mFractionalScaleCallback is called from
  // wp_fractional_scale_v1_add_listener when scale is changed.
  std::function<void(void)> mFractionalScaleCallback = []() {};
};

}  // namespace mozilla::widget

#endif /* __MOZ_WAYLAND_SURFACE_H__ */
