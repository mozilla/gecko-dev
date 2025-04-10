/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_PipelineError_H_
#define GPU_PipelineError_H_

#include "Error.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/WebGPUBinding.h"
#include "nsIGlobalObject.h"

namespace mozilla::webgpu {

class PipelineError final : public dom::DOMException {
 public:
  GPU_DECL_JS_WRAP(PipelineError)

  explicit PipelineError(const nsACString& aMessage,
                         dom::GPUPipelineErrorReason aReason);

  ~PipelineError() override = default;

  static already_AddRefed<PipelineError> Constructor(
      const dom::GlobalObject& global, const nsAString& message,
      const dom::GPUPipelineErrorInit& options);

  dom::GPUPipelineErrorReason Reason() const;

 private:
  dom::GPUPipelineErrorReason mReason;
};

}  // namespace mozilla::webgpu

#endif  // GPU_PipelineError_H_
