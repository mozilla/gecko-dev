/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstddef>
#include <cstdint>

#include "scoped_ptrs_smime.h"
#include "smime.h"

#include "asn1/mutators.h"
#include "base/database.h"
#include "base/mutate.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static NSSDatabase db = NSSDatabase();

  SECItem buffer = {siBuffer, (unsigned char *)data, (unsigned int)size};

  ScopedNSSCMSMessage cmsg(NSS_CMSMessage_CreateFromDER(
      &buffer, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));
  (void)NSS_CMSMessage_IsSigned(cmsg.get());

  return 0;
}

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *data, size_t size,
                                          size_t maxSize, unsigned int seed) {
  return CustomMutate(
      Mutators({ASN1Mutators::FlipConstructed, ASN1Mutators::ChangeType}), data,
      size, maxSize, seed);
}
