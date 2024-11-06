/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Device.h"
#include "QuerySet.h"
#include "ipc/WebGPUChild.h"
#include "mozilla/dom/WebGPUBinding.h"

namespace mozilla::webgpu {

QuerySet::~QuerySet() { Cleanup(); }

void QuerySet::Cleanup() {
  if (!mValid) {
    return;
  }
  mValid = false;

  auto bridge = mParent->GetBridge();
  if (!bridge) {
    return;
  }

  if (bridge->CanSend()) {
    bridge->SendQuerySetDrop(mId);
  }

  wgpu_client_free_query_set_id(bridge->GetClient(), mId);
}

GPU_IMPL_CYCLE_COLLECTION(QuerySet, mParent)
GPU_IMPL_JS_WRAP(QuerySet)

QuerySet::QuerySet(Device* const aParent,
                   const dom::GPUQuerySetDescriptor& aDesc, RawId aId)
    : ChildOf(aParent), mId(aId), mType(aDesc.mType), mCount(aDesc.mCount) {}

void QuerySet::Destroy() {
  // TODO: <https://bugzilla.mozilla.org/show_bug.cgi?id=1929168>
}

dom::GPUQueryType QuerySet::Type() const { return mType; }

uint32_t QuerySet::Count() const { return mCount; }

}  // namespace mozilla::webgpu
