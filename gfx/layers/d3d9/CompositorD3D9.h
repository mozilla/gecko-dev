/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_COMPOSITORD3D9_H
#define MOZILLA_GFX_COMPOSITORD3D9_H

#include "mozilla/gfx/2D.h"
#include "gfx2DGlue.h"
#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/TextureD3D9.h"
#include "DeviceManagerD3D9.h"

class nsWidget;

namespace mozilla {
namespace layers {

class CompositorD3D9 : public Compositor
{
public:
  CompositorD3D9(PCompositorParent* aParent, nsIWidget *aWidget);
  ~CompositorD3D9();

  virtual bool Initialize() MOZ_OVERRIDE;
  virtual void Destroy() MOZ_OVERRIDE {}

  virtual TextureFactoryIdentifier
    GetTextureFactoryIdentifier() MOZ_OVERRIDE;

  virtual bool CanUseCanvasLayerForSize(const gfx::IntSize &aSize) MOZ_OVERRIDE;
  virtual int32_t GetMaxTextureSize() const MOZ_FINAL;

  virtual void MakeCurrent(MakeCurrentFlags aFlags = 0) MOZ_OVERRIDE {}

  virtual TemporaryRef<CompositingRenderTarget>
    CreateRenderTarget(const gfx::IntRect &aRect,
                       SurfaceInitMode aInit) MOZ_OVERRIDE;

  virtual TemporaryRef<CompositingRenderTarget>
    CreateRenderTargetFromSource(const gfx::IntRect &aRect,
                                 const CompositingRenderTarget *aSource,
                                 const gfx::IntPoint &aSourcePoint) MOZ_OVERRIDE;

  virtual void SetRenderTarget(CompositingRenderTarget *aSurface);
  virtual CompositingRenderTarget* GetCurrentRenderTarget() const MOZ_OVERRIDE
  {
    return mCurrentRT;
  }

  virtual void SetDestinationSurfaceSize(const gfx::IntSize& aSize) MOZ_OVERRIDE {}

  virtual void ClearRect(const gfx::Rect& aRect) MOZ_OVERRIDE;

  virtual void DrawQuad(const gfx::Rect &aRect,
                        const gfx::Rect &aClipRect,
                        const EffectChain &aEffectChain,
                        gfx::Float aOpacity,
                        const gfx::Matrix4x4 &aTransform) MOZ_OVERRIDE;

  virtual void BeginFrame(const nsIntRegion& aInvalidRegion,
                          const gfx::Rect *aClipRectIn,
                          const gfx::Matrix& aTransform,
                          const gfx::Rect& aRenderBounds,
                          gfx::Rect *aClipRectOut = nullptr,
                          gfx::Rect *aRenderBoundsOut = nullptr) MOZ_OVERRIDE;

  virtual void EndFrame() MOZ_OVERRIDE;

  virtual void EndFrameForExternalComposition(const gfx::Matrix& aTransform) MOZ_OVERRIDE {}

  virtual void AbortFrame() MOZ_OVERRIDE {}

  virtual void PrepareViewport(const gfx::IntSize& aSize,
                               const gfx::Matrix& aWorldTransform) MOZ_OVERRIDE;

  virtual bool SupportsPartialTextureUpdate() MOZ_OVERRIDE{ return true; }

#ifdef MOZ_DUMP_PAINTING
  virtual const char* Name() const MOZ_OVERRIDE { return "Direct3D9"; }
#endif

  virtual LayersBackend GetBackendType() const MOZ_OVERRIDE {
    return LayersBackend::LAYERS_D3D9;
  }

  virtual nsIWidget* GetWidget() const MOZ_OVERRIDE { return mWidget; }

  IDirect3DDevice9* device() const
  {
    return mDeviceManager
           ? mDeviceManager->device()
           : nullptr;
  }

  /**
   * Returns true if the Compositor is ready to go.
   * D3D9 devices can be awkward and there is a bunch of logic around
   * resetting/recreating devices and swap chains. That is handled by this method.
   * If we don't have a device and swap chain ready for rendering, we will return
   * false and if necessary destroy the device and/or swap chain. We will also
   * schedule another composite so we get another go at rendering, thus we shouldn't
   * miss a composite due to re-creating a device.
   */
  virtual bool Ready() MOZ_OVERRIDE;

  /**
   * Declare an offset to use when rendering layers. This will be ignored when
   * rendering to a target instead of the screen.
   */
  virtual void SetScreenRenderOffset(const ScreenPoint& aOffset) MOZ_OVERRIDE
  {
    if (aOffset.x || aOffset.y) {
      NS_RUNTIMEABORT("SetScreenRenderOffset not supported by CompositorD3D9.");
    }
    // If the offset is 0, 0 that's okay.
  }

  virtual TemporaryRef<DataTextureSource>
    CreateDataTextureSource(TextureFlags aFlags = TextureFlags::NO_FLAGS) MOZ_OVERRIDE;
private:
  // ensure mSize is up to date with respect to mWidget
  void EnsureSize();
  void SetSamplerForFilter(gfx::Filter aFilter);
  void PaintToTarget();
  void SetMask(const EffectChain &aEffectChain, uint32_t aMaskTexture);
  /**
   * Ensure we have a swap chain and it is ready for rendering.
   * Requires mDeviceManger to be non-null.
   * Returns true if we have a working swap chain; false otherwise.
   * If we cannot create or validate the swap chain due to a bad device manager,
   * then the device will be destroyed and set mDeviceManager to null. We will
   * schedule another composite if it is a good idea to try again or we need to
   * recreate the device.
   */
  bool EnsureSwapChain();

  /**
   * DeviceManagerD3D9 keeps a count of the number of times its device is
   * reset or recreated. We keep a parallel count (mDeviceResetCount). It
   * is possible that we miss a reset if it is 'caused' by another
   * compositor (for another window). In which case we need to invalidate
   * everything and render it all. This method checks the reset counts
   * match and if not invalidates everything (a long comment on that in
   * the cpp file).
   */
  void CheckResetCount();

  void ReportFailure(const nsACString &aMsg, HRESULT aCode);

  virtual gfx::IntSize GetWidgetSize() const MOZ_OVERRIDE
  {
    return gfx::ToIntSize(mSize);
  }

  /* Device manager instance for this compositor */
  nsRefPtr<DeviceManagerD3D9> mDeviceManager;

  /* Swap chain associated with this compositor */
  nsRefPtr<SwapChainD3D9> mSwapChain;

  /* Widget associated with this layer manager */
  nsIWidget *mWidget;

  RefPtr<CompositingRenderTargetD3D9> mDefaultRT;
  RefPtr<CompositingRenderTargetD3D9> mCurrentRT;

  nsIntSize mSize;

  uint32_t mDeviceResetCount;
};

}
}

#endif
