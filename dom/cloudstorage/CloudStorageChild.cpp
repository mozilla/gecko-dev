/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CloudStorageChild.h"
#include "CloudStorageRequestChild.h"
#include "mozilla/DebugOnly.h"
#include "CloudStorageLog.h"

namespace mozilla {
namespace dom {
namespace cloudstorage {

CloudStorageChild::CloudStorageChild()
{
}

CloudStorageChild::~CloudStorageChild()
{
}

PCloudStorageRequestChild*
CloudStorageChild::AllocPCloudStorageRequestChild(const CloudStorageRequest& request)
{
  LOG("CloudStorageChild::AllocPCloudStorageRequestChild()");
  MOZ_CRASH("should not be here");
}

bool
CloudStorageChild::DeallocPCloudStorageRequestChild(PCloudStorageRequestChild* aActor)
{
  LOG("CloudStorageChild::DeallocPCloudStorageRequestChild()");
  delete aActor;
  return true;
}

void
CloudStorageChild::ActorDestroy(ActorDestroyReason aWhy)
{
}

} // end cloudstorage
} // end dom
} // end mozilla
