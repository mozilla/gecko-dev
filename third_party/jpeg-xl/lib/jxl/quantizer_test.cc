// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/quantizer.h"

#include <jxl/memory_manager.h>

#include <cstddef>
#include <cstdint>

#include "lib/jxl/dec_bit_reader.h"
#include "lib/jxl/enc_aux_out.h"
#include "lib/jxl/enc_bit_writer.h"
#include "lib/jxl/enc_fields.h"
#include "lib/jxl/fields.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_test_utils.h"
#include "lib/jxl/quant_weights.h"
#include "lib/jxl/test_memory_manager.h"
#include "lib/jxl/test_utils.h"
#include "lib/jxl/testing.h"

namespace jxl {
namespace {

void TestEquivalence(int qxsize, int qysize, const Quantizer& quantizer1,
                     const Quantizer& quantizer2) {
  ASSERT_NEAR(quantizer1.inv_quant_dc(), quantizer2.inv_quant_dc(), 1e-7);
}

TEST(QuantizerTest, QuantizerParams) {
  for (uint32_t i = 1; i < 10000; ++i) {
    QuantizerParams p;
    p.global_scale = i;
    size_t extension_bits = 0;
    size_t total_bits = 0;
    EXPECT_TRUE(Bundle::CanEncode(p, &extension_bits, &total_bits));
    EXPECT_EQ(0u, extension_bits);
    EXPECT_GE(total_bits, 4u);
  }
}

TEST(QuantizerTest, BitStreamRoundtripSameQuant) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  const int qxsize = 8;
  const int qysize = 8;
  DequantMatrices dequant;
  Quantizer quantizer1(dequant);
  JXL_TEST_ASSIGN_OR_DIE(
      ImageI raw_quant_field,
      ImageI::Create(jxl::test::MemoryManager(), qxsize, qysize));
  quantizer1.SetQuant(0.17f, 0.17f, &raw_quant_field);
  BitWriter writer{memory_manager};
  QuantizerParams params = quantizer1.GetParams();
  EXPECT_TRUE(
      WriteQuantizerParams(params, &writer, LayerType::Header, nullptr));
  writer.ZeroPadToByte();
  const size_t bits_written = writer.BitsWritten();
  Quantizer quantizer2(dequant);
  BitReader reader(writer.GetSpan());
  EXPECT_TRUE(quantizer2.Decode(&reader));
  EXPECT_TRUE(reader.JumpToByteBoundary());
  EXPECT_EQ(reader.TotalBitsConsumed(), bits_written);
  EXPECT_TRUE(reader.Close());
  TestEquivalence(qxsize, qysize, quantizer1, quantizer2);
}

TEST(QuantizerTest, BitStreamRoundtripRandomQuant) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  const int qxsize = 8;
  const int qysize = 8;
  DequantMatrices dequant;
  Quantizer quantizer1(dequant);
  JXL_TEST_ASSIGN_OR_DIE(ImageI raw_quant_field,
                         ImageI::Create(memory_manager, qxsize, qysize));
  quantizer1.SetQuant(0.17f, 0.17f, &raw_quant_field);
  float quant_dc = 0.17f;
  JXL_TEST_ASSIGN_OR_DIE(ImageF qf,
                         ImageF::Create(memory_manager, qxsize, qysize));
  RandomFillImage(&qf, 0.0f, 1.0f);
  ASSERT_TRUE(quantizer1.SetQuantField(quant_dc, qf, &raw_quant_field));
  BitWriter writer{memory_manager};
  QuantizerParams params = quantizer1.GetParams();
  EXPECT_TRUE(
      WriteQuantizerParams(params, &writer, LayerType::Header, nullptr));
  writer.ZeroPadToByte();
  const size_t bits_written = writer.BitsWritten();
  Quantizer quantizer2(dequant);
  BitReader reader(writer.GetSpan());
  EXPECT_TRUE(quantizer2.Decode(&reader));
  EXPECT_TRUE(reader.JumpToByteBoundary());
  EXPECT_EQ(reader.TotalBitsConsumed(), bits_written);
  EXPECT_TRUE(reader.Close());
  TestEquivalence(qxsize, qysize, quantizer1, quantizer2);
}
}  // namespace
}  // namespace jxl
