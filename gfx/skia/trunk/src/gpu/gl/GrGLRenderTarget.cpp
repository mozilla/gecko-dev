/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrGLRenderTarget.h"

#include "GrGpuGL.h"

#define GPUGL static_cast<GrGpuGL*>(getGpu())

#define GL_CALL(X) GR_GL_CALL(GPUGL->glInterface(), X)

void GrGLRenderTarget::init(const Desc& desc,
                            const GrGLIRect& viewport,
                            GrGLTexID* texID) {
    fRTFBOID                = desc.fRTFBOID;
    fTexFBOID               = desc.fTexFBOID;
    fMSColorRenderbufferID  = desc.fMSColorRenderbufferID;
    fViewport               = viewport;
    fTexIDObj.reset(SkSafeRef(texID));
}

namespace {
GrTextureDesc MakeDesc(GrTextureFlags flags,
                       int width, int height,
                       GrPixelConfig config, int sampleCnt,
                       GrSurfaceOrigin origin) {
    GrTextureDesc temp;
    temp.fFlags = flags;
    temp.fWidth = width;
    temp.fHeight = height;
    temp.fConfig = config;
    temp.fSampleCnt = sampleCnt;
    temp.fOrigin = origin;
    return temp;
}

};

GrGLRenderTarget::GrGLRenderTarget(GrGpuGL* gpu,
                                   const Desc& desc,
                                   const GrGLIRect& viewport,
                                   GrGLTexID* texID,
                                   GrGLTexture* texture)
    : INHERITED(gpu,
                desc.fIsWrapped,
                texture,
                MakeDesc(kNone_GrTextureFlags,
                         viewport.fWidth, viewport.fHeight,
                         desc.fConfig, desc.fSampleCnt,
                         desc.fOrigin)) {
    SkASSERT(NULL != texID);
    SkASSERT(NULL != texture);
    // FBO 0 can't also be a texture, right?
    SkASSERT(0 != desc.fRTFBOID);
    SkASSERT(0 != desc.fTexFBOID);

    // we assume this is true, TODO: get rid of viewport as a param.
    SkASSERT(viewport.fWidth == texture->width());
    SkASSERT(viewport.fHeight == texture->height());

    this->init(desc, viewport, texID);
}

GrGLRenderTarget::GrGLRenderTarget(GrGpuGL* gpu,
                                   const Desc& desc,
                                   const GrGLIRect& viewport)
    : INHERITED(gpu,
                desc.fIsWrapped,
                NULL,
                MakeDesc(kNone_GrTextureFlags,
                         viewport.fWidth, viewport.fHeight,
                         desc.fConfig, desc.fSampleCnt,
                         desc.fOrigin)) {
    this->init(desc, viewport, NULL);
}

void GrGLRenderTarget::onRelease() {
    if (!this->isWrapped()) {
        if (fTexFBOID) {
            GL_CALL(DeleteFramebuffers(1, &fTexFBOID));
        }
        if (fRTFBOID && fRTFBOID != fTexFBOID) {
            GL_CALL(DeleteFramebuffers(1, &fRTFBOID));
        }
        if (fMSColorRenderbufferID) {
            GL_CALL(DeleteRenderbuffers(1, &fMSColorRenderbufferID));
        }
    }
    fRTFBOID                = 0;
    fTexFBOID               = 0;
    fMSColorRenderbufferID  = 0;
    fTexIDObj.reset(NULL);
    INHERITED::onRelease();
}

void GrGLRenderTarget::onAbandon() {
    fRTFBOID                = 0;
    fTexFBOID               = 0;
    fMSColorRenderbufferID  = 0;
    if (NULL != fTexIDObj.get()) {
        fTexIDObj->abandon();
        fTexIDObj.reset(NULL);
    }
    INHERITED::onAbandon();
}
