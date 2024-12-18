/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MLSGroupView.h"
#include "mozilla/dom/MLSBinding.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/Promise.h"
#include "nsTArray.h"
#include "mozilla/dom/MLSTransactionChild.h"
#include "mozilla/dom/MLSTransactionMessage.h"
#include "ipc/IPCMessageUtilsSpecializations.h"
#include "mozilla/BasePrincipal.h"
#include "nsTArray.h"
#include "mozilla/Logging.h"
#include "mozilla/Span.h"
#include "nsDebug.h"
#include "MLSTypeUtils.h"
namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_WITH_JS_MEMBERS(MLSGroupView, (mMLS),
                                                      (mJsGroupId, mJsClientId))

NS_IMPL_CYCLE_COLLECTING_ADDREF(MLSGroupView)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MLSGroupView)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MLSGroupView)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

// Setup logging
extern mozilla::LazyLogModule gMlsLog;

MLSGroupView::MLSGroupView(MLS* aMLS, nsTArray<uint8_t>&& aGroupId,
                           nsTArray<uint8_t>&& aClientId)
    : mMLS(aMLS),
      mGroupId(std::move(aGroupId)),
      mClientId(std::move(aClientId)) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug, ("MLSGroupView::MLSGroupView()"));

  // Indicate that the object holds JS objects
  mozilla::HoldJSObjects(this);
}

JSObject* MLSGroupView::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return MLSGroupView_Binding::Wrap(aCx, this, aGivenProto);
}

//
// API
//

void MLSGroupView::GetGroupId(JSContext* aCx,
                              JS::MutableHandle<JSObject*> aGroupId,
                              ErrorResult& aRv) {
  if (!mJsGroupId) {
    mJsGroupId = Uint8Array::Create(aCx, this, mGroupId, aRv);
    if (aRv.Failed()) {
      return;
    }
  }
  aGroupId.set(mJsGroupId);
}

void MLSGroupView::GetClientId(JSContext* aCx,
                               JS::MutableHandle<JSObject*> aClientId,
                               ErrorResult& aRv) {
  if (!mJsClientId) {
    mJsClientId = Uint8Array::Create(aCx, this, mClientId, aRv);
    if (aRv.Failed()) {
      return;
    }
  }
  aClientId.set(mJsClientId);
}

already_AddRefed<Promise> MLSGroupView::DeleteState(ErrorResult& aRv) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug, ("MLSGroupView::DeleteState()"));

  // Create a new Promise object for the result
  RefPtr<Promise> promise = Promise::Create(mMLS->GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  mMLS->mTransactionChild->SendRequestGroupStateDelete(mGroupId, mClientId)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [promise](
              Maybe<mozilla::security::mls::GkGroupIdEpoch>&& groupIdEpoch) {
            // Check if the value is Nothing or Some with an empty group_epoch
            if (groupIdEpoch.isNothing()) {
              promise->MaybeRejectWithUnknownError(
                  "Failed to delete group state");
              return;
            }

            // Check if the epoch is 0xFFFF..FF
            bool isMaxEpoch =
                std::all_of(groupIdEpoch->group_epoch.begin(),
                            groupIdEpoch->group_epoch.end(),
                            [](uint8_t byte) { return byte == 0xFF; });

            // If the epoch is 0xFFFF..FF, then the group has been deleted
            if (isMaxEpoch) {
              promise->MaybeResolveWithUndefined();
            } else {
              promise->MaybeRejectWithUnknownError(
                  "Group has not been deleted");
            }
          },
          [promise](::mozilla::ipc::ResponseRejectReason aReason) {
            promise->MaybeRejectWithUnknownError(
                "Failed to delete group state");
          });

  return promise.forget();
}

already_AddRefed<Promise> MLSGroupView::Add(
    const MLSBytesOrUint8Array& aJsKeyPackage, ErrorResult& aRv) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug, ("MLSGroupView::Add()"));

  // Handle the key package parameter
  nsTArray<uint8_t> keyPackage = ExtractMLSBytesOrUint8Array(
      MLSObjectType::Key_package, aJsKeyPackage, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Check if the key package is empty
  if (NS_WARN_IF(keyPackage.IsEmpty())) {
    aRv.ThrowTypeError("The key package must not be empty");
    return nullptr;
  }

  // Create a new Promise object for the result
  RefPtr<Promise> promise = Promise::Create(mMLS->GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  mMLS->mTransactionChild->SendRequestGroupAdd(mGroupId, mClientId, keyPackage)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [promise, self = RefPtr<MLSGroupView>(this)](
              Maybe<mozilla::security::mls::GkMlsCommitOutput>&& commitOutput) {
            // Check if the value is Nothing
            if (commitOutput.isNothing()) {
              promise->MaybeReject(NS_ERROR_FAILURE);
              return;
            }

            // Get the context from the GlobalObject
            AutoJSAPI jsapi;
            if (NS_WARN_IF(!jsapi.Init(self->mMLS->GetParentObject()))) {
              promise->MaybeReject(NS_ERROR_FAILURE);
              return;
            }
            JSContext* cx = jsapi.cx();

            // Construct the Uint8Array objects
            ErrorResult error;
            JS::Rooted<JSObject*> jsGroupId(
                cx, Uint8Array::Create(cx, self->mGroupId, error));
            error.WouldReportJSException();
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }
            JS::Rooted<JSObject*> jsClientId(
                cx, Uint8Array::Create(cx, commitOutput->identity, error));
            error.WouldReportJSException();
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }
            JS::Rooted<JSObject*> jsCommit(
                cx, Uint8Array::Create(cx, commitOutput->commit, error));
            error.WouldReportJSException();
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }
            JS::Rooted<JSObject*> jsWelcome(
                cx, Uint8Array::Create(cx, commitOutput->welcome, error));
            error.WouldReportJSException();
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }
            JS::Rooted<JSObject*> jsGroupInfo(
                cx, Uint8Array::Create(cx, commitOutput->group_info, error));
            error.WouldReportJSException();
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }
            JS::Rooted<JSObject*> jsRatchetTree(
                cx, Uint8Array::Create(cx, commitOutput->ratchet_tree, error));
            error.WouldReportJSException();
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }

            // Construct MLSCommitOutput with the parsed data
            RootedDictionary<MLSCommitOutput> rvalue(cx);
            rvalue.mType = MLSObjectType::Commit_output;
            rvalue.mGroupId.Init(jsGroupId);
            rvalue.mCommit.Init(jsCommit);
            if (!commitOutput->welcome.IsEmpty()) {
              rvalue.mWelcome.Construct();
              rvalue.mWelcome.Value().Init(jsWelcome);
            }
            if (!commitOutput->group_info.IsEmpty()) {
              rvalue.mGroupInfo.Construct();
              rvalue.mGroupInfo.Value().Init(jsGroupInfo);
            }
            if (!commitOutput->ratchet_tree.IsEmpty()) {
              rvalue.mRatchetTree.Construct();
              rvalue.mRatchetTree.Value().Init(jsRatchetTree);
            }
            if (!commitOutput->identity.IsEmpty()) {
              rvalue.mClientId.Construct();
              rvalue.mClientId.Value().Init(jsClientId);
            }

            // Resolve the promise
            promise->MaybeResolve(rvalue);
          },
          [promise](::mozilla::ipc::ResponseRejectReason aReason) {
            promise->MaybeRejectWithUnknownError("Failed to add to group");
          });

  return promise.forget();
}

already_AddRefed<Promise> MLSGroupView::ProposeAdd(
    const MLSBytesOrUint8Array& aJsKeyPackage, ErrorResult& aRv) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug, ("MLSGroupView::ProposeAdd()"));

  // Handle the key package parameter
  nsTArray<uint8_t> keyPackage = ExtractMLSBytesOrUint8Array(
      MLSObjectType::Key_package, aJsKeyPackage, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Check if the key package is empty
  if (NS_WARN_IF(keyPackage.IsEmpty())) {
    aRv.ThrowTypeError("The key package must not be empty");
    return nullptr;
  }

  // Create a new Promise object for the result
  RefPtr<Promise> promise = Promise::Create(mMLS->GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  mMLS->mTransactionChild
      ->SendRequestGroupProposeAdd(mGroupId, mClientId, keyPackage)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [promise,
           self = RefPtr<MLSGroupView>(this)](Maybe<RawBytes>&& proposal) {
            // Check if the value is Nothing
            if (proposal.isNothing()) {
              promise->MaybeRejectWithUnknownError(
                  "Failed to propose add to group");
              return;
            }

            // Get the context from the GlobalObject
            AutoJSAPI jsapi;
            if (NS_WARN_IF(!jsapi.Init(self->mMLS->GetParentObject()))) {
              promise->MaybeReject(NS_ERROR_FAILURE);
              return;
            }
            JSContext* cx = jsapi.cx();

            // Construct the Uint8Array object
            ErrorResult error;
            JS::Rooted<JSObject*> content(
                cx, Uint8Array::Create(cx, proposal->data(), error));
            error.WouldReportJSException();
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }

            // Construct MLSBytes with the proposal as content
            RootedDictionary<MLSBytes> rvalue(cx);
            rvalue.mType = MLSObjectType::Proposal;
            rvalue.mContent.Init(content);

            // Resolve the promise
            promise->MaybeResolve(rvalue);
          },
          [promise](::mozilla::ipc::ResponseRejectReason aReason) {
            promise->MaybeRejectWithUnknownError(
                "Failed to propose add to group");
          });

  return promise.forget();
}

already_AddRefed<Promise> MLSGroupView::Remove(
    const MLSBytesOrUint8Array& aJsRemClientIdentifier, ErrorResult& aRv) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug, ("MLSGroupView::Remove()"));

  // Handle the remove client identifier parameter
  nsTArray<uint8_t> remClientIdentifier = ExtractMLSBytesOrUint8Array(
      MLSObjectType::Client_identifier, aJsRemClientIdentifier, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Check if the remove client identifier is empty
  if (NS_WARN_IF(remClientIdentifier.IsEmpty())) {
    aRv.ThrowTypeError("The remove client identifier must not be empty");
    return nullptr;
  }

  // Create a new Promise object for the result
  RefPtr<Promise> promise = Promise::Create(mMLS->GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Use the static method or instance to send the IPC message
  mMLS->mTransactionChild
      ->SendRequestGroupRemove(mGroupId, mClientId, remClientIdentifier)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [promise, self = RefPtr<MLSGroupView>(this)](
              Maybe<mozilla::security::mls::GkMlsCommitOutput>&& commitOutput) {
            // Check if the value is Nothing
            if (commitOutput.isNothing()) {
              promise->MaybeReject(NS_ERROR_FAILURE);
              return;
            }

            // Get the context from the GlobalObject
            AutoJSAPI jsapi;
            if (NS_WARN_IF(!jsapi.Init(self->mMLS->GetParentObject()))) {
              promise->MaybeReject(NS_ERROR_FAILURE);
              return;
            }
            JSContext* cx = jsapi.cx();

            // Construct the Uint8Array objects
            ErrorResult error;
            JS::Rooted<JSObject*> jsGroupId(
                cx, Uint8Array::Create(cx, self->mGroupId, error));
            error.WouldReportJSException();
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }
            JS::Rooted<JSObject*> jsClientId(
                cx, Uint8Array::Create(cx, commitOutput->identity, error));
            error.WouldReportJSException();
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }
            JS::Rooted<JSObject*> jsCommit(
                cx, Uint8Array::Create(cx, commitOutput->commit, error));
            error.WouldReportJSException();
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }
            JS::Rooted<JSObject*> jsWelcome(
                cx, Uint8Array::Create(cx, commitOutput->welcome, error));
            error.WouldReportJSException();
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }
            JS::Rooted<JSObject*> jsGroupInfo(
                cx, Uint8Array::Create(cx, commitOutput->group_info, error));
            error.WouldReportJSException();
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }
            JS::Rooted<JSObject*> jsRatchetTree(
                cx, Uint8Array::Create(cx, commitOutput->ratchet_tree, error));
            error.WouldReportJSException();
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }

            // Construct MLSCommitOutput with the parsed data
            RootedDictionary<MLSCommitOutput> rvalue(cx);
            rvalue.mType = MLSObjectType::Commit_output;
            rvalue.mGroupId.Init(jsGroupId);
            rvalue.mCommit.Init(jsCommit);
            if (!commitOutput->welcome.IsEmpty()) {
              rvalue.mWelcome.Construct();
              rvalue.mWelcome.Value().Init(jsWelcome);
            }
            if (!commitOutput->group_info.IsEmpty()) {
              rvalue.mGroupInfo.Construct();
              rvalue.mGroupInfo.Value().Init(jsGroupInfo);
            }
            if (!commitOutput->ratchet_tree.IsEmpty()) {
              rvalue.mRatchetTree.Construct();
              rvalue.mRatchetTree.Value().Init(jsRatchetTree);
            }
            if (!commitOutput->identity.IsEmpty()) {
              rvalue.mClientId.Construct();
              rvalue.mClientId.Value().Init(jsClientId);
            }

            // Resolve the promise
            promise->MaybeResolve(rvalue);
          },
          [promise](::mozilla::ipc::ResponseRejectReason aReason) {
            promise->MaybeRejectWithUnknownError("Failed to remove from group");
          });

  return promise.forget();
}

already_AddRefed<Promise> MLSGroupView::ProposeRemove(
    const MLSBytesOrUint8Array& aJsRemClientIdentifier, ErrorResult& aRv) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug, ("MLSGroupView::ProposeRemove()"));

  // Handle the remove client identifier parameter
  nsTArray<uint8_t> remClientIdentifier = ExtractMLSBytesOrUint8Array(
      MLSObjectType::Client_identifier, aJsRemClientIdentifier, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Check if the removed client identifier is empty
  if (NS_WARN_IF(remClientIdentifier.IsEmpty())) {
    aRv.ThrowTypeError("The removed client identifier must not be empty");
    return nullptr;
  }

  // Create a new Promise object for the result
  RefPtr<Promise> promise = Promise::Create(mMLS->GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  mMLS->mTransactionChild
      ->SendRequestGroupProposeRemove(mGroupId, mClientId, remClientIdentifier)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [promise,
           self = RefPtr<MLSGroupView>(this)](Maybe<RawBytes>&& proposal) {
            // Check if the value is Nothing
            if (proposal.isNothing()) {
              promise->MaybeReject(NS_ERROR_FAILURE);
              return;
            }

            // Get the context from the GlobalObject
            AutoJSAPI jsapi;
            if (NS_WARN_IF(!jsapi.Init(self->mMLS->GetParentObject()))) {
              promise->MaybeReject(NS_ERROR_FAILURE);
              return;
            }
            JSContext* cx = jsapi.cx();

            // Construct the Uint8Array object
            ErrorResult error;
            JS::Rooted<JSObject*> content(
                cx, Uint8Array::Create(cx, proposal->data(), error));
            error.WouldReportJSException();
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }

            // Construct MLSBytes with the proposal as content
            RootedDictionary<MLSBytes> rvalue(cx);
            rvalue.mType = MLSObjectType::Proposal;
            rvalue.mContent.Init(content);

            // Resolve the promise
            promise->MaybeResolve(rvalue);
          },
          [promise](::mozilla::ipc::ResponseRejectReason aReason) {
            promise->MaybeRejectWithUnknownError(
                "Failed to propose remove from group");
          });

  return promise.forget();
}

already_AddRefed<Promise> MLSGroupView::Close(ErrorResult& aRv) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug, ("MLSGroupView::Close()"));

  // Create a new Promise object for the result
  RefPtr<Promise> promise = Promise::Create(mMLS->GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  mMLS->mTransactionChild->SendRequestGroupClose(mGroupId, mClientId)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [promise, self = RefPtr<MLSGroupView>(this)](
              Maybe<mozilla::security::mls::GkMlsCommitOutput>&& commitOutput) {
            // Check if the value is Nothing
            if (commitOutput.isNothing()) {
              promise->MaybeReject(NS_ERROR_FAILURE);
              return;
            }

            // Get the context from the GlobalObject
            AutoJSAPI jsapi;
            if (NS_WARN_IF(!jsapi.Init(self->mMLS->GetParentObject()))) {
              promise->MaybeReject(NS_ERROR_FAILURE);
              return;
            }
            JSContext* cx = jsapi.cx();

            // Construct the Uint8Array objects
            ErrorResult error;
            JS::Rooted<JSObject*> jsGroupId(
                cx, Uint8Array::Create(cx, self->mGroupId, error));
            error.WouldReportJSException();
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }
            JS::Rooted<JSObject*> jsClientId(
                cx, Uint8Array::Create(cx, commitOutput->identity, error));
            error.WouldReportJSException();
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }
            JS::Rooted<JSObject*> jsCommit(
                cx, Uint8Array::Create(cx, commitOutput->commit, error));
            error.WouldReportJSException();
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }
            JS::Rooted<JSObject*> jsWelcome(
                cx, Uint8Array::Create(cx, commitOutput->welcome, error));
            error.WouldReportJSException();
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }
            JS::Rooted<JSObject*> jsGroupInfo(
                cx, Uint8Array::Create(cx, commitOutput->group_info, error));
            error.WouldReportJSException();
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }
            JS::Rooted<JSObject*> jsRatchetTree(
                cx, Uint8Array::Create(cx, commitOutput->ratchet_tree, error));
            error.WouldReportJSException();
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }

            // Construct MLSCommitOutput with the parsed data
            RootedDictionary<MLSCommitOutput> rvalue(cx);
            rvalue.mType = MLSObjectType::Commit_output;
            rvalue.mGroupId.Init(jsGroupId);
            rvalue.mCommit.Init(jsCommit);
            if (!commitOutput->welcome.IsEmpty()) {
              rvalue.mWelcome.Construct();
              rvalue.mWelcome.Value().Init(jsWelcome);
            }
            if (!commitOutput->group_info.IsEmpty()) {
              rvalue.mGroupInfo.Construct();
              rvalue.mGroupInfo.Value().Init(jsGroupInfo);
            }
            if (!commitOutput->ratchet_tree.IsEmpty()) {
              rvalue.mRatchetTree.Construct();
              rvalue.mRatchetTree.Value().Init(jsRatchetTree);
            }
            if (!commitOutput->identity.IsEmpty()) {
              rvalue.mClientId.Construct();
              rvalue.mClientId.Value().Init(jsClientId);
            }

            // Resolve the promise
            promise->MaybeResolve(rvalue);
          },
          [promise](::mozilla::ipc::ResponseRejectReason aReason) {
            promise->MaybeRejectWithUnknownError("Failed to close group");
          });

  return promise.forget();
}

already_AddRefed<Promise> MLSGroupView::Details(ErrorResult& aRv) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug, ("MLSGroupView::Details()"));

  // Create a new Promise object for the result
  RefPtr<Promise> promise = Promise::Create(mMLS->GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  mMLS->mTransactionChild->SendRequestGroupDetails(mGroupId, mClientId)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [promise, self = RefPtr<MLSGroupView>(this)](
              Maybe<GkGroupMembers>&& groupMembers) {
            // Check if the value is Nothing
            if (groupMembers.isNothing()) {
              promise->MaybeReject(NS_ERROR_FAILURE);
              return;
            }

            // Get the context from the GlobalObject
            AutoJSAPI jsapi;
            if (NS_WARN_IF(!jsapi.Init(self->mMLS->GetParentObject()))) {
              promise->MaybeReject(NS_ERROR_FAILURE);
              return;
            }
            JSContext* cx = jsapi.cx();

            // Construct the Uint8Array objects
            ErrorResult error;
            JS::Rooted<JSObject*> jsGroupId(
                cx, Uint8Array::Create(cx, groupMembers->group_id, error));
            error.WouldReportJSException();
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }
            JS::Rooted<JSObject*> jsGroupEpoch(
                cx, Uint8Array::Create(cx, groupMembers->group_epoch, error));
            error.WouldReportJSException();
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }

            // Construct MLSGroupDetails
            RootedDictionary<MLSGroupDetails> rvalue(cx);
            rvalue.mType = MLSObjectType::Group_info;
            rvalue.mGroupId.Init(jsGroupId);
            rvalue.mGroupEpoch.Init(jsGroupEpoch);
            rvalue.mMembers.Clear();

            Sequence<MLSGroupMember> membersSequence;
            for (size_t i = 0; i < groupMembers->group_members.Length(); ++i) {
              const GkClientIdentifiers& member =
                  groupMembers->group_members[i];
              JS::Rooted<JSObject*> jsClientId(
                  cx, Uint8Array::Create(cx, member.identity, error));
              error.WouldReportJSException();
              if (error.Failed()) {
                promise->MaybeReject(std::move(error));
                return;
              }
              JS::Rooted<JSObject*> jsCredential(
                  cx, Uint8Array::Create(cx, member.credential, error));
              error.WouldReportJSException();
              if (error.Failed()) {
                promise->MaybeReject(std::move(error));
                return;
              }

              MLSGroupMember jsMember;
              jsMember.mClientId.Init(jsClientId);
              jsMember.mCredential.Init(jsCredential);

              if (!membersSequence.AppendElement(std::move(jsMember),
                                                 fallible)) {
                promise->MaybeReject(NS_ERROR_OUT_OF_MEMORY);
                return;
              }
            }
            rvalue.mMembers = std::move(membersSequence);

            // Resolve the promise
            promise->MaybeResolve(rvalue);
          },
          [promise](::mozilla::ipc::ResponseRejectReason aReason) {
            promise->MaybeRejectWithUnknownError("Failed to get group details");
          });

  return promise.forget();
}

already_AddRefed<Promise> MLSGroupView::Send(
    const MLSBytesOrUint8ArrayOrUTF8String& aJsMessage, ErrorResult& aRv) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug, ("MLSGroupView::Send()"));

  // Handle the message parameter
  nsTArray<uint8_t> message = ExtractMLSBytesOrUint8ArrayOrUTF8String(
      MLSObjectType::Application_message_plaintext, aJsMessage, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Create a new Promise object for the result
  RefPtr<Promise> promise = Promise::Create(mMLS->GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  mMLS->mTransactionChild->SendRequestSend(mGroupId, mClientId, message)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [promise,
           self = RefPtr<MLSGroupView>(this)](Maybe<RawBytes>&& result) {
            // Check if the value is Nothing
            if (result.isNothing()) {
              promise->MaybeReject(NS_ERROR_FAILURE);
              return;
            }

            // Get the context from the GlobalObject
            AutoJSAPI jsapi;
            if (NS_WARN_IF(!jsapi.Init(self->mMLS->GetParentObject()))) {
              promise->MaybeReject(NS_ERROR_FAILURE);
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

            // Construct MLSBytes with the group identifier as content
            RootedDictionary<MLSBytes> rvalue(cx);
            rvalue.mType = MLSObjectType::Application_message_ciphertext;
            rvalue.mContent.Init(content);

            promise->MaybeResolve(rvalue);
          },
          [promise](::mozilla::ipc::ResponseRejectReason aReason) {
            promise->MaybeRejectWithUnknownError("Failed to send message");
          });

  return promise.forget();
}

already_AddRefed<Promise> MLSGroupView::Receive(
    const MLSBytesOrUint8Array& aJsMessage, ErrorResult& aRv) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug, ("MLSGroupView::Receive()"));

  // Handle the message parameter
  nsTArray<uint8_t> message =
      ExtractMLSBytesOrUint8ArrayWithUnknownType(aJsMessage, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Check if the message is empty
  if (NS_WARN_IF(message.IsEmpty())) {
    aRv.ThrowTypeError("The receivedmessage must not be empty");
    return nullptr;
  }

  // Create a new Promise object for the result
  RefPtr<Promise> promise = Promise::Create(mMLS->GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Receive the message
  mMLS->mTransactionChild->SendRequestReceive(mClientId, message)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [promise, self = RefPtr<MLSGroupView>(this)](GkReceived&& received) {
            // Check if the Maybe contains a value
            if (received.tag == GkReceived::Tag::None) {
              promise->MaybeReject(NS_ERROR_FAILURE);
              return;
            }

            // Get the context from the GlobalObject
            AutoJSAPI jsapi;
            if (NS_WARN_IF(!jsapi.Init(self->mMLS->GetParentObject()))) {
              promise->MaybeReject(NS_ERROR_FAILURE);
              return;
            }
            JSContext* cx = jsapi.cx();

            // Construct the Uint8Array objects based on the tag
            ErrorResult error;
            JS::Rooted<JSObject*> jsGroupId(
                cx, Uint8Array::Create(cx, self->mGroupId, error));
            error.WouldReportJSException();
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }

            // Initialize the Received dictionary
            RootedDictionary<MLSReceived> rvalue(cx);
            rvalue.mGroupId.Init(jsGroupId);

            // Populate the Received object based on the tag
            switch (received.tag) {
              case GkReceived::Tag::GroupIdEpoch: {
                MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
                        ("Processing GroupIdEpoch"));

                JS::Rooted<JSObject*> jsGroupEpoch(
                    cx, Uint8Array::Create(
                            cx, received.group_id_epoch._0.group_epoch, error));
                error.WouldReportJSException();
                if (error.Failed()) {
                  promise->MaybeReject(std::move(error));
                  return;
                }

                // Populate the Received object
                rvalue.mType = MLSObjectType::Commit_processed;
                rvalue.mGroupEpoch.Construct();
                rvalue.mGroupEpoch.Value().Init(jsGroupEpoch);
                break;
              }
              case GkReceived::Tag::ApplicationMessage: {
                MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
                        ("Processing ApplicationMessage"));

                JS::Rooted<JSObject*> jsApplicationMessage(
                    cx, Uint8Array::Create(cx, received.application_message._0,
                                           error));
                error.WouldReportJSException();
                if (error.Failed()) {
                  promise->MaybeReject(std::move(error));
                  return;
                }

                // Populate the Received object
                rvalue.mType = MLSObjectType::Application_message_plaintext;
                rvalue.mContent.Construct();
                rvalue.mContent.Value().Init(jsApplicationMessage);

                break;
              }
              case GkReceived::Tag::CommitOutput: {
                MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
                        ("Processing CommitOutput"));

                JS::Rooted<JSObject*> jsClientId(
                    cx, Uint8Array::Create(
                            cx, received.commit_output._0.identity, error));
                error.WouldReportJSException();
                if (error.Failed()) {
                  promise->MaybeReject(std::move(error));
                  return;
                }
                JS::Rooted<JSObject*> jsCommit(
                    cx, Uint8Array::Create(cx, received.commit_output._0.commit,
                                           error));
                error.WouldReportJSException();
                if (error.Failed()) {
                  promise->MaybeReject(std::move(error));
                  return;
                }
                JS::Rooted<JSObject*> jsWelcome(
                    cx, Uint8Array::Create(
                            cx, received.commit_output._0.welcome, error));
                error.WouldReportJSException();
                if (error.Failed()) {
                  promise->MaybeReject(std::move(error));
                  return;
                }
                JS::Rooted<JSObject*> jsGroupInfo(
                    cx, Uint8Array::Create(
                            cx, received.commit_output._0.group_info, error));
                error.WouldReportJSException();
                if (error.Failed()) {
                  promise->MaybeReject(std::move(error));
                  return;
                }
                JS::Rooted<JSObject*> jsRatchetTree(
                    cx, Uint8Array::Create(
                            cx, received.commit_output._0.ratchet_tree, error));
                error.WouldReportJSException();
                if (error.Failed()) {
                  promise->MaybeReject(std::move(error));
                  return;
                }

                // Construct MLSCommitOutput with the parsed data
                rvalue.mType = MLSObjectType::Commit_output;
                rvalue.mGroupId.Init(jsGroupId);
                rvalue.mCommitOutput.Construct();
                rvalue.mCommitOutput.Value().mType =
                    MLSObjectType::Commit_output;
                rvalue.mCommitOutput.Value().mCommit.Init(jsCommit);
                rvalue.mCommitOutput.Value().mGroupId.Init(jsGroupId);
                if (!received.commit_output._0.welcome.IsEmpty()) {
                  rvalue.mCommitOutput.Value().mWelcome.Construct();
                  rvalue.mCommitOutput.Value().mWelcome.Value().Init(jsWelcome);
                }
                if (!received.commit_output._0.group_info.IsEmpty()) {
                  rvalue.mCommitOutput.Value().mGroupInfo.Construct();
                  rvalue.mCommitOutput.Value().mGroupInfo.Value().Init(
                      jsGroupInfo);
                }
                if (!received.commit_output._0.ratchet_tree.IsEmpty()) {
                  rvalue.mCommitOutput.Value().mRatchetTree.Construct();
                  rvalue.mCommitOutput.Value().mRatchetTree.Value().Init(
                      jsRatchetTree);
                }
                if (!received.commit_output._0.identity.IsEmpty()) {
                  rvalue.mCommitOutput.Value().mClientId.Construct();
                  rvalue.mCommitOutput.Value().mClientId.Value().Init(
                      jsClientId);
                }

                MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
                        ("Finished processing CommitOutput"));
                break;
              }
              default:
                MOZ_LOG(gMlsLog, mozilla::LogLevel::Error,
                        ("Unhandled tag in received data"));
                promise->MaybeRejectWithUnknownError(
                    "Unhandled tag in received data");
                return;
            }

            MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
                    ("Successfully constructed MLSReceived"));

            // Resolve the promise
            promise->MaybeResolve(rvalue);
          },
          [promise](::mozilla::ipc::ResponseRejectReason aReason) {
            MOZ_LOG(gMlsLog, mozilla::LogLevel::Error,
                    ("IPC call rejected with reason: %d",
                     static_cast<int>(aReason)));
            promise->MaybeRejectWithUnknownError("Failed to receive message");
          });

  return promise.forget();
}

already_AddRefed<Promise> MLSGroupView::ApplyPendingCommit(ErrorResult& aRv) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
          ("MLSGroupView::ApplyPendingCommit()"));

  // Create a new Promise object for the result
  RefPtr<Promise> promise = Promise::Create(mMLS->GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Receive the message
  mMLS->mTransactionChild->SendRequestApplyPendingCommit(mGroupId, mClientId)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [promise, self = RefPtr<MLSGroupView>(this)](GkReceived&& received) {
            // Check if the Maybe contains a value
            if (received.tag == GkReceived::Tag::None) {
              promise->MaybeRejectWithUnknownError(
                  "Failed to apply pending commit");
              return;
            }

            // Get the context from the GlobalObject
            AutoJSAPI jsapi;
            if (NS_WARN_IF(!jsapi.Init(self->mMLS->GetParentObject()))) {
              MOZ_LOG(gMlsLog, mozilla::LogLevel::Error,
                      ("Failed to initialize JSAPI"));
              promise->MaybeRejectWithUnknownError(
                  "Failed to initialize JSAPI");
              return;
            }
            JSContext* cx = jsapi.cx();

            // Construct the Uint8Array objects based on the tag
            ErrorResult error;
            JS::Rooted<JSObject*> jsGroupId(
                cx, Uint8Array::Create(cx, self->mGroupId, error));
            error.WouldReportJSException();
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }

            // Initialize the Received dictionary
            RootedDictionary<MLSReceived> rvalue(cx);
            rvalue.mGroupId.Init(jsGroupId);

            // Populate the Received object based on the tag
            switch (received.tag) {
              case GkReceived::Tag::GroupIdEpoch: {
                JS::Rooted<JSObject*> jsGroupEpoch(
                    cx, Uint8Array::Create(
                            cx, received.group_id_epoch._0.group_epoch, error));
                error.WouldReportJSException();
                if (error.Failed()) {
                  promise->MaybeReject(std::move(error));
                  return;
                }

                // Populate the Received object
                rvalue.mType = MLSObjectType::Commit_processed;
                rvalue.mGroupEpoch.Construct();
                rvalue.mGroupEpoch.Value().Init(jsGroupEpoch);
                break;
              }
              default:
                MOZ_LOG(gMlsLog, mozilla::LogLevel::Error,
                        ("Unhandled tag in received data"));
                promise->MaybeRejectWithUnknownError(
                    "Unhandled tag in received data");
                return;
            }

            MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
                    ("Successfully constructed MLSReceived"));

            // Resolve the promise
            promise->MaybeResolve(rvalue);
          },
          [promise](::mozilla::ipc::ResponseRejectReason aReason) {
            MOZ_LOG(gMlsLog, mozilla::LogLevel::Error,
                    ("IPC call rejected with reason: %d",
                     static_cast<int>(aReason)));
            promise->MaybeRejectWithUnknownError(
                "Failed to apply pending commit");
          });

  return promise.forget();
}

already_AddRefed<Promise> MLSGroupView::ExportSecret(
    const MLSBytesOrUint8ArrayOrUTF8String& aJsLabel,
    const MLSBytesOrUint8Array& aJsContext, const uint64_t aLen,
    ErrorResult& aRv) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug, ("MLSGroupView::ExportSecret()"));

  // Handle the label parameter
  nsTArray<uint8_t> label = ExtractMLSBytesOrUint8ArrayOrUTF8String(
      MLSObjectType::Exporter_label, aJsLabel, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // We allow the context to be empty
  if (NS_WARN_IF(label.IsEmpty())) {
    aRv.ThrowTypeError("The label must not be empty");
    return nullptr;
  }

  // Handle the context parameter
  // Note: we allow the context to be empty
  nsTArray<uint8_t> context = ExtractMLSBytesOrUint8Array(
      MLSObjectType::Exporter_context, aJsContext, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Create a new Promise object for the result
  RefPtr<Promise> promise = Promise::Create(mMLS->GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  mMLS->mTransactionChild
      ->SendRequestExportSecret(mGroupId, mClientId, label, context, aLen)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [promise, self = RefPtr<MLSGroupView>(this)](
              Maybe<mozilla::security::mls::GkExporterOutput>&&
                  exporterOutput) {
            // Check if the Maybe contains a value
            if (exporterOutput.isNothing()) {
              promise->MaybeReject(NS_ERROR_FAILURE);
              return;
            }

            // Get the context from the GlobalObject
            AutoJSAPI jsapi;
            if (NS_WARN_IF(!jsapi.Init(self->mMLS->GetParentObject()))) {
              promise->MaybeRejectWithUnknownError(
                  "Failed to initialize JSAPI");
              return;
            }
            JSContext* cx = jsapi.cx();

            // Construct the Uint8Array objects
            ErrorResult error;
            JS::Rooted<JSObject*> jsGroupId(
                cx, Uint8Array::Create(cx, exporterOutput->group_id, error));
            error.WouldReportJSException();
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }
            JS::Rooted<JSObject*> jsGroupEpoch(
                cx, Uint8Array::Create(cx, exporterOutput->group_epoch, error));
            error.WouldReportJSException();
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }
            JS::Rooted<JSObject*> jsLabel(
                cx, Uint8Array::Create(cx, exporterOutput->label, error));
            error.WouldReportJSException();
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }
            JS::Rooted<JSObject*> jsContext(
                cx, Uint8Array::Create(cx, exporterOutput->context, error));
            error.WouldReportJSException();
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }
            JS::Rooted<JSObject*> jsExporter(
                cx, Uint8Array::Create(cx, exporterOutput->exporter, error));
            error.WouldReportJSException();
            if (error.Failed()) {
              promise->MaybeReject(std::move(error));
              return;
            }

            // Construct MLSBytes with the group identifier as content
            RootedDictionary<MLSExporterOutput> rvalue(cx);
            rvalue.mType = MLSObjectType::Exporter_output;
            rvalue.mGroupId.Init(jsGroupId);
            rvalue.mGroupEpoch.Init(jsGroupEpoch);
            rvalue.mLabel.Init(jsLabel);
            rvalue.mContext.Init(jsContext);
            rvalue.mSecret.Init(jsExporter);

            // Resolve the promise
            promise->MaybeResolve(rvalue);
          },
          [promise](::mozilla::ipc::ResponseRejectReason aReason) {
            promise->MaybeRejectWithUnknownError("Failed to export secret");
          });

  return promise.forget();
}

};  // namespace mozilla::dom
