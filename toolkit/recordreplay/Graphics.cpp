/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Interfaces for drawing graphics to an in process buffer when
// recording/replaying.

#include "mozilla/layers/BasicCompositor.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/LayerTransactionParent.h"
#include "mozilla/layers/LayersMessages.h"

using namespace mozilla::layers;

namespace mozilla::recordreplay {

static LayerManagerComposite* gLayerManager;
static LayerTransactionParent* gLayerTransactionParent;

static void EnsureInitialized() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (gLayerTransactionParent) {
    return;
  }

  Compositor* compositor = new BasicCompositor(nullptr, nullptr);
  gLayerManager = new LayerManagerComposite(compositor);
  gLayerTransactionParent = new LayerTransactionParent(gLayerManager, nullptr, nullptr,
                                                       LayersId(), TimeDuration());
}

void SendUpdate(const TransactionInfo& aInfo) {
  EnsureInitialized();

  PrintLog("GraphicsSendUpdate");

  ipc::IPCResult rv = gLayerTransactionParent->RecvUpdate(aInfo);
  MOZ_RELEASE_ASSERT(rv == ipc::IPCResult::Ok());

  nsCString none;
  gLayerManager->BeginTransaction(none);
  gLayerManager->EndTransaction(TimeStamp::Now());
}

void SendNewCompositable(const layers::CompositableHandle& aHandle,
                         const layers::TextureInfo& aInfo) {
  EnsureInitialized();

  ipc::IPCResult rv = gLayerTransactionParent->RecvNewCompositable(aHandle, aInfo);
  MOZ_RELEASE_ASSERT(rv == ipc::IPCResult::Ok());
}

} // namespace mozilla::recordreplay
