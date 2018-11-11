/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrMtlGpuCommandBuffer_DEFINED
#define GrMtlGpuCommandBuffer_DEFINED

#include "GrGpuCommandBuffer.h"
#include "GrMtlGpu.h"
#include "GrMesh.h"

#import <metal/metal.h>

typedef uint32_t GrColor;
class GrMtlPipelineState;
class GrMtlRenderTarget;

class GrMtlGpuTextureCommandBuffer : public GrGpuTextureCommandBuffer {
public:
    GrMtlGpuTextureCommandBuffer(GrMtlGpu* gpu, GrTexture* texture, GrSurfaceOrigin origin)
            : INHERITED(texture, origin)
            , fGpu(gpu) {
    }

    ~GrMtlGpuTextureCommandBuffer() override {}

    void copy(GrSurface* src, GrSurfaceOrigin srcOrigin, const SkIRect& srcRect,
              const SkIPoint& dstPoint) override {
        fGpu->copySurface(fTexture, fOrigin, src, srcOrigin, srcRect, dstPoint);
    }

    void insertEventMarker(const char* msg) override {}

private:
    GrMtlGpu* fGpu;

    typedef GrGpuTextureCommandBuffer INHERITED;
};

class GrMtlGpuRTCommandBuffer : public GrGpuRTCommandBuffer, private GrMesh::SendToGpuImpl {
public:
    GrMtlGpuRTCommandBuffer(GrMtlGpu* gpu, GrRenderTarget* rt, GrSurfaceOrigin origin,
                            const GrGpuRTCommandBuffer::LoadAndStoreInfo& colorInfo,
                            const GrGpuRTCommandBuffer::StencilLoadAndStoreInfo& stencilInfo);

    ~GrMtlGpuRTCommandBuffer() override;

    void begin() override {}
    void end() override {}

    void discard() override {}

    void insertEventMarker(const char* msg) override {}

    void inlineUpload(GrOpFlushState* state, GrDeferredTextureUploadFn& upload) override {}

    void copy(GrSurface* src, GrSurfaceOrigin srcOrigin, const SkIRect& srcRect,
              const SkIPoint& dstPoint) override;

    void submit();

private:
    void internalBegin();
    void internalEnd();

    GrGpu* gpu() override { return fGpu; }

    GrMtlPipelineState* prepareDrawState(
            const GrPrimitiveProcessor& primProc,
            const GrPipeline& pipeline,
            const GrPipeline::FixedDynamicState* fixedDynamicState,
            const GrMesh meshes[],
            int meshCount);

    void onDraw(const GrPrimitiveProcessor& primProc,
                const GrPipeline& pipeline,
                const GrPipeline::FixedDynamicState* fixedDynamicState,
                const GrPipeline::DynamicStateArrays* dynamicStateArrays,
                const GrMesh mesh[],
                int meshCount,
                const SkRect& bounds) override;

    void onClear(const GrFixedClip& clip, GrColor color) override {}

    void onClearStencilClip(const GrFixedClip& clip, bool insideStencilMask) override;

    MTLRenderPassDescriptor* createRenderPassDesc() const;

    void bindGeometry(const GrBuffer* vertexBuffer, const GrBuffer* instanceBuffer);

    // GrMesh::SendToGpuImpl methods. These issue the actual Metal draw commands.
    // Marked final as a hint to the compiler to not use virtual dispatch.
    void sendMeshToGpu(GrPrimitiveType primType, const GrBuffer* vertexBuffer, int vertexCount,
                       int baseVertex) final {
        this->sendInstancedMeshToGpu(primType, vertexBuffer, vertexCount, baseVertex, nullptr, 1,
                                     0);
    }

    void sendIndexedMeshToGpu(GrPrimitiveType primType, const GrBuffer* indexBuffer, int indexCount,
                              int baseIndex, uint16_t /*minIndexValue*/, uint16_t /*maxIndexValue*/,
                              const GrBuffer* vertexBuffer, int baseVertex,
                              GrPrimitiveRestart restart) final {
        SkASSERT(restart == GrPrimitiveRestart::kNo);
        this->sendIndexedInstancedMeshToGpu(primType, indexBuffer, indexCount, baseIndex,
                                            vertexBuffer, baseVertex, nullptr, 1, 0,
                                            GrPrimitiveRestart::kNo);
    }

    void sendInstancedMeshToGpu(GrPrimitiveType, const GrBuffer* vertexBuffer, int vertexCount,
                                int baseVertex, const GrBuffer* instanceBuffer, int instanceCount,
                                int baseInstance) final;

    void sendIndexedInstancedMeshToGpu(GrPrimitiveType, const GrBuffer* indexBuffer, int indexCount,
                                       int baseIndex, const GrBuffer* vertexBuffer, int baseVertex,
                                       const GrBuffer* instanceBuffer, int instanceCount,
                                       int baseInstance, GrPrimitiveRestart) final;

    GrMtlGpu*                                     fGpu;
    GrGpuRTCommandBuffer::LoadAndStoreInfo        fColorLoadAndStoreInfo;
    GrGpuRTCommandBuffer::StencilLoadAndStoreInfo fStencilLoadAndStoreInfo;

    id<MTLRenderCommandEncoder> fActiveRenderCmdEncoder;
    MTLRenderPassDescriptor* fRenderPassDesc;

    struct CommandBufferInfo {
        SkRect fBounds;
    };

    CommandBufferInfo fCommandBufferInfo;

    typedef GrGpuRTCommandBuffer INHERITED;
};

#endif

