/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_QuerySet_H_
#define GPU_QuerySet_H_

#include "ObjectModel.h"
#include "mozilla/webgpu/WebGPUTypes.h"

namespace mozilla {

namespace dom {
struct GPUQuerySetDescriptor;
enum class GPUQueryType : uint8_t;
}  // namespace dom

namespace webgpu {

class Device;

class QuerySet final : public ObjectBase, public ChildOf<Device> {
 public:
  GPU_DECL_CYCLE_COLLECTION(QuerySet)
  GPU_DECL_JS_WRAP(QuerySet)

  QuerySet() = delete;
  QuerySet(Device* const aParent, const dom::GPUQuerySetDescriptor& aDesc,
           RawId aId);

  void Destroy();

  dom::GPUQueryType Type() const;
  uint32_t Count() const;

  const RawId mId;

 private:
  virtual ~QuerySet();
  void Cleanup();

  dom::GPUQueryType mType;
  uint32_t mCount;
};

}  // namespace webgpu

}  // namespace mozilla

#endif  // GPU_QuerySet_H_
