/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerManagerChild.h"
#include "ServiceWorkerManager.h"
#include "mozilla/Unused.h"

namespace mozilla {

using namespace ipc;

namespace dom {
namespace workers {

bool
ServiceWorkerManagerChild::RecvNotifyRegister(
                                     const ServiceWorkerRegistrationData& aData)
{
  if (mShuttingDown) {
    return true;
  }

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (swm) {
    swm->LoadRegistration(aData);
  }

  return true;
}

bool
ServiceWorkerManagerChild::RecvNotifySoftUpdate(
                                      const PrincipalOriginAttributes& aOriginAttributes,
                                      const nsString& aScope)
{
  if (mShuttingDown) {
    return true;
  }

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (swm) {
    swm->SoftUpdate(aOriginAttributes, NS_ConvertUTF16toUTF8(aScope));
  }

  return true;
}

bool
ServiceWorkerManagerChild::RecvNotifyUnregister(const PrincipalInfo& aPrincipalInfo,
                                                const nsString& aScope)
{
  if (mShuttingDown) {
    return true;
  }

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    // browser shutdown
    return true;
  }

  nsCOMPtr<nsIPrincipal> principal = PrincipalInfoToPrincipal(aPrincipalInfo);
  if (NS_WARN_IF(!principal)) {
    return true;
  }

  nsresult rv = swm->NotifyUnregister(principal, aScope);
  Unused << NS_WARN_IF(NS_FAILED(rv));
  return true;
}

bool
ServiceWorkerManagerChild::RecvNotifyRemove(const nsCString& aHost)
{
  if (mShuttingDown) {
    return true;
  }

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (swm) {
    swm->Remove(aHost);
  }

  return true;
}

bool
ServiceWorkerManagerChild::RecvNotifyRemoveAll()
{
  if (mShuttingDown) {
    return true;
  }

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (swm) {
    swm->RemoveAll();
  }

  return true;
}

} // namespace workers
} // namespace dom
} // namespace mozilla
