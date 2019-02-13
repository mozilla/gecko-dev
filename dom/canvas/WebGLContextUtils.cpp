/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"

#include "GLContext.h"
#include "jsapi.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/Preferences.h"
#include "nsIDOMDataContainerEvent.h"
#include "nsIDOMEvent.h"
#include "nsIScriptSecurityManager.h"
#include "nsIVariant.h"
#include "nsPrintfCString.h"
#include "nsServiceManagerUtils.h"
#include "prprf.h"
#include <stdarg.h>
#include "WebGLBuffer.h"
#include "WebGLExtensions.h"
#include "WebGLFramebuffer.h"
#include "WebGLProgram.h"
#include "WebGLTexture.h"
#include "WebGLVertexArray.h"
#include "WebGLContextUtils.h"

namespace mozilla {

using namespace gl;

bool
IsGLDepthFormat(TexInternalFormat internalformat)
{
    TexInternalFormat unsizedformat = UnsizedInternalFormatFromInternalFormat(internalformat);
    return unsizedformat == LOCAL_GL_DEPTH_COMPONENT;
}

bool
IsGLDepthStencilFormat(TexInternalFormat internalformat)
{
    TexInternalFormat unsizedformat = UnsizedInternalFormatFromInternalFormat(internalformat);
    return unsizedformat == LOCAL_GL_DEPTH_STENCIL;
}

bool
FormatHasAlpha(TexInternalFormat internalformat)
{
    TexInternalFormat unsizedformat = UnsizedInternalFormatFromInternalFormat(internalformat);
    return unsizedformat == LOCAL_GL_RGBA ||
           unsizedformat == LOCAL_GL_LUMINANCE_ALPHA ||
           unsizedformat == LOCAL_GL_ALPHA ||
           unsizedformat == LOCAL_GL_SRGB_ALPHA ||
           unsizedformat == LOCAL_GL_RGBA_INTEGER;
}

TexTarget
TexImageTargetToTexTarget(TexImageTarget texImageTarget)
{
    switch (texImageTarget.get()) {
    case LOCAL_GL_TEXTURE_2D:
    case LOCAL_GL_TEXTURE_3D:
        return texImageTarget.get();
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        return LOCAL_GL_TEXTURE_CUBE_MAP;
    default:
        MOZ_ASSERT(false, "Bad texture target");
        // Should be caught by the constructor for TexTarget
        return LOCAL_GL_NONE;
    }
}

JS::Value
StringValue(JSContext* cx, const char* chars, ErrorResult& rv)
{
    JSString* str = JS_NewStringCopyZ(cx, chars);
    if (!str) {
        rv.Throw(NS_ERROR_OUT_OF_MEMORY);
        return JS::NullValue();
    }

    return JS::StringValue(str);
}

GLComponents::GLComponents(TexInternalFormat internalformat)
{
    TexInternalFormat unsizedformat = UnsizedInternalFormatFromInternalFormat(internalformat);
    mComponents = 0;

    switch (unsizedformat.get()) {
    case LOCAL_GL_RGBA:
    case LOCAL_GL_RGBA4:
    case LOCAL_GL_RGBA8:
    case LOCAL_GL_RGB5_A1:
    // Luminance + Alpha can be converted
    // to and from RGBA
    case LOCAL_GL_LUMINANCE_ALPHA:
        mComponents |= Components::Alpha;
    // Drops through
    case LOCAL_GL_RGB:
    case LOCAL_GL_RGB565:
    // Luminance can be converted to and from RGB
    case LOCAL_GL_LUMINANCE:
        mComponents |= Components::Red | Components::Green | Components::Blue;
        break;
    case LOCAL_GL_ALPHA:
        mComponents |= Components::Alpha;
        break;
    case LOCAL_GL_DEPTH_COMPONENT:
        mComponents |= Components::Depth;
        break;
    case LOCAL_GL_DEPTH_STENCIL:
        mComponents |= Components::Stencil;
        break;
    default:
        MOZ_ASSERT(false, "Unhandled case - GLComponents");
        break;
    }
}

bool
GLComponents::IsSubsetOf(const GLComponents& other) const
{
    return (mComponents | other.mComponents) == other.mComponents;
}

TexType
TypeFromInternalFormat(TexInternalFormat internalformat)
{
#define HANDLE_WEBGL_INTERNAL_FORMAT(table_effectiveinternalformat, table_internalformat, table_type) \
    if (internalformat == table_effectiveinternalformat) { \
        return table_type; \
    }

#include "WebGLInternalFormatsTable.h"

    // if we're here, then internalformat is not an effective internalformat i.e. is an unsized internalformat.
    return LOCAL_GL_NONE; // no size, no type
}

TexInternalFormat
UnsizedInternalFormatFromInternalFormat(TexInternalFormat internalformat)
{
#define HANDLE_WEBGL_INTERNAL_FORMAT(table_effectiveinternalformat, table_internalformat, table_type) \
    if (internalformat == table_effectiveinternalformat) { \
        return table_internalformat; \
    }

#include "WebGLInternalFormatsTable.h"

    // if we're here, then internalformat is not an effective internalformat i.e. is an unsized internalformat.
    // so we can just return it.
    return internalformat;
}

/*
 * Note that the following two functions are inverse of each other:
 * EffectiveInternalFormatFromInternalFormatAndType and
 * InternalFormatAndTypeFromEffectiveInternalFormat both implement OpenGL ES 3.0.3 Table 3.2
 * but in opposite directions.
 */
TexInternalFormat
EffectiveInternalFormatFromUnsizedInternalFormatAndType(TexInternalFormat internalformat,
                                                        TexType type)
{
    MOZ_ASSERT(TypeFromInternalFormat(internalformat) == LOCAL_GL_NONE);

#define HANDLE_WEBGL_INTERNAL_FORMAT(table_effectiveinternalformat, table_internalformat, table_type) \
    if (internalformat == table_internalformat && type == table_type) { \
        return table_effectiveinternalformat; \
    }

#include "WebGLInternalFormatsTable.h"

    // If we're here, that means that type was incompatible with the given internalformat.
    return LOCAL_GL_NONE;
}

void
UnsizedInternalFormatAndTypeFromEffectiveInternalFormat(TexInternalFormat effectiveinternalformat,
                                                        TexInternalFormat* const out_internalformat,
                                                        TexType* const out_type)
{
    MOZ_ASSERT(TypeFromInternalFormat(effectiveinternalformat) != LOCAL_GL_NONE);

    MOZ_ASSERT(out_internalformat);
    MOZ_ASSERT(out_type);

    GLenum internalformat = LOCAL_GL_NONE;
    GLenum type = LOCAL_GL_NONE;

    switch (effectiveinternalformat.get()) {

#define HANDLE_WEBGL_INTERNAL_FORMAT(table_effectiveinternalformat, table_internalformat, table_type) \
    case table_effectiveinternalformat: \
        internalformat = table_internalformat; \
        type = table_type; \
        break;

#include "WebGLInternalFormatsTable.h"

        default:
            MOZ_CRASH(); // impossible to get here
    }

    *out_internalformat = internalformat;
    *out_type = type;
}

TexInternalFormat
EffectiveInternalFormatFromInternalFormatAndType(TexInternalFormat internalformat,
                                                 TexType type)
{
    TexType typeOfInternalFormat = TypeFromInternalFormat(internalformat);
    if (typeOfInternalFormat == LOCAL_GL_NONE)
        return EffectiveInternalFormatFromUnsizedInternalFormatAndType(internalformat, type);

    if (typeOfInternalFormat == type)
        return internalformat;

    return LOCAL_GL_NONE;
}

/**
 * Convert effective internalformat into GL function parameters
 * valid for underlying driver.
 */
void
DriverFormatsFromEffectiveInternalFormat(gl::GLContext* gl,
                                         TexInternalFormat effectiveinternalformat,
                                         GLenum* const out_driverInternalFormat,
                                         GLenum* const out_driverFormat,
                                         GLenum* const out_driverType)
{
    MOZ_ASSERT(out_driverInternalFormat);
    MOZ_ASSERT(out_driverFormat);
    MOZ_ASSERT(out_driverType);

    TexInternalFormat unsizedinternalformat = LOCAL_GL_NONE;
    TexType type = LOCAL_GL_NONE;

    UnsizedInternalFormatAndTypeFromEffectiveInternalFormat(effectiveinternalformat,
                                                            &unsizedinternalformat,
                                                            &type);

    // driverType: almost always the generic type that we just got, except on ES
    // we must replace HALF_FLOAT by HALF_FLOAT_OES
    GLenum driverType = type.get();
    if (gl->IsGLES() && type == LOCAL_GL_HALF_FLOAT)
        driverType = LOCAL_GL_HALF_FLOAT_OES;

    // driverFormat: always just the unsized internalformat that we just got
    GLenum driverFormat = unsizedinternalformat.get();

    // driverInternalFormat: almost always the same as driverFormat, but on desktop GL,
    // in some cases we must pass a different value. On ES, they are equal by definition
    // as it is an error to pass internalformat!=format.
    GLenum driverInternalFormat = driverFormat;
    if (gl->IsCompatibilityProfile()) {
        // Cases where desktop OpenGL requires a tweak to 'format'
        if (driverFormat == LOCAL_GL_SRGB)
            driverFormat = LOCAL_GL_RGB;
        else if (driverFormat == LOCAL_GL_SRGB_ALPHA)
            driverFormat = LOCAL_GL_RGBA;

        // WebGL2's new formats are not legal values for internalformat,
        // as using unsized internalformat is deprecated.
        if (driverFormat == LOCAL_GL_RED ||
            driverFormat == LOCAL_GL_RG ||
            driverFormat == LOCAL_GL_RED_INTEGER ||
            driverFormat == LOCAL_GL_RG_INTEGER ||
            driverFormat == LOCAL_GL_RGB_INTEGER ||
            driverFormat == LOCAL_GL_RGBA_INTEGER)
        {
            driverInternalFormat = effectiveinternalformat.get();
        }

        // Cases where desktop OpenGL requires a sized internalformat,
        // as opposed to the unsized internalformat that had the same
        // GLenum value as 'format', in order to get the precise
        // semantics that we want. For example, for floating-point formats,
        // we seem to need a sized internalformat to get non-clamped floating
        // point texture sampling. Can't find the spec reference for that,
        // but that's at least the case on my NVIDIA driver version 331.
        if (unsizedinternalformat == LOCAL_GL_DEPTH_COMPONENT ||
            unsizedinternalformat == LOCAL_GL_DEPTH_STENCIL ||
            type == LOCAL_GL_FLOAT ||
            type == LOCAL_GL_HALF_FLOAT)
        {
            driverInternalFormat = effectiveinternalformat.get();
        }
    }

    // OpenGL core profile removed texture formats ALPHA, LUMINANCE and LUMINANCE_ALPHA
    if (gl->IsCoreProfile()) {
        switch (driverFormat) {
        case LOCAL_GL_ALPHA:
        case LOCAL_GL_LUMINANCE:
            driverInternalFormat = driverFormat = LOCAL_GL_RED;
            break;

        case LOCAL_GL_LUMINANCE_ALPHA:
            driverInternalFormat = driverFormat = LOCAL_GL_RG;
            break;
        }
    }

    *out_driverInternalFormat = driverInternalFormat;
    *out_driverFormat = driverFormat;
    *out_driverType = driverType;
}

// Map R to A
static const GLenum kLegacyAlphaSwizzle[4] = {
    LOCAL_GL_ZERO, LOCAL_GL_ZERO, LOCAL_GL_ZERO, LOCAL_GL_RED
};
// Map R to RGB
static const GLenum kLegacyLuminanceSwizzle[4] = {
    LOCAL_GL_RED, LOCAL_GL_RED, LOCAL_GL_RED, LOCAL_GL_ONE
};
// Map R to RGB, G to A
static const GLenum kLegacyLuminanceAlphaSwizzle[4] = {
    LOCAL_GL_RED, LOCAL_GL_RED, LOCAL_GL_RED, LOCAL_GL_GREEN
};

void
SetLegacyTextureSwizzle(gl::GLContext* gl, GLenum target, GLenum internalformat)
{
    if (!gl->IsCoreProfile())
        return;

    /* Only support swizzling on core profiles. */
    // Bug 1159117: Fix this.
    // MOZ_RELEASE_ASSERT(gl->IsSupported(gl::GLFeature::texture_swizzle));

    switch (internalformat) {
    case LOCAL_GL_ALPHA:
        gl->fTexParameteriv(target, LOCAL_GL_TEXTURE_SWIZZLE_RGBA,
                            (GLint*) kLegacyAlphaSwizzle);
        break;

    case LOCAL_GL_LUMINANCE:
        gl->fTexParameteriv(target, LOCAL_GL_TEXTURE_SWIZZLE_RGBA,
                            (GLint*) kLegacyLuminanceSwizzle);
        break;

    case LOCAL_GL_LUMINANCE_ALPHA:
        gl->fTexParameteriv(target, LOCAL_GL_TEXTURE_SWIZZLE_RGBA,
                            (GLint*) kLegacyLuminanceAlphaSwizzle);
        break;
    }
}

/**
 * Return the bits per texel for format & type combination.
 * Assumes that format & type are a valid combination as checked with
 * ValidateTexImageFormatAndType().
 */
size_t
GetBitsPerTexel(TexInternalFormat effectiveinternalformat)
{
    switch (effectiveinternalformat.get()) {
    case LOCAL_GL_COMPRESSED_RGB_PVRTC_2BPPV1:
    case LOCAL_GL_COMPRESSED_RGBA_PVRTC_2BPPV1:
        return 2;

    case LOCAL_GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case LOCAL_GL_ATC_RGB:
    case LOCAL_GL_COMPRESSED_RGB_PVRTC_4BPPV1:
    case LOCAL_GL_COMPRESSED_RGBA_PVRTC_4BPPV1:
    case LOCAL_GL_ETC1_RGB8_OES:
        return 4;

    case LOCAL_GL_ALPHA8:
    case LOCAL_GL_LUMINANCE8:
    case LOCAL_GL_R8:
    case LOCAL_GL_R8I:
    case LOCAL_GL_R8UI:
    case LOCAL_GL_R8_SNORM:
    case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
    case LOCAL_GL_ATC_RGBA_EXPLICIT_ALPHA:
    case LOCAL_GL_ATC_RGBA_INTERPOLATED_ALPHA:
        return 8;

    case LOCAL_GL_LUMINANCE8_ALPHA8:
    case LOCAL_GL_RGBA4:
    case LOCAL_GL_RGB5_A1:
    case LOCAL_GL_DEPTH_COMPONENT16:
    case LOCAL_GL_RG8:
    case LOCAL_GL_R16I:
    case LOCAL_GL_R16UI:
    case LOCAL_GL_RGB565:
    case LOCAL_GL_R16F:
    case LOCAL_GL_RG8I:
    case LOCAL_GL_RG8UI:
    case LOCAL_GL_RG8_SNORM:
    case LOCAL_GL_ALPHA16F_EXT:
    case LOCAL_GL_LUMINANCE16F_EXT:
        return 16;

    case LOCAL_GL_RGB8:
    case LOCAL_GL_DEPTH_COMPONENT24:
    case LOCAL_GL_SRGB8:
    case LOCAL_GL_RGB8UI:
    case LOCAL_GL_RGB8I:
    case LOCAL_GL_RGB8_SNORM:
        return 24;

    case LOCAL_GL_RGBA8:
    case LOCAL_GL_RGB10_A2:
    case LOCAL_GL_R32F:
    case LOCAL_GL_RG16F:
    case LOCAL_GL_R32I:
    case LOCAL_GL_R32UI:
    case LOCAL_GL_RG16I:
    case LOCAL_GL_RG16UI:
    case LOCAL_GL_DEPTH24_STENCIL8:
    case LOCAL_GL_R11F_G11F_B10F:
    case LOCAL_GL_RGB9_E5:
    case LOCAL_GL_SRGB8_ALPHA8:
    case LOCAL_GL_DEPTH_COMPONENT32F:
    case LOCAL_GL_RGBA8UI:
    case LOCAL_GL_RGBA8I:
    case LOCAL_GL_RGBA8_SNORM:
    case LOCAL_GL_RGB10_A2UI:
    case LOCAL_GL_LUMINANCE_ALPHA16F_EXT:
    case LOCAL_GL_ALPHA32F_EXT:
    case LOCAL_GL_LUMINANCE32F_EXT:
        return 32;

    case LOCAL_GL_DEPTH32F_STENCIL8:
        return 40;

    case LOCAL_GL_RGB16F:
    case LOCAL_GL_RGB16UI:
    case LOCAL_GL_RGB16I:
        return 48;

    case LOCAL_GL_RG32F:
    case LOCAL_GL_RG32I:
    case LOCAL_GL_RG32UI:
    case LOCAL_GL_RGBA16F:
    case LOCAL_GL_RGBA16UI:
    case LOCAL_GL_RGBA16I:
    case LOCAL_GL_LUMINANCE_ALPHA32F_EXT:
        return 64;

    case LOCAL_GL_RGB32F:
    case LOCAL_GL_RGB32UI:
    case LOCAL_GL_RGB32I:
        return 96;

    case LOCAL_GL_RGBA32F:
    case LOCAL_GL_RGBA32UI:
    case LOCAL_GL_RGBA32I:
        return 128;

    default:
        MOZ_ASSERT(false, "Unhandled format");
        return 0;
    }
}

void
WebGLContext::GenerateWarning(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    GenerateWarning(fmt, ap);

    va_end(ap);
}

void
WebGLContext::GenerateWarning(const char* fmt, va_list ap)
{
    if (!ShouldGenerateWarnings())
        return;

    mAlreadyGeneratedWarnings++;

    char buf[1024];
    PR_vsnprintf(buf, 1024, fmt, ap);

    // no need to print to stderr, as JS_ReportWarning takes care of this for us.

    if (!mCanvasElement) {
        return;
    }

    AutoJSAPI api;
    if (!api.Init(mCanvasElement->OwnerDoc()->GetScopeObject())) {
        return;
    }

    JSContext* cx = api.cx();
    JS_ReportWarning(cx, "WebGL: %s", buf);
    if (!ShouldGenerateWarnings()) {
        JS_ReportWarning(cx,
                         "WebGL: No further warnings will be reported for this"
                         " WebGL context. (already reported %d warnings)",
                         mAlreadyGeneratedWarnings);
    }
}

bool
WebGLContext::ShouldGenerateWarnings() const
{
    if (mMaxWarnings == -1)
        return true;

    return mAlreadyGeneratedWarnings < mMaxWarnings;
}

CheckedUint32
WebGLContext::GetImageSize(GLsizei height, GLsizei width, GLsizei depth,
                           uint32_t pixelSize, uint32_t packOrUnpackAlignment)
{
    CheckedUint32 checked_plainRowSize = CheckedUint32(width) * pixelSize;

    // alignedRowSize = row size rounded up to next multiple of packAlignment
    CheckedUint32 checked_alignedRowSize = RoundedToNextMultipleOf(checked_plainRowSize, packOrUnpackAlignment);

    // if height is 0, we don't need any memory to store this; without this check, we'll get an overflow
    CheckedUint32 checked_2dImageSize = 0;
    if (height >= 1) {
        checked_2dImageSize = (height-1) * checked_alignedRowSize +
                              checked_plainRowSize;
    }

    // FIXME - we should honor UNPACK_IMAGE_HEIGHT
    CheckedUint32 checked_imageSize = checked_2dImageSize * depth;
    return checked_imageSize;
}

void
WebGLContext::SynthesizeGLError(GLenum err)
{
    /* ES2 section 2.5 "GL Errors" states that implementations can have
     * multiple 'flags', as errors might be caught in different parts of
     * a distributed implementation.
     * We're signing up as a distributed implementation here, with
     * separate flags for WebGL and the underlying GLContext.
     */
    if (!mWebGLError)
        mWebGLError = err;
}

void
WebGLContext::SynthesizeGLError(GLenum err, const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    GenerateWarning(fmt, va);
    va_end(va);

    return SynthesizeGLError(err);
}

void
WebGLContext::ErrorInvalidEnum(const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    GenerateWarning(fmt, va);
    va_end(va);

    return SynthesizeGLError(LOCAL_GL_INVALID_ENUM);
}

void
WebGLContext::ErrorInvalidEnumInfo(const char* info, GLenum enumValue)
{
    nsCString name;
    EnumName(enumValue, &name);

    return ErrorInvalidEnum("%s: invalid enum value %s", info, name.BeginReading());
}

void
WebGLContext::ErrorInvalidEnumInfo(const char* info, const char* funcName,
                                   GLenum enumValue)
{
    nsCString name;
    EnumName(enumValue, &name);

    ErrorInvalidEnum("%s: %s: Invalid enum: 0x%04x (%s).", funcName, info,
                     enumValue, name.BeginReading());
}

void
WebGLContext::ErrorInvalidOperation(const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    GenerateWarning(fmt, va);
    va_end(va);

    return SynthesizeGLError(LOCAL_GL_INVALID_OPERATION);
}

void
WebGLContext::ErrorInvalidValue(const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    GenerateWarning(fmt, va);
    va_end(va);

    return SynthesizeGLError(LOCAL_GL_INVALID_VALUE);
}

void
WebGLContext::ErrorInvalidFramebufferOperation(const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    GenerateWarning(fmt, va);
    va_end(va);

    return SynthesizeGLError(LOCAL_GL_INVALID_FRAMEBUFFER_OPERATION);
}

void
WebGLContext::ErrorOutOfMemory(const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    GenerateWarning(fmt, va);
    va_end(va);

    return SynthesizeGLError(LOCAL_GL_OUT_OF_MEMORY);
}

const char*
WebGLContext::ErrorName(GLenum error)
{
    switch(error) {
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

// This version is 'fallible' and will return NULL if glenum is not recognized.
const char*
WebGLContext::EnumName(GLenum glenum)
{
    switch (glenum) {
#define XX(x) case LOCAL_GL_##x: return #x
        XX(ALPHA);
        XX(ATC_RGB);
        XX(ATC_RGBA_EXPLICIT_ALPHA);
        XX(ATC_RGBA_INTERPOLATED_ALPHA);
        XX(COMPRESSED_RGBA_PVRTC_2BPPV1);
        XX(COMPRESSED_RGBA_PVRTC_4BPPV1);
        XX(COMPRESSED_RGBA_S3TC_DXT1_EXT);
        XX(COMPRESSED_RGBA_S3TC_DXT3_EXT);
        XX(COMPRESSED_RGBA_S3TC_DXT5_EXT);
        XX(COMPRESSED_RGB_PVRTC_2BPPV1);
        XX(COMPRESSED_RGB_PVRTC_4BPPV1);
        XX(COMPRESSED_RGB_S3TC_DXT1_EXT);
        XX(DEPTH_ATTACHMENT);
        XX(DEPTH_COMPONENT);
        XX(DEPTH_COMPONENT16);
        XX(DEPTH_COMPONENT32);
        XX(DEPTH_STENCIL);
        XX(DEPTH24_STENCIL8);
        XX(DRAW_FRAMEBUFFER);
        XX(ETC1_RGB8_OES);
        XX(FLOAT);
        XX(FRAMEBUFFER);
        XX(HALF_FLOAT);
        XX(LUMINANCE);
        XX(LUMINANCE_ALPHA);
        XX(READ_FRAMEBUFFER);
        XX(RGB);
        XX(RGB16F);
        XX(RGB32F);
        XX(RGBA);
        XX(RGBA16F);
        XX(RGBA32F);
        XX(SRGB);
        XX(SRGB_ALPHA);
        XX(TEXTURE_2D);
        XX(TEXTURE_3D);
        XX(TEXTURE_CUBE_MAP);
        XX(TEXTURE_CUBE_MAP_NEGATIVE_X);
        XX(TEXTURE_CUBE_MAP_NEGATIVE_Y);
        XX(TEXTURE_CUBE_MAP_NEGATIVE_Z);
        XX(TEXTURE_CUBE_MAP_POSITIVE_X);
        XX(TEXTURE_CUBE_MAP_POSITIVE_Y);
        XX(TEXTURE_CUBE_MAP_POSITIVE_Z);
        XX(UNSIGNED_BYTE);
        XX(UNSIGNED_INT);
        XX(UNSIGNED_INT_24_8);
        XX(UNSIGNED_SHORT);
        XX(UNSIGNED_SHORT_4_4_4_4);
        XX(UNSIGNED_SHORT_5_5_5_1);
        XX(UNSIGNED_SHORT_5_6_5);
        XX(READ_BUFFER);
        XX(UNPACK_ROW_LENGTH);
        XX(UNPACK_SKIP_ROWS);
        XX(UNPACK_SKIP_PIXELS);
        XX(PACK_ROW_LENGTH);
        XX(PACK_SKIP_ROWS);
        XX(PACK_SKIP_PIXELS);
        XX(COLOR);
        XX(DEPTH);
        XX(STENCIL);
        XX(RED);
        XX(RGB8);
        XX(RGBA8);
        XX(RGB10_A2);
        XX(TEXTURE_BINDING_3D);
        XX(UNPACK_SKIP_IMAGES);
        XX(UNPACK_IMAGE_HEIGHT);
        XX(TEXTURE_WRAP_R);
        XX(MAX_3D_TEXTURE_SIZE);
        XX(UNSIGNED_INT_2_10_10_10_REV);
        XX(MAX_ELEMENTS_VERTICES);
        XX(MAX_ELEMENTS_INDICES);
        XX(TEXTURE_MIN_LOD);
        XX(TEXTURE_MAX_LOD);
        XX(TEXTURE_BASE_LEVEL);
        XX(TEXTURE_MAX_LEVEL);
        XX(MIN);
        XX(MAX);
        XX(DEPTH_COMPONENT24);
        XX(MAX_TEXTURE_LOD_BIAS);
        XX(TEXTURE_COMPARE_MODE);
        XX(TEXTURE_COMPARE_FUNC);
        XX(CURRENT_QUERY);
        XX(QUERY_RESULT);
        XX(QUERY_RESULT_AVAILABLE);
        XX(STREAM_READ);
        XX(STREAM_COPY);
        XX(STATIC_READ);
        XX(STATIC_COPY);
        XX(DYNAMIC_READ);
        XX(DYNAMIC_COPY);
        XX(MAX_DRAW_BUFFERS);
        XX(DRAW_BUFFER0);
        XX(DRAW_BUFFER1);
        XX(DRAW_BUFFER2);
        XX(DRAW_BUFFER3);
        XX(DRAW_BUFFER4);
        XX(DRAW_BUFFER5);
        XX(DRAW_BUFFER6);
        XX(DRAW_BUFFER7);
        XX(DRAW_BUFFER8);
        XX(DRAW_BUFFER9);
        XX(DRAW_BUFFER10);
        XX(DRAW_BUFFER11);
        XX(DRAW_BUFFER12);
        XX(DRAW_BUFFER13);
        XX(DRAW_BUFFER14);
        XX(DRAW_BUFFER15);
        XX(MAX_FRAGMENT_UNIFORM_COMPONENTS);
        XX(MAX_VERTEX_UNIFORM_COMPONENTS);
        XX(SAMPLER_3D);
        XX(SAMPLER_2D_SHADOW);
        XX(FRAGMENT_SHADER_DERIVATIVE_HINT);
        XX(PIXEL_PACK_BUFFER);
        XX(PIXEL_UNPACK_BUFFER);
        XX(PIXEL_PACK_BUFFER_BINDING);
        XX(PIXEL_UNPACK_BUFFER_BINDING);
        XX(FLOAT_MAT2x3);
        XX(FLOAT_MAT2x4);
        XX(FLOAT_MAT3x2);
        XX(FLOAT_MAT3x4);
        XX(FLOAT_MAT4x2);
        XX(FLOAT_MAT4x3);
        XX(SRGB8);
        XX(SRGB8_ALPHA8);
        XX(COMPARE_REF_TO_TEXTURE);
        XX(VERTEX_ATTRIB_ARRAY_INTEGER);
        XX(MAX_ARRAY_TEXTURE_LAYERS);
        XX(MIN_PROGRAM_TEXEL_OFFSET);
        XX(MAX_PROGRAM_TEXEL_OFFSET);
        XX(MAX_VARYING_COMPONENTS);
        XX(TEXTURE_2D_ARRAY);
        XX(TEXTURE_BINDING_2D_ARRAY);
        XX(R11F_G11F_B10F);
        XX(UNSIGNED_INT_10F_11F_11F_REV);
        XX(RGB9_E5);
        XX(UNSIGNED_INT_5_9_9_9_REV);
        XX(TRANSFORM_FEEDBACK_BUFFER_MODE);
        XX(MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS);
        XX(TRANSFORM_FEEDBACK_VARYINGS);
        XX(TRANSFORM_FEEDBACK_BUFFER_START);
        XX(TRANSFORM_FEEDBACK_BUFFER_SIZE);
        XX(TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
        XX(RASTERIZER_DISCARD);
        XX(MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS);
        XX(MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS);
        XX(INTERLEAVED_ATTRIBS);
        XX(SEPARATE_ATTRIBS);
        XX(TRANSFORM_FEEDBACK_BUFFER);
        XX(TRANSFORM_FEEDBACK_BUFFER_BINDING);
        XX(RGBA32UI);
        XX(RGB32UI);
        XX(RGBA16UI);
        XX(RGB16UI);
        XX(RGBA8UI);
        XX(RGB8UI);
        XX(RGBA32I);
        XX(RGB32I);
        XX(RGBA16I);
        XX(RGB16I);
        XX(RGBA8I);
        XX(RGB8I);
        XX(RED_INTEGER);
        XX(RGB_INTEGER);
        XX(RGBA_INTEGER);
        XX(SAMPLER_2D_ARRAY);
        XX(SAMPLER_2D_ARRAY_SHADOW);
        XX(SAMPLER_CUBE_SHADOW);
        XX(UNSIGNED_INT_VEC2);
        XX(UNSIGNED_INT_VEC3);
        XX(UNSIGNED_INT_VEC4);
        XX(INT_SAMPLER_2D);
        XX(INT_SAMPLER_3D);
        XX(INT_SAMPLER_CUBE);
        XX(INT_SAMPLER_2D_ARRAY);
        XX(UNSIGNED_INT_SAMPLER_2D);
        XX(UNSIGNED_INT_SAMPLER_3D);
        XX(UNSIGNED_INT_SAMPLER_CUBE);
        XX(UNSIGNED_INT_SAMPLER_2D_ARRAY);
        XX(DEPTH_COMPONENT32F);
        XX(DEPTH32F_STENCIL8);
        XX(FLOAT_32_UNSIGNED_INT_24_8_REV);
        XX(FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING);
        XX(FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE);
        XX(FRAMEBUFFER_ATTACHMENT_RED_SIZE);
        XX(FRAMEBUFFER_ATTACHMENT_GREEN_SIZE);
        XX(FRAMEBUFFER_ATTACHMENT_BLUE_SIZE);
        XX(FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE);
        XX(FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE);
        XX(FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE);
        XX(FRAMEBUFFER_DEFAULT);
        XX(DEPTH_STENCIL_ATTACHMENT);
        XX(UNSIGNED_NORMALIZED);
        XX(DRAW_FRAMEBUFFER_BINDING);
        XX(READ_FRAMEBUFFER_BINDING);
        XX(RENDERBUFFER_SAMPLES);
        XX(FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER);
        XX(MAX_COLOR_ATTACHMENTS);
        XX(COLOR_ATTACHMENT0);
        XX(COLOR_ATTACHMENT1);
        XX(COLOR_ATTACHMENT2);
        XX(COLOR_ATTACHMENT3);
        XX(COLOR_ATTACHMENT4);
        XX(COLOR_ATTACHMENT5);
        XX(COLOR_ATTACHMENT6);
        XX(COLOR_ATTACHMENT7);
        XX(COLOR_ATTACHMENT8);
        XX(COLOR_ATTACHMENT9);
        XX(COLOR_ATTACHMENT10);
        XX(COLOR_ATTACHMENT11);
        XX(COLOR_ATTACHMENT12);
        XX(COLOR_ATTACHMENT13);
        XX(COLOR_ATTACHMENT14);
        XX(COLOR_ATTACHMENT15);
        XX(FRAMEBUFFER_INCOMPLETE_MULTISAMPLE);
        XX(MAX_SAMPLES);
        XX(RG);
        XX(RG_INTEGER);
        XX(R8);
        XX(RG8);
        XX(R16F);
        XX(R32F);
        XX(RG16F);
        XX(RG32F);
        XX(R8I);
        XX(R8UI);
        XX(R16I);
        XX(R16UI);
        XX(R32I);
        XX(R32UI);
        XX(RG8I);
        XX(RG8UI);
        XX(RG16I);
        XX(RG16UI);
        XX(RG32I);
        XX(RG32UI);
        XX(VERTEX_ARRAY_BINDING);
        XX(R8_SNORM);
        XX(RG8_SNORM);
        XX(RGB8_SNORM);
        XX(RGBA8_SNORM);
        XX(SIGNED_NORMALIZED);
        XX(PRIMITIVE_RESTART_FIXED_INDEX);
        XX(COPY_READ_BUFFER);
        XX(COPY_WRITE_BUFFER);
        XX(UNIFORM_BUFFER);
        XX(UNIFORM_BUFFER_BINDING);
        XX(UNIFORM_BUFFER_START);
        XX(UNIFORM_BUFFER_SIZE);
        XX(MAX_VERTEX_UNIFORM_BLOCKS);
        XX(MAX_FRAGMENT_UNIFORM_BLOCKS);
        XX(MAX_COMBINED_UNIFORM_BLOCKS);
        XX(MAX_UNIFORM_BUFFER_BINDINGS);
        XX(MAX_UNIFORM_BLOCK_SIZE);
        XX(MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS);
        XX(MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS);
        XX(UNIFORM_BUFFER_OFFSET_ALIGNMENT);
        XX(ACTIVE_UNIFORM_BLOCKS);
        XX(UNIFORM_TYPE);
        XX(UNIFORM_SIZE);
        XX(UNIFORM_BLOCK_INDEX);
        XX(UNIFORM_OFFSET);
        XX(UNIFORM_ARRAY_STRIDE);
        XX(UNIFORM_MATRIX_STRIDE);
        XX(UNIFORM_IS_ROW_MAJOR);
        XX(UNIFORM_BLOCK_BINDING);
        XX(UNIFORM_BLOCK_DATA_SIZE);
        XX(UNIFORM_BLOCK_ACTIVE_UNIFORMS);
        XX(UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES);
        XX(UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER);
        XX(UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER);
        XX(MAX_VERTEX_OUTPUT_COMPONENTS);
        XX(MAX_FRAGMENT_INPUT_COMPONENTS);
        XX(MAX_SERVER_WAIT_TIMEOUT);
        XX(OBJECT_TYPE);
        XX(SYNC_CONDITION);
        XX(SYNC_STATUS);
        XX(SYNC_FLAGS);
        XX(SYNC_FENCE);
        XX(SYNC_GPU_COMMANDS_COMPLETE);
        XX(UNSIGNALED);
        XX(SIGNALED);
        XX(ALREADY_SIGNALED);
        XX(TIMEOUT_EXPIRED);
        XX(CONDITION_SATISFIED);
        XX(WAIT_FAILED);
        XX(VERTEX_ATTRIB_ARRAY_DIVISOR);
        XX(ANY_SAMPLES_PASSED);
        XX(ANY_SAMPLES_PASSED_CONSERVATIVE);
        XX(SAMPLER_BINDING);
        XX(RGB10_A2UI);
        XX(TEXTURE_SWIZZLE_R);
        XX(TEXTURE_SWIZZLE_G);
        XX(TEXTURE_SWIZZLE_B);
        XX(TEXTURE_SWIZZLE_A);
        XX(GREEN);
        XX(BLUE);
        XX(INT_2_10_10_10_REV);
        XX(TRANSFORM_FEEDBACK);
        XX(TRANSFORM_FEEDBACK_PAUSED);
        XX(TRANSFORM_FEEDBACK_ACTIVE);
        XX(TRANSFORM_FEEDBACK_BINDING);
        XX(COMPRESSED_R11_EAC);
        XX(COMPRESSED_SIGNED_R11_EAC);
        XX(COMPRESSED_RG11_EAC);
        XX(COMPRESSED_SIGNED_RG11_EAC);
        XX(COMPRESSED_RGB8_ETC2);
        XX(COMPRESSED_SRGB8_ETC2);
        XX(COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2);
        XX(COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2);
        XX(COMPRESSED_RGBA8_ETC2_EAC);
        XX(COMPRESSED_SRGB8_ALPHA8_ETC2_EAC);
        XX(TEXTURE_IMMUTABLE_FORMAT);
        XX(MAX_ELEMENT_INDEX);
        XX(NUM_SAMPLE_COUNTS);
        XX(TEXTURE_IMMUTABLE_LEVELS);
#undef XX
    }

    return nullptr;
}

void
WebGLContext::EnumName(GLenum glenum, nsACString* out_name)
{
    const char* name = EnumName(glenum);
    if (name) {
        *out_name = nsDependentCString(name);
    } else {
        nsPrintfCString enumAsHex("<enum 0x%04x>", glenum);
        *out_name = enumAsHex;
    }
}

bool
WebGLContext::IsCompressedTextureFormat(GLenum format)
{
    switch (format) {
    case LOCAL_GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
    case LOCAL_GL_ATC_RGB:
    case LOCAL_GL_ATC_RGBA_EXPLICIT_ALPHA:
    case LOCAL_GL_ATC_RGBA_INTERPOLATED_ALPHA:
    case LOCAL_GL_COMPRESSED_RGB_PVRTC_4BPPV1:
    case LOCAL_GL_COMPRESSED_RGB_PVRTC_2BPPV1:
    case LOCAL_GL_COMPRESSED_RGBA_PVRTC_4BPPV1:
    case LOCAL_GL_COMPRESSED_RGBA_PVRTC_2BPPV1:
    case LOCAL_GL_ETC1_RGB8_OES:
    case LOCAL_GL_COMPRESSED_R11_EAC:
    case LOCAL_GL_COMPRESSED_SIGNED_R11_EAC:
    case LOCAL_GL_COMPRESSED_RG11_EAC:
    case LOCAL_GL_COMPRESSED_SIGNED_RG11_EAC:
    case LOCAL_GL_COMPRESSED_RGB8_ETC2:
    case LOCAL_GL_COMPRESSED_SRGB8_ETC2:
    case LOCAL_GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case LOCAL_GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case LOCAL_GL_COMPRESSED_RGBA8_ETC2_EAC:
    case LOCAL_GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
        return true;
    default:
        return false;
    }
}


bool
WebGLContext::IsTextureFormatCompressed(TexInternalFormat format)
{
    return IsCompressedTextureFormat(format.get());
}

GLenum
WebGLContext::GetAndFlushUnderlyingGLErrors()
{
    // Get and clear GL error in ALL cases.
    GLenum error = gl->fGetError();

    // Only store in mUnderlyingGLError if is hasn't already recorded an
    // error.
    if (!mUnderlyingGLError)
        mUnderlyingGLError = error;

    return error;
}

#ifdef DEBUG
// For NaNs, etc.
static bool
IsCacheCorrect(float cached, float actual)
{
    if (IsNaN(cached)) {
        // GL is allowed to do anything it wants for NaNs, so if we're shadowing
        // a NaN, then whatever `actual` is might be correct.
        return true;
    }

    return cached == actual;
}

void
AssertUintParamCorrect(gl::GLContext* gl, GLenum pname, GLuint shadow)
{
    GLuint val = 0;
    gl->GetUIntegerv(pname, &val);
    if (val != shadow) {
      printf_stderr("Failed 0x%04x shadow: Cached 0x%x/%u, should be 0x%x/%u.\n",
                    pname, shadow, shadow, val, val);
      MOZ_ASSERT(false, "Bad cached value.");
    }
}

void
AssertMaskedUintParamCorrect(gl::GLContext* gl, GLenum pname, GLuint mask,
                             GLuint shadow)
{
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
void
AssertUintParamCorrect(gl::GLContext*, GLenum, GLuint)
{
}
#endif

void
WebGLContext::AssertCachedBindings()
{
#ifdef DEBUG
    MakeContextCurrent();

    GetAndFlushUnderlyingGLErrors();

    if (IsExtensionEnabled(WebGLExtensionID::OES_vertex_array_object)) {
        GLuint bound = mBoundVertexArray ? mBoundVertexArray->GLName() : 0;
        AssertUintParamCorrect(gl, LOCAL_GL_VERTEX_ARRAY_BINDING, bound);
    }

    // Bound object state
    if (IsWebGL2()) {
        GLuint bound = mBoundDrawFramebuffer ? mBoundDrawFramebuffer->mGLName
                                             : 0;
        AssertUintParamCorrect(gl, LOCAL_GL_DRAW_FRAMEBUFFER_BINDING, bound);

        bound = mBoundReadFramebuffer ? mBoundReadFramebuffer->mGLName : 0;
        AssertUintParamCorrect(gl, LOCAL_GL_READ_FRAMEBUFFER_BINDING, bound);
    } else {
        MOZ_ASSERT(mBoundDrawFramebuffer == mBoundReadFramebuffer);
        GLuint bound = mBoundDrawFramebuffer ? mBoundDrawFramebuffer->mGLName
                                             : 0;
        AssertUintParamCorrect(gl, LOCAL_GL_FRAMEBUFFER_BINDING, bound);
    }

    GLuint bound = mCurrentProgram ? mCurrentProgram->mGLName : 0;
    AssertUintParamCorrect(gl, LOCAL_GL_CURRENT_PROGRAM, bound);

    // Textures
    GLenum activeTexture = mActiveTexture + LOCAL_GL_TEXTURE0;
    AssertUintParamCorrect(gl, LOCAL_GL_ACTIVE_TEXTURE, activeTexture);

    WebGLTexture* curTex = ActiveBoundTextureForTarget(LOCAL_GL_TEXTURE_2D);
    bound = curTex ? curTex->mGLName : 0;
    AssertUintParamCorrect(gl, LOCAL_GL_TEXTURE_BINDING_2D, bound);

    curTex = ActiveBoundTextureForTarget(LOCAL_GL_TEXTURE_CUBE_MAP);
    bound = curTex ? curTex->mGLName : 0;
    AssertUintParamCorrect(gl, LOCAL_GL_TEXTURE_BINDING_CUBE_MAP, bound);

    // Buffers
    bound = mBoundArrayBuffer ? mBoundArrayBuffer->mGLName : 0;
    AssertUintParamCorrect(gl, LOCAL_GL_ARRAY_BUFFER_BINDING, bound);

    MOZ_ASSERT(mBoundVertexArray);
    WebGLBuffer* curBuff = mBoundVertexArray->mElementArrayBuffer;
    bound = curBuff ? curBuff->mGLName : 0;
    AssertUintParamCorrect(gl, LOCAL_GL_ELEMENT_ARRAY_BUFFER_BINDING, bound);

    MOZ_ASSERT(!GetAndFlushUnderlyingGLErrors());
#endif
}

void
WebGLContext::AssertCachedState()
{
#ifdef DEBUG
    MakeContextCurrent();

    GetAndFlushUnderlyingGLErrors();

    // extensions
    if (IsExtensionEnabled(WebGLExtensionID::WEBGL_draw_buffers)) {
        AssertUintParamCorrect(gl, LOCAL_GL_MAX_COLOR_ATTACHMENTS, mGLMaxColorAttachments);
        AssertUintParamCorrect(gl, LOCAL_GL_MAX_DRAW_BUFFERS, mGLMaxDrawBuffers);
    }

    // Draw state
    MOZ_ASSERT(gl->fIsEnabled(LOCAL_GL_DITHER) == mDitherEnabled);
    MOZ_ASSERT_IF(IsWebGL2(),
                  gl->fIsEnabled(LOCAL_GL_RASTERIZER_DISCARD) == mRasterizerDiscardEnabled);
    MOZ_ASSERT(gl->fIsEnabled(LOCAL_GL_SCISSOR_TEST) == mScissorTestEnabled);
    MOZ_ASSERT(gl->fIsEnabled(LOCAL_GL_STENCIL_TEST) == mStencilTestEnabled);

    realGLboolean colorWriteMask[4] = {0, 0, 0, 0};
    gl->fGetBooleanv(LOCAL_GL_COLOR_WRITEMASK, colorWriteMask);
    MOZ_ASSERT(colorWriteMask[0] == mColorWriteMask[0] &&
               colorWriteMask[1] == mColorWriteMask[1] &&
               colorWriteMask[2] == mColorWriteMask[2] &&
               colorWriteMask[3] == mColorWriteMask[3]);

    GLfloat colorClearValue[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    gl->fGetFloatv(LOCAL_GL_COLOR_CLEAR_VALUE, colorClearValue);
    MOZ_ASSERT(IsCacheCorrect(mColorClearValue[0], colorClearValue[0]) &&
               IsCacheCorrect(mColorClearValue[1], colorClearValue[1]) &&
               IsCacheCorrect(mColorClearValue[2], colorClearValue[2]) &&
               IsCacheCorrect(mColorClearValue[3], colorClearValue[3]));

    realGLboolean depthWriteMask = 0;
    gl->fGetBooleanv(LOCAL_GL_DEPTH_WRITEMASK, &depthWriteMask);
    MOZ_ASSERT(depthWriteMask == mDepthWriteMask);

    GLfloat depthClearValue = 0.0f;
    gl->fGetFloatv(LOCAL_GL_DEPTH_CLEAR_VALUE, &depthClearValue);
    MOZ_ASSERT(IsCacheCorrect(mDepthClearValue, depthClearValue));

    AssertUintParamCorrect(gl, LOCAL_GL_STENCIL_CLEAR_VALUE, mStencilClearValue);

    GLint stencilBits = 0;
    if (GetStencilBits(&stencilBits)) {
        const GLuint stencilRefMask = (1 << stencilBits) - 1;

        AssertMaskedUintParamCorrect(gl, LOCAL_GL_STENCIL_REF,      stencilRefMask, mStencilRefFront);
        AssertMaskedUintParamCorrect(gl, LOCAL_GL_STENCIL_BACK_REF, stencilRefMask, mStencilRefBack);
    }

    // GLES 3.0.4, $4.1.4, p177:
    //   [...] the front and back stencil mask are both set to the value `2^s - 1`, where
    //   `s` is greater than or equal to the number of bits in the deepest stencil buffer
    //   supported by the GL implementation.
    const int maxStencilBits = 8;
    const GLuint maxStencilBitsMask = (1 << maxStencilBits) - 1;
    AssertMaskedUintParamCorrect(gl, LOCAL_GL_STENCIL_VALUE_MASK,      maxStencilBitsMask, mStencilValueMaskFront);
    AssertMaskedUintParamCorrect(gl, LOCAL_GL_STENCIL_BACK_VALUE_MASK, maxStencilBitsMask, mStencilValueMaskBack);

    AssertMaskedUintParamCorrect(gl, LOCAL_GL_STENCIL_WRITEMASK,       maxStencilBitsMask, mStencilWriteMaskFront);
    AssertMaskedUintParamCorrect(gl, LOCAL_GL_STENCIL_BACK_WRITEMASK,  maxStencilBitsMask, mStencilWriteMaskBack);

    // Viewport
    GLint int4[4] = {0, 0, 0, 0};
    gl->fGetIntegerv(LOCAL_GL_VIEWPORT, int4);
    MOZ_ASSERT(int4[0] == mViewportX &&
               int4[1] == mViewportY &&
               int4[2] == mViewportWidth &&
               int4[3] == mViewportHeight);

    AssertUintParamCorrect(gl, LOCAL_GL_PACK_ALIGNMENT, mPixelStorePackAlignment);
    AssertUintParamCorrect(gl, LOCAL_GL_UNPACK_ALIGNMENT, mPixelStoreUnpackAlignment);

    MOZ_ASSERT(!GetAndFlushUnderlyingGLErrors());
#endif
}

const char*
InfoFrom(WebGLTexImageFunc func, WebGLTexDimensions dims)
{
    switch (dims) {
    case WebGLTexDimensions::Tex2D:
        switch (func) {
        case WebGLTexImageFunc::TexImage:        return "texImage2D";
        case WebGLTexImageFunc::TexSubImage:     return "texSubImage2D";
        case WebGLTexImageFunc::CopyTexImage:    return "copyTexImage2D";
        case WebGLTexImageFunc::CopyTexSubImage: return "copyTexSubImage2D";
        case WebGLTexImageFunc::CompTexImage:    return "compressedTexImage2D";
        case WebGLTexImageFunc::CompTexSubImage: return "compressedTexSubImage2D";
        default:
            MOZ_CRASH();
        }
    case WebGLTexDimensions::Tex3D:
        switch (func) {
        case WebGLTexImageFunc::TexImage:        return "texImage3D";
        case WebGLTexImageFunc::TexSubImage:     return "texSubImage3D";
        case WebGLTexImageFunc::CopyTexSubImage: return "copyTexSubImage3D";
        case WebGLTexImageFunc::CompTexSubImage: return "compressedTexSubImage3D";
        default:
            MOZ_CRASH();
        }
    default:
        MOZ_CRASH();
    }
}

} // namespace mozilla
