/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"

namespace mozilla {

WebGLExtensionFragDepth::WebGLExtensionFragDepth(WebGLContext* webgl)
    : WebGLExtensionBase(webgl)
{
    MOZ_ASSERT(IsSupported(webgl), "Don't construct extension if unsupported.");
}

WebGLExtensionFragDepth::~WebGLExtensionFragDepth()
{
}

bool
WebGLExtensionFragDepth::IsSupported(const WebGLContext* webgl)
{
    gl::GLContext* gl = webgl->GL();
    return gl->IsSupported(gl::GLFeature::frag_depth);
}


IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionFragDepth, EXT_frag_depth)

} // namespace mozilla
