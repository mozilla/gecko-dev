/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mutators.h"

#include <cassert>
#include <cstring>
#include <random>
#include <tuple>

static std::tuple<uint8_t *, size_t> ParseItem(uint8_t *data,
                                               size_t maxLength) {
  // Short form. Bit 8 has value "0" and bits 7-1 give the length.
  if ((data[1] & 0x80) == 0) {
    size_t length = std::min(static_cast<size_t>(data[1]), maxLength - 2);
    return std::make_tuple(&data[2], length);
  }

  // Constructed, indefinite length. Read until {0x00, 0x00}.
  if (data[1] == 0x80) {
    void *offset = memmem(&data[2], maxLength - 2, "\0", 2);
    size_t length = offset ? (static_cast<uint8_t *>(offset) - &data[2]) + 2
                           : maxLength - 2;
    return std::make_tuple(&data[2], length);
  }

  // Long form. Two to 127 octets. Bit 8 of first octet has value "1"
  // and bits 7-1 give the number of additional length octets.
  size_t octets = std::min(static_cast<size_t>(data[1] & 0x7f), maxLength - 2);

  // Handle lengths bigger than 32 bits.
  if (octets > 4) {
    // Ignore any further children, assign remaining length.
    return std::make_tuple(&data[2] + octets, maxLength - 2 - octets);
  }

  // Parse the length.
  size_t length = 0;
  for (size_t j = 0; j < octets; j++) {
    length = (length << 8) | data[2 + j];
  }

  length = std::min(length, maxLength - 2 - octets);
  return std::make_tuple(&data[2] + octets, length);
}

static std::vector<uint8_t *> ParseItems(uint8_t *data, size_t size) {
  std::vector<uint8_t *> items;
  std::vector<size_t> lengths;

  // The first item is always the whole corpus.
  items.push_back(data);
  lengths.push_back(size);

  // Can't use iterators here because the `items` vector is modified inside the
  // loop. That's safe as long as we always check `items.size()` before every
  // iteration, and only call `.push_back()` to append new items we found.
  // Items are accessed through `items.at()`, we hold no references.
  for (size_t i = 0; i < items.size(); i++) {
    uint8_t *item = items.at(i);
    size_t remaining = lengths.at(i);

    // Empty or primitive items have no children.
    if (remaining == 0 || (0x20 & item[0]) == 0) {
      continue;
    }

    while (remaining > 2) {
      uint8_t *content;
      size_t length;

      std::tie(content, length) = ParseItem(item, remaining);

      if (length > 0) {
        // Record the item.
        items.push_back(content);

        // Record the length for further parsing.
        lengths.push_back(length);
      }

      // Reduce number of bytes left in current item.
      remaining -= length + (content - item);

      // Skip the item we just parsed.
      item = content + length;
    }
  }

  return items;
}

namespace ASN1Mutators {

size_t FlipConstructed(uint8_t *data, size_t size, size_t maxSize,
                       unsigned int seed) {
  auto items = ParseItems(data, size);

  std::mt19937 rng(seed);
  std::uniform_int_distribution<size_t> dist(0, items.size() - 1);
  uint8_t *item = items.at(dist(rng));

  // Flip "constructed" type bit.
  item[0] ^= 0x20;

  return size;
}

size_t ChangeType(uint8_t *data, size_t size, size_t maxSize,
                  unsigned int seed) {
  auto items = ParseItems(data, size);

  std::mt19937 rng(seed);
  std::uniform_int_distribution<size_t> dist(0, items.size() - 1);
  uint8_t *item = items.at(dist(rng));

  // Change type to a random int [0, 30].
  static std::uniform_int_distribution<size_t> tdist(0, 30);
  item[0] = tdist(rng);

  return size;
}

}  // namespace ASN1Mutators
