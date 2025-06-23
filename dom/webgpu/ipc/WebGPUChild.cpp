/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGPUChild.h"

#include "js/RootingAPI.h"
#include "js/String.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "js/Warnings.h"  // JS::WarnUTF8
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/EnumTypeTraits.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/WebGPUBinding.h"
#include "mozilla/dom/GPUUncapturedErrorEvent.h"
#include "mozilla/webgpu/ValidationError.h"
#include "mozilla/webgpu/OutOfMemoryError.h"
#include "mozilla/webgpu/InternalError.h"
#include "mozilla/webgpu/WebGPUTypes.h"
#include "mozilla/webgpu/ffi/wgpu.h"
#include "Adapter.h"
#include "DeviceLostInfo.h"
#include "PipelineLayout.h"
#include "Sampler.h"
#include "CompilationInfo.h"
#include "Utility.h"

#include <utility>

namespace mozilla::webgpu {

NS_IMPL_CYCLE_COLLECTION(WebGPUChild)

void WebGPUChild::JsWarning(nsIGlobalObject* aGlobal,
                            const nsACString& aMessage) {
  const auto& flatString = PromiseFlatCString(aMessage);
  if (aGlobal) {
    dom::AutoJSAPI api;
    if (api.Init(aGlobal)) {
      JS::WarnUTF8(api.cx(), "Uncaptured WebGPU error: %s", flatString.get());
    }
  } else {
    printf_stderr("Uncaptured WebGPU error without device target: %s\n",
                  flatString.get());
  }
}

static UniquePtr<ffi::WGPUClient> initialize() {
  ffi::WGPUInfrastructure infra = ffi::wgpu_client_new();
  return UniquePtr<ffi::WGPUClient>{infra.client};
}

WebGPUChild::WebGPUChild() : mClient(initialize()) {}

WebGPUChild::~WebGPUChild() = default;

RawId WebGPUChild::RenderBundleEncoderFinish(
    ffi::WGPURenderBundleEncoder& aEncoder, RawId aDeviceId,
    const dom::GPURenderBundleDescriptor& aDesc) {
  ffi::WGPURenderBundleDescriptor desc = {};

  webgpu::StringHelper label(aDesc.mLabel);
  desc.label = label.Get();

  ipc::ByteBuf bb;
  RawId id = ffi::wgpu_client_create_render_bundle(
      mClient.get(), aDeviceId, &aEncoder, &desc, ToFFI(&bb));

  SendMessage(std::move(bb), Nothing());

  return id;
}

RawId WebGPUChild::RenderBundleEncoderFinishError(RawId aDeviceId,
                                                  const nsString& aLabel) {
  webgpu::StringHelper label(aLabel);

  ipc::ByteBuf bb;
  RawId id = ffi::wgpu_client_create_render_bundle_error(
      mClient.get(), aDeviceId, label.Get(), ToFFI(&bb));

  SendMessage(std::move(bb), Nothing());

  return id;
}

void resolve_request_adapter_promise(
    void* child, const struct ffi::WGPUAdapterInformation* adapter_info) {
  auto* c = static_cast<WebGPUChild*>(child);
  auto& pending_promises = c->mPendingRequestAdapterPromises;
  auto pending_promise = std::move(pending_promises.front());
  pending_promises.pop_front();

  if (adapter_info == nullptr) {
    pending_promise.promise->MaybeResolve(JS::NullHandleValue);
  } else {
    auto info = std::make_shared<ffi::WGPUAdapterInformation>(*adapter_info);
    RefPtr<Adapter> adapter = new Adapter(pending_promise.instance, c, info);
    pending_promise.promise->MaybeResolve(adapter);
  }
}

void resolve_request_device_promise(void* child, const nsCString* error) {
  auto* c = static_cast<WebGPUChild*>(child);
  auto& pending_promises = c->mPendingRequestDevicePromises;
  auto pending_promise = std::move(pending_promises.front());
  pending_promises.pop_front();

  if (error == nullptr) {
    RefPtr<Device> device =
        new Device(pending_promise.adapter, pending_promise.device_id,
                   pending_promise.queue_id, pending_promise.features,
                   pending_promise.limits);
    device->SetLabel(pending_promise.label);
    pending_promise.promise->MaybeResolve(device);
  } else {
    pending_promise.promise->MaybeRejectWithOperationError(*error);
  }
}

void resolve_pop_error_scope_promise(void* child, uint8_t ty,
                                     const nsCString* message) {
  auto* c = static_cast<WebGPUChild*>(child);
  auto& pending_promises = c->mPendingPopErrorScopePromises;
  auto pending_promise = std::move(pending_promises.front());
  pending_promises.pop_front();

  RefPtr<Error> error;

  switch ((PopErrorScopeResultType)ty) {
    case PopErrorScopeResultType::NoError:
      pending_promise.promise->MaybeResolve(JS::NullHandleValue);
      return;

    case PopErrorScopeResultType::DeviceLost:
      pending_promise.promise->MaybeResolve(JS::NullHandleValue);
      return;

    case PopErrorScopeResultType::ThrowOperationError:
      pending_promise.promise->MaybeRejectWithOperationError(*message);
      return;

    case PopErrorScopeResultType::OutOfMemory:
      error = new OutOfMemoryError(pending_promise.device->GetParentObject(),
                                   *message);
      break;

    case PopErrorScopeResultType::ValidationError:
      error = new ValidationError(pending_promise.device->GetParentObject(),
                                  *message);
      break;

    case PopErrorScopeResultType::InternalError:
      error = new InternalError(pending_promise.device->GetParentObject(),
                                *message);
      break;
  }
  pending_promise.promise->MaybeResolve(std::move(error));
}

ipc::IPCResult WebGPUChild::RecvServerMessage(const ipc::ByteBuf& aByteBuf) {
  ffi::wgpu_client_receive_server_message(
      this, GetClient(), ToFFI(&aByteBuf), resolve_request_adapter_promise,
      resolve_request_device_promise, resolve_pop_error_scope_promise);
  return IPC_OK();
}

ipc::IPCResult WebGPUChild::RecvUncapturedError(RawId aDeviceId,
                                                const nsACString& aMessage) {
  RefPtr<Device> device;
  if (aDeviceId) {
    const auto itr = mDeviceMap.find(aDeviceId);
    if (itr != mDeviceMap.end()) {
      device = itr->second.get();
      MOZ_ASSERT(device);
    }
  }
  if (!device) {
    JsWarning(nullptr, aMessage);
  } else {
    // We don't want to spam the errors to the console indefinitely
    if (device->CheckNewWarning(aMessage)) {
      JsWarning(device->GetOwnerGlobal(), aMessage);

      dom::GPUUncapturedErrorEventInit init;
      init.mError = new ValidationError(device->GetParentObject(), aMessage);
      RefPtr<mozilla::dom::GPUUncapturedErrorEvent> event =
          dom::GPUUncapturedErrorEvent::Constructor(
              device, u"uncapturederror"_ns, init);
      device->DispatchEvent(*event);
    }
  }
  return IPC_OK();
}

bool WebGPUChild::ResolveLostForDeviceId(RawId aDeviceId,
                                         Maybe<uint8_t> aReason,
                                         const nsAString& aMessage) {
  RefPtr<Device> device;
  const auto itr = mDeviceMap.find(aDeviceId);
  if (itr != mDeviceMap.end()) {
    device = itr->second.get();
    MOZ_ASSERT(device);
  }
  if (!device) {
    // We must have unregistered the device already.
    return false;
  }

  if (aReason.isSome()) {
    dom::GPUDeviceLostReason reason =
        static_cast<dom::GPUDeviceLostReason>(*aReason);
    MOZ_ASSERT(reason == dom::GPUDeviceLostReason::Destroyed,
               "There is only one valid GPUDeviceLostReason value.");
    device->ResolveLost(Some(reason), aMessage);
  } else {
    device->ResolveLost(Nothing(), aMessage);
  }

  return true;
}

ipc::IPCResult WebGPUChild::RecvDeviceLost(RawId aDeviceId,
                                           Maybe<uint8_t> aReason,
                                           const nsACString& aMessage) {
  auto message = NS_ConvertUTF8toUTF16(aMessage);
  ResolveLostForDeviceId(aDeviceId, aReason, message);
  return IPC_OK();
}

void WebGPUChild::QueueOnSubmittedWorkDone(
    const RawId aSelfId, const RefPtr<dom::Promise>& aPromise) {
  SendQueueOnSubmittedWorkDone(aSelfId)->Then(
      GetCurrentSerialEventTarget(), __func__,
      [aPromise]() { aPromise->MaybeResolveWithUndefined(); },
      [aPromise](const ipc::ResponseRejectReason& aReason) {
        aPromise->MaybeRejectWithNotSupportedError("IPC error");
      });
}

void WebGPUChild::SwapChainPresent(RawId aTextureId,
                                   const RemoteTextureId& aRemoteTextureId,
                                   const RemoteTextureOwnerId& aOwnerId) {
  // Hack: the function expects `DeviceId`, but it only uses it for `backend()`
  // selection.
  // The parent side needs to create a command encoder which will be submitted
  // and dropped right away so we create and release an encoder ID here.
  RawId encoderId = ffi::wgpu_client_make_encoder_id(mClient.get());

  ipc::ByteBuf bb;
  ffi::wgpu_client_swap_chain_present(
      aTextureId, encoderId, aRemoteTextureId.mId, aOwnerId.mId, ToFFI(&bb));
  SendMessage(std::move(bb), Nothing());

  ffi::wgpu_client_free_command_encoder_id(mClient.get(), encoderId);
}

void WebGPUChild::RegisterDevice(Device* const aDevice) {
  mDeviceMap.insert({aDevice->mId, aDevice});
}

void WebGPUChild::UnregisterDevice(RawId aDeviceId) {
  if (CanSend()) {
    ipc::ByteBuf bb;
    ffi::wgpu_client_drop_device(aDeviceId, ToFFI(&bb));
    SendMessage(std::move(bb), Nothing());
  }
  mDeviceMap.erase(aDeviceId);
}

void WebGPUChild::ActorDestroy(ActorDestroyReason) {
  // Resolving the promise could cause us to update the original map if the
  // callee frees the Device objects immediately. Since any remaining entries
  // in the map are no longer valid, we can just move the map onto the stack.
  const auto deviceMap = std::move(mDeviceMap);
  mDeviceMap.clear();

  for (const auto& targetIter : deviceMap) {
    RefPtr<Device> device = targetIter.second.get();
    MOZ_ASSERT(device);
    // It would be cleaner to call ResolveLostForDeviceId, but we
    // just cleared the device map, so we have to invoke ResolveLost
    // directly on the device.
    device->ResolveLost(Nothing(), u"WebGPUChild destroyed"_ns);
  }
}

void WebGPUChild::QueueSubmit(RawId aSelfId, RawId aDeviceId,
                              nsTArray<RawId>& aCommandBuffers) {
  ipc::ByteBuf bb;
  ffi::wgpu_client_queue_submit(
      aDeviceId, aSelfId, aCommandBuffers.Elements(), aCommandBuffers.Length(),
      mSwapChainTexturesWaitingForSubmit.Elements(),
      mSwapChainTexturesWaitingForSubmit.Length(), ToFFI(&bb));
  mSwapChainTexturesWaitingForSubmit.Clear();
  SendMessage(std::move(bb), Nothing());
}

void WebGPUChild::NotifyWaitForSubmit(RawId aTextureId) {
  mSwapChainTexturesWaitingForSubmit.AppendElement(aTextureId);
}

}  // namespace mozilla::webgpu
