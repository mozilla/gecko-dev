/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsICanvasRenderingContextInternal_h___
#define nsICanvasRenderingContextInternal_h___

#include "mozilla/gfx/2D.h"
#include "nsISupports.h"
#include "nsIInputStream.h"
#include "nsIDocShell.h"
#include "nsRefreshDriver.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "GraphicsFilter.h"
#include "mozilla/RefPtr.h"

#define NS_ICANVASRENDERINGCONTEXTINTERNAL_IID \
{ 0x3cc9e801, 0x1806, 0x4ff6, \
  { 0x86, 0x14, 0xf9, 0xd0, 0xf4, 0xfb, 0x3b, 0x08 } }

class gfxASurface;
class nsDisplayListBuilder;

namespace mozilla {
namespace layers {
class CanvasLayer;
class LayerManager;
}
namespace gfx {
class SourceSurface;
}
}

class nsICanvasRenderingContextInternal :
  public nsISupports,
  public nsAPostRefreshObserver
{
public:
  typedef mozilla::layers::CanvasLayer CanvasLayer;
  typedef mozilla::layers::LayerManager LayerManager;

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICANVASRENDERINGCONTEXTINTERNAL_IID)

  void SetCanvasElement(mozilla::dom::HTMLCanvasElement* aParentCanvas)
  {
    RemovePostRefreshObserver();
    mCanvasElement = aParentCanvas;
    AddPostRefreshObserverIfNecessary();
  }

  virtual nsIPresShell *GetPresShell() {
    if (mCanvasElement) {
      return mCanvasElement->OwnerDoc()->GetShell();
    }
    return nullptr;
  }

  void RemovePostRefreshObserver()
  {
    if (mRefreshDriver) {
      mRefreshDriver->RemovePostRefreshObserver(this);
      mRefreshDriver = nullptr;
    }
  }

  void AddPostRefreshObserverIfNecessary()
  {
    if (!GetPresShell() ||
        !GetPresShell()->GetPresContext() ||
        !GetPresShell()->GetPresContext()->RefreshDriver()) {
      return;
    }
    mRefreshDriver = GetPresShell()->GetPresContext()->RefreshDriver();
    mRefreshDriver->AddPostRefreshObserver(this);
  }

  mozilla::dom::HTMLCanvasElement* GetParentObject() const
  {
    return mCanvasElement;
  }

#ifdef DEBUG
    // Useful for testing
    virtual int32_t GetWidth() const = 0;
    virtual int32_t GetHeight() const = 0;
#endif

  // Sets the dimensions of the canvas, in pixels.  Called
  // whenever the size of the element changes.
  NS_IMETHOD SetDimensions(int32_t width, int32_t height) = 0;

  NS_IMETHOD InitializeWithSurface(nsIDocShell *docShell, gfxASurface *surface, int32_t width, int32_t height) = 0;

  // Creates an image buffer. Returns null on failure.
  virtual void GetImageBuffer(uint8_t** aImageBuffer, int32_t* aFormat) = 0;

  // Gives you a stream containing the image represented by this context.
  // The format is given in aMimeTime, for example "image/png".
  //
  // If the image format does not support transparency or aIncludeTransparency
  // is false, alpha will be discarded and the result will be the image
  // composited on black.
  NS_IMETHOD GetInputStream(const char *aMimeType,
                            const char16_t *aEncoderOptions,
                            nsIInputStream **aStream) = 0;
  
  // This gets an Azure SourceSurface for the canvas, this will be a snapshot
  // of the canvas at the time it was called.
  // If aPremultAlpha is provided, then it assumed the callee can handle
  // un-premultiplied surfaces, and *aPremultAlpha will be set to false
  // if one is returned.
  virtual mozilla::TemporaryRef<mozilla::gfx::SourceSurface> GetSurfaceSnapshot(bool* aPremultAlpha = nullptr) = 0;

  // If this context is opaque, the backing store of the canvas should
  // be created as opaque; all compositing operators should assume the
  // dst alpha is always 1.0.  If this is never called, the context
  // defaults to false (not opaque).
  NS_IMETHOD SetIsOpaque(bool isOpaque) = 0;
  virtual bool GetIsOpaque() = 0;

  // Invalidate this context and release any held resources, in preperation
  // for possibly reinitializing with SetDimensions/InitializeWithSurface.
  NS_IMETHOD Reset() = 0;

  // Return the CanvasLayer for this context, creating
  // one for the given layer manager if not available.
  virtual already_AddRefed<CanvasLayer> GetCanvasLayer(nsDisplayListBuilder* aBuilder,
                                                       CanvasLayer *aOldLayer,
                                                       LayerManager *aManager) = 0;

  // Return true if the canvas should be forced to be "inactive" to ensure
  // it can be drawn to the screen even if it's too large to be blitted by
  // an accelerated CanvasLayer.
  virtual bool ShouldForceInactiveLayer(LayerManager *aManager) { return false; }

  virtual void MarkContextClean() = 0;

  // Redraw the dirty rectangle of this canvas.
  NS_IMETHOD Redraw(const gfxRect &dirty) = 0;

  NS_IMETHOD SetContextOptions(JSContext* aCx, JS::Handle<JS::Value> aOptions) { return NS_OK; }

  // return true and fills in the bounding rect if elementis a child and has a hit region.
  virtual bool GetHitRegionRect(mozilla::dom::Element* aElement, nsRect& aRect) { return false; }

  // Given a point, return hit region ID if it exists or an empty string if it doesn't
  virtual nsString GetHitRegion(const mozilla::gfx::Point& aPoint) { return nsString(); }

  //
  // shmem support
  //

  // If this context can be set to use Mozilla's Shmem segments as its backing
  // store, this will set it to that state. Note that if you have drawn
  // anything into this canvas before changing the shmem state, it will be
  // lost.
  NS_IMETHOD SetIsIPC(bool isIPC) = 0;

protected:
  nsRefPtr<mozilla::dom::HTMLCanvasElement> mCanvasElement;
  nsRefPtr<nsRefreshDriver> mRefreshDriver;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsICanvasRenderingContextInternal,
                              NS_ICANVASRENDERINGCONTEXTINTERNAL_IID)

#endif /* nsICanvasRenderingContextInternal_h___ */
