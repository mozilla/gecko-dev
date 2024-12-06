/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MLS.h"
#include "mozilla/dom/MLSGroupView.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/Promise.h"
#include "nsTArray.h"
#include "nsCOMPtr.h"
#include "nsIGlobalObject.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/dom/MLSTransactionChild.h"
#include "mozilla/dom/MLSTransactionMessage.h"
#include "mozilla/dom/PMLSTransaction.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/BasePrincipal.h"
#include "MLSGroupView.h"
#include "nsTArray.h"
#include "mozilla/Logging.h"
#include "mozilla/Span.h"
#include "nsDebug.h"
#include "MLSLogging.h"
#include "MLSTypeUtils.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MLS, mGlobalObject)

NS_IMPL_CYCLE_COLLECTING_ADDREF(MLS)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MLS)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MLS)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

// Setup logging
mozilla::LazyLogModule gMlsLog("MLS");

/* static */ already_AddRefed<MLS> MLS::Constructor(GlobalObject& aGlobalObject,
                                                    ErrorResult& aRv) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug, ("MLS::Constructor()"));

  nsCOMPtr<nsIGlobalObject> global(
      do_QueryInterface(aGlobalObject.GetAsSupports()));
  if (NS_WARN_IF(!global)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // Get the principal and perform some validation on it.
  // We do not allow MLS in Private Browsing Mode for now.
  nsIPrincipal* principal = global->PrincipalOrNull();
  if (!principal || !principal->GetIsContentPrincipal() ||
      principal->GetIsInPrivateBrowsing()) {
    aRv.ThrowSecurityError("Cannot create MLS store for origin");
    return nullptr;
  }

  // Create the endpoints for the MLS actor
  mozilla::ipc::Endpoint<PMLSTransactionParent> parentEndpoint;
  mozilla::ipc::Endpoint<PMLSTransactionChild> childEndpoint;
  MOZ_ALWAYS_SUCCEEDS(
      PMLSTransaction::CreateEndpoints(&parentEndpoint, &childEndpoint));

  mozilla::ipc::PBackgroundChild* backgroundChild =
      mozilla::ipc::BackgroundChild::GetOrCreateForCurrentThread();
  if (!backgroundChild) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  // Bind the child actor, and send the parent endpoint.
  RefPtr<MLSTransactionChild> actor = new MLSTransactionChild();
  MOZ_ALWAYS_TRUE(childEndpoint.Bind(actor));

  MOZ_ALWAYS_TRUE(backgroundChild->SendCreateMLSTransaction(
      std::move(parentEndpoint), WrapNotNull(principal)));

  return MakeAndAddRef<MLS>(global, actor);
}

MLS::MLS(nsIGlobalObject* aGlobalObject, MLSTransactionChild* aActor)
    : mGlobalObject(aGlobalObject), mTransactionChild(aActor) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug, ("MLS::MLS()"));
}

MLS::~MLS() {
  if (mTransactionChild) {
    mTransactionChild->Close();
  }
}

JSObject* MLS::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return MLS_Binding::Wrap(aCx, this, aGivenProto);
}

//
// API
//

already_AddRefed<Promise> MLS::DeleteState(ErrorResult& aRv) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug, ("MLS::DeleteState()"));

  // Create a new Promise object for the result
  RefPtr<Promise> promise = Promise::Create(mGlobalObject, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  mTransactionChild->SendRequestStateDelete(
      [promise](bool result) {
        if (result) {
          promise->MaybeResolveWithUndefined();
        } else {
          promise->MaybeReject(NS_ERROR_FAILURE);
        }
      },
      [promise](::mozilla::ipc::ResponseRejectReason) {
        promise->MaybeRejectWithUnknownError("deleteState failed");
      });

  return promise.forget();
}

already_AddRefed<Promise> MLS::GenerateIdentity(ErrorResult& aRv) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug, ("MLS::GenerateIdentity()"));

  // Create a new Promise object for the result
  RefPtr<Promise> promise = Promise::Create(mGlobalObject, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  mTransactionChild->SendRequestGenerateIdentityKeypair()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [promise, self = RefPtr{this}](Maybe<RawBytes>&& result) {
        // Check if the value is Nothing
        if (result.isNothing()) {
          promise->MaybeRejectWithUnknownError(
              "generateIdentityKeypair failed");
          return;
        }

        // Get the context from the GlobalObject
        AutoJSAPI jsapi;
        if (NS_WARN_IF(!jsapi.Init(self->mGlobalObject))) {
          promise->MaybeRejectWithUnknownError(
              "generateIdentityKeypair failed");
          return;
        }
        JSContext* cx = jsapi.cx();

        // Construct the Uint8Array object
        ErrorResult error;
        JS::Rooted<JSObject*> content(
            cx, Uint8Array::Create(cx, result->data(), error));
        error.WouldReportJSException();
        if (error.Failed()) {
          promise->MaybeReject(std::move(error));
          return;
        }

        // Construct MLSBytes with the client identifer as content
        RootedDictionary<MLSBytes> rvalue(cx);
        rvalue.mType = MLSObjectType::Client_identifier;
        rvalue.mContent.Init(content);

        // Resolve the promise with the MLSBytes object
        promise->MaybeResolve(rvalue);
      },
      [promise](::mozilla::ipc::ResponseRejectReason aReason) {
        promise->MaybeRejectWithUnknownError("generateIdentity failed");
      });

  return promise.forget();
}

already_AddRefed<Promise> MLS::GenerateCredential(
    const MLSBytesOrUint8ArrayOrUTF8String& aJsCredContent, ErrorResult& aRv) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
          ("MLS::GenerateCredentialBasic()"));

  // Handle the credential content parameter
  nsTArray<uint8_t> credContent = ExtractMLSBytesOrUint8ArrayOrUTF8String(
      MLSObjectType::Credential_basic, aJsCredContent, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Check if the credContent is empty
  if (NS_WARN_IF(credContent.IsEmpty())) {
    aRv.ThrowTypeError("The credential content must not be empty");
    return nullptr;
  }

  // Create a new Promise object for the result
  RefPtr<Promise> promise = Promise::Create(mGlobalObject, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  mTransactionChild->SendRequestGenerateCredentialBasic(credContent)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [promise, self = RefPtr{this}](Maybe<RawBytes>&& result) {
            // Check if the value is Nothing
            if (result.isNothing()) {
              promise->MaybeRejectWithUnknownError(
                  "generateCredentialBasic failed");
              return;
            }

            // Get the context from the GlobalObject
            AutoJSAPI jsapi;
            if (NS_WARN_IF(!jsapi.Init(self->mGlobalObject))) {
              promise->MaybeRejectWithUnknownError(
                  "generateCredentialBasic failed");
              return;
            }
            JSContext* cx = jsapi.cx();

            // Construct the Uint8Array object
            ErrorResult error;
            JS::Rooted<JSObject*> content(
                cx, Uint8Array::Create(cx, result->data(), error));
            error.WouldReportJSException();
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }

            // Construct MLSBytes with the client identifer as content
            RootedDictionary<MLSBytes> rvalue(cx);
            rvalue.mType = MLSObjectType::Credential_basic;
            rvalue.mContent.Init(content);

            // Resolve the promise
            promise->MaybeResolve(rvalue);
          },
          [promise](::mozilla::ipc::ResponseRejectReason aReason) {
            promise->MaybeRejectWithUnknownError(
                "generateCredentialBasic failed");
          });

  return promise.forget();
}

already_AddRefed<Promise> MLS::GenerateKeyPackage(
    const MLSBytesOrUint8Array& aJsClientIdentifier,
    const MLSBytesOrUint8Array& aJsCredential, ErrorResult& aRv) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug, ("MLS::GenerateKeyPackage()"));

  // Handle the client identifier parameter
  nsTArray<uint8_t> clientIdentifier = ExtractMLSBytesOrUint8Array(
      MLSObjectType::Client_identifier, aJsClientIdentifier, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Check if the client identifier is empty
  if (NS_WARN_IF(clientIdentifier.IsEmpty())) {
    aRv.ThrowTypeError("The client identifier must not be empty");
    return nullptr;
  }

  // Handle the credential parameter
  nsTArray<uint8_t> credential = ExtractMLSBytesOrUint8Array(
      MLSObjectType::Credential_basic, aJsCredential, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Check if the credential is empty
  if (NS_WARN_IF(credential.IsEmpty())) {
    aRv.ThrowTypeError("The credential must not be empty");
    return nullptr;
  }

  // Create a new Promise object for the result
  RefPtr<Promise> promise = Promise::Create(mGlobalObject, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Use the static method or instance to send the IPC message
  mTransactionChild->SendRequestGenerateKeyPackage(clientIdentifier, credential)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [promise, self = RefPtr{this}](Maybe<RawBytes>&& keyPackage) {
            // Check if the value is Nothing
            if (keyPackage.isNothing()) {
              promise->MaybeReject(NS_ERROR_FAILURE);
              return;
            }

            // Get the context from the GlobalObject
            AutoJSAPI jsapi;
            if (NS_WARN_IF(!jsapi.Init(self->mGlobalObject))) {
              promise->MaybeReject(NS_ERROR_FAILURE);
              return;
            }
            JSContext* cx = jsapi.cx();

            // Construct the Uint8Array object
            ErrorResult error;
            JS::Rooted<JSObject*> content(
                cx, Uint8Array::Create(cx, keyPackage->data(), error));
            error.WouldReportJSException();
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }

            // Construct MLSBytes with the client identifer as content
            RootedDictionary<MLSBytes> rvalue(cx);
            rvalue.mType = MLSObjectType::Key_package;
            rvalue.mContent.Init(content);

            // Resolve the promise
            promise->MaybeResolve(rvalue);
          },
          [promise](::mozilla::ipc::ResponseRejectReason aReason) {
            promise->MaybeRejectWithUnknownError("generateKeyPackage failed");
          });

  return promise.forget();
}

already_AddRefed<Promise> MLS::GroupCreate(
    const MLSBytesOrUint8Array& aJsClientIdentifier,
    const MLSBytesOrUint8Array& aJsCredential, ErrorResult& aRv) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug, ("MLS::GroupCreate()"));

  // Handle the client identifier parameter
  nsTArray<uint8_t> clientIdentifier = ExtractMLSBytesOrUint8Array(
      MLSObjectType::Client_identifier, aJsClientIdentifier, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Check if the client identifier is empty
  if (NS_WARN_IF(clientIdentifier.IsEmpty())) {
    aRv.ThrowTypeError("The client identifier must not be empty");
    return nullptr;
  }

  // Handle the credential parameter
  nsTArray<uint8_t> credential = ExtractMLSBytesOrUint8Array(
      MLSObjectType::Credential_basic, aJsCredential, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Check if the credential is empty
  if (NS_WARN_IF(credential.IsEmpty())) {
    aRv.ThrowTypeError("The credential must not be empty");
    return nullptr;
  }

  // Log the hex of clientIdentifier
  if (MOZ_LOG_TEST(gMlsLog, LogLevel::Debug)) {
    nsAutoCString clientIdHex;
    for (uint8_t byte : clientIdentifier) {
      clientIdHex.AppendPrintf("%02X", byte);
    }
    MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
            ("clientIdentifier in hex: %s\n", clientIdHex.get()));
  }

  // Initialize jsGroupIdentifier to one byte of value 0xFF.
  // We do not want to allow choosing the GID at this point.
  // This value not being of the correct length will be discarded
  // internally and a fresh GID will be generated.
  //
  // In the future, the caller will allow choosing the GID.
  AutoTArray<uint8_t, 1> groupIdentifier{0xFF};

  // Create a new Promise object for the result
  RefPtr<Promise> promise = Promise::Create(mGlobalObject, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Use the static method or instance to send the IPC message
  mTransactionChild
      ->SendRequestGroupCreate(clientIdentifier, credential, groupIdentifier)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [promise, self = RefPtr{this},
           clientIdentifier(std::move(clientIdentifier))](
              Maybe<mozilla::security::mls::GkGroupIdEpoch>&&
                  groupIdEpoch) mutable {
            // Check if the value is Nothing
            if (groupIdEpoch.isNothing()) {
              promise->MaybeReject(NS_ERROR_FAILURE);
              return;
            }

            RefPtr<MLSGroupView> group =
                new MLSGroupView(self, std::move(groupIdEpoch->group_id),
                                 std::move(clientIdentifier));

            promise->MaybeResolve(group);
          },
          [promise](::mozilla::ipc::ResponseRejectReason aReason) {
            MOZ_LOG(gMlsLog, mozilla::LogLevel::Error,
                    ("IPC message rejected with reason: %d",
                     static_cast<int>(aReason)));
            promise->MaybeRejectWithUnknownError("groupCreate failed");
          });

  return promise.forget();
}

already_AddRefed<mozilla::dom::Promise> MLS::GroupGet(
    const MLSBytesOrUint8Array& aJsGroupIdentifier,
    const MLSBytesOrUint8Array& aJsClientIdentifier, ErrorResult& aRv) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug, ("MLS::GroupGet()"));

  // Handle the group identifier parameter
  nsTArray<uint8_t> groupIdentifier = ExtractMLSBytesOrUint8Array(
      MLSObjectType::Group_identifier, aJsGroupIdentifier, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Check if the group identifier is empty
  if (NS_WARN_IF(groupIdentifier.IsEmpty())) {
    aRv.ThrowTypeError("The group identifier must not be empty");
    return nullptr;
  }

  // Handle the client identifier parameter
  nsTArray<uint8_t> clientIdentifier = ExtractMLSBytesOrUint8Array(
      MLSObjectType::Client_identifier, aJsClientIdentifier, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Check if the client identifier is empty
  if (NS_WARN_IF(clientIdentifier.IsEmpty())) {
    aRv.ThrowTypeError("The client identifier must not be empty");
    return nullptr;
  }

  // Create a new Promise object for the result
  RefPtr<Promise> promise = Promise::Create(mGlobalObject, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Initialize label, context and len
  // We pass this through IPC to be able to reuse the same code for different
  // labels in the future
  AutoTArray<uint8_t, 7> label{'l', 'i', 'v', 'e', 'n', 'e', 's', 's'};
  AutoTArray<uint8_t, 1> context{0x00};
  uint64_t len = 32;

  // Use the static method or instance to send the IPC message
  mTransactionChild
      ->SendRequestExportSecret(groupIdentifier, clientIdentifier, label,
                                context, len)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [promise, self = RefPtr{this},
           groupIdentifier(std::move(groupIdentifier)),
           clientIdentifier(std::move(clientIdentifier))](
              Maybe<mozilla::security::mls::GkExporterOutput>&&
                  exporterOutput) mutable {
            // Check if the exporterOutput contains a value
            if (exporterOutput.isNothing()) {
              promise->MaybeReject(NS_ERROR_FAILURE);
              return;
            }

            RefPtr<MLSGroupView> group =
                new MLSGroupView(self, std::move(exporterOutput->group_id),
                                 std::move(clientIdentifier));
            promise->MaybeResolve(group);
          },
          [promise](::mozilla::ipc::ResponseRejectReason aReason) {
            promise->MaybeRejectWithUnknownError("exportSecret failed");
          });

  return promise.forget();
}

already_AddRefed<Promise> MLS::GroupJoin(
    const MLSBytesOrUint8Array& aJsClientIdentifier,
    const MLSBytesOrUint8Array& aJsWelcome, ErrorResult& aRv) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug, ("MLS::GroupJoin()"));

  // Handle the client identifier parameter
  nsTArray<uint8_t> clientIdentifier = ExtractMLSBytesOrUint8Array(
      MLSObjectType::Client_identifier, aJsClientIdentifier, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Check if the client identifier is empty
  if (NS_WARN_IF(clientIdentifier.IsEmpty())) {
    aRv.ThrowTypeError("The client identifier must not be empty");
    return nullptr;
  }

  // Handle the welcome parameter
  nsTArray<uint8_t> welcome =
      ExtractMLSBytesOrUint8Array(MLSObjectType::Welcome, aJsWelcome, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Check if the welcome is empty
  if (NS_WARN_IF(welcome.IsEmpty())) {
    aRv.ThrowTypeError("The welcome must not be empty");
    return nullptr;
  }

  // Create a new Promise object for the result
  RefPtr<Promise> promise = Promise::Create(mGlobalObject, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  mTransactionChild->SendRequestGroupJoin(clientIdentifier, welcome)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [promise, self = RefPtr{this},
           clientIdentifier(std::move(clientIdentifier))](
              Maybe<mozilla::security::mls::GkGroupIdEpoch>&&
                  groupIdEpoch) mutable {
            // Check if the value is Nothing
            if (groupIdEpoch.isNothing()) {
              promise->MaybeReject(NS_ERROR_FAILURE);
              return;
            }

            // Returns groupId and epoch
            RefPtr<MLSGroupView> group =
                new MLSGroupView(self, std::move(groupIdEpoch->group_id),
                                 std::move(clientIdentifier));

            // Resolve the promise
            promise->MaybeResolve(group);
          },
          [promise](::mozilla::ipc::ResponseRejectReason aReason) {
            promise->MaybeRejectWithUnknownError("groupJoin failed");
          });

  return promise.forget();
}

already_AddRefed<Promise> MLS::GetGroupIdFromMessage(
    const MLSBytesOrUint8Array& aJsMessage, ErrorResult& aRv) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug, ("MLS::GetGroupIdFromMessage()"));

  // Handle the message parameter
  nsTArray<uint8_t> message =
      ExtractMLSBytesOrUint8ArrayWithUnknownType(aJsMessage, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Check if the message is empty
  if (NS_WARN_IF(message.IsEmpty())) {
    aRv.ThrowTypeError("The message must not be empty");
    return nullptr;
  }

  // Create a new Promise object for the result
  RefPtr<Promise> promise = Promise::Create(mGlobalObject, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  mTransactionChild->SendRequestGetGroupIdentifier(message)->Then(
      GetCurrentSerialEventTarget(), __func__,
      [promise, self = RefPtr{this},
       message(std::move(message))](Maybe<RawBytes>&& result) {
        // Check if the value is Nothing
        if (result.isNothing()) {
          promise->MaybeReject(NS_ERROR_FAILURE);
          return;
        }

        // Get the context from the GlobalObject
        AutoJSAPI jsapi;
        if (NS_WARN_IF(!jsapi.Init(self->mGlobalObject))) {
          MOZ_LOG(gMlsLog, mozilla::LogLevel::Error,
                  ("Failed to initialize JSAPI"));
          promise->MaybeReject(NS_ERROR_FAILURE);
          return;
        }
        JSContext* cx = jsapi.cx();

        // Construct the Uint8Array objects based on the tag
        ErrorResult error;
        JS::Rooted<JSObject*> jsGroupId(
            cx, Uint8Array::Create(cx, result->data(), error));
        error.WouldReportJSException();
        if (error.Failed()) {
          promise->MaybeReject(std::move(error));
          return;
        }

        // Construct the MLSBytes object for the groupId
        RootedDictionary<MLSBytes> rvalue(cx);
        rvalue.mType = MLSObjectType::Group_identifier;
        rvalue.mContent.Init(jsGroupId);

        // Log if in debug mode
        if (MOZ_LOG_TEST(gMlsLog, LogLevel::Debug)) {
          MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
                  ("Successfully constructed MLSBytes"));
        }

        // Resolve the promise
        promise->MaybeResolve(rvalue);
      },
      [promise](::mozilla::ipc::ResponseRejectReason aReason) {
        MOZ_LOG(
            gMlsLog, mozilla::LogLevel::Error,
            ("IPC call rejected with reason: %d", static_cast<int>(aReason)));
        promise->MaybeRejectWithUnknownError("getGroupIdFromMessage failed");
      });

  return promise.forget();
}

}  // namespace mozilla::dom
