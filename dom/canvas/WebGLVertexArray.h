/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_VERTEX_ARRAY_H_
#define WEBGL_VERTEX_ARRAY_H_

#include "mozilla/LinkedList.h"
#include "nsWrapperCache.h"
#include "WebGLBuffer.h"
#include "WebGLObjectModel.h"
#include "WebGLStrongTypes.h"
#include "WebGLVertexAttribData.h"

namespace mozilla {

class WebGLVertexArrayFake;

class WebGLVertexArray
    : public nsWrapperCache
    , public WebGLRefCountedObject<WebGLVertexArray>
    , public LinkedListElement<WebGLVertexArray>
    , public WebGLContextBoundObject
{
public:
    static WebGLVertexArray* Create(WebGLContext* webgl);

    void BindVertexArray() {
        // Bind to dummy value to signal that this vertex array has ever been
        // bound.
        BindVertexArrayImpl();
    };

    void EnsureAttrib(GLuint index);
    bool HasAttrib(GLuint index) const {
        return index < mAttribs.Length();
    }
    bool IsAttribArrayEnabled(GLuint index) const {
        return HasAttrib(index) && mAttribs[index].enabled;
    }

    // Implement parent classes:
    void Delete();
    bool IsVertexArray();

    WebGLContext* GetParentObject() const {
        return Context();
    }

    virtual JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override;

    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLVertexArray)
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLVertexArray)

    GLuint GLName() const { return mGLName; }

protected:
    explicit WebGLVertexArray(WebGLContext* webgl);

    virtual ~WebGLVertexArray() {
        MOZ_ASSERT(IsDeleted());
    }

    virtual void GenVertexArray() = 0;
    virtual void BindVertexArrayImpl() = 0;
    virtual void DeleteImpl() = 0;
    virtual bool IsVertexArrayImpl() = 0;

    GLuint mGLName;
    nsTArray<WebGLVertexAttribData> mAttribs;
    WebGLRefPtr<WebGLBuffer> mElementArrayBuffer;

    friend class WebGLContext;
    friend class WebGLVertexArrayFake;
    friend class WebGL2Context;
};

} // namespace mozilla

#endif // WEBGL_VERTEX_ARRAY_H_
