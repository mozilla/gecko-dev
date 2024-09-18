// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/color_encoding_internal.h"

#include <jxl/color_encoding.h>

#include <cstdlib>  // rand

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/cms/color_encoding_cms.h"
#include "lib/jxl/test_utils.h"
#include "lib/jxl/testing.h"

namespace jxl {
namespace {

using ::jxl::cms::ColorEncoding;

TEST(ColorEncodingTest, RoundTripAll) {
  for (const test::ColorEncodingDescriptor& cdesc : test::AllEncodings()) {
    ColorEncoding c_original = test::ColorEncodingFromDescriptor(cdesc).View();
    // Verify Set(Get) yields the same white point/primaries/gamma.
    {
      ColorEncoding c;
      EXPECT_TRUE(c.SetWhitePoint(c_original.GetWhitePoint()));
      EXPECT_EQ(c_original.white_point, c.white_point);
    }
    {
      ColorEncoding c;
      PrimariesCIExy p;
      ASSERT_TRUE(c_original.GetPrimaries(p));
      EXPECT_TRUE(c.SetPrimaries(p));
      EXPECT_EQ(c_original.primaries, c.primaries);
    }
    if (c_original.tf.have_gamma) {
      ColorEncoding c;
      EXPECT_TRUE(c.tf.SetGamma(c_original.tf.GetGamma()));
      EXPECT_TRUE(c_original.tf.IsSame(c.tf));
    }
  }
}

TEST(ColorEncodingTest, CustomWhitePoint) {
  ColorEncoding c;
  // Nonsensical values
  CIExy xy_in;
  xy_in.x = 0.8;
  xy_in.y = 0.01;
  EXPECT_TRUE(c.SetWhitePoint(xy_in));
  const CIExy xy = c.GetWhitePoint();

  ColorEncoding c2;
  EXPECT_TRUE(c2.SetWhitePoint(xy));
  EXPECT_TRUE(c.SameColorSpace(c2));
}

TEST(ColorEncodingTest, CustomPrimaries) {
  ColorEncoding c;
  PrimariesCIExy xy_in;
  // Nonsensical values
  xy_in.r.x = -0.01;
  xy_in.r.y = 0.2;
  xy_in.g.x = 0.4;
  xy_in.g.y = 0.401;
  xy_in.b.x = 1.1;
  xy_in.b.y = -1.2;
  EXPECT_TRUE(c.SetPrimaries(xy_in));
  PrimariesCIExy xy;
  ASSERT_TRUE(c.GetPrimaries(xy));

  ColorEncoding c2;
  EXPECT_TRUE(c2.SetPrimaries(xy));
  EXPECT_TRUE(c.SameColorSpace(c2));
}

TEST(ColorEncodingTest, CustomGamma) {
  ColorEncoding c;
  if (!JXL_CRASH_ON_ERROR) {
    EXPECT_FALSE(c.tf.SetGamma(0.0));
    EXPECT_FALSE(c.tf.SetGamma(-1E-6));
    EXPECT_FALSE(c.tf.SetGamma(1.001));
  }

  EXPECT_TRUE(c.tf.SetGamma(1.0));
  EXPECT_FALSE(c.tf.have_gamma);
  EXPECT_TRUE(c.tf.IsLinear());

  EXPECT_TRUE(c.tf.SetGamma(0.123));
  EXPECT_TRUE(c.tf.have_gamma);
  const double gamma = c.tf.GetGamma();

  ColorEncoding c2;
  EXPECT_TRUE(c2.tf.SetGamma(gamma));
  EXPECT_TRUE(c.SameColorEncoding(c2));
  EXPECT_TRUE(c2.tf.have_gamma);
}

TEST(ColorEncodingTest, InternalExternalConversion) {
  ColorEncoding source_internal;
  ColorEncoding destination_internal;

  const auto rand_float = []() {
    return (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 0.5) +
           0.25;
  };

  for (int i = 0; i < 100; i++) {
    source_internal.color_space = static_cast<ColorSpace>(rand() % 4);
    CIExy wp;
    wp.x = rand_float();
    wp.y = rand_float();
    EXPECT_TRUE(source_internal.SetWhitePoint(wp));
    if (source_internal.HasPrimaries()) {
      PrimariesCIExy primaries;
      primaries.r.x = rand_float();
      primaries.r.y = rand_float();
      primaries.g.x = rand_float();
      primaries.g.y = rand_float();
      primaries.b.x = rand_float();
      primaries.b.y = rand_float();
      EXPECT_TRUE(source_internal.SetPrimaries(primaries));
    }
    jxl::cms::CustomTransferFunction tf;
    EXPECT_TRUE(tf.SetGamma(rand_float()));
    source_internal.tf = tf;
    source_internal.rendering_intent = static_cast<RenderingIntent>(rand() % 4);

    JxlColorEncoding external = source_internal.ToExternal();
    EXPECT_TRUE(destination_internal.FromExternal(external));

    EXPECT_EQ(source_internal.color_space, destination_internal.color_space);
    EXPECT_EQ(source_internal.white_point, destination_internal.white_point);
    CIExy src_wp = source_internal.GetWhitePoint();
    CIExy dst_wp = destination_internal.GetWhitePoint();
    EXPECT_EQ(src_wp.x, dst_wp.x);
    EXPECT_EQ(src_wp.y, dst_wp.y);
    if (source_internal.HasPrimaries()) {
      PrimariesCIExy src_p;
      ASSERT_TRUE(source_internal.GetPrimaries(src_p));
      PrimariesCIExy dst_p;
      ASSERT_TRUE(destination_internal.GetPrimaries(dst_p));
      EXPECT_EQ(src_p.r.x, dst_p.r.x);
      EXPECT_EQ(src_p.r.y, dst_p.r.y);
      EXPECT_EQ(src_p.g.x, dst_p.g.x);
      EXPECT_EQ(src_p.g.y, dst_p.g.y);
      EXPECT_EQ(src_p.b.x, dst_p.b.x);
      EXPECT_EQ(src_p.b.y, dst_p.b.y);
    }
    EXPECT_EQ(source_internal.tf.have_gamma,
              destination_internal.tf.have_gamma);
    if (source_internal.tf.have_gamma) {
      EXPECT_EQ(source_internal.tf.GetGamma(),
                destination_internal.tf.GetGamma());
    } else {
      EXPECT_EQ(source_internal.tf.GetTransferFunction(),
                destination_internal.tf.GetTransferFunction());
    }
    EXPECT_EQ(source_internal.rendering_intent,
              destination_internal.rendering_intent);
  }
}

}  // namespace
}  // namespace jxl
