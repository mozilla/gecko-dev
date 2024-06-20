/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RemoteWorkerServiceParent_h
#define mozilla_dom_RemoteWorkerServiceParent_h

#include "mozilla/dom/PRemoteWorkerServiceParent.h"
#include "mozilla/dom/RemoteType.h"

namespace mozilla::dom {

class ContentParent;
class RemoteWorkerManager;
class ThreadsafeContentParentHandle;

/**
 * PBackground-thread parent actor that registers with the PBackground
 * RemoteWorkerManager and used to relay spawn requests.
 */
class RemoteWorkerServiceParent final : public PRemoteWorkerServiceParent {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RemoteWorkerServiceParent, override);

  static RefPtr<RemoteWorkerServiceParent> CreateForProcess(
      ContentParent* aProcess, Endpoint<PRemoteWorkerServiceChild>* aChildEp);

  void ActorDestroy(mozilla::ipc::IProtocol::ActorDestroyReason) override;

  bool IsOtherProcessActor() const { return mProcess != nullptr; }

  ThreadsafeContentParentHandle* GetContentParentHandle() const {
    return mProcess;
  }

  nsCString GetRemoteType() const;

 private:
  explicit RemoteWorkerServiceParent(ThreadsafeContentParentHandle* aProcess);
  ~RemoteWorkerServiceParent();

  void InitializeOnThread(Endpoint<PRemoteWorkerServiceParent> aEndpoint);

  const RefPtr<ThreadsafeContentParentHandle> mProcess;
  RefPtr<RemoteWorkerManager> mManager;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_RemoteWorkerServiceParent_h
