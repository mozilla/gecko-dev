/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerCSPContext.h"

#include "nsNetUtil.h"
#include "mozilla/dom/nsCSPParser.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/ipc/BackgroundUtils.h"

namespace mozilla::dom {

/* static */
Result<UniquePtr<WorkerCSPContext>, nsresult> WorkerCSPContext::CreateFromCSP(
    nsIContentSecurityPolicy* aCSP) {
  AssertIsOnMainThread();

  mozilla::ipc::CSPInfo cspInfo;
  nsresult rv = CSPToCSPInfo(aCSP, &cspInfo);
  if (NS_FAILED(rv)) {
    return Err(rv);
  }
  return MakeUnique<WorkerCSPContext>(std::move(cspInfo));
}

const nsTArray<UniquePtr<const nsCSPPolicy>>& WorkerCSPContext::Policies() {
  EnsureIPCPoliciesRead();
  return mPolicies;
}

void WorkerCSPContext::EnsureIPCPoliciesRead() {
  MOZ_DIAGNOSTIC_ASSERT(!!GetCurrentThreadWorkerPrivate());

  if (!mPolicies.IsEmpty() || mCSPInfo.policyInfos().IsEmpty()) {
    return;
  }

  nsCOMPtr<nsIURI> selfURI;
  if (NS_WARN_IF(NS_FAILED(
          NS_NewURI(getter_AddRefs(selfURI), mCSPInfo.selfURISpec())))) {
    return;
  }

  for (const auto& policy : mCSPInfo.policyInfos()) {
    UniquePtr<const nsCSPPolicy> cspPolicy(
        nsCSPParser::parseContentSecurityPolicy(
            policy.policy(), selfURI, policy.reportOnlyFlag(), nullptr,
            policy.deliveredViaMetaTagFlag(),
            /* aSuppressLogMessages */ true));
    if (cspPolicy) {
      mPolicies.AppendElement(std::move(cspPolicy));
    }
  }
}

}  // namespace mozilla::dom
