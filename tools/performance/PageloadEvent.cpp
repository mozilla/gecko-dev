/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Components.h"
#include "mozilla/Maybe.h"
#include "mozilla/PageloadEvent.h"
#include "mozilla/RandomNum.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/glean/DomMetrics.h"

#include "nsIChannel.h"
#include "nsIEffectiveTLDService.h"
#include "nsITransportSecurityInfo.h"
#include "nsIURI.h"
#include "nsIX509Cert.h"

#include "ScopedNSSTypes.h"
#include "cert.h"
#include "portreg.h"

namespace mozilla::performance::pageload_event {

// We don't want to record an event for every page load, so instead we
// randomly sample the events based on the channel.
//
// For nightly, 10% of page loads will be sent as page_load_domain pings, and
// all other page loads will be sent using the default page_load ping.
//
// For release and beta, only 0.1% of page loads will be sent as
// page_load_domain pings, and 10% of the other page loads will be sent using
// the default ping.
#ifdef NIGHTLY_BUILD
static constexpr uint64_t kNormalSamplingInterval = 1;   // Every pageload.
static constexpr uint64_t kDomainSamplingInterval = 10;  // Every 10 pageloads.
#else
static constexpr uint64_t kNormalSamplingInterval = 10;  // Every 10 pageloads.
static constexpr uint64_t kDomainSamplingInterval =
    1000;  // Every 1000 pageloads.
#endif

PageloadEventType GetPageloadEventType() {
  static_assert(kDomainSamplingInterval >= kNormalSamplingInterval,
                "kDomainSamplingInterval should always be higher than "
                "kNormalSamplingInterval");

  Maybe<uint64_t> rand = mozilla::RandomUint64();
  if (rand.isSome()) {
    uint64_t result =
        static_cast<uint64_t>(rand.value() % kDomainSamplingInterval);
    if (result == 0) {
      return PageloadEventType::kDomain;
    }
    result = static_cast<uint64_t>(rand.value() % kNormalSamplingInterval);
    if (result == 0) {
      return PageloadEventType::kNormal;
    }
  }
  return PageloadEventType::kNone;
}

void PageloadEventData::SetDocumentFeature(DocumentFeature aFeature) {
  uint32_t value = 0;
  if (documentFeatures.isSome()) {
    value = documentFeatures.value();
  }
  value |= aFeature;
  documentFeatures = mozilla::Some(value);
}

void PageloadEventData::SetUserFeature(UserFeature aFeature) {
  uint32_t value = 0;
  if (userFeatures.isSome()) {
    value = userFeatures.value();
  }
  value |= aFeature;
  userFeatures = mozilla::Some(value);
}

// Check for a wildcard in the given cn and return the wildcard
// basename if the hn matches this wildcard.
//
// Using the rules outlined in RFC 2818 and 9525, wildcard names
// must start with "*." in the left-most label.
//
// e.g. `cn = *..example.com` would match `hn = foo.example.com`
//      and we would just return example.com in this case.
static bool DomainMatchesWildcard(char* cn, const char* hn,
                                  nsCString& newDomainOut) {
  if (!cn) {
    return false;
  }

  // Check if cn contains a wildcard. Must start with "*.".
  const bool wildcard = PORT_Strncmp(cn, "*.", 2) == 0;

  if (!wildcard) {
    return false;
  }

  // Get suffix after wildcard label.
  const char* cn_suffix = cn + 2;

  // Check for dot in hn
  const char* hn_suffix = PORT_Strchr(hn, '.');
  if (!hn_suffix) {
    return false;
  }

  // Skip the dot
  hn_suffix++;

  // Need three labels to match a wildcard
  if (!PORT_Strchr(hn_suffix, '.')) {
    return false;
  }

  // Check if the wildcard suffix matches the hn suffix.
  // If true, return the wildcard suffix.
  if (PORT_Strcasecmp(cn_suffix, hn_suffix) == 0) {
    newDomainOut.Assign(cn_suffix);
    return true;
  }

  return false;
}

// There are several conditions before we can assign an etld+1 domain:
// 1.  The server's IP address must be a public IP.
// 2.  The suffix must be on the PSL (Public Suffix List).
// 3.  The certificate chain root must be in the built in root list to
// ensure the trust anchor is public.
// 4.  If the domain matches a wildcard name, then replace it with the
// basename of the wildcard instead.
bool PageloadEventData::MaybeSetPublicRegistrableDomain(nsCOMPtr<nsIURI> aURI,
                                                        nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel, "Expecting a valid channel.");

  nsCOMPtr<nsIEffectiveTLDService> tldService =
      mozilla::components::EffectiveTLD::Service();
  if (!tldService) {
    return false;
  }

  // Make sure the IP address range of the host is public.
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  if (loadInfo->GetIpAddressSpace() != nsILoadInfo::IPAddressSpace::Public) {
    return false;
  }

  nsCOMPtr<nsITransportSecurityInfo> tsi;
  nsresult rv = aChannel->GetSecurityInfo(getter_AddRefs(tsi));
  if (NS_FAILED(rv) || !tsi) {
    return false;
  }

  // Make sure the cert root is in the builtin list.
  bool rootIsBuiltIn = false;
  rv = tsi->GetIsBuiltCertChainRootBuiltInRoot(&rootIsBuiltIn);
  if (NS_FAILED(rv) || !rootIsBuiltIn) {
    return false;
  }

  // Make sure the suffix is on the PSL.
  bool hasKnownPublicSuffix = false;
  rv = tldService->HasKnownPublicSuffix(aURI, &hasKnownPublicSuffix);
  if (NS_FAILED(rv) || !hasKnownPublicSuffix) {
    return false;
  }

  // Get cert for wildcard matching.
  nsCOMPtr<nsIX509Cert> cert;
  rv = tsi->GetServerCert(getter_AddRefs(cert));
  if (NS_FAILED(rv) || !cert) {
    return false;
  }

  UniqueCERTCertificate nssCert(cert->GetCert());
  if (!nssCert) {
    return false;
  }

  // Get ETLD+1 from url, or return on failure.
  nsAutoCString currentBaseDomain;
  rv = tldService->GetBaseDomain(aURI, 0, currentBaseDomain);
  if (NS_FAILED(rv) || currentBaseDomain.IsEmpty()) {
    return false;
  }

  SECStatus secrv = SECFailure;

  // cn and cnbuf is used by CERT_RFC1485_EscapeAndQuote as an output
  // buffer and will add a null terminator.
  const size_t cnBufLen = 255;
  char cnBuf[cnBufLen];

  UniquePLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (!arena) {
    return false;
  }

  // Get subject alternate namelist.
  SECItem subAltName = {siBuffer, nullptr, 0};
  auto onScopeExit = mozilla::MakeScopeExit(
      [&]() { SECITEM_FreeItem(&subAltName, PR_FALSE); });

  secrv = CERT_FindCertExtension(nssCert.get(), SEC_OID_X509_SUBJECT_ALT_NAME,
                                 &subAltName);
  if (secrv != SECSuccess) {
    return false;
  }

  CERTGeneralName* nameList =
      CERT_DecodeAltNameExtension(arena.get(), &subAltName);
  if (!nameList) {
    return false;
  }

  // Loop through the subject alternate namelist and check if any of
  // them are wildcards and match the domain we were given. If a match
  // was found, replace the domain with the wildcard basename.
  CERTGeneralName* current = nameList;
  const char* hn = currentBaseDomain.get();
  do {
    if (current->type == certDNSName) {
      // EscapeAndQuote will copy and add a null terminator.
      secrv = CERT_RFC1485_EscapeAndQuote(cnBuf, cnBufLen,
                                          (char*)current->name.other.data,
                                          current->name.other.len);
      if (secrv != SECSuccess) {
        return false;
      }

      nsCString newDomain;
      if (DomainMatchesWildcard(cnBuf, hn, newDomain)) {
        mDomain = mozilla::Some(newDomain);
        return true;
      }
    }
    current = CERT_GetNextGeneralName(current);
  } while (current && current != nameList);

  // If a matching wildcard wasn't found, use the original etld+1.
  mDomain = mozilla::Some(currentBaseDomain);
  return true;
}

mozilla::glean::perf::PageLoadExtra PageloadEventData::ToPageLoadExtra() const {
  mozilla::glean::perf::PageLoadExtra out;

#define COPY_METRIC(name, type) out.name = this->name;
  FOR_EACH_PAGELOAD_METRIC(COPY_METRIC)
#undef COPY_METRIC
  return out;
}

mozilla::glean::perf::PageLoadDomainExtra
PageloadEventData::ToPageLoadDomainExtra() const {
  mozilla::glean::perf::PageLoadDomainExtra out;
  out.domain = this->mDomain;
  out.httpVer = this->httpVer;
  out.sameOriginNav = this->sameOriginNav;
  out.documentFeatures = this->documentFeatures;
  out.loadType = this->loadType;
  out.lcpTime = this->lcpTime;
  return out;
}

}  // namespace mozilla::performance::pageload_event
