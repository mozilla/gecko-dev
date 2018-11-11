/*
* Copyright 2016 Google Inc.
*
* Use of this source code is governed by a BSD-style license that can be
* found in the LICENSE file.
*/

#include "vk/GrVkPipelineStateBuilder.h"
#include "GrContext.h"
#include "GrContextPriv.h"
#include "GrShaderCaps.h"
#include "GrStencilSettings.h"
#include "GrVkRenderTarget.h"
#include "vk/GrVkDescriptorSetManager.h"
#include "vk/GrVkGpu.h"
#include "vk/GrVkRenderPass.h"

GrVkPipelineState* GrVkPipelineStateBuilder::CreatePipelineState(
        GrVkGpu* gpu,
        const GrPrimitiveProcessor& primProc,
        const GrPipeline& pipeline,
        const GrStencilSettings& stencil,
        GrPrimitiveType primitiveType,
        Desc* desc,
        VkRenderPass compatibleRenderPass) {
    // create a builder.  This will be handed off to effects so they can use it to add
    // uniforms, varyings, textures, etc
    GrVkPipelineStateBuilder builder(gpu, pipeline, primProc, desc);

    if (!builder.emitAndInstallProcs()) {
        return nullptr;
    }

    return builder.finalize(stencil, primitiveType, compatibleRenderPass, desc);
}

GrVkPipelineStateBuilder::GrVkPipelineStateBuilder(GrVkGpu* gpu,
                                                   const GrPipeline& pipeline,
                                                   const GrPrimitiveProcessor& primProc,
                                                   GrProgramDesc* desc)
        : INHERITED(primProc, pipeline, desc)
        , fGpu(gpu)
        , fVaryingHandler(this)
        , fUniformHandler(this) {}

const GrCaps* GrVkPipelineStateBuilder::caps() const {
    return fGpu->caps();
}

void GrVkPipelineStateBuilder::finalizeFragmentOutputColor(GrShaderVar& outputColor) {
    outputColor.addLayoutQualifier("location = 0, index = 0");
}

void GrVkPipelineStateBuilder::finalizeFragmentSecondaryColor(GrShaderVar& outputColor) {
    outputColor.addLayoutQualifier("location = 0, index = 1");
}

bool GrVkPipelineStateBuilder::createVkShaderModule(VkShaderStageFlagBits stage,
                                                    const GrGLSLShaderBuilder& builder,
                                                    VkShaderModule* shaderModule,
                                                    VkPipelineShaderStageCreateInfo* stageInfo,
                                                    const SkSL::Program::Settings& settings,
                                                    Desc* desc) {
    SkString shaderString;
    for (int i = 0; i < builder.fCompilerStrings.count(); ++i) {
        if (builder.fCompilerStrings[i]) {
            shaderString.append(builder.fCompilerStrings[i]);
            shaderString.append("\n");
        }
    }

    SkSL::Program::Inputs inputs;
    bool result = GrCompileVkShaderModule(fGpu, shaderString.c_str(), stage, shaderModule,
                                          stageInfo, settings, &inputs);
    if (!result) {
        return false;
    }
    if (inputs.fRTHeight) {
        this->addRTHeightUniform(SKSL_RTHEIGHT_NAME);
    }
    if (inputs.fFlipY) {
        desc->setSurfaceOriginKey(GrGLSLFragmentShaderBuilder::KeyForSurfaceOrigin(
                                                     this->pipeline().proxy()->origin()));
        desc->finalize();
    }
    return result;
}

GrVkPipelineState* GrVkPipelineStateBuilder::finalize(const GrStencilSettings& stencil,
                                                      GrPrimitiveType primitiveType,
                                                      VkRenderPass compatibleRenderPass,
                                                      Desc* desc) {
    VkDescriptorSetLayout dsLayout[2];
    VkPipelineLayout pipelineLayout;
    VkShaderModule vertShaderModule = VK_NULL_HANDLE;
    VkShaderModule geomShaderModule = VK_NULL_HANDLE;
    VkShaderModule fragShaderModule = VK_NULL_HANDLE;

    GrVkResourceProvider& resourceProvider = fGpu->resourceProvider();
    // These layouts are not owned by the PipelineStateBuilder and thus should not be destroyed
    dsLayout[GrVkUniformHandler::kUniformBufferDescSet] = resourceProvider.getUniformDSLayout();

    GrVkDescriptorSetManager::Handle samplerDSHandle;
    resourceProvider.getSamplerDescriptorSetHandle(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                   fUniformHandler, &samplerDSHandle);
    dsLayout[GrVkUniformHandler::kSamplerDescSet] =
            resourceProvider.getSamplerDSLayout(samplerDSHandle);

    // Create the VkPipelineLayout
    VkPipelineLayoutCreateInfo layoutCreateInfo;
    memset(&layoutCreateInfo, 0, sizeof(VkPipelineLayoutCreateFlags));
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCreateInfo.pNext = 0;
    layoutCreateInfo.flags = 0;
    layoutCreateInfo.setLayoutCount = 2;
    layoutCreateInfo.pSetLayouts = dsLayout;
    layoutCreateInfo.pushConstantRangeCount = 0;
    layoutCreateInfo.pPushConstantRanges = nullptr;

    GR_VK_CALL_ERRCHECK(fGpu->vkInterface(), CreatePipelineLayout(fGpu->device(),
                                                                  &layoutCreateInfo,
                                                                  nullptr,
                                                                  &pipelineLayout));

    // We need to enable the following extensions so that the compiler can correctly make spir-v
    // from our glsl shaders.
    fVS.extensions().appendf("#extension GL_ARB_separate_shader_objects : enable\n");
    fFS.extensions().appendf("#extension GL_ARB_separate_shader_objects : enable\n");
    fVS.extensions().appendf("#extension GL_ARB_shading_language_420pack : enable\n");
    fFS.extensions().appendf("#extension GL_ARB_shading_language_420pack : enable\n");

    this->finalizeShaders();

    VkPipelineShaderStageCreateInfo shaderStageInfo[3];
    SkSL::Program::Settings settings;
    settings.fCaps = this->caps()->shaderCaps();
    settings.fFlipY = this->pipeline().proxy()->origin() != kTopLeft_GrSurfaceOrigin;
    settings.fSharpenTextures = this->gpu()->getContext()->contextPriv().sharpenMipmappedTextures();
    SkASSERT(!this->fragColorIsInOut());
    SkAssertResult(this->createVkShaderModule(VK_SHADER_STAGE_VERTEX_BIT,
                                              fVS,
                                              &vertShaderModule,
                                              &shaderStageInfo[0],
                                              settings,
                                              desc));

    SkAssertResult(this->createVkShaderModule(VK_SHADER_STAGE_FRAGMENT_BIT,
                                              fFS,
                                              &fragShaderModule,
                                              &shaderStageInfo[1],
                                              settings,
                                              desc));

    int numShaderStages = 2; // We always have at least vertex and fragment stages.
    if (this->primitiveProcessor().willUseGeoShader()) {
        SkAssertResult(this->createVkShaderModule(VK_SHADER_STAGE_GEOMETRY_BIT,
                                                  fGS,
                                                  &geomShaderModule,
                                                  &shaderStageInfo[2],
                                                  settings,
                                                  desc));
        ++numShaderStages;
    }

    GrVkPipeline* pipeline = resourceProvider.createPipeline(fPrimProc,
                                                             fPipeline,
                                                             stencil,
                                                             shaderStageInfo,
                                                             numShaderStages,
                                                             primitiveType,
                                                             compatibleRenderPass,
                                                             pipelineLayout);
    GR_VK_CALL(fGpu->vkInterface(), DestroyShaderModule(fGpu->device(), vertShaderModule,
                                                        nullptr));
    GR_VK_CALL(fGpu->vkInterface(), DestroyShaderModule(fGpu->device(), fragShaderModule,
                                                        nullptr));
    // This if check should not be needed since calling destroy on a VK_NULL_HANDLE is allowed.
    // However this is causing a crash in certain drivers (e.g. NVidia).
    if (this->primitiveProcessor().willUseGeoShader()) {
        GR_VK_CALL(fGpu->vkInterface(), DestroyShaderModule(fGpu->device(), geomShaderModule,
                                                            nullptr));
    }

    if (!pipeline) {
        GR_VK_CALL(fGpu->vkInterface(), DestroyPipelineLayout(fGpu->device(), pipelineLayout,
                                                              nullptr));
        return nullptr;
    }

    return new GrVkPipelineState(fGpu,
                                 pipeline,
                                 pipelineLayout,
                                 samplerDSHandle,
                                 fUniformHandles,
                                 fUniformHandler.fUniforms,
                                 fUniformHandler.fCurrentGeometryUBOOffset,
                                 fUniformHandler.fCurrentFragmentUBOOffset,
                                 (uint32_t)fUniformHandler.numSamplers(),
                                 std::move(fGeometryProcessor),
                                 std::move(fXferProcessor),
                                 std::move(fFragmentProcessors),
                                 fFragmentProcessorCnt);
}

//////////////////////////////////////////////////////////////////////////////

uint32_t get_blend_info_key(const GrPipeline& pipeline) {
    GrXferProcessor::BlendInfo blendInfo;
    pipeline.getXferProcessor().getBlendInfo(&blendInfo);

    static const uint32_t kBlendWriteShift = 1;
    static const uint32_t kBlendCoeffShift = 5;
    GR_STATIC_ASSERT(kLast_GrBlendCoeff < (1 << kBlendCoeffShift));
    GR_STATIC_ASSERT(kFirstAdvancedGrBlendEquation - 1 < 4);

    uint32_t key = blendInfo.fWriteColor;
    key |= (blendInfo.fSrcBlend << kBlendWriteShift);
    key |= (blendInfo.fDstBlend << (kBlendWriteShift + kBlendCoeffShift));
    key |= (blendInfo.fEquation << (kBlendWriteShift + 2 * kBlendCoeffShift));

    return key;
}

bool GrVkPipelineStateBuilder::Desc::Build(Desc* desc,
                                           const GrPrimitiveProcessor& primProc,
                                           const GrPipeline& pipeline,
                                           const GrStencilSettings& stencil,
                                           GrPrimitiveType primitiveType,
                                           const GrShaderCaps& caps) {
    if (!INHERITED::Build(desc, primProc, primitiveType == GrPrimitiveType::kPoints, pipeline,
                          caps)) {
        return false;
    }

    GrProcessorKeyBuilder b(&desc->key());
    GrVkRenderTarget* vkRT = (GrVkRenderTarget*)pipeline.renderTarget();
    vkRT->simpleRenderPass()->genKey(&b);

    stencil.genKey(&b);

    b.add32(get_blend_info_key(pipeline));

    b.add32((uint32_t)primitiveType);

    return true;
}
