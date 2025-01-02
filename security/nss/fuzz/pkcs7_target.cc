/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstddef>
#include <cstdint>
#include <memory>

#include "cert.h"
#include "seccomon.h"

#include "asn1_mutators.h"
#include "shared.h"

static SECStatus importFunc(void *arg, SECItem **certs, int numCerts) {
  // This way we check that the callback gets called with the correct
  // `numCerts`, as an invalid value potentially causes `certs` to go
  // out-of-bounds. Testing `CERT_Hexify` is a nice bonus.
  while (numCerts--) {
    char *hex = CERT_Hexify(*certs, false);
    free(hex);

    certs++;
  }

  return SECSuccess;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static std::unique_ptr<NSSDatabase> db(new NSSDatabase());

  CERT_DecodeCertPackage((char *)data, (int)size, importFunc, nullptr);

  return 0;
}

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *data, size_t size,
                                          size_t maxSize, unsigned int seed) {
  return CustomMutate(
      Mutators({ASN1Mutators::FlipConstructed, ASN1Mutators::ChangeType}), data,
      size, maxSize, seed);
}
