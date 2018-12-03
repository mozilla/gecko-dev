/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RemoteWorkerParent_h
#define mozilla_dom_RemoteWorkerParent_h

#include "mozilla/dom/PRemoteWorkerParent.h"

namespace mozilla {
namespace dom {

class RemoteWorkerController;

class RemoteWorkerParent final : public PRemoteWorkerParent {
 public:
  NS_INLINE_DECL_REFCOUNTING(RemoteWorkerParent)

  RemoteWorkerParent();

  void Initialize();

  void SetController(RemoteWorkerController* aController);

 private:
  ~RemoteWorkerParent();

  void ActorDestroy(mozilla::ipc::IProtocol::ActorDestroyReason) override;

  mozilla::ipc::IPCResult RecvError(const ErrorValue& aValue) override;

  mozilla::ipc::IPCResult RecvClose() override;

  mozilla::ipc::IPCResult RecvCreated(const bool& aStatus) override;

  RefPtr<RemoteWorkerController> mController;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_RemoteWorkerParent_h
