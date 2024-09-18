// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <jxl/memory_manager.h>

#include "benchmark/benchmark.h"
#include "lib/jxl/dec_external_image.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_ops.h"
#include "tools/no_memory_manager.h"

namespace jxl {
namespace {

#define QUIT(M)           \
  state.SkipWithError(M); \
  return;

#define BM_CHECK(C) \
  if (!(C)) {       \
    QUIT(#C)        \
  }

// Decoder case, interleaves an internal float image.
void BM_DecExternalImage_ConvertImageRGBA(benchmark::State& state) {
  JxlMemoryManager* memory_manager = jpegxl::tools::NoMemoryManager();
  const size_t kNumIter = 5;
  size_t xsize = state.range();
  size_t ysize = state.range();
  size_t num_channels = 4;

  ImageMetadata im;
  im.SetAlphaBits(8);
  ImageBundle ib(memory_manager, &im);
  JXL_ASSIGN_OR_QUIT(Image3F color,
                     Image3F::Create(memory_manager, xsize, ysize),
                     "Failed to allocate color plane.");
  ZeroFillImage(&color);
  BM_CHECK(ib.SetFromImage(std::move(color), ColorEncoding::SRGB()));
  JXL_ASSIGN_OR_QUIT(ImageF alpha, ImageF::Create(memory_manager, xsize, ysize),
                     "Failed to allocate alpha plane.");
  ZeroFillImage(&alpha);
  BM_CHECK(ib.SetAlpha(std::move(alpha)));

  const size_t bytes_per_row = xsize * num_channels;
  std::vector<uint8_t> interleaved(bytes_per_row * ysize);

  for (auto _ : state) {
    (void)_;
    for (size_t i = 0; i < kNumIter; ++i) {
      BM_CHECK(ConvertToExternal(
          ib,
          /*bits_per_sample=*/8,
          /*float_out=*/false, num_channels, JXL_NATIVE_ENDIAN,
          /*stride*/ bytes_per_row,
          /*thread_pool=*/nullptr, interleaved.data(), interleaved.size(),
          /*out_callback=*/{},
          /*undo_orientation=*/jxl::Orientation::kIdentity));
    }
  }

  // Pixels per second.
  state.SetItemsProcessed(kNumIter * state.iterations() * xsize * ysize);
  state.SetBytesProcessed(kNumIter * state.iterations() * interleaved.size());
}

BENCHMARK(BM_DecExternalImage_ConvertImageRGBA)
    ->RangeMultiplier(2)
    ->Range(256, 2048);

}  // namespace
}  // namespace jxl
