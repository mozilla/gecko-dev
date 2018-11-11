/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "glsl/GrGLSLProgramBuilder.h"

#include "GrCaps.h"
#include "GrPipeline.h"
#include "GrShaderCaps.h"
#include "GrTexturePriv.h"
#include "glsl/GrGLSLFragmentProcessor.h"
#include "glsl/GrGLSLGeometryProcessor.h"
#include "glsl/GrGLSLVarying.h"
#include "glsl/GrGLSLXferProcessor.h"
#include "SkSLCompiler.h"

const int GrGLSLProgramBuilder::kVarsPerBlock = 8;

GrGLSLProgramBuilder::GrGLSLProgramBuilder(const GrPrimitiveProcessor& primProc,
                                           const GrPipeline& pipeline,
                                           GrProgramDesc* desc)
        : fVS(this)
        , fGS(this)
        , fFS(this)
        , fStageIndex(-1)
        , fPipeline(pipeline)
        , fPrimProc(primProc)
        , fDesc(desc)
        , fGeometryProcessor(nullptr)
        , fXferProcessor(nullptr)
        , fNumFragmentSamplers(0) {}

void GrGLSLProgramBuilder::addFeature(GrShaderFlags shaders,
                                      uint32_t featureBit,
                                      const char* extensionName) {
    if (shaders & kVertex_GrShaderFlag) {
        fVS.addFeature(featureBit, extensionName);
    }
    if (shaders & kGeometry_GrShaderFlag) {
        SkASSERT(this->primitiveProcessor().willUseGeoShader());
        fGS.addFeature(featureBit, extensionName);
    }
    if (shaders & kFragment_GrShaderFlag) {
        fFS.addFeature(featureBit, extensionName);
    }
}

bool GrGLSLProgramBuilder::emitAndInstallProcs() {
    // First we loop over all of the installed processors and collect coord transforms.  These will
    // be sent to the GrGLSLPrimitiveProcessor in its emitCode function
    const GrPrimitiveProcessor& primProc = this->primitiveProcessor();

    SkString inputColor;
    SkString inputCoverage;
    this->emitAndInstallPrimProc(primProc, &inputColor, &inputCoverage);
    this->emitAndInstallFragProcs(&inputColor, &inputCoverage);
    this->emitAndInstallXferProc(inputColor, inputCoverage);
    this->emitFSOutputSwizzle(this->pipeline().getXferProcessor().hasSecondaryOutput());

    return this->checkSamplerCounts();
}

void GrGLSLProgramBuilder::emitAndInstallPrimProc(const GrPrimitiveProcessor& proc,
                                                  SkString* outputColor,
                                                  SkString* outputCoverage) {
    // Program builders have a bit of state we need to clear with each effect
    AutoStageAdvance adv(this);
    this->nameExpression(outputColor, "outputColor");
    this->nameExpression(outputCoverage, "outputCoverage");

    SkASSERT(!fUniformHandles.fRTAdjustmentUni.isValid());
    GrShaderFlags rtAdjustVisibility;
    if (proc.willUseGeoShader()) {
        rtAdjustVisibility = kGeometry_GrShaderFlag;
    } else {
        rtAdjustVisibility = kVertex_GrShaderFlag;
    }
    fUniformHandles.fRTAdjustmentUni = this->uniformHandler()->addUniform(
                                                                     rtAdjustVisibility,
                                                                     kFloat4_GrSLType,
                                                                     SkSL::Compiler::RTADJUST_NAME);
    const char* rtAdjustName =
        this->uniformHandler()->getUniformCStr(fUniformHandles.fRTAdjustmentUni);

    // Enclose custom code in a block to avoid namespace conflicts
    SkString openBrace;
    openBrace.printf("{ // Stage %d, %s\n", fStageIndex, proc.name());
    fFS.codeAppend(openBrace.c_str());
    fVS.codeAppendf("// Primitive Processor %s\n", proc.name());

    SkASSERT(!fGeometryProcessor);
    fGeometryProcessor.reset(proc.createGLSLInstance(*this->shaderCaps()));

    SkAutoSTMalloc<4, SamplerHandle> texSamplers(proc.numTextureSamplers());
    for (int i = 0; i < proc.numTextureSamplers(); ++i) {
        SkString name;
        name.printf("TextureSampler_%d", i);
        const auto& sampler = proc.textureSampler(i);
        texSamplers[i] = this->emitSampler(sampler.textureType(), sampler.config(), name.c_str());
    }

    GrGLSLPrimitiveProcessor::FPCoordTransformHandler transformHandler(fPipeline,
                                                                       &fTransformedCoordVars);
    GrGLSLGeometryProcessor::EmitArgs args(&fVS,
                                           proc.willUseGeoShader() ? &fGS : nullptr,
                                           &fFS,
                                           this->varyingHandler(),
                                           this->uniformHandler(),
                                           this->shaderCaps(),
                                           proc,
                                           outputColor->c_str(),
                                           outputCoverage->c_str(),
                                           rtAdjustName,
                                           texSamplers.get(),
                                           &transformHandler);
    fGeometryProcessor->emitCode(args);

    // We have to check that effects and the code they emit are consistent, ie if an effect
    // asks for dst color, then the emit code needs to follow suit
    SkDEBUGCODE(verify(proc);)

    fFS.codeAppend("}");
}

void GrGLSLProgramBuilder::emitAndInstallFragProcs(SkString* color, SkString* coverage) {
    int transformedCoordVarsIdx = 0;
    SkString** inOut = &color;
    SkSTArray<8, std::unique_ptr<GrGLSLFragmentProcessor>> glslFragmentProcessors;
    for (int i = 0; i < this->pipeline().numFragmentProcessors(); ++i) {
        if (i == this->pipeline().numColorFragmentProcessors()) {
            inOut = &coverage;
        }
        SkString output;
        const GrFragmentProcessor& fp = this->pipeline().getFragmentProcessor(i);
        output = this->emitAndInstallFragProc(fp, i, transformedCoordVarsIdx, **inOut, output,
                                              &glslFragmentProcessors);
        GrFragmentProcessor::Iter iter(&fp);
        while (const GrFragmentProcessor* fp = iter.next()) {
            transformedCoordVarsIdx += fp->numCoordTransforms();
        }
        **inOut = output;
    }
    fFragmentProcessorCnt = glslFragmentProcessors.count();
    fFragmentProcessors.reset(new std::unique_ptr<GrGLSLFragmentProcessor>[fFragmentProcessorCnt]);
    for (int i = 0; i < fFragmentProcessorCnt; ++i) {
        fFragmentProcessors[i] = std::move(glslFragmentProcessors[i]);
    }
}

// TODO Processors cannot output zeros because an empty string is all 1s
// the fix is to allow effects to take the SkString directly
SkString GrGLSLProgramBuilder::emitAndInstallFragProc(
        const GrFragmentProcessor& fp,
        int index,
        int transformedCoordVarsIdx,
        const SkString& input,
        SkString output,
        SkTArray<std::unique_ptr<GrGLSLFragmentProcessor>>* glslFragmentProcessors) {
    SkASSERT(input.size());
    // Program builders have a bit of state we need to clear with each effect
    AutoStageAdvance adv(this);
    this->nameExpression(&output, "output");

    // Enclose custom code in a block to avoid namespace conflicts
    SkString openBrace;
    openBrace.printf("{ // Stage %d, %s\n", fStageIndex, fp.name());
    fFS.codeAppend(openBrace.c_str());

    GrGLSLFragmentProcessor* fragProc = fp.createGLSLInstance();

    SkSTArray<4, SamplerHandle> texSamplers;
    GrFragmentProcessor::Iter fpIter(&fp);
    int samplerIdx = 0;
    while (const auto* subFP = fpIter.next()) {
        for (int i = 0; i < subFP->numTextureSamplers(); ++i) {
            SkString name;
            name.printf("TextureSampler_%d", samplerIdx++);
            const auto& sampler = subFP->textureSampler(i);
            GrTextureType textureType = sampler.peekTexture()->texturePriv().textureType();
            texSamplers.emplace_back(this->emitSampler(textureType, sampler.peekTexture()->config(),
                                                       name.c_str()));
        }
    }

    const GrShaderVar* coordVars = fTransformedCoordVars.begin() + transformedCoordVarsIdx;
    GrGLSLFragmentProcessor::TransformedCoordVars coords(&fp, coordVars);
    GrGLSLFragmentProcessor::TextureSamplers textureSamplers(&fp, texSamplers.begin());
    GrGLSLFragmentProcessor::EmitArgs args(&fFS,
                                           this->uniformHandler(),
                                           this->shaderCaps(),
                                           fp,
                                           output.c_str(),
                                           input.c_str(),
                                           coords,
                                           textureSamplers);

    fragProc->emitCode(args);

    // We have to check that effects and the code they emit are consistent, ie if an effect
    // asks for dst color, then the emit code needs to follow suit
    SkDEBUGCODE(verify(fp);)
    glslFragmentProcessors->emplace_back(fragProc);

    fFS.codeAppend("}");
    return output;
}

void GrGLSLProgramBuilder::emitAndInstallXferProc(const SkString& colorIn,
                                                  const SkString& coverageIn) {
    // Program builders have a bit of state we need to clear with each effect
    AutoStageAdvance adv(this);

    SkASSERT(!fXferProcessor);
    const GrXferProcessor& xp = fPipeline.getXferProcessor();
    fXferProcessor.reset(xp.createGLSLInstance());

    // Enable dual source secondary output if we have one
    if (xp.hasSecondaryOutput()) {
        fFS.enableSecondaryOutput();
    }

    if (this->shaderCaps()->mustDeclareFragmentShaderOutput()) {
        fFS.enableCustomOutput();
    }

    SkString openBrace;
    openBrace.printf("{ // Xfer Processor: %s\n", xp.name());
    fFS.codeAppend(openBrace.c_str());

    SamplerHandle dstTextureSamplerHandle;
    GrSurfaceOrigin dstTextureOrigin = kTopLeft_GrSurfaceOrigin;

    if (GrTexture* dstTexture = fPipeline.peekDstTexture()) {
        // GrProcessor::TextureSampler sampler(dstTexture);
        SkString name("DstTextureSampler");
        dstTextureSamplerHandle =
                this->emitSampler(dstTexture->texturePriv().textureType(), dstTexture->config(),
                                  "DstTextureSampler");
        dstTextureOrigin = fPipeline.dstTextureProxy()->origin();
        SkASSERT(dstTexture->texturePriv().textureType() != GrTextureType::kExternal);
    }

    GrGLSLXferProcessor::EmitArgs args(&fFS,
                                       this->uniformHandler(),
                                       this->shaderCaps(),
                                       xp,
                                       colorIn.size() ? colorIn.c_str() : "float4(1)",
                                       coverageIn.size() ? coverageIn.c_str() : "float4(1)",
                                       fFS.getPrimaryColorOutputName(),
                                       fFS.getSecondaryColorOutputName(),
                                       dstTextureSamplerHandle,
                                       dstTextureOrigin);
    fXferProcessor->emitCode(args);

    // We have to check that effects and the code they emit are consistent, ie if an effect
    // asks for dst color, then the emit code needs to follow suit
    SkDEBUGCODE(verify(xp);)
    fFS.codeAppend("}");
}

GrGLSLProgramBuilder::SamplerHandle GrGLSLProgramBuilder::emitSampler(GrTextureType textureType,
                                                                      GrPixelConfig config,
                                                                      const char* name) {
    ++fNumFragmentSamplers;
    GrSLPrecision precision = GrSLSamplerPrecision(config);
    GrSwizzle swizzle = this->shaderCaps()->configTextureSwizzle(config);
    return this->uniformHandler()->addSampler(swizzle, textureType, precision, name);
}

void GrGLSLProgramBuilder::emitFSOutputSwizzle(bool hasSecondaryOutput) {
    // Swizzle the fragment shader outputs if necessary.
    GrSwizzle swizzle;
    swizzle.setFromKey(this->desc()->header().fOutputSwizzle);
    if (swizzle != GrSwizzle::RGBA()) {
        fFS.codeAppendf("%s = %s.%s;", fFS.getPrimaryColorOutputName(),
                        fFS.getPrimaryColorOutputName(),
                        swizzle.c_str());
        if (hasSecondaryOutput) {
            fFS.codeAppendf("%s = %s.%s;", fFS.getSecondaryColorOutputName(),
                            fFS.getSecondaryColorOutputName(),
                            swizzle.c_str());
        }
    }
}

bool GrGLSLProgramBuilder::checkSamplerCounts() {
    const GrShaderCaps& shaderCaps = *this->shaderCaps();
    if (fNumFragmentSamplers > shaderCaps.maxFragmentSamplers()) {
        GrCapsDebugf(this->caps(), "Program would use too many fragment samplers\n");
        return false;
    }
    return true;
}

#ifdef SK_DEBUG
void GrGLSLProgramBuilder::verify(const GrPrimitiveProcessor& gp) {
}

void GrGLSLProgramBuilder::verify(const GrXferProcessor& xp) {
    SkASSERT(fFS.hasReadDstColor() == xp.willReadDstColor());
}

void GrGLSLProgramBuilder::verify(const GrFragmentProcessor& fp) {
}
#endif

void GrGLSLProgramBuilder::nameVariable(SkString* out, char prefix, const char* name, bool mangle) {
    if ('\0' == prefix) {
        *out = name;
    } else {
        out->printf("%c%s", prefix, name);
    }
    if (mangle) {
        if (out->endsWith('_')) {
            // Names containing "__" are reserved.
            out->append("x");
        }
        out->appendf("_Stage%d%s", fStageIndex, fFS.getMangleString().c_str());
    }
}

void GrGLSLProgramBuilder::nameExpression(SkString* output, const char* baseName) {
    // create var to hold stage result.  If we already have a valid output name, just use that
    // otherwise create a new mangled one.  This name is only valid if we are reordering stages
    // and have to tell stage exactly where to put its output.
    SkString outName;
    if (output->size()) {
        outName = output->c_str();
    } else {
        this->nameVariable(&outName, '\0', baseName);
    }
    fFS.codeAppendf("half4 %s;", outName.c_str());
    *output = outName;
}

void GrGLSLProgramBuilder::appendUniformDecls(GrShaderFlags visibility, SkString* out) const {
    this->uniformHandler()->appendUniformDecls(visibility, out);
}

void GrGLSLProgramBuilder::addRTWidthUniform(const char* name) {
        SkASSERT(!fUniformHandles.fRTWidthUni.isValid());
        GrGLSLUniformHandler* uniformHandler = this->uniformHandler();
        fUniformHandles.fRTWidthUni =
            uniformHandler->internalAddUniformArray(kFragment_GrShaderFlag,
                                                    kHalf_GrSLType, kDefault_GrSLPrecision,
                                                    name, false, 0, nullptr);
}

void GrGLSLProgramBuilder::addRTHeightUniform(const char* name) {
        SkASSERT(!fUniformHandles.fRTHeightUni.isValid());
        GrGLSLUniformHandler* uniformHandler = this->uniformHandler();
        fUniformHandles.fRTHeightUni =
            uniformHandler->internalAddUniformArray(kFragment_GrShaderFlag,
                                                    kHalf_GrSLType, kDefault_GrSLPrecision,
                                                    name, false, 0, nullptr);
}

void GrGLSLProgramBuilder::finalizeShaders() {
    this->varyingHandler()->finalize();
    fVS.finalize(kVertex_GrShaderFlag);
    if (this->primitiveProcessor().willUseGeoShader()) {
        SkASSERT(this->shaderCaps()->geometryShaderSupport());
        fGS.finalize(kGeometry_GrShaderFlag);
    }
    fFS.finalize(kFragment_GrShaderFlag);
}
