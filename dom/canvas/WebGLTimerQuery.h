/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_TIMER_QUERY_H_
#define WEBGL_TIMER_QUERY_H_

#include "GLConsts.h"
#include "nsWrapperCache.h"
#include "WebGLObjectModel.h"

namespace mozilla {

class WebGLTimerQuery final
  : public nsWrapperCache
  , public WebGLRefCountedObject<WebGLTimerQuery>
  , public WebGLContextBoundObject
{
public:
  static WebGLTimerQuery* Create(WebGLContext* webgl);

  void Delete();

  bool HasEverBeenBound() const { return mTarget != LOCAL_GL_NONE; }
  GLenum Target() const { return mTarget; }

  WebGLContext* GetParentObject() const;

  // NS
  virtual JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override;

  const GLenum mGLName;

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLTimerQuery)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLTimerQuery)

private:
  explicit WebGLTimerQuery(WebGLContext* webgl, GLuint aName);
  ~WebGLTimerQuery();

  GLenum mTarget;

  friend class WebGLExtensionDisjointTimerQuery;
};

} // namespace mozilla

#endif // WEBGL_TIMER_QUERY_H_
