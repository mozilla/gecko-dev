/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteWorkerServiceParent.h"
#include "RemoteWorkerManager.h"
#include "RemoteWorkerParent.h"
#include "mozilla/dom/RemoteWorkerTypes.h"
#include "mozilla/ipc/BackgroundParent.h"

namespace mozilla {

using namespace ipc;

namespace dom {

RemoteWorkerServiceParent::RemoteWorkerServiceParent(
    ThreadsafeContentParentHandle* aProcess)
    : mProcess(aProcess) {}

RemoteWorkerServiceParent::~RemoteWorkerServiceParent() {
  MOZ_ASSERT(!mManager,
             "ActorDestroy not called before ~RemoteWorkerServiceParent");
}

RefPtr<RemoteWorkerServiceParent> RemoteWorkerServiceParent::CreateForProcess(
    ContentParent* aProcess, Endpoint<PRemoteWorkerServiceChild>* aChildEp) {
  AssertIsOnMainThread();

  nsCOMPtr<nsISerialEventTarget> backgroundThread =
      BackgroundParent::GetBackgroundThread();
  NS_ENSURE_TRUE(backgroundThread, nullptr);

  Endpoint<PRemoteWorkerServiceParent> parentEp;
  nsresult rv = PRemoteWorkerService::CreateEndpoints(
      EndpointProcInfo::Current(),
      aProcess ? aProcess->OtherEndpointProcInfo()
               : EndpointProcInfo::Current(),
      &parentEp, aChildEp);
  NS_ENSURE_SUCCESS(rv, nullptr);

  RefPtr<RemoteWorkerServiceParent> actor = new RemoteWorkerServiceParent(
      aProcess ? aProcess->ThreadsafeHandle() : nullptr);
  rv = backgroundThread->Dispatch(
      NS_NewRunnableFunction("RemoteWorkerServiceParent::CreateForProcess",
                             [actor, parentEp = std::move(parentEp)]() mutable {
                               actor->InitializeOnThread(std::move(parentEp));
                             }));
  NS_ENSURE_SUCCESS(rv, nullptr);

  return actor;
}

void RemoteWorkerServiceParent::InitializeOnThread(
    Endpoint<PRemoteWorkerServiceParent> aEndpoint) {
  AssertIsOnBackgroundThread();
  if (NS_WARN_IF(!aEndpoint.Bind(this))) {
    return;
  }
  mManager = RemoteWorkerManager::GetOrCreate();
  mManager->RegisterActor(this);
}

void RemoteWorkerServiceParent::ActorDestroy(IProtocol::ActorDestroyReason) {
  AssertIsOnBackgroundThread();
  if (mManager) {
    mManager->UnregisterActor(this);
    mManager = nullptr;
  }
}

nsCString RemoteWorkerServiceParent::GetRemoteType() const {
  if (mProcess) {
    return mProcess->GetRemoteType();
  }
  return NOT_REMOTE_TYPE;
}

}  // namespace dom
}  // namespace mozilla
