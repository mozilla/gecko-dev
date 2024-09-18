// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <jxl/cms.h>
#include <jxl/cms_interface.h>
#include <jxl/codestream_header.h>
#include <jxl/color_encoding.h>
#include <jxl/decode.h>
#include <jxl/decode_cxx.h>
#include <jxl/encode.h>
#include <jxl/encode_cxx.h>
#include <jxl/memory_manager.h>
#include <jxl/types.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <ostream>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "lib/extras/codec.h"
#include "lib/extras/dec/jxl.h"
#include "lib/extras/metrics.h"
#include "lib/extras/packed_image.h"
#include "lib/jxl/base/byte_order.h"
#include "lib/jxl/base/c_callback_support.h"
#include "lib/jxl/base/override.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/common.h"  // JXL_HIGH_PRECISION
#include "lib/jxl/enc_params.h"
#include "lib/jxl/encode_internal.h"
#include "lib/jxl/modular/options.h"
#include "lib/jxl/test_image.h"
#include "lib/jxl/test_memory_manager.h"
#include "lib/jxl/test_utils.h"
#include "lib/jxl/testing.h"

namespace {
bool SameDecodedPixels(const std::vector<uint8_t>& compressed0,
                       const std::vector<uint8_t>& compressed1) {
  jxl::extras::JXLDecompressParams dparams;
  dparams.accepted_formats = {
      {3, JXL_TYPE_UINT16, JXL_LITTLE_ENDIAN, 0},
      {4, JXL_TYPE_UINT16, JXL_LITTLE_ENDIAN, 0},
  };
  jxl::extras::PackedPixelFile ppf0;
  EXPECT_TRUE(DecodeImageJXL(compressed0.data(), compressed0.size(), dparams,
                             nullptr, &ppf0, nullptr));
  jxl::extras::PackedPixelFile ppf1;
  EXPECT_TRUE(DecodeImageJXL(compressed1.data(), compressed1.size(), dparams,
                             nullptr, &ppf1, nullptr));
  return jxl::test::SamePixels(ppf0, ppf1);
}
}  // namespace

TEST(EncodeTest, AddFrameAfterCloseInputTest) {
  JxlEncoderPtr enc = JxlEncoderMake(nullptr);
  EXPECT_NE(nullptr, enc.get());

  JxlEncoderCloseInput(enc.get());

  size_t xsize = 64;
  size_t ysize = 64;
  JxlPixelFormat pixel_format = {4, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);

  jxl::CodecInOut input_io =
      jxl::test::SomeTestImageToCodecInOut(pixels, 4, xsize, ysize);

  JxlBasicInfo basic_info;
  jxl::test::JxlBasicInfoSetFromPixelFormat(&basic_info, &pixel_format);
  basic_info.xsize = xsize;
  basic_info.ysize = ysize;
  basic_info.uses_original_profile = 0;
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetCodestreamLevel(enc.get(), 10));
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetBasicInfo(enc.get(), &basic_info));
  JxlColorEncoding color_encoding;
  JXL_BOOL is_gray = TO_JXL_BOOL(pixel_format.num_channels < 3);
  JxlColorEncodingSetToSRGB(&color_encoding, is_gray);
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderSetColorEncoding(enc.get(), &color_encoding));
  JxlEncoderFrameSettings* frame_settings =
      JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
  EXPECT_EQ(JXL_ENC_ERROR,
            JxlEncoderAddImageFrame(frame_settings, &pixel_format,
                                    pixels.data(), pixels.size()));
}

TEST(EncodeTest, AddJPEGAfterCloseTest) {
  JxlEncoderPtr enc = JxlEncoderMake(nullptr);
  EXPECT_NE(nullptr, enc.get());

  JxlEncoderCloseInput(enc.get());

  const std::string jpeg_path = "jxl/flower/flower.png.im_q85_420.jpg";
  const std::vector<uint8_t> orig = jxl::test::ReadTestData(jpeg_path);

  JxlEncoderFrameSettings* frame_settings =
      JxlEncoderFrameSettingsCreate(enc.get(), nullptr);

  EXPECT_EQ(JXL_ENC_ERROR,
            JxlEncoderAddJPEGFrame(frame_settings, orig.data(), orig.size()));
}

TEST(EncodeTest, AddFrameBeforeBasicInfoTest) {
  JxlEncoderPtr enc = JxlEncoderMake(nullptr);
  EXPECT_NE(nullptr, enc.get());

  size_t xsize = 64;
  size_t ysize = 64;
  JxlPixelFormat pixel_format = {4, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);

  jxl::CodecInOut input_io =
      jxl::test::SomeTestImageToCodecInOut(pixels, 4, xsize, ysize);

  JxlColorEncoding color_encoding;
  JXL_BOOL is_gray = TO_JXL_BOOL(pixel_format.num_channels < 3);
  JxlColorEncodingSetToSRGB(&color_encoding, is_gray);
  EXPECT_EQ(JXL_ENC_ERROR,
            JxlEncoderSetColorEncoding(enc.get(), &color_encoding));
  JxlEncoderFrameSettings* frame_settings =
      JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
  EXPECT_EQ(JXL_ENC_ERROR,
            JxlEncoderAddImageFrame(frame_settings, &pixel_format,
                                    pixels.data(), pixels.size()));
}

TEST(EncodeTest, DefaultAllocTest) {
  JxlEncoder* enc = JxlEncoderCreate(nullptr);
  EXPECT_NE(nullptr, enc);
  JxlEncoderDestroy(enc);
}

TEST(EncodeTest, CustomAllocTest) {
  struct CalledCounters {
    int allocs = 0;
    int frees = 0;
  } counters;

  JxlMemoryManager mm;
  mm.opaque = &counters;
  mm.alloc = [](void* opaque, size_t size) {
    reinterpret_cast<CalledCounters*>(opaque)->allocs++;
    return malloc(size);
  };
  mm.free = [](void* opaque, void* address) {
    reinterpret_cast<CalledCounters*>(opaque)->frees++;
    free(address);
  };

  {
    JxlEncoderPtr enc = JxlEncoderMake(&mm);
    EXPECT_NE(nullptr, enc.get());
    EXPECT_LE(1, counters.allocs);
    EXPECT_EQ(0, counters.frees);
  }
  EXPECT_LE(1, counters.frees);
}

TEST(EncodeTest, DefaultParallelRunnerTest) {
  JxlEncoderPtr enc = JxlEncoderMake(nullptr);
  EXPECT_NE(nullptr, enc.get());
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderSetParallelRunner(enc.get(), nullptr, nullptr));
}

void VerifyFrameEncoding(size_t xsize, size_t ysize, JxlEncoder* enc,
                         const JxlEncoderFrameSettings* frame_settings,
                         size_t max_compressed_size,
                         bool lossy_use_original_profile) {
  JxlPixelFormat pixel_format = {4, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);

  jxl::CodecInOut input_io =
      jxl::test::SomeTestImageToCodecInOut(pixels, 4, xsize, ysize);

  JxlBasicInfo basic_info;
  jxl::test::JxlBasicInfoSetFromPixelFormat(&basic_info, &pixel_format);
  basic_info.xsize = xsize;
  basic_info.ysize = ysize;
  if (frame_settings->values.lossless || lossy_use_original_profile) {
    basic_info.uses_original_profile = JXL_TRUE;
  } else {
    basic_info.uses_original_profile = JXL_FALSE;
  }
  // 16-bit alpha means this requires level 10
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetCodestreamLevel(enc, 10));
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetBasicInfo(enc, &basic_info));
  JxlColorEncoding color_encoding;
  JxlColorEncodingSetToSRGB(&color_encoding, JXL_TRUE);
  EXPECT_EQ(JXL_ENC_ERROR, JxlEncoderSetColorEncoding(enc, &color_encoding));
  JxlColorEncodingSetToSRGB(&color_encoding, JXL_FALSE);
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetColorEncoding(enc, &color_encoding));
  pixel_format.num_channels = 1;
  EXPECT_EQ(JXL_ENC_ERROR,
            JxlEncoderAddImageFrame(frame_settings, &pixel_format,
                                    pixels.data(), pixels.size()));
  pixel_format.num_channels = 4;
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderAddImageFrame(frame_settings, &pixel_format,
                                    pixels.data(), pixels.size()));
  JxlEncoderCloseInput(enc);

  std::vector<uint8_t> compressed = std::vector<uint8_t>(64);
  uint8_t* next_out = compressed.data();
  size_t avail_out = compressed.size() - (next_out - compressed.data());
  JxlEncoderStatus process_result = JXL_ENC_NEED_MORE_OUTPUT;
  while (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
    process_result = JxlEncoderProcessOutput(enc, &next_out, &avail_out);
    if (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
      size_t offset = next_out - compressed.data();
      compressed.resize(compressed.size() * 2);
      next_out = compressed.data() + offset;
      avail_out = compressed.size() - offset;
    }
  }
  compressed.resize(next_out - compressed.data());
  EXPECT_LE(compressed.size(), max_compressed_size);
  EXPECT_EQ(JXL_ENC_SUCCESS, process_result);
  jxl::CodecInOut decoded_io{jxl::test::MemoryManager()};
  EXPECT_TRUE(jxl::test::DecodeFile(
      {}, jxl::Bytes(compressed.data(), compressed.size()), &decoded_io));

  static constexpr double kMaxButteraugli =
#if JXL_HIGH_PRECISION
      3.2;
#else
      8.7;
#endif
  EXPECT_LE(
      ComputeDistance2(input_io.Main(), decoded_io.Main(), *JxlGetDefaultCms()),
      kMaxButteraugli);
}

void VerifyFrameEncoding(JxlEncoder* enc,
                         const JxlEncoderFrameSettings* frame_settings) {
  VerifyFrameEncoding(63, 129, enc, frame_settings, 27000,
                      /*lossy_use_original_profile=*/false);
}

TEST(EncodeTest, FrameEncodingTest) {
  JxlEncoderPtr enc = JxlEncoderMake(nullptr);
  EXPECT_NE(nullptr, enc.get());
  VerifyFrameEncoding(enc.get(),
                      JxlEncoderFrameSettingsCreate(enc.get(), nullptr));
}

TEST(EncodeTest, EncoderResetTest) {
  JxlEncoderPtr enc = JxlEncoderMake(nullptr);
  EXPECT_NE(nullptr, enc.get());
  VerifyFrameEncoding(50, 200, enc.get(),
                      JxlEncoderFrameSettingsCreate(enc.get(), nullptr), 4599,
                      false);
  // Encoder should become reusable for a new image from scratch after using
  // reset.
  JxlEncoderReset(enc.get());
  VerifyFrameEncoding(157, 77, enc.get(),
                      JxlEncoderFrameSettingsCreate(enc.get(), nullptr), 2300,
                      false);
}

TEST(EncodeTest, CmsTest) {
  JxlEncoderPtr enc = JxlEncoderMake(nullptr);
  EXPECT_NE(nullptr, enc.get());
  bool cms_called = false;
  JxlCmsInterface cms = *JxlGetDefaultCms();
  struct InitData {
    void* original_init_data;
    jpegxl_cms_init_func original_init;
    bool* cms_called;
  };
  InitData init_data = {/*original_init_data=*/cms.init_data,
                        /*original_init=*/cms.init,
                        /*cms_called=*/&cms_called};
  cms.init_data = &init_data;
  cms.init = +[](void* raw_init_data, size_t num_threads,
                 size_t pixels_per_thread, const JxlColorProfile* input_profile,
                 const JxlColorProfile* output_profile,
                 float intensity_target) {
    const InitData* init_data = static_cast<const InitData*>(raw_init_data);
    *init_data->cms_called = true;
    return init_data->original_init(init_data->original_init_data, num_threads,
                                    pixels_per_thread, input_profile,
                                    output_profile, intensity_target);
  };
  JxlEncoderSetCms(enc.get(), cms);
  JxlEncoderFrameSettings* frame_settings =
      JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
  JxlEncoderSetFrameLossless(frame_settings, JXL_FALSE);
  ASSERT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderFrameSettingsSetOption(frame_settings,
                                             JXL_ENC_FRAME_SETTING_EFFORT, 8));
  VerifyFrameEncoding(enc.get(), frame_settings);
  EXPECT_TRUE(cms_called);
}

TEST(EncodeTest, FrameSettingsTest) {
  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    EXPECT_NE(nullptr, enc.get());
    JxlEncoderFrameSettings* frame_settings =
        JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderFrameSettingsSetOption(
                  frame_settings, JXL_ENC_FRAME_SETTING_EFFORT, 5));
    VerifyFrameEncoding(enc.get(), frame_settings);
    EXPECT_EQ(jxl::SpeedTier::kHare, enc->last_used_cparams.speed_tier);
  }

  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    EXPECT_NE(nullptr, enc.get());
    JxlEncoderFrameSettings* frame_settings =
        JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
    const size_t nb_options = 23;
    const JxlEncoderFrameSettingId options[nb_options] = {
        JXL_ENC_FRAME_SETTING_EFFORT,
        JXL_ENC_FRAME_SETTING_BROTLI_EFFORT,
        JXL_ENC_FRAME_SETTING_DECODING_SPEED,
        JXL_ENC_FRAME_SETTING_RESAMPLING,
        JXL_ENC_FRAME_SETTING_EXTRA_CHANNEL_RESAMPLING,
        JXL_ENC_FRAME_SETTING_ALREADY_DOWNSAMPLED,
        JXL_ENC_FRAME_SETTING_EPF,
        JXL_ENC_FRAME_SETTING_GROUP_ORDER_CENTER_X,
        JXL_ENC_FRAME_SETTING_GROUP_ORDER_CENTER_Y,
        JXL_ENC_FRAME_SETTING_PROGRESSIVE_DC,
        JXL_ENC_FRAME_SETTING_PALETTE_COLORS,
        JXL_ENC_FRAME_SETTING_COLOR_TRANSFORM,
        JXL_ENC_FRAME_SETTING_MODULAR_COLOR_SPACE,
        JXL_ENC_FRAME_SETTING_MODULAR_GROUP_SIZE,
        JXL_ENC_FRAME_SETTING_MODULAR_PREDICTOR,
        JXL_ENC_FRAME_SETTING_MODULAR_NB_PREV_CHANNELS,
        JXL_ENC_FRAME_SETTING_JPEG_RECON_CFL,
        JXL_ENC_FRAME_INDEX_BOX,
        JXL_ENC_FRAME_SETTING_JPEG_COMPRESS_BOXES,
        JXL_ENC_FRAME_SETTING_BUFFERING,
        JXL_ENC_FRAME_SETTING_JPEG_KEEP_EXIF,
        JXL_ENC_FRAME_SETTING_JPEG_KEEP_XMP,
        JXL_ENC_FRAME_SETTING_JPEG_KEEP_JUMBF};
    const int too_low[nb_options] = {0,  -2, -2, 3,  -2, -2, -2, -2,
                                     -2, -2, -2, -2, -2, -2, -2, -2,
                                     -2, -1, -2, -2, -2, -2, -2};
    const int too_high[nb_options] = {11, 12, 5,     16, 6,  2, 4,  -3,
                                      -3, 3,  70914, 3,  42, 4, 16, 12,
                                      2,  2,  2,     4,  2,  2, 2};
    const int in_range[nb_options] = {5,  5, 3,  1,  1,  1,  3,  -1,
                                      0,  1, -1, -1, 3,  2,  15, -1,
                                      -1, 1, 0,  0,  -1, -1, -1};
    for (size_t i = 0; i < nb_options; i++) {
      // Lower than currently supported values
      EXPECT_EQ(JXL_ENC_ERROR, JxlEncoderFrameSettingsSetOption(
                                   frame_settings, options[i], too_low[i]));
      // Higher than currently supported values
      EXPECT_EQ(JXL_ENC_ERROR, JxlEncoderFrameSettingsSetOption(
                                   frame_settings, options[i], too_high[i]));
      // Using SetFloatOption on integer options
      EXPECT_EQ(JXL_ENC_ERROR, JxlEncoderFrameSettingsSetFloatOption(
                                   frame_settings, options[i], 1.0f));
      // Within range of the currently supported values
      EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderFrameSettingsSetOption(
                                     frame_settings, options[i], in_range[i]));
    }
    // Effort 11 should only work when expert options are allowed
    EXPECT_EQ(JXL_ENC_ERROR,
              JxlEncoderFrameSettingsSetOption(
                  frame_settings, JXL_ENC_FRAME_SETTING_EFFORT, 11));
    JxlEncoderAllowExpertOptions(enc.get());
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderFrameSettingsSetOption(
                  frame_settings, JXL_ENC_FRAME_SETTING_EFFORT, 11));

    // Non-existing option
    EXPECT_EQ(JXL_ENC_ERROR,
              JxlEncoderFrameSettingsSetOption(
                  frame_settings, JXL_ENC_FRAME_SETTING_FILL_ENUM, 0));
    EXPECT_EQ(JXL_ENC_ERROR,
              JxlEncoderFrameSettingsSetFloatOption(
                  frame_settings, JXL_ENC_FRAME_SETTING_FILL_ENUM, 0.f));

    // Float options
    EXPECT_EQ(JXL_ENC_ERROR,
              JxlEncoderFrameSettingsSetFloatOption(
                  frame_settings, JXL_ENC_FRAME_SETTING_PHOTON_NOISE, -1.0f));
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderFrameSettingsSetFloatOption(
                  frame_settings, JXL_ENC_FRAME_SETTING_PHOTON_NOISE, 100.0f));
    EXPECT_EQ(
        JXL_ENC_ERROR,
        JxlEncoderFrameSettingsSetFloatOption(
            frame_settings,
            JXL_ENC_FRAME_SETTING_MODULAR_MA_TREE_LEARNING_PERCENT, 101.0f));
    EXPECT_EQ(
        JXL_ENC_ERROR,
        JxlEncoderFrameSettingsSetFloatOption(
            frame_settings,
            JXL_ENC_FRAME_SETTING_MODULAR_MA_TREE_LEARNING_PERCENT, -2.0f));
    EXPECT_EQ(
        JXL_ENC_SUCCESS,
        JxlEncoderFrameSettingsSetFloatOption(
            frame_settings,
            JXL_ENC_FRAME_SETTING_MODULAR_MA_TREE_LEARNING_PERCENT, -1.0f));
    EXPECT_EQ(JXL_ENC_ERROR,
              JxlEncoderFrameSettingsSetFloatOption(
                  frame_settings,
                  JXL_ENC_FRAME_SETTING_CHANNEL_COLORS_GLOBAL_PERCENT, 101.0f));
    EXPECT_EQ(JXL_ENC_ERROR,
              JxlEncoderFrameSettingsSetFloatOption(
                  frame_settings,
                  JXL_ENC_FRAME_SETTING_CHANNEL_COLORS_GLOBAL_PERCENT, -2.0f));
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderFrameSettingsSetFloatOption(
                  frame_settings,
                  JXL_ENC_FRAME_SETTING_CHANNEL_COLORS_GLOBAL_PERCENT, -1.0f));
    EXPECT_EQ(JXL_ENC_ERROR,
              JxlEncoderFrameSettingsSetFloatOption(
                  frame_settings,
                  JXL_ENC_FRAME_SETTING_CHANNEL_COLORS_GROUP_PERCENT, 101.0f));
    EXPECT_EQ(JXL_ENC_ERROR,
              JxlEncoderFrameSettingsSetFloatOption(
                  frame_settings,
                  JXL_ENC_FRAME_SETTING_CHANNEL_COLORS_GROUP_PERCENT, -2.0f));
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderFrameSettingsSetFloatOption(
                  frame_settings,
                  JXL_ENC_FRAME_SETTING_CHANNEL_COLORS_GROUP_PERCENT, -1.0f));
    EXPECT_EQ(JXL_ENC_ERROR,
              JxlEncoderFrameSettingsSetOption(
                  frame_settings,
                  JXL_ENC_FRAME_SETTING_CHANNEL_COLORS_GROUP_PERCENT, 50.0f));
    EXPECT_EQ(JXL_ENC_ERROR,
              JxlEncoderFrameSettingsSetOption(
                  frame_settings, JXL_ENC_FRAME_SETTING_PHOTON_NOISE, 50.0f));

    VerifyFrameEncoding(63, 129, enc.get(), frame_settings, 3700, false);
  }

  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    EXPECT_NE(nullptr, enc.get());
    JxlEncoderFrameSettings* frame_settings =
        JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderSetFrameLossless(frame_settings, JXL_TRUE));
    VerifyFrameEncoding(63, 129, enc.get(), frame_settings, 3600, false);
    EXPECT_EQ(true, enc->last_used_cparams.IsLossless());
  }

  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    EXPECT_NE(nullptr, enc.get());
    JxlEncoderFrameSettings* frame_settings =
        JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
    EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetFrameDistance(frame_settings, 0.5));
    VerifyFrameEncoding(63, 129, enc.get(), frame_settings, 3200, false);
    EXPECT_EQ(0.5, enc->last_used_cparams.butteraugli_distance);
  }

  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    JxlEncoderFrameSettings* frame_settings =
        JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
    // Disallowed negative distance
    EXPECT_EQ(JXL_ENC_ERROR, JxlEncoderSetFrameDistance(frame_settings, -1));
  }

  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    EXPECT_NE(nullptr, enc.get());
    JxlEncoderFrameSettings* frame_settings =
        JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderFrameSettingsSetOption(
                  frame_settings, JXL_ENC_FRAME_SETTING_DECODING_SPEED, 2));
    VerifyFrameEncoding(enc.get(), frame_settings);
    EXPECT_EQ(2u, enc->last_used_cparams.decoding_speed_tier);
  }

  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    EXPECT_NE(nullptr, enc.get());
    JxlEncoderFrameSettings* frame_settings =
        JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
    EXPECT_EQ(JXL_ENC_ERROR,
              JxlEncoderFrameSettingsSetOption(
                  frame_settings, JXL_ENC_FRAME_SETTING_GROUP_ORDER, 100));
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderFrameSettingsSetOption(
                  frame_settings, JXL_ENC_FRAME_SETTING_GROUP_ORDER, 1));
    EXPECT_EQ(
        JXL_ENC_SUCCESS,
        JxlEncoderFrameSettingsSetOption(
            frame_settings, JXL_ENC_FRAME_SETTING_GROUP_ORDER_CENTER_X, 5));
    VerifyFrameEncoding(enc.get(), frame_settings);
    EXPECT_EQ(true, enc->last_used_cparams.centerfirst);
    EXPECT_EQ(5, enc->last_used_cparams.center_x);
  }

  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    EXPECT_NE(nullptr, enc.get());
    JxlEncoderFrameSettings* frame_settings =
        JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderFrameSettingsSetOption(
                  frame_settings, JXL_ENC_FRAME_SETTING_RESPONSIVE, 0));
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderFrameSettingsSetOption(
                  frame_settings, JXL_ENC_FRAME_SETTING_PROGRESSIVE_AC, 1));
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderFrameSettingsSetOption(
                  frame_settings, JXL_ENC_FRAME_SETTING_QPROGRESSIVE_AC, -1));
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderFrameSettingsSetOption(
                  frame_settings, JXL_ENC_FRAME_SETTING_PROGRESSIVE_DC, 2));
    VerifyFrameEncoding(63, 129, enc.get(), frame_settings, 3430,
                        /*lossy_use_original_profile=*/false);
    EXPECT_EQ(false, enc->last_used_cparams.responsive);
    EXPECT_EQ(jxl::Override::kOn, enc->last_used_cparams.progressive_mode);
    EXPECT_EQ(2, enc->last_used_cparams.progressive_dc);
  }

  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    EXPECT_NE(nullptr, enc.get());
    JxlEncoderFrameSettings* frame_settings =
        JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
    EXPECT_EQ(
        JXL_ENC_SUCCESS,
        JxlEncoderFrameSettingsSetFloatOption(
            frame_settings, JXL_ENC_FRAME_SETTING_PHOTON_NOISE, 1777.777));
    VerifyFrameEncoding(enc.get(), frame_settings);
    EXPECT_NEAR(1777.777f, enc->last_used_cparams.photon_noise_iso, 1E-4);
  }

  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    EXPECT_NE(nullptr, enc.get());
    JxlEncoderFrameSettings* frame_settings =
        JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderFrameSettingsSetFloatOption(
                  frame_settings,
                  JXL_ENC_FRAME_SETTING_CHANNEL_COLORS_GLOBAL_PERCENT, 55.0f));
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderFrameSettingsSetFloatOption(
                  frame_settings,
                  JXL_ENC_FRAME_SETTING_CHANNEL_COLORS_GROUP_PERCENT, 25.0f));
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderFrameSettingsSetOption(
                  frame_settings, JXL_ENC_FRAME_SETTING_PALETTE_COLORS, 70000));
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderFrameSettingsSetOption(
                  frame_settings, JXL_ENC_FRAME_SETTING_LOSSY_PALETTE, 1));
    VerifyFrameEncoding(enc.get(), frame_settings);
    EXPECT_NEAR(55.0f,
                enc->last_used_cparams.channel_colors_pre_transform_percent,
                1E-6);
    EXPECT_NEAR(25.0f, enc->last_used_cparams.channel_colors_percent, 1E-6);
    EXPECT_EQ(70000, enc->last_used_cparams.palette_colors);
    EXPECT_EQ(true, enc->last_used_cparams.lossy_palette);
  }

  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    EXPECT_NE(nullptr, enc.get());
    JxlEncoderFrameSettings* frame_settings =
        JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
    EXPECT_EQ(
        JXL_ENC_SUCCESS,
        JxlEncoderFrameSettingsSetOption(
            frame_settings, JXL_ENC_FRAME_SETTING_MODULAR_COLOR_SPACE, 30));
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderFrameSettingsSetOption(
                  frame_settings, JXL_ENC_FRAME_SETTING_MODULAR_GROUP_SIZE, 2));
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderFrameSettingsSetOption(
                  frame_settings, JXL_ENC_FRAME_SETTING_MODULAR_PREDICTOR, 14));
    EXPECT_EQ(
        JXL_ENC_SUCCESS,
        JxlEncoderFrameSettingsSetFloatOption(
            frame_settings,
            JXL_ENC_FRAME_SETTING_MODULAR_MA_TREE_LEARNING_PERCENT, 77.0f));
    EXPECT_EQ(
        JXL_ENC_SUCCESS,
        JxlEncoderFrameSettingsSetOption(
            frame_settings, JXL_ENC_FRAME_SETTING_MODULAR_NB_PREV_CHANNELS, 7));
    VerifyFrameEncoding(enc.get(), frame_settings);
    EXPECT_EQ(30, enc->last_used_cparams.colorspace);
    EXPECT_EQ(2, enc->last_used_cparams.modular_group_size_shift);
    EXPECT_EQ(jxl::Predictor::Best, enc->last_used_cparams.options.predictor);
    EXPECT_NEAR(0.77f, enc->last_used_cparams.options.nb_repeats, 1E-6);
    EXPECT_EQ(7, enc->last_used_cparams.options.max_properties);
  }

  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    EXPECT_NE(nullptr, enc.get());
    JxlEncoderFrameSettings* frame_settings =
        JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderFrameSettingsSetOption(
                  frame_settings, JXL_ENC_FRAME_SETTING_JPEG_RECON_CFL, 0));
    VerifyFrameEncoding(enc.get(), frame_settings);
    EXPECT_EQ(false, enc->last_used_cparams.force_cfl_jpeg_recompression);
  }

  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    EXPECT_NE(nullptr, enc.get());
    JxlEncoderFrameSettings* frame_settings =
        JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderFrameSettingsSetOption(
                  frame_settings, JXL_ENC_FRAME_SETTING_JPEG_RECON_CFL, 1));
    VerifyFrameEncoding(enc.get(), frame_settings);
    EXPECT_EQ(true, enc->last_used_cparams.force_cfl_jpeg_recompression);
  }
}

TEST(EncodeTest, LossyEncoderUseOriginalProfileTest) {
  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    ASSERT_NE(nullptr, enc.get());
    JxlEncoderFrameSettings* frame_settings =
        JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
    VerifyFrameEncoding(63, 129, enc.get(), frame_settings, 7897, true);
  }
  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    ASSERT_NE(nullptr, enc.get());
    JxlEncoderFrameSettings* frame_settings =
        JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderFrameSettingsSetOption(
                  frame_settings, JXL_ENC_FRAME_SETTING_PROGRESSIVE_DC, 2));
    VerifyFrameEncoding(63, 129, enc.get(), frame_settings, 8310, true);
  }
  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    ASSERT_NE(nullptr, enc.get());
    JxlEncoderFrameSettings* frame_settings =
        JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
    ASSERT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderFrameSettingsSetOption(
                  frame_settings, JXL_ENC_FRAME_SETTING_EFFORT, 8));
    VerifyFrameEncoding(63, 129, enc.get(), frame_settings, 7228, true);
  }
}

namespace {
// Returns a copy of buf from offset to offset+size, or a new zeroed vector if
// the result would have been out of bounds taking integer overflow into
// account.
std::vector<uint8_t> SliceSpan(const jxl::Span<const uint8_t>& buf,
                               size_t offset, size_t size) {
  if (offset + size >= buf.size()) {
    return std::vector<uint8_t>(size, 0);
  }
  if (offset + size < offset) {
    return std::vector<uint8_t>(size, 0);
  }
  return std::vector<uint8_t>(buf.data() + offset, buf.data() + offset + size);
}

struct Box {
  // The type of the box.
  // If "uuid", use extended_type instead
  char type[4] = {0, 0, 0, 0};

  // The extended_type is only used when type == "uuid".
  // Extended types are not used in JXL. However, the box format itself
  // supports this so they are handled correctly.
  char extended_type[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

  // Box data.
  jxl::Span<const uint8_t> data = jxl::Bytes(nullptr, 0);

  // If the size is not given, the datasize extends to the end of the file.
  // If this field is false, the size field is not encoded when the box is
  // serialized.
  bool data_size_given = true;

  // If successful, returns true and sets `in` to be the rest data (if any).
  // If `in` contains a box with a size larger than `in.size()`, will not
  // modify `in`, and will return true but the data `Span<uint8_t>` will
  // remain set to nullptr.
  // If unsuccessful, returns error and doesn't modify `in`.
  jxl::Status Decode(jxl::Span<const uint8_t>* in) {
    // Total box_size including this header itself.
    uint64_t box_size = LoadBE32(SliceSpan(*in, 0, 4).data());
    size_t pos = 4;

    memcpy(type, SliceSpan(*in, pos, 4).data(), 4);
    pos += 4;

    if (box_size == 1) {
      // If the size is 1, it indicates extended size read from 64-bit integer.
      box_size = LoadBE64(SliceSpan(*in, pos, 8).data());
      pos += 8;
    }

    if (!memcmp("uuid", type, 4)) {
      memcpy(extended_type, SliceSpan(*in, pos, 16).data(), 16);
      pos += 16;
    }

    // This is the end of the box header, the box data begins here. Handle
    // the data size now.
    const size_t header_size = pos;

    if (box_size != 0) {
      if (box_size < header_size) {
        return JXL_FAILURE("Invalid box size");
      }
      if (box_size > in->size()) {
        // The box is fine, but the input is too short.
        return true;
      }
      data_size_given = true;
      data = jxl::Bytes(in->data() + header_size, box_size - header_size);
    } else {
      data_size_given = false;
      data = jxl::Bytes(in->data() + header_size, in->size() - header_size);
    }

    *in = jxl::Bytes(in->data() + header_size + data.size(),
                     in->size() - header_size - data.size());
    return true;
  }
};

struct Container {
  std::vector<Box> boxes;

  // If successful, returns true and sets `in` to be the rest data (if any).
  // If unsuccessful, returns error and doesn't modify `in`.
  jxl::Status Decode(jxl::Span<const uint8_t>* in) {
    boxes.clear();

    Box signature_box;
    JXL_RETURN_IF_ERROR(signature_box.Decode(in));
    if (memcmp("JXL ", signature_box.type, 4) != 0) {
      return JXL_FAILURE("Invalid magic signature");
    }
    if (signature_box.data.size() != 4)
      return JXL_FAILURE("Invalid magic signature");
    if (signature_box.data[0] != 0xd || signature_box.data[1] != 0xa ||
        signature_box.data[2] != 0x87 || signature_box.data[3] != 0xa) {
      return JXL_FAILURE("Invalid magic signature");
    }

    Box ftyp_box;
    JXL_RETURN_IF_ERROR(ftyp_box.Decode(in));
    if (memcmp("ftyp", ftyp_box.type, 4) != 0) {
      return JXL_FAILURE("Invalid ftyp");
    }
    if (ftyp_box.data.size() != 12) return JXL_FAILURE("Invalid ftyp");
    const char* expected = "jxl \0\0\0\0jxl ";
    if (memcmp(expected, ftyp_box.data.data(), 12) != 0)
      return JXL_FAILURE("Invalid ftyp");

    while (!in->empty()) {
      Box box = {};
      JXL_RETURN_IF_ERROR(box.Decode(in));
      if (box.data.data() == nullptr) {
        // The decoding encountered a box, but not enough data yet.
        return true;
      }
      boxes.emplace_back(box);
    }

    return true;
  }
};

}  // namespace

TEST(EncodeTest, SingleFrameBoundedJXLCTest) {
  JxlEncoderPtr enc = JxlEncoderMake(nullptr);
  EXPECT_NE(nullptr, enc.get());
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderUseContainer(enc.get(), true));
  JxlEncoderFrameSettings* frame_settings =
      JxlEncoderFrameSettingsCreate(enc.get(), nullptr);

  size_t xsize = 71;
  size_t ysize = 23;
  JxlPixelFormat pixel_format = {4, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);

  JxlBasicInfo basic_info;
  jxl::test::JxlBasicInfoSetFromPixelFormat(&basic_info, &pixel_format);
  basic_info.xsize = xsize;
  basic_info.ysize = ysize;
  basic_info.uses_original_profile = JXL_FALSE;
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetCodestreamLevel(enc.get(), 10));
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetBasicInfo(enc.get(), &basic_info));
  JxlColorEncoding color_encoding;
  JxlColorEncodingSetToSRGB(&color_encoding,
                            /*is_gray=*/JXL_FALSE);
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderSetColorEncoding(enc.get(), &color_encoding));
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderAddImageFrame(frame_settings, &pixel_format,
                                    pixels.data(), pixels.size()));
  JxlEncoderCloseInput(enc.get());

  std::vector<uint8_t> compressed = std::vector<uint8_t>(64);
  uint8_t* next_out = compressed.data();
  size_t avail_out = compressed.size() - (next_out - compressed.data());
  JxlEncoderStatus process_result = JXL_ENC_NEED_MORE_OUTPUT;
  while (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
    process_result = JxlEncoderProcessOutput(enc.get(), &next_out, &avail_out);
    if (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
      size_t offset = next_out - compressed.data();
      compressed.resize(compressed.size() * 2);
      next_out = compressed.data() + offset;
      avail_out = compressed.size() - offset;
    }
  }
  compressed.resize(next_out - compressed.data());
  EXPECT_EQ(JXL_ENC_SUCCESS, process_result);

  Container container = {};
  jxl::Span<const uint8_t> encoded_span =
      jxl::Bytes(compressed.data(), compressed.size());
  EXPECT_TRUE(container.Decode(&encoded_span));
  EXPECT_EQ(0u, encoded_span.size());
  bool found_jxlc = false;
  bool found_jxlp = false;
  // The encoder is allowed to either emit a jxlc or one or more jxlp.
  for (const auto& box : container.boxes) {
    if (memcmp("jxlc", box.type, 4) == 0) {
      EXPECT_EQ(false, found_jxlc);  // Max 1 jxlc
      EXPECT_EQ(false, found_jxlp);  // Can't mix jxlc and jxlp
      found_jxlc = true;
    }
    if (memcmp("jxlp", box.type, 4) == 0) {
      EXPECT_EQ(false, found_jxlc);  // Can't mix jxlc and jxlp
      found_jxlp = true;
    }
    // The encoder shouldn't create an unbounded box in this case, with the
    // single frame it knows the full size in time, so can help make decoding
    // more efficient by giving the full box size of the final box.
    EXPECT_EQ(true, box.data_size_given);
  }
  EXPECT_EQ(true, found_jxlc || found_jxlp);
}

TEST(EncodeTest, CodestreamLevelTest) {
  size_t xsize = 64;
  size_t ysize = 64;
  JxlPixelFormat pixel_format = {4, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);

  jxl::CodecInOut input_io =
      jxl::test::SomeTestImageToCodecInOut(pixels, 4, xsize, ysize);

  JxlBasicInfo basic_info;
  jxl::test::JxlBasicInfoSetFromPixelFormat(&basic_info, &pixel_format);
  basic_info.xsize = xsize;
  basic_info.ysize = ysize;
  basic_info.uses_original_profile = 0;

  JxlEncoderPtr enc = JxlEncoderMake(nullptr);
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetCodestreamLevel(enc.get(), 10));
  JxlEncoderFrameSettings* frame_settings =
      JxlEncoderFrameSettingsCreate(enc.get(), nullptr);

  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetBasicInfo(enc.get(), &basic_info));
  JxlColorEncoding color_encoding;
  JXL_BOOL is_gray = TO_JXL_BOOL(pixel_format.num_channels < 3);
  JxlColorEncodingSetToSRGB(&color_encoding, is_gray);
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderSetColorEncoding(enc.get(), &color_encoding));
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderAddImageFrame(frame_settings, &pixel_format,
                                    pixels.data(), pixels.size()));
  JxlEncoderCloseInput(enc.get());

  std::vector<uint8_t> compressed = std::vector<uint8_t>(64);
  uint8_t* next_out = compressed.data();
  size_t avail_out = compressed.size() - (next_out - compressed.data());
  JxlEncoderStatus process_result = JXL_ENC_NEED_MORE_OUTPUT;
  while (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
    process_result = JxlEncoderProcessOutput(enc.get(), &next_out, &avail_out);
    if (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
      size_t offset = next_out - compressed.data();
      compressed.resize(compressed.size() * 2);
      next_out = compressed.data() + offset;
      avail_out = compressed.size() - offset;
    }
  }
  compressed.resize(next_out - compressed.data());
  EXPECT_EQ(JXL_ENC_SUCCESS, process_result);

  Container container = {};
  jxl::Span<const uint8_t> encoded_span =
      jxl::Bytes(compressed.data(), compressed.size());
  EXPECT_TRUE(container.Decode(&encoded_span));
  EXPECT_EQ(0u, encoded_span.size());
  EXPECT_EQ(0, memcmp("jxll", container.boxes[0].type, 4));
}

TEST(EncodeTest, CodestreamLevelVerificationTest) {
  JxlPixelFormat pixel_format = {4, JXL_TYPE_UINT8, JXL_BIG_ENDIAN, 0};

  JxlBasicInfo basic_info;
  jxl::test::JxlBasicInfoSetFromPixelFormat(&basic_info, &pixel_format);
  basic_info.xsize = 64;
  basic_info.ysize = 64;
  basic_info.uses_original_profile = JXL_FALSE;

  JxlEncoderPtr enc = JxlEncoderMake(nullptr);
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetBasicInfo(enc.get(), &basic_info));

  EXPECT_EQ(5, JxlEncoderGetRequiredCodestreamLevel(enc.get()));

  // Set an image dimension that is too large for level 5, but fits in level 10

  basic_info.xsize = 1ull << 30ull;
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetCodestreamLevel(enc.get(), 5));
  EXPECT_EQ(JXL_ENC_ERROR, JxlEncoderSetBasicInfo(enc.get(), &basic_info));
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetCodestreamLevel(enc.get(), 10));
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetBasicInfo(enc.get(), &basic_info));
  EXPECT_EQ(10, JxlEncoderGetRequiredCodestreamLevel(enc.get()));

  // Set an image dimension that is too large even for level 10

  basic_info.xsize = 1ull << 31ull;
  EXPECT_EQ(JXL_ENC_ERROR, JxlEncoderSetBasicInfo(enc.get(), &basic_info));
}

JXL_TRANSCODE_JPEG_TEST(EncodeTest, JPEGReconstructionTest) {
  const std::string jpeg_path = "jxl/flower/flower.png.im_q85_420.jpg";
  const std::vector<uint8_t> orig = jxl::test::ReadTestData(jpeg_path);

  JxlEncoderPtr enc = JxlEncoderMake(nullptr);
  JxlEncoderFrameSettings* frame_settings =
      JxlEncoderFrameSettingsCreate(enc.get(), nullptr);

  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderStoreJPEGMetadata(enc.get(), JXL_TRUE));
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderAddJPEGFrame(frame_settings, orig.data(), orig.size()));
  JxlEncoderCloseInput(enc.get());

  std::vector<uint8_t> compressed = std::vector<uint8_t>(64);
  uint8_t* next_out = compressed.data();
  size_t avail_out = compressed.size() - (next_out - compressed.data());
  JxlEncoderStatus process_result = JXL_ENC_NEED_MORE_OUTPUT;
  while (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
    process_result = JxlEncoderProcessOutput(enc.get(), &next_out, &avail_out);
    if (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
      size_t offset = next_out - compressed.data();
      compressed.resize(compressed.size() * 2);
      next_out = compressed.data() + offset;
      avail_out = compressed.size() - offset;
    }
  }
  compressed.resize(next_out - compressed.data());
  EXPECT_EQ(JXL_ENC_SUCCESS, process_result);

  jxl::extras::JXLDecompressParams dparams;
  jxl::test::DefaultAcceptedFormats(dparams);
  std::vector<uint8_t> decoded_jpeg_bytes;
  jxl::extras::PackedPixelFile ppf;
  EXPECT_TRUE(DecodeImageJXL(compressed.data(), compressed.size(), dparams,
                             nullptr, &ppf, &decoded_jpeg_bytes));

  EXPECT_EQ(decoded_jpeg_bytes.size(), orig.size());
  EXPECT_EQ(0, memcmp(decoded_jpeg_bytes.data(), orig.data(), orig.size()));
}

JXL_TRANSCODE_JPEG_TEST(EncodeTest, ProgressiveJPEGReconstructionTest) {
  const std::string jpeg_path = "jxl/flower/flower.png.im_q85_420.jpg";
  const std::vector<uint8_t> orig = jxl::test::ReadTestData(jpeg_path);

  JxlEncoderPtr enc = JxlEncoderMake(nullptr);
  JxlEncoderFrameSettings* frame_settings =
      JxlEncoderFrameSettingsCreate(enc.get(), nullptr);

  frame_settings->values.cparams.progressive_mode = jxl::Override::kOn;

  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderStoreJPEGMetadata(enc.get(), JXL_TRUE));
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderAddJPEGFrame(frame_settings, orig.data(), orig.size()));
  JxlEncoderCloseInput(enc.get());

  std::vector<uint8_t> compressed = std::vector<uint8_t>(64);
  uint8_t* next_out = compressed.data();
  size_t avail_out = compressed.size() - (next_out - compressed.data());
  JxlEncoderStatus process_result = JXL_ENC_NEED_MORE_OUTPUT;
  while (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
    process_result = JxlEncoderProcessOutput(enc.get(), &next_out, &avail_out);
    if (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
      size_t offset = next_out - compressed.data();
      compressed.resize(compressed.size() * 2);
      next_out = compressed.data() + offset;
      avail_out = compressed.size() - offset;
    }
  }
  compressed.resize(next_out - compressed.data());
  EXPECT_EQ(JXL_ENC_SUCCESS, process_result);

  jxl::extras::JXLDecompressParams dparams;
  jxl::test::DefaultAcceptedFormats(dparams);
  std::vector<uint8_t> decoded_jpeg_bytes;
  jxl::extras::PackedPixelFile ppf;
  EXPECT_TRUE(DecodeImageJXL(compressed.data(), compressed.size(), dparams,
                             nullptr, &ppf, &decoded_jpeg_bytes));

  EXPECT_EQ(decoded_jpeg_bytes.size(), orig.size());
  EXPECT_EQ(0, memcmp(decoded_jpeg_bytes.data(), orig.data(), orig.size()));
}

static void ProcessEncoder(JxlEncoder* enc, std::vector<uint8_t>& compressed,
                           uint8_t*& next_out, size_t& avail_out) {
  JxlEncoderStatus process_result = JXL_ENC_NEED_MORE_OUTPUT;
  while (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
    process_result = JxlEncoderProcessOutput(enc, &next_out, &avail_out);
    if (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
      size_t offset = next_out - compressed.data();
      compressed.resize(compressed.size() * 2);
      next_out = compressed.data() + offset;
      avail_out = compressed.size() - offset;
    }
  }
  size_t offset = next_out - compressed.data();
  compressed.resize(next_out - compressed.data());
  next_out = compressed.data() + offset;
  avail_out = compressed.size() - offset;
  EXPECT_EQ(JXL_ENC_SUCCESS, process_result);
}

TEST(EncodeTest, BasicInfoTest) {
  JxlEncoderPtr enc = JxlEncoderMake(nullptr);
  EXPECT_NE(nullptr, enc.get());

  JxlEncoderFrameSettings* frame_settings =
      JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
  size_t xsize = 1;
  size_t ysize = 1;
  JxlPixelFormat pixel_format = {4, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);
  JxlBasicInfo basic_info;
  jxl::test::JxlBasicInfoSetFromPixelFormat(&basic_info, &pixel_format);
  basic_info.xsize = xsize;
  basic_info.ysize = ysize;
  basic_info.uses_original_profile = 0;
  basic_info.have_animation = 1;
  basic_info.intensity_target = 123.4;
  basic_info.min_nits = 5.0;
  basic_info.linear_below = 12.7;
  basic_info.orientation = JXL_ORIENT_ROTATE_90_CW;
  basic_info.intrinsic_xsize = 88;
  basic_info.intrinsic_ysize = 99;
  basic_info.animation.tps_numerator = 55;
  basic_info.animation.tps_denominator = 77;
  basic_info.animation.num_loops = 10;
  basic_info.animation.have_timecodes = JXL_TRUE;
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetCodestreamLevel(enc.get(), 10));
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetBasicInfo(enc.get(), &basic_info));
  JxlColorEncoding color_encoding;
  JxlColorEncodingSetToSRGB(&color_encoding, /*is_gray=*/JXL_FALSE);
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderSetColorEncoding(enc.get(), &color_encoding));

  std::vector<uint8_t> compressed = std::vector<uint8_t>(64);
  uint8_t* next_out = compressed.data();
  size_t avail_out = compressed.size() - (next_out - compressed.data());
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderAddImageFrame(frame_settings, &pixel_format,
                                    pixels.data(), pixels.size()));
  JxlEncoderCloseFrames(enc.get());
  ProcessEncoder(enc.get(), compressed, next_out, avail_out);

  // Decode to verify the boxes, we don't decode to pixels, only the boxes.
  JxlDecoderPtr dec = JxlDecoderMake(nullptr);
  EXPECT_NE(nullptr, dec.get());
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(dec.get(), JXL_DEC_BASIC_INFO));
  // Allow testing the orientation field, without this setting it will be
  // overridden to identity.
  JxlDecoderSetKeepOrientation(dec.get(), JXL_TRUE);
  JxlDecoderSetInput(dec.get(), compressed.data(), compressed.size());
  JxlDecoderCloseInput(dec.get());

  for (;;) {
    JxlDecoderStatus status = JxlDecoderProcessInput(dec.get());
    if (status == JXL_DEC_ERROR) {
      FAIL();
    } else if (status == JXL_DEC_SUCCESS) {
      break;
    } else if (status == JXL_DEC_BASIC_INFO) {
      JxlBasicInfo basic_info2;
      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderGetBasicInfo(dec.get(), &basic_info2));
      EXPECT_EQ(basic_info.xsize, basic_info2.xsize);
      EXPECT_EQ(basic_info.ysize, basic_info2.ysize);
      EXPECT_EQ(basic_info.bits_per_sample, basic_info2.bits_per_sample);
      EXPECT_EQ(basic_info.exponent_bits_per_sample,
                basic_info2.exponent_bits_per_sample);
      EXPECT_NEAR(basic_info.intensity_target, basic_info2.intensity_target,
                  0.5);
      EXPECT_NEAR(basic_info.min_nits, basic_info2.min_nits, 0.5);
      EXPECT_NEAR(basic_info.linear_below, basic_info2.linear_below, 0.5);
      EXPECT_EQ(basic_info.relative_to_max_display,
                basic_info2.relative_to_max_display);
      EXPECT_EQ(basic_info.uses_original_profile,
                basic_info2.uses_original_profile);
      EXPECT_EQ(basic_info.orientation, basic_info2.orientation);
      EXPECT_EQ(basic_info.intrinsic_xsize, basic_info2.intrinsic_xsize);
      EXPECT_EQ(basic_info.intrinsic_ysize, basic_info2.intrinsic_ysize);
      EXPECT_EQ(basic_info.num_color_channels, basic_info2.num_color_channels);
      // TODO(lode): also test num_extra_channels, but currently there may be a
      // mismatch between 0 and 1 if there is alpha, until encoder support for
      // extra channels is fully implemented.
      EXPECT_EQ(basic_info.alpha_bits, basic_info2.alpha_bits);
      EXPECT_EQ(basic_info.alpha_exponent_bits,
                basic_info2.alpha_exponent_bits);
      EXPECT_EQ(basic_info.alpha_premultiplied,
                basic_info2.alpha_premultiplied);

      EXPECT_EQ(basic_info.have_preview, basic_info2.have_preview);
      if (basic_info.have_preview) {
        EXPECT_EQ(basic_info.preview.xsize, basic_info2.preview.xsize);
        EXPECT_EQ(basic_info.preview.ysize, basic_info2.preview.ysize);
      }

      EXPECT_EQ(basic_info.have_animation, basic_info2.have_animation);
      if (basic_info.have_animation) {
        EXPECT_EQ(basic_info.animation.tps_numerator,
                  basic_info2.animation.tps_numerator);
        EXPECT_EQ(basic_info.animation.tps_denominator,
                  basic_info2.animation.tps_denominator);
        EXPECT_EQ(basic_info.animation.num_loops,
                  basic_info2.animation.num_loops);
        EXPECT_EQ(basic_info.animation.have_timecodes,
                  basic_info2.animation.have_timecodes);
      }
    } else {
      FAIL();  // unexpected status
    }
  }
}

TEST(EncodeTest, AnimationHeaderTest) {
  JxlEncoderPtr enc = JxlEncoderMake(nullptr);
  EXPECT_NE(nullptr, enc.get());

  JxlEncoderFrameSettings* frame_settings =
      JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
  size_t xsize = 1;
  size_t ysize = 1;
  JxlPixelFormat pixel_format = {4, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);
  JxlBasicInfo basic_info;
  jxl::test::JxlBasicInfoSetFromPixelFormat(&basic_info, &pixel_format);
  basic_info.xsize = xsize;
  basic_info.ysize = ysize;
  basic_info.have_animation = JXL_TRUE;
  basic_info.animation.tps_numerator = 1000;
  basic_info.animation.tps_denominator = 1;
  basic_info.animation.have_timecodes = JXL_TRUE;
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetCodestreamLevel(enc.get(), 10));
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetBasicInfo(enc.get(), &basic_info));
  JxlColorEncoding color_encoding;
  JxlColorEncodingSetToSRGB(&color_encoding, /*is_gray=*/JXL_FALSE);
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderSetColorEncoding(enc.get(), &color_encoding));

  std::string frame_name = "test frame";
  JxlFrameHeader header;
  JxlEncoderInitFrameHeader(&header);
  header.duration = 50;
  header.timecode = 800;
  header.layer_info.blend_info.blendmode = JXL_BLEND_BLEND;
  header.layer_info.blend_info.source = 2;
  header.layer_info.blend_info.clamp = 1;
  JxlBlendInfo extra_channel_blend_info;
  JxlEncoderInitBlendInfo(&extra_channel_blend_info);
  extra_channel_blend_info.blendmode = JXL_BLEND_MULADD;
  JxlEncoderSetFrameHeader(frame_settings, &header);
  JxlEncoderSetExtraChannelBlendInfo(frame_settings, 0,
                                     &extra_channel_blend_info);
  JxlEncoderSetFrameName(frame_settings, frame_name.c_str());

  std::vector<uint8_t> compressed = std::vector<uint8_t>(64);
  uint8_t* next_out = compressed.data();
  size_t avail_out = compressed.size() - (next_out - compressed.data());
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderAddImageFrame(frame_settings, &pixel_format,
                                    pixels.data(), pixels.size()));
  JxlEncoderCloseFrames(enc.get());
  ProcessEncoder(enc.get(), compressed, next_out, avail_out);

  // Decode to verify the boxes, we don't decode to pixels, only the boxes.
  JxlDecoderPtr dec = JxlDecoderMake(nullptr);
  EXPECT_NE(nullptr, dec.get());

  // To test the blend_info fields, coalescing must be set to false in the
  // decoder.
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetCoalescing(dec.get(), JXL_FALSE));
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(dec.get(), JXL_DEC_FRAME));
  JxlDecoderSetInput(dec.get(), compressed.data(), compressed.size());
  JxlDecoderCloseInput(dec.get());

  bool seen_frame = false;

  for (;;) {
    JxlDecoderStatus status = JxlDecoderProcessInput(dec.get());
    if (status == JXL_DEC_ERROR) {
      FAIL();
    } else if (status == JXL_DEC_SUCCESS) {
      break;
    } else if (status == JXL_DEC_FRAME) {
      seen_frame = true;
      JxlFrameHeader header2;
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetFrameHeader(dec.get(), &header2));
      EXPECT_EQ(header.duration, header2.duration);
      EXPECT_EQ(header.timecode, header2.timecode);
      EXPECT_EQ(header.layer_info.blend_info.blendmode,
                header2.layer_info.blend_info.blendmode);
      EXPECT_EQ(header.layer_info.blend_info.clamp,
                header2.layer_info.blend_info.clamp);
      EXPECT_EQ(header.layer_info.blend_info.source,
                header2.layer_info.blend_info.source);
      EXPECT_EQ(frame_name.size(), header2.name_length);
      JxlBlendInfo extra_channel_blend_info2;
      JxlDecoderGetExtraChannelBlendInfo(dec.get(), 0,
                                         &extra_channel_blend_info2);
      EXPECT_EQ(extra_channel_blend_info.blendmode,
                extra_channel_blend_info2.blendmode);
      if (header2.name_length > 0) {
        std::string frame_name2(header2.name_length + 1, '\0');
        EXPECT_EQ(JXL_DEC_SUCCESS,
                  JxlDecoderGetFrameName(dec.get(), &frame_name2.front(),
                                         frame_name2.size()));
        frame_name2.resize(header2.name_length);
        EXPECT_EQ(frame_name, frame_name2);
      }
    } else {
      FAIL();  // unexpected status
    }
  }

  EXPECT_EQ(true, seen_frame);
}
TEST(EncodeTest, CroppedFrameTest) {
  JxlEncoderPtr enc = JxlEncoderMake(nullptr);
  EXPECT_NE(nullptr, enc.get());

  JxlEncoderFrameSettings* frame_settings =
      JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
  size_t xsize = 300;
  size_t ysize = 300;
  JxlPixelFormat pixel_format = {4, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);
  std::vector<uint8_t> pixels2(pixels.size());
  JxlBasicInfo basic_info;
  jxl::test::JxlBasicInfoSetFromPixelFormat(&basic_info, &pixel_format);
  // Encoding a 300x300 frame in an image that is only 100x100
  basic_info.xsize = 100;
  basic_info.ysize = 100;
  basic_info.uses_original_profile = JXL_TRUE;
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetCodestreamLevel(enc.get(), 10));
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetBasicInfo(enc.get(), &basic_info));
  JxlColorEncoding color_encoding;
  JxlColorEncodingSetToSRGB(&color_encoding, /*is_gray=*/JXL_FALSE);
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderSetColorEncoding(enc.get(), &color_encoding));

  JxlFrameHeader header;
  JxlEncoderInitFrameHeader(&header);
  header.layer_info.have_crop = JXL_TRUE;
  header.layer_info.xsize = xsize;
  header.layer_info.ysize = ysize;
  header.layer_info.crop_x0 = -50;
  header.layer_info.crop_y0 = -250;
  JxlEncoderSetFrameLossless(frame_settings, JXL_TRUE);
  JxlEncoderSetFrameHeader(frame_settings, &header);
  JxlEncoderFrameSettingsSetOption(frame_settings, JXL_ENC_FRAME_SETTING_EFFORT,
                                   1);

  std::vector<uint8_t> compressed = std::vector<uint8_t>(100);
  uint8_t* next_out = compressed.data();
  size_t avail_out = compressed.size() - (next_out - compressed.data());
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderAddImageFrame(frame_settings, &pixel_format,
                                    pixels.data(), pixels.size()));
  JxlEncoderCloseFrames(enc.get());
  ProcessEncoder(enc.get(), compressed, next_out, avail_out);

  JxlDecoderPtr dec = JxlDecoderMake(nullptr);
  EXPECT_NE(nullptr, dec.get());
  // Non-coalesced decoding so we can get the full uncropped frame
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetCoalescing(dec.get(), JXL_FALSE));
  EXPECT_EQ(
      JXL_DEC_SUCCESS,
      JxlDecoderSubscribeEvents(dec.get(), JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE));
  JxlDecoderSetInput(dec.get(), compressed.data(), compressed.size());
  JxlDecoderCloseInput(dec.get());

  bool seen_frame = false;
  bool checked_frame = false;
  for (;;) {
    JxlDecoderStatus status = JxlDecoderProcessInput(dec.get());
    if (status == JXL_DEC_ERROR) {
      FAIL();
    } else if (status == JXL_DEC_SUCCESS) {
      break;
    } else if (status == JXL_DEC_FRAME) {
      seen_frame = true;
      JxlFrameHeader header2;
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetFrameHeader(dec.get(), &header2));
      EXPECT_EQ(header.layer_info.xsize, header2.layer_info.xsize);
      EXPECT_EQ(header.layer_info.ysize, header2.layer_info.ysize);
      EXPECT_EQ(header.layer_info.crop_x0, header2.layer_info.crop_x0);
      EXPECT_EQ(header.layer_info.crop_y0, header2.layer_info.crop_y0);
    } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderSetImageOutBuffer(dec.get(), &pixel_format,
                                            pixels2.data(), pixels2.size()));
    } else if (status == JXL_DEC_FULL_IMAGE) {
      EXPECT_EQ(0, memcmp(pixels.data(), pixels2.data(), pixels.size()));
      checked_frame = true;
    } else {
      FAIL();  // unexpected status
    }
  }
  EXPECT_EQ(true, checked_frame);
  EXPECT_EQ(true, seen_frame);
}

struct EncodeBoxTest : public testing::TestWithParam<std::tuple<bool, size_t>> {
};

JXL_BOXES_TEST_P(EncodeBoxTest, BoxTest) {
  // Test with uncompressed boxes and with brob boxes
  bool compress_box = std::get<0>(GetParam());
  size_t xml_box_size = std::get<1>(GetParam());
  // TODO(firsching): use xml_box_size
  (void)xml_box_size;
  // Tests adding two metadata boxes with the encoder: an exif box before the
  // image frame, and an xml box after the image frame. Then verifies the
  // decoder can decode them, they are in the expected place, and have the
  // correct content after decoding.
  JxlEncoderPtr enc = JxlEncoderMake(nullptr);
  EXPECT_NE(nullptr, enc.get());

  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderUseBoxes(enc.get()));

  JxlEncoderFrameSettings* frame_settings =
      JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
  size_t xsize = 50;
  size_t ysize = 17;
  JxlPixelFormat pixel_format = {4, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);
  JxlBasicInfo basic_info;
  jxl::test::JxlBasicInfoSetFromPixelFormat(&basic_info, &pixel_format);
  basic_info.xsize = xsize;
  basic_info.ysize = ysize;
  basic_info.uses_original_profile = JXL_FALSE;
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetCodestreamLevel(enc.get(), 10));
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetBasicInfo(enc.get(), &basic_info));
  JxlColorEncoding color_encoding;
  JxlColorEncodingSetToSRGB(&color_encoding,
                            /*is_gray=*/JXL_FALSE);
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderSetColorEncoding(enc.get(), &color_encoding));

  std::vector<uint8_t> compressed = std::vector<uint8_t>(64);
  uint8_t* next_out = compressed.data();
  size_t avail_out = compressed.size() - (next_out - compressed.data());

  // Add an early metadata box. Also add a valid 4-byte TIFF offset header
  // before the fake exif data of these box contents.
  constexpr const char* exif_test_string = "\0\0\0\0exif test data";
  const uint8_t* exif_data = reinterpret_cast<const uint8_t*>(exif_test_string);
  // Skip the 4 zeroes for strlen
  const size_t exif_size = 4 + strlen(exif_test_string + 4);
  JxlEncoderAddBox(enc.get(), "Exif", exif_data, exif_size,
                   TO_JXL_BOOL(compress_box));

  // Write to output
  ProcessEncoder(enc.get(), compressed, next_out, avail_out);

  // Add image frame
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderAddImageFrame(frame_settings, &pixel_format,
                                    pixels.data(), pixels.size()));
  // Indicate this is the last frame
  JxlEncoderCloseFrames(enc.get());

  // Write to output
  ProcessEncoder(enc.get(), compressed, next_out, avail_out);

  // Add a late metadata box
  constexpr const char* xml_test_string = "<some random xml data>";
  const uint8_t* xml_data = reinterpret_cast<const uint8_t*>(xml_test_string);
  size_t xml_size = strlen(xml_test_string);
  JxlEncoderAddBox(enc.get(), "XML ", xml_data, xml_size,
                   TO_JXL_BOOL(compress_box));

  // Indicate this is the last box
  JxlEncoderCloseBoxes(enc.get());

  // Write to output
  ProcessEncoder(enc.get(), compressed, next_out, avail_out);

  // Decode to verify the boxes, we don't decode to pixels, only the boxes.
  JxlDecoderPtr dec = JxlDecoderMake(nullptr);
  EXPECT_NE(nullptr, dec.get());

  if (compress_box) {
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderSetDecompressBoxes(dec.get(), JXL_TRUE));
  }

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(dec.get(), JXL_DEC_FRAME | JXL_DEC_BOX));

  JxlDecoderSetInput(dec.get(), compressed.data(), compressed.size());
  JxlDecoderCloseInput(dec.get());

  std::vector<uint8_t> dec_exif_box(exif_size);
  std::vector<uint8_t> dec_xml_box(xml_size);

  for (bool post_frame = false;;) {
    JxlDecoderStatus status = JxlDecoderProcessInput(dec.get());
    if (status == JXL_DEC_ERROR) {
      FAIL();
    } else if (status == JXL_DEC_SUCCESS) {
      EXPECT_EQ(0, JxlDecoderReleaseBoxBuffer(dec.get()));
      break;
    } else if (status == JXL_DEC_FRAME) {
      post_frame = true;
    } else if (status == JXL_DEC_BOX) {
      // Since we gave the exif/xml box output buffer of the exact known
      // correct size, 0 bytes should be released. Same when no buffer was
      // set.
      EXPECT_EQ(0, JxlDecoderReleaseBoxBuffer(dec.get()));
      JxlBoxType type;
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBoxType(dec.get(), type, true));
      if (memcmp(type, "Exif", 4) == 0) {
        // This box should have been encoded before the image frame
        EXPECT_EQ(false, post_frame);
        JxlDecoderSetBoxBuffer(dec.get(), dec_exif_box.data(),
                               dec_exif_box.size());
      } else if (memcmp(type, "XML ", 4) == 0) {
        // This box should have been encoded after the image frame
        EXPECT_EQ(true, post_frame);
        JxlDecoderSetBoxBuffer(dec.get(), dec_xml_box.data(),
                               dec_xml_box.size());
      }
    } else {
      FAIL();  // unexpected status
    }
  }

  EXPECT_EQ(0, memcmp(exif_data, dec_exif_box.data(), exif_size));
  EXPECT_EQ(0, memcmp(xml_data, dec_xml_box.data(), xml_size));
}

std::string nameBoxTest(
    const ::testing::TestParamInfo<std::tuple<bool, size_t>>& info) {
  return (std::get<0>(info.param) ? "C" : "Unc") + std::string("ompressed") +
         "_BoxSize_" + std::to_string((std::get<1>(info.param)));
}

JXL_GTEST_INSTANTIATE_TEST_SUITE_P(
    EncodeBoxParamsTest, EncodeBoxTest,
    testing::Combine(testing::Values(false, true),
                     testing::Values(256,
                                     jxl::kLargeBoxContentSizeThreshold + 77)),
    nameBoxTest);

JXL_TRANSCODE_JPEG_TEST(EncodeTest, JPEGFrameTest) {
  TEST_LIBJPEG_SUPPORT();
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  for (int skip_basic_info = 0; skip_basic_info < 2; skip_basic_info++) {
    for (int skip_color_encoding = 0; skip_color_encoding < 2;
         skip_color_encoding++) {
      // cannot set color encoding if basic info is not set
      if (skip_basic_info && !skip_color_encoding) continue;
      const std::string jpeg_path = "jxl/flower/flower_cropped.jpg";
      const std::vector<uint8_t> orig = jxl::test::ReadTestData(jpeg_path);
      jxl::CodecInOut orig_io{memory_manager};
      ASSERT_TRUE(SetFromBytes(jxl::Bytes(orig), &orig_io,
                               /*pool=*/nullptr));

      JxlEncoderPtr enc = JxlEncoderMake(nullptr);
      JxlEncoderFrameSettings* frame_settings =
          JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
      JxlEncoderFrameSettingsSetOption(frame_settings,
                                       JXL_ENC_FRAME_SETTING_EFFORT, 1);
      if (!skip_basic_info) {
        JxlBasicInfo basic_info;
        JxlEncoderInitBasicInfo(&basic_info);
        basic_info.xsize = orig_io.xsize();
        basic_info.ysize = orig_io.ysize();
        basic_info.uses_original_profile = JXL_TRUE;
        EXPECT_EQ(JXL_ENC_SUCCESS,
                  JxlEncoderSetBasicInfo(enc.get(), &basic_info));
      }
      if (!skip_color_encoding) {
        JxlColorEncoding color_encoding;
        JxlColorEncodingSetToSRGB(&color_encoding, /*is_gray=*/JXL_FALSE);
        EXPECT_EQ(JXL_ENC_SUCCESS,
                  JxlEncoderSetColorEncoding(enc.get(), &color_encoding));
      }
      EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderAddJPEGFrame(
                                     frame_settings, orig.data(), orig.size()));
      JxlEncoderCloseInput(enc.get());

      std::vector<uint8_t> compressed = std::vector<uint8_t>(64);
      uint8_t* next_out = compressed.data();
      size_t avail_out = compressed.size() - (next_out - compressed.data());
      JxlEncoderStatus process_result = JXL_ENC_NEED_MORE_OUTPUT;
      while (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
        process_result =
            JxlEncoderProcessOutput(enc.get(), &next_out, &avail_out);
        if (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
          size_t offset = next_out - compressed.data();
          compressed.resize(compressed.size() * 2);
          next_out = compressed.data() + offset;
          avail_out = compressed.size() - offset;
        }
      }
      compressed.resize(next_out - compressed.data());
      EXPECT_EQ(JXL_ENC_SUCCESS, process_result);

      jxl::CodecInOut decoded_io{memory_manager};
      EXPECT_TRUE(jxl::test::DecodeFile(
          {}, jxl::Bytes(compressed.data(), compressed.size()), &decoded_io));

      EXPECT_LE(ComputeDistance2(orig_io.Main(), decoded_io.Main(),
                                 *JxlGetDefaultCms()),
                3.5);
    }
  }
}

namespace {
class JxlStreamingAdapter {
 public:
  JxlStreamingAdapter(JxlEncoder* encoder, bool return_large_buffers,
                      bool can_seek)
      : return_large_buffers_(return_large_buffers) {
    struct JxlEncoderOutputProcessor output_processor;
    output_processor.opaque = this;
    output_processor.get_buffer =
        METHOD_TO_C_CALLBACK(&JxlStreamingAdapter::GetBuffer);
    if (can_seek) {
      output_processor.seek = METHOD_TO_C_CALLBACK(&JxlStreamingAdapter::Seek);
    } else {
      output_processor.seek = nullptr;
    }
    output_processor.set_finalized_position =
        METHOD_TO_C_CALLBACK(&JxlStreamingAdapter::SetFinalizedPosition);
    output_processor.release_buffer =
        METHOD_TO_C_CALLBACK(&JxlStreamingAdapter::ReleaseBuffer);
    EXPECT_EQ(JxlEncoderSetOutputProcessor(encoder, output_processor),
              JXL_ENC_SUCCESS);
  }

  std::vector<uint8_t> output() && {
    output_.resize(position_);
    return std::move(output_);
  }

  void* GetBuffer(size_t* size) {
    if (!return_large_buffers_) {
      *size = 1;
    }
    if (position_ + *size > output_.size()) {
      output_.resize(position_ + *size, 0xDA);
    }
    if (return_large_buffers_) {
      *size = output_.size() - position_;
    }
    return output_.data() + position_;
  }

  void ReleaseBuffer(size_t written_bytes) {
    // TODO(veluca): check no more bytes were written.
    Seek(position_ + written_bytes);
  }

  void Seek(uint64_t position) {
    EXPECT_GE(position, finalized_position_);
    position_ = position;
  }

  void SetFinalizedPosition(uint64_t finalized_position) {
    EXPECT_GE(finalized_position, finalized_position_);
    finalized_position_ = finalized_position;
    EXPECT_GE(position_, finalized_position_);
  }

  void CheckFinalWatermarkPosition() const {
    EXPECT_EQ(finalized_position_, position_);
  }

 private:
  std::vector<uint8_t> output_;
  size_t position_ = 0;
  size_t finalized_position_ = 0;
  bool return_large_buffers_;
};

class JxlChunkedFrameInputSourceAdapter {
 private:
  static const void* GetDataAt(const jxl::extras::PackedImage& image,
                               size_t xpos, size_t ypos, size_t* row_offset) {
    JxlDataType data_type = image.format.data_type;
    size_t num_channels = image.format.num_channels;
    size_t bytes_per_pixel =
        num_channels * jxl::extras::PackedImage::BitsPerChannel(data_type) / 8;
    *row_offset = image.stride;
    return static_cast<uint8_t*>(image.pixels()) + bytes_per_pixel * xpos +
           ypos * image.stride;
  }

 public:
  // Constructor to wrap the image data or any other state
  explicit JxlChunkedFrameInputSourceAdapter(
      jxl::extras::PackedImage color_channel,
      jxl::extras::PackedImage extra_channel)
      : colorchannel_(std::move(color_channel)),
        extra_channel_(std::move(extra_channel)) {}
  ~JxlChunkedFrameInputSourceAdapter() { EXPECT_TRUE(active_buffers_.empty()); }

  void GetColorChannelsPixelFormat(JxlPixelFormat* pixel_format) {
    *pixel_format = colorchannel_.format;
  }

  const void* GetColorChannelDataAt(size_t xpos, size_t ypos, size_t xsize,
                                    size_t ysize, size_t* row_offset) {
    const void* p = GetDataAt(colorchannel_, xpos, ypos, row_offset);
    std::lock_guard<std::mutex> lock(mtx_);
    active_buffers_.insert(p);
    return p;
  }

  void GetExtraChannelPixelFormat(size_t ec_index,
                                  JxlPixelFormat* pixel_format) {
    // In this test, we we the same color channel data, so `ec_index` is never
    // used
    *pixel_format = extra_channel_.format;
  }

  const void* GetExtraChannelDataAt(size_t ec_index, size_t xpos, size_t ypos,
                                    size_t xsize, size_t ysize,
                                    size_t* row_offset) {
    // In this test, we we the same color channel data, so `ec_index` is never
    // used
    const void* p = GetDataAt(extra_channel_, xpos, ypos, row_offset);
    std::lock_guard<std::mutex> lock(mtx_);
    active_buffers_.insert(p);
    return p;
  }
  void ReleaseCurrentData(const void* buffer) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto iter = active_buffers_.find(buffer);
    if (iter != active_buffers_.end()) {
      active_buffers_.erase(iter);
    }
  }

  JxlChunkedFrameInputSource GetInputSource() {
    return JxlChunkedFrameInputSource{
        this,
        METHOD_TO_C_CALLBACK(
            &JxlChunkedFrameInputSourceAdapter::GetColorChannelsPixelFormat),
        METHOD_TO_C_CALLBACK(
            &JxlChunkedFrameInputSourceAdapter::GetColorChannelDataAt),
        METHOD_TO_C_CALLBACK(
            &JxlChunkedFrameInputSourceAdapter::GetExtraChannelPixelFormat),
        METHOD_TO_C_CALLBACK(
            &JxlChunkedFrameInputSourceAdapter::GetExtraChannelDataAt),
        METHOD_TO_C_CALLBACK(
            &JxlChunkedFrameInputSourceAdapter::ReleaseCurrentData)};
  }

 private:
  const jxl::extras::PackedImage colorchannel_;
  const jxl::extras::PackedImage extra_channel_;
  std::mutex mtx_;
  std::set<const void*> active_buffers_;
};

struct StreamingTestParam {
  size_t bitmask;
  bool use_container() const { return static_cast<bool>(bitmask & 0x1); }
  bool return_large_buffers() const { return static_cast<bool>(bitmask & 0x2); }
  bool multiple_frames() const { return static_cast<bool>(bitmask & 0x4); }
  bool fast_lossless() const { return static_cast<bool>(bitmask & 0x8); }
  bool can_seek() const { return static_cast<bool>(bitmask & 0x10); }
  bool with_extra_channels() const { return static_cast<bool>(bitmask & 0x20); }
  bool color_includes_alpha() const {
    return static_cast<bool>(bitmask & 0x40);
  }
  bool onegroup() const { return static_cast<bool>(bitmask & 0x80); }

  bool is_lossless() const { return fast_lossless(); }

  static std::vector<StreamingTestParam> All() {
    std::vector<StreamingTestParam> params;
    for (size_t bitmask = 0; bitmask < 256; bitmask++) {
      params.push_back(StreamingTestParam{bitmask});
    }
    return params;
  }
};

std::ostream& operator<<(std::ostream& out, StreamingTestParam p) {
  if (p.use_container()) {
    out << "WithContainer_";
  } else {
    out << "WithoutContainer_";
  }
  if (p.return_large_buffers()) {
    out << "WithLargeBuffers_";
  } else {
    out << "WithSmallBuffers_";
  }
  if (p.multiple_frames()) out << "WithMultipleFrames_";
  if (p.fast_lossless()) out << "FastLossless_";
  if (!p.can_seek()) {
    out << "CannotSeek_";
  } else {
    out << "CanSeek_";
  }
  if (p.with_extra_channels()) {
    out << "WithExtraChannels_";
  } else {
    out << "WithoutExtraChannels_";
  }
  if (p.color_includes_alpha()) {
    out << "ColorIncludesAlpha_";
  } else {
    out << "ColorWithoutAlpha_";
  }
  if (p.onegroup()) {
    out << "OneGroup_";
  } else {
    out << "MultiGroup_";
  }
  return out;
}

}  // namespace

class EncoderStreamingTest : public testing::TestWithParam<StreamingTestParam> {
 public:
  static void SetupImage(const StreamingTestParam& p, size_t xsize,
                         size_t ysize, size_t num_channels,
                         size_t bits_per_sample, jxl::test::TestImage& image) {
    ASSERT_TRUE(image.SetDimensions(xsize, ysize));
    image.SetDataType(JXL_TYPE_UINT8);
    ASSERT_TRUE(image.SetChannels(num_channels));
    image.SetAllBitDepths(bits_per_sample);
    if (p.onegroup()) {
      image.SetRowAlignment(128);
    }
    JXL_TEST_ASSIGN_OR_DIE(auto frame, image.AddFrame());
    frame.RandomFill();
  }
  static void SetUpBasicInfo(JxlBasicInfo& basic_info, size_t xsize,
                             size_t ysize, size_t number_extra_channels,
                             bool include_alpha, bool is_lossless) {
    basic_info.xsize = xsize;
    basic_info.ysize = ysize;
    basic_info.num_extra_channels =
        number_extra_channels + (include_alpha ? 1 : 0);
    basic_info.uses_original_profile = TO_JXL_BOOL(is_lossless);
  }

  static void SetupEncoder(JxlEncoderFrameSettings* frame_settings,
                           const StreamingTestParam& p,
                           const JxlBasicInfo& basic_info,
                           size_t number_extra_channels, bool streaming) {
    JxlEncoderStruct* enc = frame_settings->enc;
    EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetBasicInfo(enc, &basic_info));
    if (p.fast_lossless()) {
      EXPECT_EQ(JXL_ENC_SUCCESS,
                JxlEncoderSetFrameLossless(frame_settings, JXL_TRUE));
      EXPECT_EQ(JXL_ENC_SUCCESS,
                JxlEncoderFrameSettingsSetOption(
                    frame_settings, JXL_ENC_FRAME_SETTING_EFFORT, 1));
    }
    JxlColorEncoding color_encoding;
    JxlColorEncodingSetToSRGB(&color_encoding, /*is_gray=*/JXL_FALSE);
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderSetColorEncoding(enc, &color_encoding));
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderFrameSettingsSetOption(frame_settings,
                                               JXL_ENC_FRAME_SETTING_BUFFERING,
                                               streaming ? 3 : 0));
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderFrameSettingsSetOption(
                  frame_settings,
                  JXL_ENC_FRAME_SETTING_USE_FULL_IMAGE_HEURISTICS, 0));
    if (p.use_container()) {
      EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetCodestreamLevel(enc, 10));
    }
    for (size_t i = 0; i < number_extra_channels; i++) {
      JxlExtraChannelInfo channel_info;
      JxlExtraChannelType channel_type = JXL_CHANNEL_THERMAL;
      JxlEncoderInitExtraChannelInfo(channel_type, &channel_info);
      EXPECT_EQ(JXL_ENC_SUCCESS,
                JxlEncoderSetExtraChannelInfo(enc, i, &channel_info));
    }
  }

  static void SetupInputNonStreaming(JxlEncoderFrameSettings* frame_settings,
                                     const StreamingTestParam& p,
                                     size_t number_extra_channels,
                                     const jxl::extras::PackedImage& frame,
                                     const jxl::extras::PackedImage& ec_frame) {
    size_t frame_count = static_cast<int>(p.multiple_frames()) + 1;
    for (size_t i = 0; i < frame_count; i++) {
      {
        // Copy pixel data here because it is only guaranteed to be available
        // during the call to JxlEncoderAddImageFrame().
        std::vector<uint8_t> pixels(frame.pixels_size);
        memcpy(pixels.data(), frame.pixels(), pixels.size());
        EXPECT_EQ(JXL_ENC_SUCCESS,
                  JxlEncoderAddImageFrame(frame_settings, &frame.format,
                                          pixels.data(), pixels.size()));
      }
      for (size_t i = 0; i < number_extra_channels; i++) {
        // Copy pixel data here because it is only guaranteed to be available
        // during the call to JxlEncoderSetExtraChannelBuffer().
        std::vector<uint8_t> ec_pixels(ec_frame.pixels_size);
        memcpy(ec_pixels.data(), ec_frame.pixels(), ec_pixels.size());
        EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetExtraChannelBuffer(
                                       frame_settings, &ec_frame.format,
                                       ec_pixels.data(), ec_pixels.size(), i));
      }
    }
    JxlEncoderCloseInput(frame_settings->enc);
  }

  static void SetupInputStreaming(JxlEncoderFrameSettings* frame_settings,
                                  const StreamingTestParam& p,
                                  size_t number_extra_channels,
                                  const jxl::extras::PackedImage& frame,
                                  const jxl::extras::PackedImage& ec_frame) {
    size_t frame_count = static_cast<int>(p.multiple_frames()) + 1;
    for (size_t i = 0; i < frame_count; i++) {
      // Create local copy of pixels and adapter because they are only
      // guarantted to be available during the JxlEncoderAddChunkedFrame() call.
      JxlChunkedFrameInputSourceAdapter chunked_frame_adapter(frame.Copy(),
                                                              ec_frame.Copy());
      EXPECT_EQ(JXL_ENC_SUCCESS,
                JxlEncoderAddChunkedFrame(
                    // should only set `JXL_TRUE` in the lass pass of the loop
                    frame_settings, i + 1 == frame_count ? JXL_TRUE : JXL_FALSE,
                    chunked_frame_adapter.GetInputSource()));
    }
  }
};

TEST_P(EncoderStreamingTest, OutputCallback) {
  const StreamingTestParam p = GetParam();
  size_t xsize = p.onegroup() ? 17 : 257;
  size_t ysize = p.onegroup() ? 19 : 259;
  size_t number_extra_channels = p.with_extra_channels() ? 5 : 0;
  jxl::test::TestImage image;
  SetupImage(p, xsize, ysize, p.color_includes_alpha() ? 4 : 3,
             p.use_container() ? 16 : 8, image);
  jxl::test::TestImage ec_image;
  SetupImage(p, xsize, ysize, 1, 8, ec_image);
  const auto& frame = image.ppf().frames[0].color;
  const auto& ec_frame = ec_image.ppf().frames[0].color;
  JxlBasicInfo basic_info = image.ppf().info;
  SetUpBasicInfo(basic_info, xsize, ysize, number_extra_channels,
                 p.color_includes_alpha(), p.is_lossless());

  std::vector<uint8_t> compressed = std::vector<uint8_t>(64);
  // without streaming
  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    ASSERT_NE(nullptr, enc.get());
    JxlEncoderFrameSettings* frame_settings =
        JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
    SetupEncoder(frame_settings, p, basic_info, number_extra_channels, false);
    SetupInputNonStreaming(frame_settings, p, number_extra_channels, frame,
                           ec_frame);
    uint8_t* next_out = compressed.data();
    size_t avail_out = compressed.size();
    ProcessEncoder(enc.get(), compressed, next_out, avail_out);
  }

  std::vector<uint8_t> streaming_compressed;
  // with streaming
  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    ASSERT_NE(nullptr, enc.get());
    JxlEncoderFrameSettings* frame_settings =
        JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
    SetupEncoder(frame_settings, p, basic_info, number_extra_channels, true);
    SetupInputNonStreaming(frame_settings, p, number_extra_channels, frame,
                           ec_frame);
    JxlStreamingAdapter streaming_adapter(enc.get(), p.return_large_buffers(),
                                          p.can_seek());
    EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderFlushInput(enc.get()));
    streaming_adapter.CheckFinalWatermarkPosition();
    streaming_compressed = std::move(streaming_adapter).output();
  }

  EXPECT_TRUE(SameDecodedPixels(compressed, streaming_compressed));
  EXPECT_LE(streaming_compressed.size(), compressed.size() + 1024);
}

TEST_P(EncoderStreamingTest, ChunkedFrame) {
  const StreamingTestParam p = GetParam();
  size_t xsize = p.onegroup() ? 17 : 257;
  size_t ysize = p.onegroup() ? 19 : 259;
  size_t number_extra_channels = p.with_extra_channels() ? 5 : 0;
  jxl::test::TestImage image;
  SetupImage(p, xsize, ysize, p.color_includes_alpha() ? 4 : 3,
             p.use_container() ? 16 : 8, image);
  jxl::test::TestImage ec_image;
  SetupImage(p, xsize, ysize, 1, 8, ec_image);
  const auto& frame = image.ppf().frames[0].color;
  const auto& ec_frame = ec_image.ppf().frames[0].color;
  JxlBasicInfo basic_info = image.ppf().info;
  SetUpBasicInfo(basic_info, xsize, ysize, number_extra_channels,
                 p.color_includes_alpha(), p.is_lossless());
  std::vector<uint8_t> compressed = std::vector<uint8_t>(64);
  std::vector<uint8_t> streaming_compressed = std::vector<uint8_t>(64);

  // without streaming
  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    ASSERT_NE(nullptr, enc.get());
    JxlEncoderFrameSettings* frame_settings =
        JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
    SetupEncoder(frame_settings, p, basic_info, number_extra_channels, false);
    SetupInputNonStreaming(frame_settings, p, number_extra_channels, frame,
                           ec_frame);
    uint8_t* next_out = compressed.data();
    size_t avail_out = compressed.size();
    ProcessEncoder(enc.get(), compressed, next_out, avail_out);
  }

  // with streaming
  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    ASSERT_NE(nullptr, enc.get());
    JxlEncoderFrameSettings* frame_settings =
        JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
    SetupEncoder(frame_settings, p, basic_info, number_extra_channels, true);
    SetupInputStreaming(frame_settings, p, number_extra_channels, frame,
                        ec_frame);
    uint8_t* next_out = streaming_compressed.data();
    size_t avail_out = streaming_compressed.size();
    ProcessEncoder(enc.get(), streaming_compressed, next_out, avail_out);
  }

  EXPECT_TRUE(SameDecodedPixels(compressed, streaming_compressed));
  EXPECT_LE(streaming_compressed.size(), compressed.size() + 1024);
}

TEST_P(EncoderStreamingTest, ChunkedAndOutputCallback) {
  const StreamingTestParam p = GetParam();
  size_t xsize = p.onegroup() ? 17 : 257;
  size_t ysize = p.onegroup() ? 19 : 259;
  size_t number_extra_channels = p.with_extra_channels() ? 5 : 0;
  jxl::test::TestImage image;
  SetupImage(p, xsize, ysize, p.color_includes_alpha() ? 4 : 3,
             p.use_container() ? 16 : 8, image);
  jxl::test::TestImage ec_image;
  SetupImage(p, xsize, ysize, 1, 8, ec_image);
  const auto& frame = image.ppf().frames[0].color;
  const auto& ec_frame = ec_image.ppf().frames[0].color;
  JxlBasicInfo basic_info = image.ppf().info;
  SetUpBasicInfo(basic_info, xsize, ysize, number_extra_channels,
                 p.color_includes_alpha(), p.is_lossless());

  std::vector<uint8_t> compressed = std::vector<uint8_t>(64);

  // without streaming
  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    ASSERT_NE(nullptr, enc.get());
    JxlEncoderFrameSettings* frame_settings =
        JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
    SetupEncoder(frame_settings, p, basic_info, number_extra_channels, false);
    SetupInputNonStreaming(frame_settings, p, number_extra_channels, frame,
                           ec_frame);
    uint8_t* next_out = compressed.data();
    size_t avail_out = compressed.size();
    ProcessEncoder(enc.get(), compressed, next_out, avail_out);
  }

  std::vector<uint8_t> streaming_compressed;
  // with streaming
  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    ASSERT_NE(nullptr, enc.get());
    JxlEncoderFrameSettings* frame_settings =
        JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
    SetupEncoder(frame_settings, p, basic_info, number_extra_channels, true);
    JxlStreamingAdapter streaming_adapter =
        JxlStreamingAdapter(enc.get(), p.return_large_buffers(), p.can_seek());
    SetupInputStreaming(frame_settings, p, number_extra_channels, frame,
                        ec_frame);
    streaming_adapter.CheckFinalWatermarkPosition();
    streaming_compressed = std::move(streaming_adapter).output();
  }

  EXPECT_TRUE(SameDecodedPixels(compressed, streaming_compressed));
  EXPECT_LE(streaming_compressed.size(), compressed.size() + 1024);
}

JXL_GTEST_INSTANTIATE_TEST_SUITE_P(
    EncoderStreamingTest, EncoderStreamingTest,
    testing::ValuesIn(StreamingTestParam::All()));

TEST(EncoderTest, CMYK) {
  size_t xsize = 257;
  size_t ysize = 259;
  jxl::test::TestImage image;
  ASSERT_TRUE(image.SetDimensions(xsize, ysize));
  image.SetDataType(JXL_TYPE_UINT8);
  ASSERT_TRUE(image.SetChannels(3));
  image.SetAllBitDepths(8);
  JXL_TEST_ASSIGN_OR_DIE(auto frame0, image.AddFrame());
  frame0.RandomFill();
  jxl::test::TestImage ec_image;
  ec_image.SetDataType(JXL_TYPE_UINT8);
  ASSERT_TRUE(ec_image.SetDimensions(xsize, ysize));
  ASSERT_TRUE(ec_image.SetChannels(1));
  ec_image.SetAllBitDepths(8);
  JXL_TEST_ASSIGN_OR_DIE(auto frame1, ec_image.AddFrame());
  frame1.RandomFill();
  const auto& frame = image.ppf().frames[0].color;
  const auto& ec_frame = ec_image.ppf().frames[0].color;
  JxlBasicInfo basic_info = image.ppf().info;
  basic_info.xsize = xsize;
  basic_info.ysize = ysize;
  basic_info.num_extra_channels = 1;
  basic_info.uses_original_profile = JXL_TRUE;

  std::vector<uint8_t> compressed = std::vector<uint8_t>(64);
  JxlEncoderPtr enc_ptr = JxlEncoderMake(nullptr);
  JxlEncoderStruct* enc = enc_ptr.get();
  ASSERT_NE(nullptr, enc);
  JxlEncoderFrameSettings* frame_settings =
      JxlEncoderFrameSettingsCreate(enc, nullptr);

  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetBasicInfo(enc, &basic_info));
  JxlExtraChannelInfo channel_info;
  JxlExtraChannelType channel_type = JXL_CHANNEL_BLACK;
  JxlEncoderInitExtraChannelInfo(channel_type, &channel_info);
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderSetExtraChannelInfo(enc, 0, &channel_info));
  const std::vector<uint8_t> icc = jxl::test::ReadTestData(
      "external/Compact-ICC-Profiles/profiles/"
      "CGATS001Compat-v2-micro.icc");
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderSetICCProfile(enc, icc.data(), icc.size()));
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderAddImageFrame(frame_settings, &frame.format,
                                    frame.pixels(), frame.pixels_size));
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetExtraChannelBuffer(
                                 frame_settings, &ec_frame.format,
                                 ec_frame.pixels(), ec_frame.pixels_size, 0));
  JxlEncoderCloseInput(frame_settings->enc);
  uint8_t* next_out = compressed.data();
  size_t avail_out = compressed.size();
  ProcessEncoder(enc, compressed, next_out, avail_out);

  jxl::extras::JXLDecompressParams dparams;
  dparams.accepted_formats = {
      {3, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, 0},
  };
  jxl::extras::PackedPixelFile ppf;
  EXPECT_TRUE(DecodeImageJXL(compressed.data(), compressed.size(), dparams,
                             nullptr, &ppf, nullptr));
}
