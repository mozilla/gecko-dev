/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef crypto_hash_sha2_h
#define crypto_hash_sha2_h

#include <stdint.h>
#include <stddef.h>

extern "C" {
// 32 bytes will be written to `output` so it must point at a buffer
// at least that big.
void crypto_hash_sha256(const uint8_t* input, size_t length, uint8_t* output);
};

#endif  // crypto_hash_sha2_h
