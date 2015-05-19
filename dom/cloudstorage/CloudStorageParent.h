/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cloudstorage_CloudStorageParent_h
#define mozilla_dom_cloudstorage_CloudStorageParent_h

#include "mozilla/dom/cloudstorage/PCloudStorageParent.h"
#include "mozilla/dom/cloudstorage/PCloudStorageRequestParent.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"

#include "mozilla/Attributes.h"

namespace mozilla {
namespace dom {
namespace cloudstorage {

class CloudStorageParent : public PCloudStorageParent
{
  friend class mozilla::dom::ContentParent;
public:
  CloudStorageParent();
protected:
  virtual ~CloudStorageParent();
  
  virtual PCloudStorageRequestParent*
  AllocPCloudStorageRequestParent(const CloudStorageRequest& request) override;

  virtual bool
  DeallocPCloudStorageRequestParent(PCloudStorageRequestParent* aActor) override;

  virtual bool
  RecvPCloudStorageRequestConstructor(PCloudStorageRequestParent* aActor,
                                      const CloudStorageRequest& request) override;

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) override;

};

} // end cloudstorage
} // end dom
} // end mozilla

#endif
