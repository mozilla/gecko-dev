/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RemoteWorkerServiceChild_h
#define mozilla_dom_RemoteWorkerServiceChild_h

#include "mozilla/dom/PRemoteWorkerServiceChild.h"
#include "mozilla/dom/PRemoteWorkerNonLifeCycleOpControllerChild.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom {

class RemoteWorkerController;
class RemoteWorkerData;

/**
 * "Worker Launcher"-thread child actor created by the RemoteWorkerService to
 * receive messages from the PBackground RemoteWorkerManager in the parent.
 */
class RemoteWorkerServiceChild final : public PRemoteWorkerServiceChild {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RemoteWorkerServiceChild, final)

  RemoteWorkerServiceChild();

  already_AddRefed<PRemoteWorkerChild> AllocPRemoteWorkerChild(
      const RemoteWorkerData& aData,
      mozilla::ipc::Endpoint<PRemoteWorkerNonLifeCycleOpControllerChild>&
          aChildEp);

  mozilla::ipc::IPCResult RecvPRemoteWorkerConstructor(
      PRemoteWorkerChild* aActor, const RemoteWorkerData& aData,
      mozilla::ipc::Endpoint<PRemoteWorkerNonLifeCycleOpControllerChild>&&
          aChildEp);

 private:
  ~RemoteWorkerServiceChild();
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_RemoteWorkerServiceChild_h
