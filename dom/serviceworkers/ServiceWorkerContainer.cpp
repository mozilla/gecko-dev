/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerContainer.h"

#include "nsContentSecurityManager.h"
#include "nsContentUtils.h"
#include "nsIServiceWorkerManager.h"
#include "nsIScriptError.h"
#include "nsThreadUtils.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "mozilla/Components.h"

#include "nsCycleCollectionParticipant.h"
#include "nsGlobalWindowInner.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/StaticPrefs_extensions.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/StorageAccess.h"
#include "mozilla/StoragePrincipalHelper.h"
#include "mozilla/dom/ClientIPCTypes.h"
#include "mozilla/dom/DOMMozPromiseRequestHolder.h"
#include "mozilla/dom/MessageEvent.h"
#include "mozilla/dom/MessageEventBinding.h"
#include "mozilla/dom/Navigator.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/ServiceWorker.h"
#include "mozilla/dom/ServiceWorkerContainerBinding.h"
#include "mozilla/dom/ServiceWorkerContainerChild.h"
#include "mozilla/dom/ServiceWorkerManager.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"

#include "ServiceWorker.h"
#include "ServiceWorkerRegistration.h"
#include "ServiceWorkerUtils.h"

// This is defined to something else on Windows
#ifdef DispatchMessage
#  undef DispatchMessage
#endif

namespace mozilla::dom {

using mozilla::ipc::BackgroundChild;
using mozilla::ipc::PBackgroundChild;
using mozilla::ipc::ResponseRejectReason;

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ServiceWorkerContainer)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(ServiceWorkerContainer, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(ServiceWorkerContainer, DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_INHERITED(ServiceWorkerContainer, DOMEventTargetHelper,
                                   mControllerWorker, mReadyPromise)

// static
already_AddRefed<ServiceWorkerContainer> ServiceWorkerContainer::Create(
    nsIGlobalObject* aGlobal) {
  RefPtr<ServiceWorkerContainer> ref = new ServiceWorkerContainer(aGlobal);
  return ref.forget();
}

ServiceWorkerContainer::ServiceWorkerContainer(nsIGlobalObject* aGlobal)
    : DOMEventTargetHelper(aGlobal), mShutdown(false) {
  PBackgroundChild* parentActor =
      BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!parentActor)) {
    Shutdown();
    return;
  }

  RefPtr<ServiceWorkerContainerChild> actor =
      ServiceWorkerContainerChild::Create();
  if (NS_WARN_IF(!actor)) {
    Shutdown();
    return;
  }

  PServiceWorkerContainerChild* sentActor =
      parentActor->SendPServiceWorkerContainerConstructor(actor);
  if (NS_WARN_IF(!sentActor)) {
    Shutdown();
    return;
  }
  MOZ_DIAGNOSTIC_ASSERT(sentActor == actor);

  mActor = std::move(actor);
  mActor->SetOwner(this);

  Maybe<ServiceWorkerDescriptor> controller = aGlobal->GetController();
  if (controller.isSome()) {
    mControllerWorker = aGlobal->GetOrCreateServiceWorker(controller.ref());
  }
}

ServiceWorkerContainer::~ServiceWorkerContainer() { Shutdown(); }

void ServiceWorkerContainer::DisconnectFromOwner() {
  mControllerWorker = nullptr;
  mReadyPromise = nullptr;
  DOMEventTargetHelper::DisconnectFromOwner();
}

void ServiceWorkerContainer::ControllerChanged(ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> go = GetParentObject();
  if (!go) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  mControllerWorker = go->GetOrCreateServiceWorker(go->GetController().ref());
  aRv = DispatchTrustedEvent(u"controllerchange"_ns);
}

using mozilla::dom::ipc::StructuredCloneData;

// A ReceivedMessage represents a message sent via
// Client.postMessage(). It is used as used both for queuing of
// incoming messages and as an interface to DispatchMessage().
struct MOZ_HEAP_CLASS ServiceWorkerContainer::ReceivedMessage {
  explicit ReceivedMessage(const ClientPostMessageArgs& aArgs)
      : mServiceWorker(aArgs.serviceWorker()) {
    mClonedData.CopyFromClonedMessageData(aArgs.clonedData());
  }

  ServiceWorkerDescriptor mServiceWorker;
  StructuredCloneData mClonedData;

  NS_INLINE_DECL_REFCOUNTING(ReceivedMessage)

 private:
  ~ReceivedMessage() = default;
};

void ServiceWorkerContainer::ReceiveMessage(
    const ClientPostMessageArgs& aArgs) {
  RefPtr<ReceivedMessage> message = new ReceivedMessage(aArgs);
  if (mMessagesStarted) {
    EnqueueReceivedMessageDispatch(std::move(message));
  } else {
    mPendingMessages.AppendElement(message.forget());
  }
}

void ServiceWorkerContainer::RevokeActor(ServiceWorkerContainerChild* aActor) {
  MOZ_DIAGNOSTIC_ASSERT(mActor);
  MOZ_DIAGNOSTIC_ASSERT(mActor == aActor);
  mActor->RevokeOwner(this);
  mActor = nullptr;

  mShutdown = true;
}

JSObject* ServiceWorkerContainer::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return ServiceWorkerContainer_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<Promise> ServiceWorkerContainer::Register(
    const nsAString& aScriptURL, const RegistrationOptions& aOptions,
    const CallerType aCallerType, ErrorResult& aRv) {
  AUTO_PROFILER_MARKER_TEXT("SWC Register", DOM, {}, ""_ns);

  // Note, we can't use GetGlobalIfValid() from the start here.  If we
  // hit a storage failure we want to log a message with the final
  // scope string we put together below.
  nsIGlobalObject* global = GetParentObject();
  if (!global) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  Maybe<ClientInfo> clientInfo = global->GetClientInfo();
  if (clientInfo.isNothing()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  nsCOMPtr<nsIURI> baseURI = global->GetBaseURI();
  if (!baseURI) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  // Don't use NS_ConvertUTF16toUTF8 because that doesn't let us handle OOM.
  nsAutoCString scriptURL;
  if (!AppendUTF16toUTF8(aScriptURL, scriptURL, fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  nsCOMPtr<nsIURI> scriptURI;
  nsresult rv =
      NS_NewURI(getter_AddRefs(scriptURI), scriptURL, nullptr, baseURI);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.ThrowTypeError<MSG_INVALID_URL>(scriptURL);
    return nullptr;
  }

  // Never allow script URL with moz-extension scheme if support is fully
  // disabled by the 'extensions.background_service_worker.enabled' pref.
  if (scriptURI->SchemeIs("moz-extension") &&
      !StaticPrefs::extensions_backgroundServiceWorker_enabled_AtStartup()) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  // In ServiceWorkerContainer.register() the scope argument is parsed against
  // different base URLs depending on whether it was passed or not.
  nsCOMPtr<nsIURI> scopeURI;

  // Step 4. If none passed, parse against script's URL
  if (!aOptions.mScope.WasPassed()) {
    constexpr auto defaultScope = "./"_ns;
    rv = NS_NewURI(getter_AddRefs(scopeURI), defaultScope, nullptr, scriptURI);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      nsAutoCString spec;
      scriptURI->GetSpec(spec);
      aRv.ThrowTypeError<MSG_INVALID_SCOPE>(defaultScope, spec);
      return nullptr;
    }
  } else {
    // Step 5. Parse against entry settings object's base URL.
    rv = NS_NewURI(getter_AddRefs(scopeURI), aOptions.mScope.Value(), nullptr,
                   baseURI);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      nsIURI* uri = baseURI ? baseURI : scriptURI;
      nsAutoCString spec;
      uri->GetSpec(spec);
      aRv.ThrowTypeError<MSG_INVALID_SCOPE>(
          NS_ConvertUTF16toUTF8(aOptions.mScope.Value()), spec);
      return nullptr;
    }
  }

  // Strip the any ref from both the script and scope URLs.
  nsCOMPtr<nsIURI> cloneWithoutRef;
  aRv = NS_GetURIWithoutRef(scriptURI, getter_AddRefs(cloneWithoutRef));
  if (aRv.Failed()) {
    return nullptr;
  }
  scriptURI = std::move(cloneWithoutRef);

  aRv = NS_GetURIWithoutRef(scopeURI, getter_AddRefs(cloneWithoutRef));
  if (aRv.Failed()) {
    return nullptr;
  }
  scopeURI = std::move(cloneWithoutRef);

  ServiceWorkerScopeAndScriptAreValid(clientInfo.ref(), scopeURI, scriptURI,
                                      aRv, global);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Get the string representation for both the script and scope since
  // we sanitized them above.
  nsCString cleanedScopeURL;
  aRv = scopeURI->GetSpec(cleanedScopeURL);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsCString cleanedScriptURL;
  aRv = scriptURI->GetSpec(cleanedScriptURL);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Verify that the global is valid and has permission to store
  // data.  We perform this late so that we can report the final
  // scope URL in any error message.
  Unused << GetGlobalIfValid(aRv, [&](nsIGlobalObject* aGlobal) {
    AutoTArray<nsString, 1> param;
    CopyUTF8toUTF16(cleanedScopeURL, *param.AppendElement());
    aGlobal->ReportToConsole(nsIScriptError::errorFlag, "Service Workers"_ns,
                             nsContentUtils::eDOM_PROPERTIES,
                             "ServiceWorkerRegisterStorageError"_ns, param);
  });

  // TODO: For bug 1836707 we will move this tracking to ServiceWorkerManager
  // where it can establish the mapping between the job and our client info,
  // which will also work on workers.  For now we leave this notification for
  // Windows only.
  if (auto* window = global->GetAsInnerWindow()) {
    window->NoteCalledRegisterForServiceWorkerScope(cleanedScopeURL);
  }

  RefPtr<Promise> outer =
      Promise::Create(global, aRv, Promise::ePropagateUserInteraction);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<ServiceWorkerContainer> self = this;

  if (!mActor) {
    aRv.ThrowInvalidStateError("Can't register service worker");
    return nullptr;
  }

  mActor->SendRegister(
      clientInfo.ref().ToIPC(), nsCString(cleanedScopeURL),
      nsCString(cleanedScriptURL), aOptions.mUpdateViaCache,
      [self,
       outer](const IPCServiceWorkerRegistrationDescriptorOrCopyableErrorResult&
                  aResult) {
        AUTO_PROFILER_MARKER_TEXT("SWC Register (inner)", DOM, {}, ""_ns);

        if (aResult.type() ==
            IPCServiceWorkerRegistrationDescriptorOrCopyableErrorResult::
                TCopyableErrorResult) {
          // application layer error
          CopyableErrorResult rv = aResult.get_CopyableErrorResult();
          MOZ_DIAGNOSTIC_ASSERT(rv.Failed());
          outer->MaybeReject(std::move(rv));
          return;
        }
        // success
        const auto& ipcDesc =
            aResult.get_IPCServiceWorkerRegistrationDescriptor();
        ErrorResult rv;
        nsIGlobalObject* global = self->GetGlobalIfValid(rv);
        if (rv.Failed()) {
          outer->MaybeReject(std::move(rv));
          return;
        }
        RefPtr<ServiceWorkerRegistration> reg =
            global->GetOrCreateServiceWorkerRegistration(
                ServiceWorkerRegistrationDescriptor(ipcDesc));
        outer->MaybeResolve(reg);
      },
      [outer](ResponseRejectReason&& aReason) {
        // IPC layer error
        CopyableErrorResult rv;
        rv.ThrowInvalidStateError("Failed to register service worker");
        outer->MaybeReject(std::move(rv));
      });

  return outer.forget();
}

already_AddRefed<ServiceWorker> ServiceWorkerContainer::GetController() {
  RefPtr<ServiceWorker> ref = mControllerWorker;
  return ref.forget();
}

already_AddRefed<Promise> ServiceWorkerContainer::GetRegistrations(
    ErrorResult& aRv) {
  nsIGlobalObject* global = GetGlobalIfValid(aRv, [](nsIGlobalObject* aGlobal) {
    aGlobal->ReportToConsole(nsIScriptError::errorFlag, "Service Workers"_ns,
                             nsContentUtils::eDOM_PROPERTIES,
                             "ServiceWorkerGetRegistrationStorageError"_ns);
  });
  if (aRv.Failed()) {
    return nullptr;
  }

  Maybe<ClientInfo> clientInfo = global->GetClientInfo();
  if (clientInfo.isNothing()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  RefPtr<Promise> outer =
      Promise::Create(global, aRv, Promise::ePropagateUserInteraction);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<ServiceWorkerContainer> self = this;

  if (!mActor) {
    outer->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return outer.forget();
  }

  mActor->SendGetRegistrations(
      clientInfo.ref().ToIPC(),
      [self, outer](
          const IPCServiceWorkerRegistrationDescriptorListOrCopyableErrorResult&
              aResult) {
        if (aResult.type() ==
            IPCServiceWorkerRegistrationDescriptorListOrCopyableErrorResult::
                TCopyableErrorResult) {
          // application layer error
          const auto& rv = aResult.get_CopyableErrorResult();
          MOZ_DIAGNOSTIC_ASSERT(rv.Failed());
          outer->MaybeReject(CopyableErrorResult(rv));
          return;
        }
        // success
        const auto& ipcList =
            aResult.get_IPCServiceWorkerRegistrationDescriptorList();
        nsTArray<ServiceWorkerRegistrationDescriptor> list(
            ipcList.values().Length());
        for (const auto& ipcDesc : ipcList.values()) {
          list.AppendElement(ServiceWorkerRegistrationDescriptor(ipcDesc));
        }

        ErrorResult rv;
        nsIGlobalObject* global = self->GetGlobalIfValid(rv);
        if (rv.Failed()) {
          outer->MaybeReject(std::move(rv));
          return;
        }
        nsTArray<RefPtr<ServiceWorkerRegistration>> regList;
        for (auto& desc : list) {
          RefPtr<ServiceWorkerRegistration> reg =
              global->GetOrCreateServiceWorkerRegistration(desc);
          if (reg) {
            regList.AppendElement(std::move(reg));
          }
        }
        outer->MaybeResolve(regList);
      },
      [outer](ResponseRejectReason&& aReason) {
        // IPC layer error
        outer->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
      });

  return outer.forget();
}

void ServiceWorkerContainer::StartMessages() {
  while (!mPendingMessages.IsEmpty()) {
    EnqueueReceivedMessageDispatch(mPendingMessages.ElementAt(0));
    mPendingMessages.RemoveElementAt(0);
  }
  mMessagesStarted = true;
}

already_AddRefed<Promise> ServiceWorkerContainer::GetRegistration(
    const nsAString& aURL, ErrorResult& aRv) {
  nsIGlobalObject* global = GetGlobalIfValid(aRv, [](nsIGlobalObject* aGlobal) {
    aGlobal->ReportToConsole(nsIScriptError::errorFlag, "Service Workers"_ns,
                             nsContentUtils::eDOM_PROPERTIES,
                             "ServiceWorkerGetRegistrationStorageError"_ns);
  });
  if (aRv.Failed()) {
    return nullptr;
  }

  Maybe<ClientInfo> clientInfo = global->GetClientInfo();
  if (clientInfo.isNothing()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  nsCOMPtr<nsIURI> baseURI = global->GetBaseURI();
  if (!baseURI) {
    return nullptr;
  }

  nsCOMPtr<nsIURI> uri;
  aRv = NS_NewURI(getter_AddRefs(uri), aURL, nullptr, baseURI);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsCString spec;
  aRv = uri->GetSpec(spec);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<Promise> outer =
      Promise::Create(global, aRv, Promise::ePropagateUserInteraction);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<ServiceWorkerContainer> self = this;

  if (!mActor) {
    outer->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return outer.forget();
  }

  mActor->SendGetRegistration(
      clientInfo.ref().ToIPC(), spec,
      [self,
       outer](const IPCServiceWorkerRegistrationDescriptorOrCopyableErrorResult&
                  aResult) {
        if (aResult.type() ==
            IPCServiceWorkerRegistrationDescriptorOrCopyableErrorResult::
                TCopyableErrorResult) {
          CopyableErrorResult ipcRv(aResult.get_CopyableErrorResult());
          ErrorResult rv(std::move(ipcRv));
          if (!rv.Failed()) {
            // ErrorResult rv;
            //  If rv is a failure then this is an application layer error.
            //  Note, though, we also reject with NS_OK to indicate that we just
            //  didn't find a registration.
            Unused << self->GetGlobalIfValid(rv);
            if (!rv.Failed()) {
              outer->MaybeResolveWithUndefined();
              return;
            }
          }
          outer->MaybeReject(std::move(rv));
          return;
        }
        // success
        const auto& ipcDesc =
            aResult.get_IPCServiceWorkerRegistrationDescriptor();
        ErrorResult rv;
        nsIGlobalObject* global = self->GetGlobalIfValid(rv);
        if (rv.Failed()) {
          outer->MaybeReject(std::move(rv));
          return;
        }
        RefPtr<ServiceWorkerRegistration> reg =
            global->GetOrCreateServiceWorkerRegistration(
                ServiceWorkerRegistrationDescriptor(ipcDesc));
        outer->MaybeResolve(reg);
      },
      [self, outer](ResponseRejectReason&& aReason) {
        // IPC layer error
        outer->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
      });
  return outer.forget();
}

Promise* ServiceWorkerContainer::GetReady(ErrorResult& aRv) {
  if (mReadyPromise) {
    return mReadyPromise;
  }

  nsIGlobalObject* global = GetGlobalIfValid(aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  MOZ_DIAGNOSTIC_ASSERT(global);

  Maybe<ClientInfo> clientInfo(global->GetClientInfo());
  if (clientInfo.isNothing()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  mReadyPromise =
      Promise::Create(global, aRv, Promise::ePropagateUserInteraction);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<ServiceWorkerContainer> self = this;
  RefPtr<Promise> outer = mReadyPromise;

  if (!mActor) {
    mReadyPromise->MaybeReject(
        CopyableErrorResult(NS_ERROR_DOM_INVALID_STATE_ERR));
    return mReadyPromise;
  }

  mActor->SendGetReady(
      clientInfo.ref().ToIPC(),
      [self,
       outer](const IPCServiceWorkerRegistrationDescriptorOrCopyableErrorResult&
                  aResult) {
        if (aResult.type() ==
            IPCServiceWorkerRegistrationDescriptorOrCopyableErrorResult::
                TCopyableErrorResult) {
          // application layer error
          CopyableErrorResult rv(aResult.get_CopyableErrorResult());
          MOZ_DIAGNOSTIC_ASSERT(rv.Failed());
          outer->MaybeReject(std::move(rv));
          return;
        }
        // success
        const auto& ipcDesc =
            aResult.get_IPCServiceWorkerRegistrationDescriptor();
        ErrorResult rv;
        nsIGlobalObject* global = self->GetGlobalIfValid(rv);
        if (rv.Failed()) {
          outer->MaybeReject(std::move(rv));
          return;
        }
        RefPtr<ServiceWorkerRegistration> reg =
            global->GetOrCreateServiceWorkerRegistration(
                ServiceWorkerRegistrationDescriptor(ipcDesc));
        NS_ENSURE_TRUE_VOID(reg);

        // Don't resolve the ready promise until the registration has
        // reached the right version.  This ensures that the active
        // worker property is set correctly on the registration.
        reg->WhenVersionReached(ipcDesc.version(), [outer, reg](bool aResult) {
          outer->MaybeResolve(reg);
        });
      },
      [outer](ResponseRejectReason&& aReason) {
        // IPC layer error
        outer->MaybeReject(CopyableErrorResult(NS_ERROR_DOM_INVALID_STATE_ERR));
      });

  return mReadyPromise;
}

nsIGlobalObject* ServiceWorkerContainer::GetGlobalIfValid(
    ErrorResult& aRv,
    const std::function<void(nsIGlobalObject*)>&& aStorageFailureCB) const {
  nsIGlobalObject* global = GetOwnerGlobal();
  if (NS_WARN_IF(!global)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  if (NS_FAILED(CheckCurrentGlobalCorrectness())) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  // Don't allow a service worker to access service worker registrations
  // from a global with storage disabled.  If these globals can access
  // the registration it increases the chance they can bypass the storage
  // block via postMessage(), etc.
  auto storageAllowed = global->GetStorageAccess();
  if (NS_WARN_IF(storageAllowed != StorageAccess::eAllow &&
                 (!StaticPrefs::privacy_partition_serviceWorkers() ||
                  !StoragePartitioningEnabled(
                      storageAllowed, global->GetCookieJarSettings())))) {
    if (aStorageFailureCB) {
      aStorageFailureCB(global);
    }
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  // Don't allow service workers for system principals.
  nsIPrincipal* principal = global->PrincipalOrNull();
  if (NS_WARN_IF(!principal || principal->IsSystemPrincipal())) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  return global;
}

void ServiceWorkerContainer::EnqueueReceivedMessageDispatch(
    RefPtr<ReceivedMessage> aMessage) {
  NS_DispatchToMainThread(NewRunnableMethod<RefPtr<ReceivedMessage>>(
      "ServiceWorkerContainer::DispatchMessage", this,
      &ServiceWorkerContainer::DispatchMessage, std::move(aMessage)));
}

template <typename F>
void ServiceWorkerContainer::RunWithJSContext(F&& aCallable) {
  nsCOMPtr<nsIGlobalObject> globalObject = GetOwnerGlobal();

  // If AutoJSAPI::Init() fails then either global is nullptr or not
  // in a usable state.
  AutoJSAPI jsapi;
  if (!jsapi.Init(globalObject)) {
    return;
  }

  aCallable(jsapi.cx(), globalObject);
}

void ServiceWorkerContainer::DispatchMessage(RefPtr<ReceivedMessage> aMessage) {
  nsresult rv = CheckCurrentGlobalCorrectness();
  if (NS_FAILED(rv)) {
    return;
  }

  // When dispatching a message, either DOMContentLoaded has already
  // been fired, or someone called startMessages() or set onmessage.
  // Either way, a global object is supposed to be present. If it's
  // not, we'd fail to initialize the JS API and exit.
  RunWithJSContext([this, message = std::move(aMessage)](
                       JSContext* const aCx, nsIGlobalObject* const aGlobal) {
    ErrorResult result;
    bool deserializationFailed = false;
    RootedDictionary<MessageEventInit> init(aCx);
    auto res = FillInMessageEventInit(aCx, aGlobal, *message, init, result);
    if (res.isErr()) {
      deserializationFailed = res.unwrapErr();
      MOZ_ASSERT_IF(deserializationFailed, init.mData.isNull());
      MOZ_ASSERT_IF(deserializationFailed, init.mPorts.IsEmpty());
      MOZ_ASSERT_IF(deserializationFailed, !init.mOrigin.IsEmpty());
      MOZ_ASSERT_IF(deserializationFailed, !init.mSource.IsNull());
      result.SuppressException();

      if (!deserializationFailed && result.MaybeSetPendingException(aCx)) {
        return;
      }
    }

    RefPtr<MessageEvent> event = MessageEvent::Constructor(
        this, deserializationFailed ? u"messageerror"_ns : u"message"_ns, init);
    event->SetTrusted(true);

    result = NS_OK;
    DispatchEvent(*event, result);
    if (result.Failed()) {
      result.SuppressException();
    }
  });
}

namespace {

nsresult FillInOriginNoSuffix(const ServiceWorkerDescriptor& aServiceWorker,
                              nsString& aOrigin) {
  using mozilla::ipc::PrincipalInfoToPrincipal;

  nsresult rv;

  auto principalOrErr =
      PrincipalInfoToPrincipal(aServiceWorker.PrincipalInfo());
  if (NS_WARN_IF(principalOrErr.isErr())) {
    return principalOrErr.unwrapErr();
  }

  nsAutoCString originUTF8;
  rv = principalOrErr.unwrap()->GetOriginNoSuffix(originUTF8);
  if (NS_FAILED(rv)) {
    return rv;
  }

  CopyUTF8toUTF16(originUTF8, aOrigin);
  return NS_OK;
}

}  // namespace

Result<Ok, bool> ServiceWorkerContainer::FillInMessageEventInit(
    JSContext* const aCx, nsIGlobalObject* const aGlobal,
    ReceivedMessage& aMessage, MessageEventInit& aInit, ErrorResult& aRv) {
  // Determining the source and origin should preceed attempting deserialization
  // because on a "messageerror" event (i.e. when deserialization fails), the
  // dispatched message needs to contain such an origin and source, per spec:
  //
  // "If this throws an exception, catch it, fire an event named messageerror
  //  at destination, using MessageEvent, with the origin attribute initialized
  //  to origin and the source attribute initialized to source, and then abort
  //  these steps." - 6.4 of postMessage
  //  See: https://w3c.github.io/ServiceWorker/#service-worker-postmessage
  const RefPtr<ServiceWorker> serviceWorkerInstance =
      aGlobal->GetOrCreateServiceWorker(aMessage.mServiceWorker);
  if (serviceWorkerInstance) {
    aInit.mSource.SetValue().SetAsServiceWorker() = serviceWorkerInstance;
  }

  const nsresult rv =
      FillInOriginNoSuffix(aMessage.mServiceWorker, aInit.mOrigin);
  if (NS_FAILED(rv)) {
    return Err(false);
  }

  JS::Rooted<JS::Value> messageData(aCx);
  aMessage.mClonedData.Read(aCx, &messageData, aRv);
  if (aRv.Failed()) {
    return Err(true);
  }

  aInit.mData = messageData;

  if (!aMessage.mClonedData.TakeTransferredPortsAsSequence(aInit.mPorts)) {
    xpc::Throw(aCx, NS_ERROR_OUT_OF_MEMORY);
    return Err(false);
  }

  return Ok();
}

void ServiceWorkerContainer::Shutdown() {
  if (mShutdown) {
    return;
  }
  mShutdown = true;

  if (mActor) {
    mActor->RevokeOwner(this);
    mActor->MaybeStartTeardown();
    mActor = nullptr;
  }
}

}  // namespace mozilla::dom
