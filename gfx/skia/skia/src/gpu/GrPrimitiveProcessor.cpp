/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrPrimitiveProcessor.h"

#include "GrCoordTransform.h"

/**
 * We specialize the vertex code for each of these matrix types.
 */
enum MatrixType {
    kNoPersp_MatrixType  = 0,
    kGeneral_MatrixType  = 1,
};

GrPrimitiveProcessor::GrPrimitiveProcessor(ClassID classID) : GrProcessor(classID) {}

const GrPrimitiveProcessor::TextureSampler& GrPrimitiveProcessor::textureSampler(int i) const {
    SkASSERT(i >= 0 && i < this->numTextureSamplers());
    return this->onTextureSampler(i);
}

const GrPrimitiveProcessor::Attribute& GrPrimitiveProcessor::vertexAttribute(int i) const {
    SkASSERT(i >= 0 && i < this->numVertexAttributes());
    const auto& result = this->onVertexAttribute(i);
    SkASSERT(result.isInitialized());
    return result;
}

const GrPrimitiveProcessor::Attribute& GrPrimitiveProcessor::instanceAttribute(int i) const {
    SkASSERT(i >= 0 && i < this->numInstanceAttributes());
    const auto& result = this->onInstanceAttribute(i);
    SkASSERT(result.isInitialized());
    return result;
}

#ifdef SK_DEBUG
size_t GrPrimitiveProcessor::debugOnly_vertexStride() const {
    size_t stride = 0;
    for (int i = 0; i < fVertexAttributeCnt; ++i) {
        stride += this->vertexAttribute(i).sizeAlign4();
    }
    return stride;
}

size_t GrPrimitiveProcessor::debugOnly_instanceStride() const {
    size_t stride = 0;
    for (int i = 0; i < fInstanceAttributeCnt; ++i) {
        stride += this->instanceAttribute(i).sizeAlign4();
    }
    return stride;
}

size_t GrPrimitiveProcessor::debugOnly_vertexAttributeOffset(int i) const {
    SkASSERT(i >= 0 && i < fVertexAttributeCnt);
    size_t offset = 0;
    for (int j = 0; j < i; ++j) {
        offset += this->vertexAttribute(j).sizeAlign4();
    }
    return offset;
}

size_t GrPrimitiveProcessor::debugOnly_instanceAttributeOffset(int i) const {
    SkASSERT(i >= 0 && i < fInstanceAttributeCnt);
    size_t offset = 0;
    for (int j = 0; j < i; ++j) {
        offset += this->instanceAttribute(j).sizeAlign4();
    }
    return offset;
}
#endif

uint32_t
GrPrimitiveProcessor::getTransformKey(const SkTArray<const GrCoordTransform*, true>& coords,
                                      int numCoords) const {
    uint32_t totalKey = 0;
    for (int t = 0; t < numCoords; ++t) {
        uint32_t key = 0;
        const GrCoordTransform* coordTransform = coords[t];
        if (coordTransform->getMatrix().hasPerspective()) {
            key |= kGeneral_MatrixType;
        } else {
            key |= kNoPersp_MatrixType;
        }
        key <<= t;
        SkASSERT(0 == (totalKey & key)); // keys for each transform ought not to overlap
        totalKey |= key;
    }
    return totalKey;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static inline GrSamplerState::Filter clamp_filter(GrTextureType type,
                                                  GrSamplerState::Filter requestedFilter) {
    if (GrTextureTypeHasRestrictedSampling(type)) {
        return SkTMin(requestedFilter, GrSamplerState::Filter::kBilerp);
    }
    return requestedFilter;
}

GrPrimitiveProcessor::TextureSampler::TextureSampler(GrTextureType textureType,
                                                     GrPixelConfig config,
                                                     const GrSamplerState& samplerState) {
    this->reset(textureType, config, samplerState);
}

GrPrimitiveProcessor::TextureSampler::TextureSampler(GrTextureType textureType,
                                                     GrPixelConfig config,
                                                     GrSamplerState::Filter filterMode,
                                                     GrSamplerState::WrapMode wrapXAndY) {
    this->reset(textureType, config, filterMode, wrapXAndY);
}

void GrPrimitiveProcessor::TextureSampler::reset(GrTextureType textureType,
                                                 GrPixelConfig config,
                                                 const GrSamplerState& samplerState) {
    SkASSERT(kUnknown_GrPixelConfig != config);
    fSamplerState = samplerState;
    fSamplerState.setFilterMode(clamp_filter(textureType, samplerState.filter()));
    fTextureType = textureType;
    fConfig = config;
}

void GrPrimitiveProcessor::TextureSampler::reset(GrTextureType textureType,
                                                 GrPixelConfig config,
                                                 GrSamplerState::Filter filterMode,
                                                 GrSamplerState::WrapMode wrapXAndY) {
    SkASSERT(kUnknown_GrPixelConfig != config);
    filterMode = clamp_filter(textureType, filterMode);
    fSamplerState = GrSamplerState(wrapXAndY, filterMode);
    fTextureType = textureType;
    fConfig = config;
}
