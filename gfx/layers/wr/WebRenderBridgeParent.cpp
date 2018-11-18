/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/WebRenderBridgeParent.h"

#include "CompositableHost.h"
#include "gfxEnv.h"
#include "gfxPrefs.h"
#include "gfxEnv.h"
#include "GeckoProfiler.h"
#include "GLContext.h"
#include "GLContextProvider.h"
#include "mozilla/Range.h"
#include "mozilla/layers/AnimationHelper.h"
#include "mozilla/layers/APZSampler.h"
#include "mozilla/layers/APZUpdater.h"
#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/CompositorVsyncScheduler.h"
#include "mozilla/layers/ImageBridgeParent.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/IpcResourceUpdateQueue.h"
#include "mozilla/layers/SharedSurfacesParent.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/layers/AsyncImagePipelineManager.h"
#include "mozilla/layers/WebRenderImageHost.h"
#include "mozilla/layers/WebRenderTextureHost.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Unused.h"
#include "mozilla/webrender/RenderThread.h"
#include "mozilla/widget/CompositorWidget.h"

#ifdef MOZ_GECKO_PROFILER
#include "ProfilerMarkerPayload.h"
#endif

bool is_in_main_thread()
{
  return NS_IsMainThread();
}

bool is_in_compositor_thread()
{
  return mozilla::layers::CompositorThreadHolder::IsInCompositorThread();
}

bool is_in_render_thread()
{
  return mozilla::wr::RenderThread::IsInRenderThread();
}

void gecko_profiler_start_marker(const char* name)
{
#ifdef MOZ_GECKO_PROFILER
  profiler_tracing("WebRender", name, TRACING_INTERVAL_START);
#endif
}

void gecko_profiler_end_marker(const char* name)
{
#ifdef MOZ_GECKO_PROFILER
  profiler_tracing("WebRender", name, TRACING_INTERVAL_END);
#endif
}

bool is_glcontext_egl(void* glcontext_ptr)
{
  MOZ_ASSERT(glcontext_ptr);

  mozilla::gl::GLContext* glcontext = reinterpret_cast<mozilla::gl::GLContext*>(glcontext_ptr);
  if (!glcontext) {
    return false;
  }
  return glcontext->GetContextType() == mozilla::gl::GLContextType::EGL;
}

bool is_glcontext_angle(void* glcontext_ptr)
{
  MOZ_ASSERT(glcontext_ptr);

  mozilla::gl::GLContext* glcontext = reinterpret_cast<mozilla::gl::GLContext*>(glcontext_ptr);
  if (!glcontext) {
    return false;
  }
  return glcontext->IsANGLE();
}

bool gfx_use_wrench()
{
  return gfxEnv::EnableWebRenderRecording();
}

const char* gfx_wr_resource_path_override()
{
  const char* resourcePath = PR_GetEnv("WR_RESOURCE_PATH");
  if (!resourcePath || resourcePath[0] == '\0') {
    return nullptr;
  }
  return resourcePath;
}

void gfx_critical_note(const char* msg)
{
  gfxCriticalNote << msg;
}

void gfx_critical_error(const char* msg)
{
  gfxCriticalError() << msg;
}

void gecko_printf_stderr_output(const char* msg)
{
  printf_stderr("%s\n", msg);
}

void* get_proc_address_from_glcontext(void* glcontext_ptr, const char* procname)
{
  MOZ_ASSERT(glcontext_ptr);

  mozilla::gl::GLContext* glcontext = reinterpret_cast<mozilla::gl::GLContext*>(glcontext_ptr);
  if (!glcontext) {
    return nullptr;
  }
  PRFuncPtr p = glcontext->LookupSymbol(procname);
  return reinterpret_cast<void*>(p);
}

void
gecko_profiler_register_thread(const char* name)
{
  PROFILER_REGISTER_THREAD(name);
}

void
gecko_profiler_unregister_thread()
{
  PROFILER_UNREGISTER_THREAD();
}

void
record_telemetry_time(mozilla::wr::TelemetryProbe aProbe, uint64_t aTimeNs)
{
  uint32_t time_ms = (uint32_t)(aTimeNs / 1000000);
  switch (aProbe) {
    case mozilla::wr::TelemetryProbe::SceneBuildTime:
      mozilla::Telemetry::Accumulate(mozilla::Telemetry::WR_SCENEBUILD_TIME, time_ms);
      break;
    case mozilla::wr::TelemetryProbe::SceneSwapTime:
      mozilla::Telemetry::Accumulate(mozilla::Telemetry::WR_SCENESWAP_TIME, time_ms);
      break;
    case mozilla::wr::TelemetryProbe::RenderTime:
      mozilla::Telemetry::Accumulate(mozilla::Telemetry::WR_RENDER_TIME, time_ms);
      break;
    default:
      MOZ_ASSERT(false);
      break;
  }
}

namespace mozilla {

namespace layers {

using namespace mozilla::gfx;

class ScheduleObserveLayersUpdate: public wr::NotificationHandler {
public:
  ScheduleObserveLayersUpdate(RefPtr<CompositorBridgeParentBase> aBridge,
                              LayersId aLayersId,
                              LayersObserverEpoch aEpoch,
                              bool aIsActive)
  : mBridge(aBridge)
  , mLayersId(aLayersId)
  , mObserverEpoch(aEpoch)
  , mIsActive(aIsActive)
  {}

  virtual void Notify(wr::Checkpoint) override {
    CompositorThreadHolder::Loop()->PostTask(
      NewRunnableMethod<LayersId, LayersObserverEpoch, int>(
        "ObserveLayersUpdate",
        mBridge, &CompositorBridgeParentBase::ObserveLayersUpdate,
        mLayersId, mObserverEpoch, mIsActive
      )
    );
  }
protected:
  RefPtr<CompositorBridgeParentBase> mBridge;
  LayersId mLayersId;
  LayersObserverEpoch mObserverEpoch;
  bool mIsActive;
};

class SceneBuiltNotification: public wr::NotificationHandler {
public:
  explicit SceneBuiltNotification(TimeStamp aTxnStartTime)
  : mTxnStartTime(aTxnStartTime)
  {}

  virtual void Notify(wr::Checkpoint) override {
    auto startTime = this->mTxnStartTime;
    CompositorThreadHolder::Loop()->PostTask(
      NS_NewRunnableFunction("SceneBuiltNotificationRunnable", [startTime]() {
        auto endTime = TimeStamp::Now();
#ifdef MOZ_GECKO_PROFILER
        if (profiler_is_active()) {
          class ContentFullPaintPayload : public ProfilerMarkerPayload
          {
          public:
            ContentFullPaintPayload(const mozilla::TimeStamp& aStartTime,
                                    const mozilla::TimeStamp& aEndTime)
              : ProfilerMarkerPayload(aStartTime, aEndTime)
            {
            }
            virtual void StreamPayload(SpliceableJSONWriter& aWriter,
                                       const TimeStamp& aProcessStartTime,
                                       UniqueStacks& aUniqueStacks) override
            {
              StreamCommonProps("CONTENT_FULL_PAINT_TIME",
                                aWriter,
                                aProcessStartTime,
                                aUniqueStacks);
            }
          };

          profiler_add_marker_for_thread(profiler_current_thread_id(),
                                         "CONTENT_FULL_PAINT_TIME",
                                         MakeUnique<ContentFullPaintPayload>(startTime, endTime));
        }
#endif
        Telemetry::Accumulate(Telemetry::CONTENT_FULL_PAINT_TIME,
                              static_cast<uint32_t>((endTime - startTime).ToMilliseconds()));
      }));
  }
protected:
  TimeStamp mTxnStartTime;
};


class WebRenderBridgeParent::ScheduleSharedSurfaceRelease final
  : public wr::NotificationHandler
{
public:
  ScheduleSharedSurfaceRelease()
  { }

  void Add(const wr::ExternalImageId& aId)
  {
    mSurfaces.AppendElement(aId);
  }

  void Notify(wr::Checkpoint) override
  {
    for (const auto& id : mSurfaces) {
      SharedSurfacesParent::Release(id);
    }
  }

private:
  AutoTArray<wr::ExternalImageId, 20> mSurfaces;
};

class MOZ_STACK_CLASS AutoWebRenderBridgeParentAsyncMessageSender
{
public:
  explicit AutoWebRenderBridgeParentAsyncMessageSender(WebRenderBridgeParent* aWebRenderBridgeParent,
                                                       InfallibleTArray<OpDestroy>* aDestroyActors = nullptr)
    : mWebRenderBridgeParent(aWebRenderBridgeParent)
    , mActorsToDestroy(aDestroyActors)
  {
    mWebRenderBridgeParent->SetAboutToSendAsyncMessages();
  }

  ~AutoWebRenderBridgeParentAsyncMessageSender()
  {
    mWebRenderBridgeParent->SendPendingAsyncMessages();
    if (mActorsToDestroy) {
      // Destroy the actors after sending the async messages because the latter may contain
      // references to some actors.
      for (const auto& op : *mActorsToDestroy) {
        mWebRenderBridgeParent->DestroyActor(op);
      }
    }
  }
private:
  WebRenderBridgeParent* mWebRenderBridgeParent;
  InfallibleTArray<OpDestroy>* mActorsToDestroy;
};

WebRenderBridgeParent::WebRenderBridgeParent(CompositorBridgeParentBase* aCompositorBridge,
                                             const wr::PipelineId& aPipelineId,
                                             widget::CompositorWidget* aWidget,
                                             CompositorVsyncScheduler* aScheduler,
                                             RefPtr<wr::WebRenderAPI>&& aApi,
                                             RefPtr<AsyncImagePipelineManager>&& aImageMgr,
                                             RefPtr<CompositorAnimationStorage>&& aAnimStorage,
                                             TimeDuration aVsyncRate)
  : mCompositorBridge(aCompositorBridge)
  , mPipelineId(aPipelineId)
  , mWidget(aWidget)
  , mApi(aApi)
  , mAsyncImageManager(aImageMgr)
  , mCompositorScheduler(aScheduler)
  , mAnimStorage(aAnimStorage)
  , mVsyncRate(aVsyncRate)
  , mChildLayersObserverEpoch{0}
  , mParentLayersObserverEpoch{0}
  , mWrEpoch{0}
  , mIdNamespace(aApi->GetNamespace())
  , mPaused(false)
  , mDestroyed(false)
  , mReceivedDisplayList(false)
  , mIsFirstPaint(true)
{
  MOZ_ASSERT(mAsyncImageManager);
  MOZ_ASSERT(mAnimStorage);
  mAsyncImageManager->AddPipeline(mPipelineId, this);
  if (IsRootWebRenderBridgeParent()) {
    MOZ_ASSERT(!mCompositorScheduler);
    mCompositorScheduler = new CompositorVsyncScheduler(this, mWidget);
  }
}

WebRenderBridgeParent::WebRenderBridgeParent(const wr::PipelineId& aPipelineId)
  : mCompositorBridge(nullptr)
  , mPipelineId(aPipelineId)
  , mChildLayersObserverEpoch{0}
  , mParentLayersObserverEpoch{0}
  , mWrEpoch{0}
  , mIdNamespace{0}
  , mPaused(false)
  , mDestroyed(true)
  , mReceivedDisplayList(false)
  , mIsFirstPaint(false)
{
}

/* static */ WebRenderBridgeParent*
WebRenderBridgeParent::CreateDestroyed(const wr::PipelineId& aPipelineId)
{
  return new WebRenderBridgeParent(aPipelineId);
}

WebRenderBridgeParent::~WebRenderBridgeParent()
{
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvEnsureConnected(TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                           MaybeIdNamespace* aMaybeIdNamespace)
{
  if (mDestroyed) {
    *aTextureFactoryIdentifier = TextureFactoryIdentifier(LayersBackend::LAYERS_NONE);
    *aMaybeIdNamespace = Nothing();
    return IPC_OK();
  }

  MOZ_ASSERT(mIdNamespace.mHandle != 0);
  *aTextureFactoryIdentifier = GetTextureFactoryIdentifier();
  *aMaybeIdNamespace = Some(mIdNamespace);

  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvShutdown()
{
  return HandleShutdown();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvShutdownSync()
{
  return HandleShutdown();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::HandleShutdown()
{
  Destroy();
  IProtocol* mgr = Manager();
  if (!Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

void
WebRenderBridgeParent::Destroy()
{
  if (mDestroyed) {
    return;
  }
  mDestroyed = true;
  ClearResources();
}

bool
WebRenderBridgeParent::UpdateResources(const nsTArray<OpUpdateResource>& aResourceUpdates,
                                       const nsTArray<RefCountedShmem>& aSmallShmems,
                                       const nsTArray<ipc::Shmem>& aLargeShmems,
                                       wr::TransactionBuilder& aUpdates)
{
  wr::ShmSegmentsReader reader(aSmallShmems, aLargeShmems);
  UniquePtr<ScheduleSharedSurfaceRelease> scheduleRelease;

  for (const auto& cmd : aResourceUpdates) {
    switch (cmd.type()) {
      case OpUpdateResource::TOpAddImage: {
        const auto& op = cmd.get_OpAddImage();
        wr::Vec<uint8_t> bytes;
        if (!reader.Read(op.bytes(), bytes)) {
          return false;
        }
        aUpdates.AddImage(op.key(), op.descriptor(), bytes);
        break;
      }
      case OpUpdateResource::TOpUpdateImage: {
        const auto& op = cmd.get_OpUpdateImage();
        wr::Vec<uint8_t> bytes;
        if (!reader.Read(op.bytes(), bytes)) {
          return false;
        }
        aUpdates.UpdateImageBuffer(op.key(), op.descriptor(), bytes);
        break;
      }
      case OpUpdateResource::TOpAddBlobImage: {
        const auto& op = cmd.get_OpAddBlobImage();
        wr::Vec<uint8_t> bytes;
        if (!reader.Read(op.bytes(), bytes)) {
          return false;
        }
        aUpdates.AddBlobImage(op.key(), op.descriptor(), bytes);
        break;
      }
      case OpUpdateResource::TOpUpdateBlobImage: {
        const auto& op = cmd.get_OpUpdateBlobImage();
        wr::Vec<uint8_t> bytes;
        if (!reader.Read(op.bytes(), bytes)) {
          return false;
        }
        aUpdates.UpdateBlobImage(op.key(), op.descriptor(), bytes, wr::ToDeviceIntRect(op.dirtyRect()));
        break;
      }
      case OpUpdateResource::TOpSetImageVisibleArea: {
        const auto& op = cmd.get_OpSetImageVisibleArea();
        wr::DeviceIntRect area;
        area.origin.x = op.area().x;
        area.origin.y = op.area().y;
        area.size.width = op.area().width;
        area.size.height = op.area().height;
        aUpdates.SetImageVisibleArea(op.key(), area);
        break;
      }
      case OpUpdateResource::TOpAddExternalImage: {
        const auto& op = cmd.get_OpAddExternalImage();
        if (!AddExternalImage(op.externalImageId(), op.key(), aUpdates)) {
          return false;
        }
        break;
      }
      case OpUpdateResource::TOpPushExternalImageForTexture: {
        const auto& op = cmd.get_OpPushExternalImageForTexture();
        CompositableTextureHostRef texture;
        texture = TextureHost::AsTextureHost(op.textureParent());
        if (!PushExternalImageForTexture(op.externalImageId(), op.key(), texture, op.isUpdate(), aUpdates)) {
          return false;
        }
        break;
      }
      case OpUpdateResource::TOpUpdateExternalImage: {
        const auto& op = cmd.get_OpUpdateExternalImage();
        if (!UpdateExternalImage(op.externalImageId(), op.key(), op.dirtyRect(),
                                 aUpdates, scheduleRelease)) {
          return false;
        }
        break;
      }
      case OpUpdateResource::TOpAddRawFont: {
        const auto& op = cmd.get_OpAddRawFont();
        wr::Vec<uint8_t> bytes;
        if (!reader.Read(op.bytes(), bytes)) {
          return false;
        }
        aUpdates.AddRawFont(op.key(), bytes, op.fontIndex());
        break;
      }
      case OpUpdateResource::TOpAddFontDescriptor: {
        const auto& op = cmd.get_OpAddFontDescriptor();
        wr::Vec<uint8_t> bytes;
        if (!reader.Read(op.bytes(), bytes)) {
          return false;
        }
        aUpdates.AddFontDescriptor(op.key(), bytes, op.fontIndex());
        break;
      }
      case OpUpdateResource::TOpAddFontInstance: {
        const auto& op = cmd.get_OpAddFontInstance();
        wr::Vec<uint8_t> variations;
        if (!reader.Read(op.variations(), variations)) {
            return false;
        }
        aUpdates.AddFontInstance(op.instanceKey(), op.fontKey(),
                                 op.glyphSize(),
                                 op.options().ptrOr(nullptr),
                                 op.platformOptions().ptrOr(nullptr),
                                 variations);
        break;
      }
      case OpUpdateResource::TOpDeleteImage: {
        const auto& op = cmd.get_OpDeleteImage();
        DeleteImage(op.key(), aUpdates);
        break;
      }
      case OpUpdateResource::TOpDeleteFont: {
        const auto& op = cmd.get_OpDeleteFont();
        aUpdates.DeleteFont(op.key());
        break;
      }
      case OpUpdateResource::TOpDeleteFontInstance: {
        const auto& op = cmd.get_OpDeleteFontInstance();
        aUpdates.DeleteFontInstance(op.key());
        break;
      }
      case OpUpdateResource::T__None: break;
    }
  }

  if (scheduleRelease) {
    aUpdates.Notify(wr::Checkpoint::FrameRendered, std::move(scheduleRelease));
  }
  return true;
}

bool
WebRenderBridgeParent::AddExternalImage(wr::ExternalImageId aExtId, wr::ImageKey aKey,
                                        wr::TransactionBuilder& aResources)
{
  Range<wr::ImageKey> keys(&aKey, 1);
  // Check if key is obsoleted.
  if (keys[0].mNamespace != mIdNamespace) {
    return true;
  }

  auto key = wr::AsUint64(aKey);
  auto it = mSharedSurfaceIds.find(key);
  if (it != mSharedSurfaceIds.end()) {
    gfxCriticalNote << "Readding known shared surface: " << key;
    return false;
  }

  RefPtr<DataSourceSurface> dSurf = SharedSurfacesParent::Acquire(aExtId);
  if (!dSurf) {
    gfxCriticalNote << "DataSourceSurface of SharedSurfaces does not exist for extId:" << wr::AsUint64(aExtId);
    return false;
  }

  mSharedSurfaceIds.insert(std::make_pair(key, aExtId));

  if (!gfxEnv::EnableWebRenderRecording()) {
    wr::ImageDescriptor descriptor(dSurf->GetSize(), dSurf->Stride(),
                                   dSurf->GetFormat());
    aResources.AddExternalImage(aKey, descriptor, aExtId,
                                wr::WrExternalImageBufferType::ExternalBuffer,
                                0);
    return true;
  }

  DataSourceSurface::MappedSurface map;
  if (!dSurf->Map(gfx::DataSourceSurface::MapType::READ, &map)) {
    gfxCriticalNote << "DataSourceSurface failed to map for Image for extId:" << wr::AsUint64(aExtId);
    return false;
  }

  IntSize size = dSurf->GetSize();
  wr::ImageDescriptor descriptor(size, map.mStride, dSurf->GetFormat());
  wr::Vec<uint8_t> data;
  data.PushBytes(Range<uint8_t>(map.mData, size.height * map.mStride));
  aResources.AddImage(keys[0], descriptor, data);
  dSurf->Unmap();

  return true;
}

bool
WebRenderBridgeParent::PushExternalImageForTexture(wr::ExternalImageId aExtId,
                                                   wr::ImageKey aKey,
                                                   TextureHost* aTexture,
                                                   bool aIsUpdate,
                                                   wr::TransactionBuilder& aResources)
{
  auto op = aIsUpdate ? TextureHost::UPDATE_IMAGE : TextureHost::ADD_IMAGE;
  Range<wr::ImageKey> keys(&aKey, 1);
  // Check if key is obsoleted.
  if (keys[0].mNamespace != mIdNamespace) {
    return true;
  }

  if(!aTexture) {
    gfxCriticalNote << "TextureHost does not exist for extId:" << wr::AsUint64(aExtId);
    return false;
  }

  if (!gfxEnv::EnableWebRenderRecording()) {
    WebRenderTextureHost* wrTexture = aTexture->AsWebRenderTextureHost();
    if (wrTexture) {
      wrTexture->PushResourceUpdates(aResources, op, keys,
                                     wrTexture->GetExternalImageKey());
      auto it = mTextureHosts.find(wr::AsUint64(aKey));
      MOZ_ASSERT((it == mTextureHosts.end() && !aIsUpdate) ||
                 (it != mTextureHosts.end() && aIsUpdate));
      if (it != mTextureHosts.end()) {
        // Release Texture if it exists.
        ReleaseTextureOfImage(aKey);
      }
      mTextureHosts.emplace(wr::AsUint64(aKey), CompositableTextureHostRef(aTexture));
      return true;
    }
  }
  RefPtr<DataSourceSurface> dSurf = aTexture->GetAsSurface();
  if (!dSurf) {
    gfxCriticalNote << "TextureHost does not return DataSourceSurface for extId:" << wr::AsUint64(aExtId);
    return false;
  }

  DataSourceSurface::MappedSurface map;
  if (!dSurf->Map(gfx::DataSourceSurface::MapType::READ, &map)) {
    gfxCriticalNote << "DataSourceSurface failed to map for Image for extId:" << wr::AsUint64(aExtId);
    return false;
  }

  IntSize size = dSurf->GetSize();
  wr::ImageDescriptor descriptor(size, map.mStride, dSurf->GetFormat());
  wr::Vec<uint8_t> data;
  data.PushBytes(Range<uint8_t>(map.mData, size.height * map.mStride));

  if (op == TextureHost::UPDATE_IMAGE) {
    aResources.UpdateImageBuffer(keys[0], descriptor, data);
  } else {
    aResources.AddImage(keys[0], descriptor, data);
  }

  dSurf->Unmap();

  return true;
}

bool
WebRenderBridgeParent::UpdateExternalImage(wr::ExternalImageId aExtId,
                                           wr::ImageKey aKey,
                                           const ImageIntRect& aDirtyRect,
                                           wr::TransactionBuilder& aResources,
                                           UniquePtr<ScheduleSharedSurfaceRelease>& aScheduleRelease)
{
  Range<wr::ImageKey> keys(&aKey, 1);
  // Check if key is obsoleted.
  if (keys[0].mNamespace != mIdNamespace) {
    return true;
  }

  auto key = wr::AsUint64(aKey);
  auto it = mSharedSurfaceIds.find(key);
  if (it == mSharedSurfaceIds.end()) {
    gfxCriticalNote << "Updating unknown shared surface: " << key;
    return false;
  }

  RefPtr<DataSourceSurface> dSurf;
  if (it->second == aExtId) {
    dSurf = SharedSurfacesParent::Get(aExtId);
  } else {
    dSurf = SharedSurfacesParent::Acquire(aExtId);
  }

  if (!dSurf) {
    gfxCriticalNote << "Shared surface does not exist for extId:" << wr::AsUint64(aExtId);
    return false;
  }

  if (!(it->second == aExtId)) {
    // We already have a mapping for this image key, so ensure we release the
    // previous external image ID. This can happen when an image is animated,
    // and it is changing the external image that the animation points to.
    if (!aScheduleRelease) {
      aScheduleRelease = MakeUnique<ScheduleSharedSurfaceRelease>();
    }
    aScheduleRelease->Add(it->second);
    it->second = aExtId;
  }

  if (!gfxEnv::EnableWebRenderRecording()) {
    wr::ImageDescriptor descriptor(dSurf->GetSize(), dSurf->Stride(),
                                   dSurf->GetFormat());
    aResources.UpdateExternalImageWithDirtyRect(aKey, descriptor, aExtId,
                                                wr::WrExternalImageBufferType::ExternalBuffer,
                                                wr::ToDeviceIntRect(aDirtyRect),
                                                0);
    return true;
  }

  DataSourceSurface::ScopedMap map(dSurf, DataSourceSurface::READ);
  if (!map.IsMapped()) {
    gfxCriticalNote << "DataSourceSurface failed to map for Image for extId:" << wr::AsUint64(aExtId);
    return false;
  }

  IntSize size = dSurf->GetSize();
  wr::ImageDescriptor descriptor(size, map.GetStride(), dSurf->GetFormat());
  wr::Vec<uint8_t> data;
  data.PushBytes(Range<uint8_t>(map.GetData(), size.height * map.GetStride()));
  aResources.UpdateImageBuffer(keys[0], descriptor, data);
  return true;
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvUpdateResources(nsTArray<OpUpdateResource>&& aResourceUpdates,
                                           nsTArray<RefCountedShmem>&& aSmallShmems,
                                           nsTArray<ipc::Shmem>&& aLargeShmems,
                                           const bool& aScheduleComposite)
{
  if (mDestroyed) {
    wr::IpcResourceUpdateQueue::ReleaseShmems(this, aSmallShmems);
    wr::IpcResourceUpdateQueue::ReleaseShmems(this, aLargeShmems);
    return IPC_OK();
  }

  wr::TransactionBuilder txn;
  txn.SetLowPriority(!IsRootWebRenderBridgeParent());

  bool success =
    UpdateResources(aResourceUpdates, aSmallShmems, aLargeShmems, txn);
  wr::IpcResourceUpdateQueue::ReleaseShmems(this, aSmallShmems);
  wr::IpcResourceUpdateQueue::ReleaseShmems(this, aLargeShmems);

  if (!success) {
    return IPC_FAIL(this, "Invalid WebRender resource data shmem or address.");
  }

  if (aScheduleComposite) {
    txn.InvalidateRenderedFrame();
    ScheduleGenerateFrame();
  }

  mApi->SendTransaction(txn);

  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvDeleteCompositorAnimations(InfallibleTArray<uint64_t>&& aIds)
{
  if (mDestroyed) {
    return IPC_OK();
  }

  // Once mWrEpoch has been rendered, we can delete these compositor animations
  mCompositorAnimationsToDelete.push(CompositorAnimationIdsForEpoch(mWrEpoch, std::move(aIds)));
  return IPC_OK();
}

void
WebRenderBridgeParent::RemoveEpochDataPriorTo(const wr::Epoch& aRenderedEpoch)
{
  while (!mCompositorAnimationsToDelete.empty()) {
    if (mCompositorAnimationsToDelete.front().mEpoch.mHandle > aRenderedEpoch.mHandle) {
      break;
    }
    for (uint64_t id : mCompositorAnimationsToDelete.front().mIds) {
      if (mActiveAnimations.erase(id) > 0) {
        mAnimStorage->ClearById(id);
      } else {
        NS_ERROR("Tried to delete invalid animation");
      }
    }
    mCompositorAnimationsToDelete.pop();
  }
}

bool
WebRenderBridgeParent::IsRootWebRenderBridgeParent() const
{
  return !!mWidget;
}

CompositorBridgeParent*
WebRenderBridgeParent::GetRootCompositorBridgeParent() const
{
  if (!mCompositorBridge) {
    return nullptr;
  }

  if (IsRootWebRenderBridgeParent()) {
    // This WebRenderBridgeParent is attached to the root
    // CompositorBridgeParent.
    return static_cast<CompositorBridgeParent*>(mCompositorBridge);
  }

  // Otherwise, this WebRenderBridgeParent is attached to a
  // CrossProcessCompositorBridgeParent so we have an extra level of
  // indirection to unravel.
  CompositorBridgeParent::LayerTreeState* lts =
      CompositorBridgeParent::GetIndirectShadowTree(GetLayersId());
  if (!lts) {
    return nullptr;
  }
  return lts->mParent;
}

RefPtr<WebRenderBridgeParent>
WebRenderBridgeParent::GetRootWebRenderBridgeParent() const
{
  CompositorBridgeParent* cbp = GetRootCompositorBridgeParent();
  if (!cbp) {
    return nullptr;
  }

  return cbp->GetWebRenderBridgeParent();
}

void
WebRenderBridgeParent::UpdateAPZFocusState(const FocusTarget& aFocus)
{
  CompositorBridgeParent* cbp = GetRootCompositorBridgeParent();
  if (!cbp) {
    return;
  }
  LayersId rootLayersId = cbp->RootLayerTreeId();
  if (RefPtr<APZUpdater> apz = cbp->GetAPZUpdater()) {
    apz->UpdateFocusState(rootLayersId, GetLayersId(), aFocus);
  }
}

void
WebRenderBridgeParent::UpdateAPZScrollData(const wr::Epoch& aEpoch,
                                           WebRenderScrollData&& aData)
{
  CompositorBridgeParent* cbp = GetRootCompositorBridgeParent();
  if (!cbp) {
    return;
  }
  LayersId rootLayersId = cbp->RootLayerTreeId();
  if (RefPtr<APZUpdater> apz = cbp->GetAPZUpdater()) {
    apz->UpdateScrollDataAndTreeState(rootLayersId, GetLayersId(), aEpoch, std::move(aData));
  }
}

void
WebRenderBridgeParent::UpdateAPZScrollOffsets(ScrollUpdatesMap&& aUpdates,
                                              uint32_t aPaintSequenceNumber)
{
  CompositorBridgeParent* cbp = GetRootCompositorBridgeParent();
  if (!cbp) {
    return;
  }
  LayersId rootLayersId = cbp->RootLayerTreeId();
  if (RefPtr<APZUpdater> apz = cbp->GetAPZUpdater()) {
    apz->UpdateScrollOffsets(rootLayersId, GetLayersId(), std::move(aUpdates), aPaintSequenceNumber);
  }
}

void
WebRenderBridgeParent::SetAPZSampleTime()
{
  CompositorBridgeParent* cbp = GetRootCompositorBridgeParent();
  if (!cbp) {
    return;
  }
  if (RefPtr<APZSampler> apz = cbp->GetAPZSampler()) {
    TimeStamp animationTime = cbp->GetTestingTimeStamp().valueOr(
        mCompositorScheduler->GetLastComposeTime());
    TimeDuration frameInterval = cbp->GetVsyncInterval();
    // As with the non-webrender codepath in AsyncCompositionManager, we want to
    // use the timestamp for the next vsync when advancing animations.
    if (frameInterval != TimeDuration::Forever()) {
      animationTime += frameInterval;
    }
    apz->SetSampleTime(animationTime);
  }
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvSetDisplayList(const gfx::IntSize& aSize,
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
                                          const TimeStamp& aFwdTime)
{
  if (mDestroyed) {
    for (const auto& op : aToDestroy) {
      DestroyActor(op);
    }
    return IPC_OK();
  }

  AUTO_PROFILER_TRACING("Paint", "SetDisplayList");
  UpdateFwdTransactionId(aFwdTransactionId);

  // This ensures that destroy operations are always processed. It is not safe
  // to early-return from RecvDPEnd without doing so.
  AutoWebRenderBridgeParentAsyncMessageSender autoAsyncMessageSender(this, &aToDestroy);

  wr::Epoch wrEpoch = GetNextWrEpoch();

  mAsyncImageManager->SetCompositionTime(TimeStamp::Now());

  // If id namespaces do not match, it means the command is obsolete, probably
  // because the tab just moved to a new window.
  // In that case do not send the commands to webrender.
  bool validTransaction = aIdNamespace == mIdNamespace;
  wr::TransactionBuilder txn;
  txn.SetLowPriority(!IsRootWebRenderBridgeParent());
  Maybe<wr::AutoTransactionSender> sender;
  if (validTransaction) {
    sender.emplace(mApi, &txn);
  }

  if (!ProcessWebRenderParentCommands(aCommands, txn)) {
    return IPC_FAIL(this, "Invalid parent command found");
  }

  if (!UpdateResources(aResourceUpdates, aSmallShmems, aLargeShmems, txn)) {
    return IPC_FAIL(this, "Failed to deserialize resource updates");
  }

  mReceivedDisplayList = true;

  if (aScrollData.IsFirstPaint()) {
    mIsFirstPaint = true;
  }

  // aScrollData is moved into this function but that is not reflected by the
  // function signature due to the way the IPDL generator works. We remove the
  // const so that we can move this structure all the way to the desired
  // destination.
  // Also note that this needs to happen before the display list transaction is
  // sent to WebRender, so that the UpdateHitTestingTree call is guaranteed to
  // be in the updater queue at the time that the scene swap completes.
  UpdateAPZScrollData(wrEpoch, std::move(const_cast<WebRenderScrollData&>(aScrollData)));

  wr::Vec<uint8_t> dlData(std::move(dl));

  bool observeLayersUpdate = ShouldParentObserveEpoch();

  if (validTransaction) {
    if (IsRootWebRenderBridgeParent()) {
      LayoutDeviceIntSize widgetSize = mWidget->GetClientSize();
      LayoutDeviceIntRect docRect(LayoutDeviceIntPoint(), widgetSize);
      txn.SetWindowParameters(widgetSize, docRect);
    }
    gfx::Color clearColor(0.f, 0.f, 0.f, 0.f);
    txn.SetDisplayList(clearColor, wrEpoch, LayerSize(aSize.width, aSize.height),
                       mPipelineId, aContentSize,
                       dlDesc, dlData);

    if (observeLayersUpdate) {
      txn.Notify(
        wr::Checkpoint::SceneBuilt,
        MakeUnique<ScheduleObserveLayersUpdate>(
          mCompositorBridge,
          GetLayersId(),
          mChildLayersObserverEpoch,
          true
        )
      );
    }

    txn.Notify(
      wr::Checkpoint::SceneBuilt,
      MakeUnique<SceneBuiltNotification>(
        aTxnStartTime
      )
    );

    mApi->SendTransaction(txn);

    // We will schedule generating a frame after the scene
    // build is done, so we don't need to do it here.
  } else if (observeLayersUpdate) {
    mCompositorBridge->ObserveLayersUpdate(GetLayersId(), mChildLayersObserverEpoch, true);
  }

  HoldPendingTransactionId(wrEpoch, aTransactionId, aContainsSVGGroup,
                           aRefreshStartTime, aTxnStartTime, aFwdTime, mIsFirstPaint);
  mIsFirstPaint = false;

  if (!validTransaction) {
    // Pretend we composited since someone is wating for this event,
    // though DisplayList was not pushed to webrender.
    if (CompositorBridgeParent* cbp = GetRootCompositorBridgeParent()) {
      TimeStamp now = TimeStamp::Now();
      cbp->NotifyPipelineRendered(mPipelineId, wrEpoch, now, now);
    }
  }

  wr::IpcResourceUpdateQueue::ReleaseShmems(this, aSmallShmems);
  wr::IpcResourceUpdateQueue::ReleaseShmems(this, aLargeShmems);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvEmptyTransaction(const FocusTarget& aFocusTarget,
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
                                            const TimeStamp& aFwdTime)
{
  if (mDestroyed) {
    for (const auto& op : aToDestroy) {
      DestroyActor(op);
    }
    return IPC_OK();
  }

  AUTO_PROFILER_TRACING("Paint", "EmptyTransaction");
  UpdateFwdTransactionId(aFwdTransactionId);

  // This ensures that destroy operations are always processed. It is not safe
  // to early-return without doing so.
  AutoWebRenderBridgeParentAsyncMessageSender autoAsyncMessageSender(this, &aToDestroy);

  bool scheduleComposite = false;

  UpdateAPZFocusState(aFocusTarget);
  if (!aUpdates.empty()) {
    // aUpdates is moved into this function but that is not reflected by the
    // function signature due to the way the IPDL generator works. We remove the
    // const so that we can move this structure all the way to the desired
    // destination.
    UpdateAPZScrollOffsets(std::move(const_cast<ScrollUpdatesMap&>(aUpdates)), aPaintSequenceNumber);
    scheduleComposite = true;
  }

  wr::TransactionBuilder txn;
  txn.SetLowPriority(!IsRootWebRenderBridgeParent());
  if (!aResourceUpdates.IsEmpty()) {
    scheduleComposite = true;
  }

  if (!UpdateResources(aResourceUpdates, aSmallShmems, aLargeShmems, txn)) {
    return IPC_FAIL(this, "Failed to deserialize resource updates");
  }

  if (!aCommands.IsEmpty()) {
    mAsyncImageManager->SetCompositionTime(TimeStamp::Now());
    wr::Epoch wrEpoch = GetNextWrEpoch();
    txn.UpdateEpoch(mPipelineId, wrEpoch);
    if (!ProcessWebRenderParentCommands(aCommands, txn)) {
      return IPC_FAIL(this, "Invalid parent command found");
    }
    if (ShouldParentObserveEpoch()) {
      txn.Notify(
        wr::Checkpoint::SceneBuilt,
        MakeUnique<ScheduleObserveLayersUpdate>(
          mCompositorBridge,
          GetLayersId(),
          mChildLayersObserverEpoch,
          true
        )
      );
    }

    scheduleComposite = true;
  }

  if (!txn.IsEmpty()) {
    mApi->SendTransaction(txn);
  }

  bool sendDidComposite = true;
  if (scheduleComposite || !mPendingTransactionIds.empty()) {
    // If we are going to kick off a new composite as a result of this
    // transaction, or if there are already composite-triggering pending
    // transactions inflight, then set sendDidComposite to false because we will
    // send the DidComposite message after the composite occurs.
    // If there are no pending transactions and we're not going to do a
    // composite, then we leave sendDidComposite as true so we just send
    // the DidComposite notification now.
    sendDidComposite = false;
  }

  // Only register a value for CONTENT_FRAME_TIME telemetry if we actually drew
  // something. It is for consistency with disabling WebRender.
  HoldPendingTransactionId(mWrEpoch,
                           aTransactionId,
                           false,
                           aRefreshStartTime,
                           aTxnStartTime,
                           aFwdTime,
                           /* aIsFirstPaint */false,
                           /* aUseForTelemetry */scheduleComposite);

  if (scheduleComposite) {
    ScheduleGenerateFrame();
  } else if (sendDidComposite) {
    // The only thing in the pending transaction id queue should be the entry
    // we just added, and now we're going to pretend we rendered it
    MOZ_ASSERT(mPendingTransactionIds.size() == 1);
    if (CompositorBridgeParent* cbp = GetRootCompositorBridgeParent()) {
      TimeStamp now = TimeStamp::Now();
      cbp->NotifyPipelineRendered(mPipelineId, mWrEpoch, now, now);
    }
  }

  wr::IpcResourceUpdateQueue::ReleaseShmems(this, aSmallShmems);
  wr::IpcResourceUpdateQueue::ReleaseShmems(this, aLargeShmems);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvSetFocusTarget(const FocusTarget& aFocusTarget)
{
  UpdateAPZFocusState(aFocusTarget);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvParentCommands(nsTArray<WebRenderParentCommand>&& aCommands)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  wr::TransactionBuilder txn;
  txn.SetLowPriority(!IsRootWebRenderBridgeParent());
  if (!ProcessWebRenderParentCommands(aCommands, txn)) {
    return IPC_FAIL(this, "Invalid parent command found");
  }
  mApi->SendTransaction(txn);
  return IPC_OK();
}

bool
WebRenderBridgeParent::ProcessWebRenderParentCommands(const InfallibleTArray<WebRenderParentCommand>& aCommands,
                                                      wr::TransactionBuilder& aTxn)
{
  // Transaction for async image pipeline that uses ImageBridge always need to be non low priority.
  wr::TransactionBuilder txnForImageBridge;
  wr::AutoTransactionSender sender(mApi, &txnForImageBridge);

  for (InfallibleTArray<WebRenderParentCommand>::index_type i = 0; i < aCommands.Length(); ++i) {
    const WebRenderParentCommand& cmd = aCommands[i];
    switch (cmd.type()) {
      case WebRenderParentCommand::TOpAddPipelineIdForCompositable: {
        const OpAddPipelineIdForCompositable& op = cmd.get_OpAddPipelineIdForCompositable();
        AddPipelineIdForCompositable(op.pipelineId(),
                                     op.handle(),
                                     op.isAsync(),
                                     aTxn,
                                     txnForImageBridge);
        break;
      }
      case WebRenderParentCommand::TOpRemovePipelineIdForCompositable: {
        const OpRemovePipelineIdForCompositable& op = cmd.get_OpRemovePipelineIdForCompositable();
        RemovePipelineIdForCompositable(op.pipelineId(), aTxn);
        break;
      }
      case WebRenderParentCommand::TOpReleaseTextureOfImage: {
        const OpReleaseTextureOfImage& op = cmd.get_OpReleaseTextureOfImage();
        ReleaseTextureOfImage(op.key());
        break;
      }
      case WebRenderParentCommand::TOpUpdateAsyncImagePipeline: {
        const OpUpdateAsyncImagePipeline& op = cmd.get_OpUpdateAsyncImagePipeline();
        mAsyncImageManager->UpdateAsyncImagePipeline(op.pipelineId(),
                                                     op.scBounds(),
                                                     op.scTransform(),
                                                     op.scaleToSize(),
                                                     op.filter(),
                                                     op.mixBlendMode());
        mAsyncImageManager->ApplyAsyncImageForPipeline(op.pipelineId(), aTxn, txnForImageBridge);
        break;
      }
      case WebRenderParentCommand::TOpUpdatedAsyncImagePipeline: {
        const OpUpdatedAsyncImagePipeline& op = cmd.get_OpUpdatedAsyncImagePipeline();
        mAsyncImageManager->ApplyAsyncImageForPipeline(op.pipelineId(), aTxn, txnForImageBridge);
        break;
      }
      case WebRenderParentCommand::TCompositableOperation: {
        if (!ReceiveCompositableUpdate(cmd.get_CompositableOperation())) {
          NS_ERROR("ReceiveCompositableUpdate failed");
        }
        break;
      }
      case WebRenderParentCommand::TOpAddCompositorAnimations: {
        const OpAddCompositorAnimations& op = cmd.get_OpAddCompositorAnimations();
        CompositorAnimations data(std::move(op.data()));
        // AnimationHelper::GetNextCompositorAnimationsId() encodes the child process PID
        // in the upper 32 bits of the id, verify that this is as expected.
        if ((data.id() >> 32) != (uint64_t)OtherPid()) {
          return false;
        }
        if (data.animations().Length()) {
          mAnimStorage->SetAnimations(data.id(), data.animations());
          mActiveAnimations.insert(data.id());
        }
        break;
      }
      default: {
        // other commands are handle on the child
        break;
      }
    }
  }
  return true;
}

void
WebRenderBridgeParent::FlushSceneBuilds()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  // Since we are sending transactions through the scene builder thread, we need
  // to block until all the inflight transactions have been processed. This
  // flush message blocks until all previously sent scenes have been built
  // and received by the render backend thread.
  mApi->FlushSceneBuilder();
  // The post-swap hook for async-scene-building calls the
  // ScheduleRenderOnCompositorThread function from the scene builder thread,
  // which then triggers a call to ScheduleGenerateFrame() on the compositor
  // thread. But since *this* function is running on the compositor thread,
  // that scheduling will not happen until this call stack unwinds (or we
  // could spin a nested event loop, but that's more messy). Instead, we
  // simulate it ourselves by calling ScheduleGenerateFrame() directly.
  // Note also that the post-swap hook will run and do another
  // ScheduleGenerateFrame() after we unwind here, so we will end up with an
  // extra render/composite that is probably avoidable, but in practice we
  // shouldn't be calling this function all that much in production so this
  // is probably fine. If it becomes an issue we can add more state tracking
  // machinery to optimize it away.
  ScheduleGenerateFrame();
}

void
WebRenderBridgeParent::FlushFrameGeneration()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  MOZ_ASSERT(IsRootWebRenderBridgeParent()); // This function is only useful on the root WRBP

  // This forces a new GenerateFrame transaction to be sent to the render
  // backend thread, if one is pending. This doesn't block on any other threads.
  if (mCompositorScheduler->NeedsComposite()) {
    mCompositorScheduler->CancelCurrentCompositeTask();
    // Update timestamp of scheduler for APZ and animation.
    mCompositorScheduler->UpdateLastComposeTime();
    MaybeGenerateFrame(/* aForceGenerateFrame */ true);
  }
}

void
WebRenderBridgeParent::FlushFramePresentation()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  // This sends a message to the render backend thread to send a message
  // to the renderer thread, and waits for that message to be processed. So
  // this effectively blocks on the render backend and renderer threads,
  // following the same codepath that WebRender takes to render and composite
  // a frame.
  mApi->WaitFlushed();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvGetSnapshot(PTextureParent* aTexture)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  MOZ_ASSERT(!mPaused);

  // This function should only get called in the root WRBP. If this function
  // gets called in a non-root WRBP, we will set mForceRendering in this WRBP
  // but it will have no effect because CompositeToTarget (which reads the
  // flag) only gets invoked in the root WRBP. So we assert that this is the
  // root WRBP (i.e. has a non-null mWidget) to catch violations of this rule.
  MOZ_ASSERT(IsRootWebRenderBridgeParent());

  RefPtr<TextureHost> texture = TextureHost::AsTextureHost(aTexture);
  if (!texture) {
    // We kill the content process rather than have it continue with an invalid
    // snapshot, that may be too harsh and we could decide to return some sort
    // of error to the child process and let it deal with it...
    return IPC_FAIL_NO_REASON(this);
  }

  // XXX Add other TextureHost supports.
  // Only BufferTextureHost is supported now.
  BufferTextureHost* bufferTexture = texture->AsBufferTextureHost();
  if (!bufferTexture) {
    // We kill the content process rather than have it continue with an invalid
    // snapshot, that may be too harsh and we could decide to return some sort
    // of error to the child process and let it deal with it...
    return IPC_FAIL_NO_REASON(this);
  }

  TimeStamp start = TimeStamp::Now();
  MOZ_ASSERT(bufferTexture->GetBufferDescriptor().type() == BufferDescriptor::TRGBDescriptor);
  DebugOnly<uint32_t> stride = ImageDataSerializer::GetRGBStride(bufferTexture->GetBufferDescriptor().get_RGBDescriptor());
  uint8_t* buffer = bufferTexture->GetBuffer();
  IntSize size = bufferTexture->GetSize();

  // We only support B8G8R8A8 for now.
  MOZ_ASSERT(buffer);
  MOZ_ASSERT(bufferTexture->GetFormat() == SurfaceFormat::B8G8R8A8);
  uint32_t buffer_size = size.width * size.height * 4;

  // Assert the stride of the buffer is what webrender expects
  MOZ_ASSERT((uint32_t)(size.width * 4) == stride);

  FlushSceneBuilds();
  FlushFrameGeneration();
  mApi->Readback(start, size, Range<uint8_t>(buffer, buffer_size));

  return IPC_OK();
}

void
WebRenderBridgeParent::AddPipelineIdForCompositable(const wr::PipelineId& aPipelineId,
                                                    const CompositableHandle& aHandle,
                                                    const bool& aAsync,
                                                    wr::TransactionBuilder& aTxn,
                                                    wr::TransactionBuilder& aTxnForImageBridge)
{
  if (mDestroyed) {
    return;
  }

  MOZ_ASSERT(mAsyncCompositables.find(wr::AsUint64(aPipelineId)) == mAsyncCompositables.end());

  RefPtr<CompositableHost> host;
  if (aAsync) {
    RefPtr<ImageBridgeParent> imageBridge = ImageBridgeParent::GetInstance(OtherPid());
    if (!imageBridge) {
       return;
    }
    host = imageBridge->FindCompositable(aHandle);
  } else {
    host = FindCompositable(aHandle);
  }
  if (!host) {
    return;
  }

  WebRenderImageHost* wrHost = host->AsWebRenderImageHost();
  MOZ_ASSERT(wrHost);
  if (!wrHost) {
    gfxCriticalNote << "Incompatible CompositableHost at WebRenderBridgeParent.";
  }

  if (!wrHost) {
    return;
  }

  wrHost->SetWrBridge(this);
  wrHost->EnableUseAsyncImagePipeline();
  mAsyncCompositables.emplace(wr::AsUint64(aPipelineId), wrHost);
  mAsyncImageManager->AddAsyncImagePipeline(aPipelineId, wrHost);

  // If this is being called from WebRenderBridgeParent::RecvSetDisplayList,
  // then aTxn might contain a display list that references pipelines that
  // we just added to the async image manager.
  // If we send the display list alone then WR will not yet have the content for
  // the pipelines and so it will emit errors; the SetEmptyDisplayList call
  // below ensure that we provide its content to WR as part of the same transaction.
  mAsyncImageManager->SetEmptyDisplayList(aPipelineId, aTxn, aTxnForImageBridge);
  return;
}

void
WebRenderBridgeParent::RemovePipelineIdForCompositable(const wr::PipelineId& aPipelineId,
                                                       wr::TransactionBuilder& aTxn)
{
  if (mDestroyed) {
    return;
  }

  auto it = mAsyncCompositables.find(wr::AsUint64(aPipelineId));
  if (it == mAsyncCompositables.end()) {
    return;
  }
  RefPtr<WebRenderImageHost>& wrHost = it->second;

  wrHost->ClearWrBridge();
  mAsyncImageManager->RemoveAsyncImagePipeline(aPipelineId, aTxn);
  aTxn.RemovePipeline(aPipelineId);
  mAsyncCompositables.erase(wr::AsUint64(aPipelineId));
  return;
}

void
WebRenderBridgeParent::DeleteImage(const ImageKey& aKey,
                                   wr::TransactionBuilder& aUpdates)
{
  if (mDestroyed) {
    return;
  }

  auto it = mSharedSurfaceIds.find(wr::AsUint64(aKey));
  if (it != mSharedSurfaceIds.end()) {
    mAsyncImageManager->HoldExternalImage(mPipelineId, mWrEpoch, it->second);
    mSharedSurfaceIds.erase(it);
  }

  aUpdates.DeleteImage(aKey);
}

void
WebRenderBridgeParent::ReleaseTextureOfImage(const wr::ImageKey& aKey)
{
  if (mDestroyed) {
    return;
  }

  uint64_t id = wr::AsUint64(aKey);
  CompositableTextureHostRef texture;
  WebRenderTextureHost* wrTexture = nullptr;

  auto it = mTextureHosts.find(id);
  if (it != mTextureHosts.end()) {
    wrTexture = (*it).second->AsWebRenderTextureHost();
  }
  if (wrTexture) {
    mAsyncImageManager->HoldExternalImage(mPipelineId, mWrEpoch, wrTexture);
  }
  mTextureHosts.erase(id);
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvSetLayersObserverEpoch(const LayersObserverEpoch& aChildEpoch)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  mChildLayersObserverEpoch = aChildEpoch;
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvClearCachedResources()
{
  if (mDestroyed) {
    return IPC_OK();
  }

  // Clear resources
  wr::TransactionBuilder txn;
  txn.SetLowPriority(true);
  txn.ClearDisplayList(GetNextWrEpoch(), mPipelineId);
  txn.Notify(
    wr::Checkpoint::SceneBuilt,
    MakeUnique<ScheduleObserveLayersUpdate>(
      mCompositorBridge,
      GetLayersId(),
      mChildLayersObserverEpoch,
      false
    )
  );

  mApi->SendTransaction(txn);
  // Schedule generate frame to clean up Pipeline
  ScheduleGenerateFrame();
  // Remove animations.
  for (const auto& id : mActiveAnimations) {
    mAnimStorage->ClearById(id);
  }
  mActiveAnimations.clear();
  std::queue<CompositorAnimationIdsForEpoch>().swap(mCompositorAnimationsToDelete); // clear queue
  return IPC_OK();
}

wr::Epoch
WebRenderBridgeParent::UpdateWebRender(CompositorVsyncScheduler* aScheduler,
                                       wr::WebRenderAPI* aApi,
                                       AsyncImagePipelineManager* aImageMgr,
                                       CompositorAnimationStorage* aAnimStorage,
                                       const TextureFactoryIdentifier& aTextureFactoryIdentifier)
{
  MOZ_ASSERT(!IsRootWebRenderBridgeParent());
  MOZ_ASSERT(aScheduler);
  MOZ_ASSERT(aApi);
  MOZ_ASSERT(aImageMgr);
  MOZ_ASSERT(aAnimStorage);

  if (mDestroyed) {
    return mWrEpoch;
  }

  // Update id name space to identify obsoleted keys.
  // Since usage of invalid keys could cause crash in webrender.
  mIdNamespace = aApi->GetNamespace();
  // XXX Remove it when webrender supports sharing/moving Keys between different webrender instances.
  // XXX It requests client to update/reallocate webrender related resources,
  // but parent side does not wait end of the update.
  // The code could become simpler if we could serialise old keys deallocation and new keys allocation.
  // But we do not do it, it is because client side deallocate old layers/webrender keys
  // after new layers/webrender keys allocation.
  // Without client side's layout refactoring, we could not finish all old layers/webrender keys removals
  // before new layer/webrender keys allocation. In future, we could address the problem.
  Unused << SendWrUpdated(mIdNamespace, aTextureFactoryIdentifier);
  CompositorBridgeParentBase* cBridge = mCompositorBridge;
  // XXX Stop to clear resources if webreder supports resources sharing between different webrender instances.
  ClearResources();
  mCompositorBridge = cBridge;
  mCompositorScheduler = aScheduler;
  mApi = aApi;
  mAsyncImageManager = aImageMgr;
  mAnimStorage = aAnimStorage;

  // Register pipeline to updated AsyncImageManager.
  mAsyncImageManager->AddPipeline(mPipelineId);

  return GetNextWrEpoch(); // Update webrender epoch
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvScheduleComposite()
{
  ScheduleGenerateFrame();
  return IPC_OK();
}

void
WebRenderBridgeParent::ScheduleForcedGenerateFrame()
{
  if (mDestroyed) {
    return;
  }

  wr::TransactionBuilder fastTxn(/* aUseSceneBuilderThread */ false);
  fastTxn.InvalidateRenderedFrame();
  mApi->SendTransaction(fastTxn);

  ScheduleGenerateFrame();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvCapture()
{
  if (!mDestroyed) {
    mApi->Capture();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvSyncWithCompositor()
{
  FlushSceneBuilds();
  if (RefPtr<WebRenderBridgeParent> root = GetRootWebRenderBridgeParent()) {
    root->FlushFrameGeneration();
  }
  FlushFramePresentation();
  // Finally, we force the AsyncImagePipelineManager to handle all the
  // pipeline updates produced in the last step, so that it frees any
  // unneeded textures. Then we can return from this sync IPC call knowing
  // that we've done everything we can to flush stuff on the compositor.
  mAsyncImageManager->ProcessPipelineUpdates();

  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvSetConfirmedTargetAPZC(const uint64_t& aBlockId,
                                                  nsTArray<ScrollableLayerGuid>&& aTargets)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  mCompositorBridge->SetConfirmedTargetAPZC(GetLayersId(), aBlockId, aTargets);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvSetTestSampleTime(const TimeStamp& aTime)
{
  if (!mCompositorBridge->SetTestSampleTime(GetLayersId(), aTime)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvLeaveTestMode()
{
  mCompositorBridge->LeaveTestMode(GetLayersId());
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvGetAnimationValue(const uint64_t& aCompositorAnimationsId,
                                             OMTAValue* aValue)
{
  if (mDestroyed) {
    return IPC_FAIL_NO_REASON(this);
  }

  MOZ_ASSERT(mAnimStorage);
  if (RefPtr<WebRenderBridgeParent> root = GetRootWebRenderBridgeParent()) {
    root->AdvanceAnimations();
  } else {
    AdvanceAnimations();
  }

  *aValue = mAnimStorage->GetOMTAValue(aCompositorAnimationsId);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvSetAsyncScrollOffset(const ScrollableLayerGuid::ViewID& aScrollId,
                                                const float& aX,
                                                const float& aY)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  mCompositorBridge->SetTestAsyncScrollOffset(GetLayersId(), aScrollId, CSSPoint(aX, aY));
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvSetAsyncZoom(const ScrollableLayerGuid::ViewID& aScrollId,
                                        const float& aZoom)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  mCompositorBridge->SetTestAsyncZoom(GetLayersId(), aScrollId, LayerToParentLayerScale(aZoom));
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvFlushApzRepaints()
{
  if (mDestroyed) {
    return IPC_OK();
  }
  mCompositorBridge->FlushApzRepaints(GetLayersId());
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvGetAPZTestData(APZTestData* aOutData)
{
  mCompositorBridge->GetAPZTestData(GetLayersId(), aOutData);
  return IPC_OK();
}

void
WebRenderBridgeParent::ActorDestroy(ActorDestroyReason aWhy)
{
  Destroy();
}

bool
WebRenderBridgeParent::AdvanceAnimations()
{
  if (CompositorBridgeParent* cbp = GetRootCompositorBridgeParent()) {
    Maybe<TimeStamp> testingTimeStamp = cbp->GetTestingTimeStamp();
    if (testingTimeStamp) {
      // If we are on testing refresh mode, use the testing time stamp.  And
      // also we don't update mPreviousFrameTimeStamp since unlike normal
      // refresh mode, on the testing mode animations on the compositor are
      // synchronously composed, so we don't need to worry about the time gap
      // between the main thread and compositor thread.
      return AnimationHelper::SampleAnimations(mAnimStorage,
                                               *testingTimeStamp,
                                               *testingTimeStamp);
    }
  }

  TimeStamp lastComposeTime = mCompositorScheduler->GetLastComposeTime();
  const bool isAnimating =
    AnimationHelper::SampleAnimations(mAnimStorage,
                                      mPreviousFrameTimeStamp,
                                      lastComposeTime);

  // Reset the previous time stamp if we don't already have any running
  // animations to avoid using the time which is far behind for newly
  // started animations.
  mPreviousFrameTimeStamp = isAnimating ? lastComposeTime : TimeStamp();

  return isAnimating;
}

bool
WebRenderBridgeParent::SampleAnimations(nsTArray<wr::WrOpacityProperty>& aOpacityArray,
                                        nsTArray<wr::WrTransformProperty>& aTransformArray)
{
  const bool isAnimating = AdvanceAnimations();

  // return the animated data if has
  if (mAnimStorage->AnimatedValueCount()) {
    for(auto iter = mAnimStorage->ConstAnimatedValueTableIter();
        !iter.Done(); iter.Next()) {
      AnimatedValue * value = iter.UserData();
      if (value->mType == AnimatedValue::TRANSFORM) {
        aTransformArray.AppendElement(
          wr::ToWrTransformProperty(iter.Key(), value->mTransform.mTransformInDevSpace));
      } else if (value->mType == AnimatedValue::OPACITY) {
        aOpacityArray.AppendElement(
          wr::ToWrOpacityProperty(iter.Key(), value->mOpacity));
      }
    }
  }

  return isAnimating;
}

void
WebRenderBridgeParent::CompositeToTarget(gfx::DrawTarget* aTarget, const gfx::IntRect* aRect)
{
  // This function should only get called in the root WRBP
  MOZ_ASSERT(IsRootWebRenderBridgeParent());

  // The two arguments are part of the CompositorVsyncSchedulerOwner API but in
  // this implementation they should never be non-null.
  MOZ_ASSERT(aTarget == nullptr);
  MOZ_ASSERT(aRect == nullptr);

  AUTO_PROFILER_TRACING("Paint", "CompositeToTarget");
  if (mPaused || !mReceivedDisplayList) {
    mPreviousFrameTimeStamp = TimeStamp();
    return;
  }

  if (wr::RenderThread::Get()->TooManyPendingFrames(mApi->GetId())) {
    // Render thread is busy, try next time.
    mCompositorScheduler->ScheduleComposition();
    mPreviousFrameTimeStamp = TimeStamp();
    return;
  }
  MaybeGenerateFrame(/* aForceGenerateFrame */ false);
}

TimeDuration
WebRenderBridgeParent::GetVsyncInterval() const
{
  // This function should only get called in the root WRBP
  MOZ_ASSERT(IsRootWebRenderBridgeParent());
  if (CompositorBridgeParent* cbp = GetRootCompositorBridgeParent()) {
    return cbp->GetVsyncInterval();
  }
  return TimeDuration();
}

void
WebRenderBridgeParent::MaybeGenerateFrame(bool aForceGenerateFrame)
{
  // This function should only get called in the root WRBP
  MOZ_ASSERT(IsRootWebRenderBridgeParent());

  TimeStamp start = TimeStamp::Now();
  mAsyncImageManager->SetCompositionTime(start);

  // Ensure GenerateFrame is handled on the render backend thread rather
  // than going through the scene builder thread. That way we continue generating
  // frames with the old scene even during slow scene builds.
  bool useSceneBuilderThread = false;
  wr::TransactionBuilder fastTxn(useSceneBuilderThread);

  // Handle transaction that is related to DisplayList.
  wr::TransactionBuilder sceneBuilderTxn;
  wr::AutoTransactionSender sender(mApi, &sceneBuilderTxn);

  // Adding and updating wr::ImageKeys of ImageHosts that uses ImageBridge are
  // done without using transaction of scene builder thread. With it, updating of
  // video frame becomes faster.
  mAsyncImageManager->ApplyAsyncImagesOfImageBridge(sceneBuilderTxn, fastTxn);

  if (!mAsyncImageManager->GetCompositeUntilTime().IsNull()) {
    // Trigger another CompositeToTarget() call because there might be another
    // frame that we want to generate after this one.
    // It will check if we actually want to generate the frame or not.
    mCompositorScheduler->ScheduleComposition();
  }

  if (!mAsyncImageManager->GetAndResetWillGenerateFrame() &&
      fastTxn.IsEmpty() &&
      !aForceGenerateFrame) {
    // Could skip generating frame now.
    mPreviousFrameTimeStamp = TimeStamp();
    return;
  }

  nsTArray<wr::WrOpacityProperty> opacityArray;
  nsTArray<wr::WrTransformProperty> transformArray;

  if (SampleAnimations(opacityArray, transformArray)) {
    ScheduleGenerateFrame();
  }
  // We do this even if the arrays are empty, because it will clear out any
  // previous properties store on the WR side, which is desirable.
  fastTxn.UpdateDynamicProperties(opacityArray, transformArray);

  SetAPZSampleTime();

  wr::RenderThread::Get()->IncPendingFrameCount(mApi->GetId(), start);

#if defined(ENABLE_FRAME_LATENCY_LOG)
  auto startTime = TimeStamp::Now();
  mApi->SetFrameStartTime(startTime);
#endif

  fastTxn.GenerateFrame();

  mApi->SendTransaction(fastTxn);
}

void
WebRenderBridgeParent::HoldPendingTransactionId(const wr::Epoch& aWrEpoch,
                                                TransactionId aTransactionId,
                                                bool aContainsSVGGroup,
                                                const TimeStamp& aRefreshStartTime,
                                                const TimeStamp& aTxnStartTime,
                                                const TimeStamp& aFwdTime,
                                                const bool aIsFirstPaint,
                                                const bool aUseForTelemetry)
{
  MOZ_ASSERT(aTransactionId > LastPendingTransactionId());
  mPendingTransactionIds.push(PendingTransactionId(aWrEpoch,
                                                   aTransactionId,
                                                   aContainsSVGGroup,
                                                   aRefreshStartTime,
                                                   aTxnStartTime,
                                                   aFwdTime,
                                                   aIsFirstPaint,
                                                   aUseForTelemetry));
}

TransactionId
WebRenderBridgeParent::LastPendingTransactionId()
{
  TransactionId id{0};
  if (!mPendingTransactionIds.empty()) {
    id = mPendingTransactionIds.back().mId;
  }
  return id;
}

TransactionId
WebRenderBridgeParent::FlushTransactionIdsForEpoch(const wr::Epoch& aEpoch, const TimeStamp& aEndTime,
                                                   UiCompositorControllerParent* aUiController,
                                                   wr::RendererStats* aStats)
{
  TransactionId id{0};
  while (!mPendingTransactionIds.empty()) {
    const auto& transactionId = mPendingTransactionIds.front();

    if (aEpoch.mHandle < transactionId.mEpoch.mHandle) {
      break;
    }

    if (!IsRootWebRenderBridgeParent() && !mVsyncRate.IsZero() && transactionId.mUseForTelemetry) {
      double latencyMs = (aEndTime - transactionId.mTxnStartTime).ToMilliseconds();
      double latencyNorm = latencyMs / mVsyncRate.ToMilliseconds();
      int32_t fracLatencyNorm = lround(latencyNorm * 100.0);

#ifdef MOZ_GECKO_PROFILER
      if (profiler_is_active()) {
        class ContentFramePayload : public ProfilerMarkerPayload {
          public:
            ContentFramePayload(const mozilla::TimeStamp& aStartTime, const mozilla::TimeStamp& aEndTime)
              : ProfilerMarkerPayload(aStartTime, aEndTime)
            {}
            virtual void StreamPayload(SpliceableJSONWriter& aWriter, const TimeStamp& aProcessStartTime, UniqueStacks& aUniqueStacks) override {
              StreamCommonProps("CONTENT_FRAME_TIME", aWriter, aProcessStartTime, aUniqueStacks);
            }
        };
        profiler_add_marker_for_thread(profiler_current_thread_id(), "CONTENT_FRAME_TIME", MakeUnique<ContentFramePayload>(mPendingTransactionIds.front().mTxnStartTime,
                                                                                                                           aEndTime));
      }
#endif

      Telemetry::Accumulate(Telemetry::CONTENT_FRAME_TIME, fracLatencyNorm);
      if (fracLatencyNorm > 200) {
        wr::RenderThread::Get()->NotifySlowFrame(mApi->GetId());
      }
      if (transactionId.mContainsSVGGroup) {
        Telemetry::Accumulate(Telemetry::CONTENT_FRAME_TIME_WITH_SVG, fracLatencyNorm);
      }

      if (aStats) {
        latencyMs -= (double(aStats->resource_upload_time) / 1000000.0);
        latencyNorm = latencyMs / mVsyncRate.ToMilliseconds();
        fracLatencyNorm = lround(latencyNorm * 100.0);
      }
      Telemetry::Accumulate(Telemetry::CONTENT_FRAME_TIME_WITHOUT_RESOURCE_UPLOAD, fracLatencyNorm);

      if (aStats) {
        latencyMs -= (double(aStats->gpu_cache_upload_time) / 1000000.0);
        latencyNorm = latencyMs / mVsyncRate.ToMilliseconds();
        fracLatencyNorm = lround(latencyNorm * 100.0);
      }
      Telemetry::Accumulate(Telemetry::CONTENT_FRAME_TIME_WITHOUT_UPLOAD, fracLatencyNorm);
    }

#if defined(ENABLE_FRAME_LATENCY_LOG)
    if (transactionId.mRefreshStartTime) {
      int32_t latencyMs = lround((aEndTime - transactionId.mRefreshStartTime).ToMilliseconds());
      printf_stderr("From transaction start to end of generate frame latencyMs %d this %p\n", latencyMs, this);
    }
    if (transactionId.mFwdTime) {
      int32_t latencyMs = lround((aEndTime - transactionId.mFwdTime).ToMilliseconds());
      printf_stderr("From forwarding transaction to end of generate frame latencyMs %d this %p\n", latencyMs, this);
    }
#endif

    if (aUiController && transactionId.mIsFirstPaint) {
      aUiController->NotifyFirstPaint();
    }

    id = transactionId.mId;
    mPendingTransactionIds.pop();
  }
  return id;
}

LayersId
WebRenderBridgeParent::GetLayersId() const
{
  return wr::AsLayersId(mPipelineId);
}

void
WebRenderBridgeParent::ScheduleGenerateFrame()
{
  if (mCompositorScheduler) {
    mAsyncImageManager->SetWillGenerateFrame();
    mCompositorScheduler->ScheduleComposition();
  }
}

void
WebRenderBridgeParent::FlushRendering(bool aWaitForPresent)
{
  if (mDestroyed) {
    return;
  }

  // This gets called during e.g. window resizes, so we need to flush the
  // scene (which has the display list at the new window size).
  FlushSceneBuilds();
  FlushFrameGeneration();
  if (aWaitForPresent) {
    FlushFramePresentation();
  }
}

void
WebRenderBridgeParent::Pause()
{
  MOZ_ASSERT(IsRootWebRenderBridgeParent());
#ifdef MOZ_WIDGET_ANDROID
  if (!IsRootWebRenderBridgeParent() || mDestroyed) {
    return;
  }
  mApi->Pause();
#endif
  mPaused = true;
}

bool
WebRenderBridgeParent::Resume()
{
  MOZ_ASSERT(IsRootWebRenderBridgeParent());
#ifdef MOZ_WIDGET_ANDROID
  if (!IsRootWebRenderBridgeParent() || mDestroyed) {
    return false;
  }

  if (!mApi->Resume()) {
    return false;
  }
#endif
  mPaused = false;
  return true;
}

void
WebRenderBridgeParent::ClearResources()
{
  if (!mApi) {
    return;
  }

  wr::Epoch wrEpoch = GetNextWrEpoch();

  wr::TransactionBuilder txn;
  txn.SetLowPriority(true);
  txn.ClearDisplayList(wrEpoch, mPipelineId);
  mReceivedDisplayList = false;

  // Schedule generate frame to clean up Pipeline
  ScheduleGenerateFrame();
  // WrFontKeys and WrImageKeys are deleted during WebRenderAPI destruction.
  for (const auto& entry : mTextureHosts) {
    WebRenderTextureHost* wrTexture = entry.second->AsWebRenderTextureHost();
    MOZ_ASSERT(wrTexture);
    if (wrTexture) {
      mAsyncImageManager->HoldExternalImage(mPipelineId, wrEpoch, wrTexture);
    }
  }
  mTextureHosts.clear();
  for (const auto& entry : mAsyncCompositables) {
    wr::PipelineId pipelineId = wr::AsPipelineId(entry.first);
    RefPtr<WebRenderImageHost> host = entry.second;
    host->ClearWrBridge();
    mAsyncImageManager->RemoveAsyncImagePipeline(pipelineId, txn);
    txn.RemovePipeline(pipelineId);
  }
  mAsyncCompositables.clear();
  for (const auto& entry : mSharedSurfaceIds) {
    mAsyncImageManager->HoldExternalImage(mPipelineId, mWrEpoch, entry.second);
  }
  mSharedSurfaceIds.clear();

  mAsyncImageManager->RemovePipeline(mPipelineId, wrEpoch);
  txn.RemovePipeline(mPipelineId);

  mApi->SendTransaction(txn);

  for (const auto& id : mActiveAnimations) {
    mAnimStorage->ClearById(id);
  }
  mActiveAnimations.clear();
  std::queue<CompositorAnimationIdsForEpoch>().swap(mCompositorAnimationsToDelete); // clear queue

  if (IsRootWebRenderBridgeParent()) {
    mCompositorScheduler->Destroy();
  }

  mAnimStorage = nullptr;
  mCompositorScheduler = nullptr;
  mAsyncImageManager = nullptr;
  mApi = nullptr;
  mCompositorBridge = nullptr;
}

bool
WebRenderBridgeParent::ShouldParentObserveEpoch()
{
  if (mParentLayersObserverEpoch == mChildLayersObserverEpoch) {
    return false;
  }

  mParentLayersObserverEpoch = mChildLayersObserverEpoch;
  return true;
}

void
WebRenderBridgeParent::SendAsyncMessage(const InfallibleTArray<AsyncParentMessageData>& aMessage)
{
  MOZ_ASSERT_UNREACHABLE("unexpected to be called");
}

void
WebRenderBridgeParent::SendPendingAsyncMessages()
{
  MOZ_ASSERT(mCompositorBridge);
  mCompositorBridge->SendPendingAsyncMessages();
}

void
WebRenderBridgeParent::SetAboutToSendAsyncMessages()
{
  MOZ_ASSERT(mCompositorBridge);
  mCompositorBridge->SetAboutToSendAsyncMessages();
}

void
WebRenderBridgeParent::NotifyNotUsed(PTextureParent* aTexture, uint64_t aTransactionId)
{
  MOZ_ASSERT_UNREACHABLE("unexpected to be called");
}

base::ProcessId
WebRenderBridgeParent::GetChildProcessId()
{
  return OtherPid();
}

bool
WebRenderBridgeParent::IsSameProcess() const
{
  return OtherPid() == base::GetCurrentProcId();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvNewCompositable(const CompositableHandle& aHandle,
                                           const TextureInfo& aInfo)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  if (!AddCompositable(aHandle, aInfo, /* aUseWebRender */ true)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvReleaseCompositable(const CompositableHandle& aHandle)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  ReleaseCompositable(aHandle);
  return IPC_OK();
}

TextureFactoryIdentifier
WebRenderBridgeParent::GetTextureFactoryIdentifier()
{
  MOZ_ASSERT(mApi);

  return TextureFactoryIdentifier(LayersBackend::LAYERS_WR,
                                  XRE_GetProcessType(),
                                  mApi->GetMaxTextureSize(),
                                  false,
                                  mApi->GetUseANGLE(),
                                  mApi->GetUseDComp(),
                                  false,
                                  false,
                                  false,
                                  mApi->GetSyncHandle());
}

wr::Epoch
WebRenderBridgeParent::GetNextWrEpoch()
{
  MOZ_RELEASE_ASSERT(mWrEpoch.mHandle != UINT32_MAX);
  mWrEpoch.mHandle++;
  return mWrEpoch;
}

void
WebRenderBridgeParent::ExtractImageCompositeNotifications(nsTArray<ImageCompositeNotificationInfo>* aNotifications)
{
  MOZ_ASSERT(IsRootWebRenderBridgeParent());
  if (mDestroyed) {
    return;
  }
  mAsyncImageManager->FlushImageNotifications(aNotifications);
}

} // namespace layers
} // namespace mozilla
