/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_IDENTITYCREDENTIALREQUESTMANAGER_H_
#define MOZILLA_IDENTITYCREDENTIALREQUESTMANAGER_H_

#include "nsISupports.h"

namespace mozilla {

class IdentityCredentialRequestManager final : nsISupports {
 public:
  NS_DECL_ISUPPORTS

  static IdentityCredentialRequestManager* GetInstance();

  IdentityCredentialRequestManager(IdentityCredentialRequestManager& other) =
      delete;
  void operator=(const IdentityCredentialRequestManager&) = delete;

 private:
  static StaticRefPtr<IdentityCredentialRequestManager> sSingleton;
  IdentityCredentialRequestManager() {};
  ~IdentityCredentialRequestManager() = default;

};

}  // namespace mozilla

#endif /* MOZILLA_IDENTITYCREDENTIALSTORAGESERVICE_H_ */
