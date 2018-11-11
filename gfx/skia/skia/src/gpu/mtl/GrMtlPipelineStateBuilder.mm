/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrMtlPipelineStateBuilder.h"

#include "GrContext.h"
#include "GrContextPriv.h"

#include "GrMtlGpu.h"
#include "GrMtlPipelineState.h"
#include "GrMtlUtil.h"

#import <simd/simd.h>

GrMtlPipelineState* GrMtlPipelineStateBuilder::CreatePipelineState(
        const GrPrimitiveProcessor& primProc,
        const GrPipeline& pipeline,
        GrProgramDesc* desc,
        GrMtlGpu* gpu) {
    GrMtlPipelineStateBuilder builder(primProc, pipeline, desc, gpu);

    if (!builder.emitAndInstallProcs()) {
        return nullptr;
    }
    return builder.finalize(primProc, pipeline, desc);
}

GrMtlPipelineStateBuilder::GrMtlPipelineStateBuilder(const GrPrimitiveProcessor& primProc,
                                                     const GrPipeline& pipeline,
                                                     GrProgramDesc* desc,
                                                     GrMtlGpu* gpu)
        : INHERITED(primProc, pipeline, desc)
        , fGpu(gpu)
        , fUniformHandler(this)
        , fVaryingHandler(this) {
}

const GrCaps* GrMtlPipelineStateBuilder::caps() const {
    return fGpu->caps();
}

void GrMtlPipelineStateBuilder::finalizeFragmentOutputColor(GrShaderVar& outputColor) {
    outputColor.addLayoutQualifier("location = 0, index = 0");
}

void GrMtlPipelineStateBuilder::finalizeFragmentSecondaryColor(GrShaderVar& outputColor) {
    outputColor.addLayoutQualifier("location = 0, index = 1");
}

id<MTLLibrary> GrMtlPipelineStateBuilder::createMtlShaderLibrary(
        const GrGLSLShaderBuilder& builder,
        SkSL::Program::Kind kind,
        const SkSL::Program::Settings& settings,
        GrProgramDesc* desc) {
    SkString shaderString;
    for (int i = 0; i < builder.fCompilerStrings.count(); ++i) {
        if (builder.fCompilerStrings[i]) {
            shaderString.append(builder.fCompilerStrings[i]);
            shaderString.append("\n");
        }
    }

    SkSL::Program::Inputs inputs;
    id<MTLLibrary> shaderLibrary = GrCompileMtlShaderLibrary(fGpu, shaderString.c_str(),
                                                             kind, settings, &inputs);
    if (shaderLibrary == nil) {
        return nil;
    }
    if (inputs.fRTHeight) {
        this->addRTHeightUniform(SKSL_RTHEIGHT_NAME);
    }
    if (inputs.fFlipY) {
        desc->setSurfaceOriginKey(GrGLSLFragmentShaderBuilder::KeyForSurfaceOrigin(
                                                               this->pipeline().proxy()->origin()));
        desc->finalize();
    }
    return shaderLibrary;
}

static inline MTLVertexFormat attribute_type_to_mtlformat(GrVertexAttribType type) {
    // All half types will actually be float types. We are currently not using half types with
    // metal to avoid an issue with narrow type coercions (float->half) http://skbug.com/8221
    switch (type) {
        case kFloat_GrVertexAttribType:
            return MTLVertexFormatFloat;
        case kFloat2_GrVertexAttribType:
            return MTLVertexFormatFloat2;
        case kFloat3_GrVertexAttribType:
            return MTLVertexFormatFloat3;
        case kFloat4_GrVertexAttribType:
            return MTLVertexFormatFloat4;
        case kHalf_GrVertexAttribType:
            return MTLVertexFormatHalf;
        case kHalf2_GrVertexAttribType:
            return MTLVertexFormatHalf2;
        case kHalf3_GrVertexAttribType:
            return MTLVertexFormatHalf3;
        case kHalf4_GrVertexAttribType:
            return MTLVertexFormatHalf4;
        case kInt2_GrVertexAttribType:
            return MTLVertexFormatInt2;
        case kInt3_GrVertexAttribType:
            return MTLVertexFormatInt3;
        case kInt4_GrVertexAttribType:
            return MTLVertexFormatInt4;
        case kByte_GrVertexAttribType:
            return MTLVertexFormatChar;
        case kByte2_GrVertexAttribType:
            return MTLVertexFormatChar2;
        case kByte3_GrVertexAttribType:
            return MTLVertexFormatChar3;
        case kByte4_GrVertexAttribType:
            return MTLVertexFormatChar4;
        case kUByte_GrVertexAttribType:
            return MTLVertexFormatUChar;
        case kUByte2_GrVertexAttribType:
            return MTLVertexFormatUChar2;
        case kUByte3_GrVertexAttribType:
            return MTLVertexFormatUChar3;
        case kUByte4_GrVertexAttribType:
            return MTLVertexFormatUChar4;
        case kUByte_norm_GrVertexAttribType:
            return MTLVertexFormatUCharNormalized;
        case kUByte4_norm_GrVertexAttribType:
            return MTLVertexFormatUChar4Normalized;
        case kShort2_GrVertexAttribType:
            return MTLVertexFormatShort2;
        case kShort4_GrVertexAttribType:
            return MTLVertexFormatShort4;
        case kUShort2_GrVertexAttribType:
            return MTLVertexFormatUShort2;
        case kUShort2_norm_GrVertexAttribType:
            return MTLVertexFormatUShort2Normalized;
        case kInt_GrVertexAttribType:
            return MTLVertexFormatInt;
        case kUint_GrVertexAttribType:
            return MTLVertexFormatUInt;
    }
    SK_ABORT("Unknown vertex attribute type");
    return MTLVertexFormatInvalid;
}

static MTLVertexDescriptor* create_vertex_descriptor(const GrPrimitiveProcessor& primProc) {
    uint32_t vertexBinding = 0, instanceBinding = 0;

    int nextBinding = GrMtlUniformHandler::kLastUniformBinding + 1;
    if (primProc.hasVertexAttributes()) {
        vertexBinding = nextBinding++;
    }

    if (primProc.hasInstanceAttributes()) {
        instanceBinding = nextBinding;
    }

    auto vertexDescriptor = [[MTLVertexDescriptor alloc] init];
    int attributeIndex = 0;

    int vertexAttributeCount = primProc.numVertexAttributes();
    size_t vertexAttributeOffset = 0;
    for (int vertexIndex = 0; vertexIndex < vertexAttributeCount; vertexIndex++) {
        const GrGeometryProcessor::Attribute& attribute = primProc.vertexAttribute(vertexIndex);
        MTLVertexAttributeDescriptor* mtlAttribute = vertexDescriptor.attributes[attributeIndex];
        mtlAttribute.format = attribute_type_to_mtlformat(attribute.cpuType());
        mtlAttribute.offset = vertexAttributeOffset;
        mtlAttribute.bufferIndex = vertexBinding;

        SkASSERT(mtlAttribute.offset == primProc.debugOnly_vertexAttributeOffset(vertexIndex));
        vertexAttributeOffset += attribute.sizeAlign4();
        attributeIndex++;
    }
    SkASSERT(vertexAttributeOffset == primProc.debugOnly_vertexStride());

    if (vertexAttributeCount) {
        MTLVertexBufferLayoutDescriptor* vertexBufferLayout =
                vertexDescriptor.layouts[vertexBinding];
        vertexBufferLayout.stepFunction = MTLVertexStepFunctionPerVertex;
        vertexBufferLayout.stepRate = 1;
        vertexBufferLayout.stride = vertexAttributeOffset;
    }

    int instanceAttributeCount = primProc.numInstanceAttributes();
    size_t instanceAttributeOffset = 0;
    for (int instanceIndex = 0; instanceIndex < instanceAttributeCount; instanceIndex++) {
        const GrGeometryProcessor::Attribute& attribute = primProc.instanceAttribute(instanceIndex);
        MTLVertexAttributeDescriptor* mtlAttribute = vertexDescriptor.attributes[attributeIndex];
        mtlAttribute.format = attribute_type_to_mtlformat(attribute.cpuType());
        mtlAttribute.offset = instanceAttributeOffset;
        mtlAttribute.bufferIndex = instanceBinding;

        SkASSERT(mtlAttribute.offset == primProc.debugOnly_instanceAttributeOffset(instanceIndex));
        instanceAttributeOffset += attribute.sizeAlign4();
        attributeIndex++;
    }
    SkASSERT(instanceAttributeOffset == primProc.debugOnly_instanceStride());

    if (instanceAttributeCount) {
        MTLVertexBufferLayoutDescriptor* instanceBufferLayout =
                vertexDescriptor.layouts[instanceBinding];
        instanceBufferLayout.stepFunction = MTLVertexStepFunctionPerInstance;
        instanceBufferLayout.stepRate = 1;
        instanceBufferLayout.stride = instanceAttributeOffset;
    }
    return vertexDescriptor;
}

static MTLBlendFactor blend_coeff_to_mtl_blend(GrBlendCoeff coeff) {
    static const MTLBlendFactor gTable[] = {
        MTLBlendFactorZero,                      // kZero_GrBlendCoeff
        MTLBlendFactorOne,                       // kOne_GrBlendCoeff
        MTLBlendFactorSourceColor,               // kSC_GrBlendCoeff
        MTLBlendFactorOneMinusSourceColor,       // kISC_GrBlendCoeff
        MTLBlendFactorDestinationColor,          // kDC_GrBlendCoeff
        MTLBlendFactorOneMinusDestinationColor,  // kIDC_GrBlendCoeff
        MTLBlendFactorSourceAlpha,               // kSA_GrBlendCoeff
        MTLBlendFactorOneMinusSourceAlpha,       // kISA_GrBlendCoeff
        MTLBlendFactorDestinationAlpha,          // kDA_GrBlendCoeff
        MTLBlendFactorOneMinusDestinationAlpha,  // kIDA_GrBlendCoeff
        MTLBlendFactorBlendColor,                // kConstC_GrBlendCoeff
        MTLBlendFactorOneMinusBlendColor,        // kIConstC_GrBlendCoeff
        MTLBlendFactorBlendAlpha,                // kConstA_GrBlendCoeff
        MTLBlendFactorOneMinusBlendAlpha,        // kIConstA_GrBlendCoeff
        MTLBlendFactorSource1Color,              // kS2C_GrBlendCoeff
        MTLBlendFactorOneMinusSource1Color,      // kIS2C_GrBlendCoeff
        MTLBlendFactorSource1Alpha,              // kS2A_GrBlendCoeff
        MTLBlendFactorOneMinusSource1Alpha,      // kIS2A_GrBlendCoeff
    };
    GR_STATIC_ASSERT(SK_ARRAY_COUNT(gTable) == kGrBlendCoeffCnt);
    GR_STATIC_ASSERT(0 == kZero_GrBlendCoeff);
    GR_STATIC_ASSERT(1 == kOne_GrBlendCoeff);
    GR_STATIC_ASSERT(2 == kSC_GrBlendCoeff);
    GR_STATIC_ASSERT(3 == kISC_GrBlendCoeff);
    GR_STATIC_ASSERT(4 == kDC_GrBlendCoeff);
    GR_STATIC_ASSERT(5 == kIDC_GrBlendCoeff);
    GR_STATIC_ASSERT(6 == kSA_GrBlendCoeff);
    GR_STATIC_ASSERT(7 == kISA_GrBlendCoeff);
    GR_STATIC_ASSERT(8 == kDA_GrBlendCoeff);
    GR_STATIC_ASSERT(9 == kIDA_GrBlendCoeff);
    GR_STATIC_ASSERT(10 == kConstC_GrBlendCoeff);
    GR_STATIC_ASSERT(11 == kIConstC_GrBlendCoeff);
    GR_STATIC_ASSERT(12 == kConstA_GrBlendCoeff);
    GR_STATIC_ASSERT(13 == kIConstA_GrBlendCoeff);
    GR_STATIC_ASSERT(14 == kS2C_GrBlendCoeff);
    GR_STATIC_ASSERT(15 == kIS2C_GrBlendCoeff);
    GR_STATIC_ASSERT(16 == kS2A_GrBlendCoeff);
    GR_STATIC_ASSERT(17 == kIS2A_GrBlendCoeff);

    SkASSERT((unsigned)coeff < kGrBlendCoeffCnt);
    return gTable[coeff];
}

static MTLBlendOperation blend_equation_to_mtl_blend_op(GrBlendEquation equation) {
    static const MTLBlendOperation gTable[] = {
        MTLBlendOperationAdd,              // kAdd_GrBlendEquation
        MTLBlendOperationSubtract,         // kSubtract_GrBlendEquation
        MTLBlendOperationReverseSubtract,  // kReverseSubtract_GrBlendEquation
    };
    GR_STATIC_ASSERT(SK_ARRAY_COUNT(gTable) == kFirstAdvancedGrBlendEquation);
    GR_STATIC_ASSERT(0 == kAdd_GrBlendEquation);
    GR_STATIC_ASSERT(1 == kSubtract_GrBlendEquation);
    GR_STATIC_ASSERT(2 == kReverseSubtract_GrBlendEquation);

    SkASSERT((unsigned)equation < kGrBlendCoeffCnt);
    return gTable[equation];
}

static MTLRenderPipelineColorAttachmentDescriptor* create_color_attachment(
        const GrPipeline& pipeline) {
    auto mtlColorAttachment = [[MTLRenderPipelineColorAttachmentDescriptor alloc] init];

    // pixel format
    MTLPixelFormat format;
    SkAssertResult(GrPixelConfigToMTLFormat(pipeline.renderTarget()->config(), &format));
    mtlColorAttachment.pixelFormat = format;

    // blending
    GrXferProcessor::BlendInfo blendInfo;
    pipeline.getXferProcessor().getBlendInfo(&blendInfo);

    GrBlendEquation equation = blendInfo.fEquation;
    GrBlendCoeff srcCoeff = blendInfo.fSrcBlend;
    GrBlendCoeff dstCoeff = blendInfo.fDstBlend;
    bool blendOff = (kAdd_GrBlendEquation == equation || kSubtract_GrBlendEquation == equation) &&
                    kOne_GrBlendCoeff == srcCoeff && kZero_GrBlendCoeff == dstCoeff;

    mtlColorAttachment.blendingEnabled = !blendOff;
    if (!blendOff) {
        mtlColorAttachment.sourceRGBBlendFactor = blend_coeff_to_mtl_blend(srcCoeff);
        mtlColorAttachment.destinationRGBBlendFactor = blend_coeff_to_mtl_blend(dstCoeff);
        mtlColorAttachment.rgbBlendOperation = blend_equation_to_mtl_blend_op(equation);
        mtlColorAttachment.sourceAlphaBlendFactor = blend_coeff_to_mtl_blend(srcCoeff);
        mtlColorAttachment.destinationAlphaBlendFactor = blend_coeff_to_mtl_blend(dstCoeff);
        mtlColorAttachment.alphaBlendOperation = blend_equation_to_mtl_blend_op(equation);
    }

    if (!blendInfo.fWriteColor) {
        mtlColorAttachment.writeMask = MTLColorWriteMaskNone;
    } else {
        mtlColorAttachment.writeMask = MTLColorWriteMaskAll;
    }
    return mtlColorAttachment;
}

GrMtlPipelineState* GrMtlPipelineStateBuilder::finalize(const GrPrimitiveProcessor& primProc,
                                                        const GrPipeline& pipeline,
                                                        GrProgramDesc* desc) {
    auto pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];

    fVS.extensions().appendf("#extension GL_ARB_separate_shader_objects : enable\n");
    fFS.extensions().appendf("#extension GL_ARB_separate_shader_objects : enable\n");
    fVS.extensions().appendf("#extension GL_ARB_shading_language_420pack : enable\n");
    fFS.extensions().appendf("#extension GL_ARB_shading_language_420pack : enable\n");

    this->finalizeShaders();

    SkSL::Program::Settings settings;
    settings.fCaps = this->caps()->shaderCaps();
    settings.fFlipY = this->pipeline().proxy()->origin() != kTopLeft_GrSurfaceOrigin;
    settings.fSharpenTextures = fGpu->getContext()->contextPriv().sharpenMipmappedTextures();
    SkASSERT(!this->fragColorIsInOut());

    id<MTLLibrary> vertexLibrary = nil;
    id<MTLLibrary> fragmentLibrary = nil;
    vertexLibrary = this->createMtlShaderLibrary(fVS,
                                                 SkSL::Program::kVertex_Kind,
                                                 settings,
                                                 desc);
    fragmentLibrary = this->createMtlShaderLibrary(fFS,
                                                   SkSL::Program::kFragment_Kind,
                                                   settings,
                                                   desc);
    SkASSERT(!this->primitiveProcessor().willUseGeoShader());

    SkASSERT(vertexLibrary);
    SkASSERT(fragmentLibrary);

    id<MTLFunction> vertexFunction = [vertexLibrary newFunctionWithName: @"vertexMain"];
    id<MTLFunction> fragmentFunction = [fragmentLibrary newFunctionWithName: @"fragmentMain"];

    pipelineDescriptor.vertexFunction = vertexFunction;
    pipelineDescriptor.fragmentFunction = fragmentFunction;
    pipelineDescriptor.vertexDescriptor = create_vertex_descriptor(primProc);
    pipelineDescriptor.colorAttachments[0] = create_color_attachment(pipeline);

    SkASSERT(pipelineDescriptor.vertexFunction);
    SkASSERT(pipelineDescriptor.fragmentFunction);
    SkASSERT(pipelineDescriptor.vertexDescriptor);
    SkASSERT(pipelineDescriptor.colorAttachments[0]);

    NSError* error = nil;
    id<MTLRenderPipelineState> pipelineState =
            [fGpu->device() newRenderPipelineStateWithDescriptor: pipelineDescriptor
                                                           error: &error];
    if (error) {
        SkDebugf("Error creating pipeline: %s\n",
                 [[error localizedDescription] cStringUsingEncoding: NSASCIIStringEncoding]);
        return nullptr;
    }
    return new GrMtlPipelineState(fGpu,
                                  pipelineState,
                                  pipelineDescriptor.colorAttachments[0].pixelFormat,
                                  fUniformHandles,
                                  fUniformHandler.fUniforms,
                                  GrMtlBuffer::Create(fGpu,
                                                      fUniformHandler.fCurrentGeometryUBOOffset,
                                                      kVertex_GrBufferType,
                                                      kStatic_GrAccessPattern),
                                  GrMtlBuffer::Create(fGpu,
                                                      fUniformHandler.fCurrentFragmentUBOOffset,
                                                      kVertex_GrBufferType,
                                                      kStatic_GrAccessPattern),
                                  (uint32_t)fUniformHandler.numSamplers(),
                                  std::move(fGeometryProcessor),
                                  std::move(fXferProcessor),
                                  std::move(fFragmentProcessors),
                                  fFragmentProcessorCnt);
}
