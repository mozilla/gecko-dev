// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "kern.h"

// kern - Kerning
// http://www.microsoft.com/typography/otspec/kern.htm

#define TABLE_NAME "kern"

#define DROP_THIS_TABLE \
  do { \
    delete file->kern; \
    file->kern = 0; \
    OTS_FAILURE_MSG("Table discarded"); \
  } while (0)

namespace ots {

bool ots_kern_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  Buffer table(data, length);

  OpenTypeKERN *kern = new OpenTypeKERN;
  file->kern = kern;

  uint16_t num_tables = 0;
  if (!table.ReadU16(&kern->version) ||
      !table.ReadU16(&num_tables)) {
    return OTS_FAILURE_MSG("Failed to read kern header");
  }

  if (kern->version > 0) {
    DROP_THIS_TABLE;
    return true;
  }

  if (num_tables == 0) {
    OTS_WARNING("num_tables is zero");
    DROP_THIS_TABLE;
    return true;
  }

  kern->subtables.reserve(num_tables);
  for (unsigned i = 0; i < num_tables; ++i) {
    OpenTypeKERNFormat0 subtable;
    uint16_t sub_length = 0;

    if (!table.ReadU16(&subtable.version) ||
        !table.ReadU16(&sub_length)) {
      return OTS_FAILURE_MSG("Failed to read kern subtable %d header", i);
    }

    if (subtable.version > 0) {
      OTS_WARNING("Bad subtable version: %d", subtable.version);
      continue;
    }

    const size_t current_offset = table.offset();
    if (current_offset - 4 + sub_length > length) {
      return OTS_FAILURE_MSG("Bad kern subtable %d offset %ld", i, current_offset);
    }

    if (!table.ReadU16(&subtable.coverage)) {
      return OTS_FAILURE_MSG("Cailed to read kern subtable %d coverage", i);
    }

    if (!(subtable.coverage & 0x1)) {
      OTS_WARNING(
          "We don't support vertical data as the renderer doesn't support it.");
      continue;
    }
    if (subtable.coverage & 0xF0) {
      OTS_WARNING("Reserved fields should zero-filled.");
      DROP_THIS_TABLE;
      return true;
    }
    const uint32_t format = (subtable.coverage & 0xFF00) >> 8;
    if (format != 0) {
      OTS_WARNING("Format %d is not supported.", format);
      continue;
    }

    // Parse the format 0 field.
    uint16_t num_pairs = 0;
    if (!table.ReadU16(&num_pairs) ||
        !table.ReadU16(&subtable.search_range) ||
        !table.ReadU16(&subtable.entry_selector) ||
        !table.ReadU16(&subtable.range_shift)) {
      return OTS_FAILURE_MSG("Failed to read kern subtable %d format 0 fields", i);
    }

    if (!num_pairs) {
      OTS_WARNING("Zero length subtable is found.");
      DROP_THIS_TABLE;
      return true;
    }

    // Sanity checks for search_range, entry_selector, and range_shift. See the
    // comment in ots.cc for details.
    const size_t kFormat0PairSize = 6;  // left, right, and value. 2 bytes each.
    if (num_pairs > (65536 / kFormat0PairSize)) {
      // Some fonts (e.g. calibri.ttf, pykes_peak_zero.ttf) have pairs >= 10923.
      OTS_WARNING("Too large subtable.");
      DROP_THIS_TABLE;
      return true;
    }
    unsigned max_pow2 = 0;
    while (1u << (max_pow2 + 1) <= num_pairs) {
      ++max_pow2;
    }
    const uint16_t expected_search_range = (1u << max_pow2) * kFormat0PairSize;
    if (subtable.search_range != expected_search_range) {
      OTS_WARNING("bad search range");
      subtable.search_range = expected_search_range;
    }
    if (subtable.entry_selector != max_pow2) {
      return OTS_FAILURE_MSG("Bad subtable %d entry selector %d", i, subtable.entry_selector);
    }
    const uint32_t expected_range_shift
        = kFormat0PairSize * num_pairs - subtable.search_range;
    if (subtable.range_shift != expected_range_shift) {
      OTS_WARNING("bad range shift");
      subtable.range_shift = expected_range_shift;
    }

    // Read kerning pairs.
    subtable.pairs.reserve(num_pairs);
    uint32_t last_pair = 0;
    for (unsigned j = 0; j < num_pairs; ++j) {
      OpenTypeKERNFormat0Pair kerning_pair;
      if (!table.ReadU16(&kerning_pair.left) ||
          !table.ReadU16(&kerning_pair.right) ||
          !table.ReadS16(&kerning_pair.value)) {
        return OTS_FAILURE_MSG("Failed to read subtable %d kerning pair %d", i, j);
      }
      const uint32_t current_pair
          = (kerning_pair.left << 16) + kerning_pair.right;
      if (j != 0 && current_pair <= last_pair) {
        OTS_WARNING("Kerning pairs are not sorted.");
        // Many free fonts don't follow this rule, so we don't call OTS_FAILURE
        // in order to support these fonts.
        DROP_THIS_TABLE;
        return true;
      }
      last_pair = current_pair;
      subtable.pairs.push_back(kerning_pair);
    }

    kern->subtables.push_back(subtable);
  }

  if (!kern->subtables.size()) {
    OTS_WARNING("All subtables are removed.");
    DROP_THIS_TABLE;
    return true;
  }

  return true;
}

bool ots_kern_should_serialise(OpenTypeFile *file) {
  if (!file->glyf) return false;  // this table is not for CFF fonts.
  return file->kern != NULL;
}

bool ots_kern_serialise(OTSStream *out, OpenTypeFile *file) {
  const OpenTypeKERN *kern = file->kern;

  if (!out->WriteU16(kern->version) ||
      !out->WriteU16(kern->subtables.size())) {
    return OTS_FAILURE_MSG("Can't write kern table header");
  }

  for (unsigned i = 0; i < kern->subtables.size(); ++i) {
    const uint16_t length = 14 + (6 * kern->subtables[i].pairs.size());
    if (!out->WriteU16(kern->subtables[i].version) ||
        !out->WriteU16(length) ||
        !out->WriteU16(kern->subtables[i].coverage) ||
        !out->WriteU16(kern->subtables[i].pairs.size()) ||
        !out->WriteU16(kern->subtables[i].search_range) ||
        !out->WriteU16(kern->subtables[i].entry_selector) ||
        !out->WriteU16(kern->subtables[i].range_shift)) {
      return OTS_FAILURE_MSG("Failed to write kern subtable %d", i);
    }
    for (unsigned j = 0; j < kern->subtables[i].pairs.size(); ++j) {
      if (!out->WriteU16(kern->subtables[i].pairs[j].left) ||
          !out->WriteU16(kern->subtables[i].pairs[j].right) ||
          !out->WriteS16(kern->subtables[i].pairs[j].value)) {
        return OTS_FAILURE_MSG("Failed to write kern pair %d for subtable %d", j, i);
      }
    }
  }

  return true;
}

void ots_kern_free(OpenTypeFile *file) {
  delete file->kern;
}

}  // namespace ots

#undef TABLE_NAME
#undef DROP_THIS_TABLE
