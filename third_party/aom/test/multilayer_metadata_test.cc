/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <cmath>

#include "gtest/gtest.h"

#include "examples/multilayer_metadata.h"
#include "test/video_source.h"

namespace libaom_examples {
namespace {

TEST(MultilayerMetadataTest, ParseAlpha) {
  const std::string metadata = R"(

use_case: 1 # global alpha
layers:
  - layer_type: 5 # alpha
    luma_plane_only_flag: 1
    layer_metadata_scope: 2 # global
    alpha:
      alpha_use_idc: 2 # segmentation
      alpha_bit_depth: 8
      alpha_transparent_value: 0
      alpha_opaque_value: 4
      label_type_id:
        - 5
        - 3
        - 9
        - 128
        - 42

  - layer_type: 1 # texture
    luma_plane_only_flag: 0
    layer_metadata_scope: 2 # global
    layer_color_description:
      color_range: 1
      color_primaries: 1
      transfer_characteristics: 13
      matrix_coefficients: 6

    )";
  libaom_test::TempOutFile tmp_file(/*text_mode=*/true);
  fprintf(tmp_file.file(), "%s", metadata.c_str());
  fflush(tmp_file.file());

  MultilayerMetadata multilayer;
  EXPECT_TRUE(parse_multilayer_file(tmp_file.file_name().c_str(), &multilayer));

  EXPECT_EQ(multilayer.use_case, 1);
  ASSERT_EQ(multilayer.layers.size(), 2);
  EXPECT_EQ(multilayer.layers[0].layer_type, 5);
  EXPECT_EQ(multilayer.layers[0].luma_plane_only_flag, 1);
  EXPECT_EQ(multilayer.layers[0].layer_metadata_scope, 2);
  EXPECT_EQ(multilayer.layers[0].global_alpha_info.alpha_use_idc, 2);
  EXPECT_EQ(multilayer.layers[0].global_alpha_info.alpha_bit_depth, 8);
  EXPECT_EQ(multilayer.layers[0].global_alpha_info.alpha_transparent_value, 0);
  EXPECT_EQ(multilayer.layers[0].global_alpha_info.alpha_opaque_value, 4);
  ASSERT_EQ(multilayer.layers[0].global_alpha_info.label_type_id.size(), 5);
  EXPECT_EQ(multilayer.layers[0].global_alpha_info.label_type_id[0], 5);
  EXPECT_EQ(multilayer.layers[0].global_alpha_info.label_type_id[1], 3);
  EXPECT_EQ(multilayer.layers[0].global_alpha_info.label_type_id[2], 9);
  EXPECT_EQ(multilayer.layers[0].global_alpha_info.label_type_id[3], 128);
  EXPECT_EQ(multilayer.layers[0].global_alpha_info.label_type_id[4], 42);
  EXPECT_EQ(multilayer.layers[1].layer_type, 1);
  EXPECT_EQ(multilayer.layers[1].luma_plane_only_flag, 0);
  EXPECT_EQ(multilayer.layers[1].layer_metadata_scope, 2);
  EXPECT_TRUE(multilayer.layers[1].layer_color_description.second);
  EXPECT_EQ(multilayer.layers[1].layer_color_description.first.color_range, 1);
  EXPECT_EQ(multilayer.layers[1].layer_color_description.first.color_primaries,
            1);
  EXPECT_EQ(multilayer.layers[1]
                .layer_color_description.first.transfer_characteristics,
            13);
  EXPECT_EQ(
      multilayer.layers[1].layer_color_description.first.matrix_coefficients,
      6);
}

TEST(MultilayerMetadataTest, ParseDepth) {
  const std::string metadata = R"(
use_case: 2 # global depth
layers:
  - layer_type: 6 # depth
    luma_plane_only_flag: 1
    layer_metadata_scope: 2 # global
    depth:
      z_near: 1.456
      z_far: 9.786
      depth_representation_type: 3
      depth_nonlinear_precision: 8
      depth_nonlinear_representation_model:
         - 12
         - 23
         - 5

  - layer_type: 1 # texture
    luma_plane_only_flag: 0
    layer_metadata_scope: 2 # global
    layer_color_description:
      color_range: 1
      color_primaries: 1
      transfer_characteristics: 13
      matrix_coefficients: 6

    )";
  libaom_test::TempOutFile tmp_file(/*text_mode=*/true);
  fprintf(tmp_file.file(), "%s", metadata.c_str());
  fflush(tmp_file.file());

  MultilayerMetadata multilayer;
  EXPECT_TRUE(parse_multilayer_file(tmp_file.file_name().c_str(), &multilayer));
  EXPECT_EQ(multilayer.use_case, 2);
  ASSERT_EQ(multilayer.layers.size(), 2);
  EXPECT_EQ(multilayer.layers[0].layer_type, 6);
  EXPECT_EQ(multilayer.layers[0].luma_plane_only_flag, 1);
  EXPECT_EQ(multilayer.layers[0].layer_metadata_scope, 2);
  EXPECT_TRUE(multilayer.layers[0].global_depth_info.z_near.second);
  EXPECT_NEAR(depth_representation_element_to_double(
                  multilayer.layers[0].global_depth_info.z_near.first),
              1.456, 0.00001);
  EXPECT_TRUE(multilayer.layers[0].global_depth_info.z_far.second);
  EXPECT_NEAR(depth_representation_element_to_double(
                  multilayer.layers[0].global_depth_info.z_far.first),
              9.786, 0.00001);
  EXPECT_EQ(multilayer.layers[0].global_depth_info.depth_representation_type,
            3);
  EXPECT_EQ(multilayer.layers[0].global_depth_info.depth_nonlinear_precision,
            8);
  ASSERT_EQ(multilayer.layers[0]
                .global_depth_info.depth_nonlinear_representation_model.size(),
            3);
  EXPECT_EQ(multilayer.layers[0]
                .global_depth_info.depth_nonlinear_representation_model[0],
            12);
  EXPECT_EQ(multilayer.layers[0]
                .global_depth_info.depth_nonlinear_representation_model[1],
            23);
  EXPECT_EQ(multilayer.layers[0]
                .global_depth_info.depth_nonlinear_representation_model[2],
            5);
  EXPECT_EQ(multilayer.layers[1].layer_type, 1);
  EXPECT_EQ(multilayer.layers[1].luma_plane_only_flag, 0);
  EXPECT_EQ(multilayer.layers[1].layer_metadata_scope, 2);
  EXPECT_TRUE(multilayer.layers[1].layer_color_description.second);
  EXPECT_EQ(multilayer.layers[1].layer_color_description.first.color_range, 1);
  EXPECT_EQ(multilayer.layers[1].layer_color_description.first.color_primaries,
            1);
  EXPECT_EQ(multilayer.layers[1]
                .layer_color_description.first.transfer_characteristics,
            13);
  EXPECT_EQ(
      multilayer.layers[1].layer_color_description.first.matrix_coefficients,
      6);
}

TEST(MultilayerMetadataTest, ParseInvalid) {
  const std::string metadata = R"(

use_case: 1 # global alpha
layers:
  - layer_type: 5 # alpha
    luma_plane_only_flag: 1
    layer_metadata_scope: 2 # global

  - layer_type: 1 # texture
    luma_plane_only_flag: 0
    layer_metadata_scope: 2 # global

  - layer_type: 6 # depth => bad layer type
    luma_plane_only_flag: 1
    layer_metadata_scope: 2 # global
    )";
  libaom_test::TempOutFile tmp_file(/*text_mode=*/true);
  fprintf(tmp_file.file(), "%s", metadata.c_str());
  fflush(tmp_file.file());

  MultilayerMetadata multilayer;
  // Invalid: has a depth layer even though use_case is alpha
  EXPECT_FALSE(
      parse_multilayer_file(tmp_file.file_name().c_str(), &multilayer));
}

TEST(MultilayerMetadataTest, ParseBadIndent) {
  const std::string metadata = R"(

use_case: 1 # global alpha
layers:
  - layer_type: 5 # alpha
    luma_plane_only_flag: 1
      layer_metadata_scope: 2 # global

  - layer_type: 1 # texture
    luma_plane_only_flag: 0
    layer_metadata_scope: 2 # global
    )";
  libaom_test::TempOutFile tmp_file(/*text_mode=*/true);
  fprintf(tmp_file.file(), "%s", metadata.c_str());
  fflush(tmp_file.file());

  MultilayerMetadata multilayer;
  // Invalid indentation.
  EXPECT_FALSE(
      parse_multilayer_file(tmp_file.file_name().c_str(), &multilayer));
}

TEST(MultilayerMetadataTest, ParseUnknownField) {
  const std::string metadata = R"(

use_case: 1 # global alpha
layers:
  - layer_type: 5 # alpha
    luma_plane_only_flag: 1
    layer_metadata_scope: 2 # global
    foobar: 42

  - layer_type: 1 # texture
    luma_plane_only_flag: 0
    layer_metadata_scope: 2 # global
    )";
  libaom_test::TempOutFile tmp_file(/*text_mode=*/true);
  fprintf(tmp_file.file(), "%s", metadata.c_str());
  fflush(tmp_file.file());

  MultilayerMetadata multilayer;
  // Unkonwn field 'foobar'.
  EXPECT_FALSE(
      parse_multilayer_file(tmp_file.file_name().c_str(), &multilayer));
}

void TestConversion(double v) {
  DepthRepresentationElement e;
  ASSERT_TRUE(double_to_depth_representation_element(v, &e)) << v;
  EXPECT_NEAR(depth_representation_element_to_double(e), v, 0.000000001);
}

TEST(MultilayerMetadataTest, DoubleConversion) {
  TestConversion(0.0);
  TestConversion(1.789456e-5);
  TestConversion(-1.789456e-5);
  TestConversion(42);
  TestConversion(6.7894564456);
  TestConversion(6.7894564456e10);
  TestConversion(-6.7894564456e10);

  DepthRepresentationElement e;
  // Too small.
  ASSERT_FALSE(double_to_depth_representation_element(1e-10, &e));
  // Too big.
  ASSERT_FALSE(double_to_depth_representation_element(1e+30, &e));
}

}  // namespace
}  // namespace libaom_examples
