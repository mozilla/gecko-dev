/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cloudstorage_CloudStorageService_h
#define mozilla_dom_cloudstorage_CloudStorageService_h

#include "CloudStorageChild.h"
#include "nsString.h"
#include "nsIObserver.h"
#include "nsIThread.h"

namespace mozilla {
namespace dom {

class Promise;

namespace cloudstorage {

class CloudStorageService final
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CloudStorageService)

  static CloudStorageService* GetSingleton();

  nsresult Enable(const nsString& aCloudName, const uint16_t aCloudType, const nsString& aToken, Promise* aPromise);
  nsresult Disable(const nsString& aCloudName, Promise* aPromise);
  
private:
  CloudStorageService();
  ~CloudStorageService();

  PCloudStorageChild* GetCloudStorageChild();

  class CloudStorageRequestChildRunnable final : public nsRunnable
  {
  public:
    CloudStorageRequestChildRunnable(const CloudStorageRequest& aRequest)
      : mRequest(aRequest)
    {}

    ~CloudStorageRequestChildRunnable() {}

    nsresult Run() override;
  private:
    void FireSuccess();
  private:
    CloudStorageRequest mRequest;
  };

private:
  static CloudStorageService* sService;

  CloudStorageChild* mCloudStorageChild;
};

}
}
}
#endif
