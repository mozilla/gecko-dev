/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCloudStorageService.h"
#include "CloudStorageService.h"
#include "mozilla/ErrorResult.h"
#include "nsString.h"
#include "CloudStorageLog.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace mozilla {
namespace dom {
namespace cloudstorage {

//NS_IMPL_CYCLE_COLLECTION_INHERITED(nsCloudStorageService, DOMEventTargetHelper, __VA_ARGS__)
NS_IMPL_CYCLE_COLLECTION_CLASS(nsCloudStorageService)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsCloudStorageService, DOMEventTargetHelper)
//NS_IMPL_CYCLE_COLLECTION_UNLINK(__VA_ARGS__)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsCloudStorageService, DOMEventTargetHelper)
//NS_IMPL_CYCLE_COLLECTION_TRAVERSE(__VA_ARGS__)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsCloudStorageService)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)
NS_IMPL_ADDREF_INHERITED(nsCloudStorageService, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(nsCloudStorageService, DOMEventTargetHelper)

nsCloudStorageService::nsCloudStorageService(nsPIDOMWindow* aWindow)
  : DOMEventTargetHelper(aWindow)
{
  LOG("nsCloudStorageService constructor");
}

nsCloudStorageService::~nsCloudStorageService()
{
}

already_AddRefed<nsCloudStorageService >
nsCloudStorageService::Create(nsPIDOMWindow* aWindow)
{
  LOG("nsCloudStorageService::Create");
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);
  nsRefPtr<nsCloudStorageService> service = new nsCloudStorageService(aWindow);
  return service.forget();
}

JSObject* 
nsCloudStorageService::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return CloudStorageServiceBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<Promise >
nsCloudStorageService::Enable(const nsAString& aCloudName, CloudStorageType& aType, const nsAString& aAccessToken, ErrorResult& aRv)
{
  LOG("nsCloudStorageService::Enable()");
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    return nullptr;
  }
  ErrorResult errorResult;
  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  CloudStorageService* css = CloudStorageService::GetSingleton();
  css->Enable(nsString(aCloudName), (uint16_t) aType, nsString(aAccessToken), promise);
  return promise.forget();
}

already_AddRefed<Promise >
nsCloudStorageService::Disable(const nsAString& aCloudName, ErrorResult& aRv)
{
  LOG("nsCloudStorageService::Disable()");
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    return nullptr;
  }
  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  CloudStorageService* css = CloudStorageService::GetSingleton();
  css->Disable(nsString(aCloudName), promise);
  return promise.forget();
}

} // end cloudstorage
} // end dom
} // end mozilla
