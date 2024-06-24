/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteWorkerServiceChild.h"
#include "RemoteWorkerController.h"
#include "RemoteWorkerChild.h"

namespace mozilla::dom {

RemoteWorkerServiceChild::RemoteWorkerServiceChild() = default;

RemoteWorkerServiceChild::~RemoteWorkerServiceChild() = default;

already_AddRefed<PRemoteWorkerChild>
RemoteWorkerServiceChild::AllocPRemoteWorkerChild(
    const RemoteWorkerData& aData) {
  return MakeAndAddRef<RemoteWorkerChild>(aData);
}

mozilla::ipc::IPCResult RemoteWorkerServiceChild::RecvPRemoteWorkerConstructor(
    PRemoteWorkerChild* aActor, const RemoteWorkerData& aData) {
  RemoteWorkerChild* actor = static_cast<RemoteWorkerChild*>(aActor);
  actor->ExecWorker(aData);
  return IPC_OK();
}

}  // namespace mozilla::dom
