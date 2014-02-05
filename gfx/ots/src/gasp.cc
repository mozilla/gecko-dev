// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gasp.h"

// gasp - Grid-fitting And Scan-conversion Procedure
// http://www.microsoft.com/typography/otspec/gasp.htm

#define TABLE_NAME "gasp"

#define DROP_THIS_TABLE \
  do { \
    delete file->gasp; \
    file->gasp = 0; \
    OTS_FAILURE_MSG("Table discarded"); \
  } while (0)

namespace ots {

bool ots_gasp_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  Buffer table(data, length);

  OpenTypeGASP *gasp = new OpenTypeGASP;
  file->gasp = gasp;

  uint16_t num_ranges = 0;
  if (!table.ReadU16(&gasp->version) ||
      !table.ReadU16(&num_ranges)) {
    return OTS_FAILURE_MSG("Failed to read table header");
  }

  if (gasp->version > 1) {
    // Lots of Linux fonts have bad version numbers...
    OTS_WARNING("bad version: %u", gasp->version);
    DROP_THIS_TABLE;
    return true;
  }

  if (num_ranges == 0) {
    OTS_WARNING("num_ranges is zero");
    DROP_THIS_TABLE;
    return true;
  }

  gasp->gasp_ranges.reserve(num_ranges);
  for (unsigned i = 0; i < num_ranges; ++i) {
    uint16_t max_ppem = 0;
    uint16_t behavior = 0;
    if (!table.ReadU16(&max_ppem) ||
        !table.ReadU16(&behavior)) {
      return OTS_FAILURE_MSG("Failed to read subrange %d", i);
    }
    if ((i > 0) && (gasp->gasp_ranges[i - 1].first >= max_ppem)) {
      // The records in the gaspRange[] array must be sorted in order of
      // increasing rangeMaxPPEM value.
      OTS_WARNING("ranges are not sorted");
      DROP_THIS_TABLE;
      return true;
    }
    if ((i == num_ranges - 1u) &&  // never underflow.
        (max_ppem != 0xffffu)) {
      OTS_WARNING("The last record should be 0xFFFF as a sentinel value "
                  "for rangeMaxPPEM");
      DROP_THIS_TABLE;
      return true;
    }

    if (behavior >> 8) {
      OTS_WARNING("undefined bits are used: %x", behavior);
      // mask undefined bits.
      behavior &= 0x000fu;
    }

    if (gasp->version == 0 && (behavior >> 2) != 0) {
      OTS_WARNING("changed the version number to 1");
      gasp->version = 1;
    }

    gasp->gasp_ranges.push_back(std::make_pair(max_ppem, behavior));
  }

  return true;
}

bool ots_gasp_should_serialise(OpenTypeFile *file) {
  return file->gasp != NULL;
}

bool ots_gasp_serialise(OTSStream *out, OpenTypeFile *file) {
  const OpenTypeGASP *gasp = file->gasp;

  if (!out->WriteU16(gasp->version) ||
      !out->WriteU16(gasp->gasp_ranges.size())) {
    return OTS_FAILURE_MSG("failed to write gasp header");
  }

  for (unsigned i = 0; i < gasp->gasp_ranges.size(); ++i) {
    if (!out->WriteU16(gasp->gasp_ranges[i].first) ||
        !out->WriteU16(gasp->gasp_ranges[i].second)) {
      return OTS_FAILURE_MSG("Failed to write gasp subtable %d", i);
    }
  }

  return true;
}

void ots_gasp_free(OpenTypeFile *file) {
  delete file->gasp;
}

}  // namespace ots
