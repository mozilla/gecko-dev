/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CloudStorageRequestChild.h"
#include "CloudStorageLog.h"

namespace mozilla {
namespace dom {
namespace cloudstorage {

CloudStorageRequestChild::CloudStorageRequestChild()
{
}

CloudStorageRequestChild::~CloudStorageRequestChild()
{
}

void
CloudStorageRequestChild::ActorDestroy(ActorDestroyReason aWhy)
{
}

bool
CloudStorageRequestChild::Recv__delete__()
{
  LOG("CloudStorageRequestChild::Recv__delete__()");
  /*
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mReplyRunnable);

  nsRefPtr<BluetoothReplyRunnable> replyRunnable;
  mReplyRunnable.swap(replyRunnable);

  if (replyRunnable) {
    // XXXbent Need to fix this, it copies unnecessarily.
    replyRunnable->SetReply(new BluetoothReply(aReply));
    return NS_SUCCEEDED(NS_DispatchToCurrentThread(replyRunnable));
  }
  */
  return true;
}
} // end cloudstorage
} // end dom
} // end mozilla
