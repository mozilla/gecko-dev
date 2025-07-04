/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureView.h"

#include "Device.h"
#include "mozilla/dom/WebGPUBinding.h"
#include "mozilla/webgpu/CanvasContext.h"
#include "ipc/WebGPUChild.h"

namespace mozilla::webgpu {

GPU_IMPL_CYCLE_COLLECTION(TextureView, mParent)
GPU_IMPL_JS_WRAP(TextureView)

TextureView::TextureView(Texture* const aParent, RawId aId)
    : ChildOf(aParent), mId(aId) {
  MOZ_RELEASE_ASSERT(aId);
}

TextureView::~TextureView() { Cleanup(); }

WeakPtr<CanvasContext> TextureView::GetTargetContext() const {
  return mParent->mTargetContext;
}  // namespace webgpu

void TextureView::Cleanup() {
  if (!mValid || !mParent || !mParent->GetDevice()) {
    return;
  }
  mValid = false;

  auto bridge = mParent->GetDevice()->GetBridge();
  if (!bridge) {
    return;
  }

  if (bridge->CanSend()) {
    bridge->SendTextureViewDrop(mId);
  }

  wgpu_client_free_texture_view_id(bridge->GetClient(), mId);
}

}  // namespace mozilla::webgpu
