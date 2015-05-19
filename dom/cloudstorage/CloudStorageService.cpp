/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CloudStorageService.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/Promise.h"
#include "CloudStorageLog.h"

namespace mozilla {
namespace dom {
namespace cloudstorage {

CloudStorageService* CloudStorageService::sService = NULL;

CloudStorageService::CloudStorageService()
  : mCloudStorageChild(NULL)
{
  LOG("CloudStorageService constructor");
  mozilla::dom::ContentChild* contentChild = mozilla::dom::ContentChild::GetSingleton();
  MOZ_ASSERT(contentChild);
  mCloudStorageChild = new CloudStorageChild();
  contentChild->SendPCloudStorageConstructor(mCloudStorageChild);
}

CloudStorageService::~CloudStorageService()
{
}
//static 
CloudStorageService*
CloudStorageService::GetSingleton()
{
  LOG("CloudStorageService::GetSingleton()");
  if (sService == NULL) {
    sService = new CloudStorageService();
  }
  return sService;
}

PCloudStorageChild*
CloudStorageService::GetCloudStorageChild()
{
  LOG("CloudStorageService::GetCloudStorageChild()");
  return mCloudStorageChild;
}

nsresult
CloudStorageService::Enable(const nsString& aCloudName, const uint16_t aCloudType, const nsString& aToken, Promise* aPromise)
{
  LOG("CloudStorageService::Enable()");
  EnableStorageRequest request(aCloudName, aCloudType, aToken);
  MOZ_ASSERT(NS_IsMainThread());
  CloudStorageRequestChildRunnable* enableRunnable = new CloudStorageRequestChildRunnable(request);
  nsresult ret = NS_DispatchToCurrentThread(enableRunnable);
  AutoSafeJSContext cx; 
  JS::Rooted<JS::Value> v(cx, JS::BooleanValue(bool(true)));
  aPromise->MaybeResolve(v);
  return ret;
}

nsresult
CloudStorageService::Disable(const nsString& aCloudName, Promise* aPromise)
{
  LOG("CloudStorageService::Disable()");
  DisableStorageRequest request(aCloudName);
  MOZ_ASSERT(NS_IsMainThread());
  CloudStorageRequestChildRunnable* disableRunnable = new CloudStorageRequestChildRunnable(request);
  nsresult ret = NS_DispatchToCurrentThread(disableRunnable);
  AutoSafeJSContext cx; 
  JS::Rooted<JS::Value> v(cx, JS::UndefinedValue());
  aPromise->MaybeResolve(v);
  return ret;
}

nsresult
CloudStorageService::CloudStorageRequestChildRunnable::Run()
{
  LOG("CloudStorageRequestChildRunnable::Run()");
  //if (XRE_GetProcessType() != GeckoProcessType_Default) {
  PCloudStorageRequestChild* child = new CloudStorageRequestChild();
  CloudStorageService::GetSingleton()->GetCloudStorageChild()->SendPCloudStorageRequestConstructor(child, mRequest);
  return NS_OK;
}

} // end cloudstorage
} // end dom
} // end mozilla
