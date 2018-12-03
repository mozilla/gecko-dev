/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRParent.h"
#include "VRGPUParent.h"
#include "VRManager.h"
#include "gfxConfig.h"

#include "mozilla/gfx/gfxVars.h"
#include "mozilla/ipc/ProcessChild.h"

#if defined(XP_WIN)
#include "mozilla/gfx/DeviceManagerDx.h"
#endif

namespace mozilla {
namespace gfx {

using mozilla::ipc::IPCResult;

VRParent::VRParent() : mVRGPUParent(nullptr) {}

IPCResult VRParent::RecvNewGPUVRManager(Endpoint<PVRGPUParent>&& aEndpoint) {
  RefPtr<VRGPUParent> vrGPUParent =
      VRGPUParent::CreateForGPU(std::move(aEndpoint));
  if (!vrGPUParent) {
    return IPC_FAIL_NO_REASON(this);
  }

  mVRGPUParent = std::move(vrGPUParent);
  return IPC_OK();
}

IPCResult VRParent::RecvInit(nsTArray<GfxPrefSetting>&& prefs,
                             nsTArray<GfxVarUpdate>&& vars,
                             const DevicePrefs& devicePrefs) {
  const nsTArray<gfxPrefs::Pref*>& globalPrefs = gfxPrefs::all();
  for (auto& setting : prefs) {
    gfxPrefs::Pref* pref = globalPrefs[setting.index()];
    pref->SetCachedValue(setting.value());
  }
  for (const auto& var : vars) {
    gfxVars::ApplyUpdate(var);
  }

  // Inherit device preferences.
  gfxConfig::Inherit(Feature::HW_COMPOSITING, devicePrefs.hwCompositing());
  gfxConfig::Inherit(Feature::D3D11_COMPOSITING,
                     devicePrefs.d3d11Compositing());
  gfxConfig::Inherit(Feature::OPENGL_COMPOSITING, devicePrefs.oglCompositing());
  gfxConfig::Inherit(Feature::ADVANCED_LAYERS, devicePrefs.advancedLayers());
  gfxConfig::Inherit(Feature::DIRECT2D, devicePrefs.useD2D1());

#if defined(XP_WIN)
  if (gfxConfig::IsEnabled(Feature::D3D11_COMPOSITING)) {
    DeviceManagerDx::Get()->CreateCompositorDevices();
  }
#endif
  return IPC_OK();
}

IPCResult VRParent::RecvNotifyVsync(const TimeStamp& vsyncTimestamp) {
  VRManager* vm = VRManager::Get();
  vm->NotifyVsync(vsyncTimestamp);
  return IPC_OK();
}

IPCResult VRParent::RecvUpdatePref(const GfxPrefSetting& setting) {
  gfxPrefs::Pref* pref = gfxPrefs::all()[setting.index()];
  pref->SetCachedValue(setting.value());
  return IPC_OK();
}

IPCResult VRParent::RecvUpdateVar(const GfxVarUpdate& aUpdate) {
  gfxVars::ApplyUpdate(aUpdate);
  return IPC_OK();
}

void VRParent::ActorDestroy(ActorDestroyReason aWhy) {
  if (AbnormalShutdown == aWhy) {
    NS_WARNING("Shutting down VR process early due to a crash!");
    ProcessChild::QuickExit();
  }

  mVRGPUParent->Close();
#if defined(XP_WIN)
  DeviceManagerDx::Shutdown();
#endif
  gfxVars::Shutdown();
  gfxConfig::Shutdown();
  gfxPrefs::DestroySingleton();
  XRE_ShutdownChildProcess();
}

bool VRParent::Init(base::ProcessId aParentPid, const char* aParentBuildID,
                    MessageLoop* aIOLoop, IPC::Channel* aChannel) {
  // Initialize the thread manager before starting IPC. Otherwise, messages
  // may be posted to the main thread and we won't be able to process them.
  if (NS_WARN_IF(NS_FAILED(nsThreadManager::get().Init()))) {
    return false;
  }

  // Now it's safe to start IPC.
  if (NS_WARN_IF(!Open(aChannel, aParentPid, aIOLoop))) {
    return false;
  }

  // This must be checked before any IPDL message, which may hit sentinel
  // errors due to parent and content processes having different
  // versions.
  MessageChannel* channel = GetIPCChannel();
  if (channel && !channel->SendBuildIDsMatchMessage(aParentBuildID)) {
    // We need to quit this process if the buildID doesn't match the parent's.
    // This can occur when an update occurred in the background.
    ProcessChild::QuickExit();
  }

  // Ensure gfxPrefs are initialized.
  gfxPrefs::GetSingleton();
  gfxConfig::Init();
  gfxVars::Initialize();
#if defined(XP_WIN)
  DeviceManagerDx::Init();
#endif
  if (NS_FAILED(NS_InitMinimalXPCOM())) {
    return false;
  }

  return true;
}

}  // namespace gfx
}  // namespace mozilla