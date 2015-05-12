/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cloudstorage_CloudStorageChild_h
#define mozilla_dom_cloudstorage_CloudStorageChild_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/cloudstorage/PCloudStorageChild.h"
#include "mozilla/dom/cloudstorage/PCloudStorageRequestChild.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"

namespace mozilla {
namespace dom {
namespace cloudstorage {

class CloudStorageChild : public PCloudStorageChild
{
  friend class mozilla::dom::ContentChild;
public:
  CloudStorageChild();

protected:
  virtual ~CloudStorageChild();

  virtual PCloudStorageRequestChild*
  AllocPCloudStorageRequestChild(const CloudStorageRequest& request) override;

  virtual bool
  DeallocPCloudStorageRequestChild(PCloudStorageRequestChild* aActor) override;

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) override;

};

} // end cloudstorage
} // end dom
} // end mozilla

#endif
