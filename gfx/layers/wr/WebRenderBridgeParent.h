/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_WebRenderBridgeParent_h
#define mozilla_layers_WebRenderBridgeParent_h

#include <unordered_map>
#include <unordered_set>

#include "CompositableHost.h"           // for CompositableHost, ImageCompositeNotificationInfo
#include "GLContextProvider.h"
#include "mozilla/layers/CompositableTransactionParent.h"
#include "mozilla/layers/CompositorVsyncSchedulerOwner.h"
#include "mozilla/layers/PWebRenderBridgeParent.h"
#include "mozilla/layers/UiCompositorControllerParent.h"
#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "nsTArrayForwardDeclare.h"

namespace mozilla {

namespace gl {
class GLContext;
}

namespace widget {
class CompositorWidget;
}

namespace wr {
class WebRenderAPI;
}

namespace layers {

class Compositor;
class CompositorAnimationStorage;
class CompositorBridgeParentBase;
class CompositorVsyncScheduler;
class AsyncImagePipelineManager;
class WebRenderImageHost;

class WebRenderBridgeParent final : public PWebRenderBridgeParent
                                  , public CompositorVsyncSchedulerOwner
                                  , public CompositableParentManager
                                  , public layers::FrameRecorder
{
public:
  WebRenderBridgeParent(CompositorBridgeParentBase* aCompositorBridge,
                        const wr::PipelineId& aPipelineId,
                        widget::CompositorWidget* aWidget,
                        CompositorVsyncScheduler* aScheduler,
                        RefPtr<wr::WebRenderAPI>&& aApi,
                        RefPtr<AsyncImagePipelineManager>&& aImageMgr,
                        RefPtr<CompositorAnimationStorage>&& aAnimStorage,
                        TimeDuration aVsyncRate);

  static WebRenderBridgeParent* CreateDestroyed(const wr::PipelineId& aPipelineId);

  wr::PipelineId PipelineId() { return mPipelineId; }
  already_AddRefed<wr::WebRenderAPI> GetWebRenderAPI() { return do_AddRef(mApi); }
  AsyncImagePipelineManager* AsyncImageManager() { return mAsyncImageManager; }
  CompositorVsyncScheduler* CompositorScheduler() { return mCompositorScheduler.get(); }

  mozilla::ipc::IPCResult RecvEnsureConnected(TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                              MaybeIdNamespace* aMaybeIdNamespace) override;

  mozilla::ipc::IPCResult RecvNewCompositable(const CompositableHandle& aHandle,
                                              const TextureInfo& aInfo) override;
  mozilla::ipc::IPCResult RecvReleaseCompositable(const CompositableHandle& aHandle) override;

  mozilla::ipc::IPCResult RecvShutdown() override;
  mozilla::ipc::IPCResult RecvShutdownSync() override;
  mozilla::ipc::IPCResult RecvDeleteCompositorAnimations(InfallibleTArray<uint64_t>&& aIds) override;
  mozilla::ipc::IPCResult RecvUpdateResources(nsTArray<OpUpdateResource>&& aUpdates,
                                              nsTArray<RefCountedShmem>&& aSmallShmems,
                                              nsTArray<ipc::Shmem>&& aLargeShmems,
                                              const bool& aScheduleComposite) override;
  mozilla::ipc::IPCResult RecvSetDisplayList(const gfx::IntSize& aSize,
                                             InfallibleTArray<WebRenderParentCommand>&& aCommands,
                                             InfallibleTArray<OpDestroy>&& aToDestroy,
                                             const uint64_t& aFwdTransactionId,
                                             const TransactionId& aTransactionId,
                                             const wr::LayoutSize& aContentSize,
                                             ipc::ByteBuf&& dl,
                                             const wr::BuiltDisplayListDescriptor& dlDesc,
                                             const WebRenderScrollData& aScrollData,
                                             nsTArray<OpUpdateResource>&& aResourceUpdates,
                                             nsTArray<RefCountedShmem>&& aSmallShmems,
                                             nsTArray<ipc::Shmem>&& aLargeShmems,
                                             const wr::IdNamespace& aIdNamespace,
                                             const bool& aContainsSVGGroup,
                                             const TimeStamp& aRefreshStartTime,
                                             const TimeStamp& aTxnStartTime,
                                             const TimeStamp& aFwdTime) override;
  mozilla::ipc::IPCResult RecvEmptyTransaction(const FocusTarget& aFocusTarget,
                                               const ScrollUpdatesMap& aUpdates,
                                               const uint32_t& aPaintSequenceNumber,
                                               InfallibleTArray<WebRenderParentCommand>&& aCommands,
                                               InfallibleTArray<OpDestroy>&& aToDestroy,
                                               const uint64_t& aFwdTransactionId,
                                               const TransactionId& aTransactionId,
                                               nsTArray<OpUpdateResource>&& aResourceUpdates,
                                               nsTArray<RefCountedShmem>&& aSmallShmems,
                                               nsTArray<ipc::Shmem>&& aLargeShmems,
                                               const wr::IdNamespace& aIdNamespace,
                                               const TimeStamp& aRefreshStartTime,
                                               const TimeStamp& aTxnStartTime,
                                               const TimeStamp& aFwdTime) override;
  mozilla::ipc::IPCResult RecvSetFocusTarget(const FocusTarget& aFocusTarget) override;
  mozilla::ipc::IPCResult RecvParentCommands(nsTArray<WebRenderParentCommand>&& commands) override;
  mozilla::ipc::IPCResult RecvGetSnapshot(PTextureParent* aTexture) override;

  mozilla::ipc::IPCResult RecvSetLayersObserverEpoch(const LayersObserverEpoch& aChildEpoch) override;

  mozilla::ipc::IPCResult RecvClearCachedResources() override;
  mozilla::ipc::IPCResult RecvScheduleComposite() override;
  mozilla::ipc::IPCResult RecvCapture() override;
  mozilla::ipc::IPCResult RecvSyncWithCompositor() override;

  mozilla::ipc::IPCResult RecvSetConfirmedTargetAPZC(const uint64_t& aBlockId,
                                                     nsTArray<ScrollableLayerGuid>&& aTargets) override;

  mozilla::ipc::IPCResult RecvSetTestSampleTime(const TimeStamp& aTime) override;
  mozilla::ipc::IPCResult RecvLeaveTestMode() override;
  mozilla::ipc::IPCResult RecvGetAnimationValue(const uint64_t& aCompositorAnimationsId,
                                                OMTAValue* aValue) override;
  mozilla::ipc::IPCResult RecvSetAsyncScrollOffset(const ScrollableLayerGuid::ViewID& aScrollId,
                                                   const float& aX,
                                                   const float& aY) override;
  mozilla::ipc::IPCResult RecvSetAsyncZoom(const ScrollableLayerGuid::ViewID& aScrollId,
                                           const float& aZoom) override;
  mozilla::ipc::IPCResult RecvFlushApzRepaints() override;
  mozilla::ipc::IPCResult RecvGetAPZTestData(APZTestData* data) override;

  void ActorDestroy(ActorDestroyReason aWhy) override;

  void Pause();
  bool Resume();

  void Destroy();

  // CompositorVsyncSchedulerOwner
  bool IsPendingComposite() override { return false; }
  void FinishPendingComposite() override { }
  void CompositeToTarget(gfx::DrawTarget* aTarget, const gfx::IntRect* aRect = nullptr) override;
  TimeDuration GetVsyncInterval() const override;

  // CompositableParentManager
  bool IsSameProcess() const override;
  base::ProcessId GetChildProcessId() override;
  void NotifyNotUsed(PTextureParent* aTexture, uint64_t aTransactionId) override;
  void SendAsyncMessage(const InfallibleTArray<AsyncParentMessageData>& aMessage) override;
  void SendPendingAsyncMessages() override;
  void SetAboutToSendAsyncMessages() override;

  void HoldPendingTransactionId(const wr::Epoch& aWrEpoch,
                                TransactionId aTransactionId,
                                bool aContainsSVGGroup,
                                const TimeStamp& aRefreshStartTime,
                                const TimeStamp& aTxnStartTime,
                                const TimeStamp& aFwdTime,
                                const bool aIsFirstPaint,
                                const bool aUseForTelemetry = true);
  TransactionId LastPendingTransactionId();
  TransactionId FlushTransactionIdsForEpoch(const wr::Epoch& aEpoch, const TimeStamp& aEndTime,
                                            UiCompositorControllerParent* aUiController);

  TextureFactoryIdentifier GetTextureFactoryIdentifier();

  void ExtractImageCompositeNotifications(nsTArray<ImageCompositeNotificationInfo>* aNotifications);

  wr::Epoch GetCurrentEpoch() const { return mWrEpoch; }
  wr::IdNamespace GetIdNamespace()
  {
    return mIdNamespace;
  }

  void FlushRendering(bool aWaitForPresent = true);

  /**
   * Schedule generating WebRender frame definitely at next composite timing.
   *
   * WebRenderBridgeParent uses composite timing to check if there is an update
   * to AsyncImagePipelines. If there is no update, WebRenderBridgeParent skips
   * to generate frame. If we need to generate new frame at next composite timing,
   * call this method.
   *
   * Call CompositorVsyncScheduler::ScheduleComposition() directly, if we just
   * want to trigger AsyncImagePipelines update checks.
   */
  void ScheduleGenerateFrame();

  /**
   * Schedule forced frame rendering at next composite timing.
   *
   * WebRender could skip frame rendering if there is no update.
   * This function is used to force rendering even when there is not update.
   */
  void ScheduleForcedGenerateFrame();

  wr::Epoch UpdateWebRender(CompositorVsyncScheduler* aScheduler,
                            wr::WebRenderAPI* aApi,
                            AsyncImagePipelineManager* aImageMgr,
                            CompositorAnimationStorage* aAnimStorage,
                            const TextureFactoryIdentifier& aTextureFactoryIdentifier);

  void RemoveEpochDataPriorTo(const wr::Epoch& aRenderedEpoch);

  /**
   * This sets the is-first-paint flag to true for the next received
   * display list. This is intended to be called by the widget code when it
   * loses its viewport information (or for whatever reason wants to refresh
   * the viewport information). The message will sent back to the widget code
   * via UiCompositorControllerParent::NotifyFirstPaint() when the corresponding
   * transaction is flushed.
   */
  void ForceIsFirstPaint() { mIsFirstPaint = true; }

private:
  class ScheduleSharedSurfaceRelease;

  explicit WebRenderBridgeParent(const wr::PipelineId& aPipelineId);
  virtual ~WebRenderBridgeParent();

  void UpdateAPZFocusState(const FocusTarget& aFocus);
  void UpdateAPZScrollData(const wr::Epoch& aEpoch, WebRenderScrollData&& aData);
  void UpdateAPZScrollOffsets(ScrollUpdatesMap&& aUpdates,
                              uint32_t aPaintSequenceNumber);

  bool UpdateResources(const nsTArray<OpUpdateResource>& aResourceUpdates,
                       const nsTArray<RefCountedShmem>& aSmallShmems,
                       const nsTArray<ipc::Shmem>& aLargeShmems,
                       wr::TransactionBuilder& aUpdates);
  bool AddExternalImage(wr::ExternalImageId aExtId, wr::ImageKey aKey,
                        wr::TransactionBuilder& aResources);
  bool UpdateExternalImage(wr::ExternalImageId aExtId, wr::ImageKey aKey,
                           const ImageIntRect& aDirtyRect,
                           wr::TransactionBuilder& aResources,
                           UniquePtr<ScheduleSharedSurfaceRelease>& aScheduleRelease);

  bool PushExternalImageForTexture(wr::ExternalImageId aExtId,
                                   wr::ImageKey aKey,
                                   TextureHost* aTexture,
                                   bool aIsUpdate,
                                   wr::TransactionBuilder& aResources);

  void AddPipelineIdForCompositable(const wr::PipelineId& aPipelineIds,
                                    const CompositableHandle& aHandle,
                                    const bool& aAsync,
                                    wr::TransactionBuilder& aTxn,
                                    wr::TransactionBuilder& aTxnForImageBridge);
  void RemovePipelineIdForCompositable(const wr::PipelineId& aPipelineId,
                                       wr::TransactionBuilder& aTxn);

  void DeleteImage(const wr::ImageKey& aKey,
                   wr::TransactionBuilder& aUpdates);
  void ReleaseTextureOfImage(const wr::ImageKey& aKey);

  LayersId GetLayersId() const;
  bool ProcessWebRenderParentCommands(const InfallibleTArray<WebRenderParentCommand>& aCommands,
                                      wr::TransactionBuilder& aTxn);

  void ClearResources();
  bool ShouldParentObserveEpoch();
  mozilla::ipc::IPCResult HandleShutdown();

  // Returns true if there is any animation (including animations in delay
  // phase).
  bool AdvanceAnimations();
  bool SampleAnimations(nsTArray<wr::WrOpacityProperty>& aOpacityArray,
                        nsTArray<wr::WrTransformProperty>& aTransformArray);

  bool IsRootWebRenderBridgeParent() const;

  CompositorBridgeParent* GetRootCompositorBridgeParent() const;

  RefPtr<WebRenderBridgeParent> GetRootWebRenderBridgeParent() const;

  // Tell APZ what the subsequent sampling's timestamp should be.
  void SetAPZSampleTime();

  wr::Epoch GetNextWrEpoch();

  void FlushSceneBuilds();
  void FlushFrameGeneration();
  void FlushFramePresentation();

  void MaybeGenerateFrame(bool aForceGenerateFrame);

private:
  struct PendingTransactionId {
    PendingTransactionId(const wr::Epoch& aEpoch,
                         TransactionId aId,
                         bool aContainsSVGGroup,
                         const TimeStamp& aRefreshStartTime,
                         const TimeStamp& aTxnStartTime,
                         const TimeStamp& aFwdTime,
                         const bool aIsFirstPaint,
                         const bool aUseForTelemetry)
      : mEpoch(aEpoch)
      , mId(aId)
      , mRefreshStartTime(aRefreshStartTime)
      , mTxnStartTime(aTxnStartTime)
      , mFwdTime(aFwdTime)
      , mContainsSVGGroup(aContainsSVGGroup)
      , mIsFirstPaint(aIsFirstPaint)
      , mUseForTelemetry(aUseForTelemetry)
    {}
    wr::Epoch mEpoch;
    TransactionId mId;
    TimeStamp mRefreshStartTime;
    TimeStamp mTxnStartTime;
    TimeStamp mFwdTime;
    bool mContainsSVGGroup;
    bool mIsFirstPaint;
    bool mUseForTelemetry;
  };

  struct CompositorAnimationIdsForEpoch {
    CompositorAnimationIdsForEpoch(const wr::Epoch& aEpoch, InfallibleTArray<uint64_t>&& aIds)
      : mEpoch(aEpoch)
      , mIds(std::move(aIds))
    {
    }

    wr::Epoch mEpoch;
    InfallibleTArray<uint64_t> mIds;
  };

  CompositorBridgeParentBase* MOZ_NON_OWNING_REF mCompositorBridge;
  wr::PipelineId mPipelineId;
  RefPtr<widget::CompositorWidget> mWidget;
  RefPtr<wr::WebRenderAPI> mApi;
  RefPtr<AsyncImagePipelineManager> mAsyncImageManager;
  RefPtr<CompositorVsyncScheduler> mCompositorScheduler;
  RefPtr<CompositorAnimationStorage> mAnimStorage;
  // mActiveAnimations is used to avoid leaking animations when WebRenderBridgeParent is
  // destroyed abnormally and Tab move between different windows.
  std::unordered_set<uint64_t> mActiveAnimations;
  std::unordered_map<uint64_t, RefPtr<WebRenderImageHost>> mAsyncCompositables;
  std::unordered_map<uint64_t, CompositableTextureHostRef> mTextureHosts;
  std::unordered_map<uint64_t, wr::ExternalImageId> mSharedSurfaceIds;

  TimeDuration mVsyncRate;
  TimeStamp mPreviousFrameTimeStamp;
  // These fields keep track of the latest layer observer epoch values in the child and the
  // parent. mChildLayersObserverEpoch is the latest epoch value received from the child.
  // mParentLayersObserverEpoch is the latest epoch value that we have told TabParent about
  // (via ObserveLayerUpdate).
  LayersObserverEpoch mChildLayersObserverEpoch;
  LayersObserverEpoch mParentLayersObserverEpoch;

  std::queue<PendingTransactionId> mPendingTransactionIds;
  std::queue<CompositorAnimationIdsForEpoch> mCompositorAnimationsToDelete;
  wr::Epoch mWrEpoch;
  wr::IdNamespace mIdNamespace;

  bool mPaused;
  bool mDestroyed;
  bool mReceivedDisplayList;
  bool mIsFirstPaint;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_WebRenderBridgeParent_h
