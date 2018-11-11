/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrRenderTargetProxy.h"

#include "GrCaps.h"
#include "GrGpuResourcePriv.h"
#include "GrRenderTargetOpList.h"
#include "GrRenderTargetPriv.h"
#include "GrResourceProvider.h"
#include "GrSurfacePriv.h"
#include "GrTextureRenderTargetProxy.h"
#include "SkMathPriv.h"

// Deferred version
// TODO: we can probably munge the 'desc' in both the wrapped and deferred
// cases to make the sampleConfig/numSamples stuff more rational.
GrRenderTargetProxy::GrRenderTargetProxy(const GrCaps& caps, const GrSurfaceDesc& desc,
                                         GrSurfaceOrigin origin, SkBackingFit fit,
                                         SkBudgeted budgeted, GrInternalSurfaceFlags surfaceFlags)
        : INHERITED(desc, origin, fit, budgeted, surfaceFlags)
        , fSampleCnt(desc.fSampleCnt)
        , fNeedsStencil(false) {
    // Since we know the newly created render target will be internal, we are able to precompute
    // what the flags will ultimately end up being.
    if (caps.usesMixedSamples() && fSampleCnt > 1) {
        this->setHasMixedSamples();
    }
    if (caps.maxWindowRectangles() > 0) {
        this->setSupportsWindowRects();
    }
}

// Lazy-callback version
GrRenderTargetProxy::GrRenderTargetProxy(LazyInstantiateCallback&& callback,
                                         LazyInstantiationType lazyType, const GrSurfaceDesc& desc,
                                         GrSurfaceOrigin origin, SkBackingFit fit,
                                         SkBudgeted budgeted, GrInternalSurfaceFlags surfaceFlags)
        : INHERITED(std::move(callback), lazyType, desc, origin, fit, budgeted, surfaceFlags)
        , fSampleCnt(desc.fSampleCnt)
        , fNeedsStencil(false) {
    SkASSERT(SkToBool(kRenderTarget_GrSurfaceFlag & desc.fFlags));
}

// Wrapped version
GrRenderTargetProxy::GrRenderTargetProxy(sk_sp<GrSurface> surf, GrSurfaceOrigin origin)
        : INHERITED(std::move(surf), origin, SkBackingFit::kExact)
        , fSampleCnt(fTarget->asRenderTarget()->numStencilSamples())
        , fNeedsStencil(false) {
}

int GrRenderTargetProxy::maxWindowRectangles(const GrCaps& caps) const {
    return this->supportsWindowRects() ? caps.maxWindowRectangles() : 0;
}

bool GrRenderTargetProxy::instantiate(GrResourceProvider* resourceProvider) {
    if (LazyState::kNot != this->lazyInstantiationState()) {
        return false;
    }
    static constexpr GrSurfaceDescFlags kDescFlags = kRenderTarget_GrSurfaceFlag;

    if (!this->instantiateImpl(resourceProvider, fSampleCnt, fNeedsStencil, kDescFlags,
                               GrMipMapped::kNo, nullptr)) {
        return false;
    }
    SkASSERT(fTarget->asRenderTarget());
    SkASSERT(!fTarget->asTexture());
    return true;
}

sk_sp<GrSurface> GrRenderTargetProxy::createSurface(GrResourceProvider* resourceProvider) const {
    static constexpr GrSurfaceDescFlags kDescFlags = kRenderTarget_GrSurfaceFlag;

    sk_sp<GrSurface> surface = this->createSurfaceImpl(resourceProvider, fSampleCnt, fNeedsStencil,
                                                       kDescFlags, GrMipMapped::kNo);
    if (!surface) {
        return nullptr;
    }
    SkASSERT(surface->asRenderTarget());
    SkASSERT(!surface->asTexture());
    return surface;
}

size_t GrRenderTargetProxy::onUninstantiatedGpuMemorySize() const {
    int colorSamplesPerPixel = this->numColorSamples();
    if (colorSamplesPerPixel > 1) {
        // Add one for the resolve buffer.
        ++colorSamplesPerPixel;
    }

    // TODO: do we have enough information to improve this worst case estimate?
    return GrSurface::ComputeSize(this->config(), this->width(), this->height(),
                                  colorSamplesPerPixel, GrMipMapped::kNo, !this->priv().isExact());
}

bool GrRenderTargetProxy::refsWrappedObjects() const {
    if (!fTarget) {
        return false;
    }

    return fTarget->resourcePriv().refsWrappedObjects();
}

#ifdef SK_DEBUG
void GrRenderTargetProxy::onValidateSurface(const GrSurface* surface) {
    SkASSERT(!surface->asTexture());

    // Anything that is checked here should be duplicated in GrTextureRenderTargetProxy's version
    SkASSERT(surface->asRenderTarget());
    SkASSERT(surface->asRenderTarget()->numStencilSamples() == this->numStencilSamples());

    GrInternalSurfaceFlags proxyFlags = fSurfaceFlags;
    GrInternalSurfaceFlags surfaceFlags = surface->surfacePriv().flags();
    SkASSERT((proxyFlags & GrInternalSurfaceFlags::kRenderTargetMask) ==
             (surfaceFlags & GrInternalSurfaceFlags::kRenderTargetMask));
}
#endif
