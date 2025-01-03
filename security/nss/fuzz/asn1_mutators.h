/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ASN1_MUTATORS_H_
#define ASN1_MUTATORS_H_

#include <cstddef>
#include <cstdint>

namespace ASN1Mutators {

size_t FlipConstructed(uint8_t *data, size_t size, size_t maxSize,
                       unsigned int seed);
size_t ChangeType(uint8_t *data, size_t size, size_t maxSize,
                  unsigned int seed);

}  // namespace ASN1Mutators

#endif  // ASN1_MUTATORS_H_
