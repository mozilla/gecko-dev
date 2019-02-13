/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "FrameLayerBuilder.h"

#include "mozilla/LookAndFeel.h"
#include "mozilla/Maybe.h"
#include "mozilla/dom/ProfileTimelineMarkerBinding.h"
#include "mozilla/gfx/Matrix.h"
#include "ActiveLayerTracker.h"
#include "BasicLayers.h"
#include "ImageContainer.h"
#include "LayerTreeInvalidation.h"
#include "Layers.h"
#include "MaskLayerImageCache.h"
#include "UnitTransforms.h"
#include "Units.h"
#include "gfx2DGlue.h"
#include "gfxUtils.h"
#include "nsDisplayList.h"
#include "nsDocShell.h"
#include "nsIScrollableFrame.h"
#include "nsImageFrame.h"
#include "nsLayoutUtils.h"
#include "nsPresContext.h"
#include "nsPrintfCString.h"
#include "nsRenderingContext.h"
#include "nsSVGIntegrationUtils.h"

#include "mozilla/Move.h"
#include "mozilla/ReverseIterator.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Tools.h"
#include "mozilla/unused.h"
#include "GeckoProfiler.h"
#include "LayersLogging.h"
#include "gfxPrefs.h"

#include <algorithm>

using namespace mozilla::layers;
using namespace mozilla::gfx;

namespace mozilla {

class PaintedDisplayItemLayerUserData;

static nsTHashtable<nsPtrHashKey<FrameLayerBuilder::DisplayItemData>>* sAliveDisplayItemDatas;

FrameLayerBuilder::DisplayItemData::DisplayItemData(LayerManagerData* aParent, uint32_t aKey,
                                                    Layer* aLayer, nsIFrame* aFrame)

  : mParent(aParent)
  , mLayer(aLayer)
  , mDisplayItemKey(aKey)
  , mItem(nullptr)
  , mUsed(true)
  , mIsInvalid(false)
{
  MOZ_COUNT_CTOR(FrameLayerBuilder::DisplayItemData);

  if (!sAliveDisplayItemDatas) {
    sAliveDisplayItemDatas = new nsTHashtable<nsPtrHashKey<FrameLayerBuilder::DisplayItemData>>();
  }
  MOZ_RELEASE_ASSERT(!sAliveDisplayItemDatas->Contains(this));
  sAliveDisplayItemDatas->PutEntry(this);

  MOZ_RELEASE_ASSERT(mLayer);
  if (aFrame) {
    AddFrame(aFrame);
  }

}

void
FrameLayerBuilder::DisplayItemData::AddFrame(nsIFrame* aFrame)
{
  MOZ_RELEASE_ASSERT(mLayer);
  mFrameList.AppendElement(aFrame);

  nsTArray<DisplayItemData*>* array =
    static_cast<nsTArray<DisplayItemData*>*>(aFrame->Properties().Get(FrameLayerBuilder::LayerManagerDataProperty()));
  if (!array) {
    array = new nsTArray<DisplayItemData*>();
    aFrame->Properties().Set(FrameLayerBuilder::LayerManagerDataProperty(), array);
  }
  array->AppendElement(this);
}

void
FrameLayerBuilder::DisplayItemData::RemoveFrame(nsIFrame* aFrame)
{
  MOZ_RELEASE_ASSERT(mLayer);
  bool result = mFrameList.RemoveElement(aFrame);
  MOZ_RELEASE_ASSERT(result, "Can't remove a frame that wasn't added!");

  nsTArray<DisplayItemData*>* array =
    static_cast<nsTArray<DisplayItemData*>*>(aFrame->Properties().Get(FrameLayerBuilder::LayerManagerDataProperty()));
  MOZ_RELEASE_ASSERT(array, "Must be already stored on the frame!");
  array->RemoveElement(this);
}

void
FrameLayerBuilder::DisplayItemData::EndUpdate()
{
  MOZ_RELEASE_ASSERT(mLayer);
  MOZ_ASSERT(!mItem);
  mIsInvalid = false;
  mUsed = false;
}

void
FrameLayerBuilder::DisplayItemData::EndUpdate(nsAutoPtr<nsDisplayItemGeometry> aGeometry)
{
  MOZ_RELEASE_ASSERT(mLayer);
  MOZ_ASSERT(mItem);

  mGeometry = aGeometry;
  mClip = mItem->GetClip();
  mFrameListChanges.Clear();

  mItem = nullptr;
  EndUpdate();
}

void
FrameLayerBuilder::DisplayItemData::BeginUpdate(Layer* aLayer, LayerState aState,
                                                uint32_t aContainerLayerGeneration,
                                                nsDisplayItem* aItem /* = nullptr */)
{
  MOZ_RELEASE_ASSERT(mLayer);
  MOZ_RELEASE_ASSERT(aLayer);
  mLayer = aLayer;
  mOptLayer = nullptr;
  mInactiveManager = nullptr;
  mLayerState = aState;
  mContainerLayerGeneration = aContainerLayerGeneration;
  mUsed = true;

  if (aLayer->AsPaintedLayer()) {
    mItem = aItem;
  }

  if (!aItem) {
    return;
  }

  // We avoid adding or removing element unnecessarily
  // since we have to modify userdata each time
  nsAutoTArray<nsIFrame*, 4> copy(mFrameList);
  if (!copy.RemoveElement(aItem->Frame())) {
    AddFrame(aItem->Frame());
    mFrameListChanges.AppendElement(aItem->Frame());
  }

  nsAutoTArray<nsIFrame*,4> mergedFrames;
  aItem->GetMergedFrames(&mergedFrames);
  for (uint32_t i = 0; i < mergedFrames.Length(); ++i) {
    if (!copy.RemoveElement(mergedFrames[i])) {
      AddFrame(mergedFrames[i]);
      mFrameListChanges.AppendElement(mergedFrames[i]);
    }
  }

  for (uint32_t i = 0; i < copy.Length(); i++) {
    RemoveFrame(copy[i]);
    mFrameListChanges.AppendElement(copy[i]);
  }
}

static nsIFrame* sDestroyedFrame = nullptr;
FrameLayerBuilder::DisplayItemData::~DisplayItemData()
{
  MOZ_COUNT_DTOR(FrameLayerBuilder::DisplayItemData);
  MOZ_RELEASE_ASSERT(mLayer);
  for (uint32_t i = 0; i < mFrameList.Length(); i++) {
    nsIFrame* frame = mFrameList[i];
    if (frame == sDestroyedFrame) {
      continue;
    }
    nsTArray<DisplayItemData*> *array =
      reinterpret_cast<nsTArray<DisplayItemData*>*>(frame->Properties().Get(LayerManagerDataProperty()));
    array->RemoveElement(this);
  }

  MOZ_RELEASE_ASSERT(sAliveDisplayItemDatas && sAliveDisplayItemDatas->Contains(this));
  sAliveDisplayItemDatas->RemoveEntry(this);
  if (sAliveDisplayItemDatas->Count() == 0) {
    delete sAliveDisplayItemDatas;
    sAliveDisplayItemDatas = nullptr;
  }
}

const nsTArray<nsIFrame*>&
FrameLayerBuilder::DisplayItemData::GetFrameListChanges()
{
  return mFrameListChanges;
}

/**
 * This is the userdata we associate with a layer manager.
 */
class LayerManagerData : public LayerUserData {
public:
  explicit LayerManagerData(LayerManager *aManager)
    : mLayerManager(aManager)
#ifdef DEBUG_DISPLAY_ITEM_DATA
    , mParent(nullptr)
#endif
    , mInvalidateAllLayers(false)
  {
    MOZ_COUNT_CTOR(LayerManagerData);
  }
  ~LayerManagerData() {
    MOZ_COUNT_DTOR(LayerManagerData);
  }

#ifdef DEBUG_DISPLAY_ITEM_DATA
  void Dump(const char *aPrefix = "") {
    printf_stderr("%sLayerManagerData %p\n", aPrefix, this);
    nsAutoCString prefix;
    prefix += aPrefix;
    prefix += "  ";
    mDisplayItems.EnumerateEntries(
        FrameLayerBuilder::DumpDisplayItemDataForFrame, (void*)prefix.get());
  }
#endif

  /**
   * Tracks which frames have layers associated with them.
   */
  LayerManager *mLayerManager;
#ifdef DEBUG_DISPLAY_ITEM_DATA
  LayerManagerData *mParent;
#endif
  nsTHashtable<nsRefPtrHashKey<FrameLayerBuilder::DisplayItemData> > mDisplayItems;
  bool mInvalidateAllLayers;
};

/* static */ void
FrameLayerBuilder::DestroyDisplayItemDataFor(nsIFrame* aFrame)
{
  FrameProperties props = aFrame->Properties();
  props.Delete(LayerManagerDataProperty());
}

// a global cache of image containers used for mask layers
static MaskLayerImageCache* gMaskLayerImageCache = nullptr;

static inline MaskLayerImageCache* GetMaskLayerImageCache()
{
  if (!gMaskLayerImageCache) {
    gMaskLayerImageCache = new MaskLayerImageCache();
  }

  return gMaskLayerImageCache;
}

struct AssignedDisplayItem
{
  AssignedDisplayItem(nsDisplayItem* aItem,
                      const DisplayItemClip& aClip,
                      LayerState aLayerState)
    : mItem(aItem)
    , mClip(aClip)
    , mLayerState(aLayerState)
  {}

  nsDisplayItem* mItem;
  DisplayItemClip mClip;
  LayerState mLayerState;
};

/**
 * We keep a stack of these to represent the PaintedLayers that are
 * currently available to have display items added to.
 * We use a stack here because as much as possible we want to
 * assign display items to existing PaintedLayers, and to the lowest
 * PaintedLayer in z-order. This reduces the number of layers and
 * makes it more likely a display item will be rendered to an opaque
 * layer, giving us the best chance of getting subpixel AA.
 */
class PaintedLayerData {
public:
  PaintedLayerData() :
    mAnimatedGeometryRoot(nullptr),
    mFixedPosFrameForLayerData(nullptr),
    mReferenceFrame(nullptr),
    mLayer(nullptr),
    mIsSolidColorInVisibleRegion(false),
    mFontSmoothingBackgroundColor(NS_RGBA(0,0,0,0)),
    mSingleItemFixedToViewport(false),
    mNeedComponentAlpha(false),
    mForceTransparentSurface(false),
    mHideAllLayersBelow(false),
    mOpaqueForAnimatedGeometryRootParent(false),
    mDisableFlattening(false),
    mImage(nullptr),
    mCommonClipCount(-1),
    mNewChildLayersIndex(-1)
  {}

#ifdef MOZ_DUMP_PAINTING
  /**
   * Keep track of important decisions for debugging.
   */
  nsCString mLog;

  #define FLB_LOG_PAINTED_LAYER_DECISION(pld, ...) \
          if (gfxPrefs::LayersDumpDecision()) { \
            pld->mLog.AppendPrintf("\t\t\t\t"); \
            pld->mLog.AppendPrintf(__VA_ARGS__); \
          }
#else
  #define FLB_LOG_PAINTED_LAYER_DECISION(...)
#endif

  /**
   * Record that an item has been added to the PaintedLayer, so we
   * need to update our regions.
   * @param aVisibleRect the area of the item that's visible
   * @param aSolidColor if non-null, the visible area of the item is
   * a constant color given by *aSolidColor
   */
  void Accumulate(ContainerState* aState,
                  nsDisplayItem* aItem,
                  const nsIntRegion& aClippedOpaqueRegion,
                  const nsIntRect& aVisibleRect,
                  const DisplayItemClip& aClip,
                  LayerState aLayerState);
  const nsIFrame* GetAnimatedGeometryRoot() { return mAnimatedGeometryRoot; }

  /**
   * Add the given hit regions to the hit regions to the hit retions for this
   * PaintedLayer.
   */
  void AccumulateEventRegions(nsDisplayLayerEventRegions* aEventRegions)
  {
    FLB_LOG_PAINTED_LAYER_DECISION(this, "Accumulating event regions %p against pld=%p\n", aEventRegions, this);

    mHitRegion.Or(mHitRegion, aEventRegions->HitRegion());
    mMaybeHitRegion.Or(mMaybeHitRegion, aEventRegions->MaybeHitRegion());
    mDispatchToContentHitRegion.Or(mDispatchToContentHitRegion, aEventRegions->DispatchToContentHitRegion());
    mNoActionRegion.Or(mNoActionRegion, aEventRegions->NoActionRegion());
    mHorizontalPanRegion.Or(mHorizontalPanRegion, aEventRegions->HorizontalPanRegion());
    mVerticalPanRegion.Or(mVerticalPanRegion, aEventRegions->VerticalPanRegion());
  }

  /**
   * If this represents only a nsDisplayImage, and the image type supports being
   * optimized to an ImageLayer, returns true.
   */
  bool CanOptimizeToImageLayer(nsDisplayListBuilder* aBuilder);

  /**
   * If this represents only a nsDisplayImage, and the image type supports being
   * optimized to an ImageLayer, returns an ImageContainer for the underlying
   * image if one is available.
   */
  already_AddRefed<ImageContainer> GetContainerForImageLayer(nsDisplayListBuilder* aBuilder);

  bool VisibleAboveRegionIntersects(const nsIntRect& aRect) const
  { return mVisibleAboveRegion.Intersects(aRect); }
  bool VisibleAboveRegionIntersects(const nsIntRegion& aRegion) const
  { return !mVisibleAboveRegion.Intersect(aRegion).IsEmpty(); }

  bool VisibleRegionIntersects(const nsIntRect& aRect) const
  { return mVisibleRegion.Intersects(aRect); }
  bool VisibleRegionIntersects(const nsIntRegion& aRegion) const
  { return !mVisibleRegion.Intersect(aRegion).IsEmpty(); }

  /**
   * The region of visible content in the layer, relative to the
   * container layer (which is at the snapped top-left of the display
   * list reference frame).
   */
  nsIntRegion  mVisibleRegion;
  /**
   * The region of visible content in the layer that is opaque.
   * Same coordinate system as mVisibleRegion.
   */
  nsIntRegion  mOpaqueRegion;
  /**
   * The definitely-hit region for this PaintedLayer.
   */
  nsRegion  mHitRegion;
  /**
   * The maybe-hit region for this PaintedLayer.
   */
  nsRegion  mMaybeHitRegion;
  /**
   * The dispatch-to-content hit region for this PaintedLayer.
   */
  nsRegion  mDispatchToContentHitRegion;
  /**
   * The region for this PaintedLayer that is sensitive to events
   * but disallows panning and zooming. This is an approximation
   * and any deviation from the true region will be part of the
   * mDispatchToContentHitRegion.
   */
  nsRegion mNoActionRegion;
  /**
   * The region for this PaintedLayer that is sensitive to events and
   * allows horizontal panning but not zooming. This is an approximation
   * and any deviation from the true region will be part of the
   * mDispatchToContentHitRegion.
   */
  nsRegion mHorizontalPanRegion;
  /**
   * The region for this PaintedLayer that is sensitive to events and
   * allows vertical panning but not zooming. This is an approximation
   * and any deviation from the true region will be part of the
   * mDispatchToContentHitRegion.
   */
  nsRegion mVerticalPanRegion;
  /**
   * The "active scrolled root" for all content in the layer. Must
   * be non-null; all content in a PaintedLayer must have the same
   * active scrolled root.
   */
  const nsIFrame* mAnimatedGeometryRoot;
  /**
   * The offset between mAnimatedGeometryRoot and the reference frame.
   */
  nsPoint mAnimatedGeometryRootOffset;
  /**
   * If non-null, the frame from which we'll extract "fixed positioning"
   * metadata for this layer. This can be a position:fixed frame or a viewport
   * frame; the latter case is used for background-attachment:fixed content.
   */
  const nsIFrame* mFixedPosFrameForLayerData;
  const nsIFrame* mReferenceFrame;
  PaintedLayer* mLayer;
  /**
   * If mIsSolidColorInVisibleRegion is true, this is the color of the visible
   * region.
   */
  nscolor      mSolidColor;
  /**
   * True if every pixel in mVisibleRegion will have color mSolidColor.
   */
  bool mIsSolidColorInVisibleRegion;
  /**
   * The target background color for smoothing fonts that are drawn on top of
   * transparent parts of the layer.
   */
  nscolor mFontSmoothingBackgroundColor;
  /**
   * True if the layer contains exactly one item that returned true for
   * ShouldFixToViewport.
   */
  bool mSingleItemFixedToViewport;
  /**
   * True if there is any text visible in the layer that's over
   * transparent pixels in the layer.
   */
  bool mNeedComponentAlpha;
  /**
   * Set if the layer should be treated as transparent, even if its entire
   * area is covered by opaque display items. For example, this needs to
   * be set if something is going to "punch holes" in the layer by clearing
   * part of its surface.
   */
  bool mForceTransparentSurface;
  /**
   * Set if all layers below this PaintedLayer should be hidden.
   */
  bool mHideAllLayersBelow;
  /**
   * Set if the opaque region for this layer can be applied to the parent
   * animated geometry root of this layer's animated geometry root.
   * We set this when a PaintedLayer's animated geometry root is a scrollframe
   * and the PaintedLayer completely fills the displayport of the scrollframe.
   */
  bool mOpaqueForAnimatedGeometryRootParent;
  /**
   * Set if there is content in the layer that must avoid being flattened.
   */
  bool mDisableFlattening;
  /**
   * Stores the pointer to the nsDisplayImage if we want to
   * convert this to an ImageLayer.
   */
  nsDisplayImageContainer* mImage;
  /**
   * Stores the clip that we need to apply to the image or, if there is no
   * image, a clip for SOME item in the layer. There is no guarantee which
   * item's clip will be stored here and mItemClip should not be used to clip
   * the whole layer - only some part of the clip should be used, as determined
   * by PaintedDisplayItemLayerUserData::GetCommonClipCount() - which may even be
   * no part at all.
   */
  DisplayItemClip mItemClip;
  /**
   * The first mCommonClipCount rounded rectangle clips are identical for
   * all items in the layer.
   * -1 if there are no items in the layer; must be >=0 by the time that this
   * data is popped from the stack.
   */
  int32_t mCommonClipCount;
  /**
   * Index of this layer in mNewChildLayers.
   */
  int32_t mNewChildLayersIndex;
  /*
   * Updates mCommonClipCount by checking for rounded rect clips in common
   * between the clip on a new item (aCurrentClip) and the common clips
   * on items already in the layer (the first mCommonClipCount rounded rects
   * in mItemClip).
   */
  void UpdateCommonClipCount(const DisplayItemClip& aCurrentClip);
  /**
   * The union of all the bounds of the display items in this layer.
   */
  nsIntRect mBounds;
  /**
   * The region of visible content above the layer and below the
   * next PaintedLayerData currently in the stack, if any.
   * This is a conservative approximation: it contains the true region.
   */
  nsIntRegion mVisibleAboveRegion;
  /**
   * All the display items that have been assigned to this painted layer.
   * These items get added by Accumulate().
   */
  nsTArray<AssignedDisplayItem> mAssignedDisplayItems;

};

struct NewLayerEntry {
  NewLayerEntry()
    : mAnimatedGeometryRoot(nullptr)
    , mFixedPosFrameForLayerData(nullptr)
    , mLayerContentsVisibleRect(0, 0, -1, -1)
    , mHideAllLayersBelow(false)
    , mOpaqueForAnimatedGeometryRootParent(false)
    , mPropagateComponentAlphaFlattening(true)
  {}
  // mLayer is null if the previous entry is for a PaintedLayer that hasn't
  // been optimized to some other form (yet).
  nsRefPtr<Layer> mLayer;
  const nsIFrame* mAnimatedGeometryRoot;
  const nsIFrame* mFixedPosFrameForLayerData;
  // If non-null, this FrameMetrics is set to the be the first FrameMetrics
  // on the layer.
  UniquePtr<FrameMetrics> mBaseFrameMetrics;
  // The following are only used for retained layers (for occlusion
  // culling of those layers). These regions are all relative to the
  // container reference frame.
  nsIntRegion mVisibleRegion;
  nsIntRegion mOpaqueRegion;
  // This rect is in the layer's own coordinate space. The computed visible
  // region for the layer cannot extend beyond this rect.
  nsIntRect mLayerContentsVisibleRect;
  bool mHideAllLayersBelow;
  // When mOpaqueForAnimatedGeometryRootParent is true, the opaque region of
  // this layer is opaque in the same position even subject to the animation of
  // geometry of mAnimatedGeometryRoot. For example when mAnimatedGeometryRoot
  // is a scrolled frame and the scrolled content is opaque everywhere in the
  // displayport, we can set this flag.
  // When this flag is set, we can treat this opaque region as covering
  // content whose animated geometry root is the animated geometry root for
  // mAnimatedGeometryRoot->GetParent().
  bool mOpaqueForAnimatedGeometryRootParent;

  // If true, then the content flags for this layer should contribute
  // to our decision to flatten component alpha layers, false otherwise.
  bool mPropagateComponentAlphaFlattening;
};

class PaintedLayerDataTree;

/**
 * This is tree node type for PaintedLayerDataTree.
 * Each node corresponds to a different animated geometry root, and contains
 * a stack of PaintedLayerDatas, in bottom-to-top order.
 * There is at most one node per animated geometry root. The ancestor and
 * descendant relations in PaintedLayerDataTree tree mirror those in the frame
 * tree.
 * Each node can have clip that describes the potential extents that items in
 * this node can cover. If mHasClip is false, it means that the node's contents
 * can move anywhere.
 * Testing against the clip instead of the node's actual contents has the
 * advantage that the node's contents can move or animate without affecting
 * content in other nodes. So we don't need to re-layerize during animations
 * (sync or async), and during async animations everything is guaranteed to
 * look correct.
 * The contents of a node's PaintedLayerData stack all share the node's
 * animated geometry root. The child nodes are on top of the PaintedLayerData
 * stack, in z-order, and the clip rects of the child nodes are allowed to
 * intersect with the visible region or visible above region of their parent
 * node's PaintedLayerDatas.
 */
class PaintedLayerDataNode {
public:
  PaintedLayerDataNode(PaintedLayerDataTree& aTree,
                       PaintedLayerDataNode* aParent,
                       const nsIFrame* aAnimatedGeometryRoot);
  ~PaintedLayerDataNode();

  const nsIFrame* AnimatedGeometryRoot() const { return mAnimatedGeometryRoot; }

  /**
   * Whether this node's contents can potentially intersect aRect.
   * aRect is in our tree's ContainerState's coordinate space.
   */
  bool Intersects(const nsIntRect& aRect) const
    { return !mHasClip || mClipRect.Intersects(aRect); }

  /**
   * Create a PaintedLayerDataNode for aAnimatedGeometryRoot, add it to our
   * children, and return it.
   */
  PaintedLayerDataNode* AddChildNodeFor(const nsIFrame* aAnimatedGeometryRoot);

  /**
   * Find a PaintedLayerData in our mPaintedLayerDataStack that aItem can be
   * added to. Creates a new PaintedLayerData by calling
   * aNewPaintedLayerCallback if necessary.
   */
  template<typename NewPaintedLayerCallbackType>
  PaintedLayerData* FindPaintedLayerFor(const nsIntRect& aVisibleRect,
                                        NewPaintedLayerCallbackType aNewPaintedLayerCallback);

  /**
   * Find an opaque background color for aRegion. Pulls a color from the parent
   * geometry root if appropriate, but only if that color is present underneath
   * the whole clip of this node, so that this node's contents can animate or
   * move (possibly async) without having to change the background color.
   * @param aUnderIndex Searching will start in mPaintedLayerDataStack right
   *                    below aUnderIndex.
   */
  enum { ABOVE_TOP = -1 };
  nscolor FindOpaqueBackgroundColor(const nsIntRegion& aRegion,
                                    int32_t aUnderIndex = ABOVE_TOP) const;
  /**
   * Same as FindOpaqueBackgroundColor, but only returns a color if absolutely
   * nothing is in between, so that it can be used for a layer that can move
   * anywhere inside our clip.
   */
  nscolor FindOpaqueBackgroundColorCoveringEverything() const;

  /**
   * Adds aRect to this node's top PaintedLayerData's mVisibleAboveRegion,
   * or mVisibleAboveBackgroundRegion if mPaintedLayerDataStack is empty.
   */
  void AddToVisibleAboveRegion(const nsIntRect& aRect);
  /**
   * Call this if all of our existing content can potentially be covered, so
   * nothing can merge with it and all new content needs to create new items
   * on top. This will finish all of our children and pop our whole
   * mPaintedLayerDataStack.
   */
  void SetAllDrawingAbove();

  /**
   * Finish this node: Finish all children, finish our PaintedLayer contents,
   * and (if requested) adjust our parent's visible above region to include
   * our clip.
   */
  void Finish(bool aParentNeedsAccurateVisibleAboveRegion);

  /**
   * Finish any children that intersect aRect.
   */
  void FinishChildrenIntersecting(const nsIntRect& aRect);

  /**
   * Finish all children.
   */
  void FinishAllChildren() { FinishAllChildren(true); }

protected:
  /**
   * Finish the topmost item in mPaintedLayerDataStack and pop it from the
   * stack.
   */
  void PopPaintedLayerData();
  /**
   * Finish all items in mPaintedLayerDataStack and clear the stack.
   */
  void PopAllPaintedLayerData();
  /**
   * Finish all of our child nodes, but don't touch mPaintedLayerDataStack.
   */
  void FinishAllChildren(bool aThisNodeNeedsAccurateVisibleAboveRegion);
  /**
   * Pass off opaque background color searching to our parent node, if we have
   * one.
   */
  nscolor FindOpaqueBackgroundColorInParentNode() const;

  PaintedLayerDataTree& mTree;
  PaintedLayerDataNode* mParent;
  const nsIFrame* mAnimatedGeometryRoot;

  /**
   * Our contents: a PaintedLayerData stack and our child nodes.
   */
  nsTArray<PaintedLayerData> mPaintedLayerDataStack;

  /**
   * UniquePtr is used here in the sense of "unique ownership", i.e. there is
   * only one owner. Not in the sense of "this is the only pointer to the
   * node": There are two other, non-owning, pointers to our child nodes: The
   * node's respective children point to their parent node with their mParent
   * pointer, and the tree keeps a map of animated geometry root to node in its
   * mNodes member. These outside pointers are the reason that mChildren isn't
   * just an nsTArray<PaintedLayerDataNode> (since the pointers would become
   * invalid whenever the array expands its capacity).
   */
  nsTArray<UniquePtr<PaintedLayerDataNode>> mChildren;

  /**
   * The region that's covered between our "background" and the bottom of
   * mPaintedLayerDataStack. This is used to indicate whether we can pull
   * a background color from our parent node. If mVisibleAboveBackgroundRegion
   * should be considered infinite, mAllDrawingAboveBackground will be true and
   * the value of mVisibleAboveBackgroundRegion will be meaningless.
   */
  nsIntRegion mVisibleAboveBackgroundRegion;

  /**
   * Our clip, if we have any. If not, that means we can move anywhere, and
   * mHasClip will be false and mClipRect will be meaningless.
   */
  nsIntRect mClipRect;
  bool mHasClip;

  /**
   * Whether mVisibleAboveBackgroundRegion should be considered infinite.
   */
  bool mAllDrawingAboveBackground;
};

class ContainerState;

/**
 * A tree of PaintedLayerDataNodes. At any point in time, the tree only
 * contains nodes for animated geometry roots that new items can potentially
 * merge into. Any time content is added on top that overlaps existing things
 * in such a way that we no longer want to merge new items with some existing
 * content, that existing content gets "finished".
 * The public-facing methods of this class are FindPaintedLayerFor,
 * AddingOwnLayer, and Finish. The other public methods are for
 * PaintedLayerDataNode.
 * The tree calls out to its containing ContainerState for some things.
 * All coordinates / rects in the tree or the tree nodes are in the
 * ContainerState's coordinate space, i.e. relative to the reference frame and
 * in layer pixels.
 * The clip rects of sibling nodes never overlap. This is ensured by finishing
 * existing nodes before adding new ones, if this property were to be violated.
 * The root tree node doesn't get finished until the ContainerState is
 * finished.
 * The tree's root node is always the root reference frame of the builder. We
 * don't stop at the container state's mContainerAnimatedGeometryRoot because
 * some of our contents can have animated geometry roots that are not
 * descendants of the container's animated geometry root. Every animated
 * geometry root we encounter for our contents needs to have a defined place in
 * the tree.
 */
class PaintedLayerDataTree {
public:
  PaintedLayerDataTree(ContainerState& aContainerState,
                       nscolor& aBackgroundColor)
    : mContainerState(aContainerState)
    , mContainerUniformBackgroundColor(aBackgroundColor)
  {}

  ~PaintedLayerDataTree()
  {
    MOZ_ASSERT(!mRoot);
    MOZ_ASSERT(mNodes.Count() == 0);
  }

  /**
   * Notify our contents that some non-PaintedLayer content has been added.
   * *aRect needs to be a rectangle that doesn't move with respect to
   * aAnimatedGeometryRoot and that contains the added item.
   * If aRect is null, the extents will be considered infinite.
   * If aOutUniformBackgroundColor is non-null, it will be set to an opaque
   * color that can be pulled into the background of the added content, or
   * transparent if that is not possible.
   */
  void AddingOwnLayer(const nsIFrame* aAnimatedGeometryRoot,
                      const nsIntRect* aRect,
                      nscolor* aOutUniformBackgroundColor);

  /**
   * Find a PaintedLayerData for aItem. This can either be an existing
   * PaintedLayerData from inside a node in our tree, or a new one that gets
   * created by a call out to aNewPaintedLayerCallback.
   */
  template<typename NewPaintedLayerCallbackType>
  PaintedLayerData* FindPaintedLayerFor(const nsIFrame* aAnimatedGeometryRoot,
                                        const nsIntRect& aVisibleRect,
                                        bool aShouldFixToViewport,
                                        NewPaintedLayerCallbackType aNewPaintedLayerCallback);

  /**
   * Finish everything.
   */
  void Finish();

  /**
   * Get the parent animated geometry root of aAnimatedGeometryRoot.
   * That's either aAnimatedGeometryRoot's animated geometry root, or, if
   * that's aAnimatedGeometryRoot itself, then it's the animated geometry
   * root for aAnimatedGeometryRoot's cross-doc parent frame.
   */
  const nsIFrame* GetParentAnimatedGeometryRoot(const nsIFrame* aAnimatedGeometryRoot);

  /**
   * Whether aAnimatedGeometryRoot has an intrinsic clip that doesn't move with
   * respect to aAnimatedGeometryRoot's parent animated geometry root.
   * If aAnimatedGeometryRoot is a scroll frame, this will be the scroll frame's
   * scroll port, otherwise there is no clip.
   * This method doesn't have much to do with PaintedLayerDataTree, but this is
   * where we have easy access to a display list builder, which we use to get
   * the clip rect result into the right coordinate space.
   */
  bool IsClippedWithRespectToParentAnimatedGeometryRoot(const nsIFrame* aAnimatedGeometryRoot,
                                                        nsIntRect* aOutClip);

  /**
   * Called by PaintedLayerDataNode when it is finished, so that we can drop
   * our pointers to it.
   */
  void NodeWasFinished(const nsIFrame* aAnimatedGeometryRoot);

  nsDisplayListBuilder* Builder() const;
  ContainerState& ContState() const { return mContainerState; }
  nscolor UniformBackgroundColor() const { return mContainerUniformBackgroundColor; }

protected:
  /**
   * Finish all nodes that potentially intersect *aRect, where *aRect is a rect
   * that doesn't move with respect to aAnimatedGeometryRoot.
   * If aRect is null, *aRect will be considered infinite.
   */
  void FinishPotentiallyIntersectingNodes(const nsIFrame* aAnimatedGeometryRoot,
                                          const nsIntRect* aRect);

  /**
   * Make sure that there is a node for aAnimatedGeometryRoot and all of its
   * ancestor geometry roots. Return the node for aAnimatedGeometryRoot.
   */
  PaintedLayerDataNode* EnsureNodeFor(const nsIFrame* aAnimatedGeometryRoot);

  /**
   * Find an existing node in the tree for an ancestor of aAnimatedGeometryRoot.
   * *aOutAncestorChild will be set to the last ancestor that was encountered
   * in the search up from aAnimatedGeometryRoot; it will be a child animated
   * geometry root of the result, if neither are null.
   */
  PaintedLayerDataNode*
    FindNodeForAncestorAnimatedGeometryRoot(const nsIFrame* aAnimatedGeometryRoot,
                                            const nsIFrame** aOutAncestorChild);

  ContainerState& mContainerState;
  UniquePtr<PaintedLayerDataNode> mRoot;

  /**
   * The uniform opaque color from behind this container layer, or
   * NS_RGBA(0,0,0,0) if the background behind this container layer is not
   * uniform and opaque. This color can be pulled into PaintedLayers that are
   * directly above the background.
   */
  nscolor mContainerUniformBackgroundColor;

  /**
   * A hash map for quick access the node belonging to a particular animated
   * geometry root.
   */
  nsDataHashtable<nsPtrHashKey<const nsIFrame>, PaintedLayerDataNode*> mNodes;
};

/**
 * This is a helper object used to build up the layer children for
 * a ContainerLayer.
 */
class ContainerState {
public:
  ContainerState(nsDisplayListBuilder* aBuilder,
                 LayerManager* aManager,
                 FrameLayerBuilder* aLayerBuilder,
                 nsIFrame* aContainerFrame,
                 nsDisplayItem* aContainerItem,
                 const nsRect& aContainerBounds,
                 ContainerLayer* aContainerLayer,
                 const ContainerLayerParameters& aParameters,
                 bool aFlattenToSingleLayer,
                 nscolor aBackgroundColor) :
    mBuilder(aBuilder), mManager(aManager),
    mLayerBuilder(aLayerBuilder),
    mContainerFrame(aContainerFrame),
    mContainerLayer(aContainerLayer),
    mContainerBounds(aContainerBounds),
    mParameters(aParameters),
    mPaintedLayerDataTree(*this, aBackgroundColor),
    mFlattenToSingleLayer(aFlattenToSingleLayer)
  {
    nsPresContext* presContext = aContainerFrame->PresContext();
    mAppUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();
    mContainerReferenceFrame =
      const_cast<nsIFrame*>(aContainerItem ? aContainerItem->ReferenceFrameForChildren() :
                                             mBuilder->FindReferenceFrameFor(mContainerFrame));
    bool isAtRoot = !aContainerItem || (aContainerItem->Frame() == mBuilder->RootReferenceFrame());
    MOZ_ASSERT_IF(isAtRoot, mContainerReferenceFrame == mBuilder->RootReferenceFrame());
    mContainerAnimatedGeometryRoot = isAtRoot
      ? mContainerReferenceFrame
      : nsLayoutUtils::GetAnimatedGeometryRootFor(aContainerItem, aBuilder, aManager);
    MOZ_ASSERT(nsLayoutUtils::IsAncestorFrameCrossDoc(mBuilder->RootReferenceFrame(),
                                                      mContainerAnimatedGeometryRoot));
    NS_ASSERTION(!aContainerItem || !aContainerItem->ShouldFixToViewport(aManager),
                 "Container items never return true for ShouldFixToViewport");
    mContainerFixedPosFrame =
        FindFixedPosFrameForLayerData(mContainerAnimatedGeometryRoot, false);
    // When AllowResidualTranslation is false, display items will be drawn
    // scaled with a translation by integer pixels, so we know how the snapping
    // will work.
    mSnappingEnabled = aManager->IsSnappingEffectiveTransforms() &&
      !mParameters.AllowResidualTranslation();
    CollectOldLayers();
  }

  /**
   * This is the method that actually walks a display list and builds
   * the child layers.
   */
  void ProcessDisplayItems(nsDisplayList* aList);
  /**
   * This finalizes all the open PaintedLayers by popping every element off
   * mPaintedLayerDataStack, then sets the children of the container layer
   * to be all the layers in mNewChildLayers in that order and removes any
   * layers as children of the container that aren't in mNewChildLayers.
   * @param aTextContentFlags if any child layer has CONTENT_COMPONENT_ALPHA,
   * set *aTextContentFlags to CONTENT_COMPONENT_ALPHA
   */
  void Finish(uint32_t *aTextContentFlags, LayerManagerData* aData,
              const nsIntRect& aContainerPixelBounds,
              nsDisplayList* aChildItems, bool& aHasComponentAlphaChildren);

  nscoord GetAppUnitsPerDevPixel() { return mAppUnitsPerDevPixel; }

  nsIntRect ScaleToNearestPixels(const nsRect& aRect) const
  {
    return aRect.ScaleToNearestPixels(mParameters.mXScale, mParameters.mYScale,
                                      mAppUnitsPerDevPixel);
  }
  nsIntRegion ScaleRegionToNearestPixels(const nsRegion& aRegion) const
  {
    return aRegion.ScaleToNearestPixels(mParameters.mXScale, mParameters.mYScale,
                                        mAppUnitsPerDevPixel);
  }
  nsIntRect ScaleToOutsidePixels(const nsRect& aRect, bool aSnap = false) const
  {
    if (aSnap && mSnappingEnabled) {
      return ScaleToNearestPixels(aRect);
    }
    return aRect.ScaleToOutsidePixels(mParameters.mXScale, mParameters.mYScale,
                                      mAppUnitsPerDevPixel);
  }
  nsIntRect ScaleToInsidePixels(const nsRect& aRect, bool aSnap = false) const
  {
    if (aSnap && mSnappingEnabled) {
      return ScaleToNearestPixels(aRect);
    }
    return aRect.ScaleToInsidePixels(mParameters.mXScale, mParameters.mYScale,
                                     mAppUnitsPerDevPixel);
  }

  nsIntRegion ScaleRegionToInsidePixels(const nsRegion& aRegion, bool aSnap = false) const
  {
    if (aSnap && mSnappingEnabled) {
      return ScaleRegionToNearestPixels(aRegion);
    }
    return aRegion.ScaleToInsidePixels(mParameters.mXScale, mParameters.mYScale,
                                        mAppUnitsPerDevPixel);
  }

  nsIntRegion ScaleRegionToOutsidePixels(const nsRegion& aRegion, bool aSnap = false) const
  {
    if (aSnap && mSnappingEnabled) {
      return ScaleRegionToNearestPixels(aRegion);
    }
    return aRegion.ScaleToOutsidePixels(mParameters.mXScale, mParameters.mYScale,
                                        mAppUnitsPerDevPixel);
  }

  nsIFrame* GetContainerFrame() const { return mContainerFrame; }
  nsDisplayListBuilder* Builder() const { return mBuilder; }

  /**
   * Sets aOuterVisibleRegion as aLayer's visible region. aOuterVisibleRegion
   * is in the coordinate space of the container reference frame.
   * aLayerContentsVisibleRect, if non-null, is in the layer's own
   * coordinate system.
   */
  void SetOuterVisibleRegionForLayer(Layer* aLayer,
                                     const nsIntRegion& aOuterVisibleRegion,
                                     const nsIntRect* aLayerContentsVisibleRect = nullptr) const;

  /**
   * Try to determine whether the PaintedLayer aData has a single opaque color
   * covering aRect. If successful, return that color, otherwise return
   * NS_RGBA(0,0,0,0).
   * If aRect turns out not to intersect any content in the layer,
   * *aOutIntersectsLayer will be set to false.
   */
  nscolor FindOpaqueBackgroundColorInLayer(const PaintedLayerData* aData,
                                           const nsIntRect& aRect,
                                           bool* aOutIntersectsLayer) const;

  /**
   * Indicate that we are done adding items to the PaintedLayer represented by
   * aData. Make sure that a real PaintedLayer exists for it, and set the final
   * visible region and opaque-content.
   */
  template<typename FindOpaqueBackgroundColorCallbackType>
  void FinishPaintedLayerData(PaintedLayerData& aData, FindOpaqueBackgroundColorCallbackType aFindOpaqueBackgroundColor);

protected:
  friend class PaintedLayerData;

  LayerManager::PaintedLayerCreationHint
    GetLayerCreationHint(const nsIFrame* aAnimatedGeometryRoot);

  /**
   * Creates a new PaintedLayer and sets up the transform on the PaintedLayer
   * to account for scrolling.
   */
  already_AddRefed<PaintedLayer> CreatePaintedLayer(PaintedLayerData* aData);

  /**
   * Find a PaintedLayer for recycling, recycle it and prepare it for use, or
   * return null if no suitable layer was found.
   */
  already_AddRefed<PaintedLayer> AttemptToRecyclePaintedLayer(const nsIFrame* aAnimatedGeometryRoot,
                                                              nsDisplayItem* aItem,
                                                              const nsPoint& aTopLeft);
  /**
   * Recycle aLayer and do any necessary invalidation.
   */
  PaintedDisplayItemLayerUserData* RecyclePaintedLayer(PaintedLayer* aLayer,
                                                       const nsIFrame* aAnimatedGeometryRoot,
                                                       bool& didResetScrollPositionForLayerPixelAlignment);

  /**
   * Perform the last step of CreatePaintedLayer / AttemptToRecyclePaintedLayer:
   * Initialize aData, set up the layer's transform for scrolling, and
   * invalidate the layer for layer pixel alignment changes if necessary.
   */
  void PreparePaintedLayerForUse(PaintedLayer* aLayer,
                                 PaintedDisplayItemLayerUserData* aData,
                                 const nsIFrame* aAnimatedGeometryRoot,
                                 const nsIFrame* aReferenceFrame,
                                 const nsPoint& aTopLeft,
                                 bool aDidResetScrollPositionForLayerPixelAlignment);

  /**
   * Attempt to prepare an ImageLayer based upon the provided PaintedLayerData.
   * Returns nullptr on failure.
   */
  already_AddRefed<Layer> PrepareImageLayer(PaintedLayerData* aData);

  /**
   * Attempt to prepare a ColorLayer based upon the provided PaintedLayerData.
   * Returns nullptr on failure.
   */
  already_AddRefed<Layer> PrepareColorLayer(PaintedLayerData* aData);

  /**
   * Grab the next recyclable ColorLayer, or create one if there are no
   * more recyclable ColorLayers.
   */
  already_AddRefed<ColorLayer> CreateOrRecycleColorLayer(PaintedLayer* aPainted);
  /**
   * Grab the next recyclable ImageLayer, or create one if there are no
   * more recyclable ImageLayers.
   */
  already_AddRefed<ImageLayer> CreateOrRecycleImageLayer(PaintedLayer* aPainted);
  /**
   * Grab a recyclable ImageLayer for use as a mask layer for aLayer (that is a
   * mask layer which has been used for aLayer before), or create one if such
   * a layer doesn't exist.
   */
  already_AddRefed<ImageLayer> CreateOrRecycleMaskImageLayerFor(Layer* aLayer);
  /**
   * Grabs all PaintedLayers and ColorLayers from the ContainerLayer and makes them
   * available for recycling.
   */
  void CollectOldLayers();
  /**
   * If aItem used to belong to a PaintedLayer, invalidates the area of
   * aItem in that layer. If aNewLayer is a PaintedLayer, invalidates the area of
   * aItem in that layer.
   */
  void InvalidateForLayerChange(nsDisplayItem* aItem,
                                PaintedLayer* aNewLayer);
  /**
   * Find the fixed-pos frame, if any, containing (or equal to)
   * aAnimatedGeometryRoot. Only return a fixed-pos frame if its viewport
   * has a displayport.
   * aDisplayItemFixedToViewport is true if the layer contains a single display
   * item which returned true for ShouldFixToViewport.
   * This can return the actual viewport frame for layers whose display items
   * are directly on the viewport (e.g. background-attachment:fixed backgrounds).
   */
  const nsIFrame* FindFixedPosFrameForLayerData(const nsIFrame* aAnimatedGeometryRoot,
                                                bool aDisplayItemFixedToViewport);
  /**
   * Set fixed-pos layer metadata on aLayer according to the data for aFixedPosFrame.
   */
  void SetFixedPositionLayerData(Layer* aLayer,
                                 const nsIFrame* aFixedPosFrame);

  /**
   * Returns true if aItem's opaque area (in aOpaque) covers the entire
   * scrollable area of its presshell.
   */
  bool ItemCoversScrollableArea(nsDisplayItem* aItem, const nsRegion& aOpaque);

  /**
   * Set FrameMetrics and scroll-induced clipping on aEntry's layer.
   */
  void SetupScrollingMetadata(NewLayerEntry* aEntry);

  /**
   * Applies occlusion culling.
   * For each layer in mNewChildLayers, remove from its visible region the
   * opaque regions of the layers at higher z-index, but only if they have
   * the same animated geometry root and fixed-pos frame ancestor.
   * The opaque region for the child layers that share the same animated
   * geometry root as the container frame is returned in
   * *aOpaqueRegionForContainer.
   *
   * Also sets scroll metadata on the layers.
   */
  void PostprocessRetainedLayers(nsIntRegion* aOpaqueRegionForContainer);

  /**
   * Computes the snapped opaque area of aItem. Sets aList's opaque flag
   * if it covers the entire list bounds. Sets *aHideAllLayersBelow to true
   * this item covers the entire viewport so that all layers below are
   * permanently invisible.
   */
  nsIntRegion ComputeOpaqueRect(nsDisplayItem* aItem,
                                const nsIFrame* aAnimatedGeometryRoot,
                                const nsIFrame* aFixedPosFrame,
                                const DisplayItemClip& aClip,
                                nsDisplayList* aList,
                                bool* aHideAllLayersBelow,
                                bool* aOpaqueForAnimatedGeometryRootParent);

  /**
   * Return a PaintedLayerData object that is initialized for a layer that
   * aItem will be assigned to.
   * @param  aItem                 The item that is going to be added.
   * @param  aVisibleRect          The visible rect of the item.
   * @param  aAnimatedGeometryRoot The item's animated geometry root.
   * @param  aTopLeft              The offset between aAnimatedGeometryRoot and
   *                               the reference frame.
   * @param aShouldFixToViewport   If true, aAnimatedGeometryRoot is the
   *                               viewport and we will be adding fixed-pos
   *                               metadata for this layer because the display
   *                               item returned true from ShouldFixToViewport.
   */
  PaintedLayerData NewPaintedLayerData(nsDisplayItem* aItem,
                                       const nsIntRect& aVisibleRect,
                                       const nsIFrame* aAnimatedGeometryRoot,
                                       const nsPoint& aTopLeft,
                                       bool aShouldFixToViewport);

  /* Build a mask layer to represent the clipping region. Will return null if
   * there is no clipping specified or a mask layer cannot be built.
   * Builds an ImageLayer for the appropriate backend; the mask is relative to
   * aLayer's visible region.
   * aLayer is the layer to be clipped.
   * aLayerVisibleRegion is the region that will be set as aLayer's visible region,
   * relative to the container reference frame
   * aRoundedRectClipCount is used when building mask layers for PaintedLayers,
   * SetupMaskLayer will build a mask layer for only the first
   * aRoundedRectClipCount rounded rects in aClip
   */
  void SetupMaskLayer(Layer *aLayer, const DisplayItemClip& aClip,
                      const nsIntRegion& aLayerVisibleRegion,
                      uint32_t aRoundedRectClipCount = UINT32_MAX);

  bool ChooseAnimatedGeometryRoot(const nsDisplayList& aList,
                                  const nsIFrame **aAnimatedGeometryRoot);

  nsDisplayListBuilder*            mBuilder;
  LayerManager*                    mManager;
  FrameLayerBuilder*               mLayerBuilder;
  nsIFrame*                        mContainerFrame;
  nsIFrame*                        mContainerReferenceFrame;
  const nsIFrame*                  mContainerAnimatedGeometryRoot;
  const nsIFrame*                  mContainerFixedPosFrame;
  ContainerLayer*                  mContainerLayer;
  nsRect                           mContainerBounds;
  DebugOnly<nsRect>                mAccumulatedChildBounds;
  ContainerLayerParameters         mParameters;
  /**
   * The region of PaintedLayers that should be invalidated every time
   * we recycle one.
   */
  nsIntRegion                      mInvalidPaintedContent;
  PaintedLayerDataTree             mPaintedLayerDataTree;
  /**
   * We collect the list of children in here. During ProcessDisplayItems,
   * the layers in this array either have mContainerLayer as their parent,
   * or no parent.
   * PaintedLayers have two entries in this array: the second one is used only if
   * the PaintedLayer is optimized away to a ColorLayer or ImageLayer.
   * It's essential that this array is only appended to, since PaintedLayerData
   * records the index of its PaintedLayer in this array.
   */
  typedef nsAutoTArray<NewLayerEntry,1> AutoLayersArray;
  AutoLayersArray                  mNewChildLayers;
  nsTHashtable<nsRefPtrHashKey<PaintedLayer>> mPaintedLayersAvailableForRecycling;
  nsDataHashtable<nsPtrHashKey<Layer>, nsRefPtr<ImageLayer> >
    mRecycledMaskImageLayers;
  nscoord                          mAppUnitsPerDevPixel;
  bool                             mSnappingEnabled;
  bool                             mFlattenToSingleLayer;
};

class PaintedDisplayItemLayerUserData : public LayerUserData
{
public:
  PaintedDisplayItemLayerUserData() :
    mMaskClipCount(0),
    mForcedBackgroundColor(NS_RGBA(0,0,0,0)),
    mFontSmoothingBackgroundColor(NS_RGBA(0,0,0,0)),
    mXScale(1.f), mYScale(1.f),
    mAppUnitsPerDevPixel(0),
    mTranslation(0, 0),
    mAnimatedGeometryRootPosition(0, 0) {}

  /**
   * Record the number of clips in the PaintedLayer's mask layer.
   * Should not be reset when the layer is recycled since it is used to track
   * changes in the use of mask layers.
   */
  uint32_t mMaskClipCount;

  /**
   * A color that should be painted over the bounds of the layer's visible
   * region before any other content is painted.
   */
  nscolor mForcedBackgroundColor;

  /**
   * The target background color for smoothing fonts that are drawn on top of
   * transparent parts of the layer.
   */
  nscolor mFontSmoothingBackgroundColor;

  /**
   * The resolution scale used.
   */
  float mXScale, mYScale;

  /**
   * The appunits per dev pixel for the items in this layer.
   */
  nscoord mAppUnitsPerDevPixel;

  /**
   * The offset from the PaintedLayer's 0,0 to the
   * reference frame. This isn't necessarily the same as the transform
   * set on the PaintedLayer since we might also be applying an extra
   * offset specified by the parent ContainerLayer/
   */
  nsIntPoint mTranslation;

  /**
   * We try to make 0,0 of the PaintedLayer be the top-left of the
   * border-box of the "active scrolled root" frame (i.e. the nearest ancestor
   * frame for the display items that is being actively scrolled). But
   * we force the PaintedLayer transform to be an integer translation, and we may
   * have a resolution scale, so we have to snap the PaintedLayer transform, so
   * 0,0 may not be exactly the top-left of the active scrolled root. Here we
   * store the coordinates in PaintedLayer space of the top-left of the
   * active scrolled root.
   */
  gfxPoint mAnimatedGeometryRootPosition;

  nsIntRegion mRegionToInvalidate;

  // The offset between the active scrolled root of this layer
  // and the root of the container for the previous and current
  // paints respectively.
  nsPoint mLastAnimatedGeometryRootOrigin;
  nsPoint mAnimatedGeometryRootOrigin;

  // If mIgnoreInvalidationsOutsideRect is set, this contains the bounds of the
  // layer's old visible region, in layer pixels.
  nsIntRect mOldVisibleBounds;

  // If set, invalidations that fall outside of this rect should not result in
  // calls to layer->InvalidateRegion during DLBI. Instead, the parts outside
  // this rectangle will be invalidated in InvalidateVisibleBoundsChangesForScrolledLayer.
  // See the comment in ComputeAndSetIgnoreInvalidationRect for more information.
  Maybe<nsIntRect> mIgnoreInvalidationsOutsideRect;

  nsRefPtr<ColorLayer> mColorLayer;
  nsRefPtr<ImageLayer> mImageLayer;
};

/*
 * User data for layers which will be used as masks.
 */
struct MaskLayerUserData : public LayerUserData
{
  MaskLayerUserData()
    : mScaleX(-1.0f)
    , mScaleY(-1.0f)
    , mAppUnitsPerDevPixel(-1)
  { }

  bool
  operator== (const MaskLayerUserData& aOther) const
  {
    return mRoundedClipRects == aOther.mRoundedClipRects &&
           mScaleX == aOther.mScaleX &&
           mScaleY == aOther.mScaleY &&
           mOffset == aOther.mOffset &&
           mAppUnitsPerDevPixel == aOther.mAppUnitsPerDevPixel;
  }

  nsRefPtr<const MaskLayerImageCache::MaskLayerImageKey> mImageKey;
  // properties of the mask layer; the mask layer may be re-used if these
  // remain unchanged.
  nsTArray<DisplayItemClip::RoundedRect> mRoundedClipRects;
  // scale from the masked layer which is applied to the mask
  float mScaleX, mScaleY;
  // The ContainerLayerParameters offset which is applied to the mask's transform.
  nsIntPoint mOffset;
  int32_t mAppUnitsPerDevPixel;
};

/**
 * The address of gPaintedDisplayItemLayerUserData is used as the user
 * data key for PaintedLayers created by FrameLayerBuilder.
 * It identifies PaintedLayers used to draw non-layer content, which are
 * therefore eligible for recycling. We want display items to be able to
 * create their own dedicated PaintedLayers in BuildLayer, if necessary,
 * and we wouldn't want to accidentally recycle those.
 * The user data is a PaintedDisplayItemLayerUserData.
 */
uint8_t gPaintedDisplayItemLayerUserData;
/**
 * The address of gColorLayerUserData is used as the user
 * data key for ColorLayers created by FrameLayerBuilder.
 * The user data is null.
 */
uint8_t gColorLayerUserData;
/**
 * The address of gImageLayerUserData is used as the user
 * data key for ImageLayers created by FrameLayerBuilder.
 * The user data is null.
 */
uint8_t gImageLayerUserData;
/**
 * The address of gLayerManagerUserData is used as the user
 * data key for retained LayerManagers managed by FrameLayerBuilder.
 * The user data is a LayerManagerData.
 */
uint8_t gLayerManagerUserData;
/**
 * The address of gMaskLayerUserData is used as the user
 * data key for mask layers managed by FrameLayerBuilder.
 * The user data is a MaskLayerUserData.
 */
uint8_t gMaskLayerUserData;

/**
  * Helper functions for getting user data and casting it to the correct type.
  * aLayer is the layer where the user data is stored.
  */
MaskLayerUserData* GetMaskLayerUserData(Layer* aLayer)
{
  return static_cast<MaskLayerUserData*>(aLayer->GetUserData(&gMaskLayerUserData));
}

PaintedDisplayItemLayerUserData* GetPaintedDisplayItemLayerUserData(Layer* aLayer)
{
  return static_cast<PaintedDisplayItemLayerUserData*>(
    aLayer->GetUserData(&gPaintedDisplayItemLayerUserData));
}

/* static */ void
FrameLayerBuilder::Shutdown()
{
  if (gMaskLayerImageCache) {
    delete gMaskLayerImageCache;
    gMaskLayerImageCache = nullptr;
  }
}

void
FrameLayerBuilder::Init(nsDisplayListBuilder* aBuilder, LayerManager* aManager,
                        PaintedLayerData* aLayerData)
{
  mDisplayListBuilder = aBuilder;
  mRootPresContext = aBuilder->RootReferenceFrame()->PresContext()->GetRootPresContext();
  if (mRootPresContext) {
    mInitialDOMGeneration = mRootPresContext->GetDOMGeneration();
  }
  mContainingPaintedLayer = aLayerData;
  aManager->SetUserData(&gLayerManagerLayerBuilder, this);
}

void
FrameLayerBuilder::FlashPaint(gfxContext *aContext)
{
  float r = float(rand()) / RAND_MAX;
  float g = float(rand()) / RAND_MAX;
  float b = float(rand()) / RAND_MAX;
  aContext->SetColor(gfxRGBA(r, g, b, 0.4));
  aContext->Paint();
}

static FrameLayerBuilder::DisplayItemData*
AssertDisplayItemData(FrameLayerBuilder::DisplayItemData* aData)
{
  MOZ_RELEASE_ASSERT(aData);
  MOZ_RELEASE_ASSERT(sAliveDisplayItemDatas && sAliveDisplayItemDatas->Contains(aData));
  MOZ_RELEASE_ASSERT(aData->mLayer);
  return aData;
}

FrameLayerBuilder::DisplayItemData*
FrameLayerBuilder::GetDisplayItemData(nsIFrame* aFrame, uint32_t aKey)
{
  const nsTArray<DisplayItemData*>* array =
    static_cast<nsTArray<DisplayItemData*>*>(aFrame->Properties().Get(LayerManagerDataProperty()));
  if (array) {
    for (uint32_t i = 0; i < array->Length(); i++) {
      DisplayItemData* item = AssertDisplayItemData(array->ElementAt(i));
      if (item->mDisplayItemKey == aKey &&
          item->mLayer->Manager() == mRetainingManager) {
        return item;
      }
    }
  }
  return nullptr;
}

nsACString&
AppendToString(nsACString& s, const nsIntRect& r,
               const char* pfx="", const char* sfx="")
{
  s += pfx;
  s += nsPrintfCString(
    "(x=%d, y=%d, w=%d, h=%d)",
    r.x, r.y, r.width, r.height);
  return s += sfx;
}

nsACString&
AppendToString(nsACString& s, const nsIntRegion& r,
               const char* pfx="", const char* sfx="")
{
  s += pfx;

  nsIntRegionRectIterator it(r);
  s += "< ";
  while (const nsIntRect* sr = it.Next()) {
    AppendToString(s, *sr) += "; ";
  }
  s += ">";

  return s += sfx;
}

/**
 * Invalidate aRegion in aLayer. aLayer is in the coordinate system
 * *after* aTranslation has been applied, so we need to
 * apply the inverse of that transform before calling InvalidateRegion.
 */
template<typename RegionOrRect> void
InvalidatePostTransformRegion(PaintedLayer* aLayer, const RegionOrRect& aRegion,
                              const nsIntPoint& aTranslation,
                              PaintedDisplayItemLayerUserData* aData)
{
  // Convert the region from the coordinates of the container layer
  // (relative to the snapped top-left of the display list reference frame)
  // to the PaintedLayer's own coordinates
  RegionOrRect rgn = aRegion;
  rgn.MoveBy(-aTranslation);
  if (aData->mIgnoreInvalidationsOutsideRect) {
    rgn = rgn.Intersect(*aData->mIgnoreInvalidationsOutsideRect);
  }
  if (!rgn.IsEmpty()) {
    aLayer->InvalidateRegion(rgn);
#ifdef MOZ_DUMP_PAINTING
    if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
      nsAutoCString str;
      AppendToString(str, rgn);
      printf_stderr("Invalidating layer %p: %s\n", aLayer, str.get());
    }
#endif
  }
}

static void
InvalidatePostTransformRegion(PaintedLayer* aLayer, const nsRect& aRect,
                              const DisplayItemClip& aClip,
                              const nsIntPoint& aTranslation)
{
  PaintedDisplayItemLayerUserData* data =
      static_cast<PaintedDisplayItemLayerUserData*>(aLayer->GetUserData(&gPaintedDisplayItemLayerUserData));

  nsRect rect = aClip.ApplyNonRoundedIntersection(aRect);

  nsIntRect pixelRect = rect.ScaleToOutsidePixels(data->mXScale, data->mYScale, data->mAppUnitsPerDevPixel);
  InvalidatePostTransformRegion(aLayer, pixelRect, aTranslation, data);
}


static nsIntPoint
GetTranslationForPaintedLayer(PaintedLayer* aLayer)
{
  PaintedDisplayItemLayerUserData* data =
    static_cast<PaintedDisplayItemLayerUserData*>
      (aLayer->GetUserData(&gPaintedDisplayItemLayerUserData));
  NS_ASSERTION(data, "Must be a tracked painted layer!");

  return data->mTranslation;
}

/**
 * Some frames can have multiple, nested, retaining layer managers
 * associated with them (normal manager, inactive managers, SVG effects).
 * In these cases we store the 'outermost' LayerManager data property
 * on the frame since we can walk down the chain from there.
 *
 * If one of these frames has just been destroyed, we will free the inner
 * layer manager when removing the entry from mFramesWithLayers. Destroying
 * the layer manager destroys the LayerManagerData and calls into
 * the DisplayItemData destructor. If the inner layer manager had any
 * items with the same frame, then we attempt to retrieve properties
 * from the deleted frame.
 *
 * Cache the destroyed frame pointer here so we can avoid crashing in this case.
 */

/* static */ void
FrameLayerBuilder::RemoveFrameFromLayerManager(nsIFrame* aFrame,
                                               void* aPropertyValue)
{
  MOZ_RELEASE_ASSERT(!sDestroyedFrame);
  sDestroyedFrame = aFrame;
  nsTArray<DisplayItemData*> *array =
    reinterpret_cast<nsTArray<DisplayItemData*>*>(aPropertyValue);

  // Hold a reference to all the items so that they don't get
  // deleted from under us.
  nsTArray<nsRefPtr<DisplayItemData> > arrayCopy;
  for (uint32_t i = 0; i < array->Length(); ++i) {
    arrayCopy.AppendElement(array->ElementAt(i));
  }

#ifdef DEBUG_DISPLAY_ITEM_DATA
  if (array->Length()) {
    LayerManagerData *rootData = array->ElementAt(0)->mParent;
    while (rootData->mParent) {
      rootData = rootData->mParent;
    }
    printf_stderr("Removing frame %p - dumping display data\n", aFrame);
    rootData->Dump();
  }
#endif

  for (uint32_t i = 0; i < array->Length(); ++i) {
    DisplayItemData* data = array->ElementAt(i);

    PaintedLayer* t = data->mLayer->AsPaintedLayer();
    if (t) {
      PaintedDisplayItemLayerUserData* paintedData =
          static_cast<PaintedDisplayItemLayerUserData*>(t->GetUserData(&gPaintedDisplayItemLayerUserData));
      if (paintedData) {
        nsRegion old = data->mGeometry->ComputeInvalidationRegion();
        nsIntRegion rgn = old.ScaleToOutsidePixels(paintedData->mXScale, paintedData->mYScale, paintedData->mAppUnitsPerDevPixel);
        rgn.MoveBy(-GetTranslationForPaintedLayer(t));
        paintedData->mRegionToInvalidate.Or(paintedData->mRegionToInvalidate, rgn);
        paintedData->mRegionToInvalidate.SimplifyOutward(8);
      }
    }

    data->mParent->mDisplayItems.RemoveEntry(data);
  }

  arrayCopy.Clear();
  delete array;
  sDestroyedFrame = nullptr;
}

void
FrameLayerBuilder::DidBeginRetainedLayerTransaction(LayerManager* aManager)
{
  mRetainingManager = aManager;
  LayerManagerData* data = static_cast<LayerManagerData*>
    (aManager->GetUserData(&gLayerManagerUserData));
  if (data) {
    mInvalidateAllLayers = data->mInvalidateAllLayers;
  } else {
    data = new LayerManagerData(aManager);
    aManager->SetUserData(&gLayerManagerUserData, data);
  }
}

void
FrameLayerBuilder::StoreOptimizedLayerForFrame(nsDisplayItem* aItem, Layer* aLayer)
{
  if (!mRetainingManager) {
    return;
  }

  DisplayItemData* data = GetDisplayItemDataForManager(aItem, aLayer->Manager());
  NS_ASSERTION(data, "Must have already stored data for this item!");
  data->mOptLayer = aLayer;
}

void
FrameLayerBuilder::DidEndTransaction()
{
  GetMaskLayerImageCache()->Sweep();
}

void
FrameLayerBuilder::WillEndTransaction()
{
  if (!mRetainingManager) {
    return;
  }

  // We need to save the data we'll need to support retaining.
  LayerManagerData* data = static_cast<LayerManagerData*>
    (mRetainingManager->GetUserData(&gLayerManagerUserData));
  NS_ASSERTION(data, "Must have data!");
  // Update all the frames that used to have layers.
  data->mDisplayItems.EnumerateEntries(ProcessRemovedDisplayItems, this);
  data->mInvalidateAllLayers = false;
}

/* static */ PLDHashOperator
FrameLayerBuilder::ProcessRemovedDisplayItems(nsRefPtrHashKey<DisplayItemData>* aEntry,
                                              void* aUserArg)
{
  DisplayItemData* data = aEntry->GetKey();
  FrameLayerBuilder* layerBuilder = static_cast<FrameLayerBuilder*>(aUserArg);
  if (!data->mUsed) {
    // This item was visible, but isn't anymore.

    PaintedLayer* t = data->mLayer->AsPaintedLayer();
    if (t && data->mGeometry) {
#ifdef MOZ_DUMP_PAINTING
      if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
        printf_stderr("Invalidating unused display item (%i) belonging to frame %p from layer %p\n", data->mDisplayItemKey, data->mFrameList[0], t);
      }
#endif
      InvalidatePostTransformRegion(t,
                                    data->mGeometry->ComputeInvalidationRegion(),
                                    data->mClip,
                                    layerBuilder->GetLastPaintOffset(t));
    }
    return PL_DHASH_REMOVE;
  } else {
    layerBuilder->ComputeGeometryChangeForItem(data);
  }

  return PL_DHASH_NEXT;
}

/* static */ PLDHashOperator
FrameLayerBuilder::DumpDisplayItemDataForFrame(nsRefPtrHashKey<DisplayItemData>* aEntry,
                                               void* aClosure)
{
#ifdef DEBUG_DISPLAY_ITEM_DATA
  DisplayItemData *data = aEntry->GetKey();

  nsAutoCString prefix;
  prefix += static_cast<const char*>(aClosure);

  const char *layerState;
  switch (data->mLayerState) {
    case LAYER_NONE:
      layerState = "LAYER_NONE"; break;
    case LAYER_INACTIVE:
      layerState = "LAYER_INACTIVE"; break;
    case LAYER_ACTIVE:
      layerState = "LAYER_ACTIVE"; break;
    case LAYER_ACTIVE_FORCE:
      layerState = "LAYER_ACTIVE_FORCE"; break;
    case LAYER_ACTIVE_EMPTY:
      layerState = "LAYER_ACTIVE_EMPTY"; break;
    case LAYER_SVG_EFFECTS:
      layerState = "LAYER_SVG_EFFECTS"; break;
  }
  uint32_t mask = (1 << nsDisplayItem::TYPE_BITS) - 1;

  nsAutoCString str;
  str += prefix;
  str += nsPrintfCString("Frame %p ", data->mFrameList[0]);
  str += nsDisplayItem::DisplayItemTypeName(static_cast<nsDisplayItem::Type>(data->mDisplayItemKey & mask));
  if ((data->mDisplayItemKey >> nsDisplayItem::TYPE_BITS)) {
    str += nsPrintfCString("(%i)", data->mDisplayItemKey >> nsDisplayItem::TYPE_BITS);
  }
  str += nsPrintfCString(", %s, Layer %p", layerState, data->mLayer.get());
  if (data->mOptLayer) {
    str += nsPrintfCString(", OptLayer %p", data->mOptLayer.get());
  }
  if (data->mInactiveManager) {
    str += nsPrintfCString(", InactiveLayerManager %p", data->mInactiveManager.get());
  }
  str += "\n";

  printf_stderr("%s", str.get());

  if (data->mInactiveManager) {
    prefix += "  ";
    printf_stderr("%sDumping inactive layer info:\n", prefix.get());
    LayerManagerData* lmd = static_cast<LayerManagerData*>
      (data->mInactiveManager->GetUserData(&gLayerManagerUserData));
    lmd->Dump(prefix.get());
  }
#endif
  return PL_DHASH_NEXT;
}

/* static */ FrameLayerBuilder::DisplayItemData*
FrameLayerBuilder::GetDisplayItemDataForManager(nsDisplayItem* aItem,
                                                LayerManager* aManager)
{
  const nsTArray<DisplayItemData*>* array =
    static_cast<nsTArray<DisplayItemData*>*>(aItem->Frame()->Properties().Get(LayerManagerDataProperty()));
  if (array) {
    for (uint32_t i = 0; i < array->Length(); i++) {
      DisplayItemData* item = AssertDisplayItemData(array->ElementAt(i));
      if (item->mDisplayItemKey == aItem->GetPerFrameKey() &&
          item->mLayer->Manager() == aManager) {
        return item;
      }
    }
  }
  return nullptr;
}

bool
FrameLayerBuilder::HasRetainedDataFor(nsIFrame* aFrame, uint32_t aDisplayItemKey)
{
  const nsTArray<DisplayItemData*>* array =
    static_cast<nsTArray<DisplayItemData*>*>(aFrame->Properties().Get(LayerManagerDataProperty()));
  if (array) {
    for (uint32_t i = 0; i < array->Length(); i++) {
      if (AssertDisplayItemData(array->ElementAt(i))->mDisplayItemKey == aDisplayItemKey) {
        return true;
      }
    }
  }
  return false;
}

void
FrameLayerBuilder::IterateRetainedDataFor(nsIFrame* aFrame, DisplayItemDataCallback aCallback)
{
  const nsTArray<DisplayItemData*>* array =
    static_cast<nsTArray<DisplayItemData*>*>(aFrame->Properties().Get(LayerManagerDataProperty()));
  if (!array) {
    return;
  }

  for (uint32_t i = 0; i < array->Length(); i++) {
    DisplayItemData* data = AssertDisplayItemData(array->ElementAt(i));
    if (data->mDisplayItemKey != nsDisplayItem::TYPE_ZERO) {
      aCallback(aFrame, data);
    }
  }
}

FrameLayerBuilder::DisplayItemData*
FrameLayerBuilder::GetOldLayerForFrame(nsIFrame* aFrame, uint32_t aDisplayItemKey)
{
  // If we need to build a new layer tree, then just refuse to recycle
  // anything.
  if (!mRetainingManager || mInvalidateAllLayers)
    return nullptr;

  DisplayItemData *data = GetDisplayItemData(aFrame, aDisplayItemKey);

  if (data && data->mLayer->Manager() == mRetainingManager) {
    return data;
  }
  return nullptr;
}

Layer*
FrameLayerBuilder::GetOldLayerFor(nsDisplayItem* aItem,
                                  nsDisplayItemGeometry** aOldGeometry,
                                  DisplayItemClip** aOldClip)
{
  uint32_t key = aItem->GetPerFrameKey();
  nsIFrame* frame = aItem->Frame();

  DisplayItemData* oldData = GetOldLayerForFrame(frame, key);
  if (oldData) {
    if (aOldGeometry) {
      *aOldGeometry = oldData->mGeometry.get();
    }
    if (aOldClip) {
      *aOldClip = &oldData->mClip;
    }
    return oldData->mLayer;
  }

  return nullptr;
}

void
FrameLayerBuilder::ClearCachedGeometry(nsDisplayItem* aItem)
{
  uint32_t key = aItem->GetPerFrameKey();
  nsIFrame* frame = aItem->Frame();

  DisplayItemData* oldData = GetOldLayerForFrame(frame, key);
  if (oldData) {
    oldData->mGeometry = nullptr;
  }
}

/* static */ Layer*
FrameLayerBuilder::GetDebugOldLayerFor(nsIFrame* aFrame, uint32_t aDisplayItemKey)
{
  const nsTArray<DisplayItemData*>* array =
    static_cast<nsTArray<DisplayItemData*>*>(aFrame->Properties().Get(LayerManagerDataProperty()));

  if (!array) {
    return nullptr;
  }

  for (uint32_t i = 0; i < array->Length(); i++) {
    DisplayItemData *data = AssertDisplayItemData(array->ElementAt(i));

    if (data->mDisplayItemKey == aDisplayItemKey) {
      return data->mLayer;
    }
  }
  return nullptr;
}

/* static */ Layer*
FrameLayerBuilder::GetDebugSingleOldLayerForFrame(nsIFrame* aFrame)
{
  const nsTArray<DisplayItemData*>* array =
    static_cast<nsTArray<DisplayItemData*>*>(aFrame->Properties().Get(LayerManagerDataProperty()));

  if (!array) {
    return nullptr;
  }

  Layer* layer = nullptr;
  for (DisplayItemData* data : *array) {
    AssertDisplayItemData(data);
    if (layer && layer != data->mLayer) {
      // More than one layer assigned, bail.
      return nullptr;
    }
    layer = data->mLayer;
  }
  return layer;
}

already_AddRefed<ColorLayer>
ContainerState::CreateOrRecycleColorLayer(PaintedLayer *aPainted)
{
  PaintedDisplayItemLayerUserData* data =
      static_cast<PaintedDisplayItemLayerUserData*>(aPainted->GetUserData(&gPaintedDisplayItemLayerUserData));
  nsRefPtr<ColorLayer> layer = data->mColorLayer;
  if (layer) {
    layer->SetMaskLayer(nullptr);
    layer->ClearExtraDumpInfo();
  } else {
    // Create a new layer
    layer = mManager->CreateColorLayer();
    if (!layer)
      return nullptr;
    // Mark this layer as being used for painting display items
    data->mColorLayer = layer;
    layer->SetUserData(&gColorLayerUserData, nullptr);

    // Remove other layer types we might have stored for this PaintedLayer
    data->mImageLayer = nullptr;
  }
  return layer.forget();
}

already_AddRefed<ImageLayer>
ContainerState::CreateOrRecycleImageLayer(PaintedLayer *aPainted)
{
  PaintedDisplayItemLayerUserData* data =
      static_cast<PaintedDisplayItemLayerUserData*>(aPainted->GetUserData(&gPaintedDisplayItemLayerUserData));
  nsRefPtr<ImageLayer> layer = data->mImageLayer;
  if (layer) {
    layer->SetMaskLayer(nullptr);
    layer->ClearExtraDumpInfo();
  } else {
    // Create a new layer
    layer = mManager->CreateImageLayer();
    if (!layer)
      return nullptr;
    // Mark this layer as being used for painting display items
    data->mImageLayer = layer;
    layer->SetUserData(&gImageLayerUserData, nullptr);

    // Remove other layer types we might have stored for this PaintedLayer
    data->mColorLayer = nullptr;
  }
  return layer.forget();
}

already_AddRefed<ImageLayer>
ContainerState::CreateOrRecycleMaskImageLayerFor(Layer* aLayer)
{
  nsRefPtr<ImageLayer> result = mRecycledMaskImageLayers.Get(aLayer);
  if (result) {
    mRecycledMaskImageLayers.Remove(aLayer);
    aLayer->ClearExtraDumpInfo();
    // XXX if we use clip on mask layers, null it out here
  } else {
    // Create a new layer
    result = mManager->CreateImageLayer();
    if (!result)
      return nullptr;
    result->SetUserData(&gMaskLayerUserData, new MaskLayerUserData());
    result->SetDisallowBigImage(true);
  }

  return result.forget();
}

static const double SUBPIXEL_OFFSET_EPSILON = 0.02;

/**
 * This normally computes NSToIntRoundUp(aValue). However, if that would
 * give a residual near 0.5 while aOldResidual is near -0.5, or
 * it would give a residual near -0.5 while aOldResidual is near 0.5, then
 * instead we return the integer in the other direction so that the residual
 * is close to aOldResidual.
 */
static int32_t
RoundToMatchResidual(double aValue, double aOldResidual)
{
  int32_t v = NSToIntRoundUp(aValue);
  double residual = aValue - v;
  if (aOldResidual < 0) {
    if (residual > 0 && fabs(residual - 1.0 - aOldResidual) < SUBPIXEL_OFFSET_EPSILON) {
      // Round up instead
      return int32_t(ceil(aValue));
    }
  } else if (aOldResidual > 0) {
    if (residual < 0 && fabs(residual + 1.0 - aOldResidual) < SUBPIXEL_OFFSET_EPSILON) {
      // Round down instead
      return int32_t(floor(aValue));
    }
  }
  return v;
}

static void
ResetScrollPositionForLayerPixelAlignment(const nsIFrame* aAnimatedGeometryRoot)
{
  nsIScrollableFrame* sf = nsLayoutUtils::GetScrollableFrameFor(aAnimatedGeometryRoot);
  if (sf) {
    sf->ResetScrollPositionForLayerPixelAlignment();
  }
}

static void
InvalidateEntirePaintedLayer(PaintedLayer* aLayer, const nsIFrame* aAnimatedGeometryRoot, const char *aReason)
{
#ifdef MOZ_DUMP_PAINTING
  if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
    printf_stderr("Invalidating entire layer %p: %s\n", aLayer, aReason);
  }
#endif
  nsIntRect invalidate = aLayer->GetValidRegion().GetBounds();
  aLayer->InvalidateRegion(invalidate);
  aLayer->SetInvalidRectToVisibleRegion();
  ResetScrollPositionForLayerPixelAlignment(aAnimatedGeometryRoot);
}

LayerManager::PaintedLayerCreationHint
ContainerState::GetLayerCreationHint(const nsIFrame* aAnimatedGeometryRoot)
{
  // Check whether the layer will be scrollable. This is used as a hint to
  // influence whether tiled layers are used or not.
  if (mParameters.mInLowPrecisionDisplayPort) {
    return LayerManager::SCROLLABLE;
  }
  nsIFrame* animatedGeometryRootParent = aAnimatedGeometryRoot->GetParent();
  nsIScrollableFrame* scrollable = do_QueryFrame(animatedGeometryRootParent);
  if (scrollable && scrollable->WantAsyncScroll()) {
    // WantAsyncScroll() returns false when the frame has overflow:hidden,
    // so we won't create tiled layers for overflow:hidden frames even if
    // they have a display port. The main purpose of the WantAsyncScroll check
    // is to allow the B2G camera app to use hardware composer for compositing.
    return LayerManager::SCROLLABLE;
  }
  return LayerManager::NONE;
}

already_AddRefed<PaintedLayer>
ContainerState::AttemptToRecyclePaintedLayer(const nsIFrame* aAnimatedGeometryRoot,
                                             nsDisplayItem* aItem,
                                             const nsPoint& aTopLeft)
{
  Layer* oldLayer = mLayerBuilder->GetOldLayerFor(aItem);
  if (!oldLayer || !oldLayer->AsPaintedLayer() ||
      !mPaintedLayersAvailableForRecycling.Contains(oldLayer->AsPaintedLayer())) {
    return nullptr;
  }

  // Try to recycle a layer
  nsRefPtr<PaintedLayer> layer = oldLayer->AsPaintedLayer();
  mPaintedLayersAvailableForRecycling.RemoveEntry(layer);

  // Check if the layer hint has changed and whether or not the layer should
  // be recreated because of it.
  if (!mManager->IsOptimizedFor(layer, GetLayerCreationHint(aAnimatedGeometryRoot))) {
    return nullptr;
  }

  bool didResetScrollPositionForLayerPixelAlignment = false;
  PaintedDisplayItemLayerUserData* data =
    RecyclePaintedLayer(layer, aAnimatedGeometryRoot,
                        didResetScrollPositionForLayerPixelAlignment);
  PreparePaintedLayerForUse(layer, data, aAnimatedGeometryRoot, aItem->ReferenceFrame(),
                            aTopLeft,
                            didResetScrollPositionForLayerPixelAlignment);

  return layer.forget();
}

already_AddRefed<PaintedLayer>
ContainerState::CreatePaintedLayer(PaintedLayerData* aData)
{
  LayerManager::PaintedLayerCreationHint creationHint =
    GetLayerCreationHint(aData->mAnimatedGeometryRoot);

  // Create a new painted layer
  nsRefPtr<PaintedLayer> layer = mManager->CreatePaintedLayerWithHint(creationHint);
  if (!layer) {
    return nullptr;
  }

  // Mark this layer as being used for painting display items
  PaintedDisplayItemLayerUserData* userData = new PaintedDisplayItemLayerUserData();
  layer->SetUserData(&gPaintedDisplayItemLayerUserData, userData);
  ResetScrollPositionForLayerPixelAlignment(aData->mAnimatedGeometryRoot);

  PreparePaintedLayerForUse(layer, userData, aData->mAnimatedGeometryRoot,
                            aData->mReferenceFrame,
                            aData->mAnimatedGeometryRootOffset, true);

  return layer.forget();
}

PaintedDisplayItemLayerUserData*
ContainerState::RecyclePaintedLayer(PaintedLayer* aLayer,
                                    const nsIFrame* aAnimatedGeometryRoot,
                                    bool& didResetScrollPositionForLayerPixelAlignment)
{
  // Clear clip rect and mask layer so we don't accidentally stay clipped.
  // We will reapply any necessary clipping.
  aLayer->SetMaskLayer(nullptr);
  aLayer->ClearExtraDumpInfo();

  PaintedDisplayItemLayerUserData* data =
    static_cast<PaintedDisplayItemLayerUserData*>(
      aLayer->GetUserData(&gPaintedDisplayItemLayerUserData));
  NS_ASSERTION(data, "Recycled PaintedLayers must have user data");

  // This gets called on recycled PaintedLayers that are going to be in the
  // final layer tree, so it's a convenient time to invalidate the
  // content that changed where we don't know what PaintedLayer it belonged
  // to, or if we need to invalidate the entire layer, we can do that.
  // This needs to be done before we update the PaintedLayer to its new
  // transform. See nsGfxScrollFrame::InvalidateInternal, where
  // we ensure that mInvalidPaintedContent is updated according to the
  // scroll position as of the most recent paint.
  if (!FuzzyEqual(data->mXScale, mParameters.mXScale, 0.00001f) ||
      !FuzzyEqual(data->mYScale, mParameters.mYScale, 0.00001f) ||
      data->mAppUnitsPerDevPixel != mAppUnitsPerDevPixel) {
#ifdef MOZ_DUMP_PAINTING
  if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
    printf_stderr("Recycled layer %p changed scale\n", aLayer);
  }
#endif
    InvalidateEntirePaintedLayer(aLayer, aAnimatedGeometryRoot, "recycled layer changed state");
    didResetScrollPositionForLayerPixelAlignment = true;
  }
  if (!data->mRegionToInvalidate.IsEmpty()) {
#ifdef MOZ_DUMP_PAINTING
    if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
      printf_stderr("Invalidating deleted frame content from layer %p\n", aLayer);
    }
#endif
    aLayer->InvalidateRegion(data->mRegionToInvalidate);
#ifdef MOZ_DUMP_PAINTING
    if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
      nsAutoCString str;
      AppendToString(str, data->mRegionToInvalidate);
      printf_stderr("Invalidating layer %p: %s\n", aLayer, str.get());
    }
#endif
    data->mRegionToInvalidate.SetEmpty();
  }
  return data;
}

static void
ComputeAndSetIgnoreInvalidationRect(PaintedLayer* aLayer,
                                    PaintedDisplayItemLayerUserData* aData,
                                    const nsIFrame* aAnimatedGeometryRoot,
                                    nsDisplayListBuilder* aBuilder,
                                    const nsIntPoint& aLayerTranslation)
{
  if (!aLayer->Manager()->IsWidgetLayerManager()) {
    // This optimization is only useful for layers with retained content.
    return;
  }

  const nsIFrame* parentFrame = aAnimatedGeometryRoot->GetParent();

  // GetDirtyRectForScrolledContents will return an empty rect if parentFrame
  // is not a scrollable frame.
  nsRect dirtyRect = aBuilder->GetDirtyRectForScrolledContents(parentFrame);

  if (dirtyRect.IsEmpty()) {
    // parentFrame is not a scrollable frame, or we didn't encounter it during
    // display list building (though this shouldn't happen), or it's empty.
    // In all those cases this optimization is not needed.
    return;
  }

  // parentFrame is a scrollable frame, and aLayer contains the scrolled
  // contents of that frame.

  // maxNewVisibleBounds is a conservative approximation of the new visible
  // region of aLayer.
  nsIntRect maxNewVisibleBounds =
    dirtyRect.ScaleToOutsidePixels(aData->mXScale, aData->mYScale,
                                   aData->mAppUnitsPerDevPixel) - aLayerTranslation;
  aData->mOldVisibleBounds = aLayer->GetVisibleRegion().GetBounds();

  // When the visible region of aLayer changes (e.g. due to scrolling),
  // three distinct types of invalidations need to be triggered:
  //  (1) Items (or parts of items) that have left the visible region need
  //      to be invalidated so that the pixels they painted are no longer
  //      part of the layer's valid region.
  //  (2) Items (or parts of items) that weren't in the old visible region
  //      but are in the new visible region need to be invalidated. This
  //      invalidation isn't required for painting the right layer
  //      contents, because these items weren't part of the layer's valid
  //      region, so they'd be painted anyway. It is, however, necessary in
  //      order to get an accurate invalid region for the layer tree that
  //      aLayer is in, for example for partial compositing.
  //  (3) Any changes that happened in the intersection of the old and the
  //      new visible region need to be invalidated. There shouldn't be any
  //      of these when scrolling static content.
  //
  // We'd like to guarantee that we won't invalidate anything in the
  // intersection area of the old and the new visible region if all
  // invalidation are of type (1) and (2). However, if we just call
  // aLayer->InvalidateRegion for the invalidations of type (1) and (2),
  // at some point we'll hit the complexity limit of the layer's invalid
  // region. And the resulting region simplification can cause the region
  // to intersect with the intersection of the old and the new visible
  // region.
  // In order to get around this problem, we're using the following approach:
  //  - aData->mIgnoreInvalidationsOutsideRect is set to a conservative
  //    approximation of the intersection of the old and the new visible
  //    region. At this point we don't know the layer's new visible region.
  //  - As long as we don't know the layer's new visible region, we ignore all
  //    invalidations outside that rectangle, so roughly some of the
  //    invalidations of type (1) and (2).
  //  - Once we know the layer's new visible region, which happens at some
  //    point during PostprocessRetainedLayers, we invalidate a conservative
  //    approximation of (1) and (2). Specifically, we invalidate the region
  //    union of the old visible bounds and the new visible bounds, minus
  //    aData->mIgnoreInvalidationsOutsideRect. That region is simple enough
  //    that it will never be simplified on its own.
  //    We unset mIgnoreInvalidationsOutsideRect at this point.
  //  - Any other invalidations that happen on the layer after this point, e.g.
  //    during WillEndTransaction, will just happen regularly. If they are of
  //    type (1) or (2), they won't change the layer's invalid region because
  //    they fall inside the region we invalidated in the previous step.
  // Consequently, aData->mIgnoreInvalidationsOutsideRect is safe from
  // invalidations as long as there are no invalidations of type (3).
  aData->mIgnoreInvalidationsOutsideRect =
    Some(maxNewVisibleBounds.Intersect(aData->mOldVisibleBounds));
}

void
ContainerState::PreparePaintedLayerForUse(PaintedLayer* aLayer,
                                          PaintedDisplayItemLayerUserData* aData,
                                          const nsIFrame* aAnimatedGeometryRoot,
                                          const nsIFrame* aReferenceFrame,
                                          const nsPoint& aTopLeft,
                                          bool didResetScrollPositionForLayerPixelAlignment)
{
  aData->mXScale = mParameters.mXScale;
  aData->mYScale = mParameters.mYScale;
  aData->mLastAnimatedGeometryRootOrigin = aData->mAnimatedGeometryRootOrigin;
  aData->mAnimatedGeometryRootOrigin = aTopLeft;
  aData->mAppUnitsPerDevPixel = mAppUnitsPerDevPixel;
  aLayer->SetAllowResidualTranslation(mParameters.AllowResidualTranslation());

  mLayerBuilder->SavePreviousDataForLayer(aLayer, aData->mMaskClipCount);

  // Set up transform so that 0,0 in the PaintedLayer corresponds to the
  // (pixel-snapped) top-left of the aAnimatedGeometryRoot.
  nsPoint offset = aAnimatedGeometryRoot->GetOffsetToCrossDoc(aReferenceFrame);
  nscoord appUnitsPerDevPixel = aAnimatedGeometryRoot->PresContext()->AppUnitsPerDevPixel();
  gfxPoint scaledOffset(
      NSAppUnitsToDoublePixels(offset.x, appUnitsPerDevPixel)*mParameters.mXScale,
      NSAppUnitsToDoublePixels(offset.y, appUnitsPerDevPixel)*mParameters.mYScale);
  // We call RoundToMatchResidual here so that the residual after rounding
  // is close to aData->mAnimatedGeometryRootPosition if possible.
  nsIntPoint pixOffset(RoundToMatchResidual(scaledOffset.x, aData->mAnimatedGeometryRootPosition.x),
                       RoundToMatchResidual(scaledOffset.y, aData->mAnimatedGeometryRootPosition.y));
  aData->mTranslation = pixOffset;
  pixOffset += mParameters.mOffset;
  Matrix matrix = Matrix::Translation(pixOffset.x, pixOffset.y);
  aLayer->SetBaseTransform(Matrix4x4::From2D(matrix));

  ComputeAndSetIgnoreInvalidationRect(aLayer, aData, aAnimatedGeometryRoot, mBuilder, pixOffset);

  // FIXME: Temporary workaround for bug 681192 and bug 724786.
#ifndef MOZ_WIDGET_ANDROID
  // Calculate exact position of the top-left of the active scrolled root.
  // This might not be 0,0 due to the snapping in ScaleToNearestPixels.
  gfxPoint animatedGeometryRootTopLeft = scaledOffset - ThebesPoint(matrix.GetTranslation()) + mParameters.mOffset;
  // If it has changed, then we need to invalidate the entire layer since the
  // pixels in the layer buffer have the content at a (subpixel) offset
  // from what we need.
  if (!animatedGeometryRootTopLeft.WithinEpsilonOf(aData->mAnimatedGeometryRootPosition, SUBPIXEL_OFFSET_EPSILON)) {
    aData->mAnimatedGeometryRootPosition = animatedGeometryRootTopLeft;
    InvalidateEntirePaintedLayer(aLayer, aAnimatedGeometryRoot, "subpixel offset");
  } else if (didResetScrollPositionForLayerPixelAlignment) {
    aData->mAnimatedGeometryRootPosition = animatedGeometryRootTopLeft;
  }
#else
  unused << didResetScrollPositionForLayerPixelAlignment;
#endif
}

#if defined(DEBUG) || defined(MOZ_DUMP_PAINTING)
/**
 * Returns the appunits per dev pixel for the item's frame
 */
static int32_t
AppUnitsPerDevPixel(nsDisplayItem* aItem)
{
  // The underlying frame for zoom items is the root frame of the subdocument.
  // But zoom display items report their bounds etc using the parent document's
  // APD because zoom items act as a conversion layer between the two different
  // APDs.
  if (aItem->GetType() == nsDisplayItem::TYPE_ZOOM) {
    return static_cast<nsDisplayZoom*>(aItem)->GetParentAppUnitsPerDevPixel();
  }
  return aItem->Frame()->PresContext()->AppUnitsPerDevPixel();
}
#endif

/**
 * Set the visible region for aLayer.
 * aOuterVisibleRegion is the visible region relative to the parent layer.
 * aLayerContentsVisibleRect, if non-null, is a rectangle in the layer's
 * own coordinate system to which the layer's visible region is restricted.
 * Consumes *aOuterVisibleRegion.
 */
static void
SetOuterVisibleRegion(Layer* aLayer, nsIntRegion* aOuterVisibleRegion,
                      const nsIntRect* aLayerContentsVisibleRect = nullptr)
{
  Matrix4x4 transform = aLayer->GetTransform();
  Matrix transform2D;
  if (transform.Is2D(&transform2D) && !transform2D.HasNonIntegerTranslation()) {
    aOuterVisibleRegion->MoveBy(-int(transform2D._31), -int(transform2D._32));
    if (aLayerContentsVisibleRect) {
      aOuterVisibleRegion->And(*aOuterVisibleRegion, *aLayerContentsVisibleRect);
    }
  } else {
    nsIntRect outerRect = aOuterVisibleRegion->GetBounds();
    // if 'transform' is not invertible, then nothing will be displayed
    // for the layer, so it doesn't really matter what we do here
    Rect outerVisible(outerRect.x, outerRect.y, outerRect.width, outerRect.height);
    transform.Invert();

    Rect layerContentsVisible(-float(INT32_MAX) / 2, -float(INT32_MAX) / 2,
                              float(INT32_MAX), float(INT32_MAX));
    if (aLayerContentsVisibleRect) {
      NS_ASSERTION(aLayerContentsVisibleRect->width >= 0 &&
                   aLayerContentsVisibleRect->height >= 0,
                   "Bad layer contents rectangle");
      // restrict to aLayerContentsVisibleRect before call GfxRectToIntRect,
      // in case layerVisible is extremely large (as it can be when
      // projecting through the inverse of a 3D transform)
      layerContentsVisible = Rect(
          aLayerContentsVisibleRect->x, aLayerContentsVisibleRect->y,
          aLayerContentsVisibleRect->width, aLayerContentsVisibleRect->height);
    }
    gfxRect layerVisible = ThebesRect(transform.ProjectRectBounds(outerVisible, layerContentsVisible));
    layerVisible.RoundOut();
    nsIntRect visRect;
    if (gfxUtils::GfxRectToIntRect(layerVisible, &visRect)) {
      *aOuterVisibleRegion = visRect;
    } else  {
      aOuterVisibleRegion->SetEmpty();
    }
  }

  aLayer->SetVisibleRegion(*aOuterVisibleRegion);
}

void
ContainerState::SetOuterVisibleRegionForLayer(Layer* aLayer,
                                              const nsIntRegion& aOuterVisibleRegion,
                                              const nsIntRect* aLayerContentsVisibleRect) const
{
  nsIntRegion visRegion = aOuterVisibleRegion;
  visRegion.MoveBy(mParameters.mOffset);
  SetOuterVisibleRegion(aLayer, &visRegion, aLayerContentsVisibleRect);
}

nscolor
ContainerState::FindOpaqueBackgroundColorInLayer(const PaintedLayerData* aData,
                                                 const nsIntRect& aRect,
                                                 bool* aOutIntersectsLayer) const
{
  *aOutIntersectsLayer = true;

  // Scan the candidate's display items.
  nsIntRect deviceRect = aRect;
  nsRect appUnitRect = ToAppUnits(deviceRect, mAppUnitsPerDevPixel);
  appUnitRect.ScaleInverseRoundOut(mParameters.mXScale, mParameters.mYScale);

  for (auto& assignedItem : Reversed(aData->mAssignedDisplayItems)) {
    nsDisplayItem* item = assignedItem.mItem;
    bool snap;
    nsRect bounds = item->GetBounds(mBuilder, &snap);
    if (snap && mSnappingEnabled) {
      nsIntRect snappedBounds = ScaleToNearestPixels(bounds);
      if (!snappedBounds.Intersects(deviceRect))
        continue;

      if (!snappedBounds.Contains(deviceRect))
        return NS_RGBA(0,0,0,0);

    } else {
      // The layer's visible rect is already (close enough to) pixel
      // aligned, so no need to round out and in here.
      if (!bounds.Intersects(appUnitRect))
        continue;

      if (!bounds.Contains(appUnitRect))
        return NS_RGBA(0,0,0,0);
    }

    if (item->IsInvisibleInRect(appUnitRect)) {
      continue;
    }

    if (assignedItem.mClip.IsRectAffectedByClip(deviceRect,
                                                mParameters.mXScale,
                                                mParameters.mYScale,
                                                mAppUnitsPerDevPixel)) {
      return NS_RGBA(0,0,0,0);
    }

    nscolor color;
    if (item->IsUniform(mBuilder, &color) && NS_GET_A(color) == 255)
      return color;

    return NS_RGBA(0,0,0,0);
  }

  *aOutIntersectsLayer = false;
  return NS_RGBA(0,0,0,0);
}

nscolor
PaintedLayerDataNode::FindOpaqueBackgroundColor(const nsIntRegion& aTargetVisibleRegion,
                                                int32_t aUnderIndex) const
{
  if (aUnderIndex == ABOVE_TOP) {
    aUnderIndex = mPaintedLayerDataStack.Length();
  }
  for (int32_t i = aUnderIndex - 1; i >= 0; --i) {
    const PaintedLayerData* candidate = &mPaintedLayerDataStack[i];
    if (candidate->VisibleAboveRegionIntersects(aTargetVisibleRegion)) {
      // Some non-PaintedLayer content between target and candidate; this is
      // hopeless
      return NS_RGBA(0,0,0,0);
    }

    if (!candidate->VisibleRegionIntersects(aTargetVisibleRegion)) {
      // The layer doesn't intersect our target, ignore it and move on
      continue;
    }

    bool intersectsLayer = true;
    nsIntRect rect = aTargetVisibleRegion.GetBounds();
    nscolor color = mTree.ContState().FindOpaqueBackgroundColorInLayer(
                                        candidate, rect, &intersectsLayer);
    if (!intersectsLayer) {
      continue;
    }
    return color;
  }
  if (mAllDrawingAboveBackground ||
      !mVisibleAboveBackgroundRegion.Intersect(aTargetVisibleRegion).IsEmpty()) {
    // Some non-PaintedLayer content is between this node's background and target.
    return NS_RGBA(0,0,0,0);
  }
  return FindOpaqueBackgroundColorInParentNode();
}

nscolor
PaintedLayerDataNode::FindOpaqueBackgroundColorCoveringEverything() const
{
  if (!mPaintedLayerDataStack.IsEmpty() ||
      mAllDrawingAboveBackground ||
      !mVisibleAboveBackgroundRegion.IsEmpty()) {
    return NS_RGBA(0,0,0,0);
  }
  return FindOpaqueBackgroundColorInParentNode();
}

nscolor
PaintedLayerDataNode::FindOpaqueBackgroundColorInParentNode() const
{
  if (mParent) {
    if (mHasClip) {
      // Check whether our parent node has uniform content behind our whole
      // clip.
      // There's one tricky case here: If our parent node is also a scrollable,
      // and is currently scrolled in such a way that this inner one is
      // clipped by it, then it's not really clear how we should determine
      // whether we have a uniform background in the parent: There might be
      // non-uniform content in the parts that our scroll port covers in the
      // parent and that are currently outside the parent's clip.
      // For now, we'll fail to pull a background color in that case.
      return mParent->FindOpaqueBackgroundColor(mClipRect);
    }
    return mParent->FindOpaqueBackgroundColorCoveringEverything();
  }
  // We are the root.
  return mTree.UniformBackgroundColor();
}

void
PaintedLayerData::UpdateCommonClipCount(
    const DisplayItemClip& aCurrentClip)
{
  if (mCommonClipCount >= 0) {
    mCommonClipCount = mItemClip.GetCommonRoundedRectCount(aCurrentClip, mCommonClipCount);
  } else {
    // first item in the layer
    mCommonClipCount = aCurrentClip.GetRoundedRectCount();
  }
}

bool
PaintedLayerData::CanOptimizeToImageLayer(nsDisplayListBuilder* aBuilder)
{
  if (!mImage) {
    return false;
  }

  return mImage->CanOptimizeToImageLayer(mLayer->Manager(), aBuilder);
}

already_AddRefed<ImageContainer>
PaintedLayerData::GetContainerForImageLayer(nsDisplayListBuilder* aBuilder)
{
  if (!mImage) {
    return nullptr;
  }

  return mImage->GetContainer(mLayer->Manager(), aBuilder);
}

PaintedLayerDataNode::PaintedLayerDataNode(PaintedLayerDataTree& aTree,
                                           PaintedLayerDataNode* aParent,
                                           const nsIFrame* aAnimatedGeometryRoot)
  : mTree(aTree)
  , mParent(aParent)
  , mAnimatedGeometryRoot(aAnimatedGeometryRoot)
  , mAllDrawingAboveBackground(false)
{
  MOZ_ASSERT(nsLayoutUtils::IsAncestorFrameCrossDoc(mTree.Builder()->RootReferenceFrame(), mAnimatedGeometryRoot));
  mHasClip = mTree.IsClippedWithRespectToParentAnimatedGeometryRoot(mAnimatedGeometryRoot, &mClipRect);
}

PaintedLayerDataNode::~PaintedLayerDataNode()
{
  MOZ_ASSERT(mPaintedLayerDataStack.IsEmpty());
  MOZ_ASSERT(mChildren.IsEmpty());
}

PaintedLayerDataNode*
PaintedLayerDataNode::AddChildNodeFor(const nsIFrame* aAnimatedGeometryRoot)
{
  MOZ_ASSERT(mTree.GetParentAnimatedGeometryRoot(aAnimatedGeometryRoot) == mAnimatedGeometryRoot);
  UniquePtr<PaintedLayerDataNode> child =
    MakeUnique<PaintedLayerDataNode>(mTree, this, aAnimatedGeometryRoot);
  mChildren.AppendElement(Move(child));
  return mChildren.LastElement().get();
}

template<typename NewPaintedLayerCallbackType>
PaintedLayerData*
PaintedLayerDataNode::FindPaintedLayerFor(const nsIntRect& aVisibleRect,
                                          NewPaintedLayerCallbackType aNewPaintedLayerCallback)
{
  if (!mPaintedLayerDataStack.IsEmpty()) {
    if (mPaintedLayerDataStack[0].mSingleItemFixedToViewport) {
      MOZ_ASSERT(mPaintedLayerDataStack.Length() == 1);
      SetAllDrawingAbove();
      MOZ_ASSERT(mPaintedLayerDataStack.IsEmpty());
    } else {
      PaintedLayerData* lowestUsableLayer = nullptr;
      for (auto& data : Reversed(mPaintedLayerDataStack)) {
        if (data.VisibleAboveRegionIntersects(aVisibleRect)) {
          break;
        }
        MOZ_ASSERT(!data.mSingleItemFixedToViewport);
        lowestUsableLayer = &data;
        if (data.VisibleRegionIntersects(aVisibleRect)) {
          break;
        }
      }
      if (lowestUsableLayer) {
        return lowestUsableLayer;
      }
    }
  }
  return mPaintedLayerDataStack.AppendElement(aNewPaintedLayerCallback());
}

void
PaintedLayerDataNode::FinishChildrenIntersecting(const nsIntRect& aRect)
{
  for (int32_t i = mChildren.Length() - 1; i >= 0; i--) {
    if (mChildren[i]->Intersects(aRect)) {
      mChildren[i]->Finish(true);
      mChildren.RemoveElementAt(i);
    }
  }
}

void
PaintedLayerDataNode::FinishAllChildren(bool aThisNodeNeedsAccurateVisibleAboveRegion)
{
  for (int32_t i = mChildren.Length() - 1; i >= 0; i--) {
    mChildren[i]->Finish(aThisNodeNeedsAccurateVisibleAboveRegion);
  }
  mChildren.Clear();
}

void
PaintedLayerDataNode::Finish(bool aParentNeedsAccurateVisibleAboveRegion)
{
  // Skip "visible above region" maintenance, because this node is going away.
  FinishAllChildren(false);

  PopAllPaintedLayerData();

  if (mParent && aParentNeedsAccurateVisibleAboveRegion) {
    if (mHasClip) {
      mParent->AddToVisibleAboveRegion(mClipRect);
    } else {
      mParent->SetAllDrawingAbove();
    }
  }
  mTree.NodeWasFinished(mAnimatedGeometryRoot);
}

void
PaintedLayerDataNode::AddToVisibleAboveRegion(const nsIntRect& aRect)
{
  nsIntRegion& visibleAboveRegion = mPaintedLayerDataStack.IsEmpty()
    ? mVisibleAboveBackgroundRegion
    : mPaintedLayerDataStack.LastElement().mVisibleAboveRegion;
  visibleAboveRegion.Or(visibleAboveRegion, aRect);
  visibleAboveRegion.SimplifyOutward(8);
}

void
PaintedLayerDataNode::SetAllDrawingAbove()
{
  PopAllPaintedLayerData();
  mAllDrawingAboveBackground = true;
  mVisibleAboveBackgroundRegion.SetEmpty();
}

void
PaintedLayerDataNode::PopPaintedLayerData()
{
  MOZ_ASSERT(!mPaintedLayerDataStack.IsEmpty());
  size_t lastIndex = mPaintedLayerDataStack.Length() - 1;
  PaintedLayerData& data = mPaintedLayerDataStack[lastIndex];
  mTree.ContState().FinishPaintedLayerData(data, [this, &data, lastIndex]() {
    return this->FindOpaqueBackgroundColor(data.mVisibleRegion, lastIndex);
  });
  mPaintedLayerDataStack.RemoveElementAt(lastIndex);
}

void
PaintedLayerDataNode::PopAllPaintedLayerData()
{
  while (!mPaintedLayerDataStack.IsEmpty()) {
    PopPaintedLayerData();
  }
}

nsDisplayListBuilder*
PaintedLayerDataTree::Builder() const
{
  return mContainerState.Builder();
}

const nsIFrame*
PaintedLayerDataTree::GetParentAnimatedGeometryRoot(const nsIFrame* aAnimatedGeometryRoot)
{
  MOZ_ASSERT(aAnimatedGeometryRoot);
  MOZ_ASSERT(nsLayoutUtils::IsAncestorFrameCrossDoc(Builder()->RootReferenceFrame(), aAnimatedGeometryRoot));

  if (aAnimatedGeometryRoot == Builder()->RootReferenceFrame()) {
    return nullptr;
  }

  nsIFrame* agr = Builder()->FindAnimatedGeometryRootFor(
    const_cast<nsIFrame*>(aAnimatedGeometryRoot), Builder()->RootReferenceFrame());
  MOZ_ASSERT_IF(agr, nsLayoutUtils::IsAncestorFrameCrossDoc(Builder()->RootReferenceFrame(), agr));
  if (agr != aAnimatedGeometryRoot) {
    return agr;
  }
  // aAnimatedGeometryRoot is its own animated geometry root.
  // Find the animated geometry root for its cross-doc parent frame.
  nsIFrame* parent = nsLayoutUtils::GetCrossDocParentFrame(aAnimatedGeometryRoot);
  if (!parent) {
    return nullptr;
  }
  return Builder()->FindAnimatedGeometryRootFor(parent, Builder()->RootReferenceFrame());
}

void
PaintedLayerDataTree::Finish()
{
  if (mRoot) {
    mRoot->Finish(false);
  }
  MOZ_ASSERT(mNodes.Count() == 0);
  mRoot = nullptr;
}

void
PaintedLayerDataTree::NodeWasFinished(const nsIFrame* aAnimatedGeometryRoot)
{
  mNodes.Remove(aAnimatedGeometryRoot);
}

void
PaintedLayerDataTree::AddingOwnLayer(const nsIFrame* aAnimatedGeometryRoot,
                                     const nsIntRect* aRect,
                                     nscolor* aOutUniformBackgroundColor)
{
  FinishPotentiallyIntersectingNodes(aAnimatedGeometryRoot, aRect);
  PaintedLayerDataNode* node = EnsureNodeFor(aAnimatedGeometryRoot);
  if (aRect) {
    if (aOutUniformBackgroundColor) {
      *aOutUniformBackgroundColor = node->FindOpaqueBackgroundColor(*aRect);
    }
    node->AddToVisibleAboveRegion(*aRect);
  } else {
    if (aOutUniformBackgroundColor) {
      *aOutUniformBackgroundColor = node->FindOpaqueBackgroundColorCoveringEverything();
    }
    node->SetAllDrawingAbove();
  }
}

template<typename NewPaintedLayerCallbackType>
PaintedLayerData*
PaintedLayerDataTree::FindPaintedLayerFor(const nsIFrame* aAnimatedGeometryRoot,
                                          const nsIntRect& aVisibleRect,
                                          bool aShouldFixToViewport,
                                          NewPaintedLayerCallbackType aNewPaintedLayerCallback)
{
  const nsIntRect* bounds = aShouldFixToViewport ? nullptr : &aVisibleRect;
  FinishPotentiallyIntersectingNodes(aAnimatedGeometryRoot, bounds);
  PaintedLayerDataNode* node = EnsureNodeFor(aAnimatedGeometryRoot);
  if (aShouldFixToViewport) {
    node->SetAllDrawingAbove();
  }
  return node->FindPaintedLayerFor(aVisibleRect, aNewPaintedLayerCallback);
}

void
PaintedLayerDataTree::FinishPotentiallyIntersectingNodes(const nsIFrame* aAnimatedGeometryRoot,
                                                         const nsIntRect* aRect)
{
  const nsIFrame* ancestorThatIsChildOfCommonAncestor = nullptr;
  PaintedLayerDataNode* ancestorNode =
    FindNodeForAncestorAnimatedGeometryRoot(aAnimatedGeometryRoot,
                                            &ancestorThatIsChildOfCommonAncestor);
  if (!ancestorNode) {
    // None of our ancestors are in the tree. This should only happen if this
    // is the very first item we're looking at.
    MOZ_ASSERT(!mRoot);
    return;
  }

  if (ancestorNode->AnimatedGeometryRoot() == aAnimatedGeometryRoot) {
    // aAnimatedGeometryRoot already has a node in the tree.
    // This is the common case.
    MOZ_ASSERT(!ancestorThatIsChildOfCommonAncestor);
    if (aRect) {
      ancestorNode->FinishChildrenIntersecting(*aRect);
    } else {
      ancestorNode->FinishAllChildren();
    }
    return;
  }

  // We have found an existing ancestor, but it's a proper ancestor of our
  // animated geometry root.
  // ancestorThatIsChildOfCommonAncestor is the last animated geometry root
  // encountered on the way up from aAnimatedGeometryRoot to ancestorNode.
  MOZ_ASSERT(ancestorThatIsChildOfCommonAncestor);
  MOZ_ASSERT(nsLayoutUtils::IsAncestorFrameCrossDoc(ancestorThatIsChildOfCommonAncestor, aAnimatedGeometryRoot));
  MOZ_ASSERT(GetParentAnimatedGeometryRoot(ancestorThatIsChildOfCommonAncestor) == ancestorNode->AnimatedGeometryRoot());

  // ancestorThatIsChildOfCommonAncestor is not in the tree yet!
  MOZ_ASSERT(!mNodes.Get(ancestorThatIsChildOfCommonAncestor));

  // We're about to add a node for ancestorThatIsChildOfCommonAncestor, so we
  // finish all intersecting siblings.
  nsIntRect clip;
  if (IsClippedWithRespectToParentAnimatedGeometryRoot(ancestorThatIsChildOfCommonAncestor, &clip)) {
    ancestorNode->FinishChildrenIntersecting(clip);
  } else {
    ancestorNode->FinishAllChildren();
  }
}

PaintedLayerDataNode*
PaintedLayerDataTree::EnsureNodeFor(const nsIFrame* aAnimatedGeometryRoot)
{
  MOZ_ASSERT(aAnimatedGeometryRoot);
  PaintedLayerDataNode* node = mNodes.Get(aAnimatedGeometryRoot);
  if (node) {
    return node;
  }

  const nsIFrame* parentAnimatedGeometryRoot = GetParentAnimatedGeometryRoot(aAnimatedGeometryRoot);
  if (!parentAnimatedGeometryRoot) {
    MOZ_ASSERT(!mRoot);
    MOZ_ASSERT(aAnimatedGeometryRoot == Builder()->RootReferenceFrame());
    mRoot = MakeUnique<PaintedLayerDataNode>(*this, nullptr, aAnimatedGeometryRoot);
    node = mRoot.get();
  } else {
    PaintedLayerDataNode* parentNode = EnsureNodeFor(parentAnimatedGeometryRoot);
    MOZ_ASSERT(parentNode);
    node = parentNode->AddChildNodeFor(aAnimatedGeometryRoot);
  }
  MOZ_ASSERT(node);
  mNodes.Put(aAnimatedGeometryRoot, node);
  return node;
}

bool
PaintedLayerDataTree::IsClippedWithRespectToParentAnimatedGeometryRoot(const nsIFrame* aAnimatedGeometryRoot,
                                                                       nsIntRect* aOutClip)
{
  nsIScrollableFrame* scrollableFrame = nsLayoutUtils::GetScrollableFrameFor(aAnimatedGeometryRoot);
  if (!scrollableFrame) {
    return false;
  }
  nsIFrame* scrollFrame = do_QueryFrame(scrollableFrame);
  nsRect scrollPort = scrollableFrame->GetScrollPortRect() + Builder()->ToReferenceFrame(scrollFrame);
  *aOutClip = mContainerState.ScaleToNearestPixels(scrollPort);
  return true;
}

PaintedLayerDataNode*
PaintedLayerDataTree::FindNodeForAncestorAnimatedGeometryRoot(const nsIFrame* aAnimatedGeometryRoot,
                                                              const nsIFrame** aOutAncestorChild)
{
  if (!aAnimatedGeometryRoot) {
    return nullptr;
  }
  PaintedLayerDataNode* node = mNodes.Get(aAnimatedGeometryRoot);
  if (node) {
    return node;
  }
  *aOutAncestorChild = aAnimatedGeometryRoot;
  return FindNodeForAncestorAnimatedGeometryRoot(
    GetParentAnimatedGeometryRoot(aAnimatedGeometryRoot), aOutAncestorChild);
}

const nsIFrame*
ContainerState::FindFixedPosFrameForLayerData(const nsIFrame* aAnimatedGeometryRoot,
                                              bool aDisplayItemFixedToViewport)
{
  if (!mManager->IsWidgetLayerManager()) {
    // Never attach any fixed-pos metadata to inactive layers, it's pointless!
    return nullptr;
  }

  nsPresContext* presContext = mContainerFrame->PresContext();
  nsIFrame* viewport = presContext->PresShell()->GetRootFrame();

  if (viewport == aAnimatedGeometryRoot && aDisplayItemFixedToViewport &&
      nsLayoutUtils::ViewportHasDisplayPort(presContext)) {
    // Probably a background-attachment:fixed item
    return viewport;
  }
  // Viewports with no fixed-pos frames are not relevant.
  if (!viewport->GetFirstChild(nsIFrame::kFixedList)) {
    return nullptr;
  }
  for (const nsIFrame* f = aAnimatedGeometryRoot; f; f = f->GetParent()) {
    if (nsLayoutUtils::IsFixedPosFrameInDisplayPort(f)) {
      return f;
    }
    if (f == mContainerReferenceFrame) {
      // The metadata will go on an ancestor layer if necessary.
      return nullptr;
    }
  }
  return nullptr;
}

void
ContainerState::SetFixedPositionLayerData(Layer* aLayer,
                                          const nsIFrame* aFixedPosFrame)
{
  aLayer->SetIsFixedPosition(aFixedPosFrame != nullptr);
  if (!aFixedPosFrame) {
    return;
  }

  nsPresContext* presContext = aFixedPosFrame->PresContext();

  const nsIFrame* viewportFrame = aFixedPosFrame->GetParent();
  // anchorRect will be in the container's coordinate system (aLayer's parent layer).
  // This is the same as the display items' reference frame.
  nsRect anchorRect;
  if (viewportFrame) {
    // Fixed position frames are reflowed into the scroll-port size if one has
    // been set.
    if (presContext->PresShell()->IsScrollPositionClampingScrollPortSizeSet()) {
      anchorRect.SizeTo(presContext->PresShell()->GetScrollPositionClampingScrollPortSize());
    } else {
      anchorRect.SizeTo(viewportFrame->GetSize());
    }
  } else {
    // A display item directly attached to the viewport.
    // For background-attachment:fixed items, the anchor point is always the
    // top-left of the viewport currently.
    viewportFrame = aFixedPosFrame;
  }
  // The anchorRect top-left is always the viewport top-left.
  anchorRect.MoveTo(viewportFrame->GetOffsetToCrossDoc(mContainerReferenceFrame));

  nsLayoutUtils::SetFixedPositionLayerData(aLayer,
      viewportFrame, anchorRect, aFixedPosFrame, presContext, mParameters);
}

static bool
CanOptimizeAwayPaintedLayer(PaintedLayerData* aData,
                           FrameLayerBuilder* aLayerBuilder)
{
  if (!aLayerBuilder->IsBuildingRetainedLayers()) {
    return false;
  }

  // If there's no painted layer with valid content in it that we can reuse,
  // always create a color or image layer (and potentially throw away an
  // existing completely invalid painted layer).
  if (aData->mLayer->GetValidRegion().IsEmpty()) {
    return true;
  }

  // There is an existing painted layer we can reuse. Throwing it away can make
  // compositing cheaper (see bug 946952), but it might cause us to re-allocate
  // the painted layer frequently due to an animation. So we only discard it if
  // we're in tree compression mode, which is triggered at a low frequency.
  return aLayerBuilder->CheckInLayerTreeCompressionMode();
}

#ifdef DEBUG
static int32_t FindIndexOfLayerIn(nsTArray<NewLayerEntry>& aArray,
                                  Layer* aLayer)
{
  for (uint32_t i = 0; i < aArray.Length(); ++i) {
    if (aArray[i].mLayer == aLayer) {
      return i;
    }
  }
  return -1;
}
#endif

already_AddRefed<Layer>
ContainerState::PrepareImageLayer(PaintedLayerData* aData)
{
  nsRefPtr<ImageContainer> imageContainer =
    aData->GetContainerForImageLayer(mBuilder);
  if (!imageContainer) {
    return nullptr;
  }

  nsRefPtr<ImageLayer> imageLayer = CreateOrRecycleImageLayer(aData->mLayer);
  imageLayer->SetContainer(imageContainer);
  aData->mImage->ConfigureLayer(imageLayer, mParameters);
  imageLayer->SetPostScale(mParameters.mXScale,
                           mParameters.mYScale);

  if (aData->mItemClip.HasClip()) {
    ParentLayerIntRect clip =
      ViewAs<ParentLayerPixel>(ScaleToNearestPixels(aData->mItemClip.GetClipRect()));
    clip.MoveBy(ViewAs<ParentLayerPixel>(mParameters.mOffset));
    imageLayer->SetClipRect(Some(clip));
  } else {
    imageLayer->SetClipRect(Nothing());
  }

  mLayerBuilder->StoreOptimizedLayerForFrame(aData->mImage, imageLayer);
  FLB_LOG_PAINTED_LAYER_DECISION(aData,
                                 "  Selected image layer=%p\n", imageLayer.get());

  return imageLayer.forget();
}

already_AddRefed<Layer>
ContainerState::PrepareColorLayer(PaintedLayerData* aData)
{
  nsRefPtr<ColorLayer> colorLayer = CreateOrRecycleColorLayer(aData->mLayer);
  colorLayer->SetColor(aData->mSolidColor);

  // Copy transform
  colorLayer->SetBaseTransform(aData->mLayer->GetBaseTransform());
  colorLayer->SetPostScale(aData->mLayer->GetPostXScale(),
                           aData->mLayer->GetPostYScale());

  nsIntRect visibleRect = aData->mVisibleRegion.GetBounds();
  visibleRect.MoveBy(-GetTranslationForPaintedLayer(aData->mLayer));
  colorLayer->SetBounds(visibleRect);
  colorLayer->SetClipRect(Nothing());

  FLB_LOG_PAINTED_LAYER_DECISION(aData,
                                 "  Selected color layer=%p\n", colorLayer.get());

  return colorLayer.forget();
}

template<typename FindOpaqueBackgroundColorCallbackType>
void ContainerState::FinishPaintedLayerData(PaintedLayerData& aData, FindOpaqueBackgroundColorCallbackType aFindOpaqueBackgroundColor)
{
  PaintedLayerData* data = &aData;

  if (!data->mLayer) {
    // No layer was recycled, so we create a new one.
    nsRefPtr<PaintedLayer> paintedLayer = CreatePaintedLayer(data);
    data->mLayer = paintedLayer;

    NS_ASSERTION(FindIndexOfLayerIn(mNewChildLayers, paintedLayer) < 0,
                 "Layer already in list???");
    mNewChildLayers[data->mNewChildLayersIndex].mLayer = paintedLayer.forget();
  }

  for (auto& item : data->mAssignedDisplayItems) {
    MOZ_ASSERT(item.mItem->GetType() != nsDisplayItem::TYPE_LAYER_EVENT_REGIONS);

    InvalidateForLayerChange(item.mItem, data->mLayer);
    mLayerBuilder->AddPaintedDisplayItem(data, item.mItem, item.mClip,
                                         *this, item.mLayerState,
                                         data->mAnimatedGeometryRootOffset);
  }

  NewLayerEntry* newLayerEntry = &mNewChildLayers[data->mNewChildLayersIndex];

  nsRefPtr<Layer> layer;
  bool canOptimizeToImageLayer = data->CanOptimizeToImageLayer(mBuilder);

  FLB_LOG_PAINTED_LAYER_DECISION(data, "Selecting layer for pld=%p\n", data);
  FLB_LOG_PAINTED_LAYER_DECISION(data, "  Solid=%i, hasImage=%c, canOptimizeAwayPaintedLayer=%i\n",
          data->mIsSolidColorInVisibleRegion, canOptimizeToImageLayer ? 'y' : 'n',
          CanOptimizeAwayPaintedLayer(data, mLayerBuilder));

  if ((data->mIsSolidColorInVisibleRegion || canOptimizeToImageLayer) &&
      CanOptimizeAwayPaintedLayer(data, mLayerBuilder)) {
    NS_ASSERTION(!(data->mIsSolidColorInVisibleRegion && canOptimizeToImageLayer),
                 "Can't be a solid color as well as an image!");

    layer = canOptimizeToImageLayer ? PrepareImageLayer(data)
                                    : PrepareColorLayer(data);

    if (layer) {
      NS_ASSERTION(FindIndexOfLayerIn(mNewChildLayers, layer) < 0,
                   "Layer already in list???");
      NS_ASSERTION(newLayerEntry->mLayer == data->mLayer,
                   "Painted layer at wrong index");
      // Store optimized layer in reserved slot
      newLayerEntry = &mNewChildLayers[data->mNewChildLayersIndex + 1];
      NS_ASSERTION(!newLayerEntry->mLayer, "Slot already occupied?");
      newLayerEntry->mLayer = layer;
      newLayerEntry->mAnimatedGeometryRoot = data->mAnimatedGeometryRoot;
      newLayerEntry->mFixedPosFrameForLayerData = data->mFixedPosFrameForLayerData;

      // Hide the PaintedLayer. We leave it in the layer tree so that we
      // can find and recycle it later.
      ParentLayerIntRect emptyRect;
      data->mLayer->SetClipRect(Some(emptyRect));
      data->mLayer->SetVisibleRegion(nsIntRegion());
      data->mLayer->InvalidateRegion(data->mLayer->GetValidRegion().GetBounds());
      data->mLayer->SetEventRegions(EventRegions());
    }
  }
  
  if (!layer) {
    // We couldn't optimize to an image layer or a color layer above.
    layer = data->mLayer;
    layer->SetClipRect(Nothing());
    FLB_LOG_PAINTED_LAYER_DECISION(data, "  Selected painted layer=%p\n", layer.get());
  }

  if (mLayerBuilder->IsBuildingRetainedLayers()) {
    newLayerEntry->mVisibleRegion = data->mVisibleRegion;
    newLayerEntry->mOpaqueRegion = data->mOpaqueRegion;
    newLayerEntry->mHideAllLayersBelow = data->mHideAllLayersBelow;
    newLayerEntry->mOpaqueForAnimatedGeometryRootParent = data->mOpaqueForAnimatedGeometryRootParent;
  } else {
    SetOuterVisibleRegionForLayer(layer, data->mVisibleRegion);
  }

  nsIntRect layerBounds = data->mBounds;
  layerBounds.MoveBy(-GetTranslationForPaintedLayer(data->mLayer));
  layer->SetLayerBounds(layerBounds);

#ifdef MOZ_DUMP_PAINTING
  if (!data->mLog.IsEmpty()) {
    if (PaintedLayerData* containingPld = mLayerBuilder->GetContainingPaintedLayerData()) {
      containingPld->mLayer->AddExtraDumpInfo(nsCString(data->mLog));
    } else {
      layer->AddExtraDumpInfo(nsCString(data->mLog));
    }
  }
#endif

  nsIntRegion transparentRegion;
  transparentRegion.Sub(data->mVisibleRegion, data->mOpaqueRegion);
  bool isOpaque = transparentRegion.IsEmpty();
  // For translucent PaintedLayers, try to find an opaque background
  // color that covers the entire area beneath it so we can pull that
  // color into this layer to make it opaque.
  if (layer == data->mLayer) {
    nscolor backgroundColor = NS_RGBA(0,0,0,0);
    if (!isOpaque) {
      backgroundColor = aFindOpaqueBackgroundColor();
      if (NS_GET_A(backgroundColor) == 255) {
        isOpaque = true;
      }
    }

    // Store the background color
    PaintedDisplayItemLayerUserData* userData =
      GetPaintedDisplayItemLayerUserData(data->mLayer);
    NS_ASSERTION(userData, "where did our user data go?");
    if (userData->mForcedBackgroundColor != backgroundColor) {
      // Invalidate the entire target PaintedLayer since we're changing
      // the background color
#ifdef MOZ_DUMP_PAINTING
      if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
        printf_stderr("Forced background color has changed from #%08X to #%08X on layer %p\n",
                      userData->mForcedBackgroundColor, backgroundColor, data->mLayer);
        nsAutoCString str;
        AppendToString(str, data->mLayer->GetValidRegion());
        printf_stderr("Invalidating layer %p: %s\n", data->mLayer, str.get());
      }
#endif
      data->mLayer->InvalidateRegion(data->mLayer->GetValidRegion());
    }
    userData->mForcedBackgroundColor = backgroundColor;

    userData->mFontSmoothingBackgroundColor = data->mFontSmoothingBackgroundColor;

    // use a mask layer for rounded rect clipping.
    // data->mCommonClipCount may be -1 if we haven't put any actual
    // drawable items in this layer (i.e. it's only catching events).
    int32_t commonClipCount = std::max(0, data->mCommonClipCount);
    SetupMaskLayer(layer, data->mItemClip, data->mVisibleRegion, commonClipCount);
    // copy commonClipCount to the entry
    FrameLayerBuilder::PaintedLayerItemsEntry* entry = mLayerBuilder->
      GetPaintedLayerItemsEntry(static_cast<PaintedLayer*>(layer.get()));
    entry->mCommonClipCount = commonClipCount;
  } else {
    // mask layer for image and color layers
    SetupMaskLayer(layer, data->mItemClip, data->mVisibleRegion);
  }

  uint32_t flags = 0;
  nsIWidget* widget = mContainerReferenceFrame->PresContext()->GetRootWidget();
  // See bug 941095. Not quite ready to disable this.
  bool hidpi = false && widget && widget->GetDefaultScale().scale >= 2;
  if (hidpi) {
    flags |= Layer::CONTENT_DISABLE_SUBPIXEL_AA;
  }
  if (isOpaque && !data->mForceTransparentSurface) {
    flags |= Layer::CONTENT_OPAQUE;
  } else if (data->mNeedComponentAlpha && !hidpi) {
    flags |= Layer::CONTENT_COMPONENT_ALPHA;
  }
  if (data->mDisableFlattening) {
    flags |= Layer::CONTENT_DISABLE_FLATTENING;
  }
  layer->SetContentFlags(flags);

  SetFixedPositionLayerData(layer, data->mFixedPosFrameForLayerData);

  PaintedLayerData* containingPaintedLayerData =
     mLayerBuilder->GetContainingPaintedLayerData();
  if (containingPaintedLayerData) {
    if (!data->mDispatchToContentHitRegion.GetBounds().IsEmpty()) {
      nsRect rect = nsLayoutUtils::TransformFrameRectToAncestor(
        mContainerReferenceFrame,
        data->mDispatchToContentHitRegion.GetBounds(),
        containingPaintedLayerData->mReferenceFrame);
      containingPaintedLayerData->mDispatchToContentHitRegion.Or(
        containingPaintedLayerData->mDispatchToContentHitRegion, rect);
    }
    if (!data->mMaybeHitRegion.GetBounds().IsEmpty()) {
      nsRect rect = nsLayoutUtils::TransformFrameRectToAncestor(
        mContainerReferenceFrame,
        data->mMaybeHitRegion.GetBounds(),
        containingPaintedLayerData->mReferenceFrame);
      containingPaintedLayerData->mMaybeHitRegion.Or(
        containingPaintedLayerData->mMaybeHitRegion, rect);
    }
    nsLayoutUtils::TransformToAncestorAndCombineRegions(
      data->mHitRegion.GetBounds(),
      mContainerReferenceFrame,
      containingPaintedLayerData->mReferenceFrame,
      &containingPaintedLayerData->mHitRegion,
      &containingPaintedLayerData->mMaybeHitRegion);
    nsLayoutUtils::TransformToAncestorAndCombineRegions(
      data->mNoActionRegion.GetBounds(),
      mContainerReferenceFrame,
      containingPaintedLayerData->mReferenceFrame,
      &containingPaintedLayerData->mNoActionRegion,
      &containingPaintedLayerData->mDispatchToContentHitRegion);
    nsLayoutUtils::TransformToAncestorAndCombineRegions(
      data->mHorizontalPanRegion.GetBounds(),
      mContainerReferenceFrame,
      containingPaintedLayerData->mReferenceFrame,
      &containingPaintedLayerData->mHorizontalPanRegion,
      &containingPaintedLayerData->mDispatchToContentHitRegion);
    nsLayoutUtils::TransformToAncestorAndCombineRegions(
      data->mVerticalPanRegion.GetBounds(),
      mContainerReferenceFrame,
      containingPaintedLayerData->mReferenceFrame,
      &containingPaintedLayerData->mVerticalPanRegion,
      &containingPaintedLayerData->mDispatchToContentHitRegion);

  } else {
    EventRegions regions;
    regions.mHitRegion = ScaleRegionToOutsidePixels(data->mHitRegion);
    regions.mNoActionRegion = ScaleRegionToOutsidePixels(data->mNoActionRegion);
    regions.mHorizontalPanRegion = ScaleRegionToOutsidePixels(data->mHorizontalPanRegion);
    regions.mVerticalPanRegion = ScaleRegionToOutsidePixels(data->mVerticalPanRegion);
    // Points whose hit-region status we're not sure about need to be dispatched
    // to the content thread. If a point is in both maybeHitRegion and hitRegion
    // then it's not a "maybe" any more, and doesn't go into the dispatch-to-
    // content region.
    nsIntRegion maybeHitRegion = ScaleRegionToOutsidePixels(data->mMaybeHitRegion);
    regions.mDispatchToContentHitRegion.Sub(maybeHitRegion, regions.mHitRegion);
    regions.mDispatchToContentHitRegion.OrWith(
        ScaleRegionToOutsidePixels(data->mDispatchToContentHitRegion));
    regions.mHitRegion.OrWith(maybeHitRegion);

    Matrix mat = layer->GetTransform().As2D();
    mat.Invert();
    regions.ApplyTranslationAndScale(mat._31, mat._32, mat._11, mat._22);

    layer->SetEventRegions(regions);
  }
}

static bool
IsItemAreaInWindowOpaqueRegion(nsDisplayListBuilder* aBuilder,
                               nsDisplayItem* aItem,
                               const nsRect& aComponentAlphaBounds)
{
  if (!aItem->Frame()->PresContext()->IsChrome()) {
    // Assume that Web content is always in the window opaque region.
    return true;
  }
  if (aItem->ReferenceFrame() != aBuilder->RootReferenceFrame()) {
    // aItem is probably in some transformed subtree.
    // We're not going to bother figuring out where this landed, we're just
    // going to assume it might have landed over a transparent part of
    // the window.
    return false;
  }
  return aBuilder->GetWindowOpaqueRegion().Contains(aComponentAlphaBounds);
}

void
PaintedLayerData::Accumulate(ContainerState* aState,
                            nsDisplayItem* aItem,
                            const nsIntRegion& aClippedOpaqueRegion,
                            const nsIntRect& aVisibleRect,
                            const DisplayItemClip& aClip,
                            LayerState aLayerState)
{
  FLB_LOG_PAINTED_LAYER_DECISION(this, "Accumulating dp=%s(%p), f=%p against pld=%p\n", aItem->Name(), aItem, aItem->Frame(), this);

  bool snap;
  nsRect itemBounds = aItem->GetBounds(aState->mBuilder, &snap);
  mBounds = mBounds.Union(aState->ScaleToOutsidePixels(itemBounds, snap));

  if (aState->mBuilder->NeedToForceTransparentSurfaceForItem(aItem)) {
    mForceTransparentSurface = true;
  }
  if (aState->mParameters.mDisableSubpixelAntialiasingInDescendants) {
    // Disable component alpha.
    // Note that the transform (if any) on the PaintedLayer is always an integer translation so
    // we don't have to factor that in here.
    aItem->DisableComponentAlpha();
  }

  bool clipMatches = mItemClip == aClip;
  mItemClip = aClip;

  mAssignedDisplayItems.AppendElement(AssignedDisplayItem(aItem, aClip, aLayerState));

  if (!mIsSolidColorInVisibleRegion && mOpaqueRegion.Contains(aVisibleRect) &&
      mVisibleRegion.Contains(aVisibleRect) && !mImage) {
    // A very common case! Most pages have a PaintedLayer with the page
    // background (opaque) visible and most or all of the page content over the
    // top of that background.
    // The rest of this method won't do anything. mVisibleRegion and mOpaqueRegion
    // don't need updating. mVisibleRegion contains aVisibleRect already,
    // mOpaqueRegion contains aVisibleRect and therefore whatever the opaque
    // region of the item is. mVisibleRegion must contain mOpaqueRegion
    // and therefore aVisibleRect.
    return;
  }

  /* Mark as available for conversion to image layer if this is a nsDisplayImage and
   * it's the only thing visible in this layer.
   */
  if (nsIntRegion(aVisibleRect).Contains(mVisibleRegion) &&
      aClippedOpaqueRegion.Contains(mVisibleRegion) &&
      aItem->SupportsOptimizingToImage()) {
    mImage = static_cast<nsDisplayImageContainer*>(aItem);
    FLB_LOG_PAINTED_LAYER_DECISION(this, "  Tracking image: nsDisplayImageContainer covers the layer\n");
  } else if (mImage) {
    FLB_LOG_PAINTED_LAYER_DECISION(this, "  No longer tracking image\n");
    mImage = nullptr;
  }

  bool isFirstVisibleItem = mVisibleRegion.IsEmpty();
  if (isFirstVisibleItem) {
    nscolor fontSmoothingBGColor;
    if (aItem->ProvidesFontSmoothingBackgroundColor(aState->mBuilder,
                                                    &fontSmoothingBGColor)) {
      mFontSmoothingBackgroundColor = fontSmoothingBGColor;
    }
  }

  nscolor uniformColor;
  bool isUniform = aItem->IsUniform(aState->mBuilder, &uniformColor);

  // Some display items have to exist (so they can set forceTransparentSurface
  // below) but don't draw anything. They'll return true for isUniform but
  // a color with opacity 0.
  if (!isUniform || NS_GET_A(uniformColor) > 0) {
    // Make sure that the visible area is covered by uniform pixels. In
    // particular this excludes cases where the edges of the item are not
    // pixel-aligned (thus the item will not be truly uniform).
    if (isUniform) {
      bool snap;
      nsRect bounds = aItem->GetBounds(aState->mBuilder, &snap);
      if (!aState->ScaleToInsidePixels(bounds, snap).Contains(aVisibleRect)) {
        isUniform = false;
        FLB_LOG_PAINTED_LAYER_DECISION(this, "  Display item does not cover the visible rect\n");
      }
    }
    if (isUniform) {
      if (isFirstVisibleItem) {
        // This color is all we have
        mSolidColor = uniformColor;
        mIsSolidColorInVisibleRegion = true;
      } else if (mIsSolidColorInVisibleRegion &&
                 mVisibleRegion.IsEqual(nsIntRegion(aVisibleRect)) &&
                 clipMatches) {
        // we can just blend the colors together
        mSolidColor = NS_ComposeColors(mSolidColor, uniformColor);
      } else {
        FLB_LOG_PAINTED_LAYER_DECISION(this, "  Layer not a solid color: Can't blend colors togethers\n");
        mIsSolidColorInVisibleRegion = false;
      }
    } else {
      FLB_LOG_PAINTED_LAYER_DECISION(this, "  Layer is not a solid color: Display item is not uniform over the visible bound\n");
      mIsSolidColorInVisibleRegion = false;
    }

    mVisibleRegion.Or(mVisibleRegion, aVisibleRect);
    mVisibleRegion.SimplifyOutward(4);
  }

  if (!aClippedOpaqueRegion.IsEmpty()) {
    nsIntRegionRectIterator iter(aClippedOpaqueRegion);
    for (const nsIntRect* r = iter.Next(); r; r = iter.Next()) {
      // We don't use SimplifyInward here since it's not defined exactly
      // what it will discard. For our purposes the most important case
      // is a large opaque background at the bottom of z-order (e.g.,
      // a canvas background), so we need to make sure that the first rect
      // we see doesn't get discarded.
      nsIntRegion tmp;
      tmp.Or(mOpaqueRegion, *r);
       // Opaque display items in chrome documents whose window is partially
       // transparent are always added to the opaque region. This helps ensure
       // that we get as much subpixel-AA as possible in the chrome.
       if (tmp.GetNumRects() <= 4 || aItem->Frame()->PresContext()->IsChrome()) {
        mOpaqueRegion = tmp;
      }
    }
  }

  if (!aState->mParameters.mDisableSubpixelAntialiasingInDescendants) {
    nsRect componentAlpha = aItem->GetComponentAlphaBounds(aState->mBuilder);
    if (!componentAlpha.IsEmpty()) {
      nsIntRect componentAlphaRect =
        aState->ScaleToOutsidePixels(componentAlpha, false).Intersect(aVisibleRect);
      if (!mOpaqueRegion.Contains(componentAlphaRect)) {
        if (IsItemAreaInWindowOpaqueRegion(aState->mBuilder, aItem,
              componentAlpha.Intersect(aItem->GetVisibleRect()))) {
          mNeedComponentAlpha = true;
        } else {
          aItem->DisableComponentAlpha();
        }
      }
    }
  }

  // Ensure animated text does not get flattened, even if it forces other
  // content in the container to be layerized. The content backend might
  // not support subpixel positioning of text that animated transforms can
  // generate. bug 633097
  if (aState->mParameters.mInActiveTransformedSubtree &&
       (mNeedComponentAlpha ||
         !aItem->GetComponentAlphaBounds(aState->mBuilder).IsEmpty())) {
    mDisableFlattening = true;
  }
}

PaintedLayerData
ContainerState::NewPaintedLayerData(nsDisplayItem* aItem,
                                    const nsIntRect& aVisibleRect,
                                    const nsIFrame* aAnimatedGeometryRoot,
                                    const nsPoint& aTopLeft,
                                    bool aShouldFixToViewport)
{
  PaintedLayerData data;
  data.mAnimatedGeometryRoot = aAnimatedGeometryRoot;
  data.mAnimatedGeometryRootOffset = aTopLeft;
  data.mFixedPosFrameForLayerData =
    FindFixedPosFrameForLayerData(aAnimatedGeometryRoot, aShouldFixToViewport);
  data.mReferenceFrame = aItem->ReferenceFrame();
  data.mSingleItemFixedToViewport = aShouldFixToViewport;

  data.mNewChildLayersIndex = mNewChildLayers.Length();
  NewLayerEntry* newLayerEntry = mNewChildLayers.AppendElement();
  newLayerEntry->mAnimatedGeometryRoot = aAnimatedGeometryRoot;
  newLayerEntry->mFixedPosFrameForLayerData = data.mFixedPosFrameForLayerData;
  // newLayerEntry->mOpaqueRegion is filled in later from
  // paintedLayerData->mOpaqueRegion, if necessary.

  // Allocate another entry for this layer's optimization to ColorLayer/ImageLayer
  mNewChildLayers.AppendElement();

  return data;
}

#ifdef MOZ_DUMP_PAINTING
static void
DumpPaintedImage(nsDisplayItem* aItem, SourceSurface* aSurface)
{
  nsCString string(aItem->Name());
  string.Append('-');
  string.AppendInt((uint64_t)aItem);
  fprintf_stderr(gfxUtils::sDumpPaintFile, "array[\"%s\"]=\"", string.BeginReading());
  gfxUtils::DumpAsDataURI(aSurface, gfxUtils::sDumpPaintFile);
  fprintf_stderr(gfxUtils::sDumpPaintFile, "\";");
}
#endif

static void
PaintInactiveLayer(nsDisplayListBuilder* aBuilder,
                   LayerManager* aManager,
                   nsDisplayItem* aItem,
                   gfxContext* aContext,
                   nsRenderingContext* aCtx)
{
  // This item has an inactive layer. Render it to a PaintedLayer
  // using a temporary BasicLayerManager.
  BasicLayerManager* basic = static_cast<BasicLayerManager*>(aManager);
  nsRefPtr<gfxContext> context = aContext;
#ifdef MOZ_DUMP_PAINTING
  int32_t appUnitsPerDevPixel = AppUnitsPerDevPixel(aItem);
  nsIntRect itemVisibleRect =
    aItem->GetVisibleRect().ToOutsidePixels(appUnitsPerDevPixel);

  RefPtr<DrawTarget> tempDT;
  if (gfxUtils::sDumpPainting) {
    tempDT = gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
                                      itemVisibleRect.Size(),
                                      SurfaceFormat::B8G8R8A8);
    if (tempDT) {
      context = new gfxContext(tempDT);
      context->SetMatrix(gfxMatrix::Translation(-itemVisibleRect.x,
                                                -itemVisibleRect.y));
    }
  }
#endif
  basic->BeginTransaction();
  basic->SetTarget(context);

  if (aItem->GetType() == nsDisplayItem::TYPE_SVG_EFFECTS) {
    static_cast<nsDisplaySVGEffects*>(aItem)->PaintAsLayer(aBuilder, aCtx, basic);
    if (basic->InTransaction()) {
      basic->AbortTransaction();
    }
  } else {
    basic->EndTransaction(FrameLayerBuilder::DrawPaintedLayer, aBuilder);
  }
  FrameLayerBuilder *builder = static_cast<FrameLayerBuilder*>(basic->GetUserData(&gLayerManagerLayerBuilder));
  if (builder) {
    builder->DidEndTransaction();
  }

  basic->SetTarget(nullptr);

#ifdef MOZ_DUMP_PAINTING
  if (gfxUtils::sDumpPainting && tempDT) {
    RefPtr<SourceSurface> surface = tempDT->Snapshot();
    DumpPaintedImage(aItem, surface);

    DrawTarget* drawTarget = aContext->GetDrawTarget();
    Rect rect(itemVisibleRect.x, itemVisibleRect.y,
              itemVisibleRect.width, itemVisibleRect.height);
    drawTarget->DrawSurface(surface, rect, Rect(Point(0,0), rect.Size()));

    aItem->SetPainted();
  }
#endif
}

/**
 * Chooses a single active scrolled root for the entire display list, used
 * when we are flattening layers.
 */
bool
ContainerState::ChooseAnimatedGeometryRoot(const nsDisplayList& aList,
                                           const nsIFrame **aAnimatedGeometryRoot)
{
  for (nsDisplayItem* item = aList.GetBottom(); item; item = item->GetAbove()) {
    LayerState layerState = item->GetLayerState(mBuilder, mManager, mParameters);
    // Don't use an item that won't be part of any PaintedLayers to pick the
    // active scrolled root.
    if (layerState == LAYER_ACTIVE_FORCE) {
      continue;
    }

    // Try using the actual active scrolled root of the backmost item, as that
    // should result in the least invalidation when scrolling.
    *aAnimatedGeometryRoot =
      nsLayoutUtils::GetAnimatedGeometryRootFor(item, mBuilder, mManager);
    return true;
  }
  return false;
}

nsIntRegion
ContainerState::ComputeOpaqueRect(nsDisplayItem* aItem,
                                  const nsIFrame* aAnimatedGeometryRoot,
                                  const nsIFrame* aFixedPosFrame,
                                  const DisplayItemClip& aClip,
                                  nsDisplayList* aList,
                                  bool* aHideAllLayersBelow,
                                  bool* aOpaqueForAnimatedGeometryRootParent)
{
  bool snapOpaque;
  nsRegion opaque = aItem->GetOpaqueRegion(mBuilder, &snapOpaque);
  nsIntRegion opaquePixels;
  if (!opaque.IsEmpty()) {
    nsRegion opaqueClipped;
    nsRegionRectIterator iter(opaque);
    for (const nsRect* r = iter.Next(); r; r = iter.Next()) {
      opaqueClipped.Or(opaqueClipped, aClip.ApproximateIntersectInward(*r));
    }
    if (aAnimatedGeometryRoot == mContainerAnimatedGeometryRoot &&
        aFixedPosFrame == mContainerFixedPosFrame &&
        opaqueClipped.Contains(mContainerBounds)) {
      *aHideAllLayersBelow = true;
      aList->SetIsOpaque();
    }
    // Add opaque areas to the "exclude glass" region. Only do this when our
    // container layer is going to be the rootmost layer, otherwise transforms
    // etc will mess us up (and opaque contributions from other containers are
    // not needed).
    if (!nsLayoutUtils::GetCrossDocParentFrame(mContainerFrame)) {
      mBuilder->AddWindowOpaqueRegion(opaqueClipped);
    }
    opaquePixels = ScaleRegionToInsidePixels(opaqueClipped, snapOpaque);

    nsIScrollableFrame* sf = nsLayoutUtils::GetScrollableFrameFor(aAnimatedGeometryRoot);
    if (sf) {
      nsRect displayport;
      bool usingDisplayport =
        nsLayoutUtils::GetDisplayPort(aAnimatedGeometryRoot->GetContent(), &displayport);
      if (!usingDisplayport) {
        // No async scrolling, so all that matters is that the layer contents
        // cover the scrollport.
        displayport = sf->GetScrollPortRect();
      }
      nsIFrame* scrollFrame = do_QueryFrame(sf);
      displayport += scrollFrame->GetOffsetToCrossDoc(mContainerReferenceFrame);
      if (opaque.Contains(displayport)) {
        *aOpaqueForAnimatedGeometryRootParent = true;
      }
    }
  }
  return opaquePixels;
}

/*
 * Iterate through the non-clip items in aList and its descendants.
 * For each item we compute the effective clip rect. Each item is assigned
 * to a layer. We invalidate the areas in PaintedLayers where an item
 * has moved from one PaintedLayer to another. Also,
 * aState->mInvalidPaintedContent is invalidated in every PaintedLayer.
 * We set the clip rect for items that generated their own layer, and
 * create a mask layer to do any rounded rect clipping.
 * (PaintedLayers don't need a clip rect on the layer, we clip the items
 * individually when we draw them.)
 * We set the visible rect for all layers, although the actual setting
 * of visible rects for some PaintedLayers is deferred until the calling
 * of ContainerState::Finish.
 */
void
ContainerState::ProcessDisplayItems(nsDisplayList* aList)
{
  PROFILER_LABEL("ContainerState", "ProcessDisplayItems",
    js::ProfileEntry::Category::GRAPHICS);

  const nsIFrame* lastAnimatedGeometryRoot = mContainerReferenceFrame;
  nsPoint topLeft(0,0);

  // When NO_COMPONENT_ALPHA is set, items will be flattened into a single
  // layer, so we need to choose which active scrolled root to use for all
  // items.
  if (mFlattenToSingleLayer) {
    if (ChooseAnimatedGeometryRoot(*aList, &lastAnimatedGeometryRoot)) {
      topLeft = lastAnimatedGeometryRoot->GetOffsetToCrossDoc(mContainerReferenceFrame);
    }
  }

  int32_t maxLayers = nsDisplayItem::MaxActiveLayers();
  int layerCount = 0;

  nsDisplayList savedItems;
  nsDisplayItem* item;
  while ((item = aList->RemoveBottom()) != nullptr) {
    // Peek ahead to the next item and try merging with it or swapping with it
    // if necessary.
    nsDisplayItem* aboveItem;
    while ((aboveItem = aList->GetBottom()) != nullptr) {
      if (aboveItem->TryMerge(mBuilder, item)) {
        aList->RemoveBottom();
        item->~nsDisplayItem();
        item = aboveItem;
      } else {
        break;
      }
    }

    nsDisplayList* itemSameCoordinateSystemChildren
      = item->GetSameCoordinateSystemChildren();
    if (item->ShouldFlattenAway(mBuilder)) {
      aList->AppendToBottom(itemSameCoordinateSystemChildren);
      item->~nsDisplayItem();
      continue;
    }

    savedItems.AppendToTop(item);

    NS_ASSERTION(mAppUnitsPerDevPixel == AppUnitsPerDevPixel(item),
      "items in a container layer should all have the same app units per dev pixel");

    if (mBuilder->NeedToForceTransparentSurfaceForItem(item)) {
      aList->SetNeedsTransparentSurface();
    }

    bool snap;
    nsRect itemContent = item->GetBounds(mBuilder, &snap);
    nsDisplayItem::Type itemType = item->GetType();
    if (itemType == nsDisplayItem::TYPE_LAYER_EVENT_REGIONS) {
      nsDisplayLayerEventRegions* eventRegions =
        static_cast<nsDisplayLayerEventRegions*>(item);
      itemContent = eventRegions->GetHitRegionBounds(mBuilder, &snap);
    }
    nsIntRect itemDrawRect = ScaleToOutsidePixels(itemContent, snap);
    bool prerenderedTransform = itemType == nsDisplayItem::TYPE_TRANSFORM &&
        static_cast<nsDisplayTransform*>(item)->ShouldPrerender(mBuilder);
    ParentLayerIntRect clipRect;
    const DisplayItemClip& itemClip = item->GetClip();
    if (itemClip.HasClip()) {
      itemContent.IntersectRect(itemContent, itemClip.GetClipRect());
      clipRect = ViewAs<ParentLayerPixel>(ScaleToNearestPixels(itemClip.GetClipRect()));
      if (!prerenderedTransform) {
        itemDrawRect.IntersectRect(itemDrawRect, ParentLayerIntRect::ToUntyped(clipRect));
      }
      clipRect.MoveBy(ViewAs<ParentLayerPixel>(mParameters.mOffset));
    }
#ifdef DEBUG
    nsRect bounds = itemContent;
    bool dummy;
    if (itemType == nsDisplayItem::TYPE_LAYER_EVENT_REGIONS) {
      bounds = item->GetBounds(mBuilder, &dummy);
      if (itemClip.HasClip()) {
        bounds.IntersectRect(bounds, itemClip.GetClipRect());
      }
    }
    ((nsRect&)mAccumulatedChildBounds).UnionRect(mAccumulatedChildBounds, bounds);
#endif
    // We haven't computed visibility at this point, so item->GetVisibleRect()
    // is just the dirty rect that item was initialized with. We intersect it
    // with the clipped item bounds to get a tighter visible rect.
    nsIntRect itemVisibleRect = itemDrawRect.Intersect(
      ScaleToOutsidePixels(item->GetVisibleRect(), false));

    LayerState layerState = item->GetLayerState(mBuilder, mManager, mParameters);
    if (layerState == LAYER_INACTIVE &&
        nsDisplayItem::ForceActiveLayers()) {
      layerState = LAYER_ACTIVE;
    }

    bool forceInactive;
    const nsIFrame* animatedGeometryRoot;
    if (mFlattenToSingleLayer) {
      forceInactive = true;
      animatedGeometryRoot = lastAnimatedGeometryRoot;
    } else {
      forceInactive = false;
      if (mManager->IsWidgetLayerManager()) {
        animatedGeometryRoot = nsLayoutUtils::GetAnimatedGeometryRootFor(item, mBuilder, mManager);
      } else {
        // For inactive layer subtrees, splitting content into PaintedLayers
        // based on animated geometry roots is pointless. It's more efficient
        // to build the minimum number of layers.
        animatedGeometryRoot = mContainerAnimatedGeometryRoot;
      }
      if (animatedGeometryRoot != lastAnimatedGeometryRoot) {
        lastAnimatedGeometryRoot = animatedGeometryRoot;
        topLeft = animatedGeometryRoot->GetOffsetToCrossDoc(mContainerReferenceFrame);
      }
    }
    bool shouldFixToViewport = !animatedGeometryRoot->GetParent() &&
      item->ShouldFixToViewport(mManager);

    if (maxLayers != -1 && layerCount >= maxLayers) {
      forceInactive = true;
    }

    // Assign the item to a layer
    if (layerState == LAYER_ACTIVE_FORCE ||
        (layerState == LAYER_INACTIVE && !mManager->IsWidgetLayerManager()) ||
        (!forceInactive &&
         (layerState == LAYER_ACTIVE_EMPTY ||
          layerState == LAYER_ACTIVE))) {

      layerCount++;

      // LAYER_ACTIVE_EMPTY means the layer is created just for its metadata.
      // We should never see an empty layer with any visible content!
      NS_ASSERTION(layerState != LAYER_ACTIVE_EMPTY ||
                   itemVisibleRect.IsEmpty(),
                   "State is LAYER_ACTIVE_EMPTY but visible rect is not.");

      // As long as the new layer isn't going to be a PaintedLayer,
      // InvalidateForLayerChange doesn't need the new layer pointer.
      // We also need to check the old data now, because BuildLayer
      // can overwrite it.
      InvalidateForLayerChange(item, nullptr);

      // If the item would have its own layer but is invisible, just hide it.
      // Note that items without their own layers can't be skipped this
      // way, since their PaintedLayer may decide it wants to draw them
      // into its buffer even if they're currently covered.
      if (itemVisibleRect.IsEmpty() &&
          !item->ShouldBuildLayerEvenIfInvisible(mBuilder)) {
        continue;
      }

      // 3D-transformed layers don't necessarily draw in the order in which
      // they're added to their parent container layer.
      bool mayDrawOutOfOrder = itemType == nsDisplayItem::TYPE_TRANSFORM &&
        (item->Frame()->Preserves3D() || item->Frame()->Preserves3DChildren());

      // Let mPaintedLayerDataTree know about this item, so that
      // FindPaintedLayerFor and FindOpaqueBackgroundColor are aware of this
      // item, even though it's not in any PaintedLayerDataStack.
      // Ideally we'd only need the "else" case here and have
      // mPaintedLayerDataTree figure out the right clip from the animated
      // geometry root that we give it, but it can't easily figure about
      // overflow:hidden clips on ancestors just by looking at the frame.
      // So we'll do a little hand holding and pass the clip instead of the
      // visible rect for the two important cases.
      nscolor uniformColor = NS_RGBA(0,0,0,0);
      nscolor* uniformColorPtr = !mayDrawOutOfOrder ? &uniformColor : nullptr;
      nsIntRect clipRectUntyped;
      nsIntRect* clipPtr = itemClip.HasClip() ? &clipRectUntyped : nullptr;
      if (clipPtr) {
	      clipRectUntyped = ParentLayerIntRect::ToUntyped(clipRect);
      }
      if (animatedGeometryRoot == item->Frame() &&
          animatedGeometryRoot != mBuilder->RootReferenceFrame()) {
        // This is the case for scrollbar thumbs, for example. In that case the
        // clip we care about is the overflow:hidden clip on the scrollbar.
        const nsIFrame* clipAnimatedGeometryRoot =
          mPaintedLayerDataTree.GetParentAnimatedGeometryRoot(animatedGeometryRoot);
        mPaintedLayerDataTree.AddingOwnLayer(clipAnimatedGeometryRoot,
					     clipPtr,
					     uniformColorPtr);
      } else if (prerenderedTransform) {
        mPaintedLayerDataTree.AddingOwnLayer(animatedGeometryRoot,
					     clipPtr,
					     uniformColorPtr);
      } else {
        // Using itemVisibleRect here isn't perfect. itemVisibleRect can be
        // larger or smaller than the potential bounds of item's contents in
        // animatedGeometryRoot: It's too large if there's a clipped display
        // port somewhere among item's contents (see bug 1147673), and it can
        // be too small if the contents can move, because it only looks at the
        // contents' current bounds and doesn't anticipate any animations.
        // Time will tell whether this is good enough, or whether we need to do
        // something more sophisticated here.
        mPaintedLayerDataTree.AddingOwnLayer(animatedGeometryRoot,
                                             &itemVisibleRect, uniformColorPtr);
      }

      mParameters.mBackgroundColor = uniformColor;

      // Just use its layer.
      // Set layerContentsVisibleRect.width/height to -1 to indicate we
      // currently don't know. If BuildContainerLayerFor gets called by
      // item->BuildLayer, this will be set to a proper rect.
      nsIntRect layerContentsVisibleRect(0, 0, -1, -1);
      mParameters.mLayerContentsVisibleRect = &layerContentsVisibleRect;
      nsRefPtr<Layer> ownLayer = item->BuildLayer(mBuilder, mManager, mParameters);
      if (!ownLayer) {
        continue;
      }

      NS_ASSERTION(!ownLayer->AsPaintedLayer(),
                   "Should never have created a dedicated Painted layer!");

      const nsIFrame* fixedPosFrame =
        FindFixedPosFrameForLayerData(animatedGeometryRoot, shouldFixToViewport);
      SetFixedPositionLayerData(ownLayer, fixedPosFrame);

      nsRect invalid;
      if (item->IsInvalid(invalid)) {
        ownLayer->SetInvalidRectToVisibleRegion();
      }

      // If it's not a ContainerLayer, we need to apply the scale transform
      // ourselves.
      if (!ownLayer->AsContainerLayer()) {
        ownLayer->SetPostScale(mParameters.mXScale,
                               mParameters.mYScale);
      }

      // Update that layer's clip and visible rects.
      NS_ASSERTION(ownLayer->Manager() == mManager, "Wrong manager");
      NS_ASSERTION(!ownLayer->HasUserData(&gLayerManagerUserData),
                   "We shouldn't have a FrameLayerBuilder-managed layer here!");
      NS_ASSERTION(itemClip.HasClip() ||
                   itemClip.GetRoundedRectCount() == 0,
                   "If we have rounded rects, we must have a clip rect");
      // It has its own layer. Update that layer's clip and visible rects.
      if (itemClip.HasClip()) {
        ownLayer->SetClipRect(Some(clipRect));
      } else {
        ownLayer->SetClipRect(Nothing());
      }

      // rounded rectangle clipping using mask layers
      // (must be done after visible rect is set on layer)
      if (itemClip.IsRectClippedByRoundedCorner(itemContent)) {
        SetupMaskLayer(ownLayer, itemClip, itemVisibleRect);
      }

      ContainerLayer* oldContainer = ownLayer->GetParent();
      if (oldContainer && oldContainer != mContainerLayer) {
        oldContainer->RemoveChild(ownLayer);
      }
      NS_ASSERTION(FindIndexOfLayerIn(mNewChildLayers, ownLayer) < 0,
                   "Layer already in list???");

      NewLayerEntry* newLayerEntry = mNewChildLayers.AppendElement();
      newLayerEntry->mLayer = ownLayer;
      newLayerEntry->mAnimatedGeometryRoot = animatedGeometryRoot;
      newLayerEntry->mFixedPosFrameForLayerData = fixedPosFrame;

      // Don't attempt to flatten compnent alpha layers that are within
      // a forced active layer, or an active transform;
      if (itemType == nsDisplayItem::TYPE_TRANSFORM ||
          layerState == LAYER_ACTIVE_FORCE) {
        newLayerEntry->mPropagateComponentAlphaFlattening = false;
      }
      // nsDisplayTransform::BuildLayer must set layerContentsVisibleRect.
      // We rely on this to ensure 3D transforms compute a reasonable
      // layer visible region.
      NS_ASSERTION(itemType != nsDisplayItem::TYPE_TRANSFORM ||
                   layerContentsVisibleRect.width >= 0,
                   "Transform items must set layerContentsVisibleRect!");
      if (mLayerBuilder->IsBuildingRetainedLayers()) {
        newLayerEntry->mLayerContentsVisibleRect = layerContentsVisibleRect;
        newLayerEntry->mVisibleRegion = itemVisibleRect;
        newLayerEntry->mOpaqueRegion = ComputeOpaqueRect(item,
          animatedGeometryRoot, fixedPosFrame, itemClip, aList,
          &newLayerEntry->mHideAllLayersBelow,
          &newLayerEntry->mOpaqueForAnimatedGeometryRootParent);
      } else {
        SetOuterVisibleRegionForLayer(ownLayer, itemVisibleRect,
            layerContentsVisibleRect.width >= 0 ? &layerContentsVisibleRect : nullptr);
      }
      if (itemType == nsDisplayItem::TYPE_SCROLL_INFO_LAYER) {
        nsDisplayScrollInfoLayer* scrollItem = static_cast<nsDisplayScrollInfoLayer*>(item);
        newLayerEntry->mOpaqueForAnimatedGeometryRootParent = false;
        newLayerEntry->mBaseFrameMetrics =
            scrollItem->ComputeFrameMetrics(ownLayer, mParameters);
      } else if ((itemType == nsDisplayItem::TYPE_SUBDOCUMENT ||
                  itemType == nsDisplayItem::TYPE_ZOOM ||
                  itemType == nsDisplayItem::TYPE_RESOLUTION) &&
                 gfxPrefs::LayoutUseContainersForRootFrames())
      {
        newLayerEntry->mBaseFrameMetrics =
          static_cast<nsDisplaySubDocument*>(item)->ComputeFrameMetrics(ownLayer, mParameters);
      }

      /**
       * No need to allocate geometry for items that aren't
       * part of a PaintedLayer.
       */
      mLayerBuilder->AddLayerDisplayItem(ownLayer, item,
                                         layerState,
                                         topLeft, nullptr);
    } else {
      PaintedLayerData* paintedLayerData =
        mPaintedLayerDataTree.FindPaintedLayerFor(animatedGeometryRoot, itemVisibleRect,
                                                  shouldFixToViewport, [&]() {
          return NewPaintedLayerData(item, itemVisibleRect, animatedGeometryRoot,
                                     topLeft, shouldFixToViewport);
        });

      if (itemType == nsDisplayItem::TYPE_LAYER_EVENT_REGIONS) {
        nsDisplayLayerEventRegions* eventRegions =
            static_cast<nsDisplayLayerEventRegions*>(item);
        paintedLayerData->AccumulateEventRegions(eventRegions);
      } else {
        // check to see if the new item has rounded rect clips in common with
        // other items in the layer
        if (mManager->IsWidgetLayerManager()) {
          paintedLayerData->UpdateCommonClipCount(itemClip);
        }
        nsIntRegion opaquePixels = ComputeOpaqueRect(item,
            animatedGeometryRoot, paintedLayerData->mFixedPosFrameForLayerData,
            itemClip, aList,
            &paintedLayerData->mHideAllLayersBelow,
            &paintedLayerData->mOpaqueForAnimatedGeometryRootParent);
        MOZ_ASSERT(nsIntRegion(itemDrawRect).Contains(opaquePixels));
        opaquePixels.AndWith(itemVisibleRect);
        paintedLayerData->Accumulate(this, item, opaquePixels,
            itemVisibleRect, itemClip, layerState);

        if (!paintedLayerData->mLayer) {
          // Try to recycle the old layer of this display item.
          nsRefPtr<PaintedLayer> layer =
            AttemptToRecyclePaintedLayer(animatedGeometryRoot, item, topLeft);
          if (layer) {
            paintedLayerData->mLayer = layer;

            NS_ASSERTION(FindIndexOfLayerIn(mNewChildLayers, layer) < 0,
                         "Layer already in list???");
            mNewChildLayers[paintedLayerData->mNewChildLayersIndex].mLayer = layer.forget();
          }
        }
      }
    }

    if (itemSameCoordinateSystemChildren &&
        itemSameCoordinateSystemChildren->NeedsTransparentSurface()) {
      aList->SetNeedsTransparentSurface();
    }
  }

  aList->AppendToTop(&savedItems);
}

void
ContainerState::InvalidateForLayerChange(nsDisplayItem* aItem, PaintedLayer* aNewLayer)
{
  NS_ASSERTION(aItem->GetPerFrameKey(),
               "Display items that render using Thebes must have a key");
  nsDisplayItemGeometry* oldGeometry = nullptr;
  DisplayItemClip* oldClip = nullptr;
  Layer* oldLayer = mLayerBuilder->GetOldLayerFor(aItem, &oldGeometry, &oldClip);
  if (aNewLayer != oldLayer && oldLayer) {
    // The item has changed layers.
    // Invalidate the old bounds in the old layer and new bounds in the new layer.
    PaintedLayer* t = oldLayer->AsPaintedLayer();
    if (t && oldGeometry) {
      // Note that whenever the layer's scale changes, we invalidate the whole thing,
      // so it doesn't matter whether we are using the old scale at last paint
      // or a new scale here
#ifdef MOZ_DUMP_PAINTING
      if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
        printf_stderr("Display item type %s(%p) changed layers %p to %p!\n", aItem->Name(), aItem->Frame(), t, aNewLayer);
      }
#endif
      InvalidatePostTransformRegion(t,
          oldGeometry->ComputeInvalidationRegion(),
          *oldClip,
          mLayerBuilder->GetLastPaintOffset(t));
    }
    // Clear the old geometry so that invalidation thinks the item has been
    // added this paint.
    mLayerBuilder->ClearCachedGeometry(aItem);
    aItem->NotifyRenderingChanged();
  }
}

void
FrameLayerBuilder::ComputeGeometryChangeForItem(DisplayItemData* aData)
{
  nsDisplayItem *item = aData->mItem;
  PaintedLayer* paintedLayer = aData->mLayer->AsPaintedLayer();
  if (!item || !paintedLayer) {
    aData->EndUpdate();
    return;
  }

  PaintedLayerItemsEntry* entry = mPaintedLayerItems.GetEntry(paintedLayer);

  nsAutoPtr<nsDisplayItemGeometry> geometry(item->AllocateGeometry(mDisplayListBuilder));

  PaintedDisplayItemLayerUserData* layerData =
    static_cast<PaintedDisplayItemLayerUserData*>(aData->mLayer->GetUserData(&gPaintedDisplayItemLayerUserData));
  nsPoint shift = layerData->mAnimatedGeometryRootOrigin - layerData->mLastAnimatedGeometryRootOrigin;

  const DisplayItemClip& clip = item->GetClip();

  // If the frame is marked as invalidated, and didn't specify a rect to invalidate  then we want to
  // invalidate both the old and new bounds, otherwise we only want to invalidate the changed areas.
  // If we do get an invalid rect, then we want to add this on top of the change areas.
  nsRect invalid;
  nsRegion combined;
  bool notifyRenderingChanged = true;
  if (!aData->mGeometry) {
    // This item is being added for the first time, invalidate its entire area.
    //TODO: We call GetGeometry again in AddPaintedDisplayItem, we should reuse this.
    combined = clip.ApplyNonRoundedIntersection(geometry->ComputeInvalidationRegion());
#ifdef MOZ_DUMP_PAINTING
    if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
      printf_stderr("Display item type %s(%p) added to layer %p!\n", item->Name(), item->Frame(), aData->mLayer.get());
    }
#endif
  } else if (aData->mIsInvalid || (item->IsInvalid(invalid) && invalid.IsEmpty())) {
    // Either layout marked item as needing repainting, invalidate the entire old and new areas.
    combined = aData->mClip.ApplyNonRoundedIntersection(aData->mGeometry->ComputeInvalidationRegion());
    combined.MoveBy(shift);
    combined.Or(combined, clip.ApplyNonRoundedIntersection(geometry->ComputeInvalidationRegion()));
#ifdef MOZ_DUMP_PAINTING
    if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
      printf_stderr("Display item type %s(%p) (in layer %p) belongs to an invalidated frame!\n", item->Name(), item->Frame(), aData->mLayer.get());
    }
#endif
  } else {
    // Let the display item check for geometry changes and decide what needs to be
    // repainted.

    const nsTArray<nsIFrame*>& changedFrames = aData->GetFrameListChanges();

    // We have an optimization to cache the drawing background-attachment: fixed canvas
    // background images so we can scroll and just blit them when they are flattened into
    // the same layer as scrolling content. NotifyRenderingChanged is only used to tell
    // the canvas bg image item to purge this cache. We want to be careful not to accidentally
    // purge the cache if we are just invalidating due to scrolling (ie the background image
    // moves on the scrolling layer but it's rendering stays the same) so if
    // AddOffsetAndComputeDifference is the only thing that will invalidate we skip the
    // NotifyRenderingChanged call (ComputeInvalidationRegion for background images also calls
    // NotifyRenderingChanged if anything changes).
    if (aData->mGeometry->ComputeInvalidationRegion() == geometry->ComputeInvalidationRegion() &&
        aData->mClip == clip && invalid.IsEmpty() && changedFrames.Length() == 0) {
      notifyRenderingChanged = false;
    }

    aData->mGeometry->MoveBy(shift);
    item->ComputeInvalidationRegion(mDisplayListBuilder, aData->mGeometry, &combined);
    aData->mClip.AddOffsetAndComputeDifference(entry->mCommonClipCount,
                                               shift, aData->mGeometry->ComputeInvalidationRegion(),
                                               clip, entry->mLastCommonClipCount, geometry->ComputeInvalidationRegion(),
                                               &combined);

    // Add in any rect that the frame specified
    combined.Or(combined, invalid);

    for (uint32_t i = 0; i < changedFrames.Length(); i++) {
      combined.Or(combined, changedFrames[i]->GetVisualOverflowRect());
    }

    // Restrict invalidation to the clipped region
    nsRegion clipRegion;
    if (clip.ComputeRegionInClips(&aData->mClip, shift, &clipRegion)) {
      combined.And(combined, clipRegion);
    }
#ifdef MOZ_DUMP_PAINTING
    if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
      if (!combined.IsEmpty()) {
        printf_stderr("Display item type %s(%p) (in layer %p) changed geometry!\n", item->Name(), item->Frame(), aData->mLayer.get());
      }
    }
#endif
  }
  if (!combined.IsEmpty()) {
    if (notifyRenderingChanged) {
      item->NotifyRenderingChanged();
    }
    InvalidatePostTransformRegion(paintedLayer,
        combined.ScaleToOutsidePixels(layerData->mXScale, layerData->mYScale, layerData->mAppUnitsPerDevPixel),
        layerData->mTranslation,
        layerData);
  }

  aData->EndUpdate(geometry);
}

void
FrameLayerBuilder::AddPaintedDisplayItem(PaintedLayerData* aLayerData,
                                        nsDisplayItem* aItem,
                                        const DisplayItemClip& aClip,
                                        ContainerState& aContainerState,
                                        LayerState aLayerState,
                                        const nsPoint& aTopLeft)
{
  PaintedLayer* layer = aLayerData->mLayer;
  PaintedDisplayItemLayerUserData* paintedData =
    static_cast<PaintedDisplayItemLayerUserData*>
      (layer->GetUserData(&gPaintedDisplayItemLayerUserData));
  nsRefPtr<BasicLayerManager> tempManager;
  nsIntRect intClip;
  bool hasClip = false;
  if (aLayerState != LAYER_NONE) {
    DisplayItemData *data = GetDisplayItemDataForManager(aItem, layer->Manager());
    if (data) {
      tempManager = data->mInactiveManager;
    }
    if (!tempManager) {
      tempManager = new BasicLayerManager(BasicLayerManager::BLM_INACTIVE);
    }

    // We need to grab these before calling AddLayerDisplayItem because it will overwrite them.
    nsRegion clip;
    DisplayItemClip* oldClip = nullptr;
    GetOldLayerFor(aItem, nullptr, &oldClip);
    hasClip = aClip.ComputeRegionInClips(oldClip,
                                         aTopLeft - paintedData->mLastAnimatedGeometryRootOrigin,
                                         &clip);

    if (hasClip) {
      intClip = clip.GetBounds().ScaleToOutsidePixels(paintedData->mXScale,
                                                      paintedData->mYScale,
                                                      paintedData->mAppUnitsPerDevPixel);
    }
  }

  AddLayerDisplayItem(layer, aItem, aLayerState, aTopLeft, tempManager);

  PaintedLayerItemsEntry* entry = mPaintedLayerItems.PutEntry(layer);
  if (entry) {
    entry->mContainerLayerFrame = aContainerState.GetContainerFrame();
    if (entry->mContainerLayerGeneration == 0) {
      entry->mContainerLayerGeneration = mContainerLayerGeneration;
    }
    if (tempManager) {
      FLB_LOG_PAINTED_LAYER_DECISION(aLayerData, "Creating nested FLB for item %p\n", aItem);
      FrameLayerBuilder* layerBuilder = new FrameLayerBuilder();
      layerBuilder->Init(mDisplayListBuilder, tempManager, aLayerData);

      tempManager->BeginTransaction();
      if (mRetainingManager) {
        layerBuilder->DidBeginRetainedLayerTransaction(tempManager);
      }

      UniquePtr<LayerProperties> props(LayerProperties::CloneFrom(tempManager->GetRoot()));
      nsRefPtr<Layer> tmpLayer =
        aItem->BuildLayer(mDisplayListBuilder, tempManager, ContainerLayerParameters());
      // We have no easy way of detecting if this transaction will ever actually get finished.
      // For now, I've just silenced the warning with nested transactions in BasicLayers.cpp
      if (!tmpLayer) {
        tempManager->EndTransaction(nullptr, nullptr);
        tempManager->SetUserData(&gLayerManagerLayerBuilder, nullptr);
        return;
      }

      bool snap;
      nsRect visibleRect =
        aItem->GetVisibleRect().Intersect(aItem->GetBounds(mDisplayListBuilder, &snap));
      nsIntRegion rgn = visibleRect.ToOutsidePixels(paintedData->mAppUnitsPerDevPixel);
      SetOuterVisibleRegion(tmpLayer, &rgn);

      // If BuildLayer didn't call BuildContainerLayerFor, then our new layer won't have been
      // stored in layerBuilder. Manually add it now.
      if (mRetainingManager) {
#ifdef DEBUG_DISPLAY_ITEM_DATA
        LayerManagerData* parentLmd = static_cast<LayerManagerData*>
          (layer->Manager()->GetUserData(&gLayerManagerUserData));
        LayerManagerData* lmd = static_cast<LayerManagerData*>
          (tempManager->GetUserData(&gLayerManagerUserData));
        lmd->mParent = parentLmd;
#endif
        layerBuilder->StoreDataForFrame(aItem, tmpLayer, LAYER_ACTIVE);
      }

      tempManager->SetRoot(tmpLayer);
      layerBuilder->WillEndTransaction();
      tempManager->AbortTransaction();

      nsIntPoint offset = GetLastPaintOffset(layer) - GetTranslationForPaintedLayer(layer);
      props->MoveBy(-offset);
      nsIntRegion invalid = props->ComputeDifferences(tmpLayer, nullptr);
      if (aLayerState == LAYER_SVG_EFFECTS) {
        invalid = nsSVGIntegrationUtils::AdjustInvalidAreaForSVGEffects(aItem->Frame(),
                                                                        aItem->ToReferenceFrame(),
                                                                        invalid);
      }
      if (!invalid.IsEmpty()) {
#ifdef MOZ_DUMP_PAINTING
        if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
          printf_stderr("Inactive LayerManager(%p) for display item %s(%p) has an invalid region - invalidating layer %p\n", tempManager.get(), aItem->Name(), aItem->Frame(), layer);
        }
#endif
        invalid.ScaleRoundOut(paintedData->mXScale, paintedData->mYScale);

        if (hasClip) {
          invalid.And(invalid, intClip);
        }

        InvalidatePostTransformRegion(layer, invalid,
                                      GetTranslationForPaintedLayer(layer),
                                      paintedData);
      }
    }
    ClippedDisplayItem* cdi =
      entry->mItems.AppendElement(ClippedDisplayItem(aItem,
                                                     mContainerLayerGeneration));
    cdi->mInactiveLayerManager = tempManager;
  }
}

FrameLayerBuilder::DisplayItemData*
FrameLayerBuilder::StoreDataForFrame(nsDisplayItem* aItem, Layer* aLayer, LayerState aState)
{
  DisplayItemData* oldData = GetDisplayItemDataForManager(aItem, mRetainingManager);
  if (oldData) {
    if (!oldData->mUsed) {
      oldData->BeginUpdate(aLayer, aState, mContainerLayerGeneration, aItem);
    }
    return oldData;
  }

  LayerManagerData* lmd = static_cast<LayerManagerData*>
    (mRetainingManager->GetUserData(&gLayerManagerUserData));

  nsRefPtr<DisplayItemData> data =
    new DisplayItemData(lmd, aItem->GetPerFrameKey(), aLayer);

  data->BeginUpdate(aLayer, aState, mContainerLayerGeneration, aItem);

  lmd->mDisplayItems.PutEntry(data);
  return data;
}

void
FrameLayerBuilder::StoreDataForFrame(nsIFrame* aFrame,
                                     uint32_t aDisplayItemKey,
                                     Layer* aLayer,
                                     LayerState aState)
{
  DisplayItemData* oldData = GetDisplayItemData(aFrame, aDisplayItemKey);
  if (oldData && oldData->mFrameList.Length() == 1) {
    oldData->BeginUpdate(aLayer, aState, mContainerLayerGeneration);
    return;
  }

  LayerManagerData* lmd = static_cast<LayerManagerData*>
    (mRetainingManager->GetUserData(&gLayerManagerUserData));

  nsRefPtr<DisplayItemData> data =
    new DisplayItemData(lmd, aDisplayItemKey, aLayer, aFrame);

  data->BeginUpdate(aLayer, aState, mContainerLayerGeneration);

  lmd->mDisplayItems.PutEntry(data);
}

FrameLayerBuilder::ClippedDisplayItem::~ClippedDisplayItem()
{
  if (mInactiveLayerManager) {
    mInactiveLayerManager->SetUserData(&gLayerManagerLayerBuilder, nullptr);
  }
}

void
FrameLayerBuilder::AddLayerDisplayItem(Layer* aLayer,
                                       nsDisplayItem* aItem,
                                       LayerState aLayerState,
                                       const nsPoint& aTopLeft,
                                       BasicLayerManager* aManager)
{
  if (aLayer->Manager() != mRetainingManager)
    return;

  DisplayItemData *data = StoreDataForFrame(aItem, aLayer, aLayerState);
  data->mInactiveManager = aManager;
}

nsIntPoint
FrameLayerBuilder::GetLastPaintOffset(PaintedLayer* aLayer)
{
  PaintedLayerItemsEntry* entry = mPaintedLayerItems.PutEntry(aLayer);
  if (entry) {
    if (entry->mContainerLayerGeneration == 0) {
      entry->mContainerLayerGeneration = mContainerLayerGeneration;
    }
    if (entry->mHasExplicitLastPaintOffset)
      return entry->mLastPaintOffset;
  }
  return GetTranslationForPaintedLayer(aLayer);
}

void
FrameLayerBuilder::SavePreviousDataForLayer(PaintedLayer* aLayer, uint32_t aClipCount)
{
  PaintedLayerItemsEntry* entry = mPaintedLayerItems.PutEntry(aLayer);
  if (entry) {
    if (entry->mContainerLayerGeneration == 0) {
      entry->mContainerLayerGeneration = mContainerLayerGeneration;
    }
    entry->mLastPaintOffset = GetTranslationForPaintedLayer(aLayer);
    entry->mHasExplicitLastPaintOffset = true;
    entry->mLastCommonClipCount = aClipCount;
  }
}

bool
FrameLayerBuilder::CheckInLayerTreeCompressionMode()
{
  if (mInLayerTreeCompressionMode) {
    return true;
  }

  // If we wanted to be in layer tree compression mode, but weren't, then scheduled
  // a delayed repaint where we will be.
  mRootPresContext->PresShell()->GetRootFrame()->SchedulePaint(nsIFrame::PAINT_DELAYED_COMPRESS);

  return false;
}

void
ContainerState::CollectOldLayers()
{
  for (Layer* layer = mContainerLayer->GetFirstChild(); layer;
       layer = layer->GetNextSibling()) {
    NS_ASSERTION(!layer->HasUserData(&gMaskLayerUserData),
                 "Mask layer in layer tree; could not be recycled.");
    if (layer->HasUserData(&gPaintedDisplayItemLayerUserData)) {
      NS_ASSERTION(layer->AsPaintedLayer(), "Wrong layer type");
      mPaintedLayersAvailableForRecycling.PutEntry(static_cast<PaintedLayer*>(layer));
    }

    if (Layer* maskLayer = layer->GetMaskLayer()) {
      NS_ASSERTION(maskLayer->GetType() == Layer::TYPE_IMAGE,
                   "Could not recycle mask layer, unsupported layer type.");
      mRecycledMaskImageLayers.Put(layer, static_cast<ImageLayer*>(maskLayer));
    }
  }
}

struct OpaqueRegionEntry {
  const nsIFrame* mAnimatedGeometryRoot;
  const nsIFrame* mFixedPosFrameForLayerData;
  nsIntRegion mOpaqueRegion;
};

static OpaqueRegionEntry*
FindOpaqueRegionEntry(nsTArray<OpaqueRegionEntry>& aEntries,
                      const nsIFrame* aAnimatedGeometryRoot,
                      const nsIFrame* aFixedPosFrameForLayerData)
{
  for (uint32_t i = 0; i < aEntries.Length(); ++i) {
    OpaqueRegionEntry* d = &aEntries[i];
    if (d->mAnimatedGeometryRoot == aAnimatedGeometryRoot &&
        d->mFixedPosFrameForLayerData == aFixedPosFrameForLayerData) {
      return d;
    }
  }
  return nullptr;
}

void
ContainerState::SetupScrollingMetadata(NewLayerEntry* aEntry)
{
  if (mFlattenToSingleLayer) {
    // animated geometry roots are forced to all match, so we can't
    // use them and we don't get async scrolling.
    return;
  }

  nsAutoTArray<FrameMetrics,2> metricsArray;
  if (aEntry->mBaseFrameMetrics) {
    metricsArray.AppendElement(*aEntry->mBaseFrameMetrics);
  }
  uint32_t baseLength = metricsArray.Length();

  nsIFrame* fParent;
  for (const nsIFrame* f = aEntry->mAnimatedGeometryRoot;
       f != mContainerAnimatedGeometryRoot;
       f = nsLayoutUtils::GetAnimatedGeometryRootForFrame(this->mBuilder,
           fParent, mContainerAnimatedGeometryRoot)) {
    fParent = nsLayoutUtils::GetCrossDocParentFrame(f);
    if (!fParent) {
      // This means mContainerAnimatedGeometryRoot was not an ancestor
      // of aEntry->mAnimatedGeometryRoot. This is a weird case but it
      // can happen, e.g. when a scrolled frame contains a frame with opacity
      // which contains a frame that is not scrolled by the scrolled frame.
      // For now, we just don't apply any specific async scrolling to this layer.
      // It will async-scroll with mContainerAnimatedGeometryRoot, which
      // is substandard but not fatal.
      metricsArray.SetLength(baseLength);
      aEntry->mLayer->SetFrameMetrics(metricsArray);
      return;
    }

    nsIScrollableFrame* scrollFrame = nsLayoutUtils::GetScrollableFrameFor(f);
    if (!scrollFrame) {
      continue;
    }

    scrollFrame->ComputeFrameMetrics(aEntry->mLayer, mContainerReferenceFrame,
                                     mParameters, &metricsArray);
  }
  // Watch out for FrameMetrics copies in profiles
  aEntry->mLayer->SetFrameMetrics(metricsArray);
}

static void
InvalidateVisibleBoundsChangesForScrolledLayer(PaintedLayer* aLayer)
{
  PaintedDisplayItemLayerUserData* data =
    static_cast<PaintedDisplayItemLayerUserData*>(aLayer->GetUserData(&gPaintedDisplayItemLayerUserData));

  if (data->mIgnoreInvalidationsOutsideRect) {
    // We haven't invalidated anything outside *data->mIgnoreInvalidationsOutsideRect
    // during DLBI. Now is the right time to do that, because at this point aLayer
    // knows its new visible region.
    // We use the visible regions' bounds here (as opposed to the true region)
    // in order to limit rgn's complexity. The only possible disadvantage of
    // this is that it might cause us to unnecessarily recomposite parts of the
    // window that are in the visible region's bounds but not in the visible
    // region itself, but that is acceptable for scrolled layers.
    nsIntRegion rgn;
    rgn.Or(data->mOldVisibleBounds, aLayer->GetVisibleRegion().GetBounds());
    rgn.Sub(rgn, *data->mIgnoreInvalidationsOutsideRect);
    if (!rgn.IsEmpty()) {
      aLayer->InvalidateRegion(rgn);
#ifdef MOZ_DUMP_PAINTING
      if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
        printf_stderr("Invalidating changes of the visible region bounds of the scrolled contents\n");
        nsAutoCString str;
        AppendToString(str, rgn);
        printf_stderr("Invalidating layer %p: %s\n", aLayer, str.get());
      }
#endif
    }
    data->mIgnoreInvalidationsOutsideRect = Nothing();
  }
}

static inline const Maybe<ParentLayerIntRect>&
GetStationaryClipInContainer(Layer* aLayer)
{
  if (size_t metricsCount = aLayer->GetFrameMetricsCount()) {
    return aLayer->GetFrameMetrics(metricsCount - 1).GetClipRect();
  }
  return aLayer->GetClipRect();
}

void
ContainerState::PostprocessRetainedLayers(nsIntRegion* aOpaqueRegionForContainer)
{
  nsAutoTArray<OpaqueRegionEntry,4> opaqueRegions;
  bool hideAll = false;
  int32_t opaqueRegionForContainer = -1;

  for (int32_t i = mNewChildLayers.Length() - 1; i >= 0; --i) {
    NewLayerEntry* e = &mNewChildLayers.ElementAt(i);
    if (!e->mLayer) {
      continue;
    }

    // If mFlattenToSingleLayer is true, there isn't going to be any
    // async scrolling so we can apply all our opaqueness to the same
    // entry, the entry for mContainerAnimatedGeometryRoot.
    const nsIFrame* animatedGeometryRootForOpaqueness =
        mFlattenToSingleLayer ? mContainerAnimatedGeometryRoot : e->mAnimatedGeometryRoot;
    OpaqueRegionEntry* data = FindOpaqueRegionEntry(opaqueRegions,
        animatedGeometryRootForOpaqueness, e->mFixedPosFrameForLayerData);

    SetupScrollingMetadata(e);

    if (hideAll) {
      e->mVisibleRegion.SetEmpty();
    } else if (!e->mLayer->IsScrollbarContainer()) {
      const Maybe<ParentLayerIntRect>& clipRect = GetStationaryClipInContainer(e->mLayer);
      if (clipRect && opaqueRegionForContainer >= 0 &&
          opaqueRegions[opaqueRegionForContainer].mOpaqueRegion.Contains(ParentLayerIntRect::ToUntyped(*clipRect))) {
        e->mVisibleRegion.SetEmpty();
      } else if (data) {
        e->mVisibleRegion.Sub(e->mVisibleRegion, data->mOpaqueRegion);
      }
    }

    SetOuterVisibleRegionForLayer(e->mLayer, e->mVisibleRegion,
      e->mLayerContentsVisibleRect.width >= 0 ? &e->mLayerContentsVisibleRect : nullptr);

    PaintedLayer* p = e->mLayer->AsPaintedLayer();
    if (p) {
      InvalidateVisibleBoundsChangesForScrolledLayer(p);
    }

    if (!e->mOpaqueRegion.IsEmpty()) {
      const nsIFrame* animatedGeometryRootToCover = animatedGeometryRootForOpaqueness;
      if (e->mOpaqueForAnimatedGeometryRootParent &&
          nsLayoutUtils::GetAnimatedGeometryRootForFrame(mBuilder, e->mAnimatedGeometryRoot->GetParent(),
                                                         mContainerAnimatedGeometryRoot)
            == mContainerAnimatedGeometryRoot) {
        animatedGeometryRootToCover = mContainerAnimatedGeometryRoot;
        data = FindOpaqueRegionEntry(opaqueRegions,
            animatedGeometryRootToCover, e->mFixedPosFrameForLayerData);
      }

      if (!data) {
        if (animatedGeometryRootToCover == mContainerAnimatedGeometryRoot &&
            e->mFixedPosFrameForLayerData == mContainerFixedPosFrame) {
          NS_ASSERTION(opaqueRegionForContainer == -1, "Already found it?");
          opaqueRegionForContainer = opaqueRegions.Length();
        }
        data = opaqueRegions.AppendElement();
        data->mAnimatedGeometryRoot = animatedGeometryRootToCover;
        data->mFixedPosFrameForLayerData = e->mFixedPosFrameForLayerData;
      }

      nsIntRegion clippedOpaque = e->mOpaqueRegion;
      Maybe<ParentLayerIntRect> clipRect = e->mLayer->GetCombinedClipRect();
      if (clipRect) {
        clippedOpaque.AndWith(ParentLayerIntRect::ToUntyped(*clipRect));
      }
      data->mOpaqueRegion.Or(data->mOpaqueRegion, clippedOpaque);
      if (e->mHideAllLayersBelow) {
        hideAll = true;
      }
    }

    if (e->mLayer->GetType() == Layer::TYPE_READBACK) {
      // ReadbackLayers need to accurately read what's behind them. So,
      // we don't want to do any occlusion culling of layers behind them.
      // Theoretically we could just punch out the ReadbackLayer's rectangle
      // from all mOpaqueRegions, but that's probably not worth doing.
      opaqueRegions.Clear();
      opaqueRegionForContainer = -1;
    }
  }

  if (opaqueRegionForContainer >= 0) {
    aOpaqueRegionForContainer->Or(*aOpaqueRegionForContainer,
        opaqueRegions[opaqueRegionForContainer].mOpaqueRegion);
  }
}

void
ContainerState::Finish(uint32_t* aTextContentFlags, LayerManagerData* aData,
                       const nsIntRect& aContainerPixelBounds,
                       nsDisplayList* aChildItems, bool& aHasComponentAlphaChildren)
{
  mPaintedLayerDataTree.Finish();

  NS_ASSERTION(mContainerBounds.IsEqualInterior(mAccumulatedChildBounds),
               "Bounds computation mismatch");

  if (mLayerBuilder->IsBuildingRetainedLayers()) {
    nsIntRegion containerOpaqueRegion;
    PostprocessRetainedLayers(&containerOpaqueRegion);
    if (containerOpaqueRegion.Contains(aContainerPixelBounds)) {
      aChildItems->SetIsOpaque();
    }
  }

  uint32_t textContentFlags = 0;

  // Make sure that current/existing layers are added to the parent and are
  // in the correct order.
  Layer* layer = nullptr;
  Layer* prevChild = nullptr;
  for (uint32_t i = 0; i < mNewChildLayers.Length(); ++i, prevChild = layer) {
    if (!mNewChildLayers[i].mLayer) {
      continue;
    }

    layer = mNewChildLayers[i].mLayer;

    if (!layer->GetVisibleRegion().IsEmpty()) {
      textContentFlags |=
        layer->GetContentFlags() & (Layer::CONTENT_COMPONENT_ALPHA |
                                    Layer::CONTENT_COMPONENT_ALPHA_DESCENDANT |
                                    Layer::CONTENT_DISABLE_FLATTENING);

      // Notify the parent of component alpha children unless it's coming from
      // within a child that has asked not to contribute to layer flattening.
      if (mNewChildLayers[i].mPropagateComponentAlphaFlattening &&
          (layer->GetContentFlags() & Layer::CONTENT_COMPONENT_ALPHA)) {
        aHasComponentAlphaChildren = true;
      }
    }

    if (!layer->GetParent()) {
      // This is not currently a child of the container, so just add it
      // now.
      mContainerLayer->InsertAfter(layer, prevChild);
    } else {
      NS_ASSERTION(layer->GetParent() == mContainerLayer,
                   "Layer shouldn't be the child of some other container");
      if (layer->GetPrevSibling() != prevChild) {
        mContainerLayer->RepositionChild(layer, prevChild);
      }
    }
  }

  // Remove old layers that have become unused.
  if (!layer) {
    layer = mContainerLayer->GetFirstChild();
  } else {
    layer = layer->GetNextSibling();
  }
  while (layer) {
    Layer *layerToRemove = layer;
    layer = layer->GetNextSibling();
    mContainerLayer->RemoveChild(layerToRemove);
  }

  *aTextContentFlags = textContentFlags;
}

static inline gfxSize RoundToFloatPrecision(const gfxSize& aSize)
{
  return gfxSize(float(aSize.width), float(aSize.height));
}

static inline gfxSize NudgedToIntegerSize(const gfxSize& aSize)
{
  float width = aSize.width;
  float height = aSize.height;
  gfx::NudgeToInteger(&width);
  gfx::NudgeToInteger(&height);
  return gfxSize(width, height);
}

static void RestrictScaleToMaxLayerSize(gfxSize& aScale,
                                        const nsRect& aVisibleRect,
                                        nsIFrame* aContainerFrame,
                                        Layer* aContainerLayer)
{
  if (!aContainerLayer->Manager()->IsWidgetLayerManager()) {
    return;
  }

  nsIntRect pixelSize =
    aVisibleRect.ScaleToOutsidePixels(aScale.width, aScale.height,
                                      aContainerFrame->PresContext()->AppUnitsPerDevPixel());

  int32_t maxLayerSize = aContainerLayer->GetMaxLayerSize();

  if (pixelSize.width > maxLayerSize) {
    float scale = (float)pixelSize.width / maxLayerSize;
    scale = gfxUtils::ClampToScaleFactor(scale);
    aScale.width /= scale;
  }
  if (pixelSize.height > maxLayerSize) {
    float scale = (float)pixelSize.height / maxLayerSize;
    scale = gfxUtils::ClampToScaleFactor(scale);
    aScale.height /= scale;
  }
}
static bool
ChooseScaleAndSetTransform(FrameLayerBuilder* aLayerBuilder,
                           nsDisplayListBuilder* aDisplayListBuilder,
                           nsIFrame* aContainerFrame,
                           nsDisplayItem* aContainerItem,
                           const nsRect& aVisibleRect,
                           const Matrix4x4* aTransform,
                           const ContainerLayerParameters& aIncomingScale,
                           ContainerLayer* aLayer,
                           LayerState aState,
                           ContainerLayerParameters& aOutgoingScale)
{
  nsIntPoint offset;

  Matrix4x4 transform =
    Matrix4x4::Scaling(aIncomingScale.mXScale, aIncomingScale.mYScale, 1.0);
  if (aTransform) {
    // aTransform is applied first, then the scale is applied to the result
    transform = (*aTransform)*transform;
    // Set relevant 3d matrix entries that are close to integers to be those
    // exact integers. This protects against floating-point inaccuracies
    // causing problems in the CanDraw2D / Is2D checks below.
    // We don't nudge all matrix components here. In particular, we don't want to
    // nudge the X/Y translation components, because those include the scroll
    // offset, and we don't want scrolling to affect whether we nudge or not.
    transform.NudgeTo2D();
  }
  Matrix transform2d;
  if (aContainerFrame &&
      (aState == LAYER_INACTIVE || aState == LAYER_SVG_EFFECTS) &&
      (!aTransform || (aTransform->Is2D(&transform2d) &&
                       !transform2d.HasNonTranslation()))) {
    // When we have an inactive ContainerLayer, translate the container by the offset to the
    // reference frame (and offset all child layers by the reverse) so that the coordinate
    // space of the child layers isn't affected by scrolling.
    // This gets confusing for complicated transform (since we'd have to compute the scale
    // factors for the matrix), so we don't bother. Any frames that are building an nsDisplayTransform
    // for a css transform would have 0,0 as their offset to the reference frame, so this doesn't
    // matter.
    nsPoint appUnitOffset = aDisplayListBuilder->ToReferenceFrame(aContainerFrame);
    nscoord appUnitsPerDevPixel = aContainerFrame->PresContext()->AppUnitsPerDevPixel();
    offset = nsIntPoint(
        NS_lround(NSAppUnitsToDoublePixels(appUnitOffset.x, appUnitsPerDevPixel)*aIncomingScale.mXScale),
        NS_lround(NSAppUnitsToDoublePixels(appUnitOffset.y, appUnitsPerDevPixel)*aIncomingScale.mYScale));
  }
  transform.PostTranslate(offset.x + aIncomingScale.mOffset.x,
                          offset.y + aIncomingScale.mOffset.y,
                          0);

  if (transform.IsSingular()) {
    return false;
  }

  bool canDraw2D = transform.CanDraw2D(&transform2d);
  gfxSize scale;
  // XXX Should we do something for 3D transforms?
  if (canDraw2D) {
    // If the container's transform is animated off main thread, fix a suitable scale size
    // for animation
    if (aContainerFrame->GetContent() &&
        aContainerItem &&
        aContainerItem->GetType() == nsDisplayItem::TYPE_TRANSFORM &&
        nsLayoutUtils::HasAnimationsForCompositor(
          aContainerFrame->GetContent(), eCSSProperty_transform)) {
      // Use the size of the nearest widget as the maximum size.  This
      // is important since it might be a popup that is bigger than the
      // pres context's size.
      nsPresContext* presContext = aContainerFrame->PresContext();
      nsIWidget* widget = aContainerFrame->GetNearestWidget();
      nsSize displaySize;
      if (widget) {
        IntSize widgetSize = widget->GetClientSize();
        int32_t p2a = presContext->AppUnitsPerDevPixel();
        displaySize.width = NSIntPixelsToAppUnits(widgetSize.width, p2a);
        displaySize.height = NSIntPixelsToAppUnits(widgetSize.height, p2a);
      } else {
        displaySize = presContext->GetVisibleArea().Size();
      }
      // compute scale using the animation on the container (ignoring
      // its ancestors)
      scale = nsLayoutUtils::ComputeSuitableScaleForAnimation(
                aContainerFrame->GetContent(), aVisibleRect.Size(),
                displaySize);
      // multiply by the scale inherited from ancestors
      scale.width *= aIncomingScale.mXScale;
      scale.height *= aIncomingScale.mYScale;
    } else {
      // Scale factors are normalized to a power of 2 to reduce the number of resolution changes
      scale = RoundToFloatPrecision(ThebesMatrix(transform2d).ScaleFactors(true));
      // For frames with a changing transform that's not just a translation,
      // round scale factors up to nearest power-of-2 boundary so that we don't
      // keep having to redraw the content as it scales up and down. Rounding up to nearest
      // power-of-2 boundary ensures we never scale up, only down --- avoiding
      // jaggies. It also ensures we never scale down by more than a factor of 2,
      // avoiding bad downscaling quality.
      Matrix frameTransform;
      if (ActiveLayerTracker::IsStyleAnimated(aDisplayListBuilder, aContainerFrame, eCSSProperty_transform) &&
          aTransform &&
          (!aTransform->Is2D(&frameTransform) || frameTransform.HasNonTranslationOrFlip())) {
        // Don't clamp the scale factor when the new desired scale factor matches the old one
        // or it was previously unscaled.
        bool clamp = true;
        Matrix oldFrameTransform2d;
        if (aLayer->GetBaseTransform().Is2D(&oldFrameTransform2d)) {
          gfxSize oldScale = RoundToFloatPrecision(ThebesMatrix(oldFrameTransform2d).ScaleFactors(true));
          if (oldScale == scale || oldScale == gfxSize(1.0, 1.0)) {
            clamp = false;
          }
        }
        if (clamp) {
          scale.width = gfxUtils::ClampToScaleFactor(scale.width);
          scale.height = gfxUtils::ClampToScaleFactor(scale.height);
        }
      } else {
        scale = NudgedToIntegerSize(scale);
      }
    }
    // If the scale factors are too small, just use 1.0. The content is being
    // scaled out of sight anyway.
    if (fabs(scale.width) < 1e-8 || fabs(scale.height) < 1e-8) {
      scale = gfxSize(1.0, 1.0);
    }
    // If this is a transform container layer, then pre-rendering might
    // mean we try render a layer bigger than the max texture size. Apply
    // clmaping to prevent this.
    if (aTransform) {
      RestrictScaleToMaxLayerSize(scale, aVisibleRect, aContainerFrame, aLayer);
    }
  } else {
    scale = gfxSize(1.0, 1.0);
  }

  // Store the inverse of our resolution-scale on the layer
  aLayer->SetBaseTransform(transform);
  aLayer->SetPreScale(1.0f/float(scale.width),
                      1.0f/float(scale.height));
  aLayer->SetInheritedScale(aIncomingScale.mXScale,
                            aIncomingScale.mYScale);

  aOutgoingScale =
    ContainerLayerParameters(scale.width, scale.height, -offset, aIncomingScale);
  if (aTransform) {
    aOutgoingScale.mInTransformedSubtree = true;
    if (ActiveLayerTracker::IsStyleAnimated(aDisplayListBuilder, aContainerFrame,
                                            eCSSProperty_transform)) {
      aOutgoingScale.mInActiveTransformedSubtree = true;
    }
  }
  if (aLayerBuilder->IsBuildingRetainedLayers() &&
      (!canDraw2D || transform2d.HasNonIntegerTranslation())) {
    aOutgoingScale.mDisableSubpixelAntialiasingInDescendants = true;
  }
  return true;
}

/* static */ PLDHashOperator
FrameLayerBuilder::RestoreDisplayItemData(nsRefPtrHashKey<DisplayItemData>* aEntry, void* aUserArg)
{
  DisplayItemData* data = aEntry->GetKey();
  uint32_t *generation = static_cast<uint32_t*>(aUserArg);

  if (data->mUsed && data->mContainerLayerGeneration >= *generation) {
    return PL_DHASH_REMOVE;
  }

  return PL_DHASH_NEXT;
}

/* static */ PLDHashOperator
FrameLayerBuilder::RestorePaintedLayerItemEntries(PaintedLayerItemsEntry* aEntry, void* aUserArg)
{
  uint32_t *generation = static_cast<uint32_t*>(aUserArg);

  if (aEntry->mContainerLayerGeneration >= *generation) {
    // We can just remove these items rather than attempting to revert them
    // because we're going to want to invalidate everything when transitioning
    // to component alpha flattening.
    return PL_DHASH_REMOVE;
  }

  for (uint32_t i = 0; i < aEntry->mItems.Length(); i++) {
    if (aEntry->mItems[i].mContainerLayerGeneration >= *generation) {
      aEntry->mItems.TruncateLength(i);
      return PL_DHASH_NEXT;
    }
  }

  return PL_DHASH_NEXT;
}

already_AddRefed<ContainerLayer>
FrameLayerBuilder::BuildContainerLayerFor(nsDisplayListBuilder* aBuilder,
                                          LayerManager* aManager,
                                          nsIFrame* aContainerFrame,
                                          nsDisplayItem* aContainerItem,
                                          nsDisplayList* aChildren,
                                          const ContainerLayerParameters& aParameters,
                                          const Matrix4x4* aTransform,
                                          uint32_t aFlags)
{
  uint32_t containerDisplayItemKey =
    aContainerItem ? aContainerItem->GetPerFrameKey() : nsDisplayItem::TYPE_ZERO;
  NS_ASSERTION(aContainerFrame, "Container display items here should have a frame");
  NS_ASSERTION(!aContainerItem ||
               aContainerItem->Frame() == aContainerFrame,
               "Container display item must match given frame");

  if (!aParameters.mXScale || !aParameters.mYScale) {
    return nullptr;
  }

  nsRefPtr<ContainerLayer> containerLayer;
  if (aManager == mRetainingManager) {
    // Using GetOldLayerFor will search merged frames, as well as the underlying
    // frame. The underlying frame can change when a page scrolls, so this
    // avoids layer recreation in the situation that a new underlying frame is
    // picked for a layer.
    Layer* oldLayer = nullptr;
    if (aContainerItem) {
      oldLayer = GetOldLayerFor(aContainerItem);
    } else {
      DisplayItemData *data = GetOldLayerForFrame(aContainerFrame, containerDisplayItemKey);
      if (data) {
        oldLayer = data->mLayer;
      }
    }

    if (oldLayer) {
      NS_ASSERTION(oldLayer->Manager() == aManager, "Wrong manager");
      if (oldLayer->HasUserData(&gPaintedDisplayItemLayerUserData)) {
        // The old layer for this item is actually our PaintedLayer
        // because we rendered its layer into that PaintedLayer. So we
        // don't actually have a retained container layer.
      } else {
        NS_ASSERTION(oldLayer->GetType() == Layer::TYPE_CONTAINER,
                     "Wrong layer type");
        containerLayer = static_cast<ContainerLayer*>(oldLayer);
        containerLayer->SetMaskLayer(nullptr);
      }
    }
  }
  if (!containerLayer) {
    // No suitable existing layer was found.
    containerLayer = aManager->CreateContainerLayer();
    if (!containerLayer)
      return nullptr;
  }

  LayerState state = aContainerItem ? aContainerItem->GetLayerState(aBuilder, aManager, aParameters) : LAYER_ACTIVE;
  if (state == LAYER_INACTIVE &&
      nsDisplayItem::ForceActiveLayers()) {
    state = LAYER_ACTIVE;
  }

  if (aContainerItem && state == LAYER_ACTIVE_EMPTY) {
    // Empty layers only have metadata and should never have display items. We
    // early exit because later, invalidation will walk up the frame tree to
    // determine which painted layer gets invalidated. Since an empty layer
    // should never have anything to paint, it should never be invalidated.
    NS_ASSERTION(aChildren->IsEmpty(), "Should have no children");
    return containerLayer.forget();
  }

  ContainerLayerParameters scaleParameters;
  nsRect bounds = aChildren->GetBounds(aBuilder);
  nsRect childrenVisible =
      aContainerItem ? aContainerItem->GetVisibleRectForChildren() :
          aContainerFrame->GetVisualOverflowRectRelativeToSelf();
  if (!ChooseScaleAndSetTransform(this, aBuilder, aContainerFrame,
                                  aContainerItem,
                                  bounds.Intersect(childrenVisible),
                                  aTransform, aParameters,
                                  containerLayer, state, scaleParameters)) {
    return nullptr;
  }

  uint32_t oldGeneration = mContainerLayerGeneration;
  mContainerLayerGeneration = ++mMaxContainerLayerGeneration;

  nsRefPtr<RefCountedRegion> paintedLayerInvalidRegion = nullptr;
  if (mRetainingManager) {
    if (aContainerItem) {
      StoreDataForFrame(aContainerItem, containerLayer, LAYER_ACTIVE);
    } else {
      StoreDataForFrame(aContainerFrame, containerDisplayItemKey, containerLayer, LAYER_ACTIVE);
    }
  }

  LayerManagerData* data = static_cast<LayerManagerData*>
    (aManager->GetUserData(&gLayerManagerUserData));

  nsIntRect pixBounds;
  nscoord appUnitsPerDevPixel;
  bool flattenToSingleLayer = false;
  if ((aContainerFrame->GetStateBits() & NS_FRAME_NO_COMPONENT_ALPHA) &&
      mRetainingManager &&
      mRetainingManager->ShouldAvoidComponentAlphaLayers() &&
      !nsLayoutUtils::AsyncPanZoomEnabled(aContainerFrame))
  {
    flattenToSingleLayer = true;
  }

  nscolor backgroundColor = NS_RGBA(0,0,0,0);
  if (aFlags & CONTAINER_ALLOW_PULL_BACKGROUND_COLOR) {
    backgroundColor = aParameters.mBackgroundColor;
  }

  uint32_t flags;
  while (true) {
    ContainerState state(aBuilder, aManager, aManager->GetLayerBuilder(),
                         aContainerFrame, aContainerItem, bounds,
                         containerLayer, scaleParameters, flattenToSingleLayer,
                         backgroundColor);

    state.ProcessDisplayItems(aChildren);

    // Set CONTENT_COMPONENT_ALPHA if any of our children have it.
    // This is suboptimal ... a child could have text that's over transparent
    // pixels in its own layer, but over opaque parts of previous siblings.
    bool hasComponentAlphaChildren = false;
    pixBounds = state.ScaleToOutsidePixels(bounds, false);
    appUnitsPerDevPixel = state.GetAppUnitsPerDevPixel();
    state.Finish(&flags, data, pixBounds, aChildren, hasComponentAlphaChildren);

    if (hasComponentAlphaChildren &&
        !(flags & Layer::CONTENT_DISABLE_FLATTENING) &&
        mRetainingManager &&
        mRetainingManager->ShouldAvoidComponentAlphaLayers() &&
        containerLayer->HasMultipleChildren() &&
        !flattenToSingleLayer &&
        !nsLayoutUtils::AsyncPanZoomEnabled(aContainerFrame))
    {
      // Since we don't want any component alpha layers on BasicLayers, we repeat
      // the layer building process with this explicitely forced off.
      // We restore the previous FrameLayerBuilder state since the first set
      // of layer building will have changed it.
      flattenToSingleLayer = true;
      data->mDisplayItems.EnumerateEntries(RestoreDisplayItemData,
                                           &mContainerLayerGeneration);
      mPaintedLayerItems.EnumerateEntries(RestorePaintedLayerItemEntries,
                                         &mContainerLayerGeneration);
      aContainerFrame->AddStateBits(NS_FRAME_NO_COMPONENT_ALPHA);
      continue;
    }
    break;
  }

  // CONTENT_COMPONENT_ALPHA is propogated up to the nearest CONTENT_OPAQUE
  // ancestor so that BasicLayerManager knows when to copy the background into
  // pushed groups. Accelerated layers managers can't necessarily do this (only
  // when the visible region is a simple rect), so we propogate
  // CONTENT_COMPONENT_ALPHA_DESCENDANT all the way to the root.
  if (flags & Layer::CONTENT_COMPONENT_ALPHA) {
    flags |= Layer::CONTENT_COMPONENT_ALPHA_DESCENDANT;
  }

  // Make sure that rounding the visible region out didn't add any area
  // we won't paint
  if (aChildren->IsOpaque() && !aChildren->NeedsTransparentSurface()) {
    bounds.ScaleRoundIn(scaleParameters.mXScale, scaleParameters.mYScale);
    if (bounds.Contains(ToAppUnits(pixBounds, appUnitsPerDevPixel))) {
      // Clear CONTENT_COMPONENT_ALPHA and add CONTENT_OPAQUE instead.
      flags &= ~Layer::CONTENT_COMPONENT_ALPHA;
      flags |= Layer::CONTENT_OPAQUE;
    }
  }
  containerLayer->SetContentFlags(flags);
  // If aContainerItem is non-null some BuildContainerLayer further up the
  // call stack is responsible for setting containerLayer's visible region.
  if (!aContainerItem) {
    containerLayer->SetVisibleRegion(pixBounds);
  }
  if (aParameters.mLayerContentsVisibleRect) {
    *aParameters.mLayerContentsVisibleRect = pixBounds + scaleParameters.mOffset;
  }

  mContainerLayerGeneration = oldGeneration;
  nsPresContext::ClearNotifySubDocInvalidationData(containerLayer);

  return containerLayer.forget();
}

Layer*
FrameLayerBuilder::GetLeafLayerFor(nsDisplayListBuilder* aBuilder,
                                   nsDisplayItem* aItem)
{
  Layer* layer = GetOldLayerFor(aItem);
  if (!layer)
    return nullptr;
  if (layer->HasUserData(&gPaintedDisplayItemLayerUserData)) {
    // This layer was created to render Thebes-rendered content for this
    // display item. The display item should not use it for its own
    // layer rendering.
    return nullptr;
  }
  layer->SetMaskLayer(nullptr);
  return layer;
}

/* static */ void
FrameLayerBuilder::InvalidateAllLayers(LayerManager* aManager)
{
  LayerManagerData* data = static_cast<LayerManagerData*>
    (aManager->GetUserData(&gLayerManagerUserData));
  if (data) {
    data->mInvalidateAllLayers = true;
  }
}

/* static */ void
FrameLayerBuilder::InvalidateAllLayersForFrame(nsIFrame *aFrame)
{
  const nsTArray<DisplayItemData*>* array =
    static_cast<nsTArray<DisplayItemData*>*>(aFrame->Properties().Get(LayerManagerDataProperty()));
  if (array) {
    for (uint32_t i = 0; i < array->Length(); i++) {
      AssertDisplayItemData(array->ElementAt(i))->mParent->mInvalidateAllLayers = true;
    }
  }
}

/* static */
Layer*
FrameLayerBuilder::GetDedicatedLayer(nsIFrame* aFrame, uint32_t aDisplayItemKey)
{
  //TODO: This isn't completely correct, since a frame could exist as a layer
  // in the normal widget manager, and as a different layer (or no layer)
  // in the secondary manager

  const nsTArray<DisplayItemData*>* array =
    static_cast<nsTArray<DisplayItemData*>*>(aFrame->Properties().Get(LayerManagerDataProperty()));
  if (array) {
    for (uint32_t i = 0; i < array->Length(); i++) {
      DisplayItemData *element = AssertDisplayItemData(array->ElementAt(i));
      if (!element->mParent->mLayerManager->IsWidgetLayerManager()) {
        continue;
      }
      if (element->mDisplayItemKey == aDisplayItemKey) {
        if (element->mOptLayer) {
          return element->mOptLayer;
        }

        Layer* layer = element->mLayer;
        if (!layer->HasUserData(&gColorLayerUserData) &&
            !layer->HasUserData(&gImageLayerUserData) &&
            !layer->HasUserData(&gPaintedDisplayItemLayerUserData)) {
          return layer;
        }
      }
    }
  }
  return nullptr;
}

static gfxSize
PredictScaleForContent(nsIFrame* aFrame, nsIFrame* aAncestorWithScale,
                       const gfxSize& aScale)
{
  Matrix4x4 transform = Matrix4x4::Scaling(aScale.width, aScale.height, 1.0);
  if (aFrame != aAncestorWithScale) {
    // aTransform is applied first, then the scale is applied to the result
    transform = nsLayoutUtils::GetTransformToAncestor(aFrame, aAncestorWithScale)*transform;
  }
  Matrix transform2d;
  if (transform.CanDraw2D(&transform2d)) {
     return ThebesMatrix(transform2d).ScaleFactors(true);
  }
  return gfxSize(1.0, 1.0);
}

gfxSize
FrameLayerBuilder::GetPaintedLayerScaleForFrame(nsIFrame* aFrame)
{
  MOZ_ASSERT(aFrame, "need a frame");
  nsIFrame* last = nullptr;
  for (nsIFrame* f = aFrame; f; f = nsLayoutUtils::GetCrossDocParentFrame(f)) {
    last = f;

    if (nsLayoutUtils::IsPopup(f)) {
      // Don't examine ancestors of a popup. It won't make sense to check
      // the transform from some content inside the popup to some content
      // which is an ancestor of the popup.
      break;
    }

    const nsTArray<DisplayItemData*>* array =
      static_cast<nsTArray<DisplayItemData*>*>(f->Properties().Get(LayerManagerDataProperty()));
    if (!array) {
      continue;
    }

    for (uint32_t i = 0; i < array->Length(); i++) {
      Layer* layer = AssertDisplayItemData(array->ElementAt(i))->mLayer;
      ContainerLayer* container = layer->AsContainerLayer();
      if (!container ||
          !layer->Manager()->IsWidgetLayerManager()) {
        continue;
      }
      for (Layer* l = container->GetFirstChild(); l; l = l->GetNextSibling()) {
        PaintedDisplayItemLayerUserData* data =
            static_cast<PaintedDisplayItemLayerUserData*>
              (l->GetUserData(&gPaintedDisplayItemLayerUserData));
        if (data) {
          return PredictScaleForContent(aFrame, f, gfxSize(data->mXScale, data->mYScale));
        }
      }
    }
  }

  float presShellResolution = last->PresContext()->PresShell()->GetResolution();
  return PredictScaleForContent(aFrame, last,
      gfxSize(presShellResolution, presShellResolution));
}

#ifdef MOZ_DUMP_PAINTING
static void DebugPaintItem(DrawTarget& aDrawTarget,
                           nsPresContext* aPresContext,
                           nsDisplayItem *aItem,
                           nsDisplayListBuilder* aBuilder)
{
  bool snap;
  Rect bounds = NSRectToRect(aItem->GetBounds(aBuilder, &snap),
                             aPresContext->AppUnitsPerDevPixel());

  RefPtr<DrawTarget> tempDT =
    aDrawTarget.CreateSimilarDrawTarget(IntSize(bounds.width, bounds.height),
                                        SurfaceFormat::B8G8R8A8);
  nsRefPtr<gfxContext> context = new gfxContext(tempDT);
  context->SetMatrix(gfxMatrix::Translation(-bounds.x, -bounds.y));
  nsRenderingContext ctx(context);

  aItem->Paint(aBuilder, &ctx);
  RefPtr<SourceSurface> surface = tempDT->Snapshot();
  DumpPaintedImage(aItem, surface);

  aDrawTarget.DrawSurface(surface, bounds, Rect(Point(0,0), bounds.Size()));

  aItem->SetPainted();
}
#endif

/* static */ void
FrameLayerBuilder::RecomputeVisibilityForItems(nsTArray<ClippedDisplayItem>& aItems,
                                               nsDisplayListBuilder *aBuilder,
                                               const nsIntRegion& aRegionToDraw,
                                               const nsIntPoint& aOffset,
                                               int32_t aAppUnitsPerDevPixel,
                                               float aXScale,
                                               float aYScale)
{
  uint32_t i;
  // Update visible regions. We perform visibility analysis to take account
  // of occlusion culling.
  nsRegion visible = aRegionToDraw.ToAppUnits(aAppUnitsPerDevPixel);
  visible.MoveBy(NSIntPixelsToAppUnits(aOffset.x, aAppUnitsPerDevPixel),
                 NSIntPixelsToAppUnits(aOffset.y, aAppUnitsPerDevPixel));
  visible.ScaleInverseRoundOut(aXScale, aYScale);

  for (i = aItems.Length(); i > 0; --i) {
    ClippedDisplayItem* cdi = &aItems[i - 1];
    const DisplayItemClip& clip = cdi->mItem->GetClip();

    NS_ASSERTION(AppUnitsPerDevPixel(cdi->mItem) == aAppUnitsPerDevPixel,
                 "a painted layer should contain items only at the same zoom");

    MOZ_ASSERT(clip.HasClip() || clip.GetRoundedRectCount() == 0,
               "If we have rounded rects, we must have a clip rect");

    if (!clip.IsRectAffectedByClip(visible.GetBounds())) {
      cdi->mItem->RecomputeVisibility(aBuilder, &visible);
      continue;
    }

    // Do a little dance to account for the fact that we're clipping
    // to cdi->mClipRect
    nsRegion clipped;
    clipped.And(visible, clip.NonRoundedIntersection());
    nsRegion finalClipped = clipped;
    cdi->mItem->RecomputeVisibility(aBuilder, &finalClipped);
    // If we have rounded clip rects, don't subtract from the visible
    // region since we aren't displaying everything inside the rect.
    if (clip.GetRoundedRectCount() == 0) {
      nsRegion removed;
      removed.Sub(clipped, finalClipped);
      nsRegion newVisible;
      newVisible.Sub(visible, removed);
      // Don't let the visible region get too complex.
      if (newVisible.GetNumRects() <= 15) {
        visible = newVisible;
      }
    }
  }
}

void
FrameLayerBuilder::PaintItems(nsTArray<ClippedDisplayItem>& aItems,
                              const nsIntRect& aRect,
                              gfxContext *aContext,
                              nsRenderingContext *aRC,
                              nsDisplayListBuilder* aBuilder,
                              nsPresContext* aPresContext,
                              const nsIntPoint& aOffset,
                              float aXScale, float aYScale,
                              int32_t aCommonClipCount)
{
  DrawTarget& aDrawTarget = *aRC->GetDrawTarget();

  int32_t appUnitsPerDevPixel = aPresContext->AppUnitsPerDevPixel();
  nsRect boundRect = ToAppUnits(aRect, appUnitsPerDevPixel);
  boundRect.MoveBy(NSIntPixelsToAppUnits(aOffset.x, appUnitsPerDevPixel),
                 NSIntPixelsToAppUnits(aOffset.y, appUnitsPerDevPixel));
  boundRect.ScaleInverseRoundOut(aXScale, aYScale);

  DisplayItemClip currentClip;
  bool currentClipIsSetInContext = false;
  DisplayItemClip tmpClip;

  for (uint32_t i = 0; i < aItems.Length(); ++i) {
    ClippedDisplayItem* cdi = &aItems[i];

    nsRect paintRect = cdi->mItem->GetVisibleRect().Intersect(boundRect);
    if (paintRect.IsEmpty())
      continue;

#ifdef MOZ_DUMP_PAINTING
    PROFILER_LABEL_PRINTF("DisplayList", "Draw",
      js::ProfileEntry::Category::GRAPHICS, "%s", cdi->mItem->Name());
#else
    PROFILER_LABEL("DisplayList", "Draw",
      js::ProfileEntry::Category::GRAPHICS);
#endif

    // If the new desired clip state is different from the current state,
    // update the clip.
    const DisplayItemClip* clip = &cdi->mItem->GetClip();
    if (clip->GetRoundedRectCount() > 0 &&
        !clip->IsRectClippedByRoundedCorner(cdi->mItem->GetVisibleRect())) {
      tmpClip = *clip;
      tmpClip.RemoveRoundedCorners();
      clip = &tmpClip;
    }
    if (currentClipIsSetInContext != clip->HasClip() ||
        (clip->HasClip() && *clip != currentClip)) {
      if (currentClipIsSetInContext) {
        aContext->Restore();
      }
      currentClipIsSetInContext = clip->HasClip();
      if (currentClipIsSetInContext) {
        currentClip = *clip;
        aContext->Save();
        NS_ASSERTION(aCommonClipCount < 100,
          "Maybe you really do have more than a hundred clipping rounded rects, or maybe something has gone wrong.");
        currentClip.ApplyTo(aContext, aPresContext, aCommonClipCount);
        aContext->NewPath();
      }
    }

    if (cdi->mInactiveLayerManager) {
      bool saved = aDrawTarget.GetPermitSubpixelAA();
      PaintInactiveLayer(aBuilder, cdi->mInactiveLayerManager, cdi->mItem, aContext, aRC);
      aDrawTarget.SetPermitSubpixelAA(saved);
    } else {
      nsIFrame* frame = cdi->mItem->Frame();
      frame->AddStateBits(NS_FRAME_PAINTED_THEBES);
#ifdef MOZ_DUMP_PAINTING
      if (gfxUtils::sDumpPaintItems) {
        DebugPaintItem(aDrawTarget, aPresContext, cdi->mItem, aBuilder);
      } else {
#else
      {
#endif
        cdi->mItem->Paint(aBuilder, aRC);
      }
    }

    if (CheckDOMModified())
      break;
  }

  if (currentClipIsSetInContext) {
    aContext->Restore();
  }
}

/**
 * Returns true if it is preferred to draw the list of display
 * items separately for each rect in the visible region rather
 * than clipping to a complex region.
 */
static bool ShouldDrawRectsSeparately(gfxContext* aContext, DrawRegionClip aClip)
{
  if (!gfxPrefs::LayoutPaintRectsSeparately() ||
      aClip == DrawRegionClip::NONE) {
    return false;
  }

  DrawTarget *dt = aContext->GetDrawTarget();
  return !dt->SupportsRegionClipping();
}

static void DrawForcedBackgroundColor(DrawTarget& aDrawTarget,
                                      Layer* aLayer, nscolor
                                      aBackgroundColor)
{
  if (NS_GET_A(aBackgroundColor) > 0) {
    nsIntRect r = aLayer->GetVisibleRegion().GetBounds();
    ColorPattern color(ToDeviceColor(aBackgroundColor));
    aDrawTarget.FillRect(Rect(r.x, r.y, r.width, r.height), color);
  }
}

class LayerTimelineMarker : public TimelineMarker
{
public:
  LayerTimelineMarker(nsDocShell* aDocShell, const nsIntRegion& aRegion)
    : TimelineMarker(aDocShell, "Layer", TRACING_EVENT)
    , mRegion(aRegion)
  {
  }

  ~LayerTimelineMarker()
  {
  }

  virtual void AddLayerRectangles(mozilla::dom::Sequence<mozilla::dom::ProfileTimelineLayerRect>& aRectangles) override
  {
    nsIntRegionRectIterator it(mRegion);
    while (const nsIntRect* iterRect = it.Next()) {
      mozilla::dom::ProfileTimelineLayerRect rect;
      rect.mX = iterRect->X();
      rect.mY = iterRect->Y();
      rect.mWidth = iterRect->Width();
      rect.mHeight = iterRect->Height();
      aRectangles.AppendElement(rect, fallible);
    }
  }

private:
  nsIntRegion mRegion;
};

/*
 * A note on residual transforms:
 *
 * In a transformed subtree we sometimes apply the PaintedLayer's
 * "residual transform" when drawing content into the PaintedLayer.
 * This is a translation by components in the range [-0.5,0.5) provided
 * by the layer system; applying the residual transform followed by the
 * transforms used by layer compositing ensures that the subpixel alignment
 * of the content of the PaintedLayer exactly matches what it would be if
 * we used cairo/Thebes to draw directly to the screen without going through
 * retained layer buffers.
 *
 * The visible and valid regions of the PaintedLayer are computed without
 * knowing the residual transform (because we don't know what the residual
 * transform is going to be until we've built the layer tree!). So we have to
 * consider whether content painted in the range [x, xmost) might be painted
 * outside the visible region we computed for that content. The visible region
 * would be [floor(x), ceil(xmost)). The content would be rendered at
 * [x + r, xmost + r), where -0.5 <= r < 0.5. So some half-rendered pixels could
 * indeed fall outside the computed visible region, which is not a big deal;
 * similar issues already arise when we snap cliprects to nearest pixels.
 * Note that if the rendering of the content is snapped to nearest pixels ---
 * which it often is --- then the content is actually rendered at
 * [snap(x + r), snap(xmost + r)). It turns out that floor(x) <= snap(x + r)
 * and ceil(xmost) >= snap(xmost + r), so the rendering of snapped content
 * always falls within the visible region we computed.
 */

/* static */ void
FrameLayerBuilder::DrawPaintedLayer(PaintedLayer* aLayer,
                                   gfxContext* aContext,
                                   const nsIntRegion& aRegionToDraw,
                                   DrawRegionClip aClip,
                                   const nsIntRegion& aRegionToInvalidate,
                                   void* aCallbackData)
{
  DrawTarget& aDrawTarget = *aContext->GetDrawTarget();

  PROFILER_LABEL("FrameLayerBuilder", "DrawPaintedLayer",
    js::ProfileEntry::Category::GRAPHICS);

  nsDisplayListBuilder* builder = static_cast<nsDisplayListBuilder*>
    (aCallbackData);

  FrameLayerBuilder *layerBuilder = aLayer->Manager()->GetLayerBuilder();
  NS_ASSERTION(layerBuilder, "Unexpectedly null layer builder!");

  if (layerBuilder->CheckDOMModified())
    return;

  PaintedLayerItemsEntry* entry = layerBuilder->mPaintedLayerItems.GetEntry(aLayer);
  NS_ASSERTION(entry, "We shouldn't be drawing into a layer with no items!");
  if (!entry->mContainerLayerFrame) {
    return;
  }


  PaintedDisplayItemLayerUserData* userData =
    static_cast<PaintedDisplayItemLayerUserData*>
      (aLayer->GetUserData(&gPaintedDisplayItemLayerUserData));
  NS_ASSERTION(userData, "where did our user data go?");

  bool shouldDrawRectsSeparately = ShouldDrawRectsSeparately(aContext, aClip);

  if (!shouldDrawRectsSeparately) {
    if (aClip == DrawRegionClip::DRAW) {
      gfxUtils::ClipToRegion(aContext, aRegionToDraw);
    }

    DrawForcedBackgroundColor(aDrawTarget, aLayer,
                              userData->mForcedBackgroundColor);
  }

  if (NS_GET_A(userData->mFontSmoothingBackgroundColor) > 0) {
    aContext->SetFontSmoothingBackgroundColor(
      Color::FromABGR(userData->mFontSmoothingBackgroundColor));
  }

  // make the origin of the context coincide with the origin of the
  // PaintedLayer
  gfxContextMatrixAutoSaveRestore saveMatrix(aContext);
  nsIntPoint offset = GetTranslationForPaintedLayer(aLayer);
  nsPresContext* presContext = entry->mContainerLayerFrame->PresContext();

  if (!layerBuilder->GetContainingPaintedLayerData()) {
    // Recompute visibility of items in our PaintedLayer. Note that this
    // recomputes visibility for all descendants of our display items too,
    // so there's no need to do this for the items in inactive PaintedLayers.
    int32_t appUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();
    RecomputeVisibilityForItems(entry->mItems, builder, aRegionToDraw,
                                offset, appUnitsPerDevPixel,
                                userData->mXScale, userData->mYScale);
  }

  nsRenderingContext rc(aContext);

  if (shouldDrawRectsSeparately) {
    nsIntRegionRectIterator it(aRegionToDraw);
    while (const nsIntRect* iterRect = it.Next()) {
      gfxContextAutoSaveRestore save(aContext);
      aContext->NewPath();
      aContext->Rectangle(*iterRect);
      aContext->Clip();

      DrawForcedBackgroundColor(aDrawTarget, aLayer,
                                userData->mForcedBackgroundColor);

      // Apply the residual transform if it has been enabled, to ensure that
      // snapping when we draw into aContext exactly matches the ideal transform.
      // See above for why this is OK.
      aContext->SetMatrix(
        aContext->CurrentMatrix().Translate(aLayer->GetResidualTranslation() - gfxPoint(offset.x, offset.y)).
                                  Scale(userData->mXScale, userData->mYScale));

      layerBuilder->PaintItems(entry->mItems, *iterRect, aContext, &rc,
                               builder, presContext,
                               offset, userData->mXScale, userData->mYScale,
                               entry->mCommonClipCount);
    }
  } else {
    // Apply the residual transform if it has been enabled, to ensure that
    // snapping when we draw into aContext exactly matches the ideal transform.
    // See above for why this is OK.
    aContext->SetMatrix(
      aContext->CurrentMatrix().Translate(aLayer->GetResidualTranslation() - gfxPoint(offset.x, offset.y)).
                                Scale(userData->mXScale,userData->mYScale));

    layerBuilder->PaintItems(entry->mItems, aRegionToDraw.GetBounds(), aContext, &rc,
                             builder, presContext,
                             offset, userData->mXScale, userData->mYScale,
                             entry->mCommonClipCount);
  }

  aContext->SetFontSmoothingBackgroundColor(Color());

  bool isActiveLayerManager = !aLayer->Manager()->IsInactiveLayerManager();

  if (presContext->GetPaintFlashing() && isActiveLayerManager) {
    gfxContextAutoSaveRestore save(aContext);
    if (shouldDrawRectsSeparately) {
      if (aClip == DrawRegionClip::DRAW) {
        gfxUtils::ClipToRegion(aContext, aRegionToDraw);
      }
    }
    FlashPaint(aContext);
  }

  if (presContext && presContext->GetDocShell() && isActiveLayerManager) {
    nsDocShell* docShell = static_cast<nsDocShell*>(presContext->GetDocShell());
    bool isRecording;
    docShell->GetRecordProfileTimelineMarkers(&isRecording);
    if (isRecording) {
      mozilla::UniquePtr<TimelineMarker> marker =
        MakeUnique<LayerTimelineMarker>(docShell, aRegionToDraw);
      docShell->AddProfileTimelineMarker(Move(marker));
    }
  }

  if (!aRegionToInvalidate.IsEmpty()) {
    aLayer->AddInvalidRect(aRegionToInvalidate.GetBounds());
  }
}

bool
FrameLayerBuilder::CheckDOMModified()
{
  if (!mRootPresContext ||
      mInitialDOMGeneration == mRootPresContext->GetDOMGeneration())
    return false;
  if (mDetectedDOMModification) {
    // Don't spam the console with extra warnings
    return true;
  }
  mDetectedDOMModification = true;
  // Painting is not going to complete properly. There's not much
  // we can do here though. Invalidating the window to get another repaint
  // is likely to lead to an infinite repaint loop.
  NS_WARNING("Detected DOM modification during paint, bailing out!");
  return true;
}

/* static */ void
FrameLayerBuilder::DumpRetainedLayerTree(LayerManager* aManager, std::stringstream& aStream, bool aDumpHtml)
{
  aManager->Dump(aStream, "", aDumpHtml);
}

nsDisplayItemGeometry*
FrameLayerBuilder::GetMostRecentGeometry(nsDisplayItem* aItem)
{
  typedef nsTArray<DisplayItemData*> DataArray;

  // Retrieve the array of DisplayItemData associated with our frame.
  FrameProperties properties = aItem->Frame()->Properties();
  const DataArray* dataArray =
    static_cast<DataArray*>(properties.Get(LayerManagerDataProperty()));
  if (!dataArray) {
    return nullptr;
  }

  // Find our display item data, if it exists, and return its geometry.
  uint32_t itemPerFrameKey = aItem->GetPerFrameKey();
  for (uint32_t i = 0; i < dataArray->Length(); i++) {
    DisplayItemData* data = AssertDisplayItemData(dataArray->ElementAt(i));
    if (data->GetDisplayItemKey() == itemPerFrameKey) {
      return data->GetGeometry();
    }
  }

  return nullptr;
}

gfx::Rect
CalculateBounds(const nsTArray<DisplayItemClip::RoundedRect>& aRects, int32_t A2D)
{
  nsRect bounds = aRects[0].mRect;
  for (uint32_t i = 1; i < aRects.Length(); ++i) {
    bounds.UnionRect(bounds, aRects[i].mRect);
   }

  return gfx::ToRect(nsLayoutUtils::RectToGfxRect(bounds, A2D));
}

static void
SetClipCount(PaintedDisplayItemLayerUserData* apaintedData,
             uint32_t aClipCount)
{
  if (apaintedData) {
    apaintedData->mMaskClipCount = aClipCount;
  }
}

void
ContainerState::SetupMaskLayer(Layer *aLayer,
                               const DisplayItemClip& aClip,
                               const nsIntRegion& aLayerVisibleRegion,
                               uint32_t aRoundedRectClipCount)
{
  // if the number of clips we are going to mask has decreased, then aLayer might have
  // cached graphics which assume the existence of a soon-to-be non-existent mask layer
  // in that case, invalidate the whole layer.
  PaintedDisplayItemLayerUserData* paintedData = GetPaintedDisplayItemLayerUserData(aLayer);
  if (paintedData &&
      aRoundedRectClipCount < paintedData->mMaskClipCount) {
    PaintedLayer* painted = aLayer->AsPaintedLayer();
    painted->InvalidateRegion(painted->GetValidRegion().GetBounds());
  }

  // don't build an unnecessary mask
  nsIntRect layerBounds = aLayerVisibleRegion.GetBounds();
  if (aClip.GetRoundedRectCount() == 0 ||
      aRoundedRectClipCount == 0 ||
      layerBounds.IsEmpty()) {
    SetClipCount(paintedData, 0);
    return;
  }

  // check if we can re-use the mask layer
  nsRefPtr<ImageLayer> maskLayer =  CreateOrRecycleMaskImageLayerFor(aLayer);
  MaskLayerUserData* userData = GetMaskLayerUserData(maskLayer);

  MaskLayerUserData newData;
  aClip.AppendRoundedRects(&newData.mRoundedClipRects, aRoundedRectClipCount);
  newData.mScaleX = mParameters.mXScale;
  newData.mScaleY = mParameters.mYScale;
  newData.mOffset = mParameters.mOffset;
  newData.mAppUnitsPerDevPixel = mContainerFrame->PresContext()->AppUnitsPerDevPixel();

  if (*userData == newData) {
    aLayer->SetMaskLayer(maskLayer);
    SetClipCount(paintedData, aRoundedRectClipCount);
    return;
  }

  // calculate a more precise bounding rect
  gfx::Rect boundingRect = CalculateBounds(newData.mRoundedClipRects,
                                           newData.mAppUnitsPerDevPixel);
  boundingRect.Scale(mParameters.mXScale, mParameters.mYScale);

  uint32_t maxSize = mManager->GetMaxTextureSize();
  NS_ASSERTION(maxSize > 0, "Invalid max texture size");
  gfx::Size surfaceSize(std::min<gfx::Float>(boundingRect.Width(), maxSize),
                        std::min<gfx::Float>(boundingRect.Height(), maxSize));

  // maskTransform is applied to the clip when it is painted into the mask (as a
  // component of imageTransform), and its inverse used when the mask is used for
  // masking.
  // It is the transform from the masked layer's space to mask space
  gfx::Matrix maskTransform =
    Matrix::Scaling(surfaceSize.width / boundingRect.Width(),
                    surfaceSize.height / boundingRect.Height());
  gfx::Point p = boundingRect.TopLeft();
  maskTransform.PreTranslate(-p.x, -p.y);
  // imageTransform is only used when the clip is painted to the mask
  gfx::Matrix imageTransform = maskTransform;
  imageTransform.PreScale(mParameters.mXScale, mParameters.mYScale);

  nsAutoPtr<MaskLayerImageCache::MaskLayerImageKey> newKey(
    new MaskLayerImageCache::MaskLayerImageKey());

  // copy and transform the rounded rects
  for (uint32_t i = 0; i < newData.mRoundedClipRects.Length(); ++i) {
    newKey->mRoundedClipRects.AppendElement(
      MaskLayerImageCache::PixelRoundedRect(newData.mRoundedClipRects[i],
                                            mContainerFrame->PresContext()));
    newKey->mRoundedClipRects[i].ScaleAndTranslate(imageTransform);
  }

  const MaskLayerImageCache::MaskLayerImageKey* lookupKey = newKey;

  // check to see if we can reuse a mask image
  nsRefPtr<ImageContainer> container =
    GetMaskLayerImageCache()->FindImageFor(&lookupKey);

  if (!container) {
    IntSize surfaceSizeInt(NSToIntCeil(surfaceSize.width),
                           NSToIntCeil(surfaceSize.height));
    // no existing mask image, so build a new one
    RefPtr<DrawTarget> dt =
      aLayer->Manager()->CreateOptimalMaskDrawTarget(surfaceSizeInt);

    // fail if we can't get the right surface
    if (!dt) {
      NS_WARNING("Could not create DrawTarget for mask layer.");
      SetClipCount(paintedData, 0);
      return;
    }

    nsRefPtr<gfxContext> context = new gfxContext(dt);
    context->Multiply(ThebesMatrix(imageTransform));

    // paint the clipping rects with alpha to create the mask
    aClip.FillIntersectionOfRoundedRectClips(context,
                                             Color(1.f, 1.f, 1.f, 1.f),
                                             newData.mAppUnitsPerDevPixel,
                                             0,
                                             aRoundedRectClipCount);

    RefPtr<SourceSurface> surface = dt->Snapshot();

    // build the image and container
    container = aLayer->Manager()->CreateImageContainer();
    NS_ASSERTION(container, "Could not create image container for mask layer.");
    nsRefPtr<Image> image = container->CreateImage(ImageFormat::CAIRO_SURFACE);
    NS_ASSERTION(image, "Could not create image container for mask layer.");
    CairoImage::Data data;
    data.mSize = surfaceSizeInt;
    data.mSourceSurface = surface;

    static_cast<CairoImage*>(image.get())->SetData(data);
    container->SetCurrentImageInTransaction(image);

    GetMaskLayerImageCache()->PutImage(newKey.forget(), container);
  }

  maskLayer->SetContainer(container);

  maskTransform.Invert();
  Matrix4x4 matrix = Matrix4x4::From2D(maskTransform);
  matrix.PreTranslate(mParameters.mOffset.x, mParameters.mOffset.y, 0);
  maskLayer->SetBaseTransform(matrix);

  // save the details of the clip in user data
  userData->mScaleX = newData.mScaleX;
  userData->mScaleY = newData.mScaleY;
  userData->mOffset = newData.mOffset;
  userData->mAppUnitsPerDevPixel = newData.mAppUnitsPerDevPixel;
  userData->mRoundedClipRects.SwapElements(newData.mRoundedClipRects);
  userData->mImageKey = lookupKey;

  aLayer->SetMaskLayer(maskLayer);
  SetClipCount(paintedData, aRoundedRectClipCount);
  return;
}

} // namespace mozilla
