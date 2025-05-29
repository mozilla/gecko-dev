/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
  TODO:
  - Better layers update mechanism - update only in changed layes and updated
    properties.
  - Create cache of mapped layers?
  - Fix messages from SurfacePoolWayland() mPendingEntries num xxx
    mPoolSizeLimit 25 Are we leaking pending entries?
  - Implemented screenshotter
  - Presentation feedback
  - Fullscreen - handle differently
  - Attach dmabuf feedback to dmabuf surfaces to get formats for direct scanout
  - Don't use for tooltips/small menus etc.

  Testing:
    Mochitest test speeds
    Fractional Scale
    SW/HW rendering + VSync
*/

#include "mozilla/layers/NativeLayerWayland.h"

#include <dlfcn.h>
#include <utility>
#include <algorithm>

#include "gfxUtils.h"
#include "nsGtkUtils.h"
#include "GLContextProvider.h"
#include "GLBlitHelper.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/SurfacePoolWayland.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/webrender/RenderThread.h"
#include "mozilla/webrender/RenderDMABUFTextureHost.h"
#include "mozilla/widget/WaylandSurface.h"
#include "mozilla/StaticPrefs_widget.h"

#ifdef MOZ_LOGGING
#  undef LOG
#  undef LOGVERBOSE
#  include "mozilla/Logging.h"
#  include "nsTArray.h"
#  include "Units.h"
extern mozilla::LazyLogModule gWidgetCompositorLog;
#  define LOG(str, ...)                                     \
    MOZ_LOG(gWidgetCompositorLog, mozilla::LogLevel::Debug, \
            ("%s: " str, GetDebugTag().get(), ##__VA_ARGS__))
#  define LOGVERBOSE(str, ...)                                \
    MOZ_LOG(gWidgetCompositorLog, mozilla::LogLevel::Verbose, \
            ("%s: " str, GetDebugTag().get(), ##__VA_ARGS__))
#  define LOGS(str, ...)                                    \
    MOZ_LOG(gWidgetCompositorLog, mozilla::LogLevel::Debug, \
            (str, ##__VA_ARGS__))
#else
#  define LOG(args)
#endif /* MOZ_LOGGING */

using namespace mozilla;
using namespace mozilla::widget;

namespace mozilla::layers {

using gfx::BackendType;
using gfx::DrawTarget;
using gfx::IntPoint;
using gfx::IntRect;
using gfx::IntRegion;
using gfx::IntSize;
using gfx::Matrix4x4;
using gfx::Point;
using gfx::Rect;
using gfx::SamplingFilter;
using gfx::Size;

#ifdef MOZ_LOGGING
nsAutoCString NativeLayerRootWayland::GetDebugTag() const {
  nsAutoCString tag;
  tag.AppendPrintf("W[%p]R[%p]", mLoggingWidget, this);
  return tag;
}

nsAutoCString NativeLayerWayland::GetDebugTag() const {
  nsAutoCString tag;
  tag.AppendPrintf("W[%p]R[%p]L[%p]", mRootLayer->GetLoggingWidget(),
                   mRootLayer.get(), this);
  return tag;
}
#endif

/* static */
already_AddRefed<NativeLayerRootWayland> NativeLayerRootWayland::Create(
    RefPtr<WaylandSurface> aWaylandSurface) {
  return MakeAndAddRef<NativeLayerRootWayland>(std::move(aWaylandSurface));
}

void NativeLayerRootWayland::Init() {
  mTmpBuffer = widget::WaylandBufferSHM::Create(LayoutDeviceIntSize(1, 1));

  // Get DRM format for surfaces created by GBM.
  if (!gfx::gfxVars::UseDMABufSurfaceExport()) {
    RefPtr<DMABufFormats> formats = WaylandDisplayGet()->GetDMABufFormats();
    if (formats) {
      mDRMFormat = formats->GetFormat(GBM_FORMAT_ARGB8888,
                                      /* aScanoutFormat */ true);
    }
    if (!mDRMFormat) {
      mDRMFormat = new DRMFormat(GBM_FORMAT_ARGB8888);
    }
  }

  // Unmap all layers if nsWindow is unmapped
  WaylandSurfaceLock lock(mSurface);
  mSurface->SetUnmapCallbackLocked(lock, [this, self = RefPtr{this}]() -> void {
    MutexAutoLock lock(mMutex);
    LOG("NativeLayerRootWayland Unmap callback");
    for (RefPtr<NativeLayerWayland>& layer : mSublayers) {
      if (layer->IsMapped()) {
        layer->Unmap();
        layer->MainThreadUnmap();
      }
    }
  });

  mSurface->SetGdkCommitCallbackLocked(
      lock, [this, self = RefPtr{this}]() -> void {
        LOGVERBOSE("SetGdkCommitCallbackLocked");

        // Try to update on main thread if we need it
        UpdateLayersOnMainThread();

        MutexAutoLock lock(mMutex);
        for (RefPtr<NativeLayerWayland>& layer : mSublayers) {
          layer->ForceCommit();
        }
      });

  // Get the best DMABuf format for root wl_surface. We use the same
  // for child surfaces as we expect them to share the same window/monitor.
  //
  // Using suboptimal format doesn't cause any functional/visual issue
  // but may lead to worse performance as Wayland compositor may need
  // to convert it for direct scanout.
  //
  // TODO: Recreate (Unmap/Map and Dispose buffers) child surfaces
  // if there's format table refresh.
  //
  // Use on nightly only as it's not implemented yet by compositors
  // to get scanout formats for non-fullscreen surfaces.
#ifdef NIGHTLY_BUILD
  if (!gfx::gfxVars::UseDMABufSurfaceExport() &&
      StaticPrefs::widget_dmabuf_feedback_enabled_AtStartup()) {
    mSurface->EnableDMABufFormatsLocked(lock, [this, self = RefPtr{this}](
                                                  DMABufFormats* aFormats) {
      if (DRMFormat* format = aFormats->GetFormat(GBM_FORMAT_ARGB8888,
                                                  /* aScanoutFormat */ true)) {
        LOG("NativeLayerRootWayland DMABuf format refresh: we have scanout "
            "format.");
        mDRMFormat = format;
        return;
      }
      if (DRMFormat* format = aFormats->GetFormat(GBM_FORMAT_ARGB8888,
                                                  /* aScanoutFormat */ false)) {
        LOG("NativeLayerRootWayland DMABuf format refresh: missing scanout "
            "format, use generic one.");
        mDRMFormat = format;
        return;
      }
      LOG("NativeLayerRootWayland DMABuf format refresh: missing DRM "
          "format!");
    });
  }
#endif
}

void NativeLayerRootWayland::Shutdown() {
  LOG("NativeLayerRootWayland::Shutdown()");

  {
    WaylandSurfaceLock lock(mSurface);
    if (mSurface->IsMapped()) {
      mSurface->RemoveAttachedBufferLocked(lock);
    }
    mSurface->ClearUnmapCallbackLocked(lock);
    mSurface->ClearGdkCommitCallbackLocked(lock);
    mSurface->DisableDMABufFormatsLocked(lock);
  }
  mSurface = nullptr;
  mTmpBuffer = nullptr;
  mDRMFormat = nullptr;
}

NativeLayerRootWayland::NativeLayerRootWayland(
    RefPtr<WaylandSurface> aWaylandSurface)
    : mMutex("NativeLayerRootWayland"), mSurface(aWaylandSurface) {
#ifdef MOZ_LOGGING
  mLoggingWidget = mSurface->GetLoggingWidget();
  mSurface->SetLoggingWidget(this);
  LOG("NativeLayerRootWayland::NativeLayerRootWayland() nsWindow [%p] mapped "
      "%d",
      mLoggingWidget, mSurface->IsMapped());
#endif
  if (!WaylandSurface::IsOpaqueRegionEnabled()) {
    NS_WARNING(
        "Wayland opaque region disabled, expect poor rendering performance!");
  }
}

NativeLayerRootWayland::~NativeLayerRootWayland() {
  LOG("NativeLayerRootWayland::~NativeLayerRootWayland()");
  MOZ_DIAGNOSTIC_ASSERT(
      !mSurface, "NativeLayerRootWayland destroyed without Shutdown() call!");
}

#ifdef MOZ_LOGGING
void* NativeLayerRootWayland::GetLoggingWidget() const {
  return mLoggingWidget;
}
#endif

// Create layer for rendering to layer/surface so get blank one from
// surface pool.
already_AddRefed<NativeLayer> NativeLayerRootWayland::CreateLayer(
    const IntSize& aSize, bool aIsOpaque,
    SurfacePoolHandle* aSurfacePoolHandle) {
  LOG("NativeLayerRootWayland::CreateLayer() [%d x %d] nsWindow [%p] opaque %d",
      aSize.width, aSize.height, GetLoggingWidget(), aIsOpaque);
  return MakeAndAddRef<NativeLayerWaylandRender>(
      this, aSize, aIsOpaque, aSurfacePoolHandle->AsSurfacePoolHandleWayland());
}

already_AddRefed<NativeLayer>
NativeLayerRootWayland::CreateLayerForExternalTexture(bool aIsOpaque) {
  LOG("NativeLayerRootWayland::CreateLayerForExternalTexture() nsWindow [%p] "
      "opaque %d",
      GetLoggingWidget(), aIsOpaque);
  return MakeAndAddRef<NativeLayerWaylandExternal>(this, aIsOpaque);
}

void NativeLayerRootWayland::AppendLayer(NativeLayer* aLayer) {
  MOZ_CRASH("NativeLayerRootWayland::AppendLayer() not implemented.");
}

void NativeLayerRootWayland::RemoveLayer(NativeLayer* aLayer) {
  MOZ_CRASH("NativeLayerRootWayland::RemoveLayer() not implemented.");
}

bool NativeLayerRootWayland::IsEmptyLocked(const MutexAutoLock& aProofOfLock) {
  return mSublayers.IsEmpty();
}

void NativeLayerRootWayland::SetLayers(
    const nsTArray<RefPtr<NativeLayer>>& aLayers) {
  // Removing all layers can destroy us so hold ref
  RefPtr<NativeLayerRoot> kungfuDeathGrip = this;

  MutexAutoLock lock(mMutex);

  // Take shortcut if all layers are removed
  if (aLayers.IsEmpty()) {
    LOG("NativeLayerRootWayland::SetLayers() clear layers");
    for (const RefPtr<NativeLayerWayland>& layer : mSublayers) {
      LOG("  Unmap removed child layer [%p]", layer.get());
      layer->Unmap();
    }
    mMainThreadUpdateSublayers.AppendElements(std::move(mSublayers));
    RequestUpdateOnMainThreadLocked(lock);
    return;
  }

  nsTArray<RefPtr<NativeLayerWayland>> newLayers(aLayers.Length());
  for (const RefPtr<NativeLayer>& sublayer : aLayers) {
    RefPtr<NativeLayerWayland> layer = sublayer->AsNativeLayerWayland();
    layer->MarkClear();
    newLayers.AppendElement(std::move(layer));
  }

  if (newLayers == mSublayers) {
    return;
  }

  LOG("NativeLayerRootWayland::SetLayers(), old layers num %d new layers num "
      "%d",
      (int)mSublayers.Length(), (int)aLayers.Length());

  // newLayers (aLayers) is a mix of old (already used) and new layers.
  // We need to go through recent layers and remove the ones missing
  // in new layers.
  for (const RefPtr<NativeLayerWayland>& layer : mSublayers) {
    layer->MarkRemoved();
  }
  for (const RefPtr<NativeLayerWayland>& layer : newLayers) {
    layer->MarkAdded();
  }

  for (const RefPtr<NativeLayerWayland>& layer : mSublayers) {
    if (layer->IsRemoved()) {
      LOG("  Unmap removed child layer [%p]", layer.get());
      layer->Unmap();
      mMainThreadUpdateSublayers.AppendElement(layer);
    }
  }

  // Map newly added layers only if root surface itself is mapped.
  // We lock it to make sure root surface stays mapped.
  WaylandSurfaceLock surfaceLock(mSurface, /* force commit */ true);
  if (mSurface->IsMapped()) {
    for (const RefPtr<NativeLayerWayland>& layer : newLayers) {
      if (layer->IsNew()) {
        LOG("  Map new child layer [%p]", layer.get());
        if (!layer->Map(&surfaceLock)) {
          continue;
        }
        if (layer->IsOpaque() && WaylandSurface::IsOpaqueRegionEnabled()) {
          LOG("  adding new opaque layer [%p]", layer.get());
          mMainThreadUpdateSublayers.AppendElement(layer);
        }
      }
    }
  }

  mSublayers = std::move(newLayers);
  mNeedsLayerUpdate = true;

  // We need to process a part of map event on main thread as we use Gdk
  // code there. Ask for the processing now.
  RequestUpdateOnMainThreadLocked(lock);
}

// Update layers on main thread. Missing the main thread update is not critical
// but may lead to worse performance as we tell Gdk to skip compositing opaque
// surfaces.
void NativeLayerRootWayland::UpdateLayersOnMainThread() {
  AssertIsOnMainThread();
  LOG("NativeLayerRootWayland::UpdateLayersOnMainThread()");
  MutexAutoLock lock(mMutex);
  for (const RefPtr<NativeLayerWayland>& layer : mMainThreadUpdateSublayers) {
    layer->UpdateOnMainThread();
  }
  mMainThreadUpdateSublayers.Clear();
  mMainThreadUpdateQueued = false;
}

void NativeLayerRootWayland::RequestUpdateOnMainThreadLocked(
    const MutexAutoLock& aProofOfLock) {
  if (!mMainThreadUpdateSublayers.Length() || mMainThreadUpdateQueued) {
    return;
  }
  mMainThreadUpdateQueued = true;

  LOG("NativeLayerRootWayland::RequestUpdateOnMainThreadLocked()");
  nsCOMPtr<nsIRunnable> updateLayersRunnable = NewRunnableMethod<>(
      "layers::NativeLayerRootWayland::UpdateLayersOnMainThread", this,
      &NativeLayerRootWayland::UpdateLayersOnMainThread);
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThreadQueue(
      updateLayersRunnable.forget(), EventQueuePriority::Normal));
}

// Process all active layers, update position/scale and commit them.
// TODO: Process only changed ones
bool NativeLayerRootWayland::UpdateLayersLocked(
    const MutexAutoLock& aProofOfLock) {
  if (mNeedsLayerUpdate) {
    NativeLayerWayland* previousWaylandSurface = nullptr;
    for (RefPtr<NativeLayerWayland>& layer : mSublayers) {
      if (previousWaylandSurface) {
        layer->PlaceAbove(previousWaylandSurface);
      }
      previousWaylandSurface = layer;
    }
    mNeedsLayerUpdate = false;
  }

  // scale < 1 means we're missing any scale info (even from monitor).
  // Use default scale in such case.
  double scale = mSurface->GetScale();
  if (scale < 1) {
    scale = 1.0;
  }

  // TODO: Do recalc only if there's a change
  for (RefPtr<NativeLayerWayland>& layer : mSublayers) {
    layer->UpdateLayer(scale);
  }
  return true;
}

#ifdef MOZ_LOGGING
void NativeLayerRootWayland::LogStatsLocked(const MutexAutoLock& aProofOfLock) {
  if (!MOZ_LOG_TEST(gWidgetCompositorLog, mozilla::LogLevel::Verbose)) {
    return;
  }

  int layersNum = 0;
  int layersMapped = 0;
  int layersMappedOpaque = 0;
  int layersMappedOpaqueSet = 0;
  int layersBufferAttached = 0;
  int layersVisible = 0;

  for (RefPtr<NativeLayerWayland>& layer : mSublayers) {
    layersNum++;
    if (layer->IsMapped()) {
      layersMapped++;
    }
    if (layer->GetWaylandSurface()->HasBufferAttached()) {
      layersBufferAttached++;
    }
    if (layer->IsMapped() && layer->IsOpaque()) {
      layersMappedOpaque++;
      if (layer->GetWaylandSurface()->IsOpaqueSurfaceHandlerSet()) {
        layersMappedOpaqueSet++;
      }
    }
    if (layer->State()->mIsVisible) {
      layersVisible++;
    }
  }
  LOGVERBOSE(
      "Layers [%d] mapped [%d] attached [%d] visible [%d] opaque [%d] opaque "
      "set [%d]",
      layersNum, layersMapped, layersBufferAttached, layersVisible,
      layersMappedOpaque, layersMappedOpaqueSet);
}
#endif

bool NativeLayerRootWayland::CommitToScreen() {
  MutexAutoLock lock(mMutex);
  return CommitToScreenLocked(lock);
}

bool NativeLayerRootWayland::CommitToScreenLocked(
    const MutexAutoLock& aProofOfLock) {
  mFrameInProcess = false;

  // Lock root surface to make sure it stays mapped while we're processing
  // child surfaces.
  WaylandSurfaceLock surfaceLock(mSurface, /* force commit */ true);
  if (!mSurface->IsMapped()) {
    // TODO: Register frame callback to paint again? Are we hidden?
    LOG("NativeLayerRootWayland::CommitToScreen() root surface is not mapped");
    return false;
  }

  LOG("NativeLayerRootWayland::CommitToScreen()");

  // Attach empty tmp buffer to root layer (nsWindow).
  // We need to have any content to attach child layers to it.
  if (!mSurface->HasBufferAttached()) {
    mSurface->AttachLocked(surfaceLock, mTmpBuffer);
    mSurface->ClearOpaqueRegionLocked(surfaceLock);
  }

  // Try to map all missing surfaces
  for (RefPtr<NativeLayerWayland>& layer : mSublayers) {
    if (!layer->IsMapped() && layer->Map(&surfaceLock)) {
      if (layer->IsOpaque() && WaylandSurface::IsOpaqueRegionEnabled()) {
        mMainThreadUpdateSublayers.AppendElement(layer);
      }
      mNeedsLayerUpdate = true;
    }
  }

  if (mNeedsLayerUpdate) {
    RequestUpdateOnMainThreadLocked(aProofOfLock);
  }

  // Update layers position / scale / size
  if (!UpdateLayersLocked(aProofOfLock)) {
    return false;
  }

#ifdef MOZ_LOGGING
  LogStatsLocked(aProofOfLock);
#endif

  return true;
}

// Ready-to-paint signal from root or child surfaces. Route it to
// root WaylandSurface (owned by nsWindow) where it's used to fire VSync.
void NativeLayerRootWayland::FrameCallbackHandler(uint32_t aTime) {
  {
    // Child layer wl_subsurface already requested next frame callback
    // and we need to commit to root surface too as we're in
    // wl_subsurface synced mode.
    WaylandSurfaceLock lock(mSurface, /* force commit */ true);
  }

  if (aTime <= mLastFrameCallbackTime) {
    LOGVERBOSE(
        "NativeLayerRootWayland::FrameCallbackHandler() ignoring redundant "
        "callback %d",
        aTime);
    return;
  }
  mLastFrameCallbackTime = aTime;

  LOGVERBOSE("NativeLayerRootWayland::FrameCallbackHandler() time %d", aTime);
  mSurface->FrameCallbackHandler(nullptr, aTime,
                                 /* aRoutedFromChildSurface */ true);
}

// We don't need to lock access to GdkWindow() as we process all Gdk/Gtk
// events on main thread only.
GdkWindow* NativeLayerRootWayland::GetGdkWindow() const {
  AssertIsOnMainThread();
  return mSurface->GetGdkWindow();
}

// Try to match stored wl_buffer with provided DMABufSurface or create
// a new one.
RefPtr<WaylandBuffer> NativeLayerRootWayland::BorrowExternalBuffer(
    RefPtr<DMABufSurface> aDMABufSurface) {
  LOG("NativeLayerRootWayland::BorrowExternalBuffer() WaylandSurface [%p] UID "
      "%d PID %d",
      aDMABufSurface.get(), aDMABufSurface->GetUID(), aDMABufSurface->GetPID());

  RefPtr waylandBuffer =
      widget::WaylandBufferDMABUF::CreateExternal(aDMABufSurface);
  for (auto& b : mExternalBuffers) {
    if (b.Matches(aDMABufSurface)) {
      waylandBuffer->SetExternalWLBuffer(b.GetWLBuffer());
      return waylandBuffer.forget();
    }
  }

  wl_buffer* wlbuffer = waylandBuffer->CreateAndTakeWLBuffer();
  if (!wlbuffer) {
    return nullptr;
  }

  mExternalBuffers.EmplaceBack(aDMABufSurface, wlbuffer);
  return waylandBuffer.forget();
}

NativeLayerWayland::NativeLayerWayland(NativeLayerRootWayland* aRootLayer,
                                       const IntSize& aSize, bool aIsOpaque)
    : mMutex("NativeLayerWayland"),
      mRootLayer(aRootLayer),
      mIsOpaque(aIsOpaque),
      mSize(aSize) {
  mSurface = new WaylandSurface(mRootLayer->GetWaylandSurface(), mSize);
#ifdef MOZ_LOGGING
  mSurface->SetLoggingWidget(this);
#endif
  LOG("NativeLayerWayland::NativeLayerWayland() WaylandSurface [%p] size [%d, "
      "%d] opaque %d",
      mSurface.get(), mSize.width, mSize.height, aIsOpaque);
}

NativeLayerWayland::~NativeLayerWayland() {
  LOG("NativeLayerWayland::~NativeLayerWayland() IsMapped %d",
      mSurface->IsMapped());
  MOZ_RELEASE_ASSERT(!mSurface->IsMapped(), "Releasing mapped surface!");
}

bool NativeLayerWayland::IsMapped() { return mSurface->IsMapped(); }

void NativeLayerWayland::SetSurfaceIsFlipped(bool aIsFlipped) {
  MutexAutoLock lock(mMutex);
  if (aIsFlipped != mSurfaceIsFlipped) {
    mSurfaceIsFlipped = aIsFlipped;
  }
}

bool NativeLayerWayland::SurfaceIsFlipped() {
  MutexAutoLock lock(mMutex);
  return mSurfaceIsFlipped;
}

IntSize NativeLayerWayland::GetSize() {
  MutexAutoLock lock(mMutex);
  return mSize;
}

void NativeLayerWayland::SetPosition(const IntPoint& aPosition) {
  MutexAutoLock lock(mMutex);
  if (aPosition != mPosition) {
    LOG("NativeLayerWayland::SetPosition() [%d, %d]", (int)aPosition.x,
        (int)aPosition.y);
    mPosition = aPosition;
  }
}

IntPoint NativeLayerWayland::GetPosition() {
  MutexAutoLock lock(mMutex);
  return mPosition;
}

void NativeLayerWayland::PlaceAbove(NativeLayerWayland* aLowerLayer) {
  MOZ_DIAGNOSTIC_ASSERT(IsMapped());
  MOZ_DIAGNOSTIC_ASSERT(aLowerLayer->IsMapped());
  MOZ_DIAGNOSTIC_ASSERT(this != aLowerLayer);

  WaylandSurfaceLock lock(mSurface);
  WaylandSurfaceLock lowerSurfacelock(aLowerLayer->mSurface);
  mSurface->PlaceAboveLocked(lock, lowerSurfacelock);
}

void NativeLayerWayland::SetTransform(const Matrix4x4& aTransform) {
  MutexAutoLock lock(mMutex);
  MOZ_DIAGNOSTIC_ASSERT(aTransform.IsRectilinear());
  if (aTransform != mTransform) {
    mTransform = aTransform;
  }
}

void NativeLayerWayland::SetSamplingFilter(SamplingFilter aSamplingFilter) {
  MutexAutoLock lock(mMutex);
  if (aSamplingFilter != mSamplingFilter) {
    mSamplingFilter = aSamplingFilter;
  }
}

Matrix4x4 NativeLayerWayland::GetTransform() {
  MutexAutoLock lock(mMutex);
  return mTransform;
}

IntRect NativeLayerWayland::GetRect() {
  MutexAutoLock lock(mMutex);
  return IntRect(mPosition, mSize);
}

bool NativeLayerWayland::IsOpaque() {
  MutexAutoLock lock(mMutex);
  return mIsOpaque;
}

void NativeLayerWayland::SetClipRect(const Maybe<IntRect>& aClipRect) {
  MutexAutoLock lock(mMutex);
  if (aClipRect != mClipRect) {
#if MOZ_LOGGING
    if (aClipRect) {
      gfx::IntRect rect(aClipRect.value());
      LOG("NativeLayerWaylandRender::SetClipRect() [%d,%d] -> [%d x %d]",
          rect.x, rect.y, rect.width, rect.height);
    }
#endif
    mClipRect = aClipRect;
  }
}

Maybe<IntRect> NativeLayerWayland::ClipRect() {
  MutexAutoLock lock(mMutex);
  return mClipRect;
}

IntRect NativeLayerWayland::CurrentSurfaceDisplayRect() {
  MutexAutoLock lock(mMutex);
  return mDisplayRect;
}

void NativeLayerWayland::UpdateLayer(double aScale) {
  MOZ_DIAGNOSTIC_ASSERT(aScale > 0);
  MOZ_DIAGNOSTIC_ASSERT(IsMapped());

  MutexAutoLock lock(mMutex);

  MOZ_RELEASE_ASSERT(mTransform.Is2D());
  auto transform2D = mTransform.As2D();

  Rect surfaceRectClipped = Rect(0, 0, (float)mSize.width, (float)mSize.height);
  surfaceRectClipped = surfaceRectClipped.Intersect(Rect(mDisplayRect));

  transform2D.PostTranslate((float)mPosition.x, (float)mPosition.y);
  surfaceRectClipped = transform2D.TransformBounds(surfaceRectClipped);

  if (mClipRect) {
    surfaceRectClipped = surfaceRectClipped.Intersect(Rect(mClipRect.value()));
  }

  WaylandSurfaceLock surfaceLock(mSurface);
  mState.mIsVisible = (roundf(surfaceRectClipped.width) > 0 &&
                       roundf(surfaceRectClipped.height) > 0);
  if (mState.mIsVisible) {
    LOGVERBOSE("NativeLayerWayland::UpdateLayer() is visible");

    mSurface->SetTransformFlippedLocked(surfaceLock, transform2D._11 < 0.0,
                                        transform2D._22 < 0.0);
    gfx::IntPoint pos((int)roundf(surfaceRectClipped.x),
                      (int)roundf(surfaceRectClipped.y));

    // Only integer scale is supported right now
    int scale = (int)roundf(aScale);
    if (pos.x % scale || pos.y % scale) {
      NS_WARNING(
          "NativeLayerWayland: Tile position doesn't match scale, rendering "
          "glitches ahead!");
    }

    mSurface->MoveLocked(surfaceLock,
                         gfx::IntPoint(pos.x / scale, pos.y / scale));

    gfx::IntSize size((int)roundf(surfaceRectClipped.width),
                      (int)roundf(surfaceRectClipped.height));
    if (size.width % scale || size.height % scale) {
      NS_WARNING(
          "NativeLayerWayland: Tile size doesn't match scale, rendering "
          "glitches ahead!");
    }
    mSurface->SetViewPortDestLocked(
        surfaceLock, gfx::IntSize(size.width / scale, size.height / scale));

    auto transform2DInversed = transform2D.Inverse();
    Rect bufferClip = transform2DInversed.TransformBounds(surfaceRectClipped);
    mSurface->SetViewPortSourceRectLocked(
        surfaceLock,
        bufferClip.Intersect(Rect(0, 0, mSize.width, mSize.height)));

    CommitSurfaceToScreenLocked(lock, surfaceLock);
  } else {
    LOGVERBOSE("NativeLayerWayland::UpdateLayer() is hidden");
    RemoveAttachedBufferLocked(lock, surfaceLock);
  }
}

void NativeLayerWayland::RemoveAttachedBufferLocked(
    const MutexAutoLock& aProofOfLock, WaylandSurfaceLock& aSurfaceLock) {
  LOG("NativeLayerWayland::RemoveAttachedBufferLocked()");
  mSurface->RemoveAttachedBufferLocked(aSurfaceLock);
}

bool NativeLayerWayland::Map(WaylandSurfaceLock* aParentWaylandSurfaceLock) {
  MutexAutoLock lock(mMutex);

  if (mNeedsMainThreadUpdate == MainThreadUpdate::Unmap) {
    LOG("NativeLayerWayland::Map() waiting to MainThreadUpdate::Unmap");
    return false;
  }

  LOG("NativeLayerWayland::Map() parent %p", mRootLayer.get());

  WaylandSurfaceLock surfaceLock(mSurface);
  MOZ_DIAGNOSTIC_ASSERT(!mSurface->IsMapped());
  MOZ_DIAGNOSTIC_ASSERT(mNeedsMainThreadUpdate != MainThreadUpdate::Map);

  if (!mSurface->MapLocked(surfaceLock, aParentWaylandSurfaceLock,
                           gfx::IntPoint(0, 0))) {
    LOG("NativeLayerWayland::Map() failed!");
    return false;
  }
  mSurface->DisableUserInputLocked(surfaceLock);
  mSurface->CreateViewportLocked(surfaceLock,
                                 /* aFollowsSizeChanges */ false);

  // Route frame-to-paint (frame callback) from child layer to root layer
  // where it's passed to Vsync.
  //
  // aTime param is used to identify duplicate events.
  //
  mSurface->AddPersistentFrameCallbackLocked(
      surfaceLock,
      [this, self = RefPtr{this}](wl_callback* aCallback,
                                  uint32_t aTime) -> void {
        LOGVERBOSE("NativeLayerWayland::FrameCallbackHandler() time %d", aTime);
        mRootLayer->FrameCallbackHandler(aTime);
      },
      /* aEmulateFrameCallback */ true);

  if (mIsHDR) {
    mSurface->EnableColorManagementLocked(surfaceLock);
  }

  mNeedsMainThreadUpdate = MainThreadUpdate::Map;
  return true;
}

void NativeLayerWayland::MainThreadMap() {
  AssertIsOnMainThread();
  MOZ_DIAGNOSTIC_ASSERT(IsOpaque());
  if (!mSurface->IsOpaqueSurfaceHandlerSet()) {
    WaylandSurfaceLock lock(mSurface);
    // Don't register commit handler, we do it for all surfaces at
    // GdkCommitCallback() handler.
    mSurface->AddOpaqueSurfaceHandlerLocked(lock, mRootLayer->GetGdkWindow(),
                                            /* aRegisterCommitHandler */ false);
    mSurface->SetOpaqueLocked(lock);
    mNeedsMainThreadUpdate = MainThreadUpdate::None;
  }
}

void NativeLayerWayland::Unmap() {
  if (!mSurface->IsMapped()) {
    return;
  }

  MutexAutoLock lock(mMutex);
  LOG("NativeLayerWayland::Unmap()");

  WaylandSurfaceLock surfaceLock(mSurface);
  mSurface->UnmapLocked(surfaceLock);

  mNeedsMainThreadUpdate = MainThreadUpdate::Unmap;
}

void NativeLayerWayland::MainThreadUnmap() {
  MOZ_DIAGNOSTIC_ASSERT(!mSurface->IsMapped());
  AssertIsOnMainThread();
  if (mSurface->IsPendingGdkCleanup()) {
    WaylandSurfaceLock lock(mSurface);
    mSurface->GdkCleanUpLocked(lock);
    // TODO: Do we need to clear opaque region?
  }
  mNeedsMainThreadUpdate = MainThreadUpdate::None;
}

void NativeLayerWayland::UpdateOnMainThread() {
  AssertIsOnMainThread();
  if (mNeedsMainThreadUpdate == MainThreadUpdate::None) {
    return;
  }
  if (mNeedsMainThreadUpdate == MainThreadUpdate::Map) {
    MainThreadMap();
  } else {
    MainThreadUnmap();
  }
}

void NativeLayerWayland::DiscardBackbuffers() {
  MutexAutoLock lock(mMutex);
  DiscardBackbuffersLocked(lock);
}

void NativeLayerWayland::ForceCommit() {
  if (mSurface->IsMapped()) {
    WaylandSurfaceLock lock(mSurface);
    mSurface->CommitLocked(lock, /* force commit */ true);
  }
}

NativeLayerWaylandRender::NativeLayerWaylandRender(
    NativeLayerRootWayland* aRootLayer, const IntSize& aSize, bool aIsOpaque,
    SurfacePoolHandleWayland* aSurfacePoolHandle)
    : NativeLayerWayland(aRootLayer, aSize, aIsOpaque),
      mSurfacePoolHandle(aSurfacePoolHandle) {
  MOZ_RELEASE_ASSERT(mSurfacePoolHandle,
                     "Need a non-null surface pool handle.");
}

void NativeLayerWaylandRender::AttachExternalImage(
    wr::RenderTextureHost* aExternalImage) {
  MOZ_RELEASE_ASSERT(
      false,
      "NativeLayerWaylandRender::AttachExternalImage() not implemented.");
}

RefPtr<DrawTarget> NativeLayerWaylandRender::NextSurfaceAsDrawTarget(
    const IntRect& aDisplayRect, const IntRegion& aUpdateRegion,
    BackendType aBackendType) {
  MutexAutoLock lock(mMutex);

  LOG("NativeLayerWaylandRender::NextSurfaceAsDrawTarget()");

  mDisplayRect = IntRect(aDisplayRect);
  mDirtyRegion = IntRegion(aUpdateRegion);

  MOZ_DIAGNOSTIC_ASSERT(!mInProgressBuffer);
  if (mFrontBuffer && !mFrontBuffer->IsAttached()) {
    // the Wayland compositor released the buffer early, we can reuse it
    mInProgressBuffer = std::move(mFrontBuffer);
  } else {
    mInProgressBuffer = mSurfacePoolHandle->ObtainBufferFromPool(
        mSize, mRootLayer->GetDRMFormat());
    if (mFrontBuffer) {
      HandlePartialUpdateLocked(lock);
      mSurfacePoolHandle->ReturnBufferToPool(mFrontBuffer);
    }
  }
  mFrontBuffer = nullptr;

  if (!mInProgressBuffer) {
    gfxCriticalError() << "Failed to obtain buffer";
    wr::RenderThread::Get()->HandleWebRenderError(
        wr::WebRenderError::NEW_SURFACE);
    return nullptr;
  }

  return mInProgressBuffer->Lock();
}

Maybe<GLuint> NativeLayerWaylandRender::NextSurfaceAsFramebuffer(
    const IntRect& aDisplayRect, const IntRegion& aUpdateRegion,
    bool aNeedsDepth) {
  MutexAutoLock lock(mMutex);

  LOG("NativeLayerWaylandRender::NextSurfaceAsFramebuffer()");

  mDisplayRect = IntRect(aDisplayRect);
  mDirtyRegion = IntRegion(aUpdateRegion);

  MOZ_DIAGNOSTIC_ASSERT(!mInProgressBuffer);
  if (mFrontBuffer && !mFrontBuffer->IsAttached()) {
    // the Wayland compositor released the buffer early, we can reuse it
    mInProgressBuffer = std::move(mFrontBuffer);
    mFrontBuffer = nullptr;
  } else {
    mInProgressBuffer = mSurfacePoolHandle->ObtainBufferFromPool(
        mSize, mRootLayer->GetDRMFormat());
  }

  if (!mInProgressBuffer) {
    gfxCriticalError() << "Failed to obtain buffer";
    wr::RenderThread::Get()->HandleWebRenderError(
        wr::WebRenderError::NEW_SURFACE);
    return Nothing();
  }

  // get the framebuffer before handling partial damage so we don't accidently
  // create one without depth buffer
  Maybe<GLuint> fbo = mSurfacePoolHandle->GetFramebufferForBuffer(
      mInProgressBuffer, aNeedsDepth);
  MOZ_RELEASE_ASSERT(fbo, "GetFramebufferForBuffer failed.");

  if (mFrontBuffer) {
    HandlePartialUpdateLocked(lock);
    mSurfacePoolHandle->ReturnBufferToPool(mFrontBuffer);
    mFrontBuffer = nullptr;
  }

  return fbo;
}

void NativeLayerWaylandRender::HandlePartialUpdateLocked(
    const MutexAutoLock& aProofOfLock) {
  IntRegion copyRegion = IntRegion(mDisplayRect);
  copyRegion.SubOut(mDirtyRegion);

  LOG("NativeLayerWaylandRender::HandlePartialUpdateLocked()");

  if (!copyRegion.IsEmpty()) {
    if (mSurfacePoolHandle->gl()) {
      mSurfacePoolHandle->gl()->MakeCurrent();
      for (auto iter = copyRegion.RectIter(); !iter.Done(); iter.Next()) {
        gfx::IntRect r = iter.Get();
        Maybe<GLuint> sourceFB =
            mSurfacePoolHandle->GetFramebufferForBuffer(mFrontBuffer, false);
        Maybe<GLuint> destFB = mSurfacePoolHandle->GetFramebufferForBuffer(
            mInProgressBuffer, false);
        MOZ_RELEASE_ASSERT(sourceFB && destFB);
        mSurfacePoolHandle->gl()->BlitHelper()->BlitFramebufferToFramebuffer(
            sourceFB.value(), destFB.value(), r, r, LOCAL_GL_NEAREST);
      }
    } else {
      RefPtr<gfx::DataSourceSurface> dataSourceSurface =
          gfx::CreateDataSourceSurfaceFromData(
              mSize, mFrontBuffer->GetSurfaceFormat(),
              (const uint8_t*)mFrontBuffer->GetImageData(),
              mSize.width * BytesPerPixel(mFrontBuffer->GetSurfaceFormat()));
      RefPtr<DrawTarget> dt = mInProgressBuffer->Lock();

      for (auto iter = copyRegion.RectIter(); !iter.Done(); iter.Next()) {
        IntRect r = iter.Get();
        dt->CopySurface(dataSourceSurface, r, IntPoint(r.x, r.y));
      }
    }
  }
}

void NativeLayerWaylandRender::CommitSurfaceToScreenLocked(
    const MutexAutoLock& aProofOfLock, WaylandSurfaceLock& aSurfaceLock) {
  if (!mFrontBuffer) {
    LOG("NativeLayerWaylandRender::CommitSurfaceToScreenLocked() - missing "
        "front buffer!");
    return;
  }
  if (mDirtyRegion.IsEmpty() && mSurface->HasBufferAttached()) {
    return;
  }

  LOG("NativeLayerWaylandRender::CommitSurfaceToScreenLocked()");
  mSurface->InvalidateRegionLocked(aSurfaceLock, mDirtyRegion);
  mDirtyRegion.SetEmpty();

  mSurface->AttachLocked(aSurfaceLock, mFrontBuffer);
}

void NativeLayerWaylandRender::NotifySurfaceReady() {
  LOG("NativeLayerWaylandRender::NotifySurfaceReady()");
  MutexAutoLock lock(mMutex);
  MOZ_DIAGNOSTIC_ASSERT(!mFrontBuffer);
  MOZ_DIAGNOSTIC_ASSERT(mInProgressBuffer);
  mFrontBuffer = std::move(mInProgressBuffer);
  if (mSurfacePoolHandle->gl()) {
    mSurfacePoolHandle->gl()->FlushIfHeavyGLCallsSinceLastFlush();
  }
}

void NativeLayerWaylandRender::DiscardBackbuffersLocked(
    const MutexAutoLock& aProofOfLock, bool aForce) {
  LOG("NativeLayerWaylandRender::DiscardBackbuffersLocked()");

  if (mInProgressBuffer && (!mInProgressBuffer->IsAttached() || aForce)) {
    mSurfacePoolHandle->ReturnBufferToPool(mInProgressBuffer);
    mInProgressBuffer = nullptr;
  }
  if (mFrontBuffer && (mFrontBuffer->IsAttached() || aForce)) {
    mSurfacePoolHandle->ReturnBufferToPool(mFrontBuffer);
    mFrontBuffer = nullptr;
  }
}

NativeLayerWaylandRender::~NativeLayerWaylandRender() {
  LOG("NativeLayerWaylandRender::~NativeLayerWaylandRender()");

  MutexAutoLock lock(mMutex);
  DiscardBackbuffersLocked(lock, /* aForce */ true);
}

NativeLayerWaylandExternal::NativeLayerWaylandExternal(
    NativeLayerRootWayland* aRootLayer, bool aIsOpaque)
    : NativeLayerWayland(aRootLayer, IntSize(), aIsOpaque) {}

void NativeLayerWaylandExternal::AttachExternalImage(
    wr::RenderTextureHost* aExternalImage) {
  MutexAutoLock lock(mMutex);

  wr::RenderDMABUFTextureHost* texture =
      aExternalImage->AsRenderDMABUFTextureHost();
  MOZ_DIAGNOSTIC_ASSERT(texture);
  if (!texture) {
    LOG("NativeLayerWayland::AttachExternalImage() failed.");
    gfxCriticalNoteOnce << "ExternalImage is not RenderDMABUFTextureHost";
    return;
  }

  if (mTextureHost && mTextureHost->GetSurface() == texture->GetSurface()) {
    return;
  }
  mTextureHost = texture;
  mSize = texture->GetSize(0);
  mDisplayRect = IntRect(IntPoint{}, mSize);
  mBufferInvalided = true;

  auto surface = mTextureHost->GetSurface();
  mFrontBuffer = surface->CanRecycle()
                     ? mRootLayer->BorrowExternalBuffer(surface)
                     : widget::WaylandBufferDMABUF::CreateExternal(surface);
  mIsHDR = surface->IsHDRSurface();

  LOG("NativeLayerWaylandExternal::AttachExternalImage() host [%p] "
      "DMABufSurface [%p] DMABuf UID %d [%d x %d] HDR %d Opaque %d",
      mTextureHost.get(), mTextureHost->GetSurface().get(),
      mTextureHost->GetSurface()->GetUID(), mSize.width, mSize.height, mIsHDR,
      mIsOpaque);
}

void NativeLayerWaylandExternal::DiscardBackbuffersLocked(
    const MutexAutoLock& aProofOfLock, bool aForce) {
  LOG("NativeLayerWaylandRender::DiscardBackbuffersLocked()");

  // Buffers attached to compositor are still tracked by WaylandSurface
  // so we can release reference here.
  mTextureHost = nullptr;
  mFrontBuffer = nullptr;
}

RefPtr<DrawTarget> NativeLayerWaylandExternal::NextSurfaceAsDrawTarget(
    const IntRect& aDisplayRect, const IntRegion& aUpdateRegion,
    BackendType aBackendType) {
  MOZ_RELEASE_ASSERT(
      false,
      "NativeLayerWaylandExternal::NextSurfaceAsDrawTarget() not implemented!");
  return nullptr;
}

Maybe<GLuint> NativeLayerWaylandExternal::NextSurfaceAsFramebuffer(
    const IntRect& aDisplayRect, const IntRegion& aUpdateRegion,
    bool aNeedsDepth) {
  MOZ_RELEASE_ASSERT(false,
                     "NativeLayerWaylandExternal::NextSurfaceAsFramebuffer() "
                     "not implemented!");
  return Nothing();
}

void NativeLayerWaylandExternal::CommitSurfaceToScreenLocked(
    const MutexAutoLock& aProofOfLock, WaylandSurfaceLock& aSurfaceLock) {
  if (!mFrontBuffer) {
    LOG("NativeLayerWaylandExternal::CommitSurfaceToScreenLocked() - missing "
        "front buffer!");
    return;
  }

  if (mBufferInvalided) {
    LOG("NativeLayerWaylandExternal::CommitSurfaceToScreenLocked()");
    mSurface->InvalidateLocked(aSurfaceLock);
    mSurface->AttachLocked(aSurfaceLock, mFrontBuffer);
    mBufferInvalided = false;
  }
}

NativeLayerWaylandExternal::~NativeLayerWaylandExternal() {
  LOG("NativeLayerWaylandExternal::~NativeLayerWaylandExternal()");
}

}  // namespace mozilla::layers
