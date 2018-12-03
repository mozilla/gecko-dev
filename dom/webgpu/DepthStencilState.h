/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGPU_DepthStencilState_H_
#define WEBGPU_DepthStencilState_H_

#include "nsWrapperCache.h"
#include "ObjectModel.h"

namespace mozilla {
namespace webgpu {

class Device;

class DepthStencilState final : public ChildOf<Device> {
 public:
  WEBGPU_DECL_GOOP(DepthStencilState)

 private:
  DepthStencilState() = delete;
  virtual ~DepthStencilState();
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // WEBGPU_DepthStencilState_H_
