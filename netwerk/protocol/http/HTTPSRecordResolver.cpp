/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "HTTPSRecordResolver.h"
#include "mozilla/Components.h"
#include "mozilla/StaticPrefs_network.h"
#include "nsIDNSByTypeRecord.h"
#include "nsIDNSAdditionalInfo.h"
#include "nsIDNSService.h"
#include "nsHttpConnectionInfo.h"
#include "nsNetCID.h"
#include "nsAHttpTransaction.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(HTTPSRecordResolver, nsIDNSListener)

HTTPSRecordResolver::HTTPSRecordResolver(nsAHttpTransaction* aTransaction)
    : mTransaction(aTransaction),
      mConnInfo(aTransaction->ConnectionInfo()),
      mCaps(aTransaction->Caps()) {}

HTTPSRecordResolver::~HTTPSRecordResolver() = default;

nsresult HTTPSRecordResolver::FetchHTTPSRRInternal(
    nsIEventTarget* aTarget, nsICancelable** aDNSRequest) {
  NS_ENSURE_ARG_POINTER(aTarget);

  // Only fetch HTTPS RR for https.
  if (!mConnInfo->FirstHopSSL()) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDNSService> dns = mozilla::components::DNS::Service();
  if (!dns) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsIDNSService::DNSFlags flags =
      nsIDNSService::GetFlagsFromTRRMode(mConnInfo->GetTRRMode());
  if (mCaps & NS_HTTP_REFRESH_DNS) {
    flags |= nsIDNSService::RESOLVE_BYPASS_CACHE;
  }

  nsCOMPtr<nsIDNSAdditionalInfo> info;
  if (mConnInfo->OriginPort() != NS_HTTPS_DEFAULT_PORT) {
    dns->NewAdditionalInfo(""_ns, mConnInfo->OriginPort(),
                           getter_AddRefs(info));
  }

  MutexAutoLock lock(mMutex);

  nsresult rv = dns->AsyncResolveNative(
      mConnInfo->GetOrigin(), nsIDNSService::RESOLVE_TYPE_HTTPSSVC, flags, info,
      this, aTarget, mConnInfo->GetOriginAttributes(),
      getter_AddRefs(mHTTPSRecordRequest));

  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsICancelable> request = mHTTPSRecordRequest;
  request.forget(aDNSRequest);

  if (!StaticPrefs::network_dns_https_rr_check_record_with_cname()) {
    return rv;
  }

  rv = dns->AsyncResolveNative(
      mConnInfo->GetOrigin(), nsIDNSService::RESOLVE_TYPE_DEFAULT,
      flags | nsIDNSService::RESOLVE_CANONICAL_NAME, nullptr, this, aTarget,
      mConnInfo->GetOriginAttributes(), getter_AddRefs(mCnameRequest));
  return rv;
}

NS_IMETHODIMP HTTPSRecordResolver::OnLookupComplete(nsICancelable* aRequest,
                                                    nsIDNSRecord* aRecord,
                                                    nsresult aStatus) {
  MutexAutoLock lock(mMutex);
  if (!mTransaction || mDone) {
    // The transaction is not interesed in a response anymore.
    mCnameRequest = nullptr;
    mHTTPSRecordRequest = nullptr;
    return NS_OK;
  }

  if (aRequest == mHTTPSRecordRequest) {
    mHTTPSRecordRequest = nullptr;
    nsCOMPtr<nsIDNSHTTPSSVCRecord> record = do_QueryInterface(aRecord);

    if (!record || NS_FAILED(aStatus)) {
      // When failed, we don't want to wait for the CNAME.
      mCnameRequest = nullptr;
      MutexAutoUnlock unlock(mMutex);
      return InvokeCallback(nullptr, nullptr, ""_ns);
    }

    mHTTPSRecord = record;
    // Waiting for the address record.
    if (mCnameRequest) {
      return NS_OK;
    }

    nsCOMPtr<nsISVCBRecord> svcbRecord;
    if (NS_FAILED(mHTTPSRecord->GetServiceModeRecordWithCname(
            mCaps & NS_HTTP_DISALLOW_SPDY, mCaps & NS_HTTP_DISALLOW_HTTP3,
            ""_ns, getter_AddRefs(svcbRecord)))) {
      MutexAutoUnlock unlock(mMutex);
      return InvokeCallback(mHTTPSRecord, nullptr, ""_ns);
    }

    MutexAutoUnlock unlock(mMutex);
    return InvokeCallback(mHTTPSRecord, svcbRecord, ""_ns);
  }

  // Having mCnameRequest indicates that we are interested in the address
  // record.
  if (mCnameRequest && aRequest == mCnameRequest) {
    mCnameRequest = nullptr;
    nsCOMPtr<nsIDNSAddrRecord> addrRecord = do_QueryInterface(aRecord);

    if (!addrRecord || !mHTTPSRecord || NS_FAILED(aStatus)) {
      MutexAutoUnlock unlock(mMutex);
      return InvokeCallback(nullptr, nullptr, ""_ns);
    }

    nsCString cname;
    Unused << addrRecord->GetCanonicalName(cname);
    nsCOMPtr<nsISVCBRecord> svcbRecord;
    if (NS_FAILED(mHTTPSRecord->GetServiceModeRecordWithCname(
            mCaps & NS_HTTP_DISALLOW_SPDY, mCaps & NS_HTTP_DISALLOW_HTTP3,
            cname, getter_AddRefs(svcbRecord)))) {
      MutexAutoUnlock unlock(mMutex);
      return InvokeCallback(mHTTPSRecord, nullptr, cname);
    }

    MutexAutoUnlock unlock(mMutex);
    return InvokeCallback(mHTTPSRecord, svcbRecord, cname);
  }

  return NS_OK;
}

nsresult HTTPSRecordResolver::InvokeCallback(
    nsIDNSHTTPSSVCRecord* aHTTPSSVCRecord,
    nsISVCBRecord* aHighestPriorityRecord, const nsACString& aCname) {
  MOZ_ASSERT(!mDone);

  mDone = true;
  return mTransaction->OnHTTPSRRAvailable(aHTTPSSVCRecord,
                                          aHighestPriorityRecord, aCname);
}

void HTTPSRecordResolver::PrefetchAddrRecord(const nsACString& aTargetName,
                                             bool aRefreshDNS) {
  MOZ_ASSERT(mTransaction);
  nsCOMPtr<nsIDNSService> dns = mozilla::components::DNS::Service();
  if (!dns) {
    return;
  }

  nsIDNSService::DNSFlags flags = nsIDNSService::GetFlagsFromTRRMode(
      mTransaction->ConnectionInfo()->GetTRRMode());
  if (aRefreshDNS) {
    flags |= nsIDNSService::RESOLVE_BYPASS_CACHE;
  }

  nsCOMPtr<nsICancelable> tmpOutstanding;

  Unused << dns->AsyncResolveNative(
      aTargetName, nsIDNSService::RESOLVE_TYPE_DEFAULT,
      flags | nsIDNSService::RESOLVE_SPECULATE, nullptr, this,
      GetCurrentSerialEventTarget(),
      mTransaction->ConnectionInfo()->GetOriginAttributes(),
      getter_AddRefs(tmpOutstanding));
}

void HTTPSRecordResolver::Close() {
  mTransaction = nullptr;
  MutexAutoLock lock(mMutex);
  if (mCnameRequest) {
    mCnameRequest->Cancel(NS_ERROR_ABORT);
    mCnameRequest = nullptr;
  }
  if (mHTTPSRecordRequest) {
    mHTTPSRecordRequest->Cancel(NS_ERROR_ABORT);
    mHTTPSRecordRequest = nullptr;
  }
}

}  // namespace net
}  // namespace mozilla
