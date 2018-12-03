/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"

#include "GLContext.h"
#include "GLScreenBuffer.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/Maybe.h"
#include "mozilla/Preferences.h"
#include "MozFramebuffer.h"
#include "nsString.h"
#include "WebGLBuffer.h"
#include "WebGLContextUtils.h"
#include "WebGLFramebuffer.h"
#include "WebGLProgram.h"
#include "WebGLRenderbuffer.h"
#include "WebGLShader.h"
#include "WebGLTexture.h"
#include "WebGLVertexArray.h"

namespace mozilla {

void WebGLContext::SetEnabled(const char* const funcName, const GLenum cap,
                              const bool enabled) {
  const FuncScope funcScope(*this, funcName);
  if (IsContextLost()) return;

  if (!ValidateCapabilityEnum(cap)) return;

  const auto& slot = GetStateTrackingSlot(cap);
  if (slot) {
    *slot = enabled;
  }

  switch (cap) {
    case LOCAL_GL_DEPTH_TEST:
    case LOCAL_GL_STENCIL_TEST:
      break;  // Lazily applied, so don't tell GL yet or we will desync.

    default:
      // Non-lazy caps.
      gl->SetEnabled(cap, enabled);
      break;
  }
}

bool WebGLContext::GetStencilBits(GLint* const out_stencilBits) const {
  *out_stencilBits = 0;
  if (mBoundDrawFramebuffer) {
    if (!mBoundDrawFramebuffer->IsCheckFramebufferStatusComplete()) {
      // Error, we don't know which stencil buffer's bits to use
      ErrorInvalidFramebufferOperation(
          "getParameter: framebuffer has two stencil buffers bound");
      return false;
    }

    if (mBoundDrawFramebuffer->StencilAttachment().HasAttachment() ||
        mBoundDrawFramebuffer->DepthStencilAttachment().HasAttachment()) {
      *out_stencilBits = 8;
    }
  } else if (mOptions.stencil) {
    *out_stencilBits = 8;
  }

  return true;
}

JS::Value WebGLContext::GetParameter(JSContext* cx, GLenum pname,
                                     ErrorResult& rv) {
  const FuncScope funcScope(*this, "getParameter");

  if (IsContextLost()) return JS::NullValue();

  if (IsWebGL2() || IsExtensionEnabled(WebGLExtensionID::WEBGL_draw_buffers)) {
    if (pname == LOCAL_GL_MAX_COLOR_ATTACHMENTS) {
      return JS::Int32Value(mGLMaxColorAttachments);

    } else if (pname == LOCAL_GL_MAX_DRAW_BUFFERS) {
      return JS::Int32Value(mGLMaxDrawBuffers);

    } else if (pname >= LOCAL_GL_DRAW_BUFFER0 &&
               pname < GLenum(LOCAL_GL_DRAW_BUFFER0 + mGLMaxDrawBuffers)) {
      GLint ret = LOCAL_GL_NONE;
      if (!mBoundDrawFramebuffer) {
        if (pname == LOCAL_GL_DRAW_BUFFER0) {
          ret = mDefaultFB_DrawBuffer0;
        }
      } else {
        gl->fGetIntegerv(pname, &ret);
      }
      return JS::Int32Value(ret);
    }
  }

  if (IsWebGL2() ||
      IsExtensionEnabled(WebGLExtensionID::OES_vertex_array_object)) {
    if (pname == LOCAL_GL_VERTEX_ARRAY_BINDING) {
      WebGLVertexArray* vao = (mBoundVertexArray != mDefaultVertexArray)
                                  ? mBoundVertexArray.get()
                                  : nullptr;
      return WebGLObjectAsJSValue(cx, vao, rv);
    }
  }

  if (IsExtensionEnabled(WebGLExtensionID::EXT_disjoint_timer_query)) {
    switch (pname) {
      case LOCAL_GL_TIMESTAMP_EXT: {
        uint64_t val = 0;
        if (Has64BitTimestamps()) {
          gl->fGetInteger64v(pname, (GLint64*)&val);
        } else {
          gl->fGetIntegerv(pname, (GLint*)&val);
        }
        // TODO: JS doesn't support 64-bit integers. Be lossy and
        // cast to double (53 bits)
        return JS::NumberValue(val);
      }

      case LOCAL_GL_GPU_DISJOINT_EXT: {
        realGLboolean val = false;  // Not disjoint by default.
        if (gl->IsExtensionSupported(gl::GLContext::EXT_disjoint_timer_query)) {
          gl->fGetBooleanv(pname, &val);
        }
        return JS::BooleanValue(val);
      }

      default:
        break;
    }
  }

  // Privileged string params exposed by WEBGL_debug_renderer_info.
  // The privilege check is done in WebGLContext::IsExtensionSupported.
  // So here we just have to check that the extension is enabled.
  if (IsExtensionEnabled(WebGLExtensionID::WEBGL_debug_renderer_info)) {
    switch (pname) {
      case UNMASKED_VENDOR_WEBGL:
      case UNMASKED_RENDERER_WEBGL: {
        const char* overridePref = nullptr;
        GLenum driverEnum = LOCAL_GL_NONE;

        switch (pname) {
          case UNMASKED_RENDERER_WEBGL:
            overridePref = "webgl.renderer-string-override";
            driverEnum = LOCAL_GL_RENDERER;
            break;
          case UNMASKED_VENDOR_WEBGL:
            overridePref = "webgl.vendor-string-override";
            driverEnum = LOCAL_GL_VENDOR;
            break;
          default:
            MOZ_CRASH("GFX: bad `pname`");
        }

        bool hasRetVal = false;

        nsAutoString ret;
        if (overridePref) {
          nsresult res = Preferences::GetString(overridePref, ret);
          if (NS_SUCCEEDED(res) && ret.Length() > 0) hasRetVal = true;
        }

        if (!hasRetVal) {
          const char* chars =
              reinterpret_cast<const char*>(gl->fGetString(driverEnum));
          ret = NS_ConvertASCIItoUTF16(chars);
          hasRetVal = true;
        }

        return StringValue(cx, ret, rv);
      }
    }
  }

  if (IsWebGL2() ||
      IsExtensionEnabled(WebGLExtensionID::OES_standard_derivatives)) {
    if (pname == LOCAL_GL_FRAGMENT_SHADER_DERIVATIVE_HINT) {
      GLint i = 0;
      gl->fGetIntegerv(pname, &i);
      return JS::Int32Value(i);
    }
  }

  if (IsExtensionEnabled(WebGLExtensionID::EXT_texture_filter_anisotropic)) {
    if (pname == LOCAL_GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT) {
      GLfloat f = 0.f;
      gl->fGetFloatv(pname, &f);
      return JS::NumberValue(f);
    }
  }

  switch (pname) {
    //
    // String params
    //
    case LOCAL_GL_VENDOR:
    case LOCAL_GL_RENDERER:
      return StringValue(cx, "Mozilla", rv);
    case LOCAL_GL_VERSION:
      return StringValue(cx, "WebGL 1.0", rv);
    case LOCAL_GL_SHADING_LANGUAGE_VERSION:
      return StringValue(cx, "WebGL GLSL ES 1.0", rv);

    ////////////////////////////////
    // Single-value params

    // unsigned int
    case LOCAL_GL_CULL_FACE_MODE:
    case LOCAL_GL_FRONT_FACE:
    case LOCAL_GL_ACTIVE_TEXTURE:
    case LOCAL_GL_STENCIL_FUNC:
    case LOCAL_GL_STENCIL_FAIL:
    case LOCAL_GL_STENCIL_PASS_DEPTH_FAIL:
    case LOCAL_GL_STENCIL_PASS_DEPTH_PASS:
    case LOCAL_GL_STENCIL_BACK_FUNC:
    case LOCAL_GL_STENCIL_BACK_FAIL:
    case LOCAL_GL_STENCIL_BACK_PASS_DEPTH_FAIL:
    case LOCAL_GL_STENCIL_BACK_PASS_DEPTH_PASS:
    case LOCAL_GL_DEPTH_FUNC:
    case LOCAL_GL_BLEND_SRC_RGB:
    case LOCAL_GL_BLEND_SRC_ALPHA:
    case LOCAL_GL_BLEND_DST_RGB:
    case LOCAL_GL_BLEND_DST_ALPHA:
    case LOCAL_GL_BLEND_EQUATION_RGB:
    case LOCAL_GL_BLEND_EQUATION_ALPHA: {
      GLint i = 0;
      gl->fGetIntegerv(pname, &i);
      return JS::NumberValue(uint32_t(i));
    }

    case LOCAL_GL_GENERATE_MIPMAP_HINT:
      return JS::NumberValue(mGenerateMipmapHint);

    case LOCAL_GL_IMPLEMENTATION_COLOR_READ_FORMAT:
    case LOCAL_GL_IMPLEMENTATION_COLOR_READ_TYPE: {
      const webgl::FormatUsageInfo* usage;
      uint32_t width, height;
      if (!BindCurFBForColorRead(&usage, &width, &height))
        return JS::NullValue();

      const auto implPI = ValidImplementationColorReadPI(usage);

      GLenum ret;
      if (pname == LOCAL_GL_IMPLEMENTATION_COLOR_READ_FORMAT) {
        ret = implPI.format;
      } else {
        ret = implPI.type;
      }
      return JS::NumberValue(uint32_t(ret));
    }

    // int
    case LOCAL_GL_STENCIL_REF:
    case LOCAL_GL_STENCIL_BACK_REF: {
      GLint stencilBits = 0;
      if (!GetStencilBits(&stencilBits)) return JS::NullValue();

      // Assuming stencils have 8 bits
      const GLint stencilMask = (1 << stencilBits) - 1;

      GLint refValue = 0;
      gl->fGetIntegerv(pname, &refValue);

      return JS::Int32Value(refValue & stencilMask);
    }

    case LOCAL_GL_SAMPLE_BUFFERS:
    case LOCAL_GL_SAMPLES: {
      const auto& fb = mBoundDrawFramebuffer;
      auto samples = [&]() -> Maybe<uint32_t> {
        if (!fb) {
          if (!EnsureDefaultFB()) return Nothing();
          return Some(mDefaultFB->mSamples);
        }

        if (!fb->IsCheckFramebufferStatusComplete()) return Some(0);

        DoBindFB(fb, LOCAL_GL_FRAMEBUFFER);
        return Some(gl->GetIntAs<uint32_t>(LOCAL_GL_SAMPLES));
      }();
      if (samples && pname == LOCAL_GL_SAMPLE_BUFFERS) {
        samples = Some(uint32_t(bool(samples.value())));
      }
      if (!samples) return JS::NullValue();
      return JS::NumberValue(samples.value());
    }

    case LOCAL_GL_STENCIL_CLEAR_VALUE:
    case LOCAL_GL_UNPACK_ALIGNMENT:
    case LOCAL_GL_PACK_ALIGNMENT:
    case LOCAL_GL_SUBPIXEL_BITS: {
      GLint i = 0;
      gl->fGetIntegerv(pname, &i);
      return JS::Int32Value(i);
    }

    case LOCAL_GL_RED_BITS:
    case LOCAL_GL_GREEN_BITS:
    case LOCAL_GL_BLUE_BITS:
    case LOCAL_GL_ALPHA_BITS:
    case LOCAL_GL_DEPTH_BITS:
    case LOCAL_GL_STENCIL_BITS: {
      const auto format = [&]() -> const webgl::FormatInfo* {
        const auto& fb = mBoundDrawFramebuffer;
        if (fb) {
          if (!fb->IsCheckFramebufferStatusComplete()) return nullptr;

          const auto& attachment = [&]() -> const auto& {
            switch (pname) {
              case LOCAL_GL_DEPTH_BITS:
                if (fb->DepthStencilAttachment().HasAttachment())
                  return fb->DepthStencilAttachment();
                return fb->DepthAttachment();

              case LOCAL_GL_STENCIL_BITS:
                if (fb->DepthStencilAttachment().HasAttachment())
                  return fb->DepthStencilAttachment();
                return fb->StencilAttachment();

              default:
                return fb->ColorAttachment0();
            }
          }
          ();

          const auto imageInfo = attachment.GetImageInfo();
          if (!imageInfo) return nullptr;
          return imageInfo->mFormat->format;
        }

        auto effFormat = webgl::EffectiveFormat::RGB8;
        switch (pname) {
          case LOCAL_GL_DEPTH_BITS:
            if (mOptions.depth) {
              effFormat = webgl::EffectiveFormat::DEPTH24_STENCIL8;
            }
            break;

          case LOCAL_GL_STENCIL_BITS:
            if (mOptions.stencil) {
              effFormat = webgl::EffectiveFormat::DEPTH24_STENCIL8;
            }
            break;

          default:
            if (mOptions.alpha) {
              effFormat = webgl::EffectiveFormat::RGBA8;
            }
            break;
        }
        return webgl::GetFormat(effFormat);
      }();
      int32_t ret = 0;
      if (format) {
        switch (pname) {
          case LOCAL_GL_RED_BITS:
            ret = format->r;
            break;
          case LOCAL_GL_GREEN_BITS:
            ret = format->g;
            break;
          case LOCAL_GL_BLUE_BITS:
            ret = format->b;
            break;
          case LOCAL_GL_ALPHA_BITS:
            ret = format->a;
            break;
          case LOCAL_GL_DEPTH_BITS:
            ret = format->d;
            break;
          case LOCAL_GL_STENCIL_BITS:
            ret = format->s;
            break;
        }
      }
      return JS::Int32Value(ret);
    }

    case LOCAL_GL_MAX_TEXTURE_SIZE:
      return JS::Int32Value(mGLMaxTextureSize);

    case LOCAL_GL_MAX_CUBE_MAP_TEXTURE_SIZE:
      return JS::Int32Value(mGLMaxCubeMapTextureSize);

    case LOCAL_GL_MAX_RENDERBUFFER_SIZE:
      return JS::Int32Value(mGLMaxRenderbufferSize);

    case LOCAL_GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
      return JS::Int32Value(mGLMaxVertexTextureImageUnits);

    case LOCAL_GL_MAX_TEXTURE_IMAGE_UNITS:
      return JS::Int32Value(mGLMaxFragmentTextureImageUnits);

    case LOCAL_GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
      return JS::Int32Value(mGLMaxCombinedTextureImageUnits);

    case LOCAL_GL_MAX_VERTEX_ATTRIBS:
      return JS::Int32Value(mGLMaxVertexAttribs);

    case LOCAL_GL_MAX_VERTEX_UNIFORM_VECTORS:
      return JS::Int32Value(mGLMaxVertexUniformVectors);

    case LOCAL_GL_MAX_FRAGMENT_UNIFORM_VECTORS:
      return JS::Int32Value(mGLMaxFragmentUniformVectors);

    case LOCAL_GL_MAX_VARYING_VECTORS:
      return JS::Int32Value(mGLMaxVaryingVectors);

    case LOCAL_GL_COMPRESSED_TEXTURE_FORMATS: {
      uint32_t length = mCompressedTextureFormats.Length();
      JSObject* obj = dom::Uint32Array::Create(
          cx, this, length, mCompressedTextureFormats.Elements());
      if (!obj) {
        rv = NS_ERROR_OUT_OF_MEMORY;
      }
      return JS::ObjectOrNullValue(obj);
    }

    // unsigned int. here we may have to return very large values like 2^32-1
    // that can't be represented as javascript integer values. We just return
    // them as doubles and javascript doesn't care.
    case LOCAL_GL_STENCIL_BACK_VALUE_MASK:
      return JS::DoubleValue(
          mStencilValueMaskBack);  // pass as FP value to allow large values
                                   // such as 2^32-1.

    case LOCAL_GL_STENCIL_BACK_WRITEMASK:
      return JS::DoubleValue(mStencilWriteMaskBack);

    case LOCAL_GL_STENCIL_VALUE_MASK:
      return JS::DoubleValue(mStencilValueMaskFront);

    case LOCAL_GL_STENCIL_WRITEMASK:
      return JS::DoubleValue(mStencilWriteMaskFront);

    // float
    case LOCAL_GL_LINE_WIDTH:
      return JS::DoubleValue(mLineWidth);

    case LOCAL_GL_DEPTH_CLEAR_VALUE:
    case LOCAL_GL_POLYGON_OFFSET_FACTOR:
    case LOCAL_GL_POLYGON_OFFSET_UNITS:
    case LOCAL_GL_SAMPLE_COVERAGE_VALUE: {
      GLfloat f = 0.f;
      gl->fGetFloatv(pname, &f);
      return JS::DoubleValue(f);
    }

    // bool
    case LOCAL_GL_DEPTH_TEST:
      return JS::BooleanValue(mDepthTestEnabled);
    case LOCAL_GL_STENCIL_TEST:
      return JS::BooleanValue(mStencilTestEnabled);

    case LOCAL_GL_BLEND:
    case LOCAL_GL_CULL_FACE:
    case LOCAL_GL_DITHER:
    case LOCAL_GL_POLYGON_OFFSET_FILL:
    case LOCAL_GL_SCISSOR_TEST:
    case LOCAL_GL_SAMPLE_COVERAGE_INVERT:
    case LOCAL_GL_SAMPLE_ALPHA_TO_COVERAGE:
    case LOCAL_GL_SAMPLE_COVERAGE:
    case LOCAL_GL_DEPTH_WRITEMASK: {
      realGLboolean b = 0;
      gl->fGetBooleanv(pname, &b);
      return JS::BooleanValue(bool(b));
    }

    // bool, WebGL-specific
    case UNPACK_FLIP_Y_WEBGL:
      return JS::BooleanValue(mPixelStore_FlipY);
    case UNPACK_PREMULTIPLY_ALPHA_WEBGL:
      return JS::BooleanValue(mPixelStore_PremultiplyAlpha);

    // uint, WebGL-specific
    case UNPACK_COLORSPACE_CONVERSION_WEBGL:
      return JS::NumberValue(uint32_t(mPixelStore_ColorspaceConversion));

    ////////////////////////////////
    // Complex values

    // 2 floats
    case LOCAL_GL_DEPTH_RANGE:
    case LOCAL_GL_ALIASED_POINT_SIZE_RANGE:
    case LOCAL_GL_ALIASED_LINE_WIDTH_RANGE: {
      GLfloat fv[2] = {0};
      switch (pname) {
        case LOCAL_GL_ALIASED_POINT_SIZE_RANGE:
          fv[0] = mGLAliasedPointSizeRange[0];
          fv[1] = mGLAliasedPointSizeRange[1];
          break;
        case LOCAL_GL_ALIASED_LINE_WIDTH_RANGE:
          fv[0] = mGLAliasedLineWidthRange[0];
          fv[1] = mGLAliasedLineWidthRange[1];
          break;
        // case LOCAL_GL_DEPTH_RANGE:
        default:
          gl->fGetFloatv(pname, fv);
          break;
      }
      JSObject* obj = dom::Float32Array::Create(cx, this, 2, fv);
      if (!obj) {
        rv = NS_ERROR_OUT_OF_MEMORY;
      }
      return JS::ObjectOrNullValue(obj);
    }

    // 4 floats
    case LOCAL_GL_COLOR_CLEAR_VALUE:
    case LOCAL_GL_BLEND_COLOR: {
      GLfloat fv[4] = {0};
      gl->fGetFloatv(pname, fv);
      JSObject* obj = dom::Float32Array::Create(cx, this, 4, fv);
      if (!obj) {
        rv = NS_ERROR_OUT_OF_MEMORY;
      }
      return JS::ObjectOrNullValue(obj);
    }

    // 2 ints
    case LOCAL_GL_MAX_VIEWPORT_DIMS: {
      GLint iv[2] = {GLint(mGLMaxViewportDims[0]),
                     GLint(mGLMaxViewportDims[1])};
      JSObject* obj = dom::Int32Array::Create(cx, this, 2, iv);
      if (!obj) {
        rv = NS_ERROR_OUT_OF_MEMORY;
      }
      return JS::ObjectOrNullValue(obj);
    }

    // 4 ints
    case LOCAL_GL_SCISSOR_BOX:
    case LOCAL_GL_VIEWPORT: {
      GLint iv[4] = {0};
      gl->fGetIntegerv(pname, iv);
      JSObject* obj = dom::Int32Array::Create(cx, this, 4, iv);
      if (!obj) {
        rv = NS_ERROR_OUT_OF_MEMORY;
      }
      return JS::ObjectOrNullValue(obj);
    }

    // 4 bools
    case LOCAL_GL_COLOR_WRITEMASK: {
      const bool vals[4] = {
          bool(mColorWriteMask & (1 << 0)), bool(mColorWriteMask & (1 << 1)),
          bool(mColorWriteMask & (1 << 2)), bool(mColorWriteMask & (1 << 3))};
      JS::Rooted<JS::Value> arr(cx);
      if (!dom::ToJSValue(cx, vals, &arr)) {
        rv = NS_ERROR_OUT_OF_MEMORY;
      }
      return arr;
    }

    case LOCAL_GL_ARRAY_BUFFER_BINDING: {
      return WebGLObjectAsJSValue(cx, mBoundArrayBuffer.get(), rv);
    }

    case LOCAL_GL_ELEMENT_ARRAY_BUFFER_BINDING: {
      return WebGLObjectAsJSValue(
          cx, mBoundVertexArray->mElementArrayBuffer.get(), rv);
    }

    case LOCAL_GL_RENDERBUFFER_BINDING: {
      return WebGLObjectAsJSValue(cx, mBoundRenderbuffer.get(), rv);
    }

    // DRAW_FRAMEBUFFER_BINDING is the same as FRAMEBUFFER_BINDING.
    case LOCAL_GL_FRAMEBUFFER_BINDING: {
      return WebGLObjectAsJSValue(cx, mBoundDrawFramebuffer.get(), rv);
    }

    case LOCAL_GL_CURRENT_PROGRAM: {
      return WebGLObjectAsJSValue(cx, mCurrentProgram.get(), rv);
    }

    case LOCAL_GL_TEXTURE_BINDING_2D: {
      return WebGLObjectAsJSValue(cx, mBound2DTextures[mActiveTexture].get(),
                                  rv);
    }

    case LOCAL_GL_TEXTURE_BINDING_CUBE_MAP: {
      return WebGLObjectAsJSValue(
          cx, mBoundCubeMapTextures[mActiveTexture].get(), rv);
    }

    default:
      break;
  }

  ErrorInvalidEnumInfo("pname", pname);
  return JS::NullValue();
}

bool WebGLContext::IsEnabled(GLenum cap) {
  const FuncScope funcScope(*this, "isEnabled");
  if (IsContextLost()) return false;

  if (!ValidateCapabilityEnum(cap)) return false;

  const auto& slot = GetStateTrackingSlot(cap);
  if (slot) return *slot;

  return gl->fIsEnabled(cap);
}

bool WebGLContext::ValidateCapabilityEnum(GLenum cap) {
  switch (cap) {
    case LOCAL_GL_BLEND:
    case LOCAL_GL_CULL_FACE:
    case LOCAL_GL_DEPTH_TEST:
    case LOCAL_GL_DITHER:
    case LOCAL_GL_POLYGON_OFFSET_FILL:
    case LOCAL_GL_SAMPLE_ALPHA_TO_COVERAGE:
    case LOCAL_GL_SAMPLE_COVERAGE:
    case LOCAL_GL_SCISSOR_TEST:
    case LOCAL_GL_STENCIL_TEST:
      return true;
    case LOCAL_GL_RASTERIZER_DISCARD:
      return IsWebGL2();
    default:
      ErrorInvalidEnumInfo("cap", cap);
      return false;
  }
}

realGLboolean* WebGLContext::GetStateTrackingSlot(GLenum cap) {
  switch (cap) {
    case LOCAL_GL_DEPTH_TEST:
      return &mDepthTestEnabled;
    case LOCAL_GL_DITHER:
      return &mDitherEnabled;
    case LOCAL_GL_RASTERIZER_DISCARD:
      return &mRasterizerDiscardEnabled;
    case LOCAL_GL_SCISSOR_TEST:
      return &mScissorTestEnabled;
    case LOCAL_GL_STENCIL_TEST:
      return &mStencilTestEnabled;
  }

  return nullptr;
}

}  // namespace mozilla
