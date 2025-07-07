/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "keyhi.h"
#include "nss_scoped_ptrs.h"
#include "pk11pub.h"
#include "seccomon.h"
#include "utilrename.h"

#include "asn1/mutators.h"
#include "base/database.h"
#include "base/mutate.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static NSSDatabase db = NSSDatabase();

  SECItem derPki = {siBuffer, (unsigned char *)data, (unsigned int)size};

  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  assert(slot);

  SECKEYPrivateKey *privKey = nullptr;
  if (PK11_ImportDERPrivateKeyInfoAndReturnKey(
          slot.get(), &derPki, nullptr, nullptr, false, false, KU_ALL, &privKey,
          nullptr) != SECSuccess) {
    return 0;
  }

  (void)SECKEY_PrivateKeyStrengthInBits(privKey);
  (void)SECKEY_GetPrivateKeyType(privKey);
  (void)PK11_SignatureLen(privKey);
  (void)PK11_GetPrivateModulusLen(privKey);

  ScopedSECKEYPublicKey pubKey(SECKEY_ConvertToPublicKey(privKey));
  ScopedCERTCertificate cert(PK11_GetCertFromPrivateKey(privKey));

  char *nickname = PK11_GetPrivateKeyNickname(privKey);
  PORT_Free(nickname);

  SECKEYPQGParams *params = PK11_GetPQGParamsFromPrivateKey(privKey);
  PORT_FreeArena(params ? params->arena : nullptr, PR_FALSE);

  SECKEY_DestroyPrivateKey(privKey);

  return 0;
}

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *data, size_t size,
                                          size_t maxSize, unsigned int seed) {
  return CustomMutate(
      Mutators({ASN1Mutators::FlipConstructed, ASN1Mutators::ChangeType}), data,
      size, maxSize, seed);
}
