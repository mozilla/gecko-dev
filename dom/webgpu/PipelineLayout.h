/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGPU_PipelineLayout_H_
#define WEBGPU_PipelineLayout_H_

#include "nsWrapperCache.h"
#include "ObjectModel.h"

namespace mozilla {
namespace webgpu {

class Device;

class PipelineLayout final : public ChildOf<Device> {
 public:
  WEBGPU_DECL_GOOP(PipelineLayout)

 private:
  PipelineLayout() = delete;
  virtual ~PipelineLayout();
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // WEBGPU_PipelineLayout_H_
