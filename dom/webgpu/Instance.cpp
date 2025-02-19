/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Instance.h"

#include "Adapter.h"
#include "mozilla/Assertions.h"
#include "mozilla/ErrorResult.h"
#include "nsIGlobalObject.h"
#include "ipc/WebGPUChild.h"
#include "ipc/WebGPUTypes.h"
#include "mozilla/webgpu/ffi/wgpu.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/gfx/CanvasManagerChild.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/StaticPrefs_dom.h"
#include "nsString.h"

#ifdef RELEASE_OR_BETA
#  include "mozilla/dom/WorkerPrivate.h"
#endif

#include <optional>
#include <string_view>

namespace mozilla::webgpu {

GPU_IMPL_CYCLE_COLLECTION(WGSLLanguageFeatures, mParent)

GPU_IMPL_CYCLE_COLLECTION(Instance, mOwner, mWgslLanguageFeatures)

static inline nsDependentCString ToCString(const std::string_view s) {
  return {s.data(), s.length()};
}

/* static */ bool Instance::PrefEnabled(JSContext* aCx, JSObject* aObj) {
  if (!StaticPrefs::dom_webgpu_enabled()) {
    return false;
  }

  if (NS_IsMainThread()) {
    return true;
  }

  return StaticPrefs::dom_webgpu_workers_enabled();
}

/*static*/
already_AddRefed<Instance> Instance::Create(nsIGlobalObject* aOwner) {
  RefPtr<Instance> result = new Instance(aOwner);
  return result.forget();
}

Instance::Instance(nsIGlobalObject* aOwner)
    : mOwner(aOwner), mWgslLanguageFeatures(new WGSLLanguageFeatures(this)) {
  // Populate `mWgslLanguageFeatures`.
  IgnoredErrorResult rv;
  nsCString wgslFeature;
  for (size_t i = 0;; ++i) {
    wgslFeature.Truncate(0);
    ffi::wgpu_client_instance_get_wgsl_language_feature(&wgslFeature, i);
    if (wgslFeature.IsEmpty()) {
      break;
    }
    NS_ConvertASCIItoUTF16 feature{wgslFeature};
    this->mWgslLanguageFeatures->Add(feature, rv);
    if (rv.Failed()) {
      MOZ_CRASH("failed to append WGSL language feature");
    }
  }
}

Instance::~Instance() { Cleanup(); }

void Instance::Cleanup() {}

JSObject* Instance::WrapObject(JSContext* cx,
                               JS::Handle<JSObject*> givenProto) {
  return dom::GPU_Binding::Wrap(cx, this, givenProto);
}

already_AddRefed<dom::Promise> Instance::RequestAdapter(
    const dom::GPURequestAdapterOptions& aOptions, ErrorResult& aRv) {
  RefPtr<dom::Promise> promise = dom::Promise::Create(mOwner, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // -
  // Check if we should allow the request.

  const auto errStr = [&]() -> std::optional<std::string_view> {
#ifdef RELEASE_OR_BETA
    if (true) {
      return "WebGPU is not yet available in Release or Beta builds.";
    }

    // NOTE: Deliberately left after the above check so that we only enter
    // here if it's removed. Above is a more informative diagnostic, while the
    // check is still present.
    //
    // Follow-up to remove this check:
    // <https://bugzilla.mozilla.org/show_bug.cgi?id=1942431>
    if (dom::WorkerPrivate* wp = dom::GetCurrentThreadWorkerPrivate()) {
      if (wp->IsServiceWorker()) {
        return "WebGPU in service workers is not yet available in Release or "
               "Beta builds; see "
               "<https://bugzilla.mozilla.org/show_bug.cgi?id=1942431>.";
      }
    }
#endif
    if (!gfx::gfxVars::AllowWebGPU()) {
      return "WebGPU is disabled by blocklist.";
    }
    if (!StaticPrefs::dom_webgpu_enabled()) {
      return "WebGPU is disabled by dom.webgpu.enabled:false.";
    }
    return {};
  }();
  if (errStr) {
    promise->MaybeRejectWithNotSupportedError(ToCString(*errStr));
    return promise.forget();
  }

  // -
  // Make the request.

  auto* const canvasManager = gfx::CanvasManagerChild::Get();
  if (!canvasManager) {
    promise->MaybeRejectWithInvalidStateError(
        "Failed to create CanvasManagerChild");
    return promise.forget();
  }

  RefPtr<WebGPUChild> bridge = canvasManager->GetWebGPUChild();
  if (!bridge) {
    promise->MaybeRejectWithInvalidStateError("Failed to create WebGPUChild");
    return promise.forget();
  }

  RefPtr<Instance> instance = this;

  bridge->InstanceRequestAdapter(aOptions)->Then(
      GetCurrentSerialEventTarget(), __func__,
      [promise, instance, bridge](ipc::ByteBuf aInfoBuf) {
        auto info = std::make_shared<ffi::WGPUAdapterInformation>();
        ffi::wgpu_client_adapter_extract_info(ToFFI(&aInfoBuf), info.get());
        MOZ_ASSERT(info->id != 0);
        RefPtr<Adapter> adapter = new Adapter(instance, bridge, info);
        promise->MaybeResolve(adapter);
      },
      [promise](const Maybe<ipc::ResponseRejectReason>& aResponseReason) {
        if (aResponseReason.isSome()) {
          promise->MaybeRejectWithAbortError("Internal communication error!");
        } else {
          promise->MaybeResolve(JS::NullHandleValue);
        }
      });

  return promise.forget();
}

}  // namespace mozilla::webgpu
