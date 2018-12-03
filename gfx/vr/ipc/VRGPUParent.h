/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_GPU_PARENT_H
#define GFX_VR_GPU_PARENT_H

#include "mozilla/gfx/PVRGPUParent.h"

namespace mozilla {
namespace gfx {

class VRGPUParent final : public PVRGPUParent {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VRGPUParent)

 public:
  explicit VRGPUParent(ProcessId aChildProcessId);

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  static RefPtr<VRGPUParent> CreateForGPU(Endpoint<PVRGPUParent>&& aEndpoint);

 protected:
  ~VRGPUParent() {}

  void Bind(Endpoint<PVRGPUParent>&& aEndpoint);
  virtual mozilla::ipc::IPCResult RecvStartVRService() override;
  virtual mozilla::ipc::IPCResult RecvStopVRService() override;

 private:
  void DeferredDestroy();

  RefPtr<VRGPUParent> mSelfRef;
#if !defined(MOZ_WIDGET_ANDROID)
  RefPtr<VRService> mVRService;
#endif
};

}  // namespace gfx
}  // namespace mozilla

#endif  // GFX_VR_CONTENT_PARENT_H
