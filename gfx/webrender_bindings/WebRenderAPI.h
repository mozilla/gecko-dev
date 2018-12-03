/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_WEBRENDERAPI_H
#define MOZILLA_LAYERS_WEBRENDERAPI_H

#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/gfx/CompositorHitTestInfo.h"
#include "mozilla/layers/IpcResourceUpdateQueue.h"
#include "mozilla/layers/ScrollableLayerGuid.h"
#include "mozilla/layers/SyncObject.h"
#include "mozilla/Range.h"
#include "mozilla/webrender/webrender_ffi.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "GLTypes.h"
#include "Units.h"

class nsDisplayItem;

namespace mozilla {

struct ActiveScrolledRoot;

namespace widget {
class CompositorWidget;
}

namespace layers {
class CompositorBridgeParent;
class WebRenderBridgeParent;
class WebRenderLayerManager;
}  // namespace layers

namespace layout {
class TextDrawTarget;
}

namespace wr {

class DisplayListBuilder;
class RendererOGL;
class RendererEvent;

// This isn't part of WR's API, but we define it here to simplify layout's
// logic and data plumbing.
struct Line {
  wr::LayoutRect bounds;
  float wavyLineThickness;
  wr::LineOrientation orientation;
  wr::ColorF color;
  wr::LineStyle style;
};

/// A handler that can be bundled into a transaction and notified at specific
/// points in the rendering pipeline, such as after scene building or after
/// frame building.
///
/// If for any reason the handler is dropped before reaching the requested
/// point, it is notified with the value Checkpoint::TransactionDropped.
/// So it is safe to assume that the handler will be notified "at some point".
class NotificationHandler {
 public:
  virtual void Notify(wr::Checkpoint aCheckpoint) = 0;
  virtual ~NotificationHandler() = default;
};

class TransactionBuilder {
 public:
  explicit TransactionBuilder(bool aUseSceneBuilderThread = true);

  ~TransactionBuilder();

  void SetLowPriority(bool aIsLowPriority);

  void UpdateEpoch(PipelineId aPipelineId, Epoch aEpoch);

  void SetRootPipeline(PipelineId aPipelineId);

  void RemovePipeline(PipelineId aPipelineId);

  void SetDisplayList(gfx::Color aBgColor, Epoch aEpoch,
                      mozilla::LayerSize aViewportSize,
                      wr::WrPipelineId pipeline_id,
                      const wr::LayoutSize& content_size,
                      wr::BuiltDisplayListDescriptor dl_descriptor,
                      wr::Vec<uint8_t>& dl_data);

  void ClearDisplayList(Epoch aEpoch, wr::WrPipelineId aPipeline);

  void GenerateFrame();

  void InvalidateRenderedFrame();

  void UpdateDynamicProperties(
      const nsTArray<wr::WrOpacityProperty>& aOpacityArray,
      const nsTArray<wr::WrTransformProperty>& aTransformArray);

  void SetWindowParameters(const LayoutDeviceIntSize& aWindowSize,
                           const LayoutDeviceIntRect& aDocRect);

  void UpdateScrollPosition(
      const wr::WrPipelineId& aPipelineId,
      const layers::ScrollableLayerGuid::ViewID& aScrollId,
      const wr::LayoutPoint& aScrollPosition);

  bool IsEmpty() const;

  bool IsResourceUpdatesEmpty() const;

  void AddImage(wr::ImageKey aKey, const ImageDescriptor& aDescriptor,
                wr::Vec<uint8_t>& aBytes);

  void AddBlobImage(wr::BlobImageKey aKey, const ImageDescriptor& aDescriptor,
                    wr::Vec<uint8_t>& aBytes);

  void AddExternalImageBuffer(ImageKey key, const ImageDescriptor& aDescriptor,
                              ExternalImageId aHandle);

  void AddExternalImage(ImageKey key, const ImageDescriptor& aDescriptor,
                        ExternalImageId aExtID,
                        wr::WrExternalImageBufferType aBufferType,
                        uint8_t aChannelIndex = 0);

  void UpdateImageBuffer(wr::ImageKey aKey, const ImageDescriptor& aDescriptor,
                         wr::Vec<uint8_t>& aBytes);

  void UpdateBlobImage(wr::BlobImageKey aKey,
                       const ImageDescriptor& aDescriptor,
                       wr::Vec<uint8_t>& aBytes,
                       const wr::LayoutIntRect& aDirtyRect);

  void UpdateExternalImage(ImageKey aKey, const ImageDescriptor& aDescriptor,
                           ExternalImageId aExtID,
                           wr::WrExternalImageBufferType aBufferType,
                           uint8_t aChannelIndex = 0);

  void UpdateExternalImageWithDirtyRect(
      ImageKey aKey, const ImageDescriptor& aDescriptor, ExternalImageId aExtID,
      wr::WrExternalImageBufferType aBufferType,
      const wr::DeviceIntRect& aDirtyRect, uint8_t aChannelIndex = 0);

  void SetImageVisibleArea(BlobImageKey aKey, const wr::DeviceIntRect& aArea);

  void DeleteImage(wr::ImageKey aKey);

  void DeleteBlobImage(wr::BlobImageKey aKey);

  void AddRawFont(wr::FontKey aKey, wr::Vec<uint8_t>& aBytes, uint32_t aIndex);

  void AddFontDescriptor(wr::FontKey aKey, wr::Vec<uint8_t>& aBytes,
                         uint32_t aIndex);

  void DeleteFont(wr::FontKey aKey);

  void AddFontInstance(wr::FontInstanceKey aKey, wr::FontKey aFontKey,
                       float aGlyphSize,
                       const wr::FontInstanceOptions* aOptions,
                       const wr::FontInstancePlatformOptions* aPlatformOptions,
                       wr::Vec<uint8_t>& aVariations);

  void DeleteFontInstance(wr::FontInstanceKey aKey);

  void Notify(wr::Checkpoint aWhen, UniquePtr<NotificationHandler> aHandler);

  void Clear();

  bool UseSceneBuilderThread() const { return mUseSceneBuilderThread; }
  Transaction* Raw() { return mTxn; }

 protected:
  bool mUseSceneBuilderThread;
  Transaction* mTxn;
};

class TransactionWrapper {
 public:
  explicit TransactionWrapper(Transaction* aTxn);

  void AppendTransformProperties(
      const nsTArray<wr::WrTransformProperty>& aTransformArray);
  void UpdateScrollPosition(
      const wr::WrPipelineId& aPipelineId,
      const layers::ScrollableLayerGuid::ViewID& aScrollId,
      const wr::LayoutPoint& aScrollPosition);
  void UpdatePinchZoom(float aZoom);

 private:
  Transaction* mTxn;
};

class WebRenderAPI {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebRenderAPI);

 public:
  /// This can be called on the compositor thread only.
  static already_AddRefed<WebRenderAPI> Create(
      layers::CompositorBridgeParent* aBridge,
      RefPtr<widget::CompositorWidget>&& aWidget,
      const wr::WrWindowId& aWindowId, LayoutDeviceIntSize aSize);

  already_AddRefed<WebRenderAPI> CreateDocument(LayoutDeviceIntSize aSize,
                                                int8_t aLayerIndex);

  already_AddRefed<WebRenderAPI> Clone();

  wr::WindowId GetId() const { return mId; }

  bool HitTest(const wr::WorldPoint& aPoint, wr::WrPipelineId& aOutPipelineId,
               layers::ScrollableLayerGuid::ViewID& aOutScrollId,
               gfx::CompositorHitTestInfo& aOutHitInfo);

  void SendTransaction(TransactionBuilder& aTxn);

  void SetFrameStartTime(const TimeStamp& aTime);

  void RunOnRenderThread(UniquePtr<RendererEvent> aEvent);

  void Readback(const TimeStamp& aStartTime, gfx::IntSize aSize,
                const Range<uint8_t>& aBuffer);

  void ClearAllCaches();

  void Pause();
  bool Resume();

  void WakeSceneBuilder();
  void FlushSceneBuilder();

  void NotifyMemoryPressure();
  void AccumulateMemoryReport(wr::MemoryReport*);

  wr::WrIdNamespace GetNamespace();
  uint32_t GetMaxTextureSize() const { return mMaxTextureSize; }
  bool GetUseANGLE() const { return mUseANGLE; }
  bool GetUseDComp() const { return mUseDComp; }
  bool GetUseTripleBuffering() const { return mUseTripleBuffering; }
  layers::SyncHandle GetSyncHandle() const { return mSyncHandle; }

  void Capture();

 protected:
  WebRenderAPI(wr::DocumentHandle* aHandle, wr::WindowId aId,
               int32_t aMaxTextureSize, bool aUseANGLE, bool aUseDComp,
               bool aUseTripleBuffering, layers::SyncHandle aSyncHandle)
      : mDocHandle(aHandle),
        mId(aId),
        mMaxTextureSize(aMaxTextureSize),
        mUseANGLE(aUseANGLE),
        mUseDComp(aUseDComp),
        mUseTripleBuffering(aUseTripleBuffering),
        mSyncHandle(aSyncHandle),
        mDebugFlags({0}) {}

  ~WebRenderAPI();
  // Should be used only for shutdown handling
  void WaitFlushed();

  void UpdateDebugFlags(uint32_t aFlags);

  wr::DocumentHandle* mDocHandle;
  wr::WindowId mId;
  int32_t mMaxTextureSize;
  bool mUseANGLE;
  bool mUseDComp;
  bool mUseTripleBuffering;
  layers::SyncHandle mSyncHandle;
  wr::DebugFlags mDebugFlags;

  // We maintain alive the root api to know when to shut the render backend
  // down, and the root api for the document to know when to delete the
  // document. mRootApi is null for the api object that owns the channel (and is
  // responsible for shutting it down), and mRootDocumentApi is null for the api
  // object owning (and responsible for destroying) a given document. All api
  // objects in the same window use the same channel, and some api objects write
  // to the same document (but there is only one owner for each channel and for
  // each document).
  RefPtr<wr::WebRenderAPI> mRootApi;
  RefPtr<wr::WebRenderAPI> mRootDocumentApi;

  friend class DisplayListBuilder;
  friend class layers::WebRenderBridgeParent;
};

// This is a RAII class that automatically sends the transaction on
// destruction. This is useful for code that has multiple exit points and we
// want to ensure that the stuff accumulated in the transaction gets sent
// regardless of which exit we take. Note that if the caller explicitly calls
// mApi->SendTransaction() that's fine too because that empties out the
// TransactionBuilder and leaves it as a valid empty transaction, so calling
// SendTransaction on it again ends up being a no-op.
class MOZ_RAII AutoTransactionSender {
 public:
  AutoTransactionSender(WebRenderAPI* aApi, TransactionBuilder* aTxn)
      : mApi(aApi), mTxn(aTxn) {}

  ~AutoTransactionSender() { mApi->SendTransaction(*mTxn); }

 private:
  WebRenderAPI* mApi;
  TransactionBuilder* mTxn;
};

/// This is a simple C++ wrapper around WrState defined in the rust bindings.
/// We may want to turn this into a direct wrapper on top of
/// WebRenderFrameBuilder instead, so the interface may change a bit.
class DisplayListBuilder {
 public:
  explicit DisplayListBuilder(wr::PipelineId aId,
                              const wr::LayoutSize& aContentSize,
                              size_t aCapacity = 0);
  DisplayListBuilder(DisplayListBuilder&&) = default;

  ~DisplayListBuilder();

  void Save();
  void Restore();
  void ClearSave();
  usize Dump(usize aIndent, const Maybe<usize>& aStart,
             const Maybe<usize>& aEnd);

  void Finalize(wr::LayoutSize& aOutContentSize,
                wr::BuiltDisplayList& aOutDisplayList);

  Maybe<wr::WrClipId> PushStackingContext(
      const wr::LayoutRect&
          aBounds,  // TODO: We should work with strongly typed rects
      const wr::WrClipId* aClipNodeId,
      const wr::WrAnimationProperty* aAnimation, const float* aOpacity,
      const gfx::Matrix4x4* aTransform, wr::TransformStyle aTransformStyle,
      const gfx::Matrix4x4* aPerspective, const wr::MixBlendMode& aMixBlendMode,
      const nsTArray<wr::WrFilterOp>& aFilters, bool aIsBackfaceVisible,
      const wr::RasterSpace& aRasterSpace);
  void PopStackingContext(bool aIsReferenceFrame);

  wr::WrClipChainId DefineClipChain(const Maybe<wr::WrClipChainId>& aParent,
                                    const nsTArray<wr::WrClipId>& aClips);

  wr::WrClipId DefineClip(
      const Maybe<wr::WrClipId>& aParentId, const wr::LayoutRect& aClipRect,
      const nsTArray<wr::ComplexClipRegion>* aComplex = nullptr,
      const wr::WrImageMask* aMask = nullptr);
  void PushClip(const wr::WrClipId& aClipId);
  void PopClip();

  wr::WrClipId DefineStickyFrame(const wr::LayoutRect& aContentRect,
                                 const float* aTopMargin,
                                 const float* aRightMargin,
                                 const float* aBottomMargin,
                                 const float* aLeftMargin,
                                 const StickyOffsetBounds& aVerticalBounds,
                                 const StickyOffsetBounds& aHorizontalBounds,
                                 const wr::LayoutVector2D& aAppliedOffset);

  Maybe<wr::WrClipId> GetScrollIdForDefinedScrollLayer(
      layers::ScrollableLayerGuid::ViewID aViewId) const;
  wr::WrClipId DefineScrollLayer(
      const layers::ScrollableLayerGuid::ViewID& aViewId,
      const Maybe<wr::WrClipId>& aParentId,
      const wr::LayoutRect&
          aContentRect,  // TODO: We should work with strongly typed rects
      const wr::LayoutRect& aClipRect);

  void PushClipAndScrollInfo(const wr::WrClipId* aScrollId,
                             const wr::WrClipChainId* aClipChainId,
                             const Maybe<wr::LayoutRect>& aClipChainLeaf);
  void PopClipAndScrollInfo(const wr::WrClipId* aScrollId);

  void PushRect(const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
                bool aIsBackfaceVisible, const wr::ColorF& aColor);

  void PushClearRect(const wr::LayoutRect& aBounds);

  void PushLinearGradient(const wr::LayoutRect& aBounds,
                          const wr::LayoutRect& aClip, bool aIsBackfaceVisible,
                          const wr::LayoutPoint& aStartPoint,
                          const wr::LayoutPoint& aEndPoint,
                          const nsTArray<wr::GradientStop>& aStops,
                          wr::ExtendMode aExtendMode,
                          const wr::LayoutSize aTileSize,
                          const wr::LayoutSize aTileSpacing);

  void PushRadialGradient(const wr::LayoutRect& aBounds,
                          const wr::LayoutRect& aClip, bool aIsBackfaceVisible,
                          const wr::LayoutPoint& aCenter,
                          const wr::LayoutSize& aRadius,
                          const nsTArray<wr::GradientStop>& aStops,
                          wr::ExtendMode aExtendMode,
                          const wr::LayoutSize aTileSize,
                          const wr::LayoutSize aTileSpacing);

  void PushImage(const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
                 bool aIsBackfaceVisible, wr::ImageRendering aFilter,
                 wr::ImageKey aImage, bool aPremultipliedAlpha = true,
                 const wr::ColorF& aColor = wr::ColorF{1.0f, 1.0f, 1.0f, 1.0f});

  void PushImage(const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
                 bool aIsBackfaceVisible, const wr::LayoutSize& aStretchSize,
                 const wr::LayoutSize& aTileSpacing, wr::ImageRendering aFilter,
                 wr::ImageKey aImage, bool aPremultipliedAlpha = true,
                 const wr::ColorF& aColor = wr::ColorF{1.0f, 1.0f, 1.0f, 1.0f});

  void PushYCbCrPlanarImage(
      const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
      bool aIsBackfaceVisible, wr::ImageKey aImageChannel0,
      wr::ImageKey aImageChannel1, wr::ImageKey aImageChannel2,
      wr::WrColorDepth aColorDepth, wr::WrYuvColorSpace aColorSpace,
      wr::ImageRendering aFilter);

  void PushNV12Image(const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
                     bool aIsBackfaceVisible, wr::ImageKey aImageChannel0,
                     wr::ImageKey aImageChannel1, wr::WrColorDepth aColorDepth,
                     wr::WrYuvColorSpace aColorSpace,
                     wr::ImageRendering aFilter);

  void PushYCbCrInterleavedImage(const wr::LayoutRect& aBounds,
                                 const wr::LayoutRect& aClip,
                                 bool aIsBackfaceVisible,
                                 wr::ImageKey aImageChannel0,
                                 wr::WrColorDepth aColorDepth,
                                 wr::WrYuvColorSpace aColorSpace,
                                 wr::ImageRendering aFilter);

  void PushIFrame(const wr::LayoutRect& aBounds, bool aIsBackfaceVisible,
                  wr::PipelineId aPipeline, bool aIgnoreMissingPipeline);

  // XXX WrBorderSides are passed with Range.
  // It is just to bypass compiler bug. See Bug 1357734.
  void PushBorder(const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
                  bool aIsBackfaceVisible, const wr::LayoutSideOffsets& aWidths,
                  const Range<const wr::BorderSide>& aSides,
                  const wr::BorderRadius& aRadius,
                  wr::AntialiasBorder = wr::AntialiasBorder::Yes);

  void PushBorderImage(const wr::LayoutRect& aBounds,
                       const wr::LayoutRect& aClip, bool aIsBackfaceVisible,
                       const wr::LayoutSideOffsets& aWidths,
                       wr::ImageKey aImage, const int32_t aWidth,
                       const int32_t aHeight,
                       const wr::SideOffsets2D<int32_t>& aSlice,
                       const wr::SideOffsets2D<float>& aOutset,
                       const wr::RepeatMode& aRepeatHorizontal,
                       const wr::RepeatMode& aRepeatVertical);

  void PushBorderGradient(const wr::LayoutRect& aBounds,
                          const wr::LayoutRect& aClip, bool aIsBackfaceVisible,
                          const wr::LayoutSideOffsets& aWidths,
                          const int32_t aWidth, const int32_t aHeight,
                          const wr::SideOffsets2D<int32_t>& aSlice,
                          const wr::LayoutPoint& aStartPoint,
                          const wr::LayoutPoint& aEndPoint,
                          const nsTArray<wr::GradientStop>& aStops,
                          wr::ExtendMode aExtendMode,
                          const wr::SideOffsets2D<float>& aOutset);

  void PushBorderRadialGradient(
      const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
      bool aIsBackfaceVisible, const wr::LayoutSideOffsets& aWidths,
      const wr::LayoutPoint& aCenter, const wr::LayoutSize& aRadius,
      const nsTArray<wr::GradientStop>& aStops, wr::ExtendMode aExtendMode,
      const wr::SideOffsets2D<float>& aOutset);

  void PushText(const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
                bool aIsBackfaceVisible, const wr::ColorF& aColor,
                wr::FontInstanceKey aFontKey,
                Range<const wr::GlyphInstance> aGlyphBuffer,
                const wr::GlyphOptions* aGlyphOptions = nullptr);

  void PushLine(const wr::LayoutRect& aClip, bool aIsBackfaceVisible,
                const wr::Line& aLine);

  void PushShadow(const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
                  bool aIsBackfaceVisible, const wr::Shadow& aShadow);

  void PopAllShadows();

  void PushBoxShadow(const wr::LayoutRect& aRect, const wr::LayoutRect& aClip,
                     bool aIsBackfaceVisible, const wr::LayoutRect& aBoxBounds,
                     const wr::LayoutVector2D& aOffset,
                     const wr::ColorF& aColor, const float& aBlurRadius,
                     const float& aSpreadRadius,
                     const wr::BorderRadius& aBorderRadius,
                     const wr::BoxShadowClipMode& aClipMode);

  // Checks to see if the innermost enclosing fixed pos item has the same
  // ASR. If so, it returns the scroll target for that fixed-pos item.
  // Otherwise, it returns Nothing().
  Maybe<layers::ScrollableLayerGuid::ViewID> GetContainingFixedPosScrollTarget(
      const ActiveScrolledRoot* aAsr);

  // Set the hit-test info to be used for all display items until the next call
  // to SetHitTestInfo or ClearHitTestInfo.
  void SetHitTestInfo(const layers::ScrollableLayerGuid::ViewID& aScrollId,
                      gfx::CompositorHitTestInfo aHitInfo);
  // Clears the hit-test info so that subsequent display items will not have it.
  void ClearHitTestInfo();

  already_AddRefed<gfxContext> GetTextContext(
      wr::IpcResourceUpdateQueue& aResources,
      const layers::StackingContextHelper& aSc,
      layers::WebRenderLayerManager* aManager, nsDisplayItem* aItem,
      nsRect& aBounds, const gfx::Point& aDeviceOffset);

  // Try to avoid using this when possible.
  wr::WrState* Raw() { return mWrState; }

  // A chain of RAII objects, each holding a (ASR, ViewID) tuple of data. The
  // topmost object is pointed to by the mActiveFixedPosTracker pointer in
  // the wr::DisplayListBuilder.
  class MOZ_RAII FixedPosScrollTargetTracker {
   public:
    FixedPosScrollTargetTracker(DisplayListBuilder& aBuilder,
                                const ActiveScrolledRoot* aAsr,
                                layers::ScrollableLayerGuid::ViewID aScrollId);
    ~FixedPosScrollTargetTracker();
    Maybe<layers::ScrollableLayerGuid::ViewID> GetScrollTargetForASR(
        const ActiveScrolledRoot* aAsr);

   private:
    FixedPosScrollTargetTracker* mParentTracker;
    DisplayListBuilder& mBuilder;
    const ActiveScrolledRoot* mAsr;
    layers::ScrollableLayerGuid::ViewID mScrollId;
  };

 protected:
  wr::LayoutRect MergeClipLeaf(const wr::LayoutRect& aClip) {
    if (mClipChainLeaf) {
      return wr::IntersectLayoutRect(*mClipChainLeaf, aClip);
    }
    return aClip;
  }

  wr::WrState* mWrState;

  // Track each scroll id that we encountered. We use this structure to
  // ensure that we don't define a particular scroll layer multiple times,
  // as that results in undefined behaviour in WR.
  std::unordered_map<layers::ScrollableLayerGuid::ViewID, wr::WrClipId>
      mScrollIds;

  // Contains the current leaf of the clip chain to be merged with the
  // display item's clip rect when pushing an item. May be set to Nothing() if
  // there is no clip rect to merge with.
  Maybe<wr::LayoutRect> mClipChainLeaf;

  RefPtr<layout::TextDrawTarget> mCachedTextDT;
  RefPtr<gfxContext> mCachedContext;

  FixedPosScrollTargetTracker* mActiveFixedPosTracker;

  friend class WebRenderAPI;
};

Maybe<wr::ImageFormat> SurfaceFormatToImageFormat(gfx::SurfaceFormat aFormat);

}  // namespace wr
}  // namespace mozilla

#endif
