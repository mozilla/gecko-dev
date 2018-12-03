/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"

#include "GeckoProfiler.h"
#include "MozFramebuffer.h"
#include "GLContext.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/UniquePtrExtensions.h"
#include "nsPrintfCString.h"
#include "WebGLBuffer.h"
#include "WebGLContextUtils.h"
#include "WebGLFramebuffer.h"
#include "WebGLProgram.h"
#include "WebGLRenderbuffer.h"
#include "WebGLShader.h"
#include "WebGLTexture.h"
#include "WebGLTransformFeedback.h"
#include "WebGLVertexArray.h"
#include "WebGLVertexAttribData.h"

#include <algorithm>

namespace mozilla {

// For a Tegra workaround.
static const int MAX_DRAW_CALLS_SINCE_FLUSH = 100;

////////////////////////////////////////

class ScopedResolveTexturesForDraw {
  struct TexRebindRequest {
    uint32_t texUnit;
    WebGLTexture* tex;
  };

  WebGLContext* const mWebGL;
  std::vector<TexRebindRequest> mRebindRequests;

 public:
  ScopedResolveTexturesForDraw(WebGLContext* webgl, bool* const out_error);
  ~ScopedResolveTexturesForDraw();
};

static bool ValidateNoSamplingFeedback(const WebGLTexture& tex,
                                       const uint32_t sampledLevels,
                                       const WebGLFramebuffer* const fb,
                                       const uint32_t texUnit) {
  if (!fb) return true;

  const auto& texAttachments = fb->GetCompletenessInfo()->texAttachments;
  for (const auto& attach : texAttachments) {
    if (attach->Texture() != &tex) continue;

    const auto& srcBase = tex.BaseMipmapLevel();
    const auto srcLast = srcBase + sampledLevels - 1;
    const auto& dstLevel = attach->MipLevel();
    if (MOZ_UNLIKELY(srcBase <= dstLevel && dstLevel <= srcLast)) {
      const auto& webgl = tex.mContext;
      const auto& texTargetStr = EnumString(tex.Target().get());
      const auto& attachStr = EnumString(attach->mAttachmentPoint);
      webgl->ErrorInvalidOperation(
          "Texture level %u would be read by %s unit %u,"
          " but written by framebuffer attachment %s,"
          " which would be illegal feedback.",
          dstLevel, texTargetStr.c_str(), texUnit, attachStr.c_str());
      return false;
    }
  }
  return true;
}

ScopedResolveTexturesForDraw::ScopedResolveTexturesForDraw(
    WebGLContext* webgl, bool* const out_error)
    : mWebGL(webgl) {
  const auto& fb = mWebGL->mBoundDrawFramebuffer;

  MOZ_ASSERT(mWebGL->mActiveProgramLinkInfo);
  const auto& uniformSamplers = mWebGL->mActiveProgramLinkInfo->uniformSamplers;
  for (const auto& uniform : uniformSamplers) {
    const auto& texList = *(uniform->mSamplerTexList);

    const auto& uniformBaseType = uniform->mTexBaseType;
    for (const auto& texUnit : uniform->mSamplerValues) {
      if (texUnit >= texList.Length()) continue;

      const auto& tex = texList[texUnit];
      if (!tex) continue;

      const auto& sampler = mWebGL->mBoundSamplers[texUnit];
      const auto& samplingInfo = tex->GetSampleableInfo(sampler.get());
      if (!samplingInfo) {  // There was an error.
        *out_error = true;
        return;
      }
      if (!samplingInfo->IsComplete()) {
        if (samplingInfo->incompleteReason) {
          const auto& targetName = GetEnumName(tex->Target().get());
          mWebGL->GenerateWarning("%s at unit %u is incomplete: %s", targetName,
                                  texUnit, samplingInfo->incompleteReason);
        }
        mRebindRequests.push_back({texUnit, tex});
        continue;
      }

      // We have more validation to do if we're otherwise complete:
      const auto& texBaseType = samplingInfo->usage->format->baseType;
      if (texBaseType != uniformBaseType) {
        const auto& targetName = GetEnumName(tex->Target().get());
        const auto& srcType = ToString(texBaseType);
        const auto& dstType = ToString(uniformBaseType);
        mWebGL->ErrorInvalidOperation(
            "%s at unit %u is of type %s, but"
            " the shader samples as %s.",
            targetName, texUnit, srcType, dstType);
        *out_error = true;
        return;
      }

      if (uniform->mIsShadowSampler != samplingInfo->isDepthTexCompare) {
        const auto& targetName = GetEnumName(tex->Target().get());
        mWebGL->ErrorInvalidOperation(
            "%s at unit %u is%s a depth texture"
            " with TEXTURE_COMPARE_MODE, but"
            " the shader sampler is%s a shadow"
            " sampler.",
            targetName, texUnit, samplingInfo->isDepthTexCompare ? "" : " not",
            uniform->mIsShadowSampler ? "" : " not");
        *out_error = true;
        return;
      }

      if (!ValidateNoSamplingFeedback(*tex, samplingInfo->levels, fb.get(),
                                      texUnit)) {
        *out_error = true;
        return;
      }
    }
  }

  const auto& gl = mWebGL->gl;
  for (const auto& itr : mRebindRequests) {
    gl->fActiveTexture(LOCAL_GL_TEXTURE0 + itr.texUnit);
    gl->fBindTexture(itr.tex->Target().get(),
                     0);  // Tex 0 is always incomplete.
  }
}

ScopedResolveTexturesForDraw::~ScopedResolveTexturesForDraw() {
  if (mRebindRequests.empty()) return;

  gl::GLContext* gl = mWebGL->gl;

  for (const auto& itr : mRebindRequests) {
    gl->fActiveTexture(LOCAL_GL_TEXTURE0 + itr.texUnit);
    gl->fBindTexture(itr.tex->Target().get(), itr.tex->mGLName);
  }

  gl->fActiveTexture(LOCAL_GL_TEXTURE0 + mWebGL->mActiveTexture);
}

////////////////////////////////////////

bool WebGLContext::ValidateStencilParamsForDrawCall() const {
  const auto stencilBits = [&]() -> uint8_t {
    if (!mStencilTestEnabled) return 0;

    if (!mBoundDrawFramebuffer) return mOptions.stencil ? 8 : 0;

    if (mBoundDrawFramebuffer->StencilAttachment().HasAttachment()) return 8;

    if (mBoundDrawFramebuffer->DepthStencilAttachment().HasAttachment())
      return 8;

    return 0;
  }();
  const uint32_t stencilMax = (1 << stencilBits) - 1;

  const auto fnMask = [&](const uint32_t x) { return x & stencilMax; };
  const auto fnClamp = [&](const int32_t x) {
    return std::max(0, std::min(x, (int32_t)stencilMax));
  };

  bool ok = true;
  ok &= (fnMask(mStencilWriteMaskFront) == fnMask(mStencilWriteMaskBack));
  ok &= (fnMask(mStencilValueMaskFront) == fnMask(mStencilValueMaskBack));
  ok &= (fnClamp(mStencilRefFront) == fnClamp(mStencilRefBack));

  if (!ok) {
    ErrorInvalidOperation(
        "Stencil front/back state must effectively match."
        " (before front/back comparison, WRITEMASK and VALUE_MASK"
        " are masked with (2^s)-1, and REF is clamped to"
        " [0, (2^s)-1], where `s` is the number of enabled stencil"
        " bits in the draw framebuffer)");
  }
  return ok;
}

////////////////////////////////////////

template <typename T>
static bool DoSetsIntersect(const std::set<T>& a, const std::set<T>& b) {
  std::vector<T> intersection;
  std::set_intersection(a.begin(), a.end(), b.begin(), b.end(),
                        std::back_inserter(intersection));
  return bool(intersection.size());
}

const webgl::CachedDrawFetchLimits* ValidateDraw(WebGLContext* const webgl,
                                                 const GLenum mode,
                                                 const uint32_t instanceCount) {
  if (!webgl->BindCurFBForDraw()) return nullptr;

  switch (mode) {
    case LOCAL_GL_TRIANGLES:
    case LOCAL_GL_TRIANGLE_STRIP:
    case LOCAL_GL_TRIANGLE_FAN:
    case LOCAL_GL_POINTS:
    case LOCAL_GL_LINE_STRIP:
    case LOCAL_GL_LINE_LOOP:
    case LOCAL_GL_LINES:
      break;
    default:
      webgl->ErrorInvalidEnumInfo("mode", mode);
      return nullptr;
  }

  if (!webgl->ValidateStencilParamsForDrawCall()) return nullptr;

  if (!webgl->mActiveProgramLinkInfo) {
    webgl->ErrorInvalidOperation("The current program is not linked.");
    return nullptr;
  }
  const auto& linkInfo = webgl->mActiveProgramLinkInfo;

  // -
  // Check UBO sizes.

  for (const auto& cur : linkInfo->uniformBlocks) {
    const auto& dataSize = cur->mDataSize;
    const auto& binding = cur->mBinding;
    if (!binding) {
      webgl->ErrorInvalidOperation("Buffer for uniform block is null.");
      return nullptr;
    }

    const auto availByteCount = binding->ByteCount();
    if (dataSize > availByteCount) {
      webgl->ErrorInvalidOperation(
          "Buffer for uniform block is smaller"
          " than UNIFORM_BLOCK_DATA_SIZE.");
      return nullptr;
    }

    if (binding->mBufferBinding->IsBoundForTF()) {
      webgl->ErrorInvalidOperation(
          "Buffer for uniform block is bound or"
          " in use for transform feedback.");
      return nullptr;
    }
  }

  // -

  const auto& tfo = webgl->mBoundTransformFeedback;
  if (tfo && tfo->IsActiveAndNotPaused()) {
    uint32_t numUsed;
    switch (linkInfo->transformFeedbackBufferMode) {
      case LOCAL_GL_INTERLEAVED_ATTRIBS:
        numUsed = 1;
        break;

      case LOCAL_GL_SEPARATE_ATTRIBS:
        numUsed = linkInfo->transformFeedbackVaryings.size();
        break;

      default:
        MOZ_CRASH();
    }

    for (uint32_t i = 0; i < numUsed; ++i) {
      const auto& buffer = tfo->mIndexedBindings[i].mBufferBinding;
      if (buffer->IsBoundForNonTF()) {
        webgl->ErrorInvalidOperation(
            "Transform feedback varying %u's buffer"
            " is bound for non-transform-feedback.",
            i);
        return nullptr;
      }

      // Technically we don't know that this will be updated yet, but we can
      // speculatively mark it.
      buffer->ResetLastUpdateFenceId();
    }
  }

  // -

  const auto fetchLimits = linkInfo->GetDrawFetchLimits();
  if (!fetchLimits) return nullptr;

  if (instanceCount > fetchLimits->maxInstances) {
    webgl->ErrorInvalidOperation(
        "Instance fetch requires %u, but attribs only"
        " supply %u.",
        instanceCount, uint32_t(fetchLimits->maxInstances));
    return nullptr;
  }

  // -

  webgl->RunContextLossTimer();

  return fetchLimits;
}

////////////////////////////////////////

class ScopedFakeVertexAttrib0 final {
  WebGLContext* const mWebGL;
  bool mDidFake = false;

 public:
  ScopedFakeVertexAttrib0(WebGLContext* const webgl, const uint64_t vertexCount,
                          bool* const out_error)
      : mWebGL(webgl) {
    *out_error = false;

    if (!mWebGL->DoFakeVertexAttrib0(vertexCount)) {
      *out_error = true;
      return;
    }
    mDidFake = true;
  }

  ~ScopedFakeVertexAttrib0() {
    if (mDidFake) {
      mWebGL->UndoFakeVertexAttrib0();
    }
  }
};

////////////////////////////////////////

static uint32_t UsedVertsForTFDraw(GLenum mode, uint32_t vertCount) {
  uint8_t vertsPerPrim;

  switch (mode) {
    case LOCAL_GL_POINTS:
      vertsPerPrim = 1;
      break;
    case LOCAL_GL_LINES:
      vertsPerPrim = 2;
      break;
    case LOCAL_GL_TRIANGLES:
      vertsPerPrim = 3;
      break;
    default:
      MOZ_CRASH("`mode`");
  }

  return vertCount / vertsPerPrim * vertsPerPrim;
}

class ScopedDrawWithTransformFeedback final {
  WebGLContext* const mWebGL;
  WebGLTransformFeedback* const mTFO;
  const bool mWithTF;
  uint32_t mUsedVerts;

 public:
  ScopedDrawWithTransformFeedback(WebGLContext* webgl, GLenum mode,
                                  uint32_t vertCount, uint32_t instanceCount,
                                  bool* const out_error)
      : mWebGL(webgl),
        mTFO(mWebGL->mBoundTransformFeedback),
        mWithTF(mTFO && mTFO->mIsActive && !mTFO->mIsPaused),
        mUsedVerts(0) {
    *out_error = false;
    if (!mWithTF) return;

    if (mode != mTFO->mActive_PrimMode) {
      mWebGL->ErrorInvalidOperation(
          "Drawing with transform feedback requires"
          " `mode` to match BeginTransformFeedback's"
          " `primitiveMode`.");
      *out_error = true;
      return;
    }

    const auto usedVertsPerInstance = UsedVertsForTFDraw(mode, vertCount);
    const auto usedVerts =
        CheckedInt<uint32_t>(usedVertsPerInstance) * instanceCount;

    const auto remainingCapacity =
        mTFO->mActive_VertCapacity - mTFO->mActive_VertPosition;
    if (!usedVerts.isValid() || usedVerts.value() > remainingCapacity) {
      mWebGL->ErrorInvalidOperation(
          "Insufficient buffer capacity remaining for"
          " transform feedback.");
      *out_error = true;
      return;
    }

    mUsedVerts = usedVerts.value();
  }

  void Advance() const {
    if (!mWithTF) return;

    mTFO->mActive_VertPosition += mUsedVerts;
  }
};

static bool HasInstancedDrawing(const WebGLContext& webgl) {
  return webgl.IsWebGL2() ||
         webgl.IsExtensionEnabled(WebGLExtensionID::ANGLE_instanced_arrays);
}

////////////////////////////////////////

void WebGLContext::DrawArraysInstanced(GLenum mode, GLint first,
                                       GLsizei vertCount,
                                       GLsizei instanceCount) {
  const FuncScope funcScope(*this, "drawArraysInstanced");
  AUTO_PROFILER_LABEL("WebGLContext::DrawArraysInstanced", GRAPHICS);
  if (IsContextLost()) return;
  const gl::GLContext::TlsScope inTls(gl);

  // -

  if (!ValidateNonNegative("first", first) ||
      !ValidateNonNegative("vertCount", vertCount) ||
      !ValidateNonNegative("instanceCount", instanceCount)) {
    return;
  }

  if (IsWebGL2() && !gl->IsSupported(gl::GLFeature::prim_restart_fixed)) {
    MOZ_ASSERT(gl->IsSupported(gl::GLFeature::prim_restart));
    if (mPrimRestartTypeBytes != 0) {
      mPrimRestartTypeBytes = 0;

      // OSX appears to have severe perf issues with leaving this enabled.
      gl->fDisable(LOCAL_GL_PRIMITIVE_RESTART);
    }
  }

  // -

  const auto fetchLimits = ValidateDraw(this, mode, instanceCount);
  if (!fetchLimits) return;

  // -

  const auto totalVertCount_safe = CheckedInt<uint32_t>(first) + vertCount;
  if (!totalVertCount_safe.isValid()) {
    ErrorOutOfMemory("`first+vertCount` out of range.");
    return;
  }
  auto totalVertCount = totalVertCount_safe.value();

  if (vertCount && instanceCount && totalVertCount > fetchLimits->maxVerts) {
    ErrorInvalidOperation(
        "Vertex fetch requires %u, but attribs only supply %u.", totalVertCount,
        uint32_t(fetchLimits->maxVerts));
    return;
  }

  // -

  bool error = false;
  const ScopedFakeVertexAttrib0 attrib0(this, totalVertCount, &error);
  if (error) return;

  const ScopedResolveTexturesForDraw scopedResolve(this, &error);
  if (error) return;

  const ScopedDrawWithTransformFeedback scopedTF(this, mode, vertCount,
                                                 instanceCount, &error);
  if (error) return;

  {
    ScopedDrawCallWrapper wrapper(*this);
    if (vertCount && instanceCount) {
      AUTO_PROFILER_LABEL("glDrawArraysInstanced", GRAPHICS);
      if (HasInstancedDrawing(*this)) {
        gl->fDrawArraysInstanced(mode, first, vertCount, instanceCount);
      } else {
        MOZ_ASSERT(instanceCount == 1);
        gl->fDrawArrays(mode, first, vertCount);
      }
    }
  }

  Draw_cleanup();
  scopedTF.Advance();
}

////////////////////////////////////////

WebGLBuffer* WebGLContext::DrawElements_check(const GLsizei rawIndexCount,
                                              const GLenum type,
                                              const WebGLintptr byteOffset,
                                              const GLsizei instanceCount) {
  if (mBoundTransformFeedback && mBoundTransformFeedback->mIsActive &&
      !mBoundTransformFeedback->mIsPaused) {
    ErrorInvalidOperation(
        "DrawElements* functions are incompatible with"
        " transform feedback.");
    return nullptr;
  }

  if (!ValidateNonNegative("vertCount", rawIndexCount) ||
      !ValidateNonNegative("byteOffset", byteOffset) ||
      !ValidateNonNegative("instanceCount", instanceCount)) {
    return nullptr;
  }
  const auto indexCount = uint32_t(rawIndexCount);

  uint8_t bytesPerIndex = 0;
  switch (type) {
    case LOCAL_GL_UNSIGNED_BYTE:
      bytesPerIndex = 1;
      break;

    case LOCAL_GL_UNSIGNED_SHORT:
      bytesPerIndex = 2;
      break;

    case LOCAL_GL_UNSIGNED_INT:
      if (IsWebGL2() ||
          IsExtensionEnabled(WebGLExtensionID::OES_element_index_uint)) {
        bytesPerIndex = 4;
      }
      break;
  }
  if (!bytesPerIndex) {
    ErrorInvalidEnumInfo("type", type);
    return nullptr;
  }
  if (byteOffset % bytesPerIndex != 0) {
    ErrorInvalidOperation(
        "`byteOffset` must be a multiple of the size of `type`");
    return nullptr;
  }

  ////

  if (IsWebGL2() && !gl->IsSupported(gl::GLFeature::prim_restart_fixed)) {
    MOZ_ASSERT(gl->IsSupported(gl::GLFeature::prim_restart));
    if (mPrimRestartTypeBytes != bytesPerIndex) {
      mPrimRestartTypeBytes = bytesPerIndex;

      const uint32_t ones = UINT32_MAX >> (32 - 8 * mPrimRestartTypeBytes);
      gl->fEnable(LOCAL_GL_PRIMITIVE_RESTART);
      gl->fPrimitiveRestartIndex(ones);
    }
  }

  ////
  // Index fetching

  const auto& indexBuffer = mBoundVertexArray->mElementArrayBuffer;
  if (!indexBuffer) {
    ErrorInvalidOperation("Index buffer not bound.");
    return nullptr;
  }
  MOZ_ASSERT(!indexBuffer->IsBoundForTF(), "This should be impossible.");

  const size_t availBytes = indexBuffer->ByteLength();
  const auto availIndices =
      AvailGroups(availBytes, byteOffset, bytesPerIndex, bytesPerIndex);
  if (instanceCount && indexCount > availIndices) {
    ErrorInvalidOperation("Index buffer too small.");
    return nullptr;
  }

  return indexBuffer.get();
}

static void HandleDrawElementsErrors(
    WebGLContext* webgl, gl::GLContext::LocalErrorScope& errorScope) {
  const auto err = errorScope.GetError();
  if (err == LOCAL_GL_INVALID_OPERATION) {
    webgl->ErrorInvalidOperation(
        "Driver rejected indexed draw call, possibly"
        " due to out-of-bounds indices.");
    return;
  }

  MOZ_ASSERT(!err);
  if (err) {
    webgl->ErrorImplementationBug(
        "Unexpected driver error during indexed draw"
        " call. Please file a bug.");
    return;
  }
}

void WebGLContext::DrawElementsInstanced(GLenum mode, GLsizei indexCount,
                                         GLenum type, WebGLintptr byteOffset,
                                         GLsizei instanceCount) {
  const FuncScope funcScope(*this, "drawElementsInstanced");
  AUTO_PROFILER_LABEL("WebGLContext::DrawElementsInstanced", GRAPHICS);
  if (IsContextLost()) return;

  const gl::GLContext::TlsScope inTls(gl);

  const auto indexBuffer =
      DrawElements_check(indexCount, type, byteOffset, instanceCount);
  if (!indexBuffer) return;

  // -

  const auto fetchLimits = ValidateDraw(this, mode, instanceCount);
  if (!fetchLimits) return;

  bool collapseToDrawArrays = false;
  auto fakeVertCount = fetchLimits->maxVerts;
  if (fetchLimits->maxVerts == UINT64_MAX) {
    // This isn't observable, and keeps FakeVertexAttrib0 sane.
    collapseToDrawArrays = true;
    fakeVertCount = 1;
  }

  // -

  {
    uint64_t indexCapacity = indexBuffer->ByteLength();
    switch (type) {
      case LOCAL_GL_UNSIGNED_BYTE:
        break;
      case LOCAL_GL_UNSIGNED_SHORT:
        indexCapacity /= 2;
        break;
      case LOCAL_GL_UNSIGNED_INT:
        indexCapacity /= 4;
        break;
    }

    uint32_t maxVertId = 0;
    const auto isFetchValid = [&]() {
      if (!indexCount || !instanceCount) return true;

      const auto globalMaxVertId =
          indexBuffer->GetIndexedFetchMaxVert(type, 0, indexCapacity);
      if (!globalMaxVertId) return true;
      if (globalMaxVertId.value() < fetchLimits->maxVerts) return true;

      const auto exactMaxVertId =
          indexBuffer->GetIndexedFetchMaxVert(type, byteOffset, indexCount);
      maxVertId = exactMaxVertId.value();
      return maxVertId < fetchLimits->maxVerts;
    }();
    if (!isFetchValid) {
      ErrorInvalidOperation(
          "Indexed vertex fetch requires %u vertices, but"
          " attribs only supply %u.",
          maxVertId + 1, uint32_t(fetchLimits->maxVerts));
      return;
    }
  }

  // -

  bool error = false;
  const ScopedFakeVertexAttrib0 attrib0(this, fakeVertCount, &error);
  if (error) return;

  const ScopedResolveTexturesForDraw scopedResolve(this, &error);
  if (error) return;

  {
    ScopedDrawCallWrapper wrapper(*this);
    {
      UniquePtr<gl::GLContext::LocalErrorScope> errorScope;
      if (MOZ_UNLIKELY(gl->IsANGLE() &&
                       gl->mDebugFlags &
                           gl::GLContext::DebugFlagAbortOnError)) {
        // ANGLE does range validation even when it doesn't need to.
        // With MOZ_GL_ABORT_ON_ERROR, we need to catch it or hit assertions.
        errorScope.reset(new gl::GLContext::LocalErrorScope(*gl));
      }

      if (indexCount && instanceCount) {
        AUTO_PROFILER_LABEL("glDrawElementsInstanced", GRAPHICS);
        if (HasInstancedDrawing(*this)) {
          if (MOZ_UNLIKELY(collapseToDrawArrays)) {
            gl->fDrawArraysInstanced(mode, 0, 1, instanceCount);
          } else {
            gl->fDrawElementsInstanced(mode, indexCount, type,
                                       reinterpret_cast<GLvoid*>(byteOffset),
                                       instanceCount);
          }
        } else {
          MOZ_ASSERT(instanceCount == 1);
          if (MOZ_UNLIKELY(collapseToDrawArrays)) {
            gl->fDrawArrays(mode, 0, 1);
          } else {
            gl->fDrawElements(mode, indexCount, type,
                              reinterpret_cast<GLvoid*>(byteOffset));
          }
        }
      }

      if (errorScope) {
        HandleDrawElementsErrors(this, *errorScope);
      }
    }
  }

  Draw_cleanup();
}

////////////////////////////////////////

void WebGLContext::Draw_cleanup() {
  if (gl->WorkAroundDriverBugs()) {
    if (gl->Renderer() == gl::GLRenderer::Tegra) {
      mDrawCallsSinceLastFlush++;

      if (mDrawCallsSinceLastFlush >= MAX_DRAW_CALLS_SINCE_FLUSH) {
        gl->fFlush();
        mDrawCallsSinceLastFlush = 0;
      }
    }
  }

  // Let's check for a really common error: Viewport is larger than the actual
  // destination framebuffer.
  uint32_t destWidth;
  uint32_t destHeight;
  if (mBoundDrawFramebuffer) {
    const auto& info = mBoundDrawFramebuffer->GetCompletenessInfo();
    destWidth = info->width;
    destHeight = info->height;
  } else {
    destWidth = mDefaultFB->mSize.width;
    destHeight = mDefaultFB->mSize.height;
  }

  if (mViewportWidth > int32_t(destWidth) ||
      mViewportHeight > int32_t(destHeight)) {
    if (!mAlreadyWarnedAboutViewportLargerThanDest) {
      GenerateWarning(
          "Drawing to a destination rect smaller than the viewport"
          " rect. (This warning will only be given once)");
      mAlreadyWarnedAboutViewportLargerThanDest = true;
    }
  }
}

WebGLVertexAttrib0Status WebGLContext::WhatDoesVertexAttrib0Need() const {
  MOZ_ASSERT(mCurrentProgram);
  MOZ_ASSERT(mActiveProgramLinkInfo);

  bool legacyAttrib0 = gl->IsCompatibilityProfile();
#ifdef XP_MACOSX
  if (gl->WorkAroundDriverBugs()) {
    // Failures in conformance/attribs/gl-disabled-vertex-attrib.
    // Even in Core profiles on NV. Sigh.
    legacyAttrib0 |= (gl->Vendor() == gl::GLVendor::NVIDIA);
  }
#endif

  if (!legacyAttrib0) return WebGLVertexAttrib0Status::Default;

  if (!mActiveProgramLinkInfo->attrib0Active) {
    // Ensure that the legacy code has enough buffer.
    return WebGLVertexAttrib0Status::EmulatedUninitializedArray;
  }

  const auto& isAttribArray0Enabled = mBoundVertexArray->mAttribs[0].mEnabled;
  return isAttribArray0Enabled
             ? WebGLVertexAttrib0Status::Default
             : WebGLVertexAttrib0Status::EmulatedInitializedArray;
}

bool WebGLContext::DoFakeVertexAttrib0(const uint64_t vertexCount) {
  const auto whatDoesAttrib0Need = WhatDoesVertexAttrib0Need();
  if (MOZ_LIKELY(whatDoesAttrib0Need == WebGLVertexAttrib0Status::Default))
    return true;

  if (!mAlreadyWarnedAboutFakeVertexAttrib0) {
    GenerateWarning(
        "Drawing without vertex attrib 0 array enabled forces the browser "
        "to do expensive emulation work when running on desktop OpenGL "
        "platforms, for example on Mac. It is preferable to always draw "
        "with vertex attrib 0 array enabled, by using bindAttribLocation "
        "to bind some always-used attribute to location 0.");
    mAlreadyWarnedAboutFakeVertexAttrib0 = true;
  }

  gl->fEnableVertexAttribArray(0);

  if (!mFakeVertexAttrib0BufferObject) {
    gl->fGenBuffers(1, &mFakeVertexAttrib0BufferObject);
    mFakeVertexAttrib0BufferObjectSize = 0;
  }
  gl->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mFakeVertexAttrib0BufferObject);

  ////

  switch (mGenericVertexAttribTypes[0]) {
    case webgl::AttribBaseType::Boolean:
    case webgl::AttribBaseType::Float:
      gl->fVertexAttribPointer(0, 4, LOCAL_GL_FLOAT, false, 0, 0);
      break;

    case webgl::AttribBaseType::Int:
      gl->fVertexAttribIPointer(0, 4, LOCAL_GL_INT, 0, 0);
      break;

    case webgl::AttribBaseType::UInt:
      gl->fVertexAttribIPointer(0, 4, LOCAL_GL_UNSIGNED_INT, 0, 0);
      break;
  }

  ////

  const auto bytesPerVert = sizeof(mFakeVertexAttrib0Data);
  const auto checked_dataSize = CheckedUint32(vertexCount) * bytesPerVert;
  if (!checked_dataSize.isValid()) {
    ErrorOutOfMemory(
        "Integer overflow trying to construct a fake vertex attrib 0"
        " array for a draw-operation with %" PRIu64
        " vertices. Try"
        " reducing the number of vertices.",
        vertexCount);
    return false;
  }
  const auto dataSize = checked_dataSize.value();

  if (mFakeVertexAttrib0BufferObjectSize < dataSize) {
    gl->fBufferData(LOCAL_GL_ARRAY_BUFFER, dataSize, nullptr,
                    LOCAL_GL_DYNAMIC_DRAW);
    mFakeVertexAttrib0BufferObjectSize = dataSize;
    mFakeVertexAttrib0DataDefined = false;
  }

  if (whatDoesAttrib0Need ==
      WebGLVertexAttrib0Status::EmulatedUninitializedArray)
    return true;

  ////

  if (mFakeVertexAttrib0DataDefined &&
      memcmp(mFakeVertexAttrib0Data, mGenericVertexAttrib0Data, bytesPerVert) ==
          0) {
    return true;
  }

  ////

  const UniqueBuffer data(malloc(dataSize));
  if (!data) {
    ErrorOutOfMemory("Failed to allocate fake vertex attrib 0 array.");
    return false;
  }
  auto itr = (uint8_t*)data.get();
  const auto itrEnd = itr + dataSize;
  while (itr != itrEnd) {
    memcpy(itr, mGenericVertexAttrib0Data, bytesPerVert);
    itr += bytesPerVert;
  }

  {
    gl::GLContext::LocalErrorScope errorScope(*gl);

    gl->fBufferSubData(LOCAL_GL_ARRAY_BUFFER, 0, dataSize, data.get());

    const auto err = errorScope.GetError();
    if (err) {
      ErrorOutOfMemory("Failed to upload fake vertex attrib 0 data.");
      return false;
    }
  }

  ////

  memcpy(mFakeVertexAttrib0Data, mGenericVertexAttrib0Data, bytesPerVert);
  mFakeVertexAttrib0DataDefined = true;
  return true;
}

void WebGLContext::UndoFakeVertexAttrib0() {
  const auto whatDoesAttrib0Need = WhatDoesVertexAttrib0Need();
  if (MOZ_LIKELY(whatDoesAttrib0Need == WebGLVertexAttrib0Status::Default))
    return;

  if (mBoundVertexArray->mAttribs[0].mBuf) {
    const WebGLVertexAttribData& attrib0 = mBoundVertexArray->mAttribs[0];
    gl->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, attrib0.mBuf->mGLName);
    attrib0.DoVertexAttribPointer(gl, 0);
  } else {
    gl->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
  }

  gl->fBindBuffer(LOCAL_GL_ARRAY_BUFFER,
                  mBoundArrayBuffer ? mBoundArrayBuffer->mGLName : 0);
}

}  // namespace mozilla
