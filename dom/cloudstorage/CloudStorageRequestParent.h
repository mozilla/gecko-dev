/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cloudstorage_CloudStorageRequestParent_h
#define mozilla_dom_cloudstorage_CloudStorageRequestParent_h

#include "mozilla/dom/cloudstorage/PCloudStorageRequestParent.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/cloudstorage/CloudStorageParent.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace dom {
namespace cloudstorage {

class CloudStorageRequestParent : public PCloudStorageRequestParent
{
  friend class mozilla::dom::ContentParent;
  friend class mozilla::dom::cloudstorage::CloudStorageParent;
public:
  CloudStorageRequestParent();
protected:
  virtual ~CloudStorageRequestParent();

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) override;

  bool 
  HandleRequest(const CloudStorageRequest& aRequest);
};

} // end cloudstorage
} // end dom
} // end mozilla

#endif
