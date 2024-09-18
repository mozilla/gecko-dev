// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <jxl/cms.h>
#include <jxl/memory_manager.h>

#include <utility>

#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/rect.h"
#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/dec_xyb.h"
#include "lib/jxl/enc_xyb.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/image_test_utils.h"
#include "lib/jxl/test_memory_manager.h"
#include "lib/jxl/test_utils.h"
#include "lib/jxl/testing.h"

namespace jxl {
namespace {

TEST(OpsinInverseTest, LinearInverseInverts) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  JXL_TEST_ASSIGN_OR_DIE(Image3F linear,
                         Image3F::Create(memory_manager, 128, 128));
  RandomFillImage(&linear, 0.0f, 1.0f);

  CodecInOut io{memory_manager};
  io.metadata.m.SetFloat32Samples();
  io.metadata.m.color_encoding = ColorEncoding::LinearSRGB();
  JXL_TEST_ASSIGN_OR_DIE(Image3F linear2,
                         Image3F::Create(memory_manager, 128, 128));
  ASSERT_TRUE(CopyImageTo(linear, &linear2));
  ASSERT_TRUE(
      io.SetFromImage(std::move(linear2), io.metadata.m.color_encoding));
  ThreadPool* null_pool = nullptr;
  JXL_TEST_ASSIGN_OR_DIE(
      Image3F opsin, Image3F::Create(memory_manager, io.xsize(), io.ysize()));
  (void)ToXYB(io.Main(), null_pool, &opsin, *JxlGetDefaultCms());

  OpsinParams opsin_params;
  opsin_params.Init(/*intensity_target=*/255.0f);
  ASSERT_TRUE(OpsinToLinearInplace(&opsin, /*pool=*/nullptr, opsin_params));

  JXL_TEST_ASSERT_OK(VerifyRelativeError(linear, opsin, 3E-3, 2E-4, _));
}

TEST(OpsinInverseTest, YcbCrInverts) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  JXL_TEST_ASSIGN_OR_DIE(Image3F rgb,
                         Image3F::Create(memory_manager, 128, 128));
  RandomFillImage(&rgb, 0.0f, 1.0f);

  ThreadPool* null_pool = nullptr;
  JXL_TEST_ASSIGN_OR_DIE(
      Image3F ycbcr, Image3F::Create(memory_manager, rgb.xsize(), rgb.ysize()));
  EXPECT_TRUE(RgbToYcbcr(rgb.Plane(0), rgb.Plane(1), rgb.Plane(2),
                         &ycbcr.Plane(1), &ycbcr.Plane(0), &ycbcr.Plane(2),
                         null_pool));

  JXL_TEST_ASSIGN_OR_DIE(
      Image3F rgb2, Image3F::Create(memory_manager, rgb.xsize(), rgb.ysize()));
  YcbcrToRgb(ycbcr, &rgb2, Rect(rgb));

  JXL_TEST_ASSERT_OK(VerifyRelativeError(rgb, rgb2, 4E-5, 4E-7, _));
}

}  // namespace
}  // namespace jxl
