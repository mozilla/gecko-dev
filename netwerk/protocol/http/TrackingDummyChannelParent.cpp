/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TrackingDummyChannelParent.h"
#include "mozilla/Unused.h"
#include "nsChannelClassifier.h"
#include "nsIChannel.h"
#include "nsIPrincipal.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace net {

TrackingDummyChannelParent::TrackingDummyChannelParent() : mIPCActive(true) {}

TrackingDummyChannelParent::~TrackingDummyChannelParent() = default;

void TrackingDummyChannelParent::Init(nsIURI* aURI, nsIURI* aTopWindowURI,
                                      nsresult aTopWindowURIResult,
                                      nsILoadInfo* aLoadInfo) {
  MOZ_ASSERT(mIPCActive);

  RefPtr<TrackingDummyChannelParent> self = this;
  auto onExit =
      MakeScopeExit([self] { Unused << Send__delete__(self, false); });

  if (!aURI) {
    return;
  }

  RefPtr<TrackingDummyChannel> channel = new TrackingDummyChannel(
      aURI, aTopWindowURI, aTopWindowURIResult, aLoadInfo);

  RefPtr<nsChannelClassifier> channelClassifier =
      new nsChannelClassifier(channel);

  bool willCallback =
      NS_SUCCEEDED(channelClassifier->CheckIsTrackerWithLocalTable(
          [self = std::move(self), channel]() {
            if (self->mIPCActive) {
              Unused << Send__delete__(self, channel->IsTrackingResource());
            }
          }));

  if (willCallback) {
    onExit.release();
  }
}

void TrackingDummyChannelParent::ActorDestroy(ActorDestroyReason aWhy) {
  mIPCActive = false;
}

}  // namespace net
}  // namespace mozilla
