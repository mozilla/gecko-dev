// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <jxl/types.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "benchmark/benchmark.h"
#include "lib/jxl/enc_external_image.h"
#include "lib/jxl/image_metadata.h"
#include "tools/no_memory_manager.h"

namespace jxl {
namespace {

#define BM_CHECK(C)          \
  if (!(C)) {                \
    state.SkipWithError(#C); \
    return;                  \
  }

// Encoder case, deinterleaves a buffer.
void BM_EncExternalImage_ConvertImageRGBA(benchmark::State& state) {
  const size_t kNumIter = 5;
  size_t xsize = state.range();
  size_t ysize = state.range();

  ImageMetadata im;
  im.SetAlphaBits(8);
  ImageBundle ib(jpegxl::tools::NoMemoryManager(), &im);

  std::vector<uint8_t> interleaved(xsize * ysize * 4);
  JxlPixelFormat format = {4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};
  for (auto _ : state) {
    (void)_;
    for (size_t i = 0; i < kNumIter; ++i) {
      BM_CHECK(ConvertFromExternal(
          Bytes(interleaved.data(), interleaved.size()), xsize, ysize,
          /*c_current=*/ColorEncoding::SRGB(),
          /*bits_per_sample=*/8, format,
          /*pool=*/nullptr, &ib));
    }
  }

  // Pixels per second.
  state.SetItemsProcessed(kNumIter * state.iterations() * xsize * ysize);
  state.SetBytesProcessed(kNumIter * state.iterations() * interleaved.size());
}

BENCHMARK(BM_EncExternalImage_ConvertImageRGBA)
    ->RangeMultiplier(2)
    ->Range(256, 2048);

}  // namespace
}  // namespace jxl
