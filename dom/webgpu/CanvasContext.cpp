/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "CanvasContext.h"
#include "gfxUtils.h"
#include "LayerUserData.h"
#include "nsDisplayList.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/CanvasManagerChild.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/CanvasRenderer.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/layers/RenderRootStateManager.h"
#include "mozilla/layers/WebRenderCanvasRenderer.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/SVGObserverUtils.h"
#include "ipc/WebGPUChild.h"
#include "Utility.h"

namespace mozilla {

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    dom::GPUCanvasConfiguration& aField, const char* aName, uint32_t aFlags) {
  aField.TraverseForCC(aCallback, aFlags);
}

inline void ImplCycleCollectionUnlink(dom::GPUCanvasConfiguration& aField) {
  aField.UnlinkForCC();
}

// -

template <class T>
inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    const std::unique_ptr<T>& aField, const char* aName, uint32_t aFlags) {
  if (aField) {
    ImplCycleCollectionTraverse(aCallback, *aField, aName, aFlags);
  }
}

template <class T>
inline void ImplCycleCollectionUnlink(std::unique_ptr<T>& aField) {
  aField = nullptr;
}

}  // namespace mozilla

// -

namespace mozilla::webgpu {

NS_IMPL_CYCLE_COLLECTING_ADDREF(CanvasContext)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CanvasContext)

GPU_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_WEAK_PTR(CanvasContext, mConfiguration,
                                                mCurrentTexture, mBridge,
                                                mCanvasElement,
                                                mOffscreenCanvas)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CanvasContext)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextInternal)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

// -

CanvasContext::CanvasContext() = default;

CanvasContext::~CanvasContext() {
  Cleanup();
  RemovePostRefreshObserver();
}

void CanvasContext::Cleanup() { Unconfigure(); }

JSObject* CanvasContext::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return dom::GPUCanvasContext_Binding::Wrap(aCx, this, aGivenProto);
}

// -

void CanvasContext::GetCanvas(
    dom::OwningHTMLCanvasElementOrOffscreenCanvas& aRetVal) const {
  if (mCanvasElement) {
    aRetVal.SetAsHTMLCanvasElement() = mCanvasElement;
  } else if (mOffscreenCanvas) {
    aRetVal.SetAsOffscreenCanvas() = mOffscreenCanvas;
  } else {
    MOZ_CRASH(
        "This should only happen briefly during CC Unlink, and no JS should "
        "happen then.");
  }
}

// Note: `SetDimensions` assumes it can ignore this `ErrorResult` because the
// format is already validated. Revisit if adding other error cases.
void CanvasContext::Configure(const dom::GPUCanvasConfiguration& aConfig,
                              ErrorResult& aRv) {
  Unconfigure();

  // Only the three formats explicitly listed are permitted here (one of which
  // is not yet supported).
  // https://www.w3.org/TR/webgpu/#supported-context-formats
  switch (aConfig.mFormat) {
    case dom::GPUTextureFormat::Rgba8unorm:
      mGfxFormat = gfx::SurfaceFormat::R8G8B8A8;
      break;
    case dom::GPUTextureFormat::Bgra8unorm:
      mGfxFormat = gfx::SurfaceFormat::B8G8R8A8;
      break;
    case dom::GPUTextureFormat::Rgba16float:
      aRv.ThrowTypeError(
          "Canvas texture format `rgba16float` is not yet supported. "
          "Subscribe to <https://bugzilla.mozilla.org/show_bug.cgi?id=1967329>"
          " for updates on its development in Firefox.");
      return;
    default:
      aRv.ThrowTypeError(
          nsPrintfCString("`%s` is not a supported context format.",
                          dom::GetEnumString(aConfig.mFormat).get()));
      return;
  }

  mConfiguration.reset(new dom::GPUCanvasConfiguration(aConfig));
  mRemoteTextureOwnerId = Some(layers::RemoteTextureOwnerId::GetNext());
  mUseExternalTextureInSwapChain =
      aConfig.mDevice->mSupportExternalTextureInSwapChain;
  if (mUseExternalTextureInSwapChain) {
    bool client_can_use = wgpu_client_use_external_texture_in_swapChain(
        ConvertTextureFormat(aConfig.mFormat));
    if (!client_can_use) {
      gfxCriticalNote << "WebGPU: disabling ExternalTexture swapchain: \n"
                         "canvas configuration format not supported";
      mUseExternalTextureInSwapChain = false;
    }
  }
  if (!gfx::gfxVars::AllowWebGPUPresentWithoutReadback()) {
    gfxCriticalNote
        << "WebGPU: disabling ExternalTexture swapchain: \n"
           "`dom.webgpu.allow-present-without-readback` pref is false";
    mUseExternalTextureInSwapChain = false;
  }
#ifdef XP_WIN
  // When WebRender does not use hardware acceleration, disable external texture
  // in swap chain. Since compositor device might not exist.
  if (gfx::gfxVars::UseSoftwareWebRender() &&
      !gfx::gfxVars::AllowSoftwareWebRenderD3D11()) {
    gfxCriticalNote << "WebGPU: disabling ExternalTexture swapchain: \n"
                       "WebRender is not using hardware acceleration";
    mUseExternalTextureInSwapChain = false;
  }
#elif defined(XP_LINUX) && !defined(MOZ_WIDGET_ANDROID)
  // When DMABufDevice is not enabled, disable external texture in swap chain.
  const auto& modifiers = gfx::gfxVars::DMABufModifiersARGB();
  if (modifiers.IsEmpty()) {
    gfxCriticalNote << "WebGPU: disabling ExternalTexture swapchain: \n"
                       "missing GBM_FORMAT_ARGB8888 dmabuf format";
    mUseExternalTextureInSwapChain = false;
  }
#endif

  // buffer count doesn't matter much, will be created on demand
  const size_t maxBufferCount = 10;
  for (size_t i = 0; i < maxBufferCount; ++i) {
    mBufferIds.AppendElement(ffi::wgpu_client_make_buffer_id(
        aConfig.mDevice->GetBridge()->GetClient()));
  }

  mCurrentTexture = aConfig.mDevice->InitSwapChain(
      mConfiguration.get(), mRemoteTextureOwnerId.ref(), mBufferIds,
      mUseExternalTextureInSwapChain, mGfxFormat, mCanvasSize);
  if (!mCurrentTexture) {
    Unconfigure();
    return;
  }

  mCurrentTexture->mTargetContext = this;
  mBridge = aConfig.mDevice->GetBridge();
  if (mCanvasElement) {
    mWaitingCanvasRendererInitialized = true;
  }

  ForceNewFrame();
}

void CanvasContext::Unconfigure() {
  if (mBridge && mBridge->CanSend() && mRemoteTextureOwnerId) {
    mBridge->SendSwapChainDrop(
        *mRemoteTextureOwnerId,
        layers::ToRemoteTextureTxnType(mFwdTransactionTracker),
        layers::ToRemoteTextureTxnId(mFwdTransactionTracker));
    for (auto& id : mBufferIds) {
      ffi::wgpu_client_free_buffer_id(mBridge->GetClient(), id);
    }
  }
  mBufferIds.Clear();
  mRemoteTextureOwnerId = Nothing();
  mFwdTransactionTracker = nullptr;
  mBridge = nullptr;
  mConfiguration = nullptr;
  mCurrentTexture = nullptr;
  mGfxFormat = gfx::SurfaceFormat::UNKNOWN;
}

NS_IMETHODIMP CanvasContext::SetDimensions(int32_t aWidth, int32_t aHeight) {
  const auto newSize = gfx::IntSize{aWidth, aHeight};
  if (newSize == mCanvasSize) return NS_OK;  // No-op no-change resizes.

  mCanvasSize = newSize;
  if (mConfiguration) {
    const auto copy = dom::GPUCanvasConfiguration{
        *mConfiguration};  // So we can't null it out on ourselves.
    // The format in `mConfiguration` was already validated, we won't get an
    // error here.
    Configure(copy, IgnoredErrorResult());
  }
  return NS_OK;
}

void CanvasContext::GetConfiguration(
    dom::Nullable<dom::GPUCanvasConfiguration>& aRv) {
  if (mConfiguration) {
    aRv.SetValue(*mConfiguration);
  } else {
    aRv.SetNull();
  }
}

RefPtr<Texture> CanvasContext::GetCurrentTexture(ErrorResult& aRv) {
  if (!mCurrentTexture) {
    aRv.ThrowInvalidStateError("Canvas not configured");
    return nullptr;
  }

  MOZ_ASSERT(mConfiguration);
  MOZ_ASSERT(mRemoteTextureOwnerId.isSome());

  if (mNewTextureRequested) {
    mNewTextureRequested = false;

    mCurrentTexture = mConfiguration->mDevice->CreateTextureForSwapChain(
        mConfiguration.get(), mCanvasSize, mRemoteTextureOwnerId.ref());
    mCurrentTexture->mTargetContext = this;
  }
  return mCurrentTexture;
}

void CanvasContext::MaybeQueueSwapChainPresent() {
  if (!mConfiguration) {
    return;
  }

  MOZ_ASSERT(mCurrentTexture);

  if (mCurrentTexture) {
    mBridge->NotifyWaitForSubmit(mCurrentTexture->mId);
  }

  if (mPendingSwapChainPresent) {
    return;
  }

  mPendingSwapChainPresent = true;

  if (mWaitingCanvasRendererInitialized) {
    return;
  }

  InvalidateCanvasContent();
}

Maybe<layers::SurfaceDescriptor> CanvasContext::SwapChainPresent() {
  mPendingSwapChainPresent = false;
  if (!mBridge || !mBridge->CanSend() || mRemoteTextureOwnerId.isNothing() ||
      !mCurrentTexture) {
    return Nothing();
  }
  mLastRemoteTextureId = Some(layers::RemoteTextureId::GetNext());
  mBridge->SwapChainPresent(mCurrentTexture->mId, *mLastRemoteTextureId,
                            *mRemoteTextureOwnerId);
  if (mUseExternalTextureInSwapChain) {
    mCurrentTexture->Destroy();
    mNewTextureRequested = true;
  }
  return Some(layers::SurfaceDescriptorRemoteTexture(*mLastRemoteTextureId,
                                                     *mRemoteTextureOwnerId));
}

bool CanvasContext::UpdateWebRenderCanvasData(
    mozilla::nsDisplayListBuilder* aBuilder, WebRenderCanvasData* aCanvasData) {
  auto* renderer = aCanvasData->GetCanvasRenderer();

  if (renderer && mRemoteTextureOwnerId.isSome() &&
      renderer->GetRemoteTextureOwnerId() == mRemoteTextureOwnerId) {
    return true;
  }

  renderer = aCanvasData->CreateCanvasRenderer();
  if (!InitializeCanvasRenderer(aBuilder, renderer)) {
    // Clear CanvasRenderer of WebRenderCanvasData
    aCanvasData->ClearCanvasRenderer();
    return false;
  }
  return true;
}

bool CanvasContext::InitializeCanvasRenderer(
    nsDisplayListBuilder* aBuilder, layers::CanvasRenderer* aRenderer) {
  if (mRemoteTextureOwnerId.isNothing()) {
    return false;
  }

  layers::CanvasRendererData data;
  data.mContext = this;
  data.mSize = mCanvasSize;
  data.mIsOpaque = false;
  data.mRemoteTextureOwnerId = mRemoteTextureOwnerId;

  aRenderer->Initialize(data);
  aRenderer->SetDirty();

  if (mWaitingCanvasRendererInitialized) {
    InvalidateCanvasContent();
  }
  mWaitingCanvasRendererInitialized = false;

  return true;
}

mozilla::UniquePtr<uint8_t[]> CanvasContext::GetImageBuffer(
    int32_t* out_format, gfx::IntSize* out_imageSize) {
  *out_format = 0;
  *out_imageSize = {};

  gfxAlphaType any;
  RefPtr<gfx::SourceSurface> snapshot = GetSurfaceSnapshot(&any);
  if (!snapshot) {
    return nullptr;
  }

  RefPtr<gfx::DataSourceSurface> dataSurface = snapshot->GetDataSurface();
  *out_imageSize = dataSurface->GetSize();

  if (ShouldResistFingerprinting(RFPTarget::CanvasRandomization)) {
    gfxUtils::GetImageBufferWithRandomNoise(dataSurface,
                                            /* aIsAlphaPremultiplied */ true,
                                            GetCookieJarSettings(),
                                            PrincipalOrNull(), &*out_format);
  }

  return gfxUtils::GetImageBuffer(dataSurface, /* aIsAlphaPremultiplied */ true,
                                  &*out_format);
}

NS_IMETHODIMP CanvasContext::GetInputStream(const char* aMimeType,
                                            const nsAString& aEncoderOptions,
                                            nsIInputStream** aStream) {
  gfxAlphaType any;
  RefPtr<gfx::SourceSurface> snapshot = GetSurfaceSnapshot(&any);
  if (!snapshot) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<gfx::DataSourceSurface> dataSurface = snapshot->GetDataSurface();

  if (ShouldResistFingerprinting(RFPTarget::CanvasRandomization)) {
    return gfxUtils::GetInputStreamWithRandomNoise(
        dataSurface, /* aIsAlphaPremultiplied */ true, aMimeType,
        aEncoderOptions, GetCookieJarSettings(), PrincipalOrNull(), aStream);
  }

  return gfxUtils::GetInputStream(dataSurface, /* aIsAlphaPremultiplied */ true,
                                  aMimeType, aEncoderOptions, aStream);
}

bool CanvasContext::GetIsOpaque() {
  if (!mConfiguration) {
    return false;
  }
  return mConfiguration->mAlphaMode == dom::GPUCanvasAlphaMode::Opaque;
}

already_AddRefed<gfx::SourceSurface> CanvasContext::GetSurfaceSnapshot(
    gfxAlphaType* aOutAlphaType) {
  const bool isOpaque = GetIsOpaque();
  gfx::SurfaceFormat snapshotFormat = mGfxFormat;
  if (isOpaque) {
    if (aOutAlphaType) {
      *aOutAlphaType = gfxAlphaType::Opaque;
    }
    if (mGfxFormat == gfx::SurfaceFormat::B8G8R8A8) {
      snapshotFormat = gfx::SurfaceFormat::B8G8R8X8;
    } else if (mGfxFormat == gfx::SurfaceFormat::R8G8B8A8) {
      snapshotFormat = gfx::SurfaceFormat::R8G8B8X8;
    }
  } else {
    if (aOutAlphaType) {
      *aOutAlphaType = gfxAlphaType::Premult;
    }
  }

  auto* const cm = gfx::CanvasManagerChild::Get();
  if (!cm) {
    return nullptr;
  }

  if (!mBridge || !mBridge->CanSend() || mRemoteTextureOwnerId.isNothing()) {
    return nullptr;
  }

  MOZ_ASSERT(mRemoteTextureOwnerId.isSome());

  // The parent side needs to create a command encoder which will be submitted
  // and dropped right away so we create and release an encoder ID here.
  RawId encoderId = ffi::wgpu_client_make_encoder_id(mBridge->GetClient());
  RefPtr<gfx::DataSourceSurface> snapshot =
      cm->GetSnapshot(cm->Id(), mBridge->Id(), mRemoteTextureOwnerId,
                      Some(encoderId), snapshotFormat, /* aPremultiply */ false,
                      /* aYFlip */ false);
  ffi::wgpu_client_free_command_encoder_id(mBridge->GetClient(), encoderId);
  if (!snapshot) {
    return nullptr;
  }

  // Clear alpha channel to 0xFF / 1.0 for opaque contexts.
  // https://www.w3.org/TR/webgpu/#abstract-opdef-get-a-copy-of-the-image-contents-of-a-context
  if (isOpaque) {
    gfx::DataSourceSurface::ScopedMap map(snapshot,
                                          gfx::DataSourceSurface::WRITE);
    if (!map.IsMapped()) {
      return nullptr;
    }

    for (int32_t y = 0; y < snapshot->GetSize().height; y++) {
      for (int32_t x = 0; x < snapshot->GetSize().width; x++) {
        uint8_t* const pixel = map.GetData() + y * map.GetStride() + x * 4;
        pixel[3] = 0xFF;
      }
    }
  }

  return snapshot.forget();
}

Maybe<layers::SurfaceDescriptor> CanvasContext::GetFrontBuffer(
    WebGLFramebufferJS*, const bool) {
  if (mPendingSwapChainPresent) {
    auto desc = SwapChainPresent();
    MOZ_ASSERT(!mPendingSwapChainPresent);
    return desc;
  }
  return Nothing();
}

already_AddRefed<layers::FwdTransactionTracker>
CanvasContext::UseCompositableForwarder(
    layers::CompositableForwarder* aForwarder) {
  return layers::FwdTransactionTracker::GetOrCreate(mFwdTransactionTracker);
}

void CanvasContext::ForceNewFrame() {
  if (!mCanvasElement && !mOffscreenCanvas) {
    return;
  }

  // Force a new frame to be built, which will execute the
  // `CanvasContextType::WebGPU` switch case in `CreateWebRenderCommands` and
  // populate the WR user data.
  if (mCanvasElement) {
    mCanvasElement->InvalidateCanvas();
  } else if (mOffscreenCanvas) {
    dom::OffscreenCanvasDisplayData data;
    data.mSize = mCanvasSize;
    data.mIsOpaque = false;
    mOffscreenCanvas->UpdateDisplayData(data);
  }
}

void CanvasContext::InvalidateCanvasContent() {
  if (!mCanvasElement && !mOffscreenCanvas) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return;
  }

  if (mCanvasElement) {
    SVGObserverUtils::InvalidateDirectRenderingObservers(mCanvasElement);
    mCanvasElement->InvalidateCanvasContent(nullptr);
  } else if (mOffscreenCanvas) {
    mOffscreenCanvas->QueueCommitToCompositor();
  }
}

}  // namespace mozilla::webgpu
