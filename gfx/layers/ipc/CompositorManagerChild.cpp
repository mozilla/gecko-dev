/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/CompositorManagerChild.h"

#include "mozilla/StaticPrefs_layers.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/CompositorManagerParent.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/gfx/CanvasShutdownManager.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/dom/ContentChild.h"  // for ContentChild
#include "mozilla/dom/BrowserChild.h"  // for BrowserChild
#include "mozilla/ipc/Endpoint.h"
#include "VsyncSource.h"

namespace mozilla {
namespace layers {

using gfx::GPUProcessManager;

StaticRefPtr<CompositorManagerChild> CompositorManagerChild::sInstance;

static StaticMutex sCompositorProcInfoMutex;
static ipc::EndpointProcInfo sCompositorProcInfo
    MOZ_GUARDED_BY(sCompositorProcInfoMutex);

static void SetCompositorProcInfo(ipc::EndpointProcInfo aInfo) {
  StaticMutexAutoLock lock(sCompositorProcInfoMutex);
  sCompositorProcInfo = aInfo;
}

/* static */
ipc::EndpointProcInfo CompositorManagerChild::GetCompositorProcInfo() {
  StaticMutexAutoLock lock(sCompositorProcInfoMutex);
  return sCompositorProcInfo;
}

/* static */
bool CompositorManagerChild::IsInitialized(uint64_t aProcessToken) {
  MOZ_ASSERT(NS_IsMainThread());
  return sInstance && sInstance->CanSend() &&
         sInstance->mProcessToken == aProcessToken;
}

/* static */
void CompositorManagerChild::InitSameProcess(uint32_t aNamespace,
                                             uint64_t aProcessToken) {
  MOZ_ASSERT(NS_IsMainThread());
  if (NS_WARN_IF(IsInitialized(aProcessToken))) {
    MOZ_ASSERT_UNREACHABLE("Already initialized same process");
    return;
  }

  RefPtr<CompositorManagerParent> parent =
      CompositorManagerParent::CreateSameProcess(aNamespace);
  RefPtr<CompositorManagerChild> child = new CompositorManagerChild(
      aProcessToken, aNamespace, /* aSameProcess */ true);
  child->SetOtherEndpointProcInfo(ipc::EndpointProcInfo::Current());
  if (NS_WARN_IF(!child->Open(parent, CompositorThread(), ipc::ChildSide))) {
    MOZ_DIAGNOSTIC_CRASH("Failed to open same process protocol");
    return;
  }
  child->mCanSend = true;
  child->SetReplyTimeout();

  parent->BindComplete(/* aIsRoot */ true);
  sInstance = std::move(child);
  SetCompositorProcInfo(sInstance->OtherEndpointProcInfo());
}

/* static */
bool CompositorManagerChild::Init(Endpoint<PCompositorManagerChild>&& aEndpoint,
                                  uint32_t aNamespace,
                                  uint64_t aProcessToken /* = 0 */) {
  MOZ_ASSERT(NS_IsMainThread());
  if (sInstance) {
    MOZ_ASSERT(sInstance->mNamespace != aNamespace);
  }

  RefPtr<CompositorManagerChild> child =
      new CompositorManagerChild(aProcessToken, aNamespace,
                                 /* aSameProcess */ false);
  if (NS_WARN_IF(!aEndpoint.Bind(child))) {
    return false;
  }
  child->mCanSend = true;
  child->SetReplyTimeout();

  sInstance = std::move(child);
  SetCompositorProcInfo(sInstance->OtherEndpointProcInfo());

  // If there are any canvases waiting on the recreation of the GPUProcess or
  // CompositorManagerChild, then we need to notify them so that they can
  // restore their contexts.
  gfx::CanvasShutdownManager::OnCompositorManagerRestored();
  return true;
}

/* static */
void CompositorManagerChild::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  CompositorBridgeChild::ShutDown();

  if (!sInstance) {
    return;
  }

  sInstance->Close();
  sInstance = nullptr;
  SetCompositorProcInfo(ipc::EndpointProcInfo::Invalid());
}

/* static */
void CompositorManagerChild::OnGPUProcessLost(uint64_t aProcessToken) {
  MOZ_ASSERT(NS_IsMainThread());

  // Since GPUChild and CompositorManagerChild will race on ActorDestroy, we
  // cannot know if the CompositorManagerChild is about to be released but has
  // yet to be. As such, we want to pre-emptively set mCanSend to false.
  if (sInstance && sInstance->mProcessToken == aProcessToken) {
    sInstance->mCanSend = false;
    SetCompositorProcInfo(ipc::EndpointProcInfo::Invalid());
  }
}

/* static */
bool CompositorManagerChild::CreateContentCompositorBridge(
    uint32_t aNamespace) {
  MOZ_ASSERT(NS_IsMainThread());
  if (NS_WARN_IF(!sInstance || !sInstance->CanSend())) {
    return false;
  }

  CompositorBridgeOptions options = ContentCompositorOptions();

  RefPtr<CompositorBridgeChild> bridge = new CompositorBridgeChild(sInstance);
  if (NS_WARN_IF(
          !sInstance->SendPCompositorBridgeConstructor(bridge, options))) {
    return false;
  }

  bridge->InitForContent(aNamespace);
  return true;
}

/* static */
already_AddRefed<CompositorBridgeChild>
CompositorManagerChild::CreateWidgetCompositorBridge(
    uint64_t aProcessToken, WebRenderLayerManager* aLayerManager,
    uint32_t aNamespace, CSSToLayoutDeviceScale aScale,
    const CompositorOptions& aOptions, bool aUseExternalSurfaceSize,
    const gfx::IntSize& aSurfaceSize, uint64_t aInnerWindowId) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  if (NS_WARN_IF(!sInstance || !sInstance->CanSend())) {
    return nullptr;
  }

  TimeDuration vsyncRate =
      gfxPlatform::GetPlatform()->GetGlobalVsyncDispatcher()->GetVsyncRate();

  CompositorBridgeOptions options = WidgetCompositorOptions(
      aScale, vsyncRate, aOptions, aUseExternalSurfaceSize, aSurfaceSize,
      aInnerWindowId);

  RefPtr<CompositorBridgeChild> bridge = new CompositorBridgeChild(sInstance);
  if (NS_WARN_IF(
          !sInstance->SendPCompositorBridgeConstructor(bridge, options))) {
    return nullptr;
  }

  bridge->InitForWidget(aProcessToken, aLayerManager, aNamespace);
  return bridge.forget();
}

/* static */
already_AddRefed<CompositorBridgeChild>
CompositorManagerChild::CreateSameProcessWidgetCompositorBridge(
    WebRenderLayerManager* aLayerManager, uint32_t aNamespace) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  if (NS_WARN_IF(!sInstance || !sInstance->CanSend())) {
    return nullptr;
  }

  CompositorBridgeOptions options = SameProcessWidgetCompositorOptions();

  RefPtr<CompositorBridgeChild> bridge = new CompositorBridgeChild(sInstance);
  if (NS_WARN_IF(
          !sInstance->SendPCompositorBridgeConstructor(bridge, options))) {
    return nullptr;
  }

  bridge->InitForWidget(1, aLayerManager, aNamespace);
  return bridge.forget();
}

CompositorManagerChild::CompositorManagerChild(uint64_t aProcessToken,
                                               uint32_t aNamespace,
                                               bool aSameProcess)
    : mProcessToken(aProcessToken),
      mNamespace(aNamespace),
      mResourceId(0),
      mCanSend(false),
      mSameProcess(aSameProcess),
      mFwdTransactionCounter(this) {}

void CompositorManagerChild::ActorDestroy(ActorDestroyReason aReason) {
  mCanSend = false;
  if (sInstance == this) {
    sInstance = nullptr;
  }
}

void CompositorManagerChild::HandleFatalError(const char* aMsg) {
  dom::ContentChild::FatalErrorIfNotUsingGPUProcess(aMsg, OtherChildID());
}

void CompositorManagerChild::ProcessingError(Result aCode,
                                             const char* aReason) {
  if (aCode != MsgDropped) {
    gfxDevCrash(gfx::LogReason::ProcessingError)
        << "Processing error in CompositorBridgeChild: " << int(aCode);
  }
}

void CompositorManagerChild::SetReplyTimeout() {
#ifndef DEBUG
  // Add a timeout for release builds to kill GPU process when it hangs.
  if (XRE_IsParentProcess() && GPUProcessManager::Get()->GetGPUChild()) {
    int32_t timeout =
        StaticPrefs::layers_gpu_process_ipc_reply_timeout_ms_AtStartup();
    SetReplyTimeoutMs(timeout);
  }
#endif
}

bool CompositorManagerChild::ShouldContinueFromReplyTimeout() {
  MOZ_ASSERT_IF(mSyncIPCStartTimeStamp.isSome(), XRE_IsParentProcess());

  if (XRE_IsParentProcess()) {
#ifndef DEBUG
    // Extend sync IPC reply timeout
    if (mSyncIPCStartTimeStamp.isSome()) {
      const int32_t maxDurationMs = StaticPrefs::
          layers_gpu_process_extend_ipc_reply_timeout_ms_AtStartup();
      const auto now = TimeStamp::Now();
      const auto durationMs = static_cast<int32_t>(
          (now - mSyncIPCStartTimeStamp.ref()).ToMilliseconds());

      if (durationMs < maxDurationMs) {
        return true;
      }
    }
#endif
    gfxCriticalNote << "Killing GPU process due to IPC reply timeout";
    MOZ_DIAGNOSTIC_ASSERT(GPUProcessManager::Get()->GetGPUChild());
    GPUProcessManager::Get()->KillProcess(/* aGenerateMinidump */ true);
  }
  return false;
}

mozilla::ipc::IPCResult CompositorManagerChild::RecvNotifyWebRenderError(
    const WebRenderError&& aError) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  GPUProcessManager::Get()->NotifyWebRenderError(aError);
  return IPC_OK();
}

void CompositorManagerChild::SetSyncIPCStartTimeStamp() {
  MOZ_ASSERT(mSyncIPCStartTimeStamp.isNothing());

  mSyncIPCStartTimeStamp = Some(TimeStamp::Now());
}

void CompositorManagerChild::ClearSyncIPCStartTimeStamp() {
  mSyncIPCStartTimeStamp = Nothing();
}

}  // namespace layers
}  // namespace mozilla
