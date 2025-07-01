/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NotificationHandler.h"

#include "mozilla/dom/ClientIPCTypes.h"
#include "mozilla/dom/ClientOpenWindowUtils.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/dom/ServiceWorkerManager.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"

namespace mozilla::dom::notification {

nsresult RespondOnClick(nsIPrincipal* aPrincipal, const nsAString& aScope,
                        const IPCNotification& aNotification,
                        const nsAString& aActionName) {
  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    return NS_ERROR_FAILURE;
  }

  nsAutoCString originSuffix;
  MOZ_TRY(aPrincipal->GetOriginSuffix(originSuffix));

  nsresult rv = swm->SendNotificationClickEvent(originSuffix, aScope,
                                                aNotification, aActionName);
  if (NS_FAILED(rv)) {
    // No active service worker, let's do the last resort
    // TODO(krosylight): We should prevent entering this path as much as
    // possible and ultimately remove this. See bug 1972120.
    return OpenWindowFor(aPrincipal);
  }
  return NS_OK;
}

nsresult OpenWindowFor(nsIPrincipal* aPrincipal) {
  nsAutoCString origin;
  MOZ_TRY(aPrincipal->GetOrigin(origin));

  // XXX: We should be able to just pass nsIPrincipal directly
  mozilla::ipc::PrincipalInfo info{};
  MOZ_TRY(PrincipalToPrincipalInfo(aPrincipal, &info));

  (void)ClientOpenWindow(nullptr,
                         ClientOpenWindowArgs(info, Nothing(), ""_ns, origin));
  return NS_OK;
}

}  // namespace mozilla::dom::notification
