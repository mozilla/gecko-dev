/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_NativeLayerWayland_h
#define mozilla_layers_NativeLayerWayland_h

#include <deque>
#include <unordered_map>

#include "mozilla/Mutex.h"
#include "mozilla/layers/NativeLayer.h"
#include "mozilla/layers/SurfacePoolWayland.h"
#include "mozilla/widget/DMABufFormats.h"
#include "nsRegion.h"
#include "nsTArray.h"

namespace mozilla::wr {
class RenderDMABUFTextureHost;
}  // namespace mozilla::wr

namespace mozilla::widget {
class WaylandSurfaceLock;
}  // namespace mozilla::widget

namespace mozilla::layers {
class NativeLayerWaylandExternal;
class NativeLayerWaylandRender;

struct LayerState {
  bool mIsVisible : 1;
};

class NativeLayerRootWayland final : public NativeLayerRoot {
 public:
  static already_AddRefed<NativeLayerRootWayland> Create(
      RefPtr<widget::WaylandSurface> aWaylandSurface);

  // Overridden methods
  already_AddRefed<NativeLayer> CreateLayer(
      const gfx::IntSize& aSize, bool aIsOpaque,
      SurfacePoolHandle* aSurfacePoolHandle) override;
  already_AddRefed<NativeLayer> CreateLayerForExternalTexture(
      bool aIsOpaque) override;

  void AppendLayer(NativeLayer* aLayer) override;
  void RemoveLayer(NativeLayer* aLayer) override;
  void SetLayers(const nsTArray<RefPtr<NativeLayer>>& aLayers) override;
  void ClearLayers();

  void PrepareForCommit() override { mFrameInProcess = true; };
  bool CommitToScreen() override;

  // Main thread only
  GdkWindow* GetGdkWindow() const;

  RefPtr<widget::WaylandSurface> GetWaylandSurface() { return mSurface; }

  RefPtr<widget::DRMFormat> GetDRMFormat() { return mDRMFormat; }

  void FrameCallbackHandler(uint32_t aTime);

  RefPtr<widget::WaylandBuffer> BorrowExternalBuffer(
      RefPtr<DMABufSurface> aDMABufSurface);

#ifdef MOZ_LOGGING
  nsAutoCString GetDebugTag() const;
  void* GetLoggingWidget() const;
#endif

  void Init();
  void Shutdown();

  void UpdateLayersOnMainThread();
  void RequestUpdateOnMainThreadLocked(const MutexAutoLock& aProofOfLock);

  explicit NativeLayerRootWayland(
      RefPtr<widget::WaylandSurface> aWaylandSurface);

 private:
  ~NativeLayerRootWayland();

  bool CommitToScreenLocked(const MutexAutoLock& aProofOfLock);

  // Map NativeLayerRootWayland and all child surfaces.
  // Returns true if we're set.
  bool MapLocked(const MutexAutoLock& aProofOfLock);

  bool UpdateLayersLocked(const MutexAutoLock& aProofOfLock);

  bool IsEmptyLocked(const MutexAutoLock& aProofOfLock);

#ifdef MOZ_LOGGING
  void LogStatsLocked(const MutexAutoLock& aProofOfLock);
#endif

  Mutex mMutex MOZ_UNANNOTATED;

#ifdef MOZ_LOGGING
  void* mLoggingWidget = nullptr;
#endif

  // WaylandSurface of nsWindow (our root window).
  // This WaylandSurface is owned by nsWindow so we don't map/unmap it
  // or handle any callbacks.
  RefPtr<widget::WaylandSurface> mSurface;

  // Copy of DRM format we use to create DMABuf surfaces
  RefPtr<widget::DRMFormat> mDRMFormat;

  // Empty buffer attached to mSurface. We need to have something
  // attached to make mSurface and all child visible.
  RefPtr<widget::WaylandBufferSHM> mTmpBuffer;

  // Child layers attached to this root, they're all on the same level
  // so all child layers are attached to mContainer as subsurfaces.
  // Layer visibility is sorted by Z-order, mSublayers[0] is on bottom.
  nsTArray<RefPtr<NativeLayerWayland>> mSublayers;

  // Child layers which needs to be updated on main thread,
  // they have been added or removed.
  nsTArray<RefPtr<NativeLayerWayland>> mMainThreadUpdateSublayers;

  // External buffers (DMABuf) used by the layers.
  // We want to cache and reuse wl_buffer of external images.
  nsTArray<widget::WaylandBufferDMABUFHolder> mExternalBuffers;

  // We're between CompositorBeginFrame() / CompositorEndFrame() calls.
  mozilla::Atomic<bool, mozilla::Relaxed> mFrameInProcess{false};

  uint32_t mLastFrameCallbackTime = 0;

  // State flags used for optimizations
  // Layers have been added/removed
  bool mNeedsLayerUpdate = false;
  bool mMainThreadUpdateQueued = false;
};

class NativeLayerWayland : public NativeLayer {
 public:
  NativeLayerWayland* AsNativeLayerWayland() override { return this; }
  virtual NativeLayerWaylandExternal* AsNativeLayerWaylandExternal() {
    return nullptr;
  }
  virtual NativeLayerWaylandRender* AsNativeLayerWaylandRender() {
    return nullptr;
  }

  // Overridden methods
  gfx::IntSize GetSize() override;
  void SetPosition(const gfx::IntPoint& aPosition) override;
  gfx::IntPoint GetPosition() override;
  void SetTransform(const gfx::Matrix4x4& aTransform) override;
  gfx::Matrix4x4 GetTransform() override;
  gfx::IntRect GetRect() override;
  void SetSamplingFilter(gfx::SamplingFilter aSamplingFilter) override;

  bool IsOpaque() override;
  void SetClipRect(const Maybe<gfx::IntRect>& aClipRect) override;
  Maybe<gfx::IntRect> ClipRect() override;
  void SetRoundedClipRect(const Maybe<gfx::RoundedRect>& aClip) override;
  Maybe<gfx::RoundedRect> RoundedClipRect() override;
  gfx::IntRect CurrentSurfaceDisplayRect() override;
  void SetSurfaceIsFlipped(bool aIsFlipped) override;
  bool SurfaceIsFlipped() override;

  void UpdateLayer(double aScale);
  // TODO
  GpuFence* GetGpuFence() override { return nullptr; }

  RefPtr<widget::WaylandSurface> GetWaylandSurface() { return mSurface; }

  virtual void CommitSurfaceToScreenLocked(
      const MutexAutoLock& aProofOfLock,
      widget::WaylandSurfaceLock& aSurfaceLock) = 0;
  void RemoveAttachedBufferLocked(const MutexAutoLock& aProofOfLock,
                                  widget::WaylandSurfaceLock& aSurfaceLock);

  // Surface Map/Unamp happens on rendering thread.
  //
  // We can use surface right after map but we need to finish mapping
  // on main thread to render it correctly.
  //
  // Also Unmap() needs to be finished on main thread.
  bool IsMapped();
  bool Map(widget::WaylandSurfaceLock* aParentWaylandSurfaceLock);
  void Unmap();

  void UpdateOnMainThread();
  void MainThreadMap();
  void MainThreadUnmap();

  void ForceCommit();

  void PlaceAbove(NativeLayerWayland* aLowerLayer);

#ifdef MOZ_LOGGING
  nsAutoCString GetDebugTag() const;
#endif

  void SetFrameCallbackState(bool aState);

  virtual void DiscardBackbuffersLocked(const MutexAutoLock& aProofOfLock,
                                        bool aForce = false) = 0;
  void DiscardBackbuffers() override;

  NativeLayerWayland(NativeLayerRootWayland* aRootLayer,
                     const gfx::IntSize& aSize, bool aIsOpaque);

  // No need to lock as we used it when new layers are added only
  constexpr static int sLayerClear = 0;
  constexpr static int sLayerRemoved = 1;
  constexpr static int sLayerAdded = 2;

  void MarkClear() { mUsageCount = sLayerClear; }
  void MarkRemoved() { mUsageCount = sLayerRemoved; }
  void MarkAdded() { mUsageCount += sLayerAdded; }

  bool IsRemoved() const { return mUsageCount == sLayerRemoved; }
  bool IsNew() const { return mUsageCount == sLayerAdded; }

  LayerState* State() { return &mState; }

 protected:
  ~NativeLayerWayland();

  Mutex mMutex MOZ_UNANNOTATED;

  // There's a cycle dependency here as NativeLayerRootWayland holds strong
  // reference to NativeLayerWayland and vice versa.
  //
  // Shutdown sequence is:
  //
  // 1) NativeLayerRootWayland is released by GtkCompositorWidget
  // 2) NativeLayerRootWayland calls childs NativeLayerWayland release code and
  //    unrefs them.
  // 3) Child NativeLayerWayland register main thread callback to clean up
  //    and release itself.
  // 4) Child NativeLayerWayland unref itself and parent NativeLayerRootWayland.
  // 5) NativeLayerRootWayland is released when there isn't any
  //    NativeLayerWayland left.
  //
  RefPtr<NativeLayerRootWayland> mRootLayer;

  RefPtr<widget::WaylandSurface> mSurface;

  const bool mIsOpaque = false;

  // Used at SetLayers() when we need to identify removed layers, new layers
  // and layers removed but returned back.
  // We're adding respective constants to mUsageCount for each layer
  // so removed layers have usage count 1, newly added 2 and removed+added 3.
  int mUsageCount = 0;

  gfx::IntSize mSize;
  gfx::IntPoint mPosition;
  gfx::Matrix4x4 mTransform;
  gfx::IntRect mDisplayRect;
  Maybe<gfx::IntRect> mClipRect;
  Maybe<gfx::RoundedRect> mRoundedClipRect;
  gfx::SamplingFilter mSamplingFilter = gfx::SamplingFilter::POINT;
  LayerState mState{};
  bool mSurfaceIsFlipped = false;
  bool mIsHDR = false;

  enum class MainThreadUpdate {
    None,
    Map,
    Unmap,
  };

  // Indicate that we need to finish surface map/unmap
  // on main thread.
  // We need to perform main thread unmap even if mapping on main thread
  // is not finished, some main thread resources are created
  // by WaylandSurface itself.
  Atomic<MainThreadUpdate, mozilla::Relaxed> mNeedsMainThreadUpdate{
      MainThreadUpdate::None};
};

class NativeLayerWaylandRender final : public NativeLayerWayland {
 public:
  NativeLayerWaylandRender* AsNativeLayerWaylandRender() override {
    return this;
  }

  RefPtr<gfx::DrawTarget> NextSurfaceAsDrawTarget(
      const gfx::IntRect& aDisplayRect, const gfx::IntRegion& aUpdateRegion,
      gfx::BackendType aBackendType) override;
  Maybe<GLuint> NextSurfaceAsFramebuffer(const gfx::IntRect& aDisplayRect,
                                         const gfx::IntRegion& aUpdateRegion,
                                         bool aNeedsDepth) override;
  void NotifySurfaceReady() override;
  void AttachExternalImage(wr::RenderTextureHost* aExternalImage) override;
  void CommitSurfaceToScreenLocked(
      const MutexAutoLock& aProofOfLock,
      widget::WaylandSurfaceLock& aSurfaceLock) override;

  NativeLayerWaylandRender(NativeLayerRootWayland* aRootLayer,
                           const gfx::IntSize& aSize, bool aIsOpaque,
                           SurfacePoolHandleWayland* aSurfacePoolHandle);

 private:
  ~NativeLayerWaylandRender() override;

  void DiscardBackbuffersLocked(const MutexAutoLock& aProofOfLock,
                                bool aForce) override;
  void HandlePartialUpdateLocked(const MutexAutoLock& aProofOfLock);

  const RefPtr<SurfacePoolHandleWayland> mSurfacePoolHandle;
  RefPtr<widget::WaylandBuffer> mInProgressBuffer;
  RefPtr<widget::WaylandBuffer> mFrontBuffer;
  gfx::IntRegion mDirtyRegion;
};

class NativeLayerWaylandExternal final : public NativeLayerWayland {
 public:
  // Overridden methods
  NativeLayerWaylandExternal* AsNativeLayerWaylandExternal() override {
    return this;
  }
  RefPtr<gfx::DrawTarget> NextSurfaceAsDrawTarget(
      const gfx::IntRect& aDisplayRect, const gfx::IntRegion& aUpdateRegion,
      gfx::BackendType aBackendType) override;
  Maybe<GLuint> NextSurfaceAsFramebuffer(const gfx::IntRect& aDisplayRect,
                                         const gfx::IntRegion& aUpdateRegion,
                                         bool aNeedsDepth) override;
  void NotifySurfaceReady() override {};
  void AttachExternalImage(wr::RenderTextureHost* aExternalImage) override;
  void CommitSurfaceToScreenLocked(
      const MutexAutoLock& aProofOfLock,
      widget::WaylandSurfaceLock& aSurfaceLock) override;

  NativeLayerWaylandExternal(NativeLayerRootWayland* aRootLayer,
                             bool aIsOpaque);

 private:
  ~NativeLayerWaylandExternal() override;

  void DiscardBackbuffersLocked(const MutexAutoLock& aProofOfLock,
                                bool aForce) override;
  void FreeUnusedBackBuffers();

  bool mBufferInvalided = false;
  RefPtr<wr::RenderDMABUFTextureHost> mTextureHost;
  RefPtr<widget::WaylandBuffer> mFrontBuffer;
};

}  // namespace mozilla::layers

#endif  // mozilla_layers_NativeLayerWayland_h
