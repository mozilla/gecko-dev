/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "nss_scoped_ptrs.h"
#include "p12.h"
#include "pk11pub.h"
#include "seccomon.h"

#include "asn1_mutators.h"
#include "shared.h"

static SECItem* nicknameCollision(SECItem* oldNick, PRBool* cancel,
                                  void* wincx) {
  *cancel = true;
  return nullptr;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static std::unique_ptr<NSSDatabase> db(new NSSDatabase());

  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  assert(slot);

  // Initialize the decoder.
  SECItem pwItem = {siBuffer, nullptr, 0};
  ScopedSEC_PKCS12DecoderContext dcx(
      SEC_PKCS12DecoderStart(&pwItem, slot.get(), nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr));
  assert(dcx);

  SECStatus rv = SEC_PKCS12DecoderUpdate(dcx.get(), (unsigned char*)data, size);
  if (rv != SECSuccess) {
    return 0;
  }

  // Verify the blob.
  rv = SEC_PKCS12DecoderVerify(dcx.get());
  if (rv != SECSuccess) {
    return 0;
  }

  // Validate bags.
  rv = SEC_PKCS12DecoderValidateBags(dcx.get(), nicknameCollision);
  if (rv != SECSuccess) {
    return 0;
  }

  // Import cert and key.
  rv = SEC_PKCS12DecoderImportBags(dcx.get());
  if (rv != SECSuccess) {
    return 0;
  }

  return 0;
}

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t* data, size_t size,
                                          size_t maxSize, unsigned int seed) {
  return CustomMutate(
      Mutators({ASN1Mutators::FlipConstructed, ASN1Mutators::ChangeType}), data,
      size, maxSize, seed);
}
