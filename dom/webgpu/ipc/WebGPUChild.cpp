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
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/WebGPUBinding.h"
#include "mozilla/dom/GPUUncapturedErrorEvent.h"
#include "mozilla/webgpu/ValidationError.h"
#include "mozilla/webgpu/OutOfMemoryError.h"
#include "mozilla/webgpu/InternalError.h"
#include "mozilla/webgpu/WebGPUTypes.h"
#include "mozilla/webgpu/RenderPipeline.h"
#include "mozilla/webgpu/ComputePipeline.h"
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

void on_message_queued(ffi::WGPUWebGPUChildPtr child) {
  auto* c = static_cast<WebGPUChild*>(child);
  c->ScheduleFlushQueuedMessages();
}

WebGPUChild::WebGPUChild()
    : mClient(ffi::wgpu_client_new(this, on_message_queued)) {}

WebGPUChild::~WebGPUChild() = default;

RawId WebGPUChild::RenderBundleEncoderFinish(
    ffi::WGPURenderBundleEncoder& aEncoder, RawId aDeviceId,
    const dom::GPURenderBundleDescriptor& aDesc) {
  ffi::WGPURenderBundleDescriptor desc = {};

  webgpu::StringHelper label(aDesc.mLabel);
  desc.label = label.Get();

  RawId id = ffi::wgpu_client_create_render_bundle(GetClient(), aDeviceId,
                                                   &aEncoder, &desc);

  return id;
}

RawId WebGPUChild::RenderBundleEncoderFinishError(RawId aDeviceId,
                                                  const nsString& aLabel) {
  webgpu::StringHelper label(aLabel);

  RawId id = ffi::wgpu_client_create_render_bundle_error(GetClient(), aDeviceId,
                                                         label.Get());

  return id;
}

void resolve_request_adapter_promise(
    ffi::WGPUWebGPUChildPtr child,
    const struct ffi::WGPUAdapterInformation* adapter_info) {
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

void resolve_request_device_promise(ffi::WGPUWebGPUChildPtr child,
                                    const nsCString* error) {
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

void resolve_pop_error_scope_promise(ffi::WGPUWebGPUChildPtr child, uint8_t ty,
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

void resolve_create_pipeline_promise(ffi::WGPUWebGPUChildPtr child,
                                     bool is_render_pipeline,
                                     bool is_validation_error,
                                     const nsCString* error) {
  auto* c = static_cast<WebGPUChild*>(child);
  auto& pending_promises = c->mPendingCreatePipelinePromises;
  auto pending_promise = std::move(pending_promises.front());
  pending_promises.pop_front();

  MOZ_ASSERT(pending_promise.is_render_pipeline == is_render_pipeline);

  if (error == nullptr) {
    if (pending_promise.is_render_pipeline) {
      RefPtr<RenderPipeline> object = new RenderPipeline(
          pending_promise.device, pending_promise.pipeline_id,
          pending_promise.implicit_pipeline_layout_id,
          std::move(pending_promise.implicit_bind_group_layout_ids));
      object->SetLabel(pending_promise.label);
      pending_promise.promise->MaybeResolve(object);
    } else {
      RefPtr<ComputePipeline> object = new ComputePipeline(
          pending_promise.device, pending_promise.pipeline_id,
          pending_promise.implicit_pipeline_layout_id,
          std::move(pending_promise.implicit_bind_group_layout_ids));
      object->SetLabel(pending_promise.label);
      pending_promise.promise->MaybeResolve(object);
    }
  } else {
    // TODO: not sure how to reject with a PipelineError, we need to register it
    // with DOMEXCEPTION?
    // dom::GPUPipelineErrorReason reason;
    // if (is_validation_error) {
    //   reason = dom::GPUPipelineErrorReason::Validation;
    // } else {
    //   reason = dom::GPUPipelineErrorReason::Internal;
    // }
    // RefPtr<PipelineError> e = new PipelineError(*error, reason);
    pending_promise.promise->MaybeRejectWithOperationError(*error);
  }
}

MOZ_CAN_RUN_SCRIPT void resolve_create_shader_module_promise(
    ffi::WGPUWebGPUChildPtr child,
    const struct ffi::WGPUFfiShaderModuleCompilationMessage* messages_ptr,
    uintptr_t messages_len) {
  auto* c = static_cast<WebGPUChild*>(child);
  auto& pending_promises = c->mPendingCreateShaderModulePromises;
  auto pending_promise = std::move(pending_promises.front());
  pending_promises.pop_front();

  auto ffi_messages = Span(messages_ptr, messages_len);

  auto messages = nsTArray<WebGPUCompilationMessage>(messages_len);
  for (const auto& message : ffi_messages) {
    WebGPUCompilationMessage msg;
    msg.lineNum = message.line_number;
    msg.linePos = message.line_pos;
    msg.offset = message.utf16_offset;
    msg.length = message.utf16_length;
    msg.message = message.message;
    // wgpu currently only returns errors.
    msg.messageType = WebGPUCompilationMessageType::Error;
    messages.AppendElement(std::move(msg));
  }

  if (!messages.IsEmpty()) {
    auto shader_module = pending_promise.shader_module;
    reportCompilationMessagesToConsole(shader_module, std::cref(messages));
  }
  RefPtr<CompilationInfo> infoObject(
      new CompilationInfo(pending_promise.device));
  infoObject->SetMessages(messages);
  pending_promise.promise->MaybeResolve(infoObject);
};

void resolve_buffer_map_promise(ffi::WGPUWebGPUChildPtr child,
                                ffi::WGPUBufferId buffer_id, bool is_writable,
                                uint64_t offset, uint64_t size,
                                const nsCString* error) {
  auto* c = static_cast<WebGPUChild*>(child);
  auto& pending_promises = c->mPendingBufferMapPromises;

  WebGPUChild::PendingBufferMapPromise pending_promise;
  if (auto search = pending_promises.find(buffer_id);
      search != pending_promises.end()) {
    pending_promise = std::move(search->second.front());
    search->second.pop_front();

    if (search->second.empty()) {
      pending_promises.erase(buffer_id);
    }
  } else {
    NS_ERROR("Missing pending promise for buffer map");
  }

  // Unmap might have been called while the result was on the way back.
  if (pending_promise.promise->State() != dom::Promise::PromiseState::Pending) {
    return;
  }

  if (error == nullptr) {
    pending_promise.buffer->ResolveMapRequest(pending_promise.promise, offset,
                                              size, is_writable);
  } else {
    pending_promise.buffer->RejectMapRequest(pending_promise.promise, *error);
  }
}

void resolve_on_submitted_work_done_promise(ffi::WGPUWebGPUChildPtr child) {
  auto* c = static_cast<WebGPUChild*>(child);
  auto& pending_promises = c->mPendingOnSubmittedWorkDonePromises;
  auto pending_promise = std::move(pending_promises.front());
  pending_promises.pop_front();

  pending_promise->MaybeResolveWithUndefined();
};

ipc::IPCResult WebGPUChild::RecvServerMessage(const ipc::ByteBuf& aByteBuf) {
  ffi::wgpu_client_receive_server_message(
      GetClient(), ToFFI(&aByteBuf), resolve_request_adapter_promise,
      resolve_request_device_promise, resolve_pop_error_scope_promise,
      resolve_create_pipeline_promise, resolve_create_shader_module_promise,
      resolve_buffer_map_promise, resolve_on_submitted_work_done_promise);
  return IPC_OK();
}

void WebGPUChild::ScheduleFlushQueuedMessages() {
  if (mScheduledFlushQueuedMessages) {
    return;
  }
  mScheduledFlushQueuedMessages = true;

  nsContentUtils::RunInStableState(
      NewRunnableMethod("dom::WebGPUChild::ScheduledFlushQueuedMessages", this,
                        &WebGPUChild::ScheduledFlushQueuedMessages));
}

size_t WebGPUChild::QueueDataBuffer(ipc::ByteBuf&& bb) {
  auto buffer_index = mQueuedDataBuffers.Length();
  mQueuedDataBuffers.AppendElement(std::move(bb));
  return buffer_index;
}

size_t WebGPUChild::QueueShmemHandle(ipc::MutableSharedMemoryHandle&& handle) {
  auto shmem_handle_index = mQueuedHandles.Length();
  mQueuedHandles.AppendElement(std::move(handle));
  return shmem_handle_index;
}

void WebGPUChild::ScheduledFlushQueuedMessages() {
  MOZ_ASSERT(mScheduledFlushQueuedMessages);
  mScheduledFlushQueuedMessages = false;

  PROFILER_MARKER_UNTYPED("WebGPU: ScheduledFlushQueuedMessages",
                          GRAPHICS_WebGPU);
  FlushQueuedMessages();
}

void WebGPUChild::FlushQueuedMessages() {
  ipc::ByteBuf serialized_messages;
  auto nr_of_messages = ffi::wgpu_client_get_queued_messages(
      GetClient(), ToFFI(&serialized_messages));
  if (nr_of_messages == 0) {
    return;
  }

  PROFILER_MARKER_FMT("WebGPU: FlushQueuedMessages", GRAPHICS_WebGPU, {},
                      "messages: {}", nr_of_messages);

  bool sent =
      SendMessages(nr_of_messages, std::move(serialized_messages),
                   std::move(mQueuedDataBuffers), std::move(mQueuedHandles));
  mQueuedDataBuffers.Clear();
  mQueuedHandles.Clear();

  if (!sent) {
    ClearAllPendingPromises();
  }
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

void WebGPUChild::SwapChainPresent(RawId aTextureId,
                                   const RemoteTextureId& aRemoteTextureId,
                                   const RemoteTextureOwnerId& aOwnerId) {
  // The parent side needs to create a command encoder which will be submitted
  // and dropped right away so we create and release an encoder ID here.
  RawId encoderId = ffi::wgpu_client_make_encoder_id(GetClient());

  ffi::wgpu_client_swap_chain_present(GetClient(), aTextureId, encoderId,
                                      aRemoteTextureId.mId, aOwnerId.mId);

  ffi::wgpu_client_free_command_encoder_id(GetClient(), encoderId);
}

void WebGPUChild::RegisterDevice(Device* const aDevice) {
  mDeviceMap.insert({aDevice->mId, aDevice});
}

void WebGPUChild::UnregisterDevice(RawId aDeviceId) {
  ffi::wgpu_client_drop_device(GetClient(), aDeviceId);

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

  ClearAllPendingPromises();
}

void WebGPUChild::ClearAllPendingPromises() {
  // Resolve the promise with null since the WebGPUChild has been destroyed.
  {
    for (auto& pending_promise : mPendingRequestAdapterPromises) {
      pending_promise.promise->MaybeResolve(JS::NullHandleValue);
    }
    mPendingRequestAdapterPromises.clear();
  }
  // Pretend this worked but return a lost device, per spec.
  {
    for (auto& pending_promise : mPendingRequestDevicePromises) {
      RefPtr<Device> device =
          new Device(pending_promise.adapter, pending_promise.device_id,
                     pending_promise.queue_id, pending_promise.features,
                     pending_promise.limits);
      device->SetLabel(pending_promise.label);
      device->ResolveLost(Nothing(), u"WebGPUChild destroyed"_ns);
      pending_promise.promise->MaybeResolve(device);
    }
    mPendingRequestDevicePromises.clear();
  }
  // Pretend this worked and there is no error, per spec.
  {
    for (auto& pending_promise : mPendingPopErrorScopePromises) {
      pending_promise.promise->MaybeResolve(JS::NullHandleValue);
    }
    mPendingPopErrorScopePromises.clear();
  }
  // Pretend this worked, per spec.
  {
    for (auto& pending_promise : mPendingCreatePipelinePromises) {
      if (pending_promise.is_render_pipeline) {
        RefPtr<RenderPipeline> object = new RenderPipeline(
            pending_promise.device, pending_promise.pipeline_id,
            pending_promise.implicit_pipeline_layout_id,
            std::move(pending_promise.implicit_bind_group_layout_ids));
        object->SetLabel(pending_promise.label);
        pending_promise.promise->MaybeResolve(object);
      } else {
        RefPtr<ComputePipeline> object = new ComputePipeline(
            pending_promise.device, pending_promise.pipeline_id,
            pending_promise.implicit_pipeline_layout_id,
            std::move(pending_promise.implicit_bind_group_layout_ids));
        object->SetLabel(pending_promise.label);
        pending_promise.promise->MaybeResolve(object);
      }
    }
    mPendingCreatePipelinePromises.clear();
  }
  // Pretend this worked, the spec is not explicit about this behavior but it's
  // in line with the others.
  {
    for (auto& pending_promise : mPendingCreateShaderModulePromises) {
      nsTArray<WebGPUCompilationMessage> messages;
      RefPtr<CompilationInfo> infoObject(
          new CompilationInfo(pending_promise.device));
      infoObject->SetMessages(messages);
      pending_promise.promise->MaybeResolve(infoObject);
    }
    mPendingCreateShaderModulePromises.clear();
  }
  // Reject the promise as if unmap() has been called, per spec.
  {
    for (auto& pending_promises : mPendingBufferMapPromises) {
      for (auto& pending_promise : pending_promises.second) {
        // Unmap might have been called.
        if (pending_promise.promise->State() !=
            dom::Promise::PromiseState::Pending) {
          continue;
        }
        pending_promise.buffer->RejectMapRequestWithAbortError(
            pending_promise.promise);
      }
    }
    mPendingBufferMapPromises.clear();
  }
  // Pretend we finished the work, the spec is not explicit about this behavior
  // but it's in line with the others.
  {
    for (auto& pending_promise : mPendingOnSubmittedWorkDonePromises) {
      pending_promise->MaybeResolveWithUndefined();
    }
    mPendingOnSubmittedWorkDonePromises.clear();
  }
}

void WebGPUChild::QueueSubmit(RawId aSelfId, RawId aDeviceId,
                              nsTArray<RawId>& aCommandBuffers) {
  ffi::wgpu_client_queue_submit(
      GetClient(), aDeviceId, aSelfId, aCommandBuffers.Elements(),
      aCommandBuffers.Length(), mSwapChainTexturesWaitingForSubmit.Elements(),
      mSwapChainTexturesWaitingForSubmit.Length());
  mSwapChainTexturesWaitingForSubmit.Clear();

  PROFILER_MARKER_UNTYPED("WebGPU: QueueSubmit", GRAPHICS_WebGPU);
  FlushQueuedMessages();
}

void WebGPUChild::NotifyWaitForSubmit(RawId aTextureId) {
  mSwapChainTexturesWaitingForSubmit.AppendElement(aTextureId);
}

}  // namespace mozilla::webgpu
