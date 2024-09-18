// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <jxl/cms.h>
#include <jxl/memory_manager.h>

#include <cstddef>
#include <utility>

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/matrix_ops.h"
#include "lib/jxl/base/rect.h"
#include "lib/jxl/cms/opsin_params.h"
#include "lib/jxl/dec_xyb.h"
#include "lib/jxl/enc_xyb.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/image_metadata.h"
#include "lib/jxl/opsin_params.h"
#include "lib/jxl/test_memory_manager.h"
#include "lib/jxl/test_utils.h"
#include "lib/jxl/testing.h"

namespace jxl {
namespace {

// Convert a single linear sRGB color to xyb, using the exact image conversion
// procedure that jpeg xl uses.
void LinearSrgbToOpsin(float rgb_r, float rgb_g, float rgb_b,
                       float* JXL_RESTRICT xyb_x, float* JXL_RESTRICT xyb_y,
                       float* JXL_RESTRICT xyb_b) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  JXL_TEST_ASSIGN_OR_DIE(Image3F linear, Image3F::Create(memory_manager, 1, 1));
  linear.PlaneRow(0, 0)[0] = rgb_r;
  linear.PlaneRow(1, 0)[0] = rgb_g;
  linear.PlaneRow(2, 0)[0] = rgb_b;

  ImageMetadata metadata;
  metadata.SetFloat32Samples();
  metadata.color_encoding = ColorEncoding::LinearSRGB();
  ImageBundle ib(memory_manager, &metadata);
  ASSERT_TRUE(ib.SetFromImage(std::move(linear), metadata.color_encoding));
  JXL_TEST_ASSIGN_OR_DIE(Image3F opsin, Image3F::Create(memory_manager, 1, 1));
  (void)ToXYB(ib, /*pool=*/nullptr, &opsin, *JxlGetDefaultCms());

  *xyb_x = opsin.PlaneRow(0, 0)[0];
  *xyb_y = opsin.PlaneRow(1, 0)[0];
  *xyb_b = opsin.PlaneRow(2, 0)[0];
}

// Convert a single XYB color to linear sRGB, using the exact image conversion
// procedure that jpeg xl uses.
void OpsinToLinearSrgb(float xyb_x, float xyb_y, float xyb_b,
                       float* JXL_RESTRICT rgb_r, float* JXL_RESTRICT rgb_g,
                       float* JXL_RESTRICT rgb_b) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  JXL_TEST_ASSIGN_OR_DIE(Image3F opsin, Image3F::Create(memory_manager, 1, 1));
  opsin.PlaneRow(0, 0)[0] = xyb_x;
  opsin.PlaneRow(1, 0)[0] = xyb_y;
  opsin.PlaneRow(2, 0)[0] = xyb_b;
  JXL_TEST_ASSIGN_OR_DIE(Image3F linear, Image3F::Create(memory_manager, 1, 1));
  OpsinParams opsin_params;
  opsin_params.Init(/*intensity_target=*/255.0f);
  ASSERT_TRUE(
      OpsinToLinear(opsin, Rect(opsin), nullptr, &linear, opsin_params));
  *rgb_r = linear.PlaneRow(0, 0)[0];
  *rgb_g = linear.PlaneRow(1, 0)[0];
  *rgb_b = linear.PlaneRow(2, 0)[0];
}

void OpsinRoundtripTestRGB(float r, float g, float b) {
  float xyb_x;
  float xyb_y;
  float xyb_b;
  LinearSrgbToOpsin(r, g, b, &xyb_x, &xyb_y, &xyb_b);
  float r2;
  float g2;
  float b2;
  OpsinToLinearSrgb(xyb_x, xyb_y, xyb_b, &r2, &g2, &b2);
  EXPECT_NEAR(r, r2, 1e-3);
  EXPECT_NEAR(g, g2, 1e-3);
  EXPECT_NEAR(b, b2, 1e-3);
}

TEST(OpsinImageTest, VerifyOpsinAbsorbanceInverseMatrix) {
  Matrix3x3 matrix;  // writable copy
  matrix = GetOpsinAbsorbanceInverseMatrix();
  EXPECT_TRUE(Inv3x3Matrix(matrix));
  for (int j = 0; j < 3; j++) {
    for (int i = 0; i < 3; i++) {
      EXPECT_NEAR(matrix[j][i], jxl::cms::kOpsinAbsorbanceMatrix[j][i], 1e-6);
    }
  }
}

TEST(OpsinImageTest, OpsinRoundtrip) {
  OpsinRoundtripTestRGB(0, 0, 0);
  OpsinRoundtripTestRGB(1. / 255, 1. / 255, 1. / 255);
  OpsinRoundtripTestRGB(128. / 255, 128. / 255, 128. / 255);
  OpsinRoundtripTestRGB(1, 1, 1);

  OpsinRoundtripTestRGB(0, 0, 1. / 255);
  OpsinRoundtripTestRGB(0, 0, 128. / 255);
  OpsinRoundtripTestRGB(0, 0, 1);

  OpsinRoundtripTestRGB(0, 1. / 255, 0);
  OpsinRoundtripTestRGB(0, 128. / 255, 0);
  OpsinRoundtripTestRGB(0, 1, 0);

  OpsinRoundtripTestRGB(1. / 255, 0, 0);
  OpsinRoundtripTestRGB(128. / 255, 0, 0);
  OpsinRoundtripTestRGB(1, 0, 0);
}

TEST(OpsinImageTest, VerifyZero) {
  // Test that black color (zero energy) is 0,0,0 in xyb.
  float x;
  float y;
  float b;
  LinearSrgbToOpsin(0, 0, 0, &x, &y, &b);
  EXPECT_NEAR(0, x, 1e-9);
  EXPECT_NEAR(0, y, 1e-7);
  EXPECT_NEAR(0, b, 1e-7);
}

TEST(OpsinImageTest, VerifyGray) {
  // Test that grayscale colors have a fixed y/b ratio and x==0.
  for (size_t i = 1; i < 255; i++) {
    float x;
    float y;
    float b;
    LinearSrgbToOpsin(i / 255., i / 255., i / 255., &x, &y, &b);
    EXPECT_NEAR(0, x, 1e-6);
    EXPECT_NEAR(jxl::cms::kYToBRatio, b / y, 3e-5);
  }
}

}  // namespace
}  // namespace jxl
