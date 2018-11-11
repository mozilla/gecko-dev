/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrGLProgram.h"
#include "GrAllocator.h"
#include "GrCoordTransform.h"
#include "GrGLBuffer.h"
#include "GrGLGpu.h"
#include "GrGLPathRendering.h"
#include "GrPathProcessor.h"
#include "GrPipeline.h"
#include "GrProcessor.h"
#include "GrTexturePriv.h"
#include "GrXferProcessor.h"
#include "glsl/GrGLSLFragmentProcessor.h"
#include "glsl/GrGLSLGeometryProcessor.h"
#include "glsl/GrGLSLXferProcessor.h"

#define GL_CALL(X) GR_GL_CALL(fGpu->glInterface(), X)
#define GL_CALL_RET(R, X) GR_GL_CALL_RET(fGpu->glInterface(), R, X)

///////////////////////////////////////////////////////////////////////////////////////////////////

GrGLProgram::GrGLProgram(
        GrGLGpu* gpu,
        const GrGLSLBuiltinUniformHandles& builtinUniforms,
        GrGLuint programID,
        const UniformInfoArray& uniforms,
        const UniformInfoArray& textureSamplers,
        const VaryingInfoArray& pathProcVaryings,
        std::unique_ptr<GrGLSLPrimitiveProcessor> geometryProcessor,
        std::unique_ptr<GrGLSLXferProcessor> xferProcessor,
        std::unique_ptr<std::unique_ptr<GrGLSLFragmentProcessor>[]> fragmentProcessors,
        int fragmentProcessorCnt,
        std::unique_ptr<Attribute[]> attributes,
        int vertexAttributeCnt,
        int instanceAttributeCnt,
        int vertexStride,
        int instanceStride)
        : fBuiltinUniformHandles(builtinUniforms)
        , fProgramID(programID)
        , fPrimitiveProcessor(std::move(geometryProcessor))
        , fXferProcessor(std::move(xferProcessor))
        , fFragmentProcessors(std::move(fragmentProcessors))
        , fFragmentProcessorCnt(fragmentProcessorCnt)
        , fAttributes(std::move(attributes))
        , fVertexAttributeCnt(vertexAttributeCnt)
        , fInstanceAttributeCnt(instanceAttributeCnt)
        , fVertexStride(vertexStride)
        , fInstanceStride(instanceStride)
        , fGpu(gpu)
        , fProgramDataManager(gpu, programID, uniforms, pathProcVaryings)
        , fNumTextureSamplers(textureSamplers.count()) {
    // Assign texture units to sampler uniforms one time up front.
    GL_CALL(UseProgram(fProgramID));
    fProgramDataManager.setSamplerUniforms(textureSamplers, 0);
}

GrGLProgram::~GrGLProgram() {
    if (fProgramID) {
        GL_CALL(DeleteProgram(fProgramID));
    }
}

void GrGLProgram::abandon() {
    fProgramID = 0;
}

///////////////////////////////////////////////////////////////////////////////

void GrGLProgram::updateUniformsAndTextureBindings(const GrPrimitiveProcessor& primProc,
                                                   const GrPipeline& pipeline,
                                                   const GrTextureProxy* const primProcTextures[]) {
    this->setRenderTargetState(primProc, pipeline.proxy());

    // we set the textures, and uniforms for installed processors in a generic way, but subclasses
    // of GLProgram determine how to set coord transforms

    // We must bind to texture units in the same order in which we set the uniforms in
    // GrGLProgramDataManager. That is, we bind textures for processors in this order:
    // primProc, fragProcs, XP.
    fPrimitiveProcessor->setData(fProgramDataManager, primProc,
                                 GrFragmentProcessor::CoordTransformIter(pipeline));
    if (primProcTextures) {
        this->updatePrimitiveProcessorTextureBindings(primProc, primProcTextures);
    }
    int nextTexSamplerIdx = primProc.numTextureSamplers();

    this->setFragmentData(pipeline, &nextTexSamplerIdx);

    const GrXferProcessor& xp = pipeline.getXferProcessor();
    SkIPoint offset;
    GrTexture* dstTexture = pipeline.peekDstTexture(&offset);

    fXferProcessor->setData(fProgramDataManager, xp, dstTexture, offset);
    if (dstTexture) {
        fGpu->bindTexture(nextTexSamplerIdx++, GrSamplerState::ClampNearest(),
                          static_cast<GrGLTexture*>(dstTexture));
    }
    SkASSERT(nextTexSamplerIdx == fNumTextureSamplers);
}

void GrGLProgram::updatePrimitiveProcessorTextureBindings(const GrPrimitiveProcessor& primProc,
                                                          const GrTextureProxy* const proxies[]) {
    for (int i = 0; i < primProc.numTextureSamplers(); ++i) {
        auto* tex = static_cast<GrGLTexture*>(proxies[i]->peekTexture());
        fGpu->bindTexture(i, primProc.textureSampler(i).samplerState(), tex);
    }
}

void GrGLProgram::setFragmentData(const GrPipeline& pipeline, int* nextTexSamplerIdx) {
    GrFragmentProcessor::Iter iter(pipeline);
    GrGLSLFragmentProcessor::Iter glslIter(fFragmentProcessors.get(), fFragmentProcessorCnt);
    const GrFragmentProcessor* fp = iter.next();
    GrGLSLFragmentProcessor* glslFP = glslIter.next();
    while (fp && glslFP) {
        glslFP->setData(fProgramDataManager, *fp);
        for (int i = 0; i < fp->numTextureSamplers(); ++i) {
            const GrFragmentProcessor::TextureSampler& sampler = fp->textureSampler(i);
            fGpu->bindTexture((*nextTexSamplerIdx)++, sampler.samplerState(),
                              static_cast<GrGLTexture*>(sampler.peekTexture()));
        }
        fp = iter.next();
        glslFP = glslIter.next();
    }
    SkASSERT(!fp && !glslFP);
}

void GrGLProgram::setRenderTargetState(const GrPrimitiveProcessor& primProc,
                                       const GrRenderTargetProxy* proxy) {
    GrRenderTarget* rt = proxy->peekRenderTarget();
    // Load the RT size uniforms if they are needed
    if (fBuiltinUniformHandles.fRTWidthUni.isValid() &&
        fRenderTargetState.fRenderTargetSize.fWidth != rt->width()) {
        fProgramDataManager.set1f(fBuiltinUniformHandles.fRTWidthUni, SkIntToScalar(rt->width()));
    }
    if (fBuiltinUniformHandles.fRTHeightUni.isValid() &&
        fRenderTargetState.fRenderTargetSize.fHeight != rt->height()) {
        fProgramDataManager.set1f(fBuiltinUniformHandles.fRTHeightUni, SkIntToScalar(rt->height()));
    }

    // set RT adjustment
    SkISize size;
    size.set(rt->width(), rt->height());
    if (!primProc.isPathRendering()) {
        if (fRenderTargetState.fRenderTargetOrigin != proxy->origin() ||
            fRenderTargetState.fRenderTargetSize != size) {
            fRenderTargetState.fRenderTargetSize = size;
            fRenderTargetState.fRenderTargetOrigin = proxy->origin();

            float rtAdjustmentVec[4];
            fRenderTargetState.getRTAdjustmentVec(rtAdjustmentVec);
            fProgramDataManager.set4fv(fBuiltinUniformHandles.fRTAdjustmentUni, 1, rtAdjustmentVec);
        }
    } else {
        SkASSERT(fGpu->glCaps().shaderCaps()->pathRenderingSupport());
        const GrPathProcessor& pathProc = primProc.cast<GrPathProcessor>();
        fGpu->glPathRendering()->setProjectionMatrix(pathProc.viewMatrix(),
                                                     size, proxy->origin());
    }
}
