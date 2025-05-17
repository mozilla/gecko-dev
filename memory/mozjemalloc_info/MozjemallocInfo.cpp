/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>

#include "mozmemory.h"

/*
 * Print the configured size classes which we can then use to update
 * documentation.
 */
int main() {
  jemalloc_stats_t stats;

  const size_t num_bins = jemalloc_stats_num_bins();
  const size_t MAX_NUM_BINS = 100;
  if (num_bins > MAX_NUM_BINS) {
    fprintf(stderr, "Exceeded maximum number of jemalloc stats bins");
    return 1;
  }
  jemalloc_bin_stats_t bin_stats[MAX_NUM_BINS] = {{0}};
  jemalloc_stats(&stats, bin_stats);

  printf("\n");
  printf("Parameters\n");
  printf("----------\n\n");
  printf("Page size:    %5zu\n", stats.page_size);
  printf("Chunk size:   %5zuKiB\n", stats.chunksize / 1024);
  printf("Quantum:      %5zu\n", stats.quantum);
  printf("Quantum max:  %5zu\n", stats.quantum_max);
  printf("Sub-page max: %5zu\n", stats.page_size / 2);
  printf("Large max:    %5zuKiB\n", stats.large_max / 1024);

  printf("\n");
  printf("Run layout for each bin size\n");
  printf("----------------------------\n\n");
  printf(" Size | Reg per run | Run size | Overhead\n");
  printf("------|-------------|----------|----------\n");
  for (unsigned i = 0; i < num_bins; i++) {
    auto& bin = bin_stats[i];
    if (bin.size) {
      printf("%5zu | %11zu | %5zuKiB | %7.2f%%\n", bin.size,
             bin.regions_per_run, bin.bytes_per_run / 1024,
             float(bin.bytes_per_run - (bin.regions_per_run * bin.size)) *
                 100.0f / float(bin.regions_per_run * bin.size));
    }
  }

  return EXIT_SUCCESS;
}
