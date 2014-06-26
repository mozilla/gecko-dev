/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLEXTENSIONS_H_
#define WEBGLEXTENSIONS_H_

#include "jsapi.h"
#include "mozilla/Attributes.h"
#include "nsWrapperCache.h"
#include "WebGLObjectModel.h"
#include "WebGLTypes.h"

namespace mozilla {

class WebGLContext;
class WebGLShader;
class WebGLVertexArray;

class WebGLExtensionBase
    : public nsWrapperCache
    , public WebGLContextBoundObject
{
public:
    WebGLExtensionBase(WebGLContext*);

    WebGLContext *GetParentObject() const {
        return Context();
    }

    void MarkLost();

    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLExtensionBase)
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLExtensionBase)

protected:
    virtual ~WebGLExtensionBase();

    bool mIsLost;
};

#define DECL_WEBGL_EXTENSION_GOOP                                           \
    virtual JSObject* WrapObject(JSContext *cx) MOZ_OVERRIDE;

#define IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionType) \
    JSObject* \
    WebGLExtensionType::WrapObject(JSContext *cx) { \
        return dom::WebGLExtensionType##Binding::Wrap(cx, this); \
    }

class WebGLExtensionCompressedTextureATC
    : public WebGLExtensionBase
{
public:
    WebGLExtensionCompressedTextureATC(WebGLContext*);
    virtual ~WebGLExtensionCompressedTextureATC();

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionCompressedTextureETC1
    : public WebGLExtensionBase
{
public:
    WebGLExtensionCompressedTextureETC1(WebGLContext*);
    virtual ~WebGLExtensionCompressedTextureETC1();

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionCompressedTexturePVRTC
    : public WebGLExtensionBase
{
public:
    WebGLExtensionCompressedTexturePVRTC(WebGLContext*);
    virtual ~WebGLExtensionCompressedTexturePVRTC();

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionCompressedTextureS3TC
    : public WebGLExtensionBase
{
public:
    WebGLExtensionCompressedTextureS3TC(WebGLContext*);
    virtual ~WebGLExtensionCompressedTextureS3TC();

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionDebugRendererInfo
    : public WebGLExtensionBase
{
public:
    WebGLExtensionDebugRendererInfo(WebGLContext*);
    virtual ~WebGLExtensionDebugRendererInfo();

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionDebugShaders
    : public WebGLExtensionBase
{
public:
    WebGLExtensionDebugShaders(WebGLContext*);
    virtual ~WebGLExtensionDebugShaders();

    void GetTranslatedShaderSource(WebGLShader* shader, nsAString& retval);

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionDepthTexture
    : public WebGLExtensionBase
{
public:
    WebGLExtensionDepthTexture(WebGLContext*);
    virtual ~WebGLExtensionDepthTexture();

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionElementIndexUint
    : public WebGLExtensionBase
{
public:
    WebGLExtensionElementIndexUint(WebGLContext*);
    virtual ~WebGLExtensionElementIndexUint();

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionFragDepth
    : public WebGLExtensionBase
{
public:
    WebGLExtensionFragDepth(WebGLContext*);
    virtual ~WebGLExtensionFragDepth();

    static bool IsSupported(const WebGLContext* context);

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionLoseContext
    : public WebGLExtensionBase
{
public:
    WebGLExtensionLoseContext(WebGLContext*);
    virtual ~WebGLExtensionLoseContext();

    void LoseContext();
    void RestoreContext();

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionSRGB
    : public WebGLExtensionBase
{
public:
    WebGLExtensionSRGB(WebGLContext*);
    virtual ~WebGLExtensionSRGB();

    static bool IsSupported(const WebGLContext* context);

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionStandardDerivatives
    : public WebGLExtensionBase
{
public:
    WebGLExtensionStandardDerivatives(WebGLContext*);
    virtual ~WebGLExtensionStandardDerivatives();

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionTextureFilterAnisotropic
    : public WebGLExtensionBase
{
public:
    WebGLExtensionTextureFilterAnisotropic(WebGLContext*);
    virtual ~WebGLExtensionTextureFilterAnisotropic();

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionTextureFloat
    : public WebGLExtensionBase
{
public:
    WebGLExtensionTextureFloat(WebGLContext*);
    virtual ~WebGLExtensionTextureFloat();

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionTextureFloatLinear
    : public WebGLExtensionBase
{
public:
    WebGLExtensionTextureFloatLinear(WebGLContext*);
    virtual ~WebGLExtensionTextureFloatLinear();

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionTextureHalfFloat
    : public WebGLExtensionBase
{
public:
    WebGLExtensionTextureHalfFloat(WebGLContext*);
    virtual ~WebGLExtensionTextureHalfFloat();

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionTextureHalfFloatLinear
    : public WebGLExtensionBase
{
public:
    WebGLExtensionTextureHalfFloatLinear(WebGLContext*);
    virtual ~WebGLExtensionTextureHalfFloatLinear();

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionColorBufferFloat
    : public WebGLExtensionBase
{
public:
    WebGLExtensionColorBufferFloat(WebGLContext*);
    virtual ~WebGLExtensionColorBufferFloat();

    static bool IsSupported(const WebGLContext*);

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionColorBufferHalfFloat
    : public WebGLExtensionBase
{
public:
    WebGLExtensionColorBufferHalfFloat(WebGLContext*);
    virtual ~WebGLExtensionColorBufferHalfFloat();

    static bool IsSupported(const WebGLContext*);

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionDrawBuffers
    : public WebGLExtensionBase
{
public:
    WebGLExtensionDrawBuffers(WebGLContext*);
    virtual ~WebGLExtensionDrawBuffers();

    void DrawBuffersWEBGL(const dom::Sequence<GLenum>& buffers);

    static bool IsSupported(const WebGLContext*);

    static const size_t sMinColorAttachments = 4;
    static const size_t sMinDrawBuffers = 4;
    /*
     WEBGL_draw_buffers does not give a minal value for GL_MAX_DRAW_BUFFERS. But, we request
     for GL_MAX_DRAW_BUFFERS = 4 at least to be able to use all requested color attachments.
     See DrawBuffersWEBGL in WebGLExtensionDrawBuffers.cpp inner comments for more informations.
     */

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionVertexArray
    : public WebGLExtensionBase
{
public:
    WebGLExtensionVertexArray(WebGLContext*);
    virtual ~WebGLExtensionVertexArray();

    already_AddRefed<WebGLVertexArray> CreateVertexArrayOES();
    void DeleteVertexArrayOES(WebGLVertexArray* array);
    bool IsVertexArrayOES(WebGLVertexArray* array);
    void BindVertexArrayOES(WebGLVertexArray* array);

    static bool IsSupported(const WebGLContext* context);

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionInstancedArrays
    : public WebGLExtensionBase
{
public:
    WebGLExtensionInstancedArrays(WebGLContext* context);
    virtual ~WebGLExtensionInstancedArrays();

    void DrawArraysInstancedANGLE(GLenum mode, GLint first,
                                  GLsizei count, GLsizei primcount);
    void DrawElementsInstancedANGLE(GLenum mode, GLsizei count,
                                    GLenum type, WebGLintptr offset,
                                    GLsizei primcount);
    void VertexAttribDivisorANGLE(GLuint index, GLuint divisor);

    static bool IsSupported(const WebGLContext* context);

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionBlendMinMax
    : public WebGLExtensionBase
{
public:
    WebGLExtensionBlendMinMax(WebGLContext*);
    virtual ~WebGLExtensionBlendMinMax();

    static bool IsSupported(const WebGLContext*);

    DECL_WEBGL_EXTENSION_GOOP
};

} // namespace mozilla

#endif // WEBGLEXTENSIONS_H_
