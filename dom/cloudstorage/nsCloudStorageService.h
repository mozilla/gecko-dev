/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cloudstorage_nsCloudStorageService_h
#define mozilla_dom_cloudstorage_nsCloudStorageService_h

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "nsIObserver.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/CloudStorageServiceBinding.h"
#include "mozilla/ErrorResult.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace dom {
namespace cloudstorage {

class nsCloudStorageService final : public DOMEventTargetHelper
{
public:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsCloudStorageService, DOMEventTargetHelper)

  //IMPL_EVENT_HANDLER(enable);
  //IMPL_EVENT_HANDLER(disable);

  static already_AddRefed<nsCloudStorageService > Create(nsPIDOMWindow* aWindow);
  
  nsPIDOMWindow* GetParentObject() const
  {
    return GetOwner();
  }
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL implementation
  already_AddRefed<Promise> Enable(const nsAString& aName, CloudStorageType& aType, const nsAString& aToken, ErrorResult& aRv);
  already_AddRefed<Promise> Disable(const nsAString& aName, ErrorResult& aRv);

private:
  nsCloudStorageService(nsPIDOMWindow* aWindow);
  ~nsCloudStorageService();

};

}
}
}
#endif
