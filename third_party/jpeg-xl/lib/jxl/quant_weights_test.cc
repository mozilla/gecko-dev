// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#include "lib/jxl/quant_weights.h"

#include <jxl/memory_manager.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <hwy/base.h>  // HWY_ALIGN_MAX
#include <hwy/tests/hwy_gtest.h>
#include <iterator>
#include <numeric>
#include <utility>
#include <vector>

#include "lib/jxl/ac_strategy.h"
#include "lib/jxl/base/random.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/dct_for_test.h"
#include "lib/jxl/dec_transforms_testonly.h"
#include "lib/jxl/enc_modular.h"
#include "lib/jxl/enc_params.h"
#include "lib/jxl/enc_quant_weights.h"
#include "lib/jxl/enc_transforms.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/image_metadata.h"
#include "lib/jxl/test_memory_manager.h"
#include "lib/jxl/testing.h"

namespace jxl {
namespace {

// This should have been static assert; not compiling though with C++<17.
TEST(QuantWeightsTest, Invariant) {
  size_t sum = 0;
  ASSERT_EQ(DequantMatrices::required_size_x.size(),
            DequantMatrices::required_size_y.size());
  for (size_t i = 0; i < DequantMatrices::required_size_x.size(); ++i) {
    sum += DequantMatrices::required_size_x[i] *
           DequantMatrices::required_size_y[i];
  }
  ASSERT_EQ(DequantMatrices::kSumRequiredXy, sum);
}

template <typename T>
void CheckSimilar(T a, T b) {
  EXPECT_EQ(a, b);
}
// minimum exponent = -15.
template <>
void CheckSimilar(float a, float b) {
  float m = std::max(std::abs(a), std::abs(b));
  // 10 bits of precision are used in the format. Relative error should be
  // below 2^-10.
  EXPECT_LE(std::abs(a - b), m / 1024.0f) << "a: " << a << " b: " << b;
}

TEST(QuantWeightsTest, DC) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  DequantMatrices mat;
  float dc_quant[3] = {1e+5, 1e+3, 1e+1};
  ASSERT_TRUE(DequantMatricesSetCustomDC(memory_manager, &mat, dc_quant));
  for (size_t c = 0; c < 3; c++) {
    CheckSimilar(mat.InvDCQuant(c), dc_quant[c]);
  }
}

void RoundtripMatrices(const std::vector<QuantEncoding>& encodings) {
  ASSERT_TRUE(encodings.size() == kNumQuantTables);
  DequantMatrices mat;
  CodecMetadata metadata;
  FrameHeader frame_header(&metadata);
  JXL_ASSIGN_OR_QUIT(
      ModularFrameEncoder encoder,
      ModularFrameEncoder::Create(jxl::test::MemoryManager(), frame_header,
                                  CompressParams{}, false),
      "Failed to create ModularFrameEncoder.");
  ASSERT_TRUE(DequantMatricesSetCustom(&mat, encodings, &encoder));
  const std::vector<QuantEncoding>& encodings_dec = mat.encodings();
  for (size_t i = 0; i < encodings.size(); i++) {
    const QuantEncoding& e = encodings[i];
    const QuantEncoding& d = encodings_dec[i];
    // Check values roundtripped correctly.
    EXPECT_EQ(e.mode, d.mode);
    EXPECT_EQ(e.predefined, d.predefined);
    EXPECT_EQ(e.source, d.source);

    EXPECT_EQ(static_cast<uint64_t>(e.dct_params.num_distance_bands),
              static_cast<uint64_t>(d.dct_params.num_distance_bands));
    for (size_t c = 0; c < 3; c++) {
      for (size_t j = 0; j < DctQuantWeightParams::kMaxDistanceBands; j++) {
        CheckSimilar(e.dct_params.distance_bands[c][j],
                     d.dct_params.distance_bands[c][j]);
      }
    }

    if (e.mode == QuantEncoding::kQuantModeRAW) {
      EXPECT_FALSE(!e.qraw.qtable);
      EXPECT_FALSE(!d.qraw.qtable);
      EXPECT_EQ(e.qraw.qtable->size(), d.qraw.qtable->size());
      for (size_t j = 0; j < e.qraw.qtable->size(); j++) {
        EXPECT_EQ(e.qraw.qtable->at(j), d.qraw.qtable->at(j));
      }
      EXPECT_NEAR(e.qraw.qtable_den, d.qraw.qtable_den, 1e-7f);
    } else {
      // modes different than kQuantModeRAW use one of the other fields used
      // here, which all happen to be arrays of floats.
      for (size_t c = 0; c < 3; c++) {
        for (size_t j = 0; j < 3; j++) {
          CheckSimilar(e.idweights[c][j], d.idweights[c][j]);
        }
        for (size_t j = 0; j < 6; j++) {
          CheckSimilar(e.dct2weights[c][j], d.dct2weights[c][j]);
        }
        for (size_t j = 0; j < 2; j++) {
          CheckSimilar(e.dct4multipliers[c][j], d.dct4multipliers[c][j]);
        }
        CheckSimilar(e.dct4x8multipliers[c], d.dct4x8multipliers[c]);
        for (size_t j = 0; j < 9; j++) {
          CheckSimilar(e.afv_weights[c][j], d.afv_weights[c][j]);
        }
        for (size_t j = 0; j < DctQuantWeightParams::kMaxDistanceBands; j++) {
          CheckSimilar(e.dct_params_afv_4x4.distance_bands[c][j],
                       d.dct_params_afv_4x4.distance_bands[c][j]);
        }
      }
    }
  }
}

TEST(QuantWeightsTest, AllDefault) {
  std::vector<QuantEncoding> encodings(kNumQuantTables,
                                       QuantEncoding::Library<0>());
  RoundtripMatrices(encodings);
}

void TestSingleQuantMatrix(QuantTable kind) {
  std::vector<QuantEncoding> encodings(kNumQuantTables,
                                       QuantEncoding::Library<0>());
  size_t quant_table_idx = static_cast<size_t>(kind);
  encodings[quant_table_idx] = DequantMatrices::Library()[quant_table_idx];
  RoundtripMatrices(encodings);
}

// Ensure we can reasonably represent default quant tables.
TEST(QuantWeightsTest, DCT) { TestSingleQuantMatrix(QuantTable::DCT); }
TEST(QuantWeightsTest, IDENTITY) {
  TestSingleQuantMatrix(QuantTable::IDENTITY);
}
TEST(QuantWeightsTest, DCT2X2) { TestSingleQuantMatrix(QuantTable::DCT2X2); }
TEST(QuantWeightsTest, DCT4X4) { TestSingleQuantMatrix(QuantTable::DCT4X4); }
TEST(QuantWeightsTest, DCT16X16) {
  TestSingleQuantMatrix(QuantTable::DCT16X16);
}
TEST(QuantWeightsTest, DCT32X32) {
  TestSingleQuantMatrix(QuantTable::DCT32X32);
}
TEST(QuantWeightsTest, DCT8X16) { TestSingleQuantMatrix(QuantTable::DCT8X16); }
TEST(QuantWeightsTest, DCT8X32) { TestSingleQuantMatrix(QuantTable::DCT8X32); }
TEST(QuantWeightsTest, DCT16X32) {
  TestSingleQuantMatrix(QuantTable::DCT16X32);
}
TEST(QuantWeightsTest, DCT4X8) { TestSingleQuantMatrix(QuantTable::DCT4X8); }
TEST(QuantWeightsTest, AFV0) { TestSingleQuantMatrix(QuantTable::AFV0); }
TEST(QuantWeightsTest, RAW) {
  std::vector<QuantEncoding> encodings(kNumQuantTables,
                                       QuantEncoding::Library<0>());
  std::vector<int> matrix(3 * 32 * 32);
  Rng rng(0);
  for (int& v : matrix) v = rng.UniformI(1, 256);
  QuantTable quant_table =
      kAcStrategyToQuantTableMap[static_cast<size_t>(AcStrategyType::DCT32X32)];
  encodings[static_cast<size_t>(quant_table)] =
      QuantEncoding::RAW(std::move(matrix), 2);
  RoundtripMatrices(encodings);
}

class QuantWeightsTargetTest : public hwy::TestWithParamTarget {};
HWY_TARGET_INSTANTIATE_TEST_SUITE_P(QuantWeightsTargetTest);

TEST_P(QuantWeightsTargetTest, DCTUniform) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  constexpr float kUniformQuant = 4;
  float weights[3][2] = {{1.0f / kUniformQuant, 0},
                         {1.0f / kUniformQuant, 0},
                         {1.0f / kUniformQuant, 0}};
  DctQuantWeightParams dct_params(weights);
  std::vector<QuantEncoding> encodings(kNumQuantTables,
                                       QuantEncoding::DCT(dct_params));
  DequantMatrices dequant_matrices;
  CodecMetadata metadata;
  FrameHeader frame_header(&metadata);
  JXL_ASSIGN_OR_QUIT(
      ModularFrameEncoder encoder,
      ModularFrameEncoder::Create(jxl::test::MemoryManager(), frame_header,
                                  CompressParams{}, false),
      "Failed to create ModularFrameEncoder.");
  ASSERT_TRUE(DequantMatricesSetCustom(&dequant_matrices, encodings, &encoder));
  ASSERT_TRUE(dequant_matrices.EnsureComputed(memory_manager, ~0u));

  const float dc_quant[3] = {1.0f / kUniformQuant, 1.0f / kUniformQuant,
                             1.0f / kUniformQuant};
  ASSERT_TRUE(
      DequantMatricesSetCustomDC(memory_manager, &dequant_matrices, dc_quant));

  HWY_ALIGN_MAX float scratch_space[16 * 16 * 5];

  // DCT8
  {
    HWY_ALIGN_MAX float pixels[64];
    std::iota(std::begin(pixels), std::end(pixels), 0);
    HWY_ALIGN_MAX float coeffs[64];
    const AcStrategyType dct = AcStrategyType::DCT;
    TransformFromPixels(dct, pixels, 8, coeffs, scratch_space);
    HWY_ALIGN_MAX double slow_coeffs[64];
    for (size_t i = 0; i < 64; i++) slow_coeffs[i] = pixels[i];
    DCTSlow<8>(slow_coeffs);

    for (size_t i = 0; i < 64; i++) {
      // DCTSlow doesn't multiply/divide by 1/N, so we do it manually.
      slow_coeffs[i] = roundf(slow_coeffs[i] / kUniformQuant) * kUniformQuant;
      coeffs[i] = roundf(coeffs[i] / dequant_matrices.Matrix(dct, 0)[i]) *
                  dequant_matrices.Matrix(dct, 0)[i];
    }
    IDCTSlow<8>(slow_coeffs);
    TransformToPixels(dct, coeffs, pixels, 8, scratch_space);
    for (size_t i = 0; i < 64; i++) {
      EXPECT_NEAR(pixels[i], slow_coeffs[i], 1e-4);
    }
  }

  // DCT16
  {
    HWY_ALIGN_MAX float pixels[64 * 4];
    std::iota(std::begin(pixels), std::end(pixels), 0);
    HWY_ALIGN_MAX float coeffs[64 * 4];
    const AcStrategyType dct = AcStrategyType::DCT16X16;
    TransformFromPixels(dct, pixels, 16, coeffs, scratch_space);
    HWY_ALIGN_MAX double slow_coeffs[64 * 4];
    for (size_t i = 0; i < 64 * 4; i++) slow_coeffs[i] = pixels[i];
    DCTSlow<16>(slow_coeffs);

    for (size_t i = 0; i < 64 * 4; i++) {
      slow_coeffs[i] = roundf(slow_coeffs[i] / kUniformQuant) * kUniformQuant;
      coeffs[i] = roundf(coeffs[i] / dequant_matrices.Matrix(dct, 0)[i]) *
                  dequant_matrices.Matrix(dct, 0)[i];
    }

    IDCTSlow<16>(slow_coeffs);
    TransformToPixels(dct, coeffs, pixels, 16, scratch_space);
    for (size_t i = 0; i < 64 * 4; i++) {
      EXPECT_NEAR(pixels[i], slow_coeffs[i], 1e-4);
    }
  }

  // Check that all matrices have the same DC quantization, i.e. that they all
  // have the same scaling.
  for (size_t i = 0; i < AcStrategy::kNumValidStrategies; i++) {
    AcStrategyType kind = static_cast<AcStrategyType>(i);
    EXPECT_NEAR(dequant_matrices.Matrix(kind, 0)[0], kUniformQuant, 1e-6);
  }
}

}  // namespace
}  // namespace jxl
