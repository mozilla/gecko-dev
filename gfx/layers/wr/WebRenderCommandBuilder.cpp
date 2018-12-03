/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderCommandBuilder.h"

#include "BasicLayers.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/layers/ClipManager.h"
#include "mozilla/layers/ImageClient.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/layers/IpcResourceUpdateQueue.h"
#include "mozilla/layers/SharedSurfacesChild.h"
#include "mozilla/layers/SourceSurfaceSharedData.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/UpdateImageHelper.h"
#include "mozilla/layers/WebRenderDrawEventRecorder.h"
#include "UnitTransforms.h"
#include "gfxEnv.h"
#include "nsDisplayListInvalidation.h"
#include "WebRenderCanvasRenderer.h"
#include "LayersLogging.h"
#include "LayerTreeInvalidation.h"

namespace mozilla {
namespace layers {

using namespace gfx;
static bool PaintByLayer(nsDisplayItem* aItem,
                         nsDisplayListBuilder* aDisplayListBuilder,
                         const RefPtr<BasicLayerManager>& aManager,
                         gfxContext* aContext, const gfx::Size& aScale,
                         const std::function<void()>& aPaintFunc);
static int sIndent;
#include <stdarg.h>
#include <stdio.h>

static void GP(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
#if 0
    for (int i = 0; i < sIndent; i++) { printf(" "); }
    vprintf(fmt, args);
#endif
  va_end(args);
}

// XXX: problems:
// - How do we deal with scrolling while having only a single invalidation rect?
// We can have a valid rect and an invalid rect. As we scroll the valid rect
// will move and the invalid rect will be the new area

struct BlobItemData;
static void DestroyBlobGroupDataProperty(nsTArray<BlobItemData*>* aArray);
NS_DECLARE_FRAME_PROPERTY_WITH_DTOR(BlobGroupDataProperty,
                                    nsTArray<BlobItemData*>,
                                    DestroyBlobGroupDataProperty);

// These are currently manually allocated and ownership is help by the
// mDisplayItems hash table in DIGroup
struct BlobItemData {
  // a weak pointer to the frame for this item.
  // DisplayItemData has a mFrameList to deal with merged frames. Hopefully we
  // don't need to worry about that.
  nsIFrame* mFrame;

  uint32_t mDisplayItemKey;
  nsTArray<BlobItemData*>*
      mArray;  // a weak pointer to the array that's owned by the frame property

  IntRect mRect;
  // It would be nice to not need this. We need to be able to call
  // ComputeInvalidationRegion. ComputeInvalidationRegion will sometimes reach
  // into parent style structs to get information that can change the
  // invalidation region
  UniquePtr<nsDisplayItemGeometry> mGeometry;
  DisplayItemClip mClip;
  bool mUsed;  // initialized near construction

  // a weak pointer to the group that owns this item
  // we use this to track whether group for a particular item has changed
  struct DIGroup* mGroup;

  // XXX: only used for debugging
  bool mInvalid;
  bool mInvalidRegion;
  bool mEmpty;

  // properties that are used to emulate layer tree invalidation
  Matrix mMatrix;  // updated to track the current transform to device space
  Matrix4x4Flagged mTransform;  // only used with nsDisplayTransform items to
                                // detect transform changes
  float mOpacity;  // only used with nsDisplayOpacity items to detect change to
                   // opacity
  RefPtr<BasicLayerManager> mLayerManager;

  IntRect mImageRect;
  LayerIntPoint mGroupOffset;

  BlobItemData(DIGroup* aGroup, nsDisplayItem* aItem)
      : mUsed(false), mGroup(aGroup), mOpacity(0.0) {
    mInvalid = false;
    mInvalidRegion = false;
    mEmpty = false;
    mDisplayItemKey = aItem->GetPerFrameKey();
    AddFrame(aItem->Frame());
  }

 private:
  void AddFrame(nsIFrame* aFrame) {
    mFrame = aFrame;

    nsTArray<BlobItemData*>* array =
        aFrame->GetProperty(BlobGroupDataProperty());
    if (!array) {
      array = new nsTArray<BlobItemData*>();
      aFrame->SetProperty(BlobGroupDataProperty(), array);
    }
    array->AppendElement(this);
    mArray = array;
  }

 public:
  void ClearFrame() {
    // Delete the weak pointer to this BlobItemData on the frame
    MOZ_RELEASE_ASSERT(mFrame);
    // the property may already be removed if WebRenderUserData got deleted
    // first so we use our own mArray pointer.
    mArray->RemoveElement(this);

    // drop the entire property if nothing's left in the array
    if (mArray->IsEmpty()) {
      // If the frame is in the process of being destroyed this will fail
      // but that's ok, because the the property will be removed then anyways
      mFrame->DeleteProperty(BlobGroupDataProperty());
    }
    mFrame = nullptr;
  }

  ~BlobItemData() {
    if (mFrame) {
      ClearFrame();
    }
  }
};

static BlobItemData* GetBlobItemData(nsDisplayItem* aItem) {
  nsIFrame* frame = aItem->Frame();
  uint32_t key = aItem->GetPerFrameKey();
  const nsTArray<BlobItemData*>* array =
      frame->GetProperty(BlobGroupDataProperty());
  if (array) {
    for (BlobItemData* item : *array) {
      if (item->mDisplayItemKey == key) {
        return item;
      }
    }
  }
  return nullptr;
}

// We keep around the BlobItemData so that when we invalidate it get properly
// included in the rect
static void DestroyBlobGroupDataProperty(nsTArray<BlobItemData*>* aArray) {
  for (BlobItemData* item : *aArray) {
    GP("DestroyBlobGroupDataProperty: %p-%d\n", item->mFrame,
       item->mDisplayItemKey);
    item->mFrame = nullptr;
  }
  delete aArray;
}

static void TakeExternalSurfaces(
    WebRenderDrawEventRecorder* aRecorder,
    std::vector<RefPtr<SourceSurface>>& aExternalSurfaces,
    WebRenderLayerManager* aManager, wr::IpcResourceUpdateQueue& aResources) {
  aRecorder->TakeExternalSurfaces(aExternalSurfaces);

  for (auto& surface : aExternalSurfaces) {
    if (surface->GetType() != SurfaceType::DATA_SHARED) {
      MOZ_ASSERT_UNREACHABLE("External surface that is not a shared surface!");
      continue;
    }

    // While we don't use the image key with the surface, because the blob image
    // renderer doesn't have easy access to the resource set, we still want to
    // ensure one is generated. That will ensure the surface remains alive until
    // at least the last epoch which the blob image could be used in.
    wr::ImageKey key;
    auto sharedSurface = static_cast<SourceSurfaceSharedData*>(surface.get());
    SharedSurfacesChild::Share(sharedSurface, aManager, aResources, key);
  }
}

struct DIGroup;
struct Grouper {
  explicit Grouper(ClipManager& aClipManager)
      : mAppUnitsPerDevPixel(0),
        mDisplayListBuilder(nullptr),
        mClipManager(aClipManager) {}

  int32_t mAppUnitsPerDevPixel;
  std::vector<nsDisplayItem*> mItemStack;
  nsDisplayListBuilder* mDisplayListBuilder;
  ClipManager& mClipManager;
  Matrix mTransform;

  // Paint the list of aChildren display items.
  void PaintContainerItem(DIGroup* aGroup, nsDisplayItem* aItem,
                          const IntRect& aItemBounds, nsDisplayList* aChildren,
                          gfxContext* aContext,
                          WebRenderDrawEventRecorder* aRecorder);

  // Builds groups of display items split based on 'layer activity'
  void ConstructGroups(nsDisplayListBuilder* aDisplayListBuilder,
                       WebRenderCommandBuilder* aCommandBuilder,
                       wr::DisplayListBuilder& aBuilder,
                       wr::IpcResourceUpdateQueue& aResources, DIGroup* aGroup,
                       nsDisplayList* aList, const StackingContextHelper& aSc);
  // Builds a group of display items without promoting anything to active.
  void ConstructGroupInsideInactive(WebRenderCommandBuilder* aCommandBuilder,
                                    wr::DisplayListBuilder& aBuilder,
                                    wr::IpcResourceUpdateQueue& aResources,
                                    DIGroup* aGroup, nsDisplayList* aList,
                                    const StackingContextHelper& aSc);
  // Helper method for processing a single inactive item
  void ConstructItemInsideInactive(WebRenderCommandBuilder* aCommandBuilder,
                                   wr::DisplayListBuilder& aBuilder,
                                   wr::IpcResourceUpdateQueue& aResources,
                                   DIGroup* aGroup, nsDisplayItem* aItem,
                                   const StackingContextHelper& aSc);
  ~Grouper() {}
};

// Returns whether this is an item for which complete invalidation was
// reliant on LayerTreeInvalidation in the pre-webrender world.
static bool IsContainerLayerItem(nsDisplayItem* aItem) {
  switch (aItem->GetType()) {
    case DisplayItemType::TYPE_WRAP_LIST:
    case DisplayItemType::TYPE_TRANSFORM:
    case DisplayItemType::TYPE_OPACITY:
    case DisplayItemType::TYPE_FILTER:
    case DisplayItemType::TYPE_BLEND_CONTAINER:
    case DisplayItemType::TYPE_BLEND_MODE:
    case DisplayItemType::TYPE_MASK: {
      return true;
    }
    default: { return false; }
  }
}

#include <sstream>

bool UpdateContainerLayerPropertiesAndDetectChange(
    nsDisplayItem* aItem, BlobItemData* aData,
    nsDisplayItemGeometry& aGeometry) {
  bool changed = false;
  switch (aItem->GetType()) {
    case DisplayItemType::TYPE_TRANSFORM: {
      auto transformItem = static_cast<nsDisplayTransform*>(aItem);
      Matrix4x4Flagged trans = transformItem->GetTransform();
      changed = aData->mTransform != trans;

      if (changed) {
        std::stringstream ss;
        // ss << trans << ' ' << aData->mTransform;
        // GP("UpdateContainerLayerPropertiesAndDetectChange Matrix %d %s\n",
        //   changed, ss.str().c_str());
      }

      aData->mTransform = trans;
      break;
    }
    case DisplayItemType::TYPE_OPACITY: {
      auto opacityItem = static_cast<nsDisplayOpacity*>(aItem);
      float opacity = opacityItem->GetOpacity();
      changed = aData->mOpacity != opacity;
      aData->mOpacity = opacity;
      GP("UpdateContainerLayerPropertiesAndDetectChange Opacity\n");
      break;
    }
    case DisplayItemType::TYPE_MASK:
    case DisplayItemType::TYPE_FILTER: {
      // These two items go through BasicLayerManager composition which clips to
      // the BuildingRect
      aGeometry.mBounds = aGeometry.mBounds.Intersect(aItem->GetBuildingRect());
      break;
    }
    default:
      break;
  }

  return changed || !aGeometry.mBounds.IsEqualEdges(aData->mGeometry->mBounds);
}

struct DIGroup {
  // XXX: Storing owning pointers to the BlobItemData in a hash table is not
  // a good choice. There are two better options:
  //
  // 1. We should just be using a linked list for this stuff.
  //    That we can iterate over only the used items.
  //    We remove from the unused list and add to the used list
  //    when we see an item.
  //
  //    we allocate using a free list.
  //
  // 2. We can use a Vec and use SwapRemove().
  //    We'll just need to be careful when iterating.
  //    The advantage of a Vec is that everything stays compact
  //    and we don't need to heap allocate the BlobItemData's
  nsTHashtable<nsPtrHashKey<BlobItemData>> mDisplayItems;

  IntRect mInvalidRect;
  nsRect mGroupBounds;
  LayerIntRect mPaintRect;
  int32_t mAppUnitsPerDevPixel;
  gfx::Size mScale;
  ScrollableLayerGuid::ViewID mScrollId;
  LayerPoint mResidualOffset;
  LayerIntRect mLayerBounds;
  // The current bounds of the blob image, relative to
  // the top-left of the mLayerBounds.
  IntRect mImageBounds;
  Maybe<wr::BlobImageKey> mKey;
  std::vector<RefPtr<SourceSurface>> mExternalSurfaces;
  std::vector<RefPtr<ScaledFont>> mFonts;

  DIGroup()
      : mAppUnitsPerDevPixel(0),
        mScrollId(ScrollableLayerGuid::NULL_SCROLL_ID) {}

  void InvalidateRect(const IntRect& aRect) {
    // Empty rects get dropped
    mInvalidRect = mInvalidRect.Union(aRect);
  }

  IntRect ItemBounds(nsDisplayItem* aItem) {
    BlobItemData* data = GetBlobItemData(aItem);
    return data->mRect;
  }

  void ClearItems() {
    GP("items: %d\n", mDisplayItems.Count());
    for (auto iter = mDisplayItems.Iter(); !iter.Done(); iter.Next()) {
      BlobItemData* data = iter.Get()->GetKey();
      GP("Deleting %p-%d\n", data->mFrame, data->mDisplayItemKey);
      iter.Remove();
      delete data;
    }
  }

  void ClearImageKey(WebRenderLayerManager* aManager, bool aForce = false) {
    if (mKey) {
      MOZ_RELEASE_ASSERT(aForce || mInvalidRect.IsEmpty());
      aManager->AddBlobImageKeyForDiscard(mKey.value());
      mKey = Nothing();
    }
    mFonts.clear();
  }

  static IntRect ToDeviceSpace(nsRect aBounds, Matrix& aMatrix,
                               int32_t aAppUnitsPerDevPixel,
                               LayerIntPoint aOffset) {
    // RoundedOut can convert empty rectangles to non-empty ones
    // so special case them here
    if (aBounds.IsEmpty()) {
      return IntRect();
    }
    return RoundedOut(aMatrix.TransformBounds(ToRect(
               nsLayoutUtils::RectToGfxRect(aBounds, aAppUnitsPerDevPixel)))) -
           aOffset.ToUnknownPoint();
  }

  void ComputeGeometryChange(nsDisplayItem* aItem, BlobItemData* aData,
                             Matrix& aMatrix, nsDisplayListBuilder* aBuilder) {
    // If the frame is marked as invalidated, and didn't specify a rect to
    // invalidate then we want to invalidate both the old and new bounds,
    // otherwise we only want to invalidate the changed areas. If we do get an
    // invalid rect, then we want to add this on top of the change areas.
    nsRect invalid;
    nsRegion combined;
    const DisplayItemClip& clip = aItem->GetClip();

    int32_t appUnitsPerDevPixel =
        aItem->Frame()->PresContext()->AppUnitsPerDevPixel();
    MOZ_RELEASE_ASSERT(mAppUnitsPerDevPixel == appUnitsPerDevPixel);
    LayoutDeviceRect bounds =
        LayoutDeviceRect::FromAppUnits(mGroupBounds, appUnitsPerDevPixel);
    LayoutDeviceIntPoint offset = RoundedToInt(bounds.TopLeft());
    GP("\n");
    GP("CGC offset %d %d\n", offset.x, offset.y);
    GP("imageRect %d %d %d %d\n", mImageBounds.x, mImageBounds.y,
       mImageBounds.width, mImageBounds.height);
    /*if (aItem->IsReused() && aData->mGeometry) {
      return;
    }*/
    aData->mInvalidRegion = false;

    GP("pre mInvalidRect: %s %p-%d - inv: %d %d %d %d\n", aItem->Name(),
       aItem->Frame(), aItem->GetPerFrameKey(), mInvalidRect.x, mInvalidRect.y,
       mInvalidRect.width, mInvalidRect.height);
    if (!aData->mGeometry) {
      // This item is being added for the first time, invalidate its entire
      // area.
      UniquePtr<nsDisplayItemGeometry> geometry(
          aItem->AllocateGeometry(aBuilder));
      combined = clip.ApplyNonRoundedIntersection(
          geometry->ComputeInvalidationRegion());
      aData->mGeometry = std::move(geometry);
      nsRect bounds = combined.GetBounds();

      IntRect transformedRect =
          ToDeviceSpace(combined.GetBounds(), aMatrix, appUnitsPerDevPixel,
                        mLayerBounds.TopLeft());
      aData->mRect = transformedRect.Intersect(mImageBounds);
      GP("CGC %s %d %d %d %d\n", aItem->Name(), bounds.x, bounds.y,
         bounds.width, bounds.height);
      GP("%d %d,  %f %f\n", mLayerBounds.TopLeft().x, mLayerBounds.TopLeft().y,
         aMatrix._11, aMatrix._22);
      GP("mRect %d %d %d %d\n", aData->mRect.x, aData->mRect.y,
         aData->mRect.width, aData->mRect.height);
      InvalidateRect(aData->mRect);
      aData->mInvalid = true;
    } else if (aData->mInvalid ||
               /* XXX: handle image load invalidation */ (
                   aItem->IsInvalid(invalid) && invalid.IsEmpty())) {
      MOZ_RELEASE_ASSERT(mLayerBounds.TopLeft() == aData->mGroupOffset);
      UniquePtr<nsDisplayItemGeometry> geometry(
          aItem->AllocateGeometry(aBuilder));
      /* Instead of doing this dance, let's just invalidate the old rect and the
       * new rect.
      combined =
        aData->mClip.ApplyNonRoundedIntersection(
          aData->mGeometry->ComputeInvalidationRegion());
      combined.MoveBy(shift);
      combined.Or(combined,
                  clip.ApplyNonRoundedIntersection(
                    geometry->ComputeInvalidationRegion()));
      aData->mGeometry = std::move(geometry);
      */
      combined = clip.ApplyNonRoundedIntersection(
          geometry->ComputeInvalidationRegion());
      aData->mGeometry = std::move(geometry);

      GP("matrix: %f %f\n", aMatrix._31, aMatrix._32);
      GP("frame invalid invalidate: %s\n", aItem->Name());
      GP("old rect: %d %d %d %d\n", aData->mRect.x, aData->mRect.y,
         aData->mRect.width, aData->mRect.height);
      InvalidateRect(aData->mRect.Intersect(mImageBounds));
      // We want to snap to outside pixels. When should we multiply by the
      // matrix?
      // XXX: TransformBounds is expensive. We should avoid doing it if we have
      // no transform
      IntRect transformedRect =
          ToDeviceSpace(combined.GetBounds(), aMatrix, appUnitsPerDevPixel,
                        mLayerBounds.TopLeft());
      aData->mRect = transformedRect.Intersect(mImageBounds);
      InvalidateRect(aData->mRect);
      GP("new rect: %d %d %d %d\n", aData->mRect.x, aData->mRect.y,
         aData->mRect.width, aData->mRect.height);
      aData->mInvalid = true;
    } else {
      MOZ_RELEASE_ASSERT(mLayerBounds.TopLeft() == aData->mGroupOffset);
      GP("else invalidate: %s\n", aItem->Name());
      // this includes situations like reflow changing the position
      aItem->ComputeInvalidationRegion(aBuilder, aData->mGeometry.get(),
                                       &combined);
      if (!combined.IsEmpty()) {
        // There might be no point in doing this elaborate tracking here to get
        // smaller areas
        InvalidateRect(aData->mRect.Intersect(
            mImageBounds));  // invalidate the old area -- in theory combined
                             // should take care of this
        UniquePtr<nsDisplayItemGeometry> geometry(
            aItem->AllocateGeometry(aBuilder));
        // invalidate the invalidated area.

        aData->mGeometry = std::move(geometry);

        combined = clip.ApplyNonRoundedIntersection(
            aData->mGeometry->ComputeInvalidationRegion());
        IntRect transformedRect =
            ToDeviceSpace(combined.GetBounds(), aMatrix, appUnitsPerDevPixel,
                          mLayerBounds.TopLeft());
        aData->mRect = transformedRect.Intersect(mImageBounds);
        InvalidateRect(aData->mRect);

        // CGC invariant broken
        if (!mInvalidRect.Contains(aData->mRect)) {
          gfxCriticalError()
              << "CGC-"
              << "-" << aData->mRect.x << "-" << aData->mRect.y << "-"
              << aData->mRect.width << "-" << aData->mRect.height << "-ib";
        }

        aData->mInvalid = true;
        aData->mInvalidRegion = true;
      } else {
        if (aData->mClip != clip) {
          UniquePtr<nsDisplayItemGeometry> geometry(
              aItem->AllocateGeometry(aBuilder));
          if (!IsContainerLayerItem(aItem)) {
            // the bounds of layer items can change on us without
            // ComputeInvalidationRegion returning any change. Other items
            // shouldn't have any hidden geometry change.
            MOZ_RELEASE_ASSERT(
                geometry->mBounds.IsEqualEdges(aData->mGeometry->mBounds));
          } else {
            aData->mGeometry = std::move(geometry);
          }
          combined = clip.ApplyNonRoundedIntersection(
              aData->mGeometry->ComputeInvalidationRegion());
          IntRect transformedRect =
              ToDeviceSpace(combined.GetBounds(), aMatrix, appUnitsPerDevPixel,
                            mLayerBounds.TopLeft());
          InvalidateRect(aData->mRect.Intersect(mImageBounds));
          aData->mRect = transformedRect.Intersect(mImageBounds);
          InvalidateRect(aData->mRect);

          GP("ClipChange: %s %d %d %d %d\n", aItem->Name(), aData->mRect.x,
             aData->mRect.y, aData->mRect.XMost(), aData->mRect.YMost());

        } else if (!aMatrix.ExactlyEquals(aData->mMatrix)) {
          // We haven't detected any changes so far. Unfortunately we don't
          // currently have a good way of checking if the transform has changed
          // so we just store it and see if it see if it has changed.
          // If we want this to go faster, we can probably put a flag on the
          // frame using the style sytem UpdateTransformLayer hint and check for
          // that.

          UniquePtr<nsDisplayItemGeometry> geometry(
              aItem->AllocateGeometry(aBuilder));
          if (!IsContainerLayerItem(aItem)) {
            // the bounds of layer items can change on us
            // other items shouldn't
            MOZ_RELEASE_ASSERT(
                geometry->mBounds.IsEqualEdges(aData->mGeometry->mBounds));
          } else {
            aData->mGeometry = std::move(geometry);
          }
          combined = clip.ApplyNonRoundedIntersection(
              aData->mGeometry->ComputeInvalidationRegion());
          IntRect transformedRect =
              ToDeviceSpace(combined.GetBounds(), aMatrix, appUnitsPerDevPixel,
                            mLayerBounds.TopLeft());
          InvalidateRect(aData->mRect.Intersect(mImageBounds));
          aData->mRect = transformedRect.Intersect(mImageBounds);
          InvalidateRect(aData->mRect);

          GP("TransformChange: %s %d %d %d %d\n", aItem->Name(), aData->mRect.x,
             aData->mRect.y, aData->mRect.XMost(), aData->mRect.YMost());
        } else if (IsContainerLayerItem(aItem)) {
          UniquePtr<nsDisplayItemGeometry> geometry(
              aItem->AllocateGeometry(aBuilder));
          // we need to catch bounds changes of containers so that we continue
          // to have the correct bounds rects in the recording
          if (UpdateContainerLayerPropertiesAndDetectChange(aItem, aData,
                                                            *geometry)) {
            combined = clip.ApplyNonRoundedIntersection(
                geometry->ComputeInvalidationRegion());
            aData->mGeometry = std::move(geometry);
            IntRect transformedRect =
                ToDeviceSpace(combined.GetBounds(), aMatrix,
                              appUnitsPerDevPixel, mLayerBounds.TopLeft());
            InvalidateRect(aData->mRect.Intersect(mImageBounds));
            aData->mRect = transformedRect.Intersect(mImageBounds);
            InvalidateRect(aData->mRect);
            GP("UpdateContainerLayerPropertiesAndDetectChange change\n");
          } else if (!aData->mImageRect.IsEqualEdges(mImageBounds)) {
            // Make sure we update mRect for mImageBounds changes
            combined = clip.ApplyNonRoundedIntersection(
                geometry->ComputeInvalidationRegion());
            IntRect transformedRect =
                ToDeviceSpace(combined.GetBounds(), aMatrix,
                              appUnitsPerDevPixel, mLayerBounds.TopLeft());
            // The invalid rect should contain the old rect and the new rect
            // but may not because the parent may have been removed.
            InvalidateRect(aData->mRect);
            aData->mRect = transformedRect.Intersect(mImageBounds);
            InvalidateRect(aData->mRect);
            GP("ContainerLayer image rect bounds change\n");
          } else {
            // XXX: this code can eventually be deleted/made debug only
            combined = clip.ApplyNonRoundedIntersection(
                geometry->ComputeInvalidationRegion());
            IntRect transformedRect =
                ToDeviceSpace(combined.GetBounds(), aMatrix,
                              appUnitsPerDevPixel, mLayerBounds.TopLeft());
            auto rect = transformedRect.Intersect(mImageBounds);
            GP("Layer NoChange: %s %d %d %d %d\n", aItem->Name(),
               aData->mRect.x, aData->mRect.y, aData->mRect.XMost(),
               aData->mRect.YMost());
            MOZ_RELEASE_ASSERT(rect.IsEqualEdges(aData->mRect));
          }
        } else if (!aData->mImageRect.IsEqualEdges(mImageBounds)) {
          // Make sure we update mRect for mImageBounds changes
          UniquePtr<nsDisplayItemGeometry> geometry(
              aItem->AllocateGeometry(aBuilder));
          combined = clip.ApplyNonRoundedIntersection(
              geometry->ComputeInvalidationRegion());
          IntRect transformedRect =
              ToDeviceSpace(combined.GetBounds(), aMatrix, appUnitsPerDevPixel,
                            mLayerBounds.TopLeft());
          // The invalid rect should contain the old rect and the new rect
          // but may not because the parent may have been removed.
          InvalidateRect(aData->mRect);
          aData->mRect = transformedRect.Intersect(mImageBounds);
          InvalidateRect(aData->mRect);
          GP("image rect bounds change\n");
        } else {
          // XXX: this code can eventually be deleted/made debug only
          UniquePtr<nsDisplayItemGeometry> geometry(
              aItem->AllocateGeometry(aBuilder));
          combined = clip.ApplyNonRoundedIntersection(
              geometry->ComputeInvalidationRegion());
          IntRect transformedRect =
              ToDeviceSpace(combined.GetBounds(), aMatrix, appUnitsPerDevPixel,
                            mLayerBounds.TopLeft());
          auto rect = transformedRect.Intersect(mImageBounds);
          GP("NoChange: %s %d %d %d %d\n", aItem->Name(), aData->mRect.x,
             aData->mRect.y, aData->mRect.XMost(), aData->mRect.YMost());
          MOZ_RELEASE_ASSERT(rect.IsEqualEdges(aData->mRect));
        }
      }
    }
    aData->mClip = clip;
    aData->mMatrix = aMatrix;
    aData->mGroupOffset = mLayerBounds.TopLeft();
    aData->mImageRect = mImageBounds;
    GP("post mInvalidRect: %d %d %d %d\n", mInvalidRect.x, mInvalidRect.y,
       mInvalidRect.width, mInvalidRect.height);
  }

  void EndGroup(WebRenderLayerManager* aWrManager,
                nsDisplayListBuilder* aDisplayListBuilder,
                wr::DisplayListBuilder& aBuilder,
                wr::IpcResourceUpdateQueue& aResources, Grouper* aGrouper,
                nsDisplayItem* aStartItem, nsDisplayItem* aEndItem) {
    GP("\n\n");
    GP("Begin EndGroup\n");

    // Invalidate any unused items
    GP("mDisplayItems\n");
    for (auto iter = mDisplayItems.Iter(); !iter.Done(); iter.Next()) {
      BlobItemData* data = iter.Get()->GetKey();
      GP("  : %p-%d\n", data->mFrame, data->mDisplayItemKey);
      if (!data->mUsed) {
        GP("Invalidate unused: %p-%d\n", data->mFrame, data->mDisplayItemKey);
        InvalidateRect(data->mRect);
        iter.Remove();
        delete data;
      } else {
        data->mUsed = false;
      }
    }

    // Round the bounds out to leave space for unsnapped content
    LayoutDeviceToLayerScale2D scale(mScale.width, mScale.height);
    LayerIntRect layerBounds = mLayerBounds;
    IntSize dtSize = layerBounds.Size().ToUnknownSize();
    LayoutDeviceRect bounds =
        (LayerRect(layerBounds) - mResidualOffset) / scale;

    if (mInvalidRect.IsEmpty()) {
      GP("Not repainting group because it's empty\n");
      GP("End EndGroup\n");
      if (mKey) {
        aResources.SetBlobImageVisibleArea(
            mKey.value(),
            ViewAs<ImagePixel>(mPaintRect,
                               PixelCastJustification::LayerIsImage));
        PushImage(aBuilder, bounds);
      }
      return;
    }

    gfx::SurfaceFormat format = gfx::SurfaceFormat::B8G8R8A8;
    std::vector<RefPtr<ScaledFont>> fonts;
    RefPtr<WebRenderDrawEventRecorder> recorder =
        MakeAndAddRef<WebRenderDrawEventRecorder>(
            [&](MemStream& aStream,
                std::vector<RefPtr<ScaledFont>>& aScaledFonts) {
              size_t count = aScaledFonts.size();
              aStream.write((const char*)&count, sizeof(count));
              for (auto& scaled : aScaledFonts) {
                BlobFont font = {
                    aWrManager->WrBridge()->GetFontKeyForScaledFont(
                        scaled, &aResources),
                    scaled};
                aStream.write((const char*)&font, sizeof(font));
              }
              fonts = std::move(aScaledFonts);
            });

    RefPtr<gfx::DrawTarget> dummyDt = gfx::Factory::CreateDrawTarget(
        gfx::BackendType::SKIA, gfx::IntSize(1, 1), format);

    RefPtr<gfx::DrawTarget> dt =
        gfx::Factory::CreateRecordingDrawTarget(recorder, dummyDt, dtSize);
    // Setup the gfxContext
    RefPtr<gfxContext> context = gfxContext::CreateOrNull(dt);
    GP("ctx-offset %f %f\n", bounds.x, bounds.y);
    context->SetMatrix(Matrix::Scaling(mScale.width, mScale.height)
                           .PreTranslate(-bounds.x, -bounds.y));

    GP("mInvalidRect: %d %d %d %d\n", mInvalidRect.x, mInvalidRect.y,
       mInvalidRect.width, mInvalidRect.height);

    bool empty = aStartItem == aEndItem;
    if (empty) {
      ClearImageKey(aWrManager, true);
      return;
    }

    PaintItemRange(aGrouper, aStartItem, aEndItem, context, recorder);

    // XXX: set this correctly perhaps using
    // aItem->GetOpaqueRegion(aDisplayListBuilder, &snapped).
    //   Contains(paintBounds);?
    wr::OpacityType opacity = wr::OpacityType::HasAlphaChannel;

    TakeExternalSurfaces(recorder, mExternalSurfaces, aWrManager, aResources);
    bool hasItems = recorder->Finish();
    GP("%d Finish\n", hasItems);
    Range<uint8_t> bytes((uint8_t*)recorder->mOutputStream.mData,
                         recorder->mOutputStream.mLength);
    if (!mKey) {
      if (!hasItems)  // we don't want to send a new image that doesn't have any
                      // items in it
        return;
      wr::BlobImageKey key =
          wr::BlobImageKey{aWrManager->WrBridge()->GetNextImageKey()};
      GP("No previous key making new one %d\n", key._0.mHandle);
      wr::ImageDescriptor descriptor(dtSize, 0, dt->GetFormat(), opacity);
      MOZ_RELEASE_ASSERT(bytes.length() > sizeof(size_t));
      if (!aResources.AddBlobImage(key, descriptor, bytes)) {
        return;
      }
      mKey = Some(key);
    } else {
      wr::ImageDescriptor descriptor(dtSize, 0, dt->GetFormat(), opacity);
      auto bottomRight = mInvalidRect.BottomRight();
      GP("check invalid %d %d - %d %d\n", bottomRight.x, bottomRight.y,
         dtSize.width, dtSize.height);
      MOZ_RELEASE_ASSERT(bottomRight.x <= dtSize.width &&
                         bottomRight.y <= dtSize.height);
      GP("Update Blob %d %d %d %d\n", mInvalidRect.x, mInvalidRect.y,
         mInvalidRect.width, mInvalidRect.height);
      if (!aResources.UpdateBlobImage(mKey.value(), descriptor, bytes,
                                      ViewAs<ImagePixel>(mInvalidRect))) {
        return;
      }
    }
    mFonts = std::move(fonts);
    mInvalidRect.SetEmpty();
    aResources.SetBlobImageVisibleArea(
        mKey.value(),
        ViewAs<ImagePixel>(mPaintRect, PixelCastJustification::LayerIsImage));
    PushImage(aBuilder, bounds);
    GP("End EndGroup\n\n");
  }

  void PushImage(wr::DisplayListBuilder& aBuilder,
                 const LayoutDeviceRect& bounds) {
    wr::LayoutRect dest = wr::ToLayoutRect(bounds);
    GP("PushImage: %f %f %f %f\n", dest.origin.x, dest.origin.y,
       dest.size.width, dest.size.height);
    gfx::SamplingFilter sampleFilter = gfx::SamplingFilter::
        LINEAR;  // nsLayoutUtils::GetSamplingFilterForFrame(aItem->Frame());
    bool backfaceHidden = false;

    // Emit a dispatch-to-content hit test region covering this area
    CompositorHitTestInfo hitInfo(CompositorHitTestFlags::eVisibleToHitTest,
                                  CompositorHitTestFlags::eDispatchToContent);

    // XXX - clipping the item against the paint rect breaks some content.
    // cf. Bug 1455422.
    // wr::LayoutRect clip = wr::ToLayoutRect(bounds.Intersect(mPaintRect));

    aBuilder.SetHitTestInfo(mScrollId, hitInfo);
    aBuilder.PushImage(dest, dest, !backfaceHidden,
                       wr::ToImageRendering(sampleFilter),
                       wr::AsImageKey(mKey.value()));
    aBuilder.ClearHitTestInfo();
  }

  void PaintItemRange(Grouper* aGrouper, nsDisplayItem* aStartItem,
                      nsDisplayItem* aEndItem, gfxContext* aContext,
                      WebRenderDrawEventRecorder* aRecorder) {
    LayerIntSize size = mLayerBounds.Size();
    for (nsDisplayItem* item = aStartItem; item != aEndItem;
         item = item->GetAbove()) {
      IntRect bounds = ItemBounds(item);
      auto bottomRight = bounds.BottomRight();

      GP("Trying %s %p-%d %d %d %d %d\n", item->Name(), item->Frame(),
         item->GetPerFrameKey(), bounds.x, bounds.y, bounds.XMost(),
         bounds.YMost());
      GP("paint check invalid %d %d - %d %d\n", bottomRight.x, bottomRight.y,
         size.width, size.height);
      // skip empty items
      if (bounds.IsEmpty()) {
        continue;
      }

      bool dirty = true;
      if (!mInvalidRect.Contains(bounds)) {
        GP("Passing\n");
        dirty = false;
      }

      if (mInvalidRect.Contains(bounds)) {
        GP("Wholely contained\n");
      } else {
        BlobItemData* data = GetBlobItemData(item);
        if (data->mInvalid) {
          if (item->GetType() == DisplayItemType::TYPE_TRANSFORM) {
            nsDisplayTransform* transformItem =
                static_cast<nsDisplayTransform*>(item);
            const Matrix4x4Flagged& t = transformItem->GetTransform();
            Matrix t2d;
            bool is2D = t.Is2D(&t2d);
            gfxCriticalError()
                << "DIT-" << is2D << "-r-" << data->mInvalidRegion << "-"
                << bounds.x << "-" << bounds.y << "-" << bounds.width << "-"
                << bounds.height << "," << mInvalidRect.x << "-"
                << mInvalidRect.y << "-" << mInvalidRect.width << "-"
                << mInvalidRect.height << "-sbi";
          } else {
            gfxCriticalError() << "DisplayItem" << item->Name() << "-region-"
                               << data->mInvalidRegion << "-should be invalid";
          }
        }
        // if the item is invalid it needs to be fully contained
        MOZ_RELEASE_ASSERT(!data->mInvalid);
      }

      nsDisplayList* children = item->GetChildren();
      if (children) {
        GP("doing children in EndGroup\n");
        aGrouper->PaintContainerItem(this, item, bounds, children, aContext,
                                     aRecorder);
      } else {
        // Hit test items don't have anything to paint so skip them. Ideally we
        // would drop these items earlier...
        if (dirty &&
            item->GetType() != DisplayItemType::TYPE_COMPOSITOR_HITTEST_INFO) {
          // What should the clip settting strategy be? We can set the full clip
          // everytime. this is probably easiest for now. An alternative would
          // be to put the push and the pop into separate items and let
          // invalidation handle it that way.
          DisplayItemClip currentClip = item->GetClip();

          if (currentClip.HasClip()) {
            aContext->Save();
            currentClip.ApplyTo(aContext, aGrouper->mAppUnitsPerDevPixel);
          }
          aContext->NewPath();
          GP("painting %s %p-%d\n", item->Name(), item->Frame(),
             item->GetPerFrameKey());
          if (aGrouper->mDisplayListBuilder->IsPaintingToWindow()) {
            item->Frame()->AddStateBits(NS_FRAME_PAINTED_THEBES);
          }
          item->Paint(aGrouper->mDisplayListBuilder, aContext);
          if (currentClip.HasClip()) {
            aContext->Restore();
          }
        }
        aContext->GetDrawTarget()->FlushItem(bounds);
      }
    }
  }

  ~DIGroup() {
    GP("Group destruct\n");
    for (auto iter = mDisplayItems.Iter(); !iter.Done(); iter.Next()) {
      BlobItemData* data = iter.Get()->GetKey();
      GP("Deleting %p-%d\n", data->mFrame, data->mDisplayItemKey);
      iter.Remove();
      delete data;
    }
  }
};

// If we have an item we need to make sure it matches the current group
// otherwise it means the item switched groups and we need to invalidate
// it and recreate the data.
static BlobItemData* GetBlobItemDataForGroup(nsDisplayItem* aItem,
                                             DIGroup* aGroup) {
  BlobItemData* data = GetBlobItemData(aItem);
  if (data) {
    MOZ_RELEASE_ASSERT(data->mGroup->mDisplayItems.Contains(data));
    if (data->mGroup != aGroup) {
      GP("group don't match %p %p\n", data->mGroup, aGroup);
      data->ClearFrame();
      // the item is for another group
      // it should be cleared out as being unused at the end of this paint
      data = nullptr;
    }
  }
  if (!data) {
    GP("Allocating blob data\n");
    data = new BlobItemData(aGroup, aItem);
    aGroup->mDisplayItems.PutEntry(data);
  }
  data->mUsed = true;
  return data;
}

void Grouper::PaintContainerItem(DIGroup* aGroup, nsDisplayItem* aItem,
                                 const IntRect& aItemBounds,
                                 nsDisplayList* aChildren, gfxContext* aContext,
                                 WebRenderDrawEventRecorder* aRecorder) {
  mItemStack.push_back(aItem);
  switch (aItem->GetType()) {
    case DisplayItemType::TYPE_TRANSFORM: {
      DisplayItemClip currentClip = aItem->GetClip();

      gfx::Matrix matrix;
      if (currentClip.HasClip()) {
        aContext->Save();
        currentClip.ApplyTo(aContext, this->mAppUnitsPerDevPixel);
        aContext->GetDrawTarget()->FlushItem(aItemBounds);
      } else {
        matrix = aContext->CurrentMatrix();
      }

      auto transformItem = static_cast<nsDisplayTransform*>(aItem);
      Matrix4x4Flagged trans = transformItem->GetTransform();
      Matrix trans2d;
      if (!trans.Is2D(&trans2d)) {
        // We don't currently support doing invalidation inside 3d transforms.
        // For now just paint it as a single item.
        BlobItemData* data = GetBlobItemDataForGroup(aItem, aGroup);
        if (data->mLayerManager->GetRoot()) {
          data->mLayerManager->BeginTransaction();
          data->mLayerManager->EndTransaction(
              FrameLayerBuilder::DrawPaintedLayer, mDisplayListBuilder);
          aContext->GetDrawTarget()->FlushItem(aItemBounds);
        }
      } else {
        aContext->Multiply(ThebesMatrix(trans2d));
        aGroup->PaintItemRange(this, aChildren->GetBottom(), nullptr, aContext,
                               aRecorder);

        if (currentClip.HasClip()) {
          aContext->Restore();
          aContext->GetDrawTarget()->FlushItem(aItemBounds);
        } else {
          aContext->SetMatrix(matrix);
        }
      }
      break;
    }
    case DisplayItemType::TYPE_OPACITY: {
      auto opacityItem = static_cast<nsDisplayOpacity*>(aItem);
      float opacity = opacityItem->GetOpacity();
      if (opacity == 0.0f) {
        return;
      }

      aContext->GetDrawTarget()->PushLayer(false, opacityItem->GetOpacity(),
                                           nullptr, mozilla::gfx::Matrix(),
                                           aItemBounds);
      GP("beginGroup %s %p-%d\n", aItem->Name(), aItem->Frame(),
         aItem->GetPerFrameKey());
      aContext->GetDrawTarget()->FlushItem(aItemBounds);
      aGroup->PaintItemRange(this, aChildren->GetBottom(), nullptr, aContext,
                             aRecorder);
      aContext->GetDrawTarget()->PopLayer();
      GP("endGroup %s %p-%d\n", aItem->Name(), aItem->Frame(),
         aItem->GetPerFrameKey());
      aContext->GetDrawTarget()->FlushItem(aItemBounds);
      break;
    }
    case DisplayItemType::TYPE_BLEND_MODE: {
      auto blendItem = static_cast<nsDisplayBlendMode*>(aItem);
      auto blendMode = blendItem->BlendMode();
      aContext->GetDrawTarget()->PushLayerWithBlend(
          false, 1.0, nullptr, mozilla::gfx::Matrix(), aItemBounds, false,
          blendMode);
      GP("beginGroup %s %p-%d\n", aItem->Name(), aItem->Frame(),
         aItem->GetPerFrameKey());
      aContext->GetDrawTarget()->FlushItem(aItemBounds);
      aGroup->PaintItemRange(this, aChildren->GetBottom(), nullptr, aContext,
                             aRecorder);
      aContext->GetDrawTarget()->PopLayer();
      GP("endGroup %s %p-%d\n", aItem->Name(), aItem->Frame(),
         aItem->GetPerFrameKey());
      aContext->GetDrawTarget()->FlushItem(aItemBounds);
      break;
    }
    case DisplayItemType::TYPE_MASK: {
      GP("Paint Mask\n");
      auto maskItem = static_cast<nsDisplayMasksAndClipPaths*>(aItem);
      maskItem->SetPaintRect(maskItem->GetClippedBounds(mDisplayListBuilder));
      if (maskItem->IsValidMask()) {
        maskItem->PaintWithContentsPaintCallback(
            mDisplayListBuilder, aContext, [&] {
              GP("beginGroup %s %p-%d\n", aItem->Name(), aItem->Frame(),
                 aItem->GetPerFrameKey());
              aContext->GetDrawTarget()->FlushItem(aItemBounds);
              aGroup->PaintItemRange(this, aChildren->GetBottom(), nullptr,
                                     aContext, aRecorder);
              GP("endGroup %s %p-%d\n", aItem->Name(), aItem->Frame(),
                 aItem->GetPerFrameKey());
            });
        aContext->GetDrawTarget()->FlushItem(aItemBounds);
      }
      break;
    }
    case DisplayItemType::TYPE_FILTER: {
      GP("Paint Filter\n");
      // We don't currently support doing invalidation inside nsDisplayFilters
      // for now just paint it as a single item
      BlobItemData* data = GetBlobItemDataForGroup(aItem, aGroup);
      if (data->mLayerManager->GetRoot()) {
        data->mLayerManager->BeginTransaction();
        static_cast<nsDisplayFilters*>(aItem)->PaintAsLayer(
            mDisplayListBuilder, aContext, data->mLayerManager);
        if (data->mLayerManager->InTransaction()) {
          data->mLayerManager->AbortTransaction();
        }
        aContext->GetDrawTarget()->FlushItem(aItemBounds);
      }
      break;
    }

    default:
      aGroup->PaintItemRange(this, aChildren->GetBottom(), nullptr, aContext,
                             aRecorder);
      break;
  }
}

class WebRenderGroupData : public WebRenderUserData {
 public:
  explicit WebRenderGroupData(WebRenderLayerManager* aWRManager,
                              nsDisplayItem* aItem);
  virtual ~WebRenderGroupData();

  virtual WebRenderGroupData* AsGroupData() override { return this; }
  virtual UserDataType GetType() override { return UserDataType::eGroup; }
  static UserDataType Type() { return UserDataType::eGroup; }

  DIGroup mSubGroup;
  DIGroup mFollowingGroup;
};

static bool IsItemProbablyActive(nsDisplayItem* aItem,
                                 nsDisplayListBuilder* aDisplayListBuilder);

static bool HasActiveChildren(const nsDisplayList& aList,
                              nsDisplayListBuilder* aDisplayListBuilder) {
  for (nsDisplayItem* i = aList.GetBottom(); i; i = i->GetAbove()) {
    if (IsItemProbablyActive(i, aDisplayListBuilder)) {
      return true;
    }
  }
  return false;
}

// This function decides whether we want to treat this item as "active", which
// means that it's a container item which we will turn into a WebRender
// StackingContext, or whether we treat it as "inactive" and include it inside
// the parent blob image.
//
// We can't easily use GetLayerState because it wants a bunch of layers related
// information.
static bool IsItemProbablyActive(nsDisplayItem* aItem,
                                 nsDisplayListBuilder* aDisplayListBuilder) {
  switch (aItem->GetType()) {
    case DisplayItemType::TYPE_TRANSFORM: {
      nsDisplayTransform* transformItem =
          static_cast<nsDisplayTransform*>(aItem);
      const Matrix4x4Flagged& t = transformItem->GetTransform();
      Matrix t2d;
      bool is2D = t.Is2D(&t2d);
      GP("active: %d\n", transformItem->MayBeAnimated(aDisplayListBuilder));
      return transformItem->MayBeAnimated(aDisplayListBuilder) || !is2D ||
             HasActiveChildren(*transformItem->GetChildren(),
                               aDisplayListBuilder);
    }
    case DisplayItemType::TYPE_OPACITY: {
      nsDisplayOpacity* opacityItem = static_cast<nsDisplayOpacity*>(aItem);
      bool active = opacityItem->NeedsActiveLayer(aDisplayListBuilder,
                                                  opacityItem->Frame());
      GP("active: %d\n", active);
      return active || HasActiveChildren(*opacityItem->GetChildren(),
                                         aDisplayListBuilder);
    }
    case DisplayItemType::TYPE_FOREIGN_OBJECT: {
      return true;
    }
    case DisplayItemType::TYPE_WRAP_LIST:
    case DisplayItemType::TYPE_PERSPECTIVE: {
      if (aItem->GetChildren()) {
        return HasActiveChildren(*aItem->GetChildren(), aDisplayListBuilder);
      }
      return false;
    }
    default:
      // TODO: handle other items?
      return false;
  }
}

// This does a pass over the display lists and will join the display items
// into groups as well as paint them
void Grouper::ConstructGroups(nsDisplayListBuilder* aDisplayListBuilder,
                              WebRenderCommandBuilder* aCommandBuilder,
                              wr::DisplayListBuilder& aBuilder,
                              wr::IpcResourceUpdateQueue& aResources,
                              DIGroup* aGroup, nsDisplayList* aList,
                              const StackingContextHelper& aSc) {
  DIGroup* currentGroup = aGroup;

  nsDisplayItem* item = aList->GetBottom();
  nsDisplayItem* startOfCurrentGroup = item;
  while (item) {
    if (IsItemProbablyActive(item, mDisplayListBuilder)) {
      currentGroup->EndGroup(aCommandBuilder->mManager, aDisplayListBuilder,
                             aBuilder, aResources, this, startOfCurrentGroup,
                             item);
      mClipManager.BeginItem(item, aSc);
      sIndent++;
      // Note: this call to CreateWebRenderCommands can recurse back into
      // this function.
      bool createdWRCommands = item->CreateWebRenderCommands(
          aBuilder, aResources, aSc, aCommandBuilder->mManager,
          mDisplayListBuilder);
      sIndent--;
      MOZ_RELEASE_ASSERT(createdWRCommands,
                         "active transforms should always succeed at creating "
                         "WebRender commands");

      RefPtr<WebRenderGroupData> groupData =
          aCommandBuilder->CreateOrRecycleWebRenderUserData<WebRenderGroupData>(
              item);

      // Initialize groupData->mFollowingGroup
      // TODO: compute the group bounds post-grouping, so that they can be
      // tighter for just the sublist that made it into this group.
      // We want to ensure the tight bounds are still clipped by area
      // that we're building the display list for.
      if (!groupData->mFollowingGroup.mGroupBounds.IsEqualEdges(
              currentGroup->mGroupBounds) ||
          groupData->mFollowingGroup.mScale != currentGroup->mScale ||
          groupData->mFollowingGroup.mAppUnitsPerDevPixel !=
              currentGroup->mAppUnitsPerDevPixel ||
          groupData->mFollowingGroup.mResidualOffset !=
              currentGroup->mResidualOffset) {
        if (groupData->mFollowingGroup.mAppUnitsPerDevPixel !=
            currentGroup->mAppUnitsPerDevPixel) {
          GP("app unit change following: %d %d\n",
             groupData->mFollowingGroup.mAppUnitsPerDevPixel,
             currentGroup->mAppUnitsPerDevPixel);
        }
        // The group changed size
        GP("Inner group size change\n");
        groupData->mFollowingGroup.ClearItems();
        groupData->mFollowingGroup.ClearImageKey(aCommandBuilder->mManager);
      }
      groupData->mFollowingGroup.mGroupBounds = currentGroup->mGroupBounds;
      groupData->mFollowingGroup.mAppUnitsPerDevPixel =
          currentGroup->mAppUnitsPerDevPixel;
      groupData->mFollowingGroup.mLayerBounds = currentGroup->mLayerBounds;
      groupData->mFollowingGroup.mImageBounds = currentGroup->mImageBounds;
      groupData->mFollowingGroup.mScale = currentGroup->mScale;
      groupData->mFollowingGroup.mResidualOffset =
          currentGroup->mResidualOffset;
      groupData->mFollowingGroup.mPaintRect = currentGroup->mPaintRect;

      currentGroup = &groupData->mFollowingGroup;

      startOfCurrentGroup = item->GetAbove();
    } else {  // inactive item
      ConstructItemInsideInactive(aCommandBuilder, aBuilder, aResources,
                                  currentGroup, item, aSc);
    }

    item = item->GetAbove();
  }

  currentGroup->EndGroup(aCommandBuilder->mManager, aDisplayListBuilder,
                         aBuilder, aResources, this, startOfCurrentGroup,
                         nullptr);
}

// This does a pass over the display lists and will join the display items
// into a single group.
void Grouper::ConstructGroupInsideInactive(
    WebRenderCommandBuilder* aCommandBuilder, wr::DisplayListBuilder& aBuilder,
    wr::IpcResourceUpdateQueue& aResources, DIGroup* aGroup,
    nsDisplayList* aList, const StackingContextHelper& aSc) {
  nsDisplayItem* item = aList->GetBottom();
  while (item) {
    ConstructItemInsideInactive(aCommandBuilder, aBuilder, aResources, aGroup,
                                item, aSc);
    item = item->GetAbove();
  }
}

bool BuildLayer(nsDisplayItem* aItem, BlobItemData* aData,
                nsDisplayListBuilder* aDisplayListBuilder,
                const gfx::Size& aScale);

void Grouper::ConstructItemInsideInactive(
    WebRenderCommandBuilder* aCommandBuilder, wr::DisplayListBuilder& aBuilder,
    wr::IpcResourceUpdateQueue& aResources, DIGroup* aGroup,
    nsDisplayItem* aItem, const StackingContextHelper& aSc) {
  nsDisplayList* children = aItem->GetChildren();
  BlobItemData* data = GetBlobItemDataForGroup(aItem, aGroup);

  /* mInvalid unfortunately persists across paints. Clear it so that if we don't
   * set it to 'true' we ensure that we're not using the value from the last
   * time that we painted */
  data->mInvalid = false;

  // we compute the geometry change here because we have the transform around
  // still
  aGroup->ComputeGeometryChange(aItem, data, mTransform, mDisplayListBuilder);

  // Temporarily restrict the image bounds to the bounds of the container so
  // that clipped children within the container know about the clip.
  IntRect oldImageBounds = aGroup->mImageBounds;
  aGroup->mImageBounds = aGroup->mImageBounds.Intersect(data->mRect);

  if (aItem->GetType() == DisplayItemType::TYPE_FILTER) {
    gfx::Size scale(1, 1);
    // If ComputeDifferences finds any change, we invalidate the entire
    // container item. This is needed because blob merging requires the entire
    // item to be within the invalid region.
    if (BuildLayer(aItem, data, mDisplayListBuilder, scale)) {
      data->mInvalid = true;
      aGroup->InvalidateRect(data->mRect);
    }
  } else if (aItem->GetType() == DisplayItemType::TYPE_TRANSFORM) {
    nsDisplayTransform* transformItem = static_cast<nsDisplayTransform*>(aItem);
    const Matrix4x4Flagged& t = transformItem->GetTransform();
    Matrix t2d;
    bool is2D = t.Is2D(&t2d);
    if (!is2D) {
      // We'll use BasicLayerManager to handle 3d transforms.
      gfx::Size scale(1, 1);
      // If ComputeDifferences finds any change, we invalidate the entire
      // container item. This is needed because blob merging requires the entire
      // item to be within the invalid region.
      if (BuildLayer(aItem, data, mDisplayListBuilder, scale)) {
        data->mInvalid = true;
        aGroup->InvalidateRect(data->mRect);
      }
    } else {
      Matrix m = mTransform;

      GP("t2d: %f %f\n", t2d._31, t2d._32);
      mTransform.PreMultiply(t2d);
      GP("mTransform: %f %f\n", mTransform._31, mTransform._32);
      ConstructGroupInsideInactive(aCommandBuilder, aBuilder, aResources,
                                   aGroup, children, aSc);

      mTransform = m;
    }
  } else if (children) {
    sIndent++;
    ConstructGroupInsideInactive(aCommandBuilder, aBuilder, aResources, aGroup,
                                 children, aSc);
    sIndent--;
  }

  GP("Including %s of %d\n", aItem->Name(), aGroup->mDisplayItems.Count());
  aGroup->mImageBounds = oldImageBounds;
}

/* This is just a copy of nsRect::ScaleToOutsidePixels with an offset added in.
 * The offset is applied just before the rounding. It's in the scaled space. */
static mozilla::gfx::IntRect ScaleToOutsidePixelsOffset(
    nsRect aRect, float aXScale, float aYScale, nscoord aAppUnitsPerPixel,
    LayerPoint aOffset) {
  mozilla::gfx::IntRect rect;
  rect.SetNonEmptyBox(
      NSToIntFloor(NSAppUnitsToFloatPixels(aRect.x, float(aAppUnitsPerPixel)) *
                       aXScale +
                   aOffset.x),
      NSToIntFloor(NSAppUnitsToFloatPixels(aRect.y, float(aAppUnitsPerPixel)) *
                       aYScale +
                   aOffset.y),
      NSToIntCeil(
          NSAppUnitsToFloatPixels(aRect.XMost(), float(aAppUnitsPerPixel)) *
              aXScale +
          aOffset.x),
      NSToIntCeil(
          NSAppUnitsToFloatPixels(aRect.YMost(), float(aAppUnitsPerPixel)) *
              aYScale +
          aOffset.y));
  return rect;
}

void WebRenderCommandBuilder::DoGroupingForDisplayList(
    nsDisplayList* aList, nsDisplayItem* aWrappingItem,
    nsDisplayListBuilder* aDisplayListBuilder, const StackingContextHelper& aSc,
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources) {
  if (!aList->GetBottom()) {
    return;
  }

  mClipManager.BeginList(aSc);
  Grouper g(mClipManager);
  int32_t appUnitsPerDevPixel =
      aWrappingItem->Frame()->PresContext()->AppUnitsPerDevPixel();
  GP("DoGroupingForDisplayList\n");

  g.mDisplayListBuilder = aDisplayListBuilder;
  RefPtr<WebRenderGroupData> groupData =
      CreateOrRecycleWebRenderUserData<WebRenderGroupData>(aWrappingItem);
  bool snapped;
  nsRect groupBounds = aWrappingItem->GetBounds(aDisplayListBuilder, &snapped);
  DIGroup& group = groupData->mSubGroup;
  auto p = group.mGroupBounds;
  auto q = groupBounds;
  gfx::Size scale = aSc.GetInheritedScale();
  auto trans =
      ViewAs<LayerPixel>(aSc.GetSnappingSurfaceTransform().GetTranslation());
  auto snappedTrans = LayerIntPoint::Floor(trans);
  LayerPoint residualOffset = trans - snappedTrans;

  GP("Inherrited scale %f %f\n", scale.width, scale.height);
  GP("Bounds: %d %d %d %d vs %d %d %d %d\n", p.x, p.y, p.width, p.height, q.x,
     q.y, q.width, q.height);
  if (!group.mGroupBounds.IsEqualEdges(groupBounds) ||
      group.mAppUnitsPerDevPixel != appUnitsPerDevPixel ||
      group.mScale != scale || group.mResidualOffset != residualOffset) {
    GP("Property change. Deleting blob\n");

    if (group.mAppUnitsPerDevPixel != appUnitsPerDevPixel) {
      GP(" App unit change %d -> %d\n", group.mAppUnitsPerDevPixel,
         appUnitsPerDevPixel);
    }
    // The bounds have changed so we need to discard the old image and add all
    // the commands again.
    auto p = group.mGroupBounds;
    auto q = groupBounds;
    if (!group.mGroupBounds.IsEqualEdges(groupBounds)) {
      GP(" Bounds change: %d %d %d %d -> %d %d %d %d\n", p.x, p.y, p.width,
         p.height, q.x, q.y, q.width, q.height);
    }

    if (group.mScale != scale) {
      GP(" Scale %f %f -> %f %f\n", group.mScale.width, group.mScale.height,
         scale.width, scale.height);
    }

    if (group.mResidualOffset != residualOffset) {
      GP(" Residual Offset %f %f -> %f %f\n", group.mResidualOffset.x,
         group.mResidualOffset.y, residualOffset.x, residualOffset.y);
    }

    group.ClearItems();
    group.ClearImageKey(mManager);
  }

  ScrollableLayerGuid::ViewID scrollId = ScrollableLayerGuid::NULL_SCROLL_ID;
  if (const ActiveScrolledRoot* asr = aWrappingItem->GetActiveScrolledRoot()) {
    scrollId = asr->GetViewId();
  }

  g.mAppUnitsPerDevPixel = appUnitsPerDevPixel;
  group.mResidualOffset = residualOffset;
  group.mGroupBounds = groupBounds;
  group.mAppUnitsPerDevPixel = appUnitsPerDevPixel;
  group.mLayerBounds = LayerIntRect::FromUnknownRect(
      ScaleToOutsidePixelsOffset(group.mGroupBounds, scale.width, scale.height,
                                 group.mAppUnitsPerDevPixel, residualOffset));
  group.mImageBounds =
      IntRect(0, 0, group.mLayerBounds.width, group.mLayerBounds.height);
  group.mPaintRect =
      LayerIntRect::FromUnknownRect(
          ScaleToOutsidePixelsOffset(aWrappingItem->GetPaintRect(), scale.width,
                                     scale.height, group.mAppUnitsPerDevPixel,
                                     residualOffset))
          .Intersect(group.mLayerBounds);
  // XXX: Make the paint rect relative to the layer bounds. After we include
  // mLayerBounds.TopLeft() in the blob image we want to stop doing this
  // adjustment.
  group.mPaintRect = group.mPaintRect - group.mLayerBounds.TopLeft();
  g.mTransform = Matrix::Scaling(scale.width, scale.height)
                     .PostTranslate(residualOffset.x, residualOffset.y);
  group.mScale = scale;
  group.mScrollId = scrollId;
  g.ConstructGroups(aDisplayListBuilder, this, aBuilder, aResources, &group,
                    aList, aSc);
  mClipManager.EndList(aSc);
}

void WebRenderCommandBuilder::Destroy() {
  mLastCanvasDatas.Clear();
  ClearCachedResources();
}

void WebRenderCommandBuilder::EmptyTransaction() {
  // We need to update canvases that might have changed.
  for (auto iter = mLastCanvasDatas.Iter(); !iter.Done(); iter.Next()) {
    RefPtr<WebRenderCanvasData> canvasData = iter.Get()->GetKey();
    WebRenderCanvasRendererAsync* canvas = canvasData->GetCanvasRenderer();
    if (canvas) {
      canvas->UpdateCompositableClientForEmptyTransaction();
    }
  }
}

bool WebRenderCommandBuilder::NeedsEmptyTransaction() {
  return !mLastCanvasDatas.IsEmpty();
}

void WebRenderCommandBuilder::BuildWebRenderCommands(
    wr::DisplayListBuilder& aBuilder,
    wr::IpcResourceUpdateQueue& aResourceUpdates, nsDisplayList* aDisplayList,
    nsDisplayListBuilder* aDisplayListBuilder, WebRenderScrollData& aScrollData,
    wr::LayoutSize& aContentSize, const nsTArray<wr::WrFilterOp>& aFilters) {
  StackingContextHelper sc;
  aScrollData = WebRenderScrollData(mManager);
  MOZ_ASSERT(mLayerScrollData.empty());
  mLastCanvasDatas.Clear();
  mLastAsr = nullptr;
  mBuilderDumpIndex = 0;
  mContainsSVGGroup = false;
  MOZ_ASSERT(mDumpIndent == 0);
  mClipManager.BeginBuild(mManager, aBuilder);

  {
    if (!mZoomProp && gfxPrefs::APZAllowZooming() && XRE_IsContentProcess()) {
      mZoomProp.emplace();
      mZoomProp->effect_type = wr::WrAnimationType::Transform;
      mZoomProp->id = AnimationHelper::GetNextCompositorAnimationsId();
    }

    StackingContextHelper pageRootSc(sc, nullptr, aBuilder, aFilters,
                                     LayoutDeviceRect(), nullptr,
                                     mZoomProp.ptrOr(nullptr));
    if (ShouldDumpDisplayList(aDisplayListBuilder)) {
      mBuilderDumpIndex =
          aBuilder.Dump(mDumpIndent + 1, Some(mBuilderDumpIndex), Nothing());
    }
    CreateWebRenderCommandsFromDisplayList(aDisplayList, nullptr,
                                           aDisplayListBuilder, pageRootSc,
                                           aBuilder, aResourceUpdates);
  }

  // Make a "root" layer data that has everything else as descendants
  mLayerScrollData.emplace_back();
  mLayerScrollData.back().InitializeRoot(mLayerScrollData.size() - 1);
  if (mZoomProp) {
    mLayerScrollData.back().SetZoomAnimationId(mZoomProp->id);
  }
  auto callback =
      [&aScrollData](ScrollableLayerGuid::ViewID aScrollId) -> bool {
    return aScrollData.HasMetadataFor(aScrollId).isSome();
  };
  if (Maybe<ScrollMetadata> rootMetadata = nsLayoutUtils::GetRootMetadata(
          aDisplayListBuilder, mManager, ContainerLayerParameters(),
          callback)) {
    mLayerScrollData.back().AppendScrollMetadata(aScrollData,
                                                 rootMetadata.ref());
  }
  // Append the WebRenderLayerScrollData items into WebRenderScrollData
  // in reverse order, from topmost to bottommost. This is in keeping with
  // the semantics of WebRenderScrollData.
  for (auto i = mLayerScrollData.crbegin(); i != mLayerScrollData.crend();
       i++) {
    aScrollData.AddLayerData(*i);
  }
  mLayerScrollData.clear();
  mClipManager.EndBuild();

  // Remove the user data those are not displayed on the screen and
  // also reset the data to unused for next transaction.
  RemoveUnusedAndResetWebRenderUserData();
}

bool WebRenderCommandBuilder::ShouldDumpDisplayList(
    nsDisplayListBuilder* aBuilder) {
  return aBuilder != nullptr && aBuilder->IsInActiveDocShell() &&
         ((XRE_IsParentProcess() && gfxPrefs::WebRenderDLDumpParent()) ||
          (XRE_IsContentProcess() && gfxPrefs::WebRenderDLDumpContent()));
}

void WebRenderCommandBuilder::CreateWebRenderCommandsFromDisplayList(
    nsDisplayList* aDisplayList, nsDisplayItem* aWrappingItem,
    nsDisplayListBuilder* aDisplayListBuilder, const StackingContextHelper& aSc,
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources) {
  if (mDoGrouping) {
    MOZ_RELEASE_ASSERT(
        aWrappingItem,
        "Only the root list should have a null wrapping item, and mDoGrouping "
        "should never be true for the root list.");
    GP("actually entering the grouping code\n");
    DoGroupingForDisplayList(aDisplayList, aWrappingItem, aDisplayListBuilder,
                             aSc, aBuilder, aResources);
    return;
  }

  bool dumpEnabled = ShouldDumpDisplayList(aDisplayListBuilder);
  if (dumpEnabled) {
    // If we're inside a nested display list, print the WR DL items from the
    // wrapper item before we start processing the nested items.
    mBuilderDumpIndex =
        aBuilder.Dump(mDumpIndent + 1, Some(mBuilderDumpIndex), Nothing());
  }

  mDumpIndent++;
  mClipManager.BeginList(aSc);

  bool apzEnabled = mManager->AsyncPanZoomEnabled();

  FlattenedDisplayItemIterator iter(aDisplayListBuilder, aDisplayList);
  while (nsDisplayItem* i = iter.GetNext()) {
    nsDisplayItem* item = i;
    DisplayItemType itemType = item->GetType();

    // Peek ahead to the next item and try merging with it or swapping with it
    // if necessary.
    AutoTArray<nsDisplayItem*, 1> mergedItems;
    mergedItems.AppendElement(item);
    while (nsDisplayItem* peek = iter.PeekNext()) {
      if (!item->CanMerge(peek)) {
        break;
      }

      mergedItems.AppendElement(peek);

      // Move the iterator forward since we will merge this item.
      i = iter.GetNext();
    }

    if (mergedItems.Length() > 1) {
      item = aDisplayListBuilder->MergeItems(mergedItems);
      MOZ_ASSERT(item && itemType == item->GetType());
    }

    bool forceNewLayerData = false;
    size_t layerCountBeforeRecursing = mLayerScrollData.size();
    if (apzEnabled) {
      // For some types of display items we want to force a new
      // WebRenderLayerScrollData object, to ensure we preserve the APZ-relevant
      // data that is in the display item.
      forceNewLayerData = item->UpdateScrollData(nullptr, nullptr);

      // Anytime the ASR changes we also want to force a new layer data because
      // the stack of scroll metadata is going to be different for this
      // display item than previously, so we can't squash the display items
      // into the same "layer".
      const ActiveScrolledRoot* asr = item->GetActiveScrolledRoot();
      if (asr != mLastAsr) {
        mLastAsr = asr;
        forceNewLayerData = true;
      }

      // Refer to the comment on StackingContextHelper::mDeferredTransformItem
      // for an overview of what this is about. This bit of code applies to the
      // case where we are deferring a transform item, and we then need to defer
      // another transform with a different ASR. In such a case we cannot just
      // merge the deferred transforms, but need to force a new
      // WebRenderLayerScrollData item to flush the old deferred transform, so
      // that we can then start deferring the new one.
      if (!forceNewLayerData &&
          item->GetType() == DisplayItemType::TYPE_TRANSFORM &&
          aSc.GetDeferredTransformItem() &&
          (*aSc.GetDeferredTransformItem())->GetActiveScrolledRoot() != asr) {
        forceNewLayerData = true;
      }

      // If we're going to create a new layer data for this item, stash the
      // ASR so that if we recurse into a sublist they will know where to stop
      // walking up their ASR chain when building scroll metadata.
      if (forceNewLayerData) {
        mAsrStack.push_back(asr);
      }
    }

    mClipManager.BeginItem(item, aSc);

    {  // scope restoreDoGrouping
      AutoRestore<bool> restoreDoGrouping(mDoGrouping);
      if (itemType == DisplayItemType::TYPE_SVG_WRAPPER) {
        // Inside an <svg>, all display items that are not LAYER_ACTIVE wrapper
        // display items (like animated transforms / opacity) share the same
        // animated geometry root, so we can combine subsequent items of that
        // type into the same image.
        mContainsSVGGroup = mDoGrouping = true;
        GP("attempting to enter the grouping code\n");
      }

      if (dumpEnabled) {
        std::stringstream ss;
        nsFrame::PrintDisplayItem(aDisplayListBuilder, item, ss,
                                  static_cast<uint32_t>(mDumpIndent));
        printf_stderr("%s", ss.str().c_str());
      }

      // Note: this call to CreateWebRenderCommands can recurse back into
      // this function if the |item| is a wrapper for a sublist.
      item->SetPaintRect(item->GetBuildingRect());
      bool createdWRCommands = item->CreateWebRenderCommands(
          aBuilder, aResources, aSc, mManager, aDisplayListBuilder);
      if (!createdWRCommands) {
        PushItemAsImage(item, aBuilder, aResources, aSc, aDisplayListBuilder);
      }

      if (dumpEnabled) {
        mBuilderDumpIndex =
            aBuilder.Dump(mDumpIndent + 1, Some(mBuilderDumpIndex), Nothing());
      }
    }

    if (apzEnabled) {
      if (forceNewLayerData) {
        // Pop the thing we pushed before the recursion, so the topmost item on
        // the stack is enclosing display item's ASR (or the stack is empty)
        mAsrStack.pop_back();
        const ActiveScrolledRoot* stopAtAsr =
            mAsrStack.empty() ? nullptr : mAsrStack.back();

        int32_t descendants =
            mLayerScrollData.size() - layerCountBeforeRecursing;

        // See the comments on StackingContextHelper::mDeferredTransformItem
        // for an overview of what deferred transforms are.
        // In the case where we deferred a transform, but have a child display
        // item with a different ASR than the deferred transform item, we cannot
        // put the transform on the WebRenderLayerScrollData item for the child.
        // We cannot do this because it will not conform to APZ's expectations
        // with respect to how the APZ tree ends up structured. In particular,
        // the GetTransformToThis() for the child APZ (which is created for the
        // child item's ASR) will not include the transform when we actually do
        // want it to.
        // When we run into this scenario, we solve it by creating two
        // WebRenderLayerScrollData items; one that just holds the transform,
        // that we deferred, and a child WebRenderLayerScrollData item that
        // holds the scroll metadata for the child's ASR.
        Maybe<nsDisplayTransform*> deferred = aSc.GetDeferredTransformItem();
        if (deferred && (*deferred)->GetActiveScrolledRoot() !=
                            item->GetActiveScrolledRoot()) {
          // This creates the child WebRenderLayerScrollData for |item|, but
          // omits the transform (hence the Nothing() as the last argument to
          // Initialize(...)). We also need to make sure that the ASR from
          // the deferred transform item is not on this node, so we use that
          // ASR as the "stop at" ASR for this WebRenderLayerScrollData.
          mLayerScrollData.emplace_back();
          mLayerScrollData.back().Initialize(
              mManager->GetScrollData(), item, descendants,
              (*deferred)->GetActiveScrolledRoot(), Nothing());

          // The above WebRenderLayerScrollData will also be a descendant of
          // the transform-holding WebRenderLayerScrollData we create below.
          descendants++;

          // This creates the WebRenderLayerScrollData for the deferred
          // transform item. This holds the transform matrix and the remaining
          // ASRs needed to complete the ASR chain (i.e. the ones from the
          // stopAtAsr down to the deferred transform item's ASR, which must be
          // "between" stopAtAsr and |item|'s ASR in the ASR tree).
          mLayerScrollData.emplace_back();
          mLayerScrollData.back().Initialize(mManager->GetScrollData(),
                                             *deferred, descendants, stopAtAsr,
                                             aSc.GetDeferredTransformMatrix());
        } else {
          // This is the "simple" case where we don't need to create two
          // WebRenderLayerScrollData items; we can just create one that also
          // holds the deferred transform matrix, if any.
          mLayerScrollData.emplace_back();
          mLayerScrollData.back().Initialize(mManager->GetScrollData(), item,
                                             descendants, stopAtAsr,
                                             aSc.GetDeferredTransformMatrix());
        }
      }
    }
  }

  mDumpIndent--;
  mClipManager.EndList(aSc);
}

void WebRenderCommandBuilder::PushOverrideForASR(const ActiveScrolledRoot* aASR,
                                                 const wr::WrClipId& aClipId) {
  mClipManager.PushOverrideForASR(aASR, aClipId);
}

void WebRenderCommandBuilder::PopOverrideForASR(
    const ActiveScrolledRoot* aASR) {
  mClipManager.PopOverrideForASR(aASR);
}

Maybe<wr::ImageKey> WebRenderCommandBuilder::CreateImageKey(
    nsDisplayItem* aItem, ImageContainer* aContainer,
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    mozilla::wr::ImageRendering aRendering, const StackingContextHelper& aSc,
    gfx::IntSize& aSize, const Maybe<LayoutDeviceRect>& aAsyncImageBounds) {
  RefPtr<WebRenderImageData> imageData =
      CreateOrRecycleWebRenderUserData<WebRenderImageData>(aItem);
  MOZ_ASSERT(imageData);

  if (aContainer->IsAsync()) {
    MOZ_ASSERT(aAsyncImageBounds);

    LayoutDeviceRect rect = aAsyncImageBounds.value();
    LayoutDeviceRect scBounds(LayoutDevicePoint(0, 0), rect.Size());
    gfx::MaybeIntSize scaleToSize;
    if (!aContainer->GetScaleHint().IsEmpty()) {
      scaleToSize = Some(aContainer->GetScaleHint());
    }
    gfx::Matrix4x4 transform =
        gfx::Matrix4x4::From2D(aContainer->GetTransformHint());
    // TODO!
    // We appear to be using the image bridge for a lot (most/all?) of
    // layers-free image handling and that breaks frame consistency.
    imageData->CreateAsyncImageWebRenderCommands(
        aBuilder, aContainer, aSc, rect, scBounds, transform, scaleToSize,
        aRendering, wr::MixBlendMode::Normal, !aItem->BackfaceIsHidden());
    return Nothing();
  }

  AutoLockImage autoLock(aContainer);
  if (!autoLock.HasImage()) {
    return Nothing();
  }
  mozilla::layers::Image* image = autoLock.GetImage();
  aSize = image->GetSize();

  return imageData->UpdateImageKey(aContainer, aResources);
}

bool WebRenderCommandBuilder::PushImage(
    nsDisplayItem* aItem, ImageContainer* aContainer,
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, const LayoutDeviceRect& aRect) {
  mozilla::wr::ImageRendering rendering = wr::ToImageRendering(
      nsLayoutUtils::GetSamplingFilterForFrame(aItem->Frame()));
  gfx::IntSize size;
  Maybe<wr::ImageKey> key =
      CreateImageKey(aItem, aContainer, aBuilder, aResources, rendering, aSc,
                     size, Some(aRect));
  if (aContainer->IsAsync()) {
    // Async ImageContainer does not create ImageKey, instead it uses Pipeline.
    MOZ_ASSERT(key.isNothing());
    return true;
  }
  if (!key) {
    return false;
  }

  auto r = wr::ToRoundedLayoutRect(aRect);
  aBuilder.PushImage(r, r, !aItem->BackfaceIsHidden(), rendering, key.value());

  return true;
}

bool BuildLayer(nsDisplayItem* aItem, BlobItemData* aData,
                nsDisplayListBuilder* aDisplayListBuilder,
                const gfx::Size& aScale) {
  if (!aData->mLayerManager) {
    aData->mLayerManager =
        new BasicLayerManager(BasicLayerManager::BLM_INACTIVE);
  }
  RefPtr<BasicLayerManager> blm = aData->mLayerManager;
  UniquePtr<LayerProperties> props;
  if (blm->GetRoot()) {
    props = LayerProperties::CloneFrom(blm->GetRoot());
  }
  FrameLayerBuilder* layerBuilder = new FrameLayerBuilder();
  layerBuilder->Init(aDisplayListBuilder, blm, nullptr, true);
  layerBuilder->DidBeginRetainedLayerTransaction(blm);

  blm->BeginTransaction();
  bool isInvalidated = false;

  ContainerLayerParameters param(aScale.width, aScale.height);
  RefPtr<Layer> root = aItem->BuildLayer(aDisplayListBuilder, blm, param);

  if (root) {
    blm->SetRoot(root);
    layerBuilder->WillEndTransaction();

    // Check if there is any invalidation region.
    nsIntRegion invalid;
    if (props) {
      props->ComputeDifferences(root, invalid, nullptr);
      if (!invalid.IsEmpty()) {
        isInvalidated = true;
      }
    } else {
      isInvalidated = true;
    }
  }
  blm->AbortTransaction();

  return isInvalidated;
}

static bool PaintByLayer(nsDisplayItem* aItem,
                         nsDisplayListBuilder* aDisplayListBuilder,
                         const RefPtr<BasicLayerManager>& aManager,
                         gfxContext* aContext, const gfx::Size& aScale,
                         const std::function<void()>& aPaintFunc) {
  UniquePtr<LayerProperties> props;
  if (aManager->GetRoot()) {
    props = LayerProperties::CloneFrom(aManager->GetRoot());
  }
  FrameLayerBuilder* layerBuilder = new FrameLayerBuilder();
  layerBuilder->Init(aDisplayListBuilder, aManager, nullptr, true);
  layerBuilder->DidBeginRetainedLayerTransaction(aManager);

  aManager->SetDefaultTarget(aContext);
  nsCString none;
  aManager->BeginTransactionWithTarget(aContext, none);
  bool isInvalidated = false;

  ContainerLayerParameters param(aScale.width, aScale.height);
  RefPtr<Layer> root = aItem->BuildLayer(aDisplayListBuilder, aManager, param);

  if (root) {
    aManager->SetRoot(root);
    layerBuilder->WillEndTransaction();

    aPaintFunc();

    // Check if there is any invalidation region.
    nsIntRegion invalid;
    if (props) {
      props->ComputeDifferences(root, invalid, nullptr);
      if (!invalid.IsEmpty()) {
        isInvalidated = true;
      }
    } else {
      isInvalidated = true;
    }
  }

#ifdef MOZ_DUMP_PAINTING
  if (gfxUtils::DumpDisplayList() || gfxEnv::DumpPaint()) {
    fprintf_stderr(
        gfxUtils::sDumpPaintFile,
        "Basic layer tree for painting contents of display item %s(%p):\n",
        aItem->Name(), aItem->Frame());
    std::stringstream stream;
    aManager->Dump(stream, "", gfxEnv::DumpPaintToFile());
    fprint_stderr(
        gfxUtils::sDumpPaintFile,
        stream);  // not a typo, fprint_stderr declared in LayersLogging.h
  }
#endif

  if (aManager->InTransaction()) {
    aManager->AbortTransaction();
  }

  aManager->SetTarget(nullptr);
  aManager->SetDefaultTarget(nullptr);

  return isInvalidated;
}

static bool PaintItemByDrawTarget(nsDisplayItem* aItem, gfx::DrawTarget* aDT,
                                  const LayoutDevicePoint& aOffset,
                                  nsDisplayListBuilder* aDisplayListBuilder,
                                  const RefPtr<BasicLayerManager>& aManager,
                                  const gfx::Size& aScale,
                                  Maybe<gfx::Color>& aHighlight) {
  MOZ_ASSERT(aDT);

  bool isInvalidated = false;
  // XXX Why is this ClearRect() needed?
  aDT->ClearRect(Rect(aDT->GetRect()));
  RefPtr<gfxContext> context = gfxContext::CreateOrNull(aDT);
  MOZ_ASSERT(context);

  switch (aItem->GetType()) {
    case DisplayItemType::TYPE_MASK:
      context->SetMatrix(context->CurrentMatrix()
                             .PreScale(aScale.width, aScale.height)
                             .PreTranslate(-aOffset.x, -aOffset.y));
      static_cast<nsDisplayMasksAndClipPaths*>(aItem)->PaintMask(
          aDisplayListBuilder, context, &isInvalidated);
      break;
    case DisplayItemType::TYPE_SVG_WRAPPER: {
      // XXX Why doesn't this need the scaling applied?
      context->SetMatrix(
          context->CurrentMatrix().PreTranslate(-aOffset.x, -aOffset.y));
      isInvalidated = PaintByLayer(
          aItem, aDisplayListBuilder, aManager, context, aScale, [&]() {
            aManager->EndTransaction(FrameLayerBuilder::DrawPaintedLayer,
                                     aDisplayListBuilder);
          });
      break;
    }

    case DisplayItemType::TYPE_FILTER: {
      context->SetMatrix(context->CurrentMatrix()
                             .PreScale(aScale.width, aScale.height)
                             .PreTranslate(-aOffset.x, -aOffset.y));
      isInvalidated = PaintByLayer(
          aItem, aDisplayListBuilder, aManager, context, {1, 1}, [&]() {
            static_cast<nsDisplayFilters*>(aItem)->PaintAsLayer(
                aDisplayListBuilder, context, aManager);
          });
      break;
    }

    default:
      context->SetMatrix(context->CurrentMatrix()
                             .PreScale(aScale.width, aScale.height)
                             .PreTranslate(-aOffset.x, -aOffset.y));
      if (aDisplayListBuilder->IsPaintingToWindow()) {
        aItem->Frame()->AddStateBits(NS_FRAME_PAINTED_THEBES);
      }
      aItem->Paint(aDisplayListBuilder, context);
      isInvalidated = true;
      break;
  }

  if (aItem->GetType() != DisplayItemType::TYPE_MASK) {
    // Apply highlight fills, if the appropriate prefs are set.
    // We don't do this for masks because we'd be filling the A8 mask surface,
    // which isn't very useful.
    if (aHighlight) {
      aDT->SetTransform(gfx::Matrix());
      aDT->FillRect(Rect(aDT->GetRect()),
                    gfx::ColorPattern(aHighlight.value()));
    }
    if (aItem->Frame()->PresContext()->GetPaintFlashing() && isInvalidated) {
      aDT->SetTransform(gfx::Matrix());
      float r = float(rand()) / RAND_MAX;
      float g = float(rand()) / RAND_MAX;
      float b = float(rand()) / RAND_MAX;
      aDT->FillRect(Rect(aDT->GetRect()),
                    gfx::ColorPattern(gfx::Color(r, g, b, 0.5)));
    }
  }

  return isInvalidated;
}

already_AddRefed<WebRenderFallbackData>
WebRenderCommandBuilder::GenerateFallbackData(
    nsDisplayItem* aItem, wr::DisplayListBuilder& aBuilder,
    wr::IpcResourceUpdateQueue& aResources, const StackingContextHelper& aSc,
    nsDisplayListBuilder* aDisplayListBuilder, LayoutDeviceRect& aImageRect) {
  bool useBlobImage =
      gfxPrefs::WebRenderBlobImages() && !aItem->MustPaintOnContentSide();
  Maybe<gfx::Color> highlight = Nothing();
  if (gfxPrefs::WebRenderHighlightPaintedLayers()) {
    highlight = Some(useBlobImage ? gfx::Color(1.0, 0.0, 0.0, 0.5)
                                  : gfx::Color(1.0, 1.0, 0.0, 0.5));
  }

  RefPtr<WebRenderFallbackData> fallbackData =
      CreateOrRecycleWebRenderUserData<WebRenderFallbackData>(aItem);

  bool snap;
  nsRect itemBounds = aItem->GetBounds(aDisplayListBuilder, &snap);

  // Blob images will only draw the visible area of the blob so we don't need to
  // clip them here and can just rely on the webrender clipping.
  // TODO We also don't clip native themed widget to avoid over-invalidation
  // during scrolling. it would be better to support a sort of straming/tiling
  // scheme for large ones but the hope is that we should not have large native
  // themed items.
  nsRect paintBounds = itemBounds;
  if (useBlobImage || aItem->MustPaintOnContentSide()) {
    paintBounds = itemBounds;
  } else {
    paintBounds = aItem->GetClippedBounds(aDisplayListBuilder);
  }

  // nsDisplayItem::Paint() may refer the variables that come from
  // ComputeVisibility(). So we should call ComputeVisibility() before painting.
  // e.g.: nsDisplayBoxShadowInner uses mPaintRect in Paint() and mPaintRect is
  // computed in nsDisplayBoxShadowInner::ComputeVisibility().
  nsRegion visibleRegion(paintBounds);
  aItem->SetPaintRect(paintBounds);
  aItem->ComputeVisibility(aDisplayListBuilder, &visibleRegion);

  const int32_t appUnitsPerDevPixel =
      aItem->Frame()->PresContext()->AppUnitsPerDevPixel();
  auto bounds =
      LayoutDeviceRect::FromAppUnits(paintBounds, appUnitsPerDevPixel);
  if (bounds.IsEmpty()) {
    return nullptr;
  }

  gfx::Size scale = aSc.GetInheritedScale();
  gfx::Size oldScale = fallbackData->GetScale();
  // This scale determination should probably be done using
  // ChooseScaleAndSetTransform but for now we just fake it.
  // We tolerate slight changes in scale so that we don't, for example,
  // rerasterize on MotionMark
  bool differentScale = gfx::FuzzyEqual(scale.width, oldScale.width, 1e-6f) &&
                        gfx::FuzzyEqual(scale.height, oldScale.height, 1e-6f);

  LayoutDeviceToLayerScale2D layerScale(scale.width, scale.height);
  auto scaledBounds = bounds * layerScale;
  auto dtRect = RoundedOut(scaledBounds);
  auto dtSize = dtRect.Size();
  if (dtSize.IsEmpty()) {
    return nullptr;
  }

  aImageRect = dtRect / layerScale;

  auto offset = aImageRect.TopLeft();

  nsDisplayItemGeometry* geometry = fallbackData->GetGeometry();

  bool needPaint = true;

  // nsDisplayFilters is rendered via BasicLayerManager which means the
  // invalidate region is unknown until we traverse the displaylist contained by
  // it.
  if (geometry && !fallbackData->IsInvalid() &&
      aItem->GetType() != DisplayItemType::TYPE_FILTER &&
      aItem->GetType() != DisplayItemType::TYPE_SVG_WRAPPER && differentScale) {
    nsRect invalid;
    nsRegion invalidRegion;

    if (aItem->IsInvalid(invalid)) {
      invalidRegion.OrWith(paintBounds);
    } else {
      nsPoint shift = itemBounds.TopLeft() - geometry->mBounds.TopLeft();
      geometry->MoveBy(shift);
      aItem->ComputeInvalidationRegion(aDisplayListBuilder, geometry,
                                       &invalidRegion);

      nsRect lastBounds = fallbackData->GetBounds();
      lastBounds.MoveBy(shift);

      if (!lastBounds.IsEqualInterior(paintBounds)) {
        invalidRegion.OrWith(lastBounds);
        invalidRegion.OrWith(paintBounds);
      }
    }
    needPaint = !invalidRegion.IsEmpty();
  }

  if (needPaint || !fallbackData->GetImageKey()) {
    nsAutoPtr<nsDisplayItemGeometry> newGeometry;
    newGeometry = aItem->AllocateGeometry(aDisplayListBuilder);
    fallbackData->SetGeometry(std::move(newGeometry));

    gfx::SurfaceFormat format = aItem->GetType() == DisplayItemType::TYPE_MASK
                                    ? gfx::SurfaceFormat::A8
                                    : gfx::SurfaceFormat::B8G8R8A8;
    if (useBlobImage) {
      bool snapped;
      wr::OpacityType opacity =
          aItem->GetOpaqueRegion(aDisplayListBuilder, &snapped)
                  .Contains(paintBounds)
              ? wr::OpacityType::Opaque
              : wr::OpacityType::HasAlphaChannel;
      std::vector<RefPtr<ScaledFont>> fonts;

      RefPtr<WebRenderDrawEventRecorder> recorder =
          MakeAndAddRef<WebRenderDrawEventRecorder>(
              [&](MemStream& aStream,
                  std::vector<RefPtr<ScaledFont>>& aScaledFonts) {
                size_t count = aScaledFonts.size();
                aStream.write((const char*)&count, sizeof(count));
                for (auto& scaled : aScaledFonts) {
                  BlobFont font = {
                      mManager->WrBridge()->GetFontKeyForScaledFont(
                          scaled, &aResources),
                      scaled};
                  aStream.write((const char*)&font, sizeof(font));
                }
                fonts = std::move(aScaledFonts);
              });
      RefPtr<gfx::DrawTarget> dummyDt = gfx::Factory::CreateDrawTarget(
          gfx::BackendType::SKIA, gfx::IntSize(1, 1), format);
      RefPtr<gfx::DrawTarget> dt = gfx::Factory::CreateRecordingDrawTarget(
          recorder, dummyDt, dtSize.ToUnknownSize());
      if (!fallbackData->mBasicLayerManager) {
        fallbackData->mBasicLayerManager =
            new BasicLayerManager(BasicLayerManager::BLM_INACTIVE);
      }
      bool isInvalidated = PaintItemByDrawTarget(
          aItem, dt, offset, aDisplayListBuilder,
          fallbackData->mBasicLayerManager, scale, highlight);
      recorder->FlushItem(IntRect({0, 0}, dtSize.ToUnknownSize()));
      TakeExternalSurfaces(recorder, fallbackData->mExternalSurfaces, mManager,
                           aResources);
      recorder->Finish();

      if (isInvalidated) {
        Range<uint8_t> bytes((uint8_t*)recorder->mOutputStream.mData,
                             recorder->mOutputStream.mLength);
        wr::BlobImageKey key =
            wr::BlobImageKey{mManager->WrBridge()->GetNextImageKey()};
        wr::ImageDescriptor descriptor(dtSize.ToUnknownSize(), 0,
                                       dt->GetFormat(), opacity);
        if (!aResources.AddBlobImage(key, descriptor, bytes)) {
          return nullptr;
        }
        fallbackData->SetBlobImageKey(key);
        fallbackData->SetFonts(fonts);
      } else {
        // If there is no invalidation region and we don't have a image key,
        // it means we don't need to push image for the item.
        if (!fallbackData->GetBlobImageKey().isSome()) {
          return nullptr;
        }
      }
    } else {
      fallbackData->CreateImageClientIfNeeded();
      RefPtr<ImageClient> imageClient = fallbackData->GetImageClient();
      RefPtr<ImageContainer> imageContainer =
          LayerManager::CreateImageContainer();
      bool isInvalidated = false;

      {
        UpdateImageHelper helper(imageContainer, imageClient,
                                 dtSize.ToUnknownSize(), format);
        {
          RefPtr<gfx::DrawTarget> dt = helper.GetDrawTarget();
          if (!dt) {
            return nullptr;
          }
          if (!fallbackData->mBasicLayerManager) {
            fallbackData->mBasicLayerManager =
                new BasicLayerManager(mManager->GetWidget());
          }
          isInvalidated = PaintItemByDrawTarget(
              aItem, dt, offset, aDisplayListBuilder,
              fallbackData->mBasicLayerManager, scale, highlight);
        }

        if (isInvalidated) {
          // Update image if there it's invalidated.
          if (!helper.UpdateImage()) {
            return nullptr;
          }
        } else {
          // If there is no invalidation region and we don't have a image key,
          // it means we don't need to push image for the item.
          if (!fallbackData->GetImageKey().isSome()) {
            return nullptr;
          }
        }
      }

      // Force update the key in fallback data since we repaint the image in
      // this path. If not force update, fallbackData may reuse the original key
      // because it doesn't know UpdateImageHelper already updated the image
      // container.
      if (isInvalidated &&
          !fallbackData->UpdateImageKey(imageContainer, aResources, true)) {
        return nullptr;
      }
    }

    fallbackData->SetScale(scale);
    fallbackData->SetInvalid(false);
  }

  // Update current bounds to fallback data
  fallbackData->SetBounds(paintBounds);

  MOZ_ASSERT(fallbackData->GetImageKey());

  return fallbackData.forget();
}

Maybe<wr::WrImageMask> WebRenderCommandBuilder::BuildWrMaskImage(
    nsDisplayItem* aItem, wr::DisplayListBuilder& aBuilder,
    wr::IpcResourceUpdateQueue& aResources, const StackingContextHelper& aSc,
    nsDisplayListBuilder* aDisplayListBuilder,
    const LayoutDeviceRect& aBounds) {
  LayoutDeviceRect imageRect;
  RefPtr<WebRenderFallbackData> fallbackData = GenerateFallbackData(
      aItem, aBuilder, aResources, aSc, aDisplayListBuilder, imageRect);
  if (!fallbackData) {
    return Nothing();
  }

  wr::WrImageMask imageMask;
  imageMask.image = fallbackData->GetImageKey().value();
  imageMask.rect = wr::ToRoundedLayoutRect(imageRect);
  imageMask.repeat = false;
  return Some(imageMask);
}

bool WebRenderCommandBuilder::PushItemAsImage(
    nsDisplayItem* aItem, wr::DisplayListBuilder& aBuilder,
    wr::IpcResourceUpdateQueue& aResources, const StackingContextHelper& aSc,
    nsDisplayListBuilder* aDisplayListBuilder) {
  LayoutDeviceRect imageRect;
  RefPtr<WebRenderFallbackData> fallbackData = GenerateFallbackData(
      aItem, aBuilder, aResources, aSc, aDisplayListBuilder, imageRect);
  if (!fallbackData) {
    return false;
  }

  wr::LayoutRect dest = wr::ToRoundedLayoutRect(imageRect);
  gfx::SamplingFilter sampleFilter =
      nsLayoutUtils::GetSamplingFilterForFrame(aItem->Frame());
  aBuilder.PushImage(dest, dest, !aItem->BackfaceIsHidden(),
                     wr::ToImageRendering(sampleFilter),
                     fallbackData->GetImageKey().value());
  return true;
}

void WebRenderCommandBuilder::RemoveUnusedAndResetWebRenderUserData() {
  for (auto iter = mWebRenderUserDatas.Iter(); !iter.Done(); iter.Next()) {
    WebRenderUserData* data = iter.Get()->GetKey();
    if (!data->IsUsed()) {
      nsIFrame* frame = data->GetFrame();

      MOZ_ASSERT(frame->HasProperty(WebRenderUserDataProperty::Key()));

      WebRenderUserDataTable* userDataTable =
          frame->GetProperty(WebRenderUserDataProperty::Key());

      MOZ_ASSERT(userDataTable->Count());

      userDataTable->Remove(
          WebRenderUserDataKey(data->GetDisplayItemKey(), data->GetType()));

      if (!userDataTable->Count()) {
        frame->RemoveProperty(WebRenderUserDataProperty::Key());
        delete userDataTable;
      }

      if (data->GetType() == WebRenderUserData::UserDataType::eCanvas) {
        mLastCanvasDatas.RemoveEntry(data->AsCanvasData());
      }

      iter.Remove();
      continue;
    }

    data->SetUsed(false);
  }
}

void WebRenderCommandBuilder::ClearCachedResources() {
  RemoveUnusedAndResetWebRenderUserData();
  // UserDatas should only be in the used state during a call to
  // WebRenderCommandBuilder::BuildWebRenderCommands The should always be false
  // upon return from BuildWebRenderCommands().
  MOZ_RELEASE_ASSERT(mWebRenderUserDatas.Count() == 0);
}

WebRenderGroupData::WebRenderGroupData(WebRenderLayerManager* aWRManager,
                                       nsDisplayItem* aItem)
    : WebRenderUserData(aWRManager, aItem) {
  MOZ_COUNT_CTOR(WebRenderGroupData);
}

WebRenderGroupData::~WebRenderGroupData() {
  MOZ_COUNT_DTOR(WebRenderGroupData);
  GP("Group data destruct\n");
  mSubGroup.ClearImageKey(mWRManager, true);
  mFollowingGroup.ClearImageKey(mWRManager, true);
}

}  // namespace layers
}  // namespace mozilla
