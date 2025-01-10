/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GetAddrInfo.h"
#include "mozilla/glean/GleanMetrics.h"
#include "mozilla/net/DNSPacket.h"
#include "nsIDNSService.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/ThreadLocal.h"

#include <dns_sd.h>
#include <unistd.h>
#include <arpa/inet.h>

namespace mozilla::net {

#define LOG(msg, ...) \
  MOZ_LOG(gGetAddrInfoLog, LogLevel::Debug, ("[DNS]: " msg, ##__VA_ARGS__))

struct DNSContext {
  nsresult mRv = NS_OK;
  TypeRecordResultType* mResult;
  nsCString mHost;
  uint32_t* mTTL;
};

// Callback for DNSServiceQueryRecord
void QueryCallback(DNSServiceRef aSDRef, DNSServiceFlags aFlags,
                   uint32_t aInterfaceIndex, DNSServiceErrorType aErrorCode,
                   const char* aFullname, uint16_t aRRType, uint16_t aRRClass,
                   uint16_t aRDLen, const void* aRdata, uint32_t aTtl,
                   void* aContext) {
  struct DNSContext* context = (struct DNSContext*)aContext;

  LOG("DNS response name: %s type: %u rdlen %u class %u ttl %u", aFullname,
      aRRType, aRDLen, aRRClass, aTtl);

  if (aErrorCode != kDNSServiceErr_NoError) {
    LOG("Error resolving record: %d\n", aErrorCode);
    context->mRv = NS_ERROR_UNKNOWN_HOST;
    return;
  }

  if (NS_FAILED(context->mRv)) {
    LOG("Parsing already failed for a previous record");
    return;
  }

  // Process the rdata for HTTPS records (type 65)
  if (aRRType != TRRTYPE_HTTPSSVC || aRDLen == 0) {
    context->mRv = NS_ERROR_UNKNOWN_HOST;
    return;
  }

  nsDependentCString fullname(aFullname);
  if (fullname.Length() && fullname.Last() == '.') {
    // The fullname argument is always FQDN
    fullname.Rebind(aFullname, fullname.Length() - 1);
  }

  struct SVCB parsed;
  nsresult rv = DNSPacket::ParseHTTPS(
      aRDLen, parsed, 0, (const unsigned char*)aRdata, aRDLen, fullname);
  if (NS_FAILED(rv)) {
    LOG("ParseHTTPS failed\n");
    context->mRv = rv;
    return;
  }

  if (parsed.mSvcDomainName.IsEmpty() && parsed.mSvcFieldPriority == 0) {
    // For AliasMode SVCB RRs, a TargetName of "." indicates that the
    // service is not available or does not exist.
    return;
  }

  if (parsed.mSvcFieldPriority == 0) {
    // Alias form SvcDomainName must not have the "." value (empty)
    if (parsed.mSvcDomainName.IsEmpty()) {
      context->mRv = NS_ERROR_UNEXPECTED;
      return;
    }
    LOG("alias mode %s -> %s", context->mHost.get(),
        parsed.mSvcDomainName.get());
    context->mHost = parsed.mSvcDomainName;
    ToLowerCase(context->mHost);
    return;
  }

  if (!context->mResult->is<TypeRecordHTTPSSVC>()) {
    *context->mResult = mozilla::AsVariant(CopyableTArray<SVCB>());
  }
  auto& results = context->mResult->as<TypeRecordHTTPSSVC>();
  results.AppendElement(parsed);
  *context->mTTL = std::min<uint32_t>(*context->mTTL, aTtl);
}

nsresult ResolveHTTPSRecordImpl(const nsACString& aHost,
                                nsIDNSService::DNSFlags aFlags,
                                TypeRecordResultType& aResult, uint32_t& aTTL) {
  nsAutoCString host(aHost);
  nsAutoCString cname;

  if (xpc::IsInAutomation() &&
      !StaticPrefs::network_dns_native_https_query_in_automation()) {
    return NS_ERROR_UNKNOWN_HOST;
  }

  LOG("resolving %s\n", host.get());
  TimeStamp startTime = TimeStamp::Now();

  struct DNSContext context{
      .mResult = &aResult,
      .mHost = host,
      .mTTL = &aTTL,
  };

  DNSServiceRef sdRef;
  DNSServiceErrorType err;

  err = DNSServiceQueryRecord(&sdRef,
                              0,  // No flags
                              0,  // All interfaces
                              host.get(), TRRTYPE_HTTPSSVC, kDNSServiceClass_IN,
                              QueryCallback, &context);

  if (err != kDNSServiceErr_NoError) {
    LOG("DNSServiceQueryRecord failed: %d\n", err);
    return NS_ERROR_UNKNOWN_HOST;
  }

  int fd = DNSServiceRefSockFD(sdRef);
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(fd, &readfds);

  int result = select(fd + 1, &readfds, NULL, NULL, NULL);
  if (result > 0 && FD_ISSET(fd, &readfds)) {
    // Process the result
    DNSServiceProcessResult(sdRef);
  } else if (result < 0) {
    LOG("select() failed");
  }

  // Cleanup
  DNSServiceRefDeallocate(sdRef);
  mozilla::glean::networking::dns_native_https_call_time.AccumulateRawDuration(
      TimeStamp::Now() - startTime);

  LOG("resolving %s done %x ttl=%u", host.get(), context.mRv, aTTL);
  if (NS_FAILED(context.mRv)) {
    return context.mRv;
  }
  if (aResult.is<Nothing>()) {
    // The call succeeded, but no HTTPS records were found.
    return NS_ERROR_UNKNOWN_HOST;
  }
  if (aTTL == UINT32_MAX) {
    aTTL = 60;  // Defaults to 60 seconds
  }
  return NS_OK;
}

void DNSThreadShutdown() {}

}  // namespace mozilla::net
