// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/butteraugli/butteraugli.h"

#include <jxl/memory_manager.h>
#include <jxl/types.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "lib/extras/metrics.h"
#include "lib/extras/packed_image.h"
#include "lib/jxl/base/random.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/enc_external_image.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/test_image.h"
#include "lib/jxl/test_memory_manager.h"
#include "lib/jxl/test_utils.h"
#include "lib/jxl/testing.h"

namespace jxl {
namespace {

using ::jxl::extras::PackedImage;
using ::jxl::extras::PackedPixelFile;
using ::jxl::test::TestImage;

Image3F SinglePixelImage(float red, float green, float blue) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  JXL_TEST_ASSIGN_OR_DIE(Image3F img, Image3F::Create(memory_manager, 1, 1));
  img.PlaneRow(0, 0)[0] = red;
  img.PlaneRow(1, 0)[0] = green;
  img.PlaneRow(2, 0)[0] = blue;
  return img;
}

StatusOr<Image3F> GetColorImage(const PackedPixelFile& ppf) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  Image3F color;
  JXL_ENSURE(!ppf.frames.empty());
  const PackedImage& image = ppf.frames[0].color;
  const JxlPixelFormat& format = image.format;
  const uint8_t* pixels = reinterpret_cast<const uint8_t*>(image.pixels());
  JXL_TEST_ASSIGN_OR_DIE(
      color, Image3F::Create(memory_manager, image.xsize, image.ysize));
  for (size_t c = 0; c < format.num_channels; ++c) {
    JXL_RETURN_IF_ERROR(ConvertFromExternal(
        pixels, image.pixels_size, image.xsize, image.ysize,
        ppf.info.bits_per_sample, format, c, nullptr, &color.Plane(c)));
  }
  return color;
}

void AddUniformNoise(Image3F* img, float d, uint64_t seed) {
  Rng generator(seed);
  for (size_t y = 0; y < img->ysize(); ++y) {
    for (int c = 0; c < 3; ++c) {
      for (size_t x = 0; x < img->xsize(); ++x) {
        img->PlaneRow(c, y)[x] += generator.UniformF(-d, d);
      }
    }
  }
}

void AddEdge(Image3F* img, float d, size_t x0, size_t y0) {
  const size_t h = std::min<size_t>(img->ysize() - y0, 100);
  const size_t w = std::min<size_t>(img->xsize() - x0, 5);
  for (size_t dy = 0; dy < h; ++dy) {
    for (size_t dx = 0; dx < w; ++dx) {
      img->PlaneRow(1, y0 + dy)[x0 + dx] += d;
    }
  }
}

TEST(ButteraugliInPlaceTest, SinglePixel) {
  Image3F rgb0 = SinglePixelImage(0.5f, 0.5f, 0.5f);
  Image3F rgb1 = SinglePixelImage(0.5f, 0.49f, 0.5f);
  ButteraugliParams butteraugli_params;
  ImageF diffmap;
  double diffval;
  EXPECT_TRUE(
      ButteraugliInterface(rgb0, rgb1, butteraugli_params, diffmap, diffval));
  EXPECT_NEAR(diffval, 2.5, 0.5);
  ImageF diffmap2;
  double diffval2;
  EXPECT_TRUE(ButteraugliInterfaceInPlace(std::move(rgb0), std::move(rgb1),
                                          butteraugli_params, diffmap2,
                                          diffval2));
  EXPECT_NEAR(diffval, diffval2, 1e-10);
}

TEST(ButteraugliInPlaceTest, LargeImage) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  const size_t xsize = 1024;
  const size_t ysize = 1024;
  TestImage img;
  ASSERT_TRUE(img.SetDimensions(xsize, ysize));
  JXL_TEST_ASSIGN_OR_DIE(auto frame, img.AddFrame());
  frame.RandomFill(777);
  JXL_TEST_ASSIGN_OR_DIE(Image3F rgb0, GetColorImage(img.ppf()));
  JXL_TEST_ASSIGN_OR_DIE(Image3F rgb1,
                         Image3F::Create(memory_manager, xsize, ysize));
  ASSERT_TRUE(CopyImageTo(rgb0, &rgb1));
  AddUniformNoise(&rgb1, 0.02f, 7777);
  AddEdge(&rgb1, 0.1f, xsize / 2, xsize / 2);
  ButteraugliParams butteraugli_params;
  ImageF diffmap;
  double diffval;
  EXPECT_TRUE(
      ButteraugliInterface(rgb0, rgb1, butteraugli_params, diffmap, diffval));
  double distp = ComputeDistanceP(diffmap, butteraugli_params, 3.0);
  EXPECT_NEAR(diffval, 4.0, 0.5);
  EXPECT_NEAR(distp, 1.5, 0.5);
  ImageF diffmap2;
  double diffval2;
  EXPECT_TRUE(ButteraugliInterfaceInPlace(std::move(rgb0), std::move(rgb1),
                                          butteraugli_params, diffmap2,
                                          diffval2));
  double distp2 = ComputeDistanceP(diffmap2, butteraugli_params, 3.0);
  EXPECT_NEAR(diffval, diffval2, 5e-7);
  EXPECT_NEAR(distp, distp2, 1e-7);
}

}  // namespace
}  // namespace jxl
