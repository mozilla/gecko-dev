/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"

namespace mozilla {

WebGLExtensionCompressedTextureETC1::WebGLExtensionCompressedTextureETC1(WebGLContext* webgl)
    : WebGLExtensionBase(webgl)
{
    webgl->mCompressedTextureFormats.AppendElement(LOCAL_GL_ETC1_RGB8_OES);
}

WebGLExtensionCompressedTextureETC1::~WebGLExtensionCompressedTextureETC1()
{
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionCompressedTextureETC1, WEBGL_compressed_texture_etc1)

} // namespace mozilla
