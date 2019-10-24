/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "UiCompositorControllerParent.h"

#if defined(MOZ_WIDGET_ANDROID)
#  include "apz/src/APZCTreeManager.h"
#  include "mozilla/layers/AsyncCompositionManager.h"
#endif
#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/UiCompositorControllerMessageTypes.h"
#include "mozilla/layers/WebRenderBridgeParent.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/Move.h"
#include "mozilla/Unused.h"

#include "FrameMetrics.h"
#include "SynchronousTask.h"

namespace mozilla {
namespace layers {

typedef CompositorBridgeParent::LayerTreeState LayerTreeState;

/* static */
RefPtr<UiCompositorControllerParent>
UiCompositorControllerParent::GetFromRootLayerTreeId(
    const LayersId& aRootLayerTreeId) {
  RefPtr<UiCompositorControllerParent> controller;
  CompositorBridgeParent::CallWithIndirectShadowTree(
      aRootLayerTreeId, [&](LayerTreeState& aState) -> void {
        controller = aState.mUiControllerParent;
      });
  return controller;
}

/* static */
RefPtr<UiCompositorControllerParent> UiCompositorControllerParent::Start(
    const LayersId& aRootLayerTreeId,
    Endpoint<PUiCompositorControllerParent>&& aEndpoint) {
  RefPtr<UiCompositorControllerParent> parent =
      new UiCompositorControllerParent(aRootLayerTreeId);

  RefPtr<Runnable> task =
      NewRunnableMethod<Endpoint<PUiCompositorControllerParent>&&>(
          "layers::UiCompositorControllerParent::Open", parent,
          &UiCompositorControllerParent::Open, std::move(aEndpoint));
  CompositorThreadHolder::Loop()->PostTask(task.forget());

  return parent;
}

mozilla::ipc::IPCResult UiCompositorControllerParent::RecvPause() {
  CompositorBridgeParent* parent =
      CompositorBridgeParent::GetCompositorBridgeParentFromLayersId(
          mRootLayerTreeId);
  if (parent) {
    parent->PauseComposition();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult UiCompositorControllerParent::RecvResume() {
  CompositorBridgeParent* parent =
      CompositorBridgeParent::GetCompositorBridgeParentFromLayersId(
          mRootLayerTreeId);
  if (parent) {
    parent->ResumeComposition();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult UiCompositorControllerParent::RecvResumeAndResize(
    const int32_t& aX, const int32_t& aY, const int32_t& aWidth,
    const int32_t& aHeight) {
  CompositorBridgeParent* parent =
      CompositorBridgeParent::GetCompositorBridgeParentFromLayersId(
          mRootLayerTreeId);
  if (parent) {
    // Front-end expects a first paint callback upon resume/resize.
    parent->ForceIsFirstPaint();
    parent->ResumeCompositionAndResize(aX, aY, aWidth, aHeight);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
UiCompositorControllerParent::RecvInvalidateAndRender() {
  CompositorBridgeParent* parent =
      CompositorBridgeParent::GetCompositorBridgeParentFromLayersId(
          mRootLayerTreeId);
  if (parent) {
    parent->Invalidate();
    parent->ScheduleComposition();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult UiCompositorControllerParent::RecvMaxToolbarHeight(
    const int32_t& aHeight) {
  mMaxToolbarHeight = aHeight;
#if defined(MOZ_WIDGET_ANDROID)
  if (mAnimator) {
    mAnimator->SetMaxToolbarHeight(mMaxToolbarHeight);
  }
#endif  // defined(MOZ_WIDGET_ANDROID)

  return IPC_OK();
}

mozilla::ipc::IPCResult UiCompositorControllerParent::RecvFixedBottomOffset(
    const int32_t& aOffset) {
#if defined(MOZ_WIDGET_ANDROID)
  CompositorBridgeParent* parent =
      CompositorBridgeParent::GetCompositorBridgeParentFromLayersId(
          mRootLayerTreeId);
  if (parent) {
    parent->SetFixedLayerMargins(0, aOffset);
  }
#endif  // defined(MOZ_WIDGET_ANDROID)

  return IPC_OK();
}

mozilla::ipc::IPCResult UiCompositorControllerParent::RecvPinned(
    const bool& aPinned, const int32_t& aReason) {
#if defined(MOZ_WIDGET_ANDROID)
  if (mAnimator) {
    mAnimator->SetPinned(aPinned, aReason);
  }
#endif  // defined(MOZ_WIDGET_ANDROID)

  return IPC_OK();
}

mozilla::ipc::IPCResult
UiCompositorControllerParent::RecvToolbarAnimatorMessageFromUI(
    const int32_t& aMessage) {
#if defined(MOZ_WIDGET_ANDROID)
  if (mAnimator) {
    mAnimator->ToolbarAnimatorMessageFromUI(aMessage);
  }
#endif  // defined(MOZ_WIDGET_ANDROID)

  return IPC_OK();
}

mozilla::ipc::IPCResult UiCompositorControllerParent::RecvDefaultClearColor(
    const uint32_t& aColor) {
  LayerTreeState* state =
      CompositorBridgeParent::GetIndirectShadowTree(mRootLayerTreeId);

  if (state && state->mLayerManager) {
    Compositor* compositor = state->mLayerManager->GetCompositor();
    if (compositor) {
      // Android Color is ARGB which is apparently unusual.
      compositor->SetDefaultClearColor(gfx::Color::UnusualFromARGB(aColor));
    }
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
UiCompositorControllerParent::RecvRequestScreenPixels() {
#if defined(MOZ_WIDGET_ANDROID)
  LayerTreeState* state =
      CompositorBridgeParent::GetIndirectShadowTree(mRootLayerTreeId);

  if (state && state->mLayerManager && state->mParent) {
    state->mLayerManager->RequestScreenPixels(this);
    state->mParent->Invalidate();
    state->mParent->ScheduleComposition();
  } else if (state && state->mWrBridge) {
    state->mWrBridge->RequestScreenPixels(this);
    state->mWrBridge->ScheduleForcedGenerateFrame();
  }
#endif  // defined(MOZ_WIDGET_ANDROID)

  return IPC_OK();
}

mozilla::ipc::IPCResult
UiCompositorControllerParent::RecvEnableLayerUpdateNotifications(
    const bool& aEnable) {
#if defined(MOZ_WIDGET_ANDROID)
  // Layers updates are need by Robocop test which enables them
  mCompositorLayersUpdateEnabled = aEnable;
#endif  // defined(MOZ_WIDGET_ANDROID)

  return IPC_OK();
}

mozilla::ipc::IPCResult
UiCompositorControllerParent::RecvToolbarPixelsToCompositor(
    Shmem&& aMem, const ScreenIntSize& aSize) {
#if defined(MOZ_WIDGET_ANDROID)
  if (mAnimator) {
    // By adopting the Shmem, the animator is responsible for deallocating.
    mAnimator->AdoptToolbarPixels(std::move(aMem), aSize);
  } else {
    DeallocShmem(aMem);
  }
#endif  // defined(MOZ_WIDGET_ANDROID)

  return IPC_OK();
}

void UiCompositorControllerParent::ActorDestroy(ActorDestroyReason aWhy) {}

void UiCompositorControllerParent::ActorDealloc() {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  Shutdown();
  Release();  // For AddRef in Initialize()
}

#if defined(MOZ_WIDGET_ANDROID)
void UiCompositorControllerParent::RegisterAndroidDynamicToolbarAnimator(
    AndroidDynamicToolbarAnimator* aAnimator) {
  MOZ_ASSERT(!mAnimator);
  mAnimator = aAnimator;
  if (mAnimator) {
    mAnimator->SetMaxToolbarHeight(mMaxToolbarHeight);
  }
}
#endif  // defined(MOZ_WIDGET_ANDROID)

void UiCompositorControllerParent::ToolbarAnimatorMessageFromCompositor(
    int32_t aMessage) {
  // This function can be call from ether compositor or controller thread.
  if (!CompositorThreadHolder::IsInCompositorThread()) {
    CompositorThreadHolder::Loop()->PostTask(NewRunnableMethod<int32_t>(
        "layers::UiCompositorControllerParent::"
        "ToolbarAnimatorMessageFromCompositor",
        this,
        &UiCompositorControllerParent::ToolbarAnimatorMessageFromCompositor,
        aMessage));
    return;
  }

  Unused << SendToolbarAnimatorMessageFromCompositor(aMessage);
}

bool UiCompositorControllerParent::AllocPixelBuffer(const int32_t aSize,
                                                    ipc::Shmem* aMem) {
  MOZ_ASSERT(aSize > 0);
  return AllocShmem(aSize, ipc::SharedMemory::TYPE_BASIC, aMem);
}

void UiCompositorControllerParent::NotifyLayersUpdated() {
#ifdef MOZ_WIDGET_ANDROID
  if (mCompositorLayersUpdateEnabled) {
    ToolbarAnimatorMessageFromCompositor(LAYERS_UPDATED);
  }
#endif
}

void UiCompositorControllerParent::NotifyFirstPaint() {
  ToolbarAnimatorMessageFromCompositor(FIRST_PAINT);
}

void UiCompositorControllerParent::NotifyUpdateScreenMetrics(
    const FrameMetrics& aMetrics) {
#if defined(MOZ_WIDGET_ANDROID)
  CSSToScreenScale scale = ViewTargetAs<ScreenPixel>(
      aMetrics.GetZoom().ToScaleFactor(),
      PixelCastJustification::ScreenIsParentLayerForRoot);
  ScreenPoint scrollOffset = aMetrics.GetScrollOffset() * scale;
  CompositorThreadHolder::Loop()->PostTask(
      NewRunnableMethod<ScreenPoint, CSSToScreenScale>(
          "UiCompositorControllerParent::SendRootFrameMetrics", this,
          &UiCompositorControllerParent::SendRootFrameMetrics, scrollOffset,
          scale));
#endif
}

UiCompositorControllerParent::UiCompositorControllerParent(
    const LayersId& aRootLayerTreeId)
    : mRootLayerTreeId(aRootLayerTreeId)
#ifdef MOZ_WIDGET_ANDROID
      ,
      mCompositorLayersUpdateEnabled(false)
#endif
      ,
      mMaxToolbarHeight(0) {
  MOZ_COUNT_CTOR(UiCompositorControllerParent);
}

UiCompositorControllerParent::~UiCompositorControllerParent() {
  MOZ_COUNT_DTOR(UiCompositorControllerParent);
}

void UiCompositorControllerParent::InitializeForSameProcess() {
  // This function is called by UiCompositorControllerChild in the main thread.
  // So dispatch to the compositor thread to Initialize.
  if (!CompositorThreadHolder::IsInCompositorThread()) {
    SynchronousTask task(
        "UiCompositorControllerParent::InitializeForSameProcess");

    CompositorThreadHolder::Loop()->PostTask(NS_NewRunnableFunction(
        "UiCompositorControllerParent::InitializeForSameProcess", [&]() {
          AutoCompleteTask complete(&task);
          InitializeForSameProcess();
        }));

    task.Wait();
    return;
  }

  Initialize();
}

void UiCompositorControllerParent::InitializeForOutOfProcess() {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  Initialize();
}

void UiCompositorControllerParent::Initialize() {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  AddRef();
  LayerTreeState* state =
      CompositorBridgeParent::GetIndirectShadowTree(mRootLayerTreeId);
  MOZ_ASSERT(state);
  MOZ_ASSERT(state->mParent);
  if (!state->mParent) {
    return;
  }
  state->mUiControllerParent = this;
#if defined(MOZ_WIDGET_ANDROID)
  AndroidDynamicToolbarAnimator* animator =
      state->mParent->GetAndroidDynamicToolbarAnimator();
  // It is possible the compositor has already started shutting down and
  // the AndroidDynamicToolbarAnimator could be a nullptr. Or this could be
  // non-Fennec in which case the animator is null anyway.
  if (animator) {
    animator->Initialize(mRootLayerTreeId);
  }
#endif
}

void UiCompositorControllerParent::Open(
    Endpoint<PUiCompositorControllerParent>&& aEndpoint) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  if (!aEndpoint.Bind(this)) {
    // We can't recover from this.
    MOZ_CRASH("Failed to bind UiCompositorControllerParent to endpoint");
  }
  InitializeForOutOfProcess();
}

void UiCompositorControllerParent::Shutdown() {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
#if defined(MOZ_WIDGET_ANDROID)
  if (mAnimator) {
    mAnimator->Shutdown();
  }
#endif  // defined(MOZ_WIDGET_ANDROID)
  LayerTreeState* state =
      CompositorBridgeParent::GetIndirectShadowTree(mRootLayerTreeId);
  if (state) {
    state->mUiControllerParent = nullptr;
  }
}

}  // namespace layers
}  // namespace mozilla
