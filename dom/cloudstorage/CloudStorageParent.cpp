/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CloudStorageParent.h"
#include "CloudStorageRequestParent.h"
#include "CloudStorageLog.h"

namespace mozilla {
namespace dom {
namespace cloudstorage {

CloudStorageParent::CloudStorageParent()
{
}

CloudStorageParent::~CloudStorageParent()
{
}

void
CloudStorageParent::ActorDestroy(ActorDestroyReason aWhy)
{
}

PCloudStorageRequestParent*
CloudStorageParent::AllocPCloudStorageRequestParent(const CloudStorageRequest& aRequest)
{
  LOG("CloudStorageParent::AllocPCloudStorageRequestParent()");
  // non-used parameter aRequest
  return new CloudStorageRequestParent();
}

bool
CloudStorageParent::DeallocPCloudStorageRequestParent(PCloudStorageRequestParent* aActor)
{
  LOG("CloudStorageParent::DeallocPCloudStorageRequestParent()");
  delete aActor;
  return true;
}

bool
CloudStorageParent::RecvPCloudStorageRequestConstructor(PCloudStorageRequestParent* aActor,
                                                        const CloudStorageRequest& aRequest)
{
  LOG("CloudStorageParent::RecvPCloudStorageRequestConstructor()");
  CloudStorageRequestParent* actor = static_cast<CloudStorageRequestParent*>(aActor);
  return actor->HandleRequest(aRequest);
}

} // end cloudstorage
} // end dom
} // end mozilla


