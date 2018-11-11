//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Context.h: Defines the gl::Context class, managing all GL state and performing
// rendering operations. It is the GLES2 specific implementation of EGLContext.

#ifndef LIBANGLE_CONTEXT_H_
#define LIBANGLE_CONTEXT_H_

#include <set>
#include <string>

#include "angle_gl.h"
#include "common/MemoryBuffer.h"
#include "common/PackedEnums.h"
#include "common/angleutils.h"
#include "libANGLE/Caps.h"
#include "libANGLE/Constants.h"
#include "libANGLE/ContextState.h"
#include "libANGLE/Context_gles_1_0_autogen.h"
#include "libANGLE/Error.h"
#include "libANGLE/HandleAllocator.h"
#include "libANGLE/RefCountObject.h"
#include "libANGLE/ResourceMap.h"
#include "libANGLE/ResourceManager.h"
#include "libANGLE/VertexAttribute.h"
#include "libANGLE/Workarounds.h"
#include "libANGLE/angletypes.h"

namespace rx
{
class ContextImpl;
class EGLImplFactory;
}

namespace egl
{
class AttributeMap;
class Surface;
struct Config;
class Thread;
}

namespace gl
{
class Buffer;
class Compiler;
class FenceNV;
class Framebuffer;
class GLES1Renderer;
class MemoryProgramCache;
class Program;
class ProgramPipeline;
class Query;
class Renderbuffer;
class Sampler;
class Shader;
class Sync;
class Texture;
class TransformFeedback;
class VertexArray;
struct VertexAttribute;

class ErrorSet : angle::NonCopyable
{
  public:
    explicit ErrorSet(Context *context);
    ~ErrorSet();

    // TODO(jmadill): Remove const. http://anglebug.com/2378
    void handleError(const Error &error) const;
    bool empty() const;
    GLenum popError();

  private:
    Context *mContext;

    // TODO(jmadill): Remove mutable. http://anglebug.com/2378
    mutable std::set<GLenum> mErrors;
};

// Helper class for managing cache variables and state changes.
class StateCache final : angle::NonCopyable
{
  public:
    StateCache();
    ~StateCache();

    // Places that can trigger updateActiveAttribsMask:
    // 1. onVertexArrayBindingChange.
    // 2. onProgramExecutableChange.
    // 3. onVertexArrayStateChange.
    // 4. onGLES1ClientStateChange.
    AttributesMask getActiveBufferedAttribsMask() const { return mCachedActiveBufferedAttribsMask; }
    AttributesMask getActiveClientAttribsMask() const { return mCachedActiveClientAttribsMask; }
    bool hasAnyEnabledClientAttrib() const { return mCachedHasAnyEnabledClientAttrib; }

    // Places that can trigger updateVertexElementLimits:
    // 1. onVertexArrayBindingChange.
    // 2. onProgramExecutableChange.
    // 3. onVertexArraySizeChange.
    // 4. onVertexArrayStateChange.
    GLint64 getNonInstancedVertexElementLimit() const
    {
        return mCachedNonInstancedVertexElementLimit;
    }
    GLint64 getInstancedVertexElementLimit() const { return mCachedInstancedVertexElementLimit; }

    // State change notifications.
    void onVertexArrayBindingChange(Context *context);
    void onProgramExecutableChange(Context *context);
    void onVertexArraySizeChange(Context *context);
    void onVertexArrayStateChange(Context *context);
    void onGLES1ClientStateChange(Context *context);

  private:
    // Cache update functions.
    void updateActiveAttribsMask(Context *context);
    void updateVertexElementLimits(Context *context);

    AttributesMask mCachedActiveBufferedAttribsMask;
    AttributesMask mCachedActiveClientAttribsMask;
    bool mCachedHasAnyEnabledClientAttrib;
    GLint64 mCachedNonInstancedVertexElementLimit;
    GLint64 mCachedInstancedVertexElementLimit;
};

class Context final : public egl::LabeledObject, angle::NonCopyable, public angle::ObserverInterface
{
  public:
    Context(rx::EGLImplFactory *implFactory,
            const egl::Config *config,
            const Context *shareContext,
            TextureManager *shareTextures,
            MemoryProgramCache *memoryProgramCache,
            const egl::AttributeMap &attribs,
            const egl::DisplayExtensions &displayExtensions,
            const egl::ClientExtensions &clientExtensions);

    egl::Error onDestroy(const egl::Display *display);
    ~Context();

    void setLabel(EGLLabelKHR label) override;
    EGLLabelKHR getLabel() const override;

    egl::Error makeCurrent(egl::Display *display, egl::Surface *surface);
    egl::Error releaseSurface(const egl::Display *display);

    // These create  and destroy methods are merely pass-throughs to
    // ResourceManager, which owns these object types
    GLuint createBuffer();
    GLuint createShader(ShaderType type);
    GLuint createProgram();
    GLuint createTexture();
    GLuint createRenderbuffer();
    GLuint genPaths(GLsizei range);
    GLuint createProgramPipeline();
    GLuint createShaderProgramv(ShaderType type, GLsizei count, const GLchar *const *strings);

    void deleteBuffer(GLuint buffer);
    void deleteShader(GLuint shader);
    void deleteProgram(GLuint program);
    void deleteTexture(GLuint texture);
    void deleteRenderbuffer(GLuint renderbuffer);
    void deletePaths(GLuint first, GLsizei range);
    void deleteProgramPipeline(GLuint pipeline);

    // CHROMIUM_path_rendering
    bool isPath(GLuint path) const;
    bool isPathGenerated(GLuint path) const;
    void pathCommands(GLuint path,
                      GLsizei numCommands,
                      const GLubyte *commands,
                      GLsizei numCoords,
                      GLenum coordType,
                      const void *coords);
    void pathParameterf(GLuint path, GLenum pname, GLfloat value);
    void pathParameteri(GLuint path, GLenum pname, GLint value);
    void getPathParameterfv(GLuint path, GLenum pname, GLfloat *value);
    void getPathParameteriv(GLuint path, GLenum pname, GLint *value);
    void pathStencilFunc(GLenum func, GLint ref, GLuint mask);

    // Framebuffers are owned by the Context, so these methods do not pass through
    GLuint createFramebuffer();
    void deleteFramebuffer(GLuint framebuffer);

    // NV Fences are owned by the Context.
    void genFencesNV(GLsizei n, GLuint *fences);
    void deleteFencesNV(GLsizei n, const GLuint *fences);
    void finishFenceNV(GLuint fence);
    void getFenceivNV(GLuint fence, GLenum pname, GLint *params);
    GLboolean isFenceNV(GLuint fence);
    void setFenceNV(GLuint fence, GLenum condition);
    GLboolean testFenceNV(GLuint fence);

    // GLES1 emulation: Interface to entry points
    ANGLE_GLES1_CONTEXT_API

    // OpenGL ES 2+
    void bindTexture(TextureType target, GLuint handle);
    void bindReadFramebuffer(GLuint framebufferHandle);
    void bindDrawFramebuffer(GLuint framebufferHandle);
    void bindVertexArray(GLuint vertexArrayHandle);
    void bindVertexBuffer(GLuint bindingIndex,
                          GLuint bufferHandle,
                          GLintptr offset,
                          GLsizei stride);
    void bindSampler(GLuint textureUnit, GLuint samplerHandle);
    void bindImageTexture(GLuint unit,
                          GLuint texture,
                          GLint level,
                          GLboolean layered,
                          GLint layer,
                          GLenum access,
                          GLenum format);
    void useProgram(GLuint program);
    void useProgramStages(GLuint pipeline, GLbitfield stages, GLuint program);
    void bindTransformFeedback(GLenum target, GLuint transformFeedbackHandle);
    void bindProgramPipeline(GLuint pipelineHandle);

    void beginQuery(QueryType target, GLuint query);
    void endQuery(QueryType target);
    void queryCounter(GLuint id, QueryType target);
    void getQueryiv(QueryType target, GLenum pname, GLint *params);
    void getQueryivRobust(QueryType target,
                          GLenum pname,
                          GLsizei bufSize,
                          GLsizei *length,
                          GLint *params);
    void getQueryObjectiv(GLuint id, GLenum pname, GLint *params);
    void getQueryObjectivRobust(GLuint id,
                                GLenum pname,
                                GLsizei bufSize,
                                GLsizei *length,
                                GLint *params);
    void getQueryObjectuiv(GLuint id, GLenum pname, GLuint *params);
    void getQueryObjectuivRobust(GLuint id,
                                 GLenum pname,
                                 GLsizei bufSize,
                                 GLsizei *length,
                                 GLuint *params);
    void getQueryObjecti64v(GLuint id, GLenum pname, GLint64 *params);
    void getQueryObjecti64vRobust(GLuint id,
                                  GLenum pname,
                                  GLsizei bufSize,
                                  GLsizei *length,
                                  GLint64 *params);
    void getQueryObjectui64v(GLuint id, GLenum pname, GLuint64 *params);
    void getQueryObjectui64vRobust(GLuint id,
                                   GLenum pname,
                                   GLsizei bufSize,
                                   GLsizei *length,
                                   GLuint64 *params);

    void vertexAttribDivisor(GLuint index, GLuint divisor);
    void vertexBindingDivisor(GLuint bindingIndex, GLuint divisor);

    void getBufferParameteriv(BufferBinding target, GLenum pname, GLint *params);
    void getBufferParameterivRobust(BufferBinding target,
                                    GLenum pname,
                                    GLsizei bufSize,
                                    GLsizei *length,
                                    GLint *params);

    void getFramebufferAttachmentParameteriv(GLenum target,
                                             GLenum attachment,
                                             GLenum pname,
                                             GLint *params);
    void getFramebufferAttachmentParameterivRobust(GLenum target,
                                                   GLenum attachment,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei *length,
                                                   GLint *params);
    void getRenderbufferParameteriv(GLenum target, GLenum pname, GLint *params);
    void getRenderbufferParameterivRobust(GLenum target,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLint *params);

    void getTexParameterfv(TextureType target, GLenum pname, GLfloat *params);
    void getTexParameterfvRobust(TextureType target,
                                 GLenum pname,
                                 GLsizei bufSize,
                                 GLsizei *length,
                                 GLfloat *params);
    void getTexParameteriv(TextureType target, GLenum pname, GLint *params);
    void getTexParameterivRobust(TextureType target,
                                 GLenum pname,
                                 GLsizei bufSize,
                                 GLsizei *length,
                                 GLint *params);
    void getTexParameterIivRobust(TextureType target,
                                  GLenum pname,
                                  GLsizei bufSize,
                                  GLsizei *length,
                                  GLint *params);
    void getTexParameterIuivRobust(TextureType target,
                                   GLenum pname,
                                   GLsizei bufSize,
                                   GLsizei *length,
                                   GLuint *params);

    void getTexLevelParameteriv(TextureTarget target, GLint level, GLenum pname, GLint *params);
    void getTexLevelParameterivRobust(TextureTarget target,
                                      GLint level,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei *length,
                                      GLint *params);
    void getTexLevelParameterfv(TextureTarget target, GLint level, GLenum pname, GLfloat *params);
    void getTexLevelParameterfvRobust(TextureTarget target,
                                      GLint level,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei *length,
                                      GLfloat *params);
    void texParameterf(TextureType target, GLenum pname, GLfloat param);
    void texParameterfv(TextureType target, GLenum pname, const GLfloat *params);
    void texParameterfvRobust(TextureType target,
                              GLenum pname,
                              GLsizei bufSize,
                              const GLfloat *params);
    void texParameteri(TextureType target, GLenum pname, GLint param);
    void texParameteriv(TextureType target, GLenum pname, const GLint *params);
    void texParameterivRobust(TextureType target,
                              GLenum pname,
                              GLsizei bufSize,
                              const GLint *params);
    void texParameterIivRobust(TextureType target,
                               GLenum pname,
                               GLsizei bufSize,
                               const GLint *params);
    void texParameterIuivRobust(TextureType target,
                                GLenum pname,
                                GLsizei bufSize,
                                const GLuint *params);
    void samplerParameteri(GLuint sampler, GLenum pname, GLint param);
    void samplerParameteriv(GLuint sampler, GLenum pname, const GLint *param);
    void samplerParameterivRobust(GLuint sampler,
                                  GLenum pname,
                                  GLsizei bufSize,
                                  const GLint *param);
    void samplerParameterIivRobust(GLuint sampler,
                                   GLenum pname,
                                   GLsizei bufSize,
                                   const GLint *param);
    void samplerParameterIuivRobust(GLuint sampler,
                                    GLenum pname,
                                    GLsizei bufSize,
                                    const GLuint *param);
    void samplerParameterf(GLuint sampler, GLenum pname, GLfloat param);
    void samplerParameterfv(GLuint sampler, GLenum pname, const GLfloat *param);
    void samplerParameterfvRobust(GLuint sampler,
                                  GLenum pname,
                                  GLsizei bufSize,
                                  const GLfloat *param);

    void getSamplerParameteriv(GLuint sampler, GLenum pname, GLint *params);
    void getSamplerParameterivRobust(GLuint sampler,
                                     GLenum pname,
                                     GLsizei bufSize,
                                     GLsizei *length,
                                     GLint *params);
    void getSamplerParameterIivRobust(GLuint sampler,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei *length,
                                      GLint *params);
    void getSamplerParameterIuivRobust(GLuint sampler,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       GLsizei *length,
                                       GLuint *params);
    void getSamplerParameterfv(GLuint sampler, GLenum pname, GLfloat *params);
    void getSamplerParameterfvRobust(GLuint sampler,
                                     GLenum pname,
                                     GLsizei bufSize,
                                     GLsizei *length,
                                     GLfloat *params);

    void programParameteri(GLuint program, GLenum pname, GLint value);

    GLuint getProgramResourceIndex(GLuint program, GLenum programInterface, const GLchar *name);
    void getProgramResourceName(GLuint program,
                                GLenum programInterface,
                                GLuint index,
                                GLsizei bufSize,
                                GLsizei *length,
                                GLchar *name);
    GLint getProgramResourceLocation(GLuint program, GLenum programInterface, const GLchar *name);
    void getProgramResourceiv(GLuint program,
                              GLenum programInterface,
                              GLuint index,
                              GLsizei propCount,
                              const GLenum *props,
                              GLsizei bufSize,
                              GLsizei *length,
                              GLint *params);

    void getProgramInterfaceiv(GLuint program,
                               GLenum programInterface,
                               GLenum pname,
                               GLint *params);
    void getProgramInterfaceivRobust(GLuint program,
                                     GLenum programInterface,
                                     GLenum pname,
                                     GLsizei bufSize,
                                     GLsizei *length,
                                     GLint *params);

    Buffer *getBuffer(GLuint handle) const;
    FenceNV *getFenceNV(GLuint handle);
    Sync *getSync(GLsync handle) const;
    Texture *getTexture(GLuint handle) const;
    Framebuffer *getFramebuffer(GLuint handle) const;
    Renderbuffer *getRenderbuffer(GLuint handle) const;
    VertexArray *getVertexArray(GLuint handle) const;
    Sampler *getSampler(GLuint handle) const;
    Query *getQuery(GLuint handle, bool create, QueryType type);
    Query *getQuery(GLuint handle) const;
    TransformFeedback *getTransformFeedback(GLuint handle) const;
    ProgramPipeline *getProgramPipeline(GLuint handle) const;

    void objectLabel(GLenum identifier, GLuint name, GLsizei length, const GLchar *label);
    void objectPtrLabel(const void *ptr, GLsizei length, const GLchar *label);
    void getObjectLabel(GLenum identifier,
                        GLuint name,
                        GLsizei bufSize,
                        GLsizei *length,
                        GLchar *label) const;
    void getObjectPtrLabel(const void *ptr, GLsizei bufSize, GLsizei *length, GLchar *label) const;

    Texture *getTargetTexture(TextureType type) const;
    Texture *getSamplerTexture(unsigned int sampler, TextureType type) const;

    Compiler *getCompiler() const;

    bool isSampler(GLuint samplerName) const;

    bool isVertexArrayGenerated(GLuint vertexArray);
    bool isTransformFeedbackGenerated(GLuint vertexArray);

    void getBooleanv(GLenum pname, GLboolean *params);
    void getBooleanvRobust(GLenum pname, GLsizei bufSize, GLsizei *length, GLboolean *params);
    void getBooleanvImpl(GLenum pname, GLboolean *params);
    void getFloatv(GLenum pname, GLfloat *params);
    void getFloatvRobust(GLenum pname, GLsizei bufSize, GLsizei *length, GLfloat *params);
    void getFloatvImpl(GLenum pname, GLfloat *params);
    void getIntegerv(GLenum pname, GLint *params);
    void getIntegervRobust(GLenum pname, GLsizei bufSize, GLsizei *length, GLint *data);
    void getIntegervImpl(GLenum pname, GLint *params);
    void getInteger64vImpl(GLenum pname, GLint64 *params);
    void getPointerv(GLenum pname, void **params) const;
    void getPointervRobustANGLERobust(GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei *length,
                                      void **params);
    void getBooleani_v(GLenum target, GLuint index, GLboolean *data);
    void getBooleani_vRobust(GLenum target,
                             GLuint index,
                             GLsizei bufSize,
                             GLsizei *length,
                             GLboolean *data);
    void getIntegeri_v(GLenum target, GLuint index, GLint *data);
    void getIntegeri_vRobust(GLenum target,
                             GLuint index,
                             GLsizei bufSize,
                             GLsizei *length,
                             GLint *data);
    void getInteger64i_v(GLenum target, GLuint index, GLint64 *data);
    void getInteger64i_vRobust(GLenum target,
                               GLuint index,
                               GLsizei bufSize,
                               GLsizei *length,
                               GLint64 *data);

    void activeShaderProgram(GLuint pipeline, GLuint program);
    void activeTexture(GLenum texture);
    void blendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
    void blendEquation(GLenum mode);
    void blendEquationSeparate(GLenum modeRGB, GLenum modeAlpha);
    void blendFunc(GLenum sfactor, GLenum dfactor);
    void blendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
    void clearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
    void clearDepthf(GLfloat depth);
    void clearStencil(GLint s);
    void colorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
    void cullFace(CullFaceMode mode);
    void depthFunc(GLenum func);
    void depthMask(GLboolean flag);
    void depthRangef(GLfloat zNear, GLfloat zFar);
    void disable(GLenum cap);
    void disableVertexAttribArray(GLuint index);
    void enable(GLenum cap);
    void enableVertexAttribArray(GLuint index);
    void frontFace(GLenum mode);
    void hint(GLenum target, GLenum mode);
    void lineWidth(GLfloat width);
    void pixelStorei(GLenum pname, GLint param);
    void polygonOffset(GLfloat factor, GLfloat units);
    void sampleCoverage(GLfloat value, GLboolean invert);
    void sampleMaski(GLuint maskNumber, GLbitfield mask);
    void scissor(GLint x, GLint y, GLsizei width, GLsizei height);
    void stencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask);
    void stencilMaskSeparate(GLenum face, GLuint mask);
    void stencilOpSeparate(GLenum face, GLenum fail, GLenum zfail, GLenum zpass);
    void vertexAttrib1f(GLuint index, GLfloat x);
    void vertexAttrib1fv(GLuint index, const GLfloat *values);
    void vertexAttrib2f(GLuint index, GLfloat x, GLfloat y);
    void vertexAttrib2fv(GLuint index, const GLfloat *values);
    void vertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z);
    void vertexAttrib3fv(GLuint index, const GLfloat *values);
    void vertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
    void vertexAttrib4fv(GLuint index, const GLfloat *values);
    void vertexAttribFormat(GLuint attribIndex,
                            GLint size,
                            GLenum type,
                            GLboolean normalized,
                            GLuint relativeOffset);
    void vertexAttribIFormat(GLuint attribIndex, GLint size, GLenum type, GLuint relativeOffset);
    void vertexAttribBinding(GLuint attribIndex, GLuint bindingIndex);
    void vertexAttribPointer(GLuint index,
                             GLint size,
                             GLenum type,
                             GLboolean normalized,
                             GLsizei stride,
                             const void *ptr);
    void vertexAttribIPointer(GLuint index,
                              GLint size,
                              GLenum type,
                              GLsizei stride,
                              const void *pointer);
    void viewport(GLint x, GLint y, GLsizei width, GLsizei height);

    void vertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w);
    void vertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w);
    void vertexAttribI4iv(GLuint index, const GLint *v);
    void vertexAttribI4uiv(GLuint index, const GLuint *v);
    void getVertexAttribiv(GLuint index, GLenum pname, GLint *params);
    void getVertexAttribivRobust(GLuint index,
                                 GLenum pname,
                                 GLsizei bufSize,
                                 GLsizei *length,
                                 GLint *params);
    void getVertexAttribfv(GLuint index, GLenum pname, GLfloat *params);
    void getVertexAttribfvRobust(GLuint index,
                                 GLenum pname,
                                 GLsizei bufSize,
                                 GLsizei *length,
                                 GLfloat *params);
    void getVertexAttribIiv(GLuint index, GLenum pname, GLint *params);
    void getVertexAttribIivRobust(GLuint index,
                                  GLenum pname,
                                  GLsizei bufSize,
                                  GLsizei *length,
                                  GLint *params);
    void getVertexAttribIuiv(GLuint index, GLenum pname, GLuint *params);
    void getVertexAttribIuivRobust(GLuint index,
                                   GLenum pname,
                                   GLsizei bufSize,
                                   GLsizei *length,
                                   GLuint *params);
    void getVertexAttribPointerv(GLuint index, GLenum pname, void **pointer);
    void getVertexAttribPointervRobust(GLuint index,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       GLsizei *length,
                                       void **pointer);

    void debugMessageControl(GLenum source,
                             GLenum type,
                             GLenum severity,
                             GLsizei count,
                             const GLuint *ids,
                             GLboolean enabled);
    void debugMessageInsert(GLenum source,
                            GLenum type,
                            GLuint id,
                            GLenum severity,
                            GLsizei length,
                            const GLchar *buf);
    void debugMessageCallback(GLDEBUGPROCKHR callback, const void *userParam);
    GLuint getDebugMessageLog(GLuint count,
                              GLsizei bufSize,
                              GLenum *sources,
                              GLenum *types,
                              GLuint *ids,
                              GLenum *severities,
                              GLsizei *lengths,
                              GLchar *messageLog);
    void pushDebugGroup(GLenum source, GLuint id, GLsizei length, const GLchar *message);
    void popDebugGroup();

    void clear(GLbitfield mask);
    void clearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *values);
    void clearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint *values);
    void clearBufferiv(GLenum buffer, GLint drawbuffer, const GLint *values);
    void clearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil);

    void drawArrays(PrimitiveMode mode, GLint first, GLsizei count);
    void drawArraysInstanced(PrimitiveMode mode, GLint first, GLsizei count, GLsizei instanceCount);

    void drawElements(PrimitiveMode mode, GLsizei count, GLenum type, const void *indices);
    void drawElementsInstanced(PrimitiveMode mode,
                               GLsizei count,
                               GLenum type,
                               const void *indices,
                               GLsizei instances);
    void drawRangeElements(PrimitiveMode mode,
                           GLuint start,
                           GLuint end,
                           GLsizei count,
                           GLenum type,
                           const void *indices);
    void drawArraysIndirect(PrimitiveMode mode, const void *indirect);
    void drawElementsIndirect(PrimitiveMode mode, GLenum type, const void *indirect);

    void blitFramebuffer(GLint srcX0,
                         GLint srcY0,
                         GLint srcX1,
                         GLint srcY1,
                         GLint dstX0,
                         GLint dstY0,
                         GLint dstX1,
                         GLint dstY1,
                         GLbitfield mask,
                         GLenum filter);

    void readPixels(GLint x,
                    GLint y,
                    GLsizei width,
                    GLsizei height,
                    GLenum format,
                    GLenum type,
                    void *pixels);
    void readPixelsRobust(GLint x,
                          GLint y,
                          GLsizei width,
                          GLsizei height,
                          GLenum format,
                          GLenum type,
                          GLsizei bufSize,
                          GLsizei *length,
                          GLsizei *columns,
                          GLsizei *rows,
                          void *pixels);
    void readnPixelsRobust(GLint x,
                           GLint y,
                           GLsizei width,
                           GLsizei height,
                           GLenum format,
                           GLenum type,
                           GLsizei bufSize,
                           GLsizei *length,
                           GLsizei *columns,
                           GLsizei *rows,
                           void *data);

    void copyTexImage2D(TextureTarget target,
                        GLint level,
                        GLenum internalformat,
                        GLint x,
                        GLint y,
                        GLsizei width,
                        GLsizei height,
                        GLint border);

    void copyTexSubImage2D(TextureTarget target,
                           GLint level,
                           GLint xoffset,
                           GLint yoffset,
                           GLint x,
                           GLint y,
                           GLsizei width,
                           GLsizei height);

    void copyTexSubImage3D(TextureType target,
                           GLint level,
                           GLint xoffset,
                           GLint yoffset,
                           GLint zoffset,
                           GLint x,
                           GLint y,
                           GLsizei width,
                           GLsizei height);

    void framebufferTexture2D(GLenum target,
                              GLenum attachment,
                              TextureTarget textarget,
                              GLuint texture,
                              GLint level);

    void framebufferRenderbuffer(GLenum target,
                                 GLenum attachment,
                                 GLenum renderbuffertarget,
                                 GLuint renderbuffer);

    void framebufferTextureLayer(GLenum target,
                                 GLenum attachment,
                                 GLuint texture,
                                 GLint level,
                                 GLint layer);
    void framebufferTextureMultiviewLayered(GLenum target,
                                            GLenum attachment,
                                            GLuint texture,
                                            GLint level,
                                            GLint baseViewIndex,
                                            GLsizei numViews);
    void framebufferTextureMultiviewSideBySide(GLenum target,
                                               GLenum attachment,
                                               GLuint texture,
                                               GLint level,
                                               GLsizei numViews,
                                               const GLint *viewportOffsets);

    void drawBuffers(GLsizei n, const GLenum *bufs);
    void readBuffer(GLenum mode);

    void discardFramebuffer(GLenum target, GLsizei numAttachments, const GLenum *attachments);
    void invalidateFramebuffer(GLenum target, GLsizei numAttachments, const GLenum *attachments);
    void invalidateSubFramebuffer(GLenum target,
                                  GLsizei numAttachments,
                                  const GLenum *attachments,
                                  GLint x,
                                  GLint y,
                                  GLsizei width,
                                  GLsizei height);

    void texImage2D(TextureTarget target,
                    GLint level,
                    GLint internalformat,
                    GLsizei width,
                    GLsizei height,
                    GLint border,
                    GLenum format,
                    GLenum type,
                    const void *pixels);
    void texImage2DRobust(TextureTarget target,
                          GLint level,
                          GLint internalformat,
                          GLsizei width,
                          GLsizei height,
                          GLint border,
                          GLenum format,
                          GLenum type,
                          GLsizei bufSize,
                          const void *pixels);
    void texImage3D(TextureType target,
                    GLint level,
                    GLint internalformat,
                    GLsizei width,
                    GLsizei height,
                    GLsizei depth,
                    GLint border,
                    GLenum format,
                    GLenum type,
                    const void *pixels);
    void texImage3DRobust(TextureType target,
                          GLint level,
                          GLint internalformat,
                          GLsizei width,
                          GLsizei height,
                          GLsizei depth,
                          GLint border,
                          GLenum format,
                          GLenum type,
                          GLsizei bufSize,
                          const void *pixels);
    void texSubImage2D(TextureTarget target,
                       GLint level,
                       GLint xoffset,
                       GLint yoffset,
                       GLsizei width,
                       GLsizei height,
                       GLenum format,
                       GLenum type,
                       const void *pixels);
    void texSubImage2DRobust(TextureTarget target,
                             GLint level,
                             GLint xoffset,
                             GLint yoffset,
                             GLsizei width,
                             GLsizei height,
                             GLenum format,
                             GLenum type,
                             GLsizei bufSize,
                             const void *pixels);
    void texSubImage3D(TextureType target,
                       GLint level,
                       GLint xoffset,
                       GLint yoffset,
                       GLint zoffset,
                       GLsizei width,
                       GLsizei height,
                       GLsizei depth,
                       GLenum format,
                       GLenum type,
                       const void *pixels);
    void texSubImage3DRobust(TextureType target,
                             GLint level,
                             GLint xoffset,
                             GLint yoffset,
                             GLint zoffset,
                             GLsizei width,
                             GLsizei height,
                             GLsizei depth,
                             GLenum format,
                             GLenum type,
                             GLsizei bufSize,
                             const void *pixels);
    void compressedTexImage2D(TextureTarget target,
                              GLint level,
                              GLenum internalformat,
                              GLsizei width,
                              GLsizei height,
                              GLint border,
                              GLsizei imageSize,
                              const void *data);
    void compressedTexImage2DRobust(TextureTarget target,
                                    GLint level,
                                    GLenum internalformat,
                                    GLsizei width,
                                    GLsizei height,
                                    GLint border,
                                    GLsizei imageSize,
                                    GLsizei dataSize,
                                    const GLvoid *data);
    void compressedTexImage3D(TextureType target,
                              GLint level,
                              GLenum internalformat,
                              GLsizei width,
                              GLsizei height,
                              GLsizei depth,
                              GLint border,
                              GLsizei imageSize,
                              const void *data);
    void compressedTexImage3DRobust(TextureType target,
                                    GLint level,
                                    GLenum internalformat,
                                    GLsizei width,
                                    GLsizei height,
                                    GLsizei depth,
                                    GLint border,
                                    GLsizei imageSize,
                                    GLsizei dataSize,
                                    const GLvoid *data);
    void compressedTexSubImage2D(TextureTarget target,
                                 GLint level,
                                 GLint xoffset,
                                 GLint yoffset,
                                 GLsizei width,
                                 GLsizei height,
                                 GLenum format,
                                 GLsizei imageSize,
                                 const void *data);
    void compressedTexSubImage2DRobust(TextureTarget target,
                                       GLint level,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLsizei width,
                                       GLsizei height,
                                       GLenum format,
                                       GLsizei imageSize,
                                       GLsizei dataSize,
                                       const GLvoid *data);
    void compressedTexSubImage3D(TextureType target,
                                 GLint level,
                                 GLint xoffset,
                                 GLint yoffset,
                                 GLint zoffset,
                                 GLsizei width,
                                 GLsizei height,
                                 GLsizei depth,
                                 GLenum format,
                                 GLsizei imageSize,
                                 const void *data);
    void compressedTexSubImage3DRobust(TextureType target,
                                       GLint level,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLint zoffset,
                                       GLsizei width,
                                       GLsizei height,
                                       GLsizei depth,
                                       GLenum format,
                                       GLsizei imageSize,
                                       GLsizei dataSize,
                                       const GLvoid *data);
    void copyTexture(GLuint sourceId,
                     GLint sourceLevel,
                     TextureTarget destTarget,
                     GLuint destId,
                     GLint destLevel,
                     GLint internalFormat,
                     GLenum destType,
                     GLboolean unpackFlipY,
                     GLboolean unpackPremultiplyAlpha,
                     GLboolean unpackUnmultiplyAlpha);
    void copySubTexture(GLuint sourceId,
                        GLint sourceLevel,
                        TextureTarget destTarget,
                        GLuint destId,
                        GLint destLevel,
                        GLint xoffset,
                        GLint yoffset,
                        GLint x,
                        GLint y,
                        GLsizei width,
                        GLsizei height,
                        GLboolean unpackFlipY,
                        GLboolean unpackPremultiplyAlpha,
                        GLboolean unpackUnmultiplyAlpha);
    void compressedCopyTexture(GLuint sourceId, GLuint destId);

    void generateMipmap(TextureType target);

    void flush();
    void finish();

    void getBufferPointerv(BufferBinding target, GLenum pname, void **params);
    void getBufferPointervRobust(BufferBinding target,
                                 GLenum pname,
                                 GLsizei bufSize,
                                 GLsizei *length,
                                 void **params);
    void *mapBuffer(BufferBinding target, GLenum access);
    GLboolean unmapBuffer(BufferBinding target);
    void *mapBufferRange(BufferBinding target,
                         GLintptr offset,
                         GLsizeiptr length,
                         GLbitfield access);
    void flushMappedBufferRange(BufferBinding target, GLintptr offset, GLsizeiptr length);

    void beginTransformFeedback(PrimitiveMode primitiveMode);

    bool hasActiveTransformFeedback(GLuint program) const;

    void insertEventMarker(GLsizei length, const char *marker);
    void pushGroupMarker(GLsizei length, const char *marker);
    void popGroupMarker();

    void bindUniformLocation(GLuint program, GLint location, const GLchar *name);
    void renderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
    void renderbufferStorageMultisample(GLenum target,
                                        GLsizei samples,
                                        GLenum internalformat,
                                        GLsizei width,
                                        GLsizei height);

    void getSynciv(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei *length, GLint *values);

    // CHROMIUM_framebuffer_mixed_samples
    void coverageModulation(GLenum components);

    // CHROMIUM_path_rendering
    void matrixLoadf(GLenum matrixMode, const GLfloat *matrix);
    void matrixLoadIdentity(GLenum matrixMode);
    void stencilFillPath(GLuint path, GLenum fillMode, GLuint mask);
    void stencilStrokePath(GLuint path, GLint reference, GLuint mask);
    void coverFillPath(GLuint path, GLenum coverMode);
    void coverStrokePath(GLuint path, GLenum coverMode);
    void stencilThenCoverFillPath(GLuint path, GLenum fillMode, GLuint mask, GLenum coverMode);
    void stencilThenCoverStrokePath(GLuint path, GLint reference, GLuint mask, GLenum coverMode);
    void coverFillPathInstanced(GLsizei numPaths,
                                GLenum pathNameType,
                                const void *paths,
                                GLuint pathBase,
                                GLenum coverMode,
                                GLenum transformType,
                                const GLfloat *transformValues);
    void coverStrokePathInstanced(GLsizei numPaths,
                                  GLenum pathNameType,
                                  const void *paths,
                                  GLuint pathBase,
                                  GLenum coverMode,
                                  GLenum transformType,
                                  const GLfloat *transformValues);
    void stencilFillPathInstanced(GLsizei numPaths,
                                  GLenum pathNameType,
                                  const void *paths,
                                  GLuint pathBAse,
                                  GLenum fillMode,
                                  GLuint mask,
                                  GLenum transformType,
                                  const GLfloat *transformValues);
    void stencilStrokePathInstanced(GLsizei numPaths,
                                    GLenum pathNameType,
                                    const void *paths,
                                    GLuint pathBase,
                                    GLint reference,
                                    GLuint mask,
                                    GLenum transformType,
                                    const GLfloat *transformValues);
    void stencilThenCoverFillPathInstanced(GLsizei numPaths,
                                           GLenum pathNameType,
                                           const void *paths,
                                           GLuint pathBase,
                                           GLenum fillMode,
                                           GLuint mask,
                                           GLenum coverMode,
                                           GLenum transformType,
                                           const GLfloat *transformValues);
    void stencilThenCoverStrokePathInstanced(GLsizei numPaths,
                                             GLenum pathNameType,
                                             const void *paths,
                                             GLuint pathBase,
                                             GLint reference,
                                             GLuint mask,
                                             GLenum coverMode,
                                             GLenum transformType,
                                             const GLfloat *transformValues);
    void bindFragmentInputLocation(GLuint program, GLint location, const GLchar *name);
    void programPathFragmentInputGen(GLuint program,
                                     GLint location,
                                     GLenum genMode,
                                     GLint components,
                                     const GLfloat *coeffs);

    void bufferData(BufferBinding target, GLsizeiptr size, const void *data, BufferUsage usage);
    void bufferSubData(BufferBinding target, GLintptr offset, GLsizeiptr size, const void *data);
    void attachShader(GLuint program, GLuint shader);
    void bindAttribLocation(GLuint program, GLuint index, const GLchar *name);
    void bindBuffer(BufferBinding target, GLuint buffer);
    void bindBufferBase(BufferBinding target, GLuint index, GLuint buffer);
    void bindBufferRange(BufferBinding target,
                         GLuint index,
                         GLuint buffer,
                         GLintptr offset,
                         GLsizeiptr size);
    void bindFramebuffer(GLenum target, GLuint framebuffer);
    void bindRenderbuffer(GLenum target, GLuint renderbuffer);

    void texStorage2DMultisample(TextureType target,
                                 GLsizei samples,
                                 GLenum internalformat,
                                 GLsizei width,
                                 GLsizei height,
                                 GLboolean fixedsamplelocations);

    void texStorage3DMultisample(TextureType target,
                                 GLsizei samples,
                                 GLenum internalformat,
                                 GLsizei width,
                                 GLsizei height,
                                 GLsizei depth,
                                 GLboolean fixedsamplelocations);

    void getMultisamplefv(GLenum pname, GLuint index, GLfloat *val);
    void getMultisamplefvRobust(GLenum pname,
                                GLuint index,
                                GLsizei bufSize,
                                GLsizei *length,
                                GLfloat *val);

    void copyBufferSubData(BufferBinding readTarget,
                           BufferBinding writeTarget,
                           GLintptr readOffset,
                           GLintptr writeOffset,
                           GLsizeiptr size);

    GLenum checkFramebufferStatus(GLenum target);
    void compileShader(GLuint shader);
    void deleteBuffers(GLsizei n, const GLuint *buffers);
    void deleteFramebuffers(GLsizei n, const GLuint *framebuffers);
    void deleteRenderbuffers(GLsizei n, const GLuint *renderbuffers);
    void deleteTextures(GLsizei n, const GLuint *textures);
    void detachShader(GLuint program, GLuint shader);
    void genBuffers(GLsizei n, GLuint *buffers);
    void genFramebuffers(GLsizei n, GLuint *framebuffers);
    void genRenderbuffers(GLsizei n, GLuint *renderbuffers);
    void genTextures(GLsizei n, GLuint *textures);
    void getActiveAttrib(GLuint program,
                         GLuint index,
                         GLsizei bufsize,
                         GLsizei *length,
                         GLint *size,
                         GLenum *type,
                         GLchar *name);
    void getActiveUniform(GLuint program,
                          GLuint index,
                          GLsizei bufsize,
                          GLsizei *length,
                          GLint *size,
                          GLenum *type,
                          GLchar *name);
    void getAttachedShaders(GLuint program, GLsizei maxcount, GLsizei *count, GLuint *shaders);
    GLint getAttribLocation(GLuint program, const GLchar *name);
    void getProgramiv(GLuint program, GLenum pname, GLint *params);
    void getProgramivRobust(GLuint program,
                            GLenum pname,
                            GLsizei bufSize,
                            GLsizei *length,
                            GLint *params);
    void getProgramPipelineiv(GLuint pipeline, GLenum pname, GLint *params);
    void getProgramInfoLog(GLuint program, GLsizei bufsize, GLsizei *length, GLchar *infolog);
    void getProgramPipelineInfoLog(GLuint pipeline,
                                   GLsizei bufSize,
                                   GLsizei *length,
                                   GLchar *infoLog);
    void getShaderiv(GLuint shader, GLenum pname, GLint *params);
    void getShaderivRobust(GLuint shader,
                           GLenum pname,
                           GLsizei bufSize,
                           GLsizei *length,
                           GLint *params);

    void getShaderInfoLog(GLuint shader, GLsizei bufsize, GLsizei *length, GLchar *infolog);
    void getShaderPrecisionFormat(GLenum shadertype,
                                  GLenum precisiontype,
                                  GLint *range,
                                  GLint *precision);
    void getShaderSource(GLuint shader, GLsizei bufsize, GLsizei *length, GLchar *source);
    void getUniformfv(GLuint program, GLint location, GLfloat *params);
    void getUniformfvRobust(GLuint program,
                            GLint location,
                            GLsizei bufSize,
                            GLsizei *length,
                            GLfloat *params);
    void getUniformiv(GLuint program, GLint location, GLint *params);
    void getUniformivRobust(GLuint program,
                            GLint location,
                            GLsizei bufSize,
                            GLsizei *length,
                            GLint *params);
    GLint getUniformLocation(GLuint program, const GLchar *name);
    GLboolean isBuffer(GLuint buffer);
    GLboolean isEnabled(GLenum cap);
    GLboolean isFramebuffer(GLuint framebuffer);
    GLboolean isProgram(GLuint program);
    GLboolean isRenderbuffer(GLuint renderbuffer);
    GLboolean isShader(GLuint shader);
    GLboolean isTexture(GLuint texture);
    void linkProgram(GLuint program);
    void releaseShaderCompiler();
    void shaderBinary(GLsizei n,
                      const GLuint *shaders,
                      GLenum binaryformat,
                      const void *binary,
                      GLsizei length);
    void shaderSource(GLuint shader,
                      GLsizei count,
                      const GLchar *const *string,
                      const GLint *length);
    void stencilFunc(GLenum func, GLint ref, GLuint mask);
    void stencilMask(GLuint mask);
    void stencilOp(GLenum fail, GLenum zfail, GLenum zpass);
    void uniform1f(GLint location, GLfloat x);
    void uniform1fv(GLint location, GLsizei count, const GLfloat *v);
    void uniform1i(GLint location, GLint x);
    void uniform1iv(GLint location, GLsizei count, const GLint *v);
    void uniform2f(GLint location, GLfloat x, GLfloat y);
    void uniform2fv(GLint location, GLsizei count, const GLfloat *v);
    void uniform2i(GLint location, GLint x, GLint y);
    void uniform2iv(GLint location, GLsizei count, const GLint *v);
    void uniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z);
    void uniform3fv(GLint location, GLsizei count, const GLfloat *v);
    void uniform3i(GLint location, GLint x, GLint y, GLint z);
    void uniform3iv(GLint location, GLsizei count, const GLint *v);
    void uniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
    void uniform4fv(GLint location, GLsizei count, const GLfloat *v);
    void uniform4i(GLint location, GLint x, GLint y, GLint z, GLint w);
    void uniform4iv(GLint location, GLsizei count, const GLint *v);
    void uniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void uniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void uniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void validateProgram(GLuint program);
    void validateProgramPipeline(GLuint pipeline);

    void genQueries(GLsizei n, GLuint *ids);
    void deleteQueries(GLsizei n, const GLuint *ids);
    GLboolean isQuery(GLuint id);

    void uniform1ui(GLint location, GLuint v0);
    void uniform2ui(GLint location, GLuint v0, GLuint v1);
    void uniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2);
    void uniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
    void uniform1uiv(GLint location, GLsizei count, const GLuint *value);
    void uniform2uiv(GLint location, GLsizei count, const GLuint *value);
    void uniform3uiv(GLint location, GLsizei count, const GLuint *value);
    void uniform4uiv(GLint location, GLsizei count, const GLuint *value);

    void uniformMatrix2x3fv(GLint location,
                            GLsizei count,
                            GLboolean transpose,
                            const GLfloat *value);
    void uniformMatrix3x2fv(GLint location,
                            GLsizei count,
                            GLboolean transpose,
                            const GLfloat *value);
    void uniformMatrix2x4fv(GLint location,
                            GLsizei count,
                            GLboolean transpose,
                            const GLfloat *value);
    void uniformMatrix4x2fv(GLint location,
                            GLsizei count,
                            GLboolean transpose,
                            const GLfloat *value);
    void uniformMatrix3x4fv(GLint location,
                            GLsizei count,
                            GLboolean transpose,
                            const GLfloat *value);
    void uniformMatrix4x3fv(GLint location,
                            GLsizei count,
                            GLboolean transpose,
                            const GLfloat *value);

    void deleteVertexArrays(GLsizei n, const GLuint *arrays);
    void genVertexArrays(GLsizei n, GLuint *arrays);
    bool isVertexArray(GLuint array);

    void endTransformFeedback();
    void transformFeedbackVaryings(GLuint program,
                                   GLsizei count,
                                   const GLchar *const *varyings,
                                   GLenum bufferMode);
    void getTransformFeedbackVarying(GLuint program,
                                     GLuint index,
                                     GLsizei bufSize,
                                     GLsizei *length,
                                     GLsizei *size,
                                     GLenum *type,
                                     GLchar *name);

    void deleteTransformFeedbacks(GLsizei n, const GLuint *ids);
    void genTransformFeedbacks(GLsizei n, GLuint *ids);
    bool isTransformFeedback(GLuint id);
    void pauseTransformFeedback();
    void resumeTransformFeedback();

    void getProgramBinary(GLuint program,
                          GLsizei bufSize,
                          GLsizei *length,
                          GLenum *binaryFormat,
                          void *binary);
    void programBinary(GLuint program, GLenum binaryFormat, const void *binary, GLsizei length);

    void getUniformuiv(GLuint program, GLint location, GLuint *params);
    void getUniformuivRobust(GLuint program,
                             GLint location,
                             GLsizei bufSize,
                             GLsizei *length,
                             GLuint *params);
    GLint getFragDataLocation(GLuint program, const GLchar *name);
    void getUniformIndices(GLuint program,
                           GLsizei uniformCount,
                           const GLchar *const *uniformNames,
                           GLuint *uniformIndices);
    void getActiveUniformsiv(GLuint program,
                             GLsizei uniformCount,
                             const GLuint *uniformIndices,
                             GLenum pname,
                             GLint *params);
    GLuint getUniformBlockIndex(GLuint program, const GLchar *uniformBlockName);
    void getActiveUniformBlockiv(GLuint program,
                                 GLuint uniformBlockIndex,
                                 GLenum pname,
                                 GLint *params);
    void getActiveUniformBlockivRobust(GLuint program,
                                       GLuint uniformBlockIndex,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       GLsizei *length,
                                       GLint *params);
    void getActiveUniformBlockName(GLuint program,
                                   GLuint uniformBlockIndex,
                                   GLsizei bufSize,
                                   GLsizei *length,
                                   GLchar *uniformBlockName);
    void uniformBlockBinding(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);

    GLsync fenceSync(GLenum condition, GLbitfield flags);
    GLboolean isSync(GLsync sync);
    void deleteSync(GLsync sync);
    GLenum clientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout);
    void waitSync(GLsync sync, GLbitfield flags, GLuint64 timeout);
    void getInteger64v(GLenum pname, GLint64 *params);
    void getInteger64vRobust(GLenum pname, GLsizei bufSize, GLsizei *length, GLint64 *data);

    void getBufferParameteri64v(BufferBinding target, GLenum pname, GLint64 *params);
    void getBufferParameteri64vRobust(BufferBinding target,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei *length,
                                      GLint64 *params);
    void genSamplers(GLsizei count, GLuint *samplers);
    void deleteSamplers(GLsizei count, const GLuint *samplers);
    void getInternalformativ(GLenum target,
                             GLenum internalformat,
                             GLenum pname,
                             GLsizei bufSize,
                             GLint *params);
    void getInternalformativRobust(GLenum target,
                                   GLenum internalformat,
                                   GLenum pname,
                                   GLsizei bufSize,
                                   GLsizei *length,
                                   GLint *params);

    void programUniform1i(GLuint program, GLint location, GLint v0);
    void programUniform2i(GLuint program, GLint location, GLint v0, GLint v1);
    void programUniform3i(GLuint program, GLint location, GLint v0, GLint v1, GLint v2);
    void programUniform4i(GLuint program, GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
    void programUniform1ui(GLuint program, GLint location, GLuint v0);
    void programUniform2ui(GLuint program, GLint location, GLuint v0, GLuint v1);
    void programUniform3ui(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2);
    void programUniform4ui(GLuint program,
                           GLint location,
                           GLuint v0,
                           GLuint v1,
                           GLuint v2,
                           GLuint v3);
    void programUniform1f(GLuint program, GLint location, GLfloat v0);
    void programUniform2f(GLuint program, GLint location, GLfloat v0, GLfloat v1);
    void programUniform3f(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
    void programUniform4f(GLuint program,
                          GLint location,
                          GLfloat v0,
                          GLfloat v1,
                          GLfloat v2,
                          GLfloat v3);
    void programUniform1iv(GLuint program, GLint location, GLsizei count, const GLint *value);
    void programUniform2iv(GLuint program, GLint location, GLsizei count, const GLint *value);
    void programUniform3iv(GLuint program, GLint location, GLsizei count, const GLint *value);
    void programUniform4iv(GLuint program, GLint location, GLsizei count, const GLint *value);
    void programUniform1uiv(GLuint program, GLint location, GLsizei count, const GLuint *value);
    void programUniform2uiv(GLuint program, GLint location, GLsizei count, const GLuint *value);
    void programUniform3uiv(GLuint program, GLint location, GLsizei count, const GLuint *value);
    void programUniform4uiv(GLuint program, GLint location, GLsizei count, const GLuint *value);
    void programUniform1fv(GLuint program, GLint location, GLsizei count, const GLfloat *value);
    void programUniform2fv(GLuint program, GLint location, GLsizei count, const GLfloat *value);
    void programUniform3fv(GLuint program, GLint location, GLsizei count, const GLfloat *value);
    void programUniform4fv(GLuint program, GLint location, GLsizei count, const GLfloat *value);

    void programUniformMatrix2fv(GLuint program,
                                 GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat *value);

    void programUniformMatrix3fv(GLuint program,
                                 GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat *value);

    void programUniformMatrix4fv(GLuint program,
                                 GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat *value);

    void programUniformMatrix2x3fv(GLuint program,
                                   GLint location,
                                   GLsizei count,
                                   GLboolean transpose,
                                   const GLfloat *value);

    void programUniformMatrix3x2fv(GLuint program,
                                   GLint location,
                                   GLsizei count,
                                   GLboolean transpose,
                                   const GLfloat *value);

    void programUniformMatrix2x4fv(GLuint program,
                                   GLint location,
                                   GLsizei count,
                                   GLboolean transpose,
                                   const GLfloat *value);

    void programUniformMatrix4x2fv(GLuint program,
                                   GLint location,
                                   GLsizei count,
                                   GLboolean transpose,
                                   const GLfloat *value);

    void programUniformMatrix3x4fv(GLuint program,
                                   GLint location,
                                   GLsizei count,
                                   GLboolean transpose,
                                   const GLfloat *value);

    void programUniformMatrix4x3fv(GLuint program,
                                   GLint location,
                                   GLsizei count,
                                   GLboolean transpose,
                                   const GLfloat *value);

    void deleteProgramPipelines(GLsizei n, const GLuint *pipelines);
    void genProgramPipelines(GLsizei n, GLuint *pipelines);
    GLboolean isProgramPipeline(GLuint pipeline);

    void getTranslatedShaderSource(GLuint shader, GLsizei bufsize, GLsizei *length, GLchar *source);
    void getnUniformfv(GLuint program, GLint location, GLsizei bufSize, GLfloat *params);
    void getnUniformfvRobust(GLuint program,
                             GLint location,
                             GLsizei bufSize,
                             GLsizei *length,
                             GLfloat *params);
    void getnUniformiv(GLuint program, GLint location, GLsizei bufSize, GLint *params);
    void getnUniformivRobust(GLuint program,
                             GLint location,
                             GLsizei bufSize,
                             GLsizei *length,
                             GLint *params);
    void getnUniformuivRobust(GLuint program,
                              GLint location,
                              GLsizei bufSize,
                              GLsizei *length,
                              GLuint *params);
    void readnPixels(GLint x,
                     GLint y,
                     GLsizei width,
                     GLsizei height,
                     GLenum format,
                     GLenum type,
                     GLsizei bufSize,
                     void *data);
    void eGLImageTargetTexture2D(TextureType target, GLeglImageOES image);
    void eGLImageTargetRenderbufferStorage(GLenum target, GLeglImageOES image);

    void getFramebufferParameteriv(GLenum target, GLenum pname, GLint *params);
    void getFramebufferParameterivRobust(GLenum target,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         GLsizei *length,
                                         GLint *params);
    void framebufferParameteri(GLenum target, GLenum pname, GLint param);

    void dispatchCompute(GLuint numGroupsX, GLuint numGroupsY, GLuint numGroupsZ);
    void dispatchComputeIndirect(GLintptr indirect);

    void texStorage1D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width);
    void texStorage2D(TextureType target,
                      GLsizei levels,
                      GLenum internalFormat,
                      GLsizei width,
                      GLsizei height);
    void texStorage3D(TextureType target,
                      GLsizei levels,
                      GLenum internalFormat,
                      GLsizei width,
                      GLsizei height,
                      GLsizei depth);

    void memoryBarrier(GLbitfield barriers);
    void memoryBarrierByRegion(GLbitfield barriers);

    void framebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level);

    // Consumes the error.
    // TODO(jmadill): Remove const. http://anglebug.com/2378
    void handleError(const Error &error) const;

    GLenum getError();
    void markContextLost();
    bool isContextLost() const;
    GLenum getGraphicsResetStatus();
    bool isResetNotificationEnabled();

    const egl::Config *getConfig() const;
    EGLenum getClientType() const;
    EGLenum getRenderBuffer() const;

    const GLubyte *getString(GLenum name) const;
    const GLubyte *getStringi(GLenum name, GLuint index) const;

    size_t getExtensionStringCount() const;

    bool isExtensionRequestable(const char *name);
    void requestExtension(const char *name);
    size_t getRequestableExtensionStringCount() const;

    rx::ContextImpl *getImplementation() const { return mImplementation.get(); }
    const Workarounds &getWorkarounds() const;

    ANGLE_NO_DISCARD bool getScratchBuffer(size_t requestedSizeBytes,
                                           angle::MemoryBuffer **scratchBufferOut) const;
    ANGLE_NO_DISCARD bool getZeroFilledBuffer(size_t requstedSizeBytes,
                                              angle::MemoryBuffer **zeroBufferOut) const;

    Error prepareForDispatch();

    MemoryProgramCache *getMemoryProgramCache() const { return mMemoryProgramCache; }

    template <EntryPoint EP, typename... ParamsT>
    void gatherParams(ParamsT &&... params);

    // Notification for a state change in a Texture.
    void onTextureChange(const Texture *texture);

    bool hasBeenCurrent() const { return mHasBeenCurrent; }
    egl::Display *getCurrentDisplay() const { return mCurrentDisplay; }
    egl::Surface *getCurrentDrawSurface() const { return mCurrentSurface; }
    egl::Surface *getCurrentReadSurface() const { return mCurrentSurface; }

    bool isRobustResourceInitEnabled() const { return mGLState.isRobustResourceInitEnabled(); }

    bool isCurrentTransformFeedback(const TransformFeedback *tf) const;

    bool isCurrentVertexArray(const VertexArray *va) const
    {
        return mGLState.isCurrentVertexArray(va);
    }

    const ContextState &getContextState() const { return mState; }
    GLint getClientMajorVersion() const { return mState.getClientMajorVersion(); }
    GLint getClientMinorVersion() const { return mState.getClientMinorVersion(); }
    const Version &getClientVersion() const { return mState.getClientVersion(); }
    const State &getGLState() const { return mState.getState(); }
    const Caps &getCaps() const { return mState.getCaps(); }
    const TextureCapsMap &getTextureCaps() const { return mState.getTextureCaps(); }
    const Extensions &getExtensions() const { return mState.getExtensions(); }
    const Limitations &getLimitations() const { return mState.getLimitations(); }
    bool skipValidation() const { return mSkipValidation; }
    bool isGLES1() const;

    // Specific methods needed for validation.
    bool getQueryParameterInfo(GLenum pname, GLenum *type, unsigned int *numParams);
    bool getIndexedQueryParameterInfo(GLenum target, GLenum *type, unsigned int *numParams);

    Program *getProgram(GLuint handle) const;
    Shader *getShader(GLuint handle) const;

    bool isTextureGenerated(GLuint texture) const;
    bool isBufferGenerated(GLuint buffer) const
    {
        return mState.mBuffers->isHandleGenerated(buffer);
    }

    bool isRenderbufferGenerated(GLuint renderbuffer) const;
    bool isFramebufferGenerated(GLuint framebuffer) const;
    bool isProgramPipelineGenerated(GLuint pipeline) const;

    bool usingDisplayTextureShareGroup() const;

    // Hack for the special WebGL 1 "DEPTH_STENCIL" internal format.
    GLenum getConvertedRenderbufferFormat(GLenum internalformat) const;

    bool isWebGL() const { return mState.isWebGL(); }
    bool isWebGL1() const { return mState.isWebGL1(); }

    template <typename T>
    const T &getParams() const;

    bool isValidBufferBinding(BufferBinding binding) const { return mValidBufferBindings[binding]; }

    // GLES1 emulation: Renderer level (for validation)
    int vertexArrayIndex(ClientVertexArrayType type) const;
    static int TexCoordArrayIndex(unsigned int unit);

    // GL_KHR_parallel_shader_compile
    void maxShaderCompilerThreads(GLuint count);
    std::shared_ptr<angle::WorkerThreadPool> getWorkerThreadPool() const { return mThreadPool; }

    const StateCache &getStateCache() const { return mStateCache; }

    void onSubjectStateChange(const Context *context,
                              angle::SubjectIndex index,
                              angle::SubjectMessage message) override;

    // Do we care about the order of the provoking vertex?
    bool provokingVertexDontCare() const { return mProvokingVertexDontCare; }

  private:
    void initialize();

    bool noopDraw(PrimitiveMode mode, GLsizei count);
    bool noopDrawInstanced(PrimitiveMode mode, GLsizei count, GLsizei instanceCount);

    Error prepareForDraw(PrimitiveMode mode);
    Error prepareForClear(GLbitfield mask);
    Error prepareForClearBuffer(GLenum buffer, GLint drawbuffer);
    Error syncState(const State::DirtyBits &bitMask, const State::DirtyObjects &objectMask);
    Error syncDirtyBits();
    Error syncDirtyBits(const State::DirtyBits &bitMask);
    Error syncDirtyObjects(const State::DirtyObjects &objectMask);
    Error syncStateForReadPixels();
    Error syncStateForTexImage();
    Error syncStateForBlit();
    Error syncStateForPathOperation();

    VertexArray *checkVertexArrayAllocation(GLuint vertexArrayHandle);
    TransformFeedback *checkTransformFeedbackAllocation(GLuint transformFeedback);

    void detachBuffer(Buffer *buffer);
    void detachTexture(GLuint texture);
    void detachFramebuffer(GLuint framebuffer);
    void detachRenderbuffer(GLuint renderbuffer);
    void detachVertexArray(GLuint vertexArray);
    void detachTransformFeedback(GLuint transformFeedback);
    void detachSampler(GLuint sampler);
    void detachProgramPipeline(GLuint pipeline);

    void initRendererString();
    void initVersionStrings();
    void initExtensionStrings();

    Extensions generateSupportedExtensions() const;
    void initCaps();
    void updateCaps();
    void initWorkarounds();

    gl::LabeledObject *getLabeledObject(GLenum identifier, GLuint name) const;
    gl::LabeledObject *getLabeledObjectFromPtr(const void *ptr) const;

    void setUniform1iImpl(Program *program, GLint location, GLsizei count, const GLint *v);

    ContextState mState;
    bool mSkipValidation;
    bool mDisplayTextureShareGroup;

    // Stores for each buffer binding type whether is it allowed to be used in this context.
    angle::PackedEnumBitSet<BufferBinding> mValidBufferBindings;

    // Caches entry point parameters and values re-used between layers.
    mutable const ParamTypeInfo *mSavedArgsType;
    static constexpr size_t kParamsBufferSize = 128u;
    mutable std::array<uint8_t, kParamsBufferSize> mParamsBuffer;

    std::unique_ptr<rx::ContextImpl> mImplementation;

    EGLLabelKHR mLabel;

    // Caps to use for validation
    Caps mCaps;
    TextureCapsMap mTextureCaps;
    Extensions mExtensions;
    Limitations mLimitations;

    // Extensions supported by the implementation plus extensions that are implemented entirely
    // within the frontend.
    Extensions mSupportedExtensions;

    // Shader compiler. Lazily initialized hence the mutable value.
    mutable BindingPointer<Compiler> mCompiler;

    State mGLState;

    const egl::Config *mConfig;
    EGLenum mClientType;

    TextureMap mZeroTextures;

    ResourceMap<FenceNV> mFenceNVMap;
    HandleAllocator mFenceNVHandleAllocator;

    ResourceMap<Query> mQueryMap;
    HandleAllocator mQueryHandleAllocator;

    ResourceMap<VertexArray> mVertexArrayMap;
    HandleAllocator mVertexArrayHandleAllocator;

    ResourceMap<TransformFeedback> mTransformFeedbackMap;
    HandleAllocator mTransformFeedbackHandleAllocator;

    const char *mVersionString;
    const char *mShadingLanguageString;
    const char *mRendererString;
    const char *mExtensionString;
    std::vector<const char *> mExtensionStrings;
    const char *mRequestableExtensionString;
    std::vector<const char *> mRequestableExtensionStrings;

    // Recorded errors
    ErrorSet mErrors;

    // GLES1 renderer state
    std::unique_ptr<GLES1Renderer> mGLES1Renderer;

    // Current/lost context flags
    bool mHasBeenCurrent;
    bool mContextLost;
    GLenum mResetStatus;
    bool mContextLostForced;
    GLenum mResetStrategy;
    const bool mRobustAccess;
    const bool mSurfacelessSupported;
    const bool mExplicitContextAvailable;
    egl::Surface *mCurrentSurface;
    egl::Display *mCurrentDisplay;
    const bool mWebGLContext;
    const bool mExtensionsEnabled;
    const bool mProvokingVertexDontCare;
    MemoryProgramCache *mMemoryProgramCache;

    State::DirtyObjects mDrawDirtyObjects;
    State::DirtyObjects mPathOperationDirtyObjects;

    StateCache mStateCache;

    State::DirtyBits mTexImageDirtyBits;
    State::DirtyObjects mTexImageDirtyObjects;
    State::DirtyBits mReadPixelsDirtyBits;
    State::DirtyObjects mReadPixelsDirtyObjects;
    State::DirtyBits mClearDirtyBits;
    State::DirtyObjects mClearDirtyObjects;
    State::DirtyBits mBlitDirtyBits;
    State::DirtyObjects mBlitDirtyObjects;
    State::DirtyBits mComputeDirtyBits;
    State::DirtyObjects mComputeDirtyObjects;

    Workarounds mWorkarounds;

    // Binding to container objects that use dependent state updates.
    angle::ObserverBinding mVertexArrayObserverBinding;
    angle::ObserverBinding mDrawFramebufferObserverBinding;
    angle::ObserverBinding mReadFramebufferObserverBinding;
    std::vector<angle::ObserverBinding> mUniformBufferObserverBindings;

    // Not really a property of context state. The size and contexts change per-api-call.
    mutable angle::ScratchBuffer mScratchBuffer;
    mutable angle::ScratchBuffer mZeroFilledBuffer;

    std::shared_ptr<angle::WorkerThreadPool> mThreadPool;
};

template <typename T>
const T &Context::getParams() const
{
    const T *params = reinterpret_cast<T *>(mParamsBuffer.data());
    ASSERT(mSavedArgsType->hasDynamicType(T::TypeInfo));
    return *params;
}

template <EntryPoint EP, typename... ArgsT>
ANGLE_INLINE void Context::gatherParams(ArgsT &&... args)
{
    static_assert(sizeof(EntryPointParamType<EP>) <= kParamsBufferSize,
                  "Params struct too large, please increase kParamsBufferSize.");

    mSavedArgsType = &EntryPointParamType<EP>::TypeInfo;

    // Skip doing any work for ParamsBase/Invalid type.
    if (!EntryPointParamType<EP>::TypeInfo.isValid())
    {
        return;
    }

    EntryPointParamType<EP> *objBuffer =
        reinterpret_cast<EntryPointParamType<EP> *>(mParamsBuffer.data());
    EntryPointParamType<EP>::template Factory<EP>(objBuffer, this, std::forward<ArgsT>(args)...);
}

}  // namespace gl

#endif  // LIBANGLE_CONTEXT_H_
