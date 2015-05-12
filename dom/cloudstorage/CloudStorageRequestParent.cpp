/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CloudStorageRequestParent.h"
#include "CloudStorageLog.h"

namespace mozilla {
namespace dom {
namespace cloudstorage {

CloudStorageRequestParent::CloudStorageRequestParent()
{
// might be need CloudStorageManager here
}

CloudStorageRequestParent::~CloudStorageRequestParent()
{
}

bool
CloudStorageRequestParent::HandleRequest(const CloudStorageRequest& aRequest)
{
  LOG("CloudStorageRequestParent::HandleRequest");
  switch (aRequest.type()) {
    case CloudStorageRequest::TEnableStorageRequest: {
      // handle enable request here
      LOG("Handle enable cloud storage request");
      return true;
    }
    case CloudStorageRequest::TDisableStorageRequest: {
      // handle disable request here
      LOG("Handle disable cloud storage request");
      return true;
    }
    default: MOZ_CRASH("Unknown type!"); return false;
  }
  return false;
}

void
CloudStorageRequestParent::ActorDestroy(ActorDestroyReason aWhy)
{
}

} // end cloudstorage
} // end dom
} // end mozilla
