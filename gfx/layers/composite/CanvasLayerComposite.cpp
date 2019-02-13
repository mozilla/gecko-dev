/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasLayerComposite.h"
#include "composite/CompositableHost.h"  // for CompositableHost
#include "gfx2DGlue.h"                  // for ToFilter, ToMatrix4x4
#include "GraphicsFilter.h"             // for GraphicsFilter
#include "gfxUtils.h"                   // for gfxUtils, etc
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4
#include "mozilla/gfx/Point.h"          // for Point
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/Effects.h"     // for EffectChain
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsAString.h"
#include "nsRefPtr.h"                   // for nsRefPtr
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsString.h"                   // for nsAutoCString
#include "gfxVR.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

CanvasLayerComposite::CanvasLayerComposite(LayerManagerComposite* aManager)
  : CanvasLayer(aManager, nullptr)
  , LayerComposite(aManager)
  , mCompositableHost(nullptr)
{
  MOZ_COUNT_CTOR(CanvasLayerComposite);
  mImplData = static_cast<LayerComposite*>(this);
}

CanvasLayerComposite::~CanvasLayerComposite()
{
  MOZ_COUNT_DTOR(CanvasLayerComposite);

  CleanupResources();
}

bool
CanvasLayerComposite::SetCompositableHost(CompositableHost* aHost)
{
  switch (aHost->GetType()) {
    case CompositableType::IMAGE:
      mCompositableHost = aHost;
      return true;
    default:
      return false;
  }

}

Layer*
CanvasLayerComposite::GetLayer()
{
  return this;
}

void
CanvasLayerComposite::SetLayerManager(LayerManagerComposite* aManager)
{
  LayerComposite::SetLayerManager(aManager);
  mManager = aManager;
  if (mCompositableHost && mCompositor) {
    mCompositableHost->SetCompositor(mCompositor);
  }
}

LayerRenderState
CanvasLayerComposite::GetRenderState()
{
  if (mDestroyed || !mCompositableHost || !mCompositableHost->IsAttached()) {
    return LayerRenderState();
  }
  return mCompositableHost->GetRenderState();
}

void
CanvasLayerComposite::RenderLayer(const IntRect& aClipRect)
{
  if (!mCompositableHost || !mCompositableHost->IsAttached()) {
    return;
  }

  mCompositor->MakeCurrent();

#ifdef MOZ_DUMP_PAINTING
  if (gfxUtils::sDumpPainting) {
    RefPtr<gfx::DataSourceSurface> surf = mCompositableHost->GetAsSurface();
    WriteSnapshotToDumpFile(this, surf);
  }
#endif

  EffectChain effectChain(this);
  AddBlendModeEffect(effectChain);

  LayerManagerComposite::AutoAddMaskEffect autoMaskEffect(mMaskLayer, effectChain);
  gfx::Rect clipRect(aClipRect.x, aClipRect.y, aClipRect.width, aClipRect.height);

  mCompositableHost->Composite(effectChain,
                        GetEffectiveOpacity(),
                        GetEffectiveTransform(),
                        GetEffectFilter(),
                        clipRect);
  mCompositableHost->BumpFlashCounter();
}

CompositableHost*
CanvasLayerComposite::GetCompositableHost()
{
  if (mCompositableHost && mCompositableHost->IsAttached()) {
    return mCompositableHost.get();
  }

  return nullptr;
}

void
CanvasLayerComposite::CleanupResources()
{
  if (mCompositableHost) {
    mCompositableHost->Detach(this);
  }
  mCompositableHost = nullptr;
}

gfx::Filter
CanvasLayerComposite::GetEffectFilter()
{
  GraphicsFilter filter = mFilter;
#ifdef ANDROID
  // Bug 691354
  // Using the LINEAR filter we get unexplained artifacts.
  // Use NEAREST when no scaling is required.
  Matrix matrix;
  bool is2D = GetEffectiveTransform().Is2D(&matrix);
  if (is2D && !ThebesMatrix(matrix).HasNonTranslationOrFlip()) {
    filter = GraphicsFilter::FILTER_NEAREST;
  }
#endif
  return gfx::ToFilter(filter);
}

void
CanvasLayerComposite::GenEffectChain(EffectChain& aEffect)
{
  aEffect.mLayerRef = this;
  aEffect.mPrimaryEffect = mCompositableHost->GenEffect(GetEffectFilter());
}

void
CanvasLayerComposite::PrintInfo(std::stringstream& aStream, const char* aPrefix)
{
  CanvasLayer::PrintInfo(aStream, aPrefix);
  aStream << "\n";
  if (mCompositableHost && mCompositableHost->IsAttached()) {
    nsAutoCString pfx(aPrefix);
    pfx += "  ";
    mCompositableHost->PrintInfo(aStream, pfx.get());
  }
}

}
}
