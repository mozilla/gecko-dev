/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstddef>
#include <cstdint>
#include <memory>

#include "keyhi.h"
#include "pk11pub.h"

#include "asn1_mutators.h"
#include "shared.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static std::unique_ptr<NSSDatabase> db(new NSSDatabase());

  PK11SlotInfo *slot = PK11_GetInternalSlot();
  assert(slot);

  SECItem derPki = {siBuffer, (unsigned char *)data, (unsigned int)size};

  SECKEYPrivateKey *key = nullptr;
  if (PK11_ImportDERPrivateKeyInfoAndReturnKey(slot, &derPki, nullptr, nullptr,
                                               false, false, KU_ALL, &key,
                                               nullptr) == SECSuccess) {
    SECKEY_DestroyPrivateKey(key);
  }

  PK11_FreeSlot(slot);
  return 0;
}

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *data, size_t size,
                                          size_t maxSize, unsigned int seed) {
  return CustomMutate(
      Mutators({ASN1Mutators::FlipConstructed, ASN1Mutators::ChangeType}), data,
      size, maxSize, seed);
}
