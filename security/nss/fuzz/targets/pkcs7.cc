/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstddef>
#include <cstdint>

#include "cert.h"

#include "asn1/mutators.h"
#include "base/database.h"
#include "base/mutate.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static NSSDatabase db = NSSDatabase();

  CERTCertificate *cert = CERT_DecodeCertFromPackage((char *)data, (int)size);
  CERT_DestroyCertificate(cert);

  return 0;
}

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *data, size_t size,
                                          size_t maxSize, unsigned int seed) {
  return CustomMutate(
      Mutators({ASN1Mutators::FlipConstructed, ASN1Mutators::ChangeType}), data,
      size, maxSize, seed);
}
