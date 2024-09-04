/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContextUtils.h"
#include "WebGLContext.h"
#include "HostWebGLContext.h"
#include "GLContext.h"
#include "jsapi.h"
#include "js/Warnings.h"  // JS::WarnASCII
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/Sprintf.h"
#include "nsPrintfCString.h"
#include "nsServiceManagerUtils.h"
#include <stdarg.h>
#include "WebGLBuffer.h"
#include "WebGLExtensions.h"
#include "WebGLFramebuffer.h"
#include "WebGLProgram.h"
#include "WebGLTexture.h"
#include "WebGLVertexArray.h"

namespace mozilla {

TexTarget TexImageTargetToTexTarget(TexImageTarget texImageTarget) {
  switch (texImageTarget.get()) {
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
      return LOCAL_GL_TEXTURE_CUBE_MAP;

    default:
      return texImageTarget.get();
  }
}

/*static*/ const char* WebGLContext::ErrorName(GLenum error) {
  switch (error) {
    case LOCAL_GL_INVALID_ENUM:
      return "INVALID_ENUM";
    case LOCAL_GL_INVALID_OPERATION:
      return "INVALID_OPERATION";
    case LOCAL_GL_INVALID_VALUE:
      return "INVALID_VALUE";
    case LOCAL_GL_OUT_OF_MEMORY:
      return "OUT_OF_MEMORY";
    case LOCAL_GL_INVALID_FRAMEBUFFER_OPERATION:
      return "INVALID_FRAMEBUFFER_OPERATION";
    case LOCAL_GL_NO_ERROR:
      return "NO_ERROR";
    default:
      MOZ_ASSERT(false);
      return "[unknown WebGL error]";
  }
}

// This version is fallible and will return nullptr if unrecognized.
const char* GetEnumName(const GLenum val, const char* const defaultRet) {
  switch (val) {
    case 0:  // AKA NONE, POINTS
      return "0";

#define XX(x)        \
  case LOCAL_GL_##x: \
    return #x;

      XX(ACTIVE_UNIFORM_BLOCKS)
      XX(ALPHA)
      XX(ALREADY_SIGNALED)
      XX(ANY_SAMPLES_PASSED)
      XX(ANY_SAMPLES_PASSED_CONSERVATIVE)
      XX(BLUE)
      XX(BYTE)
      XX(COLOR)
      XX(COLOR_ATTACHMENT0)
      XX(COLOR_ATTACHMENT1)
      XX(COLOR_ATTACHMENT10)
      XX(COLOR_ATTACHMENT11)
      XX(COLOR_ATTACHMENT12)
      XX(COLOR_ATTACHMENT13)
      XX(COLOR_ATTACHMENT14)
      XX(COLOR_ATTACHMENT15)
      XX(COLOR_ATTACHMENT2)
      XX(COLOR_ATTACHMENT3)
      XX(COLOR_ATTACHMENT4)
      XX(COLOR_ATTACHMENT5)
      XX(COLOR_ATTACHMENT6)
      XX(COLOR_ATTACHMENT7)
      XX(COLOR_ATTACHMENT8)
      XX(COLOR_ATTACHMENT9)
      XX(COMPARE_REF_TO_TEXTURE)
      XX(COMPRESSED_R11_EAC)
      XX(COMPRESSED_RG11_EAC)
      XX(COMPRESSED_RGB8_ETC2)
      XX(COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2)
      XX(COMPRESSED_RGBA8_ETC2_EAC)
      XX(COMPRESSED_RGBA_PVRTC_2BPPV1)
      XX(COMPRESSED_RGBA_PVRTC_4BPPV1)
      XX(COMPRESSED_RGBA_S3TC_DXT1_EXT)
      XX(COMPRESSED_RGBA_S3TC_DXT3_EXT)
      XX(COMPRESSED_RGBA_S3TC_DXT5_EXT)
      XX(COMPRESSED_RGB_PVRTC_2BPPV1)
      XX(COMPRESSED_RGB_PVRTC_4BPPV1)
      XX(COMPRESSED_RGB_S3TC_DXT1_EXT)
      XX(COMPRESSED_SIGNED_R11_EAC)
      XX(COMPRESSED_SIGNED_RG11_EAC)
      XX(COMPRESSED_SRGB8_ALPHA8_ETC2_EAC)
      XX(COMPRESSED_SRGB8_ETC2)
      XX(COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2)
      XX(CONDITION_SATISFIED)
      XX(COPY_READ_BUFFER)
      XX(COPY_WRITE_BUFFER)
      XX(CURRENT_QUERY)
      XX(DEPTH)
      XX(DEPTH24_STENCIL8)
      XX(DEPTH32F_STENCIL8)
      XX(DEPTH_ATTACHMENT)
      XX(DEPTH_COMPONENT)
      XX(DEPTH_COMPONENT16)
      XX(DEPTH_COMPONENT24)
      XX(DEPTH_COMPONENT32)
      XX(DEPTH_COMPONENT32F)
      XX(DEPTH_STENCIL)
      XX(DEPTH_STENCIL_ATTACHMENT)
      XX(DRAW_BUFFER0)
      XX(DRAW_BUFFER1)
      XX(DRAW_BUFFER10)
      XX(DRAW_BUFFER11)
      XX(DRAW_BUFFER12)
      XX(DRAW_BUFFER13)
      XX(DRAW_BUFFER14)
      XX(DRAW_BUFFER15)
      XX(DRAW_BUFFER2)
      XX(DRAW_BUFFER3)
      XX(DRAW_BUFFER4)
      XX(DRAW_BUFFER5)
      XX(DRAW_BUFFER6)
      XX(DRAW_BUFFER7)
      XX(DRAW_BUFFER8)
      XX(DRAW_BUFFER9)
      XX(DRAW_FRAMEBUFFER)
      XX(DRAW_FRAMEBUFFER_BINDING)
      XX(DYNAMIC_COPY)
      XX(DYNAMIC_READ)
      XX(ETC1_RGB8_OES)
      XX(FLOAT)
      XX(FLOAT_32_UNSIGNED_INT_24_8_REV)
      XX(FLOAT_MAT2x3)
      XX(FLOAT_MAT2x4)
      XX(FLOAT_MAT3x2)
      XX(FLOAT_MAT3x4)
      XX(FLOAT_MAT4x2)
      XX(FLOAT_MAT4x3)
      XX(FRAGMENT_SHADER_DERIVATIVE_HINT)
      XX(FRAMEBUFFER)
      XX(FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE)
      XX(FRAMEBUFFER_ATTACHMENT_BLUE_SIZE)
      XX(FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING)
      XX(FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE)
      XX(FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE)
      XX(FRAMEBUFFER_ATTACHMENT_GREEN_SIZE)
      XX(FRAMEBUFFER_ATTACHMENT_RED_SIZE)
      XX(FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE)
      XX(FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER)
      XX(FRAMEBUFFER_DEFAULT)
      XX(FRAMEBUFFER_INCOMPLETE_MULTISAMPLE)
      XX(GREEN)
      XX(HALF_FLOAT)
      XX(INT)
      XX(INTERLEAVED_ATTRIBS)
      XX(INT_2_10_10_10_REV)
      XX(INT_SAMPLER_2D)
      XX(INT_SAMPLER_2D_ARRAY)
      XX(INT_SAMPLER_3D)
      XX(INT_SAMPLER_CUBE)
      XX(LUMINANCE)
      XX(LUMINANCE_ALPHA)
      XX(MAX)
      XX(MAX_3D_TEXTURE_SIZE)
      XX(MAX_ARRAY_TEXTURE_LAYERS)
      XX(MAX_COLOR_ATTACHMENTS)
      XX(MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS)
      XX(MAX_COMBINED_UNIFORM_BLOCKS)
      XX(MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS)
      XX(MAX_DRAW_BUFFERS)
      XX(MAX_ELEMENTS_INDICES)
      XX(MAX_ELEMENTS_VERTICES)
      XX(MAX_ELEMENT_INDEX)
      XX(MAX_FRAGMENT_INPUT_COMPONENTS)
      XX(MAX_FRAGMENT_UNIFORM_BLOCKS)
      XX(MAX_FRAGMENT_UNIFORM_COMPONENTS)
      XX(MAX_PROGRAM_TEXEL_OFFSET)
      XX(MAX_SAMPLES)
      XX(MAX_SERVER_WAIT_TIMEOUT)
      XX(MAX_TEXTURE_LOD_BIAS)
      XX(MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS)
      XX(MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS)
      XX(MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS)
      XX(MAX_UNIFORM_BLOCK_SIZE)
      XX(MAX_UNIFORM_BUFFER_BINDINGS)
      XX(MAX_VARYING_COMPONENTS)
      XX(MAX_VERTEX_OUTPUT_COMPONENTS)
      XX(MAX_VERTEX_UNIFORM_BLOCKS)
      XX(MAX_VERTEX_UNIFORM_COMPONENTS)
      XX(MIN)
      XX(MIN_PROGRAM_TEXEL_OFFSET)
      XX(NUM_SAMPLE_COUNTS)
      XX(OBJECT_TYPE)
      XX(PACK_ROW_LENGTH)
      XX(PACK_SKIP_PIXELS)
      XX(PACK_SKIP_ROWS)
      XX(PIXEL_PACK_BUFFER)
      XX(PIXEL_PACK_BUFFER_BINDING)
      XX(PIXEL_UNPACK_BUFFER)
      XX(PIXEL_UNPACK_BUFFER_BINDING)
      XX(PRIMITIVE_RESTART_FIXED_INDEX)
      XX(QUERY_RESULT)
      XX(QUERY_RESULT_AVAILABLE)
      XX(R11F_G11F_B10F)
      XX(R16F)
      XX(R16I)
      XX(R16UI)
      XX(R32F)
      XX(R32I)
      XX(R32UI)
      XX(R8)
      XX(R8I)
      XX(R8UI)
      XX(R8_SNORM)
      XX(RASTERIZER_DISCARD)
      XX(READ_BUFFER)
      XX(READ_FRAMEBUFFER)
      XX(READ_FRAMEBUFFER_BINDING)
      XX(RED)
      XX(RED_INTEGER)
      XX(RENDERBUFFER_SAMPLES)
      XX(RG)
      XX(RG16F)
      XX(RG16I)
      XX(RG16UI)
      XX(RG32F)
      XX(RG32I)
      XX(RG32UI)
      XX(RG8)
      XX(RG8I)
      XX(RG8UI)
      XX(RG8_SNORM)
      XX(RGB)
      XX(RGB10_A2)
      XX(RGB10_A2UI)
      XX(RGB16F)
      XX(RGB16I)
      XX(RGB16UI)
      XX(RGB32F)
      XX(RGB32I)
      XX(RGB32UI)
      XX(RGB8)
      XX(RGB8I)
      XX(RGB8UI)
      XX(RGB8_SNORM)
      XX(RGB9_E5)
      XX(RGBA)
      XX(RGBA16F)
      XX(RGBA16I)
      XX(RGBA16UI)
      XX(RGBA32F)
      XX(RGBA32I)
      XX(RGBA32UI)
      XX(RGBA8)
      XX(RGBA8I)
      XX(RGBA8UI)
      XX(RGBA8_SNORM)
      XX(RGBA_INTEGER)
      XX(RGB_INTEGER)
      XX(RG_INTEGER)
      XX(SAMPLER_2D)
      XX(SAMPLER_2D_ARRAY)
      XX(SAMPLER_2D_ARRAY_SHADOW)
      XX(SAMPLER_2D_SHADOW)
      XX(SAMPLER_3D)
      XX(SAMPLER_BINDING)
      XX(SAMPLER_CUBE)
      XX(SAMPLER_CUBE_SHADOW)
      XX(SEPARATE_ATTRIBS)
      XX(SHORT)
      XX(SIGNALED)
      XX(SIGNED_NORMALIZED)
      XX(SRGB)
      XX(SRGB8)
      XX(SRGB8_ALPHA8)
      XX(SRGB_ALPHA)
      XX(STATIC_COPY)
      XX(STATIC_READ)
      XX(STENCIL)
      XX(STREAM_COPY)
      XX(STREAM_READ)
      XX(SYNC_CONDITION)
      XX(SYNC_FENCE)
      XX(SYNC_FLAGS)
      XX(SYNC_GPU_COMMANDS_COMPLETE)
      XX(SYNC_STATUS)
      XX(TEXTURE_2D)
      XX(TEXTURE_2D_ARRAY)
      XX(TEXTURE_3D)
      XX(TEXTURE_BASE_LEVEL)
      XX(TEXTURE_BINDING_2D_ARRAY)
      XX(TEXTURE_BINDING_3D)
      XX(TEXTURE_COMPARE_FUNC)
      XX(TEXTURE_COMPARE_MODE)
      XX(TEXTURE_CUBE_MAP)
      XX(TEXTURE_CUBE_MAP_NEGATIVE_X)
      XX(TEXTURE_CUBE_MAP_NEGATIVE_Y)
      XX(TEXTURE_CUBE_MAP_NEGATIVE_Z)
      XX(TEXTURE_CUBE_MAP_POSITIVE_X)
      XX(TEXTURE_CUBE_MAP_POSITIVE_Y)
      XX(TEXTURE_CUBE_MAP_POSITIVE_Z)
      XX(TEXTURE_IMMUTABLE_FORMAT)
      XX(TEXTURE_IMMUTABLE_LEVELS)
      XX(TEXTURE_MAX_LEVEL)
      XX(TEXTURE_MAX_LOD)
      XX(TEXTURE_MIN_LOD)
      XX(TEXTURE_SWIZZLE_A)
      XX(TEXTURE_SWIZZLE_B)
      XX(TEXTURE_SWIZZLE_G)
      XX(TEXTURE_SWIZZLE_R)
      XX(TEXTURE_WRAP_R)
      XX(TIMEOUT_EXPIRED)
      XX(TRANSFORM_FEEDBACK)
      XX(TRANSFORM_FEEDBACK_ACTIVE)
      XX(TRANSFORM_FEEDBACK_BINDING)
      XX(TRANSFORM_FEEDBACK_BUFFER)
      XX(TRANSFORM_FEEDBACK_BUFFER_BINDING)
      XX(TRANSFORM_FEEDBACK_BUFFER_MODE)
      XX(TRANSFORM_FEEDBACK_BUFFER_SIZE)
      XX(TRANSFORM_FEEDBACK_BUFFER_START)
      XX(TRANSFORM_FEEDBACK_PAUSED)
      XX(TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN)
      XX(TRANSFORM_FEEDBACK_VARYINGS)
      XX(UNIFORM_ARRAY_STRIDE)
      XX(UNIFORM_BLOCK_ACTIVE_UNIFORMS)
      XX(UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES)
      XX(UNIFORM_BLOCK_BINDING)
      XX(UNIFORM_BLOCK_DATA_SIZE)
      XX(UNIFORM_BLOCK_INDEX)
      XX(UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER)
      XX(UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER)
      XX(UNIFORM_BUFFER)
      XX(UNIFORM_BUFFER_BINDING)
      XX(UNIFORM_BUFFER_OFFSET_ALIGNMENT)
      XX(UNIFORM_BUFFER_SIZE)
      XX(UNIFORM_BUFFER_START)
      XX(UNIFORM_IS_ROW_MAJOR)
      XX(UNIFORM_MATRIX_STRIDE)
      XX(UNIFORM_OFFSET)
      XX(UNIFORM_SIZE)
      XX(UNIFORM_TYPE)
      XX(UNPACK_IMAGE_HEIGHT)
      XX(UNPACK_ROW_LENGTH)
      XX(UNPACK_SKIP_IMAGES)
      XX(UNPACK_SKIP_PIXELS)
      XX(UNPACK_SKIP_ROWS)
      XX(UNSIGNALED)
      XX(UNSIGNED_BYTE)
      XX(UNSIGNED_INT)
      XX(UNSIGNED_INT_10F_11F_11F_REV)
      XX(UNSIGNED_INT_24_8)
      XX(UNSIGNED_INT_2_10_10_10_REV)
      XX(UNSIGNED_INT_5_9_9_9_REV)
      XX(UNSIGNED_INT_SAMPLER_2D)
      XX(UNSIGNED_INT_SAMPLER_2D_ARRAY)
      XX(UNSIGNED_INT_SAMPLER_3D)
      XX(UNSIGNED_INT_SAMPLER_CUBE)
      XX(UNSIGNED_INT_VEC2)
      XX(UNSIGNED_INT_VEC3)
      XX(UNSIGNED_INT_VEC4)
      XX(UNSIGNED_NORMALIZED)
      XX(UNSIGNED_SHORT)
      XX(UNSIGNED_SHORT_4_4_4_4)
      XX(UNSIGNED_SHORT_5_5_5_1)
      XX(UNSIGNED_SHORT_5_6_5)
      XX(VERTEX_ARRAY_BINDING)
      XX(VERTEX_ATTRIB_ARRAY_DIVISOR)
      XX(VERTEX_ATTRIB_ARRAY_INTEGER)
      XX(WAIT_FAILED)

#undef XX
  }

  return defaultRet;
}

/*static*/
void WebGLContext::EnumName(GLenum val, nsCString* out_name) {
  const char* name = GetEnumName(val, nullptr);
  if (name) {
    *out_name = name;
    return;
  }

  *out_name = nsPrintfCString("<enum 0x%04x>", val);
}

std::string EnumString(const GLenum val) {
  const char* name = GetEnumName(val, nullptr);
  if (name) {
    return name;
  }

  const nsPrintfCString hex("<enum 0x%04x>", val);
  return hex.BeginReading();
}

void WebGLContext::ErrorInvalidEnumArg(const char* const argName,
                                       const GLenum val) const {
  const auto info = nsPrintfCString("Bad `%s`", argName);
  ErrorInvalidEnumInfo(info.BeginReading(), val);
}

void WebGLContext::ErrorInvalidEnumInfo(const char* const info,
                                        const GLenum enumValue) const {
  nsCString name;
  EnumName(enumValue, &name);

  const char* hint = "";
  if (!enumValue) {
    hint = " (Did you typo `gl.SOMETHINGG` and pass `undefined`?)";
  }

  ErrorInvalidEnum("%s: Invalid enum value %s%s", info, name.BeginReading(),
                   hint);
}

#ifdef DEBUG
// For NaNs, etc.
static bool IsCacheCorrect(float cached, float actual) {
  if (std::isnan(cached)) {
    // GL is allowed to do anything it wants for NaNs, so if we're shadowing
    // a NaN, then whatever `actual` is might be correct.
    return true;
  }

  return cached == actual;
}

void AssertUintParamCorrect(gl::GLContext* gl, GLenum pname, GLuint shadow) {
  GLuint val = 0;
  gl->GetUIntegerv(pname, &val);
  if (val != shadow) {
    printf_stderr("Failed 0x%04x shadow: Cached 0x%x/%u, should be 0x%x/%u.\n",
                  pname, shadow, shadow, val, val);
    MOZ_ASSERT(false, "Bad cached value.");
  }
}

void AssertMaskedUintParamCorrect(gl::GLContext* gl, GLenum pname, GLuint mask,
                                  GLuint shadow) {
  GLuint val = 0;
  gl->GetUIntegerv(pname, &val);

  const GLuint valMasked = val & mask;
  const GLuint shadowMasked = shadow & mask;

  if (valMasked != shadowMasked) {
    printf_stderr("Failed 0x%04x shadow: Cached 0x%x/%u, should be 0x%x/%u.\n",
                  pname, shadowMasked, shadowMasked, valMasked, valMasked);
    MOZ_ASSERT(false, "Bad cached value.");
  }
}
#else
void AssertUintParamCorrect(gl::GLContext*, GLenum, GLuint) {}
#endif

void WebGLContext::AssertCachedBindings() const {
#ifdef DEBUG
  gl::GLContext::LocalErrorScope errorScope(*gl);

  GLint stencilBits = 0;
  if (GetStencilBits(&stencilBits)) {  // Depends on current draw framebuffer.
    const GLuint stencilRefMask = (1 << stencilBits) - 1;

    AssertMaskedUintParamCorrect(gl, LOCAL_GL_STENCIL_REF, stencilRefMask,
                                 mStencilRefFront);
    AssertMaskedUintParamCorrect(gl, LOCAL_GL_STENCIL_BACK_REF, stencilRefMask,
                                 mStencilRefBack);
  }

  // Program
  GLuint bound = mCurrentProgram ? mCurrentProgram->mGLName : 0;
  AssertUintParamCorrect(gl, LOCAL_GL_CURRENT_PROGRAM, bound);

  // Buffers
  bound = mBoundArrayBuffer ? mBoundArrayBuffer->mGLName : 0;
  AssertUintParamCorrect(gl, LOCAL_GL_ARRAY_BUFFER_BINDING, bound);

  MOZ_ASSERT(mBoundVertexArray);
  WebGLBuffer* curBuff = mBoundVertexArray->mElementArrayBuffer;
  bound = curBuff ? curBuff->mGLName : 0;
  AssertUintParamCorrect(gl, LOCAL_GL_ELEMENT_ARRAY_BUFFER_BINDING, bound);

  MOZ_ASSERT(!gl::GLContext::IsBadCallError(errorScope.GetError()));
#endif

  // We do not check the renderbuffer binding, because we never rely on it
  // matching.
}

void WebGLContext::AssertCachedGlobalState() const {
#ifdef DEBUG
  gl::GLContext::LocalErrorScope errorScope(*gl);

  ////////////////

  // Draw state
  MOZ_ASSERT(gl->fIsEnabled(LOCAL_GL_DITHER) == mDitherEnabled);
  MOZ_ASSERT_IF(IsWebGL2(), gl->fIsEnabled(LOCAL_GL_RASTERIZER_DISCARD) ==
                                mRasterizerDiscardEnabled);
  MOZ_ASSERT(gl->fIsEnabled(LOCAL_GL_SCISSOR_TEST) == mScissorTestEnabled);

  // Cannot trivially check COLOR_CLEAR_VALUE, since in old GL versions glGet
  // may clamp based on whether the current framebuffer is floating-point or
  // not. This also means COLOR_CLEAR_VALUE save+restore is dangerous!

  realGLboolean depthWriteMask = 0;
  gl->fGetBooleanv(LOCAL_GL_DEPTH_WRITEMASK, &depthWriteMask);
  MOZ_ASSERT(depthWriteMask == mDepthWriteMask);

  GLfloat depthClearValue = 0.0f;
  gl->fGetFloatv(LOCAL_GL_DEPTH_CLEAR_VALUE, &depthClearValue);
  MOZ_ASSERT(IsCacheCorrect(mDepthClearValue, depthClearValue));

  const int maxStencilBits = 8;
  const GLuint maxStencilBitsMask = (1 << maxStencilBits) - 1;
  AssertMaskedUintParamCorrect(gl, LOCAL_GL_STENCIL_CLEAR_VALUE,
                               maxStencilBitsMask, mStencilClearValue);

  // GLES 3.0.4, $4.1.4, p177:
  //   [...] the front and back stencil mask are both set to the value `2^s -
  //   1`, where `s` is greater than or equal to the number of bits in the
  //   deepest stencil buffer supported by the GL implementation.
  AssertMaskedUintParamCorrect(gl, LOCAL_GL_STENCIL_VALUE_MASK,
                               maxStencilBitsMask, mStencilValueMaskFront);
  AssertMaskedUintParamCorrect(gl, LOCAL_GL_STENCIL_BACK_VALUE_MASK,
                               maxStencilBitsMask, mStencilValueMaskBack);

  AssertMaskedUintParamCorrect(gl, LOCAL_GL_STENCIL_WRITEMASK,
                               maxStencilBitsMask, mStencilWriteMaskFront);
  AssertMaskedUintParamCorrect(gl, LOCAL_GL_STENCIL_BACK_WRITEMASK,
                               maxStencilBitsMask, mStencilWriteMaskBack);

  // Viewport
  GLint int4[4] = {0, 0, 0, 0};
  gl->fGetIntegerv(LOCAL_GL_VIEWPORT, int4);
  MOZ_ASSERT(int4[0] == mViewportX && int4[1] == mViewportY &&
             int4[2] == mViewportWidth && int4[3] == mViewportHeight);

  MOZ_ASSERT(!gl::GLContext::IsBadCallError(errorScope.GetError()));
#endif
}

const char* InfoFrom(WebGLTexImageFunc func, WebGLTexDimensions dims) {
  switch (dims) {
    case WebGLTexDimensions::Tex2D:
      switch (func) {
        case WebGLTexImageFunc::TexImage:
          return "texImage2D";
        case WebGLTexImageFunc::TexSubImage:
          return "texSubImage2D";
        case WebGLTexImageFunc::CopyTexImage:
          return "copyTexImage2D";
        case WebGLTexImageFunc::CopyTexSubImage:
          return "copyTexSubImage2D";
        case WebGLTexImageFunc::CompTexImage:
          return "compressedTexImage2D";
        case WebGLTexImageFunc::CompTexSubImage:
          return "compressedTexSubImage2D";
        default:
          MOZ_CRASH("GFX: invalid 2D TexDimensions");
      }
    case WebGLTexDimensions::Tex3D:
      switch (func) {
        case WebGLTexImageFunc::TexImage:
          return "texImage3D";
        case WebGLTexImageFunc::TexSubImage:
          return "texSubImage3D";
        case WebGLTexImageFunc::CopyTexSubImage:
          return "copyTexSubImage3D";
        case WebGLTexImageFunc::CompTexSubImage:
          return "compressedTexSubImage3D";
        default:
          MOZ_CRASH("GFX: invalid 3D TexDimensions");
      }
    default:
      MOZ_CRASH("GFX: invalid TexDimensions");
  }
}

////

}  // namespace mozilla
