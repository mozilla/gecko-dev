/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/LayerTransactionParent.h"
#include "nsFrameLoader.h"
#include "nsStyleStructInlines.h"
#include "nsSubDocumentFrame.h"
#include "RenderFrame.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/layers/WebRenderScrollData.h"
#include "mozilla/webrender/WebRenderAPI.h"

using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::layers;

namespace mozilla {
namespace layout {

static already_AddRefed<LayerManager> GetLayerManager(
    nsFrameLoader* aFrameLoader) {
  if (nsIContent* content = aFrameLoader->GetOwnerContent()) {
    RefPtr<LayerManager> lm = nsContentUtils::LayerManagerForContent(content);
    if (lm) {
      return lm.forget();
    }
  }

  nsIDocument* doc = aFrameLoader->GetOwnerDoc();
  if (!doc) {
    return nullptr;
  }
  return nsContentUtils::LayerManagerForDocument(doc);
}

RenderFrame::RenderFrame()
    : mLayersId{0},
      mFrameLoader(nullptr),
      mLayerManager(nullptr),
      mInitialized(false),
      mLayersConnected(false) {}

RenderFrame::~RenderFrame() {}

bool RenderFrame::Initialize(nsFrameLoader* aFrameLoader) {
  if (mInitialized || !aFrameLoader) {
    return false;
  }

  mFrameLoader = aFrameLoader;
  RefPtr<LayerManager> lm = GetLayerManager(mFrameLoader);
  PCompositorBridgeChild* compositor =
      lm ? lm->GetCompositorBridgeChild() : nullptr;

  TabParent* browser = TabParent::GetFrom(aFrameLoader);
  mTabProcessId = browser->Manager()->AsContentParent()->OtherPid();

  // Our remote frame will push layers updates to the compositor,
  // and we'll keep an indirect reference to that tree.
  GPUProcessManager* gpm = GPUProcessManager::Get();
  mLayersConnected = gpm->AllocateAndConnectLayerTreeId(
      compositor, mTabProcessId, &mLayersId, &mCompositorOptions);

  mInitialized = true;
  return true;
}

void RenderFrame::Destroy() {
  if (mLayersId.IsValid()) {
    GPUProcessManager::Get()->UnmapLayerTreeId(mLayersId, mTabProcessId);
  }

  mFrameLoader = nullptr;
  mLayerManager = nullptr;
}

void RenderFrame::EnsureLayersConnected(CompositorOptions* aCompositorOptions) {
  RefPtr<LayerManager> lm = GetLayerManager(mFrameLoader);
  if (!lm) {
    return;
  }

  if (!lm->GetCompositorBridgeChild()) {
    return;
  }

  mLayersConnected = lm->GetCompositorBridgeChild()->SendNotifyChildRecreated(
      mLayersId, &mCompositorOptions);
  *aCompositorOptions = mCompositorOptions;
}

LayerManager* RenderFrame::AttachLayerManager() {
  RefPtr<LayerManager> lm;
  if (mFrameLoader) {
    lm = GetLayerManager(mFrameLoader);
  }

  // Perhaps the document containing this frame currently has no presentation?
  if (lm && lm->GetCompositorBridgeChild() && lm != mLayerManager) {
    mLayersConnected =
        lm->GetCompositorBridgeChild()->SendAdoptChild(mLayersId);
    FrameLayerBuilder::InvalidateAllLayers(lm);
  }

  mLayerManager = lm.forget();
  return mLayerManager;
}

void RenderFrame::OwnerContentChanged(nsIContent* aContent) {
  MOZ_ASSERT(!mFrameLoader || mFrameLoader->GetOwnerContent() == aContent,
             "Don't build new map if owner is same!");

  Unused << AttachLayerManager();
}

void RenderFrame::GetTextureFactoryIdentifier(
    TextureFactoryIdentifier* aTextureFactoryIdentifier) const {
  RefPtr<LayerManager> lm =
      mFrameLoader ? GetLayerManager(mFrameLoader) : nullptr;
  // Perhaps the document containing this frame currently has no presentation?
  if (lm) {
    *aTextureFactoryIdentifier = lm->GetTextureFactoryIdentifier();
  } else {
    *aTextureFactoryIdentifier = TextureFactoryIdentifier();
  }
}

}  // namespace layout
}  // namespace mozilla

/**
 * Gets the layer-pixel offset of aContainerFrame's content rect top-left
 * from the nearest display item reference frame (which we assume will be
 * inducing a ContainerLayer).
 */
static mozilla::LayoutDeviceIntPoint GetContentRectLayerOffset(
    nsIFrame* aContainerFrame, nsDisplayListBuilder* aBuilder) {
  nscoord auPerDevPixel = aContainerFrame->PresContext()->AppUnitsPerDevPixel();

  // Offset to the content rect in case we have borders or padding
  // Note that aContainerFrame could be a reference frame itself, so
  // we need to be careful here to ensure that we call ToReferenceFrame
  // on aContainerFrame and not its parent.
  nsPoint frameOffset =
      aBuilder->ToReferenceFrame(aContainerFrame) +
      aContainerFrame->GetContentRectRelativeToSelf().TopLeft();

  return mozilla::LayoutDeviceIntPoint::FromAppUnitsToNearest(frameOffset,
                                                              auPerDevPixel);
}

// Return true iff |aManager| is a "temporary layer manager".  They're
// used for small software rendering tasks, like drawWindow.  That's
// currently implemented by a BasicLayerManager without a backing
// widget, and hence in non-retained mode.
inline static bool IsTempLayerManager(mozilla::layers::LayerManager* aManager) {
  return (mozilla::layers::LayersBackend::LAYERS_BASIC ==
              aManager->GetBackendType() &&
          !static_cast<BasicLayerManager*>(aManager)->IsRetained());
}

nsDisplayRemote::nsDisplayRemote(nsDisplayListBuilder* aBuilder,
                                 nsSubDocumentFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame),
      mTabId{0},
      mEventRegionsOverride(EventRegionsOverride::NoOverride) {
  bool frameIsPointerEventsNone = aFrame->StyleUI()->GetEffectivePointerEvents(
                                      aFrame) == NS_STYLE_POINTER_EVENTS_NONE;
  if (aBuilder->IsInsidePointerEventsNoneDoc() || frameIsPointerEventsNone) {
    mEventRegionsOverride |= EventRegionsOverride::ForceEmptyHitRegion;
  }
  if (nsLayoutUtils::HasDocumentLevelListenersForApzAwareEvents(
          aFrame->PresShell())) {
    mEventRegionsOverride |= EventRegionsOverride::ForceDispatchToContent;
  }

  nsFrameLoader* frameLoader = GetRenderFrame()->GetFrameLoader();
  if (frameLoader) {
    TabParent* browser = TabParent::GetFrom(frameLoader);
    if (browser) {
      mTabId = browser->GetTabId();
    }
  }
}

mozilla::LayerState nsDisplayRemote::GetLayerState(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aParameters) {
  if (IsTempLayerManager(aManager)) {
    return mozilla::LAYER_NONE;
  }
  return mozilla::LAYER_ACTIVE_FORCE;
}

bool nsDisplayRemote::HasDeletedFrame() const {
  // RenderFrame might change without invalidating nsSubDocumentFrame.
  return !GetRenderFrame() || nsDisplayItem::HasDeletedFrame();
}

already_AddRefed<Layer> nsDisplayRemote::BuildLayer(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aContainerParameters) {
  MOZ_ASSERT(GetRenderFrame());
  MOZ_ASSERT(mFrame, "Makes no sense to have a shadow tree without a frame");

  if (IsTempLayerManager(aManager)) {
    // This can happen if aManager is a "temporary" manager, or if the
    // widget's layer manager changed out from under us.  We need to
    // FIXME handle the former case somehow, probably with an API to
    // draw a manager's subtree.  The latter is bad bad bad, but the the
    // MOZ_ASSERT() above will flag it.  Returning nullptr here will just
    // cause the shadow subtree not to be rendered.
    if (!aContainerParameters.mForEventsAndPluginsOnly) {
      NS_WARNING("Remote iframe not rendered");
    }
    return nullptr;
  }

  LayersId remoteId = GetRenderFrame()->GetLayersId();

  if (!remoteId.IsValid()) {
    return nullptr;
  }

  RefPtr<Layer> layer =
      aManager->GetLayerBuilder()->GetLeafLayerFor(aBuilder, this);

  if (!layer) {
    layer = aManager->CreateRefLayer();
  }

  if (!layer) {
    // Probably a temporary layer manager that doesn't know how to
    // use ref layers.
    return nullptr;
  }

  static_cast<RefLayer*>(layer.get())->SetReferentId(remoteId);
  LayoutDeviceIntPoint offset = GetContentRectLayerOffset(Frame(), aBuilder);
  // We can only have an offset if we're a child of an inactive
  // container, but our display item is LAYER_ACTIVE_FORCE which
  // forces all layers above to be active.
  MOZ_ASSERT(aContainerParameters.mOffset == nsIntPoint());
  Matrix4x4 m = Matrix4x4::Translation(offset.x, offset.y, 0.0);
  // Remote content can't be repainted by us, so we multiply down
  // the resolution that our container expects onto our container.
  m.PreScale(aContainerParameters.mXScale, aContainerParameters.mYScale, 1.0);
  layer->SetBaseTransform(m);

  if (layer->AsRefLayer()) {
    layer->AsRefLayer()->SetEventRegionsOverride(mEventRegionsOverride);
  }

  return layer.forget();
}

void nsDisplayRemote::Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) {
  DrawTarget* target = aCtx->GetDrawTarget();
  if (!target->IsRecording() || mTabId == 0) {
    NS_WARNING("Remote iframe not rendered");
    return;
  }

  int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  Rect destRect = mozilla::NSRectToSnappedRect(GetContentRect(),
                                               appUnitsPerDevPixel, *target);
  target->DrawDependentSurface(mTabId, destRect);
}

bool nsDisplayRemote::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::WebRenderLayerManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  mOffset = GetContentRectLayerOffset(mFrame, aDisplayListBuilder);

  LayoutDeviceRect rect = LayoutDeviceRect::FromAppUnits(
      mFrame->GetContentRectRelativeToSelf(),
      mFrame->PresContext()->AppUnitsPerDevPixel());
  rect += mOffset;

  aBuilder.PushIFrame(mozilla::wr::ToRoundedLayoutRect(rect),
                      !BackfaceIsHidden(),
                      mozilla::wr::AsPipelineId(GetRemoteLayersId()),
                      /*ignoreMissingPipelines*/ true);

  return true;
}

bool nsDisplayRemote::UpdateScrollData(
    mozilla::layers::WebRenderScrollData* aData,
    mozilla::layers::WebRenderLayerScrollData* aLayerData) {
  if (aLayerData) {
    aLayerData->SetReferentId(GetRemoteLayersId());
    aLayerData->SetTransform(
        mozilla::gfx::Matrix4x4::Translation(mOffset.x, mOffset.y, 0.0));
    aLayerData->SetEventRegionsOverride(mEventRegionsOverride);
  }
  return true;
}

LayersId nsDisplayRemote::GetRemoteLayersId() const {
  MOZ_ASSERT(GetRenderFrame());
  return GetRenderFrame()->GetLayersId();
}

mozilla::layout::RenderFrame* nsDisplayRemote::GetRenderFrame() const {
  return mFrame ? static_cast<nsSubDocumentFrame*>(mFrame)->GetRenderFrame()
                : nullptr;
}
