/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContextUnchecked.h"

#include "GLContext.h"
#include "WebGLBuffer.h"
#include "WebGLSampler.h"

namespace mozilla {

WebGLContextUnchecked::WebGLContextUnchecked(gl::GLContext* gl)
    : gl(gl)
{ }


// -----------------------------------------------------------------------------
// Buffer Objects

void
WebGLContextUnchecked::BindBuffer(GLenum target, WebGLBuffer* buffer)
{
    gl->MakeCurrent();
    gl->fBindBuffer(target, buffer ? buffer->mGLName : 0);
}

void
WebGLContextUnchecked::BindBufferBase(GLenum target, GLuint index, WebGLBuffer* buffer)
{
    gl->MakeCurrent();
    gl->fBindBufferBase(target, index, buffer ? buffer->mGLName : 0);
}

void
WebGLContextUnchecked::BindBufferRange(GLenum target, GLuint index, WebGLBuffer* buffer, WebGLintptr offset, WebGLsizeiptr size)
{
    gl->MakeCurrent();

#ifdef XP_MACOSX
    if (buffer && buffer->Content() == WebGLBuffer::Kind::Undefined &&
        gl->WorkAroundDriverBugs())
    {
        // BindBufferRange will fail if the buffer's contents is undefined.
        // Bind so driver initializes the buffer.
        gl->fBindBuffer(target, buffer->mGLName);
    }
#endif

    gl->fBindBufferRange(target, index, buffer ? buffer->mGLName : 0, offset, size);
}

void
WebGLContextUnchecked::CopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size)
{
    gl->MakeCurrent();
    gl->fCopyBufferSubData(readTarget, writeTarget, readOffset, writeOffset, size);
}


// -----------------------------------------------------------------------------
// Sampler Objects

void
WebGLContextUnchecked::BindSampler(GLuint unit, WebGLSampler* sampler)
{
    gl->MakeCurrent();
    gl->fBindSampler(unit, sampler ? sampler->mGLName : 0);
}

GLint
WebGLContextUnchecked::GetSamplerParameteriv(WebGLSampler* sampler,
                                             GLenum pname)
{
    MOZ_ASSERT(sampler, "Did you validate?");

    GLint param = 0;
    gl->MakeCurrent();
    gl->fGetSamplerParameteriv(sampler->mGLName, pname, &param);

    return param;
}

GLfloat
WebGLContextUnchecked::GetSamplerParameterfv(WebGLSampler* sampler,
                                             GLenum pname)
{
    MOZ_ASSERT(sampler, "Did you validate?");

    GLfloat param = 0.0f;
    gl->MakeCurrent();
    gl->fGetSamplerParameterfv(sampler->mGLName, pname, &param);
    return param;
}

void
WebGLContextUnchecked::SamplerParameteri(WebGLSampler* sampler,
                                         GLenum pname,
                                         GLint param)
{
    MOZ_ASSERT(sampler, "Did you validate?");
    gl->MakeCurrent();
    gl->fSamplerParameteri(sampler->mGLName, pname, param);
}

void
WebGLContextUnchecked::SamplerParameteriv(WebGLSampler* sampler,
                                          GLenum pname,
                                          const GLint* param)
{
    MOZ_ASSERT(sampler, "Did you validate?");
    gl->MakeCurrent();
    gl->fSamplerParameteriv(sampler->mGLName, pname, param);
}

void
WebGLContextUnchecked::SamplerParameterf(WebGLSampler* sampler,
                                         GLenum pname,
                                         GLfloat param)
{
    MOZ_ASSERT(sampler, "Did you validate?");
    gl->MakeCurrent();
    gl->fSamplerParameterf(sampler->mGLName, pname, param);
}

void
WebGLContextUnchecked::SamplerParameterfv(WebGLSampler* sampler,
                                          GLenum pname,
                                          const GLfloat* param)
{
    MOZ_ASSERT(sampler, "Did you validate?");
    gl->MakeCurrent();
    gl->fSamplerParameterfv(sampler->mGLName, pname, param);
}

} // namespace mozilla
