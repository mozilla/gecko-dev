/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Permissions_h_
#define mozilla_dom_Permissions_h_

#include "nsISupports.h"
#include "nsWrapperCache.h"
#include "mozilla/GlobalTeardownObserver.h"

class nsIGlobalObject;

namespace mozilla {

class ErrorResult;

namespace dom {

class Promise;
class PermissionStatus;
struct PermissionSetParameters;

class Permissions final : public GlobalTeardownObserver, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(Permissions)

  explicit Permissions(nsIGlobalObject* aGlobal);

  nsIGlobalObject* GetParentObject() const { return GetOwnerGlobal(); }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<Promise> Query(JSContext* aCx,
                                  JS::Handle<JSObject*> aPermission,
                                  ErrorResult& aRv);

  // The IDL conversion steps of
  // https://w3c.github.io/permissions/#webdriver-command-set-permission
  already_AddRefed<PermissionStatus> ParseSetParameters(
      JSContext* aCx, const PermissionSetParameters& aParameters,
      ErrorResult& aRv);

 private:
  ~Permissions();
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_permissions_h_
