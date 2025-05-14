/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAlertsUtils.h"

#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsIURI.h"
#include "nsString.h"

/* static */
bool nsAlertsUtils::IsActionablePrincipal(nsIPrincipal* aPrincipal) {
  return aPrincipal &&
         !nsContentUtils::IsSystemOrExpandedPrincipal(aPrincipal) &&
         !aPrincipal->GetIsNullPrincipal();
}

/* static */
void nsAlertsUtils::GetSourceHostPort(nsIPrincipal* aPrincipal,
                                      nsAString& aHostPort) {
  aHostPort.Truncate();
  if (!IsActionablePrincipal(aPrincipal)) {
    return;
  }
  nsAutoCString hostPort;
  if (NS_WARN_IF(NS_FAILED(aPrincipal->GetHostPort(hostPort)))) {
    return;
  }
  CopyUTF8toUTF16(hostPort, aHostPort);
}

/* static */
nsresult nsAlertsUtils::GetOrigin(nsIPrincipal* aPrincipal,
                                  nsACString& aOrigin) {
  aOrigin.SetIsVoid(true);
  if (!IsActionablePrincipal(aPrincipal)) {
    return NS_OK;
  }
  return aPrincipal->GetOrigin(aOrigin);
}
