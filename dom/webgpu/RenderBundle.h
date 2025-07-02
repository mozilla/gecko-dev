/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_RenderBundle_H_
#define GPU_RenderBundle_H_

#include "nsWrapperCache.h"
#include "CanvasContext.h"
#include "ObjectModel.h"

namespace mozilla::webgpu {

class Device;

class RenderBundle final : public ObjectBase, public ChildOf<Device> {
 public:
  GPU_DECL_CYCLE_COLLECTION(RenderBundle)
  GPU_DECL_JS_WRAP(RenderBundle)

  RenderBundle(Device* const aParent, RawId aId,
               CanvasContextArray&& aCanvasContexts);

  const RawId mId;

  mozilla::Span<const WeakPtr<CanvasContext>> GetCanvasContexts() const {
    return mUsedCanvasContexts;
  }

 private:
  virtual ~RenderBundle();
  void Cleanup();

  // The canvas contexts of any canvas textures used in this render bundle.
  CanvasContextArray mUsedCanvasContexts;
};

}  // namespace mozilla::webgpu

#endif  // GPU_RenderBundle_H_
