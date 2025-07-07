/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstddef>
#include <cstdint>

#include "cert.h"
#include "nss_scoped_ptrs.h"
#include "prtime.h"
#include "prtypes.h"
#include "seccomon.h"
#include "utilrename.h"

#include "asn1/mutators.h"
#include "base/database.h"
#include "base/mutate.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static NSSDatabase db = NSSDatabase();

  ScopedCERTCertificate cert(
      CERT_DecodeCertFromPackage((char *)data, (int)size));
  if (!cert) {
    return 0;
  }

  SECCertificateUsage usage;
  SECItem der;

  (void)CERT_VerifyCertificateNow(CERT_GetDefaultCertDB(), cert.get(), PR_TRUE,
                                  certificateUsageCheckAllUsages, nullptr,
                                  &usage);
  (void)CERT_VerifyCertName(cert.get(), "fuzz.host");
  (void)CERT_GetCertificateDer(cert.get(), &der);

  ScopedSECKEYPublicKey pubk(CERT_ExtractPublicKey(cert.get()));
  ScopedCERTCertList chain(
      CERT_GetCertChainFromCert(cert.get(), PR_Now(), certUsageEmailSigner));

  CERTCertNicknames *patterns = CERT_GetValidDNSPatternsFromCert(cert.get());
  PORT_FreeArena(patterns ? patterns->arena : nullptr, PR_FALSE);

  return 0;
}

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *data, size_t size,
                                          size_t maxSize, unsigned int seed) {
  return CustomMutate(
      Mutators({ASN1Mutators::FlipConstructed, ASN1Mutators::ChangeType}), data,
      size, maxSize, seed);
}
