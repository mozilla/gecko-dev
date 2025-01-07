/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TLS_MUTATORS_H_
#define TLS_MUTATORS_H_

#include <cstddef>
#include <cstdint>

// Number of additional bytes in the TLS header.
// Used to properly skip DTLS seqnums.
#ifdef IS_DTLS_FUZZ
#define EXTRA_HEADER_BYTES 8
#else
#define EXTRA_HEADER_BYTES 0
#endif

namespace TlsMutators {

size_t DropRecord(uint8_t *data, size_t size, size_t maxSize,
                  unsigned int seed);
size_t ShuffleRecords(uint8_t *data, size_t size, size_t maxSize,
                      unsigned int seed);
size_t DuplicateRecord(uint8_t *data, size_t size, size_t maxSize,
                       unsigned int seed);
size_t TruncateRecord(uint8_t *data, size_t size, size_t maxSize,
                      unsigned int seed);
size_t FragmentRecord(uint8_t *data, size_t size, size_t maxSize,
                      unsigned int seed);

size_t CrossOver(const uint8_t *data1, size_t size1, const uint8_t *data2,
                 size_t size2, uint8_t *out, size_t maxOutSize,
                 unsigned int seed);

}  // namespace TlsMutators

#endif  // TLS_MUTATORS_H_
