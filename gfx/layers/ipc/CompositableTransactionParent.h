/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_COMPOSITABLETRANSACTIONPARENT_H
#define MOZILLA_LAYERS_COMPOSITABLETRANSACTIONPARENT_H

#include <vector>                       // for vector
#include "mozilla/Attributes.h"         // for override
#include "mozilla/layers/AsyncTransactionTracker.h" // for AsyncTransactionTracker
#include "mozilla/layers/ISurfaceAllocator.h"  // for ISurfaceAllocator
#include "mozilla/layers/LayersMessages.h"  // for EditReply, etc

namespace mozilla {
namespace layers {

class CompositableHost;

typedef std::vector<mozilla::layers::EditReply> EditReplyVector;

// Since PCompositble has two potential manager protocols, we can't just call
// the Manager() method usually generated when there's one manager protocol,
// so both manager protocols implement this and we keep a reference to them
// through this interface.
class CompositableParentManager : public ISurfaceAllocator
{
public:
  virtual void SendFenceHandleIfPresent(PTextureParent* aTexture,
                                        CompositableHost* aCompositableHost) = 0;

  virtual void SendAsyncMessage(const InfallibleTArray<AsyncParentMessageData>& aMessage) = 0;

  void SendPendingAsyncMessages();

  /**
   * Get child side's process Id.
   */
  virtual base::ProcessId GetChildProcessId() = 0;

protected:
  /**
   * Handle the IPDL messages that affect PCompositable actors.
   */
  bool ReceiveCompositableUpdate(const CompositableOperation& aEdit,
                                 EditReplyVector& replyv);
  bool IsOnCompositorSide() const override { return true; }

  /**
   * Return true if this protocol is asynchronous with respect to the content
   * thread (ImageBridge for instance).
   */
  virtual bool IsAsync() const { return false; }

  virtual void ReplyRemoveTexture(const OpReplyRemoveTexture& aReply) {}

  std::vector<AsyncParentMessageData> mPendingAsyncMessage;
};

} // namespace
} // namespace

#endif
