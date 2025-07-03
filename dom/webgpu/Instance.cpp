/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Instance.h"

#include "Adapter.h"
#include "js/Value.h"
#include "mozilla/Assertions.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/gfx/Logging.h"
#include "nsDebug.h"
#include "nsIGlobalObject.h"
#include "ipc/WebGPUChild.h"
#include "ipc/WebGPUTypes.h"
#include "mozilla/webgpu/ffi/wgpu.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/gfx/CanvasManagerChild.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/StaticPrefs_dom.h"
#include "nsString.h"
#include "nsStringFwd.h"

#include "mozilla/dom/WorkerPrivate.h"

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

  dom::WorkerPrivate* wp = dom::GetCurrentThreadWorkerPrivate();
  if (wp && wp->IsServiceWorker()) {
    return StaticPrefs::dom_webgpu_service_workers_enabled();
  }

  return true;
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
      if (rv.ErrorCodeIs(NS_ERROR_UNEXPECTED)) {
        // This is fine; something went wrong with the JS scope we're in, and we
        // can just let that happen.
        NS_WARNING(
            "`Instance::Instance`: failed to append WGSL language feature: got "
            "`NS_ERROR_UNEXPECTED`");
      } else {
        MOZ_CRASH_UNSAFE_PRINTF(
            "`Instance::Instance`: failed to append WGSL language feature: %d",
            rv.ErrorCodeAsInt());
      }
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

  std::optional<std::string_view> rejectionMessage = {};
  const auto rejectIf = [&rejectionMessage](bool condition,
                                            const char* message) {
    if (condition && !rejectionMessage.has_value()) {
      rejectionMessage = message;
    }
  };

#ifndef EARLY_BETA_OR_EARLIER
#  ifndef XP_WIN
  rejectIf(true,
           "WebGPU is only available on Windows, and in Nightly and Early Beta "
           "builds on other platforms.");
#  endif

  // NOTE: Deliberately left after the above check so that we only enter
  // here if it's removed. Above is a more informative diagnostic, while the
  // check is still present.
  //
  // Follow-up to remove this check:
  // <https://bugzilla.mozilla.org/show_bug.cgi?id=1942431>
  if (dom::WorkerPrivate* wp = dom::GetCurrentThreadWorkerPrivate()) {
    rejectIf(wp->IsServiceWorker(),
             "WebGPU in service workers is not yet available in Release or "
             "late Beta builds; see "
             "<https://bugzilla.mozilla.org/show_bug.cgi?id=1942431>.");
  }
#endif
  rejectIf(!gfx::gfxVars::AllowWebGPU(), "WebGPU is disabled by blocklist.");
  rejectIf(!StaticPrefs::dom_webgpu_enabled(),
           "WebGPU is disabled because the `dom.webgpu.enabled` pref. is set "
           "to `false`.");
#ifdef WIN32
#  ifndef MOZ_DXCOMPILER
  rejectIf(true,
           "WebGPU is disabled because dxcompiler is unavailable with this "
           "build configuration");
#  endif
#endif

  if (rejectionMessage) {
    promise->MaybeRejectWithNotSupportedError(ToCString(*rejectionMessage));
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

  if (aOptions.mFeatureLevel.EqualsASCII("core")) {
    // Good! That's all we support.
  } else if (aOptions.mFeatureLevel.EqualsASCII("compatibility")) {
    dom::AutoJSAPI api;
    if (api.Init(mOwner)) {
      JS::WarnUTF8(api.cx(),
                   "User requested a WebGPU adapter with `featureLevel: "
                   "\"compatibility\"`, which is not yet supported; returning "
                   "a \"core\"-defaulting adapter for now. Subscribe to "
                   "<https://bugzilla.mozilla.org/show_bug.cgi?id=1905951>"
                   " for updates on its development in Firefox.");
    }
  } else {
    NS_ConvertUTF16toUTF8 featureLevel(aOptions.mFeatureLevel);
    dom::AutoJSAPI api;
    if (api.Init(mOwner)) {
      JS::WarnUTF8(api.cx(),
                   "expected one of `\"core\"` or `\"compatibility\"` for "
                   "`GPUAdapter.featureLevel`, got %s",
                   featureLevel.get());
    }
    promise->MaybeResolve(JS::NullValue());
    return promise.forget();
  }

  if (aOptions.mXrCompatible) {
    dom::AutoJSAPI api;
    if (api.Init(mOwner)) {
      JS::WarnUTF8(
          api.cx(),
          "User requested a WebGPU adapter with `xrCompatible: true`, "
          "but WebXR sessions are not yet supported in WebGPU. Returning "
          "a regular adapter for now. Subscribe to "
          "<https://bugzilla.mozilla.org/show_bug.cgi?id=1963829>"
          " for updates on its development in Firefox.");
    }
  }

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
