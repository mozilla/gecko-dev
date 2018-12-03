/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GFX_VR_PROCESS_MANAGER_H
#define GFX_VR_PROCESS_MANAGER_H

namespace mozilla {
namespace gfx {

class VRProcessParent;
class VRManagerChild;
class PVRGPUChild;
class VRChild;

// The VRProcessManager is a singleton responsible for creating VR-bound
// objects that may live in another process.
class VRProcessManager final {
 public:
  static VRProcessManager* Get();
  static void Initialize();
  static void Shutdown();

  ~VRProcessManager();

  // If not using a VR process, launch a new VR process asynchronously.
  void LaunchVRProcess();
  void DestroyProcess();

  bool CreateGPUBridges(base::ProcessId aOtherProcess,
                        mozilla::ipc::Endpoint<PVRGPUChild>* aOutVRBridge);

  VRChild* GetVRChild();

 private:
  VRProcessManager();

  DISALLOW_COPY_AND_ASSIGN(VRProcessManager);

  bool CreateGPUVRManager(base::ProcessId aOtherProcess,
                          mozilla::ipc::Endpoint<PVRGPUChild>* aOutEndpoint);
  void OnXPCOMShutdown();
  void CleanShutdown();

  // Permanently disable the VR process and record a message why.
  void DisableVRProcess(const char* aMessage);

  class Observer final : public nsIObserver {
   public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
    explicit Observer(VRProcessManager* aManager);

   protected:
    ~Observer() {}

    VRProcessManager* mManager;
  };
  friend class Observer;

  RefPtr<Observer> mObserver;
  VRProcessParent* mProcess;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // GFX_VR_PROCESS_MANAGER_H
