/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrGpu_DEFINED
#define GrGpu_DEFINED

#include "GrCaps.h"
#include "GrGpuCommandBuffer.h"
#include "GrProgramDesc.h"
#include "GrSwizzle.h"
#include "GrAllocator.h"
#include "GrTextureProducer.h"
#include "GrTypes.h"
#include "GrXferProcessor.h"
#include "SkPath.h"
#include "SkTArray.h"
#include <map>

class GrBackendRenderTarget;
class GrBackendSemaphore;
class GrBuffer;
class GrContext;
struct GrContextOptions;
class GrGLContext;
class GrMesh;
class GrPath;
class GrPathRenderer;
class GrPathRendererChain;
class GrPathRendering;
class GrPipeline;
class GrPrimitiveProcessor;
class GrRenderTarget;
class GrSemaphore;
class GrStencilAttachment;
class GrStencilSettings;
class GrSurface;
class GrTexture;
class SkJSONWriter;

class GrGpu : public SkRefCnt {
public:
    GrGpu(GrContext* context);
    ~GrGpu() override;

    GrContext* getContext() { return fContext; }
    const GrContext* getContext() const { return fContext; }

    /**
     * Gets the capabilities of the draw target.
     */
    const GrCaps* caps() const { return fCaps.get(); }
    sk_sp<const GrCaps> refCaps() const { return fCaps; }

    GrPathRendering* pathRendering() { return fPathRendering.get();  }

    enum class DisconnectType {
        // No cleanup should be attempted, immediately cease making backend API calls
        kAbandon,
        // Free allocated resources (not known by GrResourceCache) before returning and
        // ensure no backend backend 3D API calls will be made after disconnect() returns.
        kCleanup,
    };

    // Called by GrContext when the underlying backend context is already or will be destroyed
    // before GrContext.
    virtual void disconnect(DisconnectType);

    /**
     * The GrGpu object normally assumes that no outsider is setting state
     * within the underlying 3D API's context/device/whatever. This call informs
     * the GrGpu that the state was modified and it shouldn't make assumptions
     * about the state.
     */
    void markContextDirty(uint32_t state = kAll_GrBackendState) { fResetBits |= state; }

    /**
     * Creates a texture object. If kRenderTarget_GrSurfaceFlag the texture can
     * be used as a render target by calling GrTexture::asRenderTarget(). Not all
     * pixel configs can be used as render targets. Support for configs as textures
     * or render targets can be checked using GrCaps.
     *
     * @param desc           describes the texture to be created.
     * @param budgeted       does this texture count against the resource cache budget?
     * @param texels         array of mipmap levels containing texel data to load.
     *                       Each level begins with full-size palette data for paletted textures.
     *                       It contains width*height texels. If there is only one
     *                       element and it contains nullptr fPixels, texture data is
     *                       uninitialized.
     * @param mipLevelCount  the number of levels in 'texels'
     * @return  The texture object if successful, otherwise nullptr.
     */
    sk_sp<GrTexture> createTexture(const GrSurfaceDesc&, SkBudgeted, const GrMipLevel texels[],
                                   int mipLevelCount);

    /**
     * Simplified createTexture() interface for when there is no initial texel data to upload.
     */
    sk_sp<GrTexture> createTexture(const GrSurfaceDesc& desc, SkBudgeted);

    /**
     * Implements GrResourceProvider::wrapBackendTexture
     */
    sk_sp<GrTexture> wrapBackendTexture(const GrBackendTexture&, GrWrapOwnership);

    /**
     * Implements GrResourceProvider::wrapRenderableBackendTexture
     */
    sk_sp<GrTexture> wrapRenderableBackendTexture(const GrBackendTexture&,
                                                  int sampleCnt, GrWrapOwnership);

    /**
     * Implements GrResourceProvider::wrapBackendRenderTarget
     */
    sk_sp<GrRenderTarget> wrapBackendRenderTarget(const GrBackendRenderTarget&);

    /**
     * Implements GrResourceProvider::wrapBackendTextureAsRenderTarget
     */
    sk_sp<GrRenderTarget> wrapBackendTextureAsRenderTarget(const GrBackendTexture&,
                                                           int sampleCnt);

    /**
     * Creates a buffer in GPU memory. For a client-side buffer use GrBuffer::CreateCPUBacked.
     *
     * @param size            size of buffer to create.
     * @param intendedType    hint to the graphics subsystem about what the buffer will be used for.
     * @param accessPattern   hint to the graphics subsystem about how the data will be accessed.
     * @param data            optional data with which to initialize the buffer.
     *
     * @return the buffer if successful, otherwise nullptr.
     */
    GrBuffer* createBuffer(size_t size, GrBufferType intendedType, GrAccessPattern accessPattern,
                           const void* data = nullptr);

    /**
     * Resolves MSAA.
     */
    void resolveRenderTarget(GrRenderTarget*);

    /**
     * Uses the base of the texture to recompute the contents of the other levels.
     */
    bool regenerateMipMapLevels(GrTexture*);

    /**
     * Reads a rectangle of pixels from a render target. No sRGB/linear conversions are performed.
     *
     * @param surface       The surface to read from
     * @param left          left edge of the rectangle to read (inclusive)
     * @param top           top edge of the rectangle to read (inclusive)
     * @param width         width of rectangle to read in pixels.
     * @param height        height of rectangle to read in pixels.
     * @param dstColorType  the color type of the destination buffer.
     * @param buffer        memory to read the rectangle into.
     * @param rowBytes      the number of bytes between consecutive rows. Zero
     *                      means rows are tightly packed.
     * @param invertY       buffer should be populated bottom-to-top as opposed
     *                      to top-to-bottom (skia's usual order)
     *
     * @return true if the read succeeded, false if not. The read can fail
     *              because of a unsupported pixel config or because no render
     *              target is currently set.
     */
    bool readPixels(GrSurface* surface, int left, int top, int width, int height,
                    GrColorType dstColorType, void* buffer, size_t rowBytes);

    /**
     * Updates the pixels in a rectangle of a surface.  No sRGB/linear conversions are performed.
     *
     * @param surface       The surface to write to.
     * @param left          left edge of the rectangle to write (inclusive)
     * @param top           top edge of the rectangle to write (inclusive)
     * @param width         width of rectangle to write in pixels.
     * @param height        height of rectangle to write in pixels.
     * @param srcColorType  the color type of the source buffer.
     * @param texels        array of mipmap levels containing texture data
     * @param mipLevelCount number of levels in 'texels'
     */
    bool writePixels(GrSurface* surface, int left, int top, int width, int height,
                     GrColorType srcColorType, const GrMipLevel texels[], int mipLevelCount);

    /**
     * Helper for the case of a single level.
     */
    bool writePixels(GrSurface* surface, int left, int top, int width, int height,
                     GrColorType srcColorType, const void* buffer, size_t rowBytes) {
        GrMipLevel mipLevel = {buffer, rowBytes};
        return this->writePixels(surface, left, top, width, height, srcColorType, &mipLevel, 1);
    }

    /**
     * Updates the pixels in a rectangle of a texture using a buffer
     *
     * There are a couple of assumptions here. First, we only update the top miplevel.
     * And second, that any y flip needed has already been done in the buffer.
     *
     * @param texture          The texture to write to.
     * @param left             left edge of the rectangle to write (inclusive)
     * @param top              top edge of the rectangle to write (inclusive)
     * @param width            width of rectangle to write in pixels.
     * @param height           height of rectangle to write in pixels.
     * @param bufferColorType  the color type of the transfer buffer's pixel data
     * @param transferBuffer   GrBuffer to read pixels from (type must be "kXferCpuToGpu")
     * @param offset           offset from the start of the buffer
     * @param rowBytes         number of bytes between consecutive rows in the buffer. Zero
     *                         means rows are tightly packed.
     */
    bool transferPixels(GrTexture* texture, int left, int top, int width, int height,
                        GrColorType bufferColorType, GrBuffer* transferBuffer, size_t offset,
                        size_t rowBytes);

    // After the client interacts directly with the 3D context state the GrGpu
    // must resync its internal state and assumptions about 3D context state.
    // Each time this occurs the GrGpu bumps a timestamp.
    // state of the 3D context
    // At 10 resets / frame and 60fps a 64bit timestamp will overflow in about
    // a billion years.
    typedef uint64_t ResetTimestamp;

    // This timestamp is always older than the current timestamp
    static const ResetTimestamp kExpiredTimestamp = 0;
    // Returns a timestamp based on the number of times the context was reset.
    // This timestamp can be used to lazily detect when cached 3D context state
    // is dirty.
    ResetTimestamp getResetTimestamp() const { return fResetTimestamp; }

    // Called to perform a surface to surface copy. Fallbacks to issuing a draw from the src to dst
    // take place at the GrOpList level and this function implement faster copy paths. The rect
    // and point are pre-clipped. The src rect and implied dst rect are guaranteed to be within the
    // src/dst bounds and non-empty. If canDiscardOutsideDstRect is set to true then we don't need
    // to preserve any data on the dst surface outside of the copy.
    bool copySurface(GrSurface* dst, GrSurfaceOrigin dstOrigin,
                     GrSurface* src, GrSurfaceOrigin srcOrigin,
                     const SkIRect& srcRect,
                     const SkIPoint& dstPoint,
                     bool canDiscardOutsideDstRect = false);

    // Returns a GrGpuRTCommandBuffer which GrOpLists send draw commands to instead of directly
    // to the Gpu object.
    virtual GrGpuRTCommandBuffer* getCommandBuffer(
            GrRenderTarget*, GrSurfaceOrigin,
            const GrGpuRTCommandBuffer::LoadAndStoreInfo&,
            const GrGpuRTCommandBuffer::StencilLoadAndStoreInfo&) = 0;

    // Returns a GrGpuTextureCommandBuffer which GrOpLists send texture commands to instead of
    // directly to the Gpu object.
    virtual GrGpuTextureCommandBuffer* getCommandBuffer(GrTexture*, GrSurfaceOrigin) = 0;

    // Called by GrDrawingManager when flushing.
    // Provides a hook for post-flush actions (e.g. Vulkan command buffer submits). This will also
    // insert any numSemaphore semaphores on the gpu and set the backendSemaphores to match the
    // inserted semaphores.
    GrSemaphoresSubmitted finishFlush(int numSemaphores, GrBackendSemaphore backendSemaphores[]);

    virtual void submit(GrGpuCommandBuffer*) = 0;

    virtual GrFence SK_WARN_UNUSED_RESULT insertFence() = 0;
    virtual bool waitFence(GrFence, uint64_t timeout = 1000) = 0;
    virtual void deleteFence(GrFence) const = 0;

    virtual sk_sp<GrSemaphore> SK_WARN_UNUSED_RESULT makeSemaphore(bool isOwned = true) = 0;
    virtual sk_sp<GrSemaphore> wrapBackendSemaphore(const GrBackendSemaphore& semaphore,
                                                    GrResourceProvider::SemaphoreWrapType wrapType,
                                                    GrWrapOwnership ownership) = 0;
    virtual void insertSemaphore(sk_sp<GrSemaphore> semaphore, bool flush = false) = 0;
    virtual void waitSemaphore(sk_sp<GrSemaphore> semaphore) = 0;

    /**
     *  Put this texture in a safe and known state for use across multiple GrContexts. Depending on
     *  the backend, this may return a GrSemaphore. If so, other contexts should wait on that
     *  semaphore before using this texture.
     */
    virtual sk_sp<GrSemaphore> prepareTextureForCrossContextUsage(GrTexture*) = 0;

    ///////////////////////////////////////////////////////////////////////////
    // Debugging and Stats

    class Stats {
    public:
#if GR_GPU_STATS
        Stats() { this->reset(); }

        void reset() {
            fRenderTargetBinds = 0;
            fShaderCompilations = 0;
            fTextureCreates = 0;
            fTextureUploads = 0;
            fTransfersToTexture = 0;
            fStencilAttachmentCreates = 0;
            fNumDraws = 0;
            fNumFailedDraws = 0;
        }

        int renderTargetBinds() const { return fRenderTargetBinds; }
        void incRenderTargetBinds() { fRenderTargetBinds++; }
        int shaderCompilations() const { return fShaderCompilations; }
        void incShaderCompilations() { fShaderCompilations++; }
        int textureCreates() const { return fTextureCreates; }
        void incTextureCreates() { fTextureCreates++; }
        int textureUploads() const { return fTextureUploads; }
        void incTextureUploads() { fTextureUploads++; }
        int transfersToTexture() const { return fTransfersToTexture; }
        void incTransfersToTexture() { fTransfersToTexture++; }
        void incStencilAttachmentCreates() { fStencilAttachmentCreates++; }
        void incNumDraws() { fNumDraws++; }
        void incNumFailedDraws() { ++fNumFailedDraws; }
        void dump(SkString*);
        void dumpKeyValuePairs(SkTArray<SkString>* keys, SkTArray<double>* values);
        int numDraws() const { return fNumDraws; }
        int numFailedDraws() const { return fNumFailedDraws; }
    private:
        int fRenderTargetBinds;
        int fShaderCompilations;
        int fTextureCreates;
        int fTextureUploads;
        int fTransfersToTexture;
        int fStencilAttachmentCreates;
        int fNumDraws;
        int fNumFailedDraws;
#else
        void dump(SkString*) {}
        void dumpKeyValuePairs(SkTArray<SkString>*, SkTArray<double>*) {}
        void incRenderTargetBinds() {}
        void incShaderCompilations() {}
        void incTextureCreates() {}
        void incTextureUploads() {}
        void incTransfersToTexture() {}
        void incStencilAttachmentCreates() {}
        void incNumDraws() {}
        void incNumFailedDraws() {}
#endif
    };

    Stats* stats() { return &fStats; }
    void dumpJSON(SkJSONWriter*) const;

#if GR_TEST_UTILS
    GrBackendTexture createTestingOnlyBackendTexture(const void* pixels, int w, int h,
                                                     SkColorType, bool isRenderTarget,
                                                     GrMipMapped, size_t rowBytes = 0);

    /** Creates a texture directly in the backend API without wrapping it in a GrTexture. This is
        only to be used for testing (particularly for testing the methods that import an externally
        created texture into Skia. Must be matched with a call to deleteTestingOnlyTexture(). */
    virtual GrBackendTexture createTestingOnlyBackendTexture(const void* pixels, int w, int h,
                                                             GrColorType, bool isRenderTarget,
                                                             GrMipMapped, size_t rowBytes = 0) = 0;

    /** Check a handle represents an actual texture in the backend API that has not been freed. */
    virtual bool isTestingOnlyBackendTexture(const GrBackendTexture&) const = 0;
    /**
     * Frees a texture created by createTestingOnlyBackendTexture(). If ownership of the backend
     * texture has been transferred to a GrContext using adopt semantics this should not be called.
     */
    virtual void deleteTestingOnlyBackendTexture(const GrBackendTexture&) = 0;

    virtual GrBackendRenderTarget createTestingOnlyBackendRenderTarget(int w, int h,
                                                                       GrColorType) = 0;

    virtual void deleteTestingOnlyBackendRenderTarget(const GrBackendRenderTarget&) = 0;

    // This is only to be used in GL-specific tests.
    virtual const GrGLContext* glContextForTesting() const { return nullptr; }

    // This is only to be used by testing code
    virtual void resetShaderCacheForTesting() const {}

    /**
     * Flushes all work to the gpu and forces the GPU to wait until all the gpu work has completed.
     * This is for testing purposes only.
     */
    virtual void testingOnly_flushGpuAndSync() = 0;
#endif

    // width and height may be larger than rt (if underlying API allows it).
    // Returns nullptr if compatible sb could not be created, otherwise the caller owns the ref on
    // the GrStencilAttachment.
    virtual GrStencilAttachment* createStencilAttachmentForRenderTarget(const GrRenderTarget*,
                                                                        int width,
                                                                        int height) = 0;

    // Determines whether a texture will need to be rescaled in order to be used with the
    // GrSamplerState.
    static bool IsACopyNeededForRepeatWrapMode(const GrCaps*, GrTextureProxy* texProxy,
                                               int width, int height,
                                               GrSamplerState::Filter,
                                               GrTextureProducer::CopyParams*,
                                               SkScalar scaleAdjust[2]);

    // Determines whether a texture will need to be copied because the draw requires mips but the
    // texutre doesn't have any. This call should be only checked if IsACopyNeededForTextureParams
    // fails. If the previous call succeeds, then a copy should be done using those params and the
    // mip mapping requirements will be handled there.
    static bool IsACopyNeededForMips(const GrCaps* caps, const GrTextureProxy* texProxy,
                                     GrSamplerState::Filter filter,
                                     GrTextureProducer::CopyParams* copyParams);

    void handleDirtyContext() {
        if (fResetBits) {
            this->resetContext();
        }
    }

protected:
    // Handles cases where a surface will be updated without a call to flushRenderTarget.
    void didWriteToSurface(GrSurface* surface, GrSurfaceOrigin origin, const SkIRect* bounds,
                           uint32_t mipLevels = 1) const;

    Stats                            fStats;
    std::unique_ptr<GrPathRendering> fPathRendering;
    // Subclass must initialize this in its constructor.
    sk_sp<const GrCaps>              fCaps;

    typedef SkTArray<SkPoint, true> SamplePattern;

private:
    // called when the 3D context state is unknown. Subclass should emit any
    // assumed 3D context state and dirty any state cache.
    virtual void onResetContext(uint32_t resetBits) = 0;

    // Called before certain draws in order to guarantee coherent results from dst reads.
    virtual void xferBarrier(GrRenderTarget*, GrXferBarrierType) = 0;

    // overridden by backend-specific derived class to create objects.
    // Texture size and sample size will have already been validated in base class before
    // onCreateTexture is called.
    virtual sk_sp<GrTexture> onCreateTexture(const GrSurfaceDesc&, SkBudgeted,
                                             const GrMipLevel texels[], int mipLevelCount) = 0;

    virtual sk_sp<GrTexture> onWrapBackendTexture(const GrBackendTexture&, GrWrapOwnership) = 0;
    virtual sk_sp<GrTexture> onWrapRenderableBackendTexture(const GrBackendTexture&,
                                                            int sampleCnt,
                                                            GrWrapOwnership) = 0;
    virtual sk_sp<GrRenderTarget> onWrapBackendRenderTarget(const GrBackendRenderTarget&) = 0;
    virtual sk_sp<GrRenderTarget> onWrapBackendTextureAsRenderTarget(const GrBackendTexture&,
                                                                     int sampleCnt) = 0;
    virtual GrBuffer* onCreateBuffer(size_t size, GrBufferType intendedType, GrAccessPattern,
                                     const void* data) = 0;

    // overridden by backend-specific derived class to perform the surface read
    virtual bool onReadPixels(GrSurface*, int left, int top, int width, int height, GrColorType,
                              void* buffer, size_t rowBytes) = 0;

    // overridden by backend-specific derived class to perform the surface write
    virtual bool onWritePixels(GrSurface*, int left, int top, int width, int height, GrColorType,
                               const GrMipLevel texels[], int mipLevelCount) = 0;

    // overridden by backend-specific derived class to perform the texture transfer
    virtual bool onTransferPixels(GrTexture*, int left, int top, int width, int height,
                                  GrColorType colorType, GrBuffer* transferBuffer, size_t offset,
                                  size_t rowBytes) = 0;

    // overridden by backend-specific derived class to perform the resolve
    virtual void onResolveRenderTarget(GrRenderTarget* target) = 0;

    // overridden by backend specific derived class to perform mip map level regeneration.
    virtual bool onRegenerateMipMapLevels(GrTexture*) = 0;

    // overridden by backend specific derived class to perform the copy surface
    virtual bool onCopySurface(GrSurface* dst, GrSurfaceOrigin dstOrigin,
                               GrSurface* src, GrSurfaceOrigin srcOrigin,
                               const SkIRect& srcRect, const SkIPoint& dstPoint,
                               bool canDiscardOutsideDstRect) = 0;

    virtual void onFinishFlush(bool insertedSemaphores) = 0;

#ifdef SK_ENABLE_DUMP_GPU
    virtual void onDumpJSON(SkJSONWriter*) const {}
#endif

    void resetContext() {
        this->onResetContext(fResetBits);
        fResetBits = 0;
        ++fResetTimestamp;
    }

    ResetTimestamp fResetTimestamp;
    uint32_t fResetBits;
    // The context owns us, not vice-versa, so this ptr is not ref'ed by Gpu.
    GrContext* fContext;

    friend class GrPathRendering;
    typedef SkRefCnt INHERITED;
};

#endif
