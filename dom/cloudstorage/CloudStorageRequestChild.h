/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cloudstorage_CloudStorageRequestChild_h
#define mozilla_dom_cloudstorage_CloudStorageRequestChild_h

#include "mozilla/dom/cloudstorage/PCloudStorageRequestChild.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/cloudstorage/CloudStorageChild.h"

#include "mozilla/Attributes.h"

namespace mozilla {
namespace dom {
namespace cloudstorage {

class CloudStorageRequestChild : public PCloudStorageRequestChild
{
  friend class mozilla::dom::ContentChild;
  friend class mozilla::dom::cloudstorage::CloudStorageChild;
public:
  CloudStorageRequestChild();
protected:
  virtual ~CloudStorageRequestChild();
  
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) override;

  virtual bool
  Recv__delete__() override;
};

} // end cloudstorage
} // end dom
} // end mozilla

#endif
