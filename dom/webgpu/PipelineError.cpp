/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PipelineError.h"
#include "mozilla/RefPtr.h"
#include "nsIGlobalObject.h"

namespace mozilla::webgpu {

GPU_IMPL_JS_WRAP(PipelineError)

PipelineError::PipelineError(const nsACString& aMessage,
                             dom::GPUPipelineErrorReason aReason)
    : dom::DOMException(nsresult::NS_OK, aMessage, "GPUPipelineError"_ns, 0),
      mReason(aReason) {}

/*static*/ already_AddRefed<PipelineError> PipelineError::Constructor(
    const dom::GlobalObject& aGlobal, const nsAString& aMessage,
    const dom::GPUPipelineErrorInit& aOptions) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  MOZ_RELEASE_ASSERT(global);

  auto reason = aOptions.mReason;
  NS_ConvertUTF16toUTF8 msg(aMessage);
  auto error = MakeRefPtr<PipelineError>(msg, reason);
  return error.forget();
}

dom::GPUPipelineErrorReason PipelineError::Reason() const { return mReason; }

}  // namespace mozilla::webgpu
