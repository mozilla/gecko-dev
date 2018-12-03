/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CLIENTCANVASLAYER_H
#define GFX_CLIENTCANVASLAYER_H

#include "ClientCanvasRenderer.h"
#include "ClientLayerManager.h"             // for ClientLayerManager, etc
#include "Layers.h"                         // for CanvasLayer, etc
#include "mozilla/Attributes.h"             // for override
#include "mozilla/layers/CanvasClient.h"    // for CanvasClient, etc
#include "mozilla/layers/LayersMessages.h"  // for CanvasLayerAttributes, etc
#include "mozilla/mozalloc.h"               // for operator delete
#include "nsDebug.h"                        // for NS_ASSERTION
#include "nsISupportsImpl.h"                // for MOZ_COUNT_CTOR, etc

namespace mozilla {
namespace layers {

class CompositableClient;
class ShadowableLayer;

class ClientCanvasLayer : public CanvasLayer, public ClientLayer {
 public:
  explicit ClientCanvasLayer(ClientLayerManager* aLayerManager)
      : CanvasLayer(aLayerManager, static_cast<ClientLayer*>(this)) {
    MOZ_COUNT_CTOR(ClientCanvasLayer);
  }

  CanvasRenderer* CreateCanvasRendererInternal() override;

 protected:
  virtual ~ClientCanvasLayer();

 public:
  virtual void SetVisibleRegion(const LayerIntRegion& aRegion) override {
    NS_ASSERTION(ClientManager()->InConstruction(),
                 "Can only set properties in construction phase");
    CanvasLayer::SetVisibleRegion(aRegion);
  }

  virtual void RenderLayer() override;

  virtual void ClearCachedResources() override {
    mCanvasRenderer->ClearCachedResources();
  }

  virtual void HandleMemoryPressure() override {
    mCanvasRenderer->ClearCachedResources();
  }

  virtual void FillSpecificAttributes(
      SpecificLayerAttributes& aAttrs) override {
    aAttrs = CanvasLayerAttributes(mSamplingFilter, mBounds);
  }

  virtual Layer* AsLayer() override { return this; }
  virtual ShadowableLayer* AsShadowableLayer() override { return this; }

  virtual void Disconnect() override { mCanvasRenderer->Destroy(); }

  virtual CompositableClient* GetCompositableClient() override {
    ClientCanvasRenderer* canvasRenderer =
        mCanvasRenderer->AsClientCanvasRenderer();
    MOZ_ASSERT(canvasRenderer);
    return canvasRenderer->GetCanvasClient();
  }

 protected:
  ClientLayerManager* ClientManager() {
    return static_cast<ClientLayerManager*>(mManager);
  }
};

}  // namespace layers
}  // namespace mozilla

#endif
