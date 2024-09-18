// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/dec/decode.h"

#include <jxl/cms.h>
#include <jxl/codestream_header.h>
#include <jxl/color_encoding.h>
#include <jxl/decode.h>
#include <jxl/decode_cxx.h>
#include <jxl/memory_manager.h>
#include <jxl/parallel_runner.h>
#include <jxl/resizable_parallel_runner.h>
#include <jxl/resizable_parallel_runner_cxx.h>
#include <jxl/thread_parallel_runner.h>
#include <jxl/thread_parallel_runner_cxx.h>
#include <jxl/types.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "lib/extras/dec/color_description.h"
#include "lib/extras/enc/encode.h"
#include "lib/extras/enc/jpg.h"
#include "lib/extras/packed_image.h"
#include "lib/jxl/base/byte_order.h"
#include "lib/jxl/base/common.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/override.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/butteraugli/butteraugli.h"
#include "lib/jxl/cms/color_encoding_cms.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/common.h"  // SpeedTier
#include "lib/jxl/dec_bit_reader.h"
#include "lib/jxl/dec_external_image.h"
#include "lib/jxl/enc_aux_out.h"
#include "lib/jxl/enc_external_image.h"
#include "lib/jxl/enc_fields.h"
#include "lib/jxl/enc_frame.h"
#include "lib/jxl/enc_icc_codec.h"
#include "lib/jxl/enc_params.h"
#include "lib/jxl/enc_progressive_split.h"
#include "lib/jxl/encode_internal.h"
#include "lib/jxl/fields.h"
#include "lib/jxl/frame_dimensions.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/headers.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/image_metadata.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/jpeg/enc_jpeg_data.h"
#include "lib/jxl/jpeg/jpeg_data.h"
#include "lib/jxl/padded_bytes.h"
#include "lib/jxl/test_image.h"
#include "lib/jxl/test_memory_manager.h"
#include "lib/jxl/test_utils.h"
#include "lib/jxl/testing.h"
#include "lib/jxl/toc.h"
using ::jxl::test::GetIccTestProfile;
////////////////////////////////////////////////////////////////////////////////

namespace {
void AppendU32BE(uint32_t u32, std::vector<uint8_t>* bytes) {
  bytes->push_back(u32 >> 24);
  bytes->push_back(u32 >> 16);
  bytes->push_back(u32 >> 8);
  bytes->push_back(u32 >> 0);
}

// What type of codestream format in the boxes to use for testing
enum CodeStreamBoxFormat {
  // Do not use box format at all, only pure codestream
  kCSBF_None,
  // Have a single codestream box, with its actual size given in the box
  kCSBF_Single,
  // Have a single codestream box, with box size 0 (final box running to end)
  kCSBF_Single_Zero_Terminated,
  // Single codestream box, with another unknown box behind it
  kCSBF_Single_Other,
  // Have multiple partial codestream boxes
  kCSBF_Multi,
  // Have multiple partial codestream boxes, with final box size 0 (running
  // to end)
  kCSBF_Multi_Zero_Terminated,
  // Have multiple partial codestream boxes, terminated by non-codestream box
  kCSBF_Multi_Other_Terminated,
  // Have multiple partial codestream boxes, terminated by non-codestream box
  // that has its size set to 0 (running to end)
  kCSBF_Multi_Other_Zero_Terminated,
  // Have multiple partial codestream boxes, and the first one has a content
  // of zero length
  kCSBF_Multi_First_Empty,
  // Have multiple partial codestream boxes, and the last one has a content
  // of zero length and there is an unknown empty box at the end
  kCSBF_Multi_Last_Empty_Other,
  // Have a compressed exif box before a regular codestream box
  kCSBF_Brob_Exif,
  // Not a value but used for counting amount of enum entries
  kCSBF_NUM_ENTRIES,
};

// Unknown boxes for testing
const char* unk1_box_type = "unk1";
const char* unk1_box_contents = "abcdefghijklmnopqrstuvwxyz";
const size_t unk1_box_size = strlen(unk1_box_contents);
const char* unk2_box_type = "unk2";
const char* unk2_box_contents = "0123456789";
const size_t unk2_box_size = strlen(unk2_box_contents);
const char* unk3_box_type = "unk3";
const char* unk3_box_contents = "ABCDEF123456";
const size_t unk3_box_size = strlen(unk3_box_contents);
// Box with brob-compressed exif, including header
const uint8_t* box_brob_exif = reinterpret_cast<const uint8_t*>(
    "\0\0\0@brobExif\241\350\2\300\177\244v\2525\304\360\27=?\267{"
    "\33\37\314\332\214QX17PT\"\256\0\0\202s\214\313t\333\310\320k\20\276\30"
    "\204\277l$\326c#\1\b");
size_t box_brob_exif_size = 64;
// The uncompressed Exif data from the brob box
const uint8_t* exif_uncompressed = reinterpret_cast<const uint8_t*>(
    "\0\0\0\0MM\0*"
    "\0\0\0\b\0\5\1\22\0\3\0\0\0\1\0\5\0\0\1\32\0\5\0\0\0\1\0\0\0J\1\33\0\5\0\0"
    "\0\1\0\0\0R\1("
    "\0\3\0\0\0\1\0\1\0\0\2\23\0\3\0\0\0\1\0\1\0\0\0\0\0\0\0\0\0\1\0\0\0\1\0\0"
    "\0\1\0\0\0\1");
size_t exif_uncompressed_size = 94;

}  // namespace

namespace jxl {
namespace {

void AppendTestBox(const char* type, const char* contents, size_t contents_size,
                   bool unbounded, std::vector<uint8_t>* bytes) {
  AppendU32BE(contents_size + 8, bytes);
  bytes->push_back(type[0]);
  bytes->push_back(type[1]);
  bytes->push_back(type[2]);
  bytes->push_back(type[3]);
  const uint8_t* contents_u = reinterpret_cast<const uint8_t*>(contents);
  Bytes(contents_u, contents_size).AppendTo(*bytes);
}

enum PreviewMode {
  kNoPreview,
  kSmallPreview,
  kBigPreview,
  kNumPreviewModes,
};

void GeneratePreview(PreviewMode preview_mode, ImageBundle* ib) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  if (preview_mode == kSmallPreview) {
    ASSERT_TRUE(ib->ShrinkTo(ib->xsize() / 7, ib->ysize() / 7));
  } else if (preview_mode == kBigPreview) {
    auto upsample7 = [&](const ImageF& in, ImageF* out) {
      for (size_t y = 0; y < out->ysize(); ++y) {
        for (size_t x = 0; x < out->xsize(); ++x) {
          out->Row(y)[x] = in.ConstRow(y / 7)[x / 7];
        }
      }
    };
    JXL_TEST_ASSIGN_OR_DIE(
        Image3F preview,
        Image3F::Create(memory_manager, ib->xsize() * 7, ib->ysize() * 7));
    for (size_t c = 0; c < 3; ++c) {
      upsample7(ib->color()->Plane(c), &preview.Plane(c));
    }
    std::vector<ImageF> extra_channels;
    for (size_t i = 0; i < ib->extra_channels().size(); ++i) {
      JXL_TEST_ASSIGN_OR_DIE(
          ImageF ec,
          ImageF::Create(memory_manager, ib->xsize() * 7, ib->ysize() * 7));
      upsample7(ib->extra_channels()[i], &ec);
      extra_channels.emplace_back(std::move(ec));
    }
    ib->RemoveColor();
    ib->ClearExtraChannels();
    ASSERT_TRUE(ib->SetFromImage(std::move(preview), ib->c_current()));
    ASSERT_TRUE(ib->SetExtraChannels(std::move(extra_channels)));
  }
}

struct TestCodestreamParams {
  CompressParams cparams;
  CodeStreamBoxFormat box_format = kCSBF_None;
  JxlOrientation orientation = JXL_ORIENT_IDENTITY;
  PreviewMode preview_mode = kNoPreview;
  bool add_intrinsic_size = false;
  bool add_icc_profile = false;
  float intensity_target = 0.0;
  std::string color_space;
  std::vector<uint8_t>* jpeg_codestream = nullptr;
};

// Input pixels always given as 16-bit RGBA, 8 bytes per pixel.
// include_alpha determines if the encoded image should contain the alpha
// channel.
// add_icc_profile: if false, encodes the image as sRGB using the JXL fields,
// for grayscale or RGB images. If true, encodes the image using the ICC profile
// returned by GetIccTestProfile, without the JXL fields, this requires the
// image is RGB, not grayscale.
// Providing jpeg_codestream will populate the jpeg_codestream with compressed
// JPEG bytes, and make it possible to reconstruct those exact JPEG bytes using
// the return value _if_ add_container indicates a box format.
std::vector<uint8_t> CreateTestJXLCodestream(
    Span<const uint8_t> pixels, size_t xsize, size_t ysize, size_t num_channels,
    const TestCodestreamParams& params) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  // Compress the pixels with JPEG XL.
  bool grayscale = (num_channels <= 2);
  bool have_alpha = ((num_channels & 1) == 0);
  bool include_alpha = have_alpha && params.jpeg_codestream == nullptr;
  size_t bitdepth = params.jpeg_codestream == nullptr ? 16 : 8;
  CodecInOut io{jxl::test::MemoryManager()};
  EXPECT_TRUE(io.SetSize(xsize, ysize));
  ColorEncoding color_encoding;
  if (params.add_icc_profile) {
    // the hardcoded ICC profile we attach requires RGB.
    EXPECT_EQ(false, grayscale);
    EXPECT_TRUE(params.color_space.empty());
    EXPECT_TRUE(color_encoding.SetICC(GetIccTestProfile(), JxlGetDefaultCms()));
  } else if (!params.color_space.empty()) {
    JxlColorEncoding c;
    EXPECT_TRUE(jxl::ParseDescription(params.color_space, &c));
    EXPECT_TRUE(color_encoding.FromExternal(c));
    EXPECT_EQ(color_encoding.IsGray(), grayscale);
  } else {
    color_encoding = jxl::ColorEncoding::SRGB(/*is_gray=*/grayscale);
  }
  io.metadata.m.SetUintSamples(bitdepth);
  if (include_alpha) {
    io.metadata.m.SetAlphaBits(bitdepth);
  }
  if (params.intensity_target != 0) {
    io.metadata.m.SetIntensityTarget(params.intensity_target);
  }
  JxlPixelFormat format = {static_cast<uint32_t>(num_channels), JXL_TYPE_UINT16,
                           JXL_BIG_ENDIAN, 0};
  // Make the grayscale-ness of the io metadata color_encoding and the packed
  // image match.
  io.metadata.m.color_encoding = color_encoding;
  EXPECT_TRUE(ConvertFromExternal(pixels, xsize, ysize, color_encoding,
                                  /*bits_per_sample=*/16, format,
                                  /* pool */ nullptr, &io.Main()));
  std::vector<uint8_t> jpeg_data;
  if (params.jpeg_codestream != nullptr) {
    if (jxl::extras::CanDecode(jxl::extras::Codec::kJPG)) {
      std::vector<uint8_t> jpeg_bytes;
      extras::PackedPixelFile ppf;
      JXL_TEST_ASSIGN_OR_DIE(extras::PackedFrame frame,
                             extras::PackedFrame::Create(xsize, ysize, format));
      EXPECT_TRUE(frame.color.pixels_size == pixels.size());
      memcpy(frame.color.pixels(0, 0, 0), pixels.data(), pixels.size());
      ppf.frames.emplace_back(std::move(frame));
      ppf.info.xsize = xsize;
      ppf.info.ysize = ysize;
      ppf.info.num_color_channels = grayscale ? 1 : 3;
      ppf.info.bits_per_sample = 16;
      auto encoder = extras::GetJPEGEncoder();
      encoder->SetOption("quality", "70");
      extras::EncodedImage encoded;
      EXPECT_TRUE(encoder->Encode(ppf, &encoded, nullptr));
      jpeg_bytes = encoded.bitstreams[0];
      Bytes(jpeg_bytes).AppendTo(*params.jpeg_codestream);
      EXPECT_TRUE(jxl::jpeg::DecodeImageJPG(
          jxl::Bytes(jpeg_bytes.data(), jpeg_bytes.size()), &io));
      EXPECT_TRUE(EncodeJPEGData(memory_manager, *io.Main().jpeg_data,
                                 &jpeg_data, params.cparams));
      io.metadata.m.xyb_encoded = false;
    } else {
      ADD_FAILURE();
    }
  }
  if (params.preview_mode) {
    JXL_TEST_ASSIGN_OR_DIE(io.preview_frame, io.Main().Copy());
    GeneratePreview(params.preview_mode, &io.preview_frame);
    io.metadata.m.have_preview = true;
    EXPECT_TRUE(io.metadata.m.preview_size.Set(io.preview_frame.xsize(),
                                               io.preview_frame.ysize()));
  }
  if (params.add_intrinsic_size) {
    EXPECT_TRUE(io.metadata.m.intrinsic_size.Set(xsize / 3, ysize / 3));
  }
  io.metadata.m.orientation = params.orientation;
  std::vector<uint8_t> compressed;
  EXPECT_TRUE(test::EncodeFile(params.cparams, &io, &compressed));
  CodeStreamBoxFormat add_container = params.box_format;
  if (add_container != kCSBF_None) {
    // Header with signature box and ftyp box.
    const uint8_t header[] = {0,    0,    0,    0xc,  0x4a, 0x58, 0x4c, 0x20,
                              0xd,  0xa,  0x87, 0xa,  0,    0,    0,    0x14,
                              0x66, 0x74, 0x79, 0x70, 0x6a, 0x78, 0x6c, 0x20,
                              0,    0,    0,    0,    0x6a, 0x78, 0x6c, 0x20};

    bool is_multi = add_container == kCSBF_Multi ||
                    add_container == kCSBF_Multi_Zero_Terminated ||
                    add_container == kCSBF_Multi_Other_Terminated ||
                    add_container == kCSBF_Multi_Other_Zero_Terminated ||
                    add_container == kCSBF_Multi_First_Empty ||
                    add_container == kCSBF_Multi_Last_Empty_Other;

    if (is_multi) {
      size_t third = compressed.size() / 3;
      std::vector<uint8_t> compressed0(compressed.data(),
                                       compressed.data() + third);
      std::vector<uint8_t> compressed1(compressed.data() + third,
                                       compressed.data() + 2 * third);
      std::vector<uint8_t> compressed2(compressed.data() + 2 * third,
                                       compressed.data() + compressed.size());

      std::vector<uint8_t> c;
      Bytes(header).AppendTo(c);
      if (params.jpeg_codestream != nullptr) {
        jxl::AppendBoxHeader(jxl::MakeBoxType("jbrd"), jpeg_data.size(), false,
                             &c);
        Bytes(jpeg_data).AppendTo(c);
      }
      uint32_t jxlp_index = 0;
      if (add_container == kCSBF_Multi_First_Empty) {
        // Empty placeholder codestream part
        AppendU32BE(12, &c);
        c.push_back('j');
        c.push_back('x');
        c.push_back('l');
        c.push_back('p');
        AppendU32BE(jxlp_index++, &c);
      }
      // First codestream part
      AppendU32BE(compressed0.size() + 12, &c);
      c.push_back('j');
      c.push_back('x');
      c.push_back('l');
      c.push_back('p');
      AppendU32BE(jxlp_index++, &c);
      Bytes(compressed0).AppendTo(c);
      // A few non-codestream boxes in between
      AppendTestBox(unk1_box_type, unk1_box_contents, unk1_box_size, false, &c);
      AppendTestBox(unk2_box_type, unk2_box_contents, unk2_box_size, false, &c);
      // Empty placeholder codestream part
      AppendU32BE(12, &c);
      c.push_back('j');
      c.push_back('x');
      c.push_back('l');
      c.push_back('p');
      AppendU32BE(jxlp_index++, &c);
      // Second codestream part
      AppendU32BE(compressed1.size() + 12, &c);
      c.push_back('j');
      c.push_back('x');
      c.push_back('l');
      c.push_back('p');
      AppendU32BE(jxlp_index++, &c);
      Bytes(compressed1).AppendTo(c);
      // Third (last) codestream part
      AppendU32BE(add_container == kCSBF_Multi_Zero_Terminated
                      ? 0
                      : (compressed2.size() + 12),
                  &c);
      c.push_back('j');
      c.push_back('x');
      c.push_back('l');
      c.push_back('p');
      if (add_container != kCSBF_Multi_Last_Empty_Other) {
        AppendU32BE(jxlp_index++ | 0x80000000, &c);
      } else {
        AppendU32BE(jxlp_index++, &c);
      }
      Bytes(compressed2).AppendTo(c);
      if (add_container == kCSBF_Multi_Last_Empty_Other) {
        // Empty placeholder codestream part
        AppendU32BE(12, &c);
        c.push_back('j');
        c.push_back('x');
        c.push_back('l');
        c.push_back('p');
        AppendU32BE(jxlp_index++ | 0x80000000, &c);
        AppendTestBox(unk3_box_type, unk3_box_contents, unk3_box_size, false,
                      &c);
      }
      if (add_container == kCSBF_Multi_Other_Terminated) {
        AppendTestBox(unk3_box_type, unk3_box_contents, unk3_box_size, false,
                      &c);
      }
      if (add_container == kCSBF_Multi_Other_Zero_Terminated) {
        AppendTestBox(unk3_box_type, unk3_box_contents, unk3_box_size, true,
                      &c);
      }
      compressed.swap(c);
    } else {
      std::vector<uint8_t> c;
      Bytes(header).AppendTo(c);
      if (params.jpeg_codestream != nullptr) {
        jxl::AppendBoxHeader(jxl::MakeBoxType("jbrd"), jpeg_data.size(), false,
                             &c);
        Bytes(jpeg_data).AppendTo(c);
      }
      if (add_container == kCSBF_Brob_Exif) {
        Bytes(box_brob_exif, box_brob_exif_size).AppendTo(c);
      }
      AppendU32BE(add_container == kCSBF_Single_Zero_Terminated
                      ? 0
                      : (compressed.size() + 8),
                  &c);
      c.push_back('j');
      c.push_back('x');
      c.push_back('l');
      c.push_back('c');
      Bytes(compressed).AppendTo(c);
      if (add_container == kCSBF_Single_Other) {
        AppendTestBox(unk1_box_type, unk1_box_contents, unk1_box_size, false,
                      &c);
      }
      compressed.swap(c);
    }
  }

  return compressed;
}

JxlDecoderStatus ProcessInputIgnoreBoxes(JxlDecoder* dec) {
  JxlDecoderStatus status = JXL_DEC_BOX;
  while (status == JXL_DEC_BOX) {
    status = JxlDecoderProcessInput(dec);
  }
  return status;
}

// Decodes one-shot with the API for non-streaming decoding tests.
std::vector<uint8_t> DecodeWithAPI(JxlDecoder* dec,
                                   Span<const uint8_t> compressed,
                                   const JxlPixelFormat& format,
                                   bool use_callback, bool set_buffer_early,
                                   bool use_resizable_runner,
                                   bool require_boxes, bool expect_success,
                                   std::vector<uint8_t>* icc = nullptr) {
  JxlThreadParallelRunnerPtr runner_fixed;
  JxlResizableParallelRunnerPtr runner_resizable;
  JxlParallelRunner runner_fn;
  void* runner;

  if (use_resizable_runner) {
    runner_resizable = JxlResizableParallelRunnerMake(nullptr);
    runner = runner_resizable.get();
    runner_fn = JxlResizableParallelRunner;
  } else {
    size_t hw_threads = JxlThreadParallelRunnerDefaultNumWorkerThreads();
    runner_fixed =
        JxlThreadParallelRunnerMake(nullptr, std::min<size_t>(hw_threads, 16));
    runner = runner_fixed.get();
    runner_fn = JxlThreadParallelRunner;
  }
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetParallelRunner(dec, runner_fn, runner));

  auto process_input =
      require_boxes ? ProcessInputIgnoreBoxes : JxlDecoderProcessInput;

  EXPECT_EQ(
      JXL_DEC_SUCCESS,
      JxlDecoderSubscribeEvents(
          dec, JXL_DEC_BASIC_INFO | (set_buffer_early ? JXL_DEC_FRAME : 0) |
                   JXL_DEC_PREVIEW_IMAGE | JXL_DEC_FULL_IMAGE |
                   (require_boxes ? JXL_DEC_BOX : 0) |
                   (icc != nullptr ? JXL_DEC_COLOR_ENCODING : 0)));

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetInput(dec, compressed.data(), compressed.size()));
  EXPECT_EQ(JXL_DEC_BASIC_INFO, process_input(dec));
  size_t buffer_size;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderImageOutBufferSize(dec, &format, &buffer_size));
  JxlBasicInfo info;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
  if (use_resizable_runner) {
    JxlResizableParallelRunnerSetThreads(
        runner,
        JxlResizableParallelRunnerSuggestThreads(info.xsize, info.ysize));
  }

  std::vector<uint8_t> pixels(buffer_size);
  size_t bytes_per_pixel = format.num_channels *
                           test::GetDataBits(format.data_type) /
                           jxl::kBitsPerByte;
  size_t stride = bytes_per_pixel * info.xsize;
  if (format.align > 1) {
    stride = jxl::DivCeil(stride, format.align) * format.align;
  }
  auto callback = [&](size_t x, size_t y, size_t num_pixels,
                      const void* pixels_row) {
    memcpy(pixels.data() + stride * y + bytes_per_pixel * x, pixels_row,
           num_pixels * bytes_per_pixel);
  };

  JxlDecoderStatus status = process_input(dec);

  if (status == JXL_DEC_COLOR_ENCODING) {
    size_t icc_size = 0;
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderGetICCProfileSize(dec, JXL_COLOR_PROFILE_TARGET_DATA,
                                          &icc_size));
    icc->resize(icc_size);
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderGetColorAsICCProfile(dec, JXL_COLOR_PROFILE_TARGET_DATA,
                                             icc->data(), icc_size));

    status = process_input(dec);
  }

  std::vector<uint8_t> preview;
  if (status == JXL_DEC_NEED_PREVIEW_OUT_BUFFER) {
    size_t buffer_size;
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderPreviewOutBufferSize(dec, &format, &buffer_size));
    preview.resize(buffer_size);
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderSetPreviewOutBuffer(dec, &format, preview.data(),
                                            preview.size()));
    EXPECT_EQ(JXL_DEC_PREVIEW_IMAGE, process_input(dec));

    status = process_input(dec);
  }

  if (set_buffer_early) {
    EXPECT_EQ(JXL_DEC_FRAME, status);
  } else {
    EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, status);
  }

  if (use_callback) {
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderSetImageOutCallback(
                  dec, &format,
                  [](void* opaque, size_t x, size_t y, size_t xsize,
                     const void* pixels_row) {
                    auto cb = static_cast<decltype(&callback)>(opaque);
                    (*cb)(x, y, xsize, pixels_row);
                  },
                  /*opaque=*/&callback));
  } else {
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                   dec, &format, pixels.data(), pixels.size()));
  }

  EXPECT_EQ(JXL_DEC_FULL_IMAGE, process_input(dec));

  // After the full image was output, JxlDecoderProcessInput should return
  // success to indicate all is done, unless we requested boxes and the last
  // box was not a terminal unbounded box, in which case it should ask for
  // more input.
  JxlDecoderStatus expected_status =
      expect_success ? JXL_DEC_SUCCESS : JXL_DEC_NEED_MORE_INPUT;
  EXPECT_EQ(expected_status, process_input(dec));

  return pixels;
}

// Decodes one-shot with the API for non-streaming decoding tests.
std::vector<uint8_t> DecodeWithAPI(Span<const uint8_t> compressed,
                                   const JxlPixelFormat& format,
                                   bool use_callback, bool set_buffer_early,
                                   bool use_resizable_runner,
                                   bool require_boxes, bool expect_success) {
  JxlDecoder* dec = JxlDecoderCreate(nullptr);
  std::vector<uint8_t> pixels =
      DecodeWithAPI(dec, compressed, format, use_callback, set_buffer_early,
                    use_resizable_runner, require_boxes, expect_success);
  JxlDecoderDestroy(dec);
  return pixels;
}

}  // namespace
}  // namespace jxl

////////////////////////////////////////////////////////////////////////////////

using ::jxl::Image3F;
using ::jxl::ImageF;
using ::jxl::test::BoolToCStr;
using ::jxl::test::ButteraugliDistance;

TEST(DecodeTest, JxlSignatureCheckTest) {
  std::vector<std::pair<int, std::vector<uint8_t>>> tests = {
      // No JPEGXL header starts with 'a'.
      {JXL_SIG_INVALID, {'a'}},
      {JXL_SIG_INVALID, {'a', 'b', 'c', 'd', 'e', 'f'}},

      // Empty file is not enough bytes.
      {JXL_SIG_NOT_ENOUGH_BYTES, {}},

      // JPEGXL headers.
      {JXL_SIG_NOT_ENOUGH_BYTES, {0xff}},  // Part of a signature.
      {JXL_SIG_INVALID, {0xff, 0xD8}},     // JPEG-1
      {JXL_SIG_CODESTREAM, {0xff, 0x0a}},

      // JPEGXL container file.
      {JXL_SIG_CONTAINER,
       {0, 0, 0, 0xc, 'J', 'X', 'L', ' ', 0xD, 0xA, 0x87, 0xA}},
      // Ending with invalid byte.
      {JXL_SIG_INVALID, {0, 0, 0, 0xc, 'J', 'X', 'L', ' ', 0xD, 0xA, 0x87, 0}},
      // Part of signature.
      {JXL_SIG_NOT_ENOUGH_BYTES,
       {0, 0, 0, 0xc, 'J', 'X', 'L', ' ', 0xD, 0xA, 0x87}},
      {JXL_SIG_NOT_ENOUGH_BYTES, {0}},
  };
  for (const auto& test : tests) {
    EXPECT_EQ(test.first,
              JxlSignatureCheck(test.second.data(), test.second.size()))
        << "Where test data is " << ::testing::PrintToString(test.second);
  }
}

TEST(DecodeTest, DefaultAllocTest) {
  JxlDecoder* dec = JxlDecoderCreate(nullptr);
  EXPECT_NE(nullptr, dec);
  JxlDecoderDestroy(dec);
}

TEST(DecodeTest, CustomAllocTest) {
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

  JxlDecoder* dec = JxlDecoderCreate(&mm);
  EXPECT_NE(nullptr, dec);
  EXPECT_LE(1, counters.allocs);
  EXPECT_EQ(0, counters.frees);
  JxlDecoderDestroy(dec);
  EXPECT_LE(1, counters.frees);
}

// TODO(lode): add multi-threaded test when multithreaded pixel decoding from
// API is implemented.
TEST(DecodeTest, DefaultParallelRunnerTest) {
  JxlDecoder* dec = JxlDecoderCreate(nullptr);
  EXPECT_NE(nullptr, dec);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetParallelRunner(dec, nullptr, nullptr));
  JxlDecoderDestroy(dec);
}

// Creates the header of a JPEG XL file with various custom parameters for
// testing.
// xsize, ysize: image dimensions to store in the SizeHeader, max 512.
// bits_per_sample, orientation: a selection of header parameters to test with.
// orientation: image orientation to set in the metadata
// alpha_bits: if non-0, alpha extra channel bits to set in the metadata. Also
//   gives the alpha channel the name "alpha_test"
// have_container: add box container format around the codestream.
// metadata_default: if true, ImageMetadata is set to default and
//   bits_per_sample, orientation and alpha_bits are ignored.
// insert_box: insert an extra box before the codestream box, making the header
// farther away from the front than is ideal. Only used if have_container.
std::vector<uint8_t> GetTestHeader(size_t xsize, size_t ysize,
                                   size_t bits_per_sample, size_t orientation,
                                   size_t alpha_bits, bool xyb_encoded,
                                   bool have_container, bool metadata_default,
                                   bool insert_extra_box,
                                   const jxl::IccBytes& icc_profile) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  jxl::BitWriter writer{memory_manager};
  EXPECT_TRUE(writer.WithMaxBits(
      65536,  // Large enough
      jxl::LayerType::Header, nullptr, [&] {
        if (have_container) {
          const std::vector<uint8_t> signature_box = {
              0, 0, 0, 0xc, 'J', 'X', 'L', ' ', 0xd, 0xa, 0x87, 0xa};
          const std::vector<uint8_t> filetype_box = {
              0,   0,   0, 0x14, 'f', 't', 'y', 'p', 'j', 'x',
              'l', ' ', 0, 0,    0,   0,   'j', 'x', 'l', ' '};
          const std::vector<uint8_t> extra_box_header = {0,   0,   0,   0xff,
                                                         't', 'e', 's', 't'};
          // Beginning of codestream box, with an arbitrary size certainly large
          // enough to contain the header
          const std::vector<uint8_t> codestream_box_header = {
              0, 0, 0, 0xff, 'j', 'x', 'l', 'c'};

          for (uint8_t c : signature_box) {
            writer.Write(8, c);
          }
          for (uint8_t c : filetype_box) {
            writer.Write(8, c);
          }
          if (insert_extra_box) {
            for (uint8_t c : extra_box_header) {
              writer.Write(8, c);
            }
            for (size_t i = 0; i < 255 - 8; i++) {
              writer.Write(8, 0);
            }
          }
          for (uint8_t c : codestream_box_header) {
            writer.Write(8, c);
          }
        }

        // JXL signature
        writer.Write(8, 0xff);
        writer.Write(8, 0x0a);

        // SizeHeader
        jxl::CodecMetadata metadata;
        EXPECT_TRUE(metadata.size.Set(xsize, ysize));
        EXPECT_TRUE(WriteSizeHeader(metadata.size, &writer,
                                    jxl::LayerType::Header, nullptr));

        if (!metadata_default) {
          metadata.m.SetUintSamples(bits_per_sample);
          metadata.m.orientation = orientation;
          metadata.m.SetAlphaBits(alpha_bits);
          metadata.m.xyb_encoded = xyb_encoded;
          if (alpha_bits != 0) {
            metadata.m.extra_channel_info[0].name = "alpha_test";
          }
        }

        if (!icc_profile.empty()) {
          jxl::IccBytes copy = icc_profile;
          EXPECT_TRUE(metadata.m.color_encoding.SetICC(std::move(copy),
                                                       JxlGetDefaultCms()));
        }

        EXPECT_TRUE(jxl::Bundle::Write(metadata.m, &writer,
                                       jxl::LayerType::Header, nullptr));
        metadata.transform_data.nonserialized_xyb_encoded =
            metadata.m.xyb_encoded;
        EXPECT_TRUE(jxl::Bundle::Write(metadata.transform_data, &writer,
                                       jxl::LayerType::Header, nullptr));

        if (!icc_profile.empty()) {
          EXPECT_TRUE(metadata.m.color_encoding.WantICC());
          EXPECT_TRUE(jxl::WriteICC(jxl::Span<const uint8_t>(icc_profile),
                                    &writer, jxl::LayerType::Header, nullptr));
        }

        writer.ZeroPadToByte();
        return true;
      }));
  jxl::Bytes bytes = writer.GetSpan();
  return std::vector<uint8_t>(bytes.data(), bytes.data() + bytes.size());
}

TEST(DecodeTest, BasicInfoTest) {
  size_t xsize[2] = {50, 33};
  size_t ysize[2] = {50, 77};
  size_t bits_per_sample[2] = {8, 23};
  size_t orientation[2] = {3, 5};
  size_t alpha_bits[2] = {0, 8};
  bool have_container[2] = {false, true};
  bool xyb_encoded = false;

  std::vector<std::vector<uint8_t>> test_samples;
  // Test with direct codestream
  test_samples.push_back(GetTestHeader(
      xsize[0], ysize[0], bits_per_sample[0], orientation[0], alpha_bits[0],
      xyb_encoded, have_container[0], /*metadata_default=*/false,
      /*insert_extra_box=*/false, {}));
  // Test with container and different parameters
  test_samples.push_back(GetTestHeader(
      xsize[1], ysize[1], bits_per_sample[1], orientation[1], alpha_bits[1],
      xyb_encoded, have_container[1], /*metadata_default=*/false,
      /*insert_extra_box=*/false, {}));

  for (size_t i = 0; i < test_samples.size(); ++i) {
    const std::vector<uint8_t>& data = test_samples[i];
    // Test decoding too small header first, until we reach the final byte.
    for (size_t size = 0; size <= data.size(); ++size) {
      // Test with a new decoder for each tested byte size.
      JxlDecoder* dec = JxlDecoderCreate(nullptr);
      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO));
      const uint8_t* next_in = data.data();
      size_t avail_in = size;
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));
      JxlDecoderStatus status = JxlDecoderProcessInput(dec);

      JxlBasicInfo info;
      JxlDecoderStatus bi_status = JxlDecoderGetBasicInfo(dec, &info);
      bool have_basic_info = (bi_status == JXL_DEC_SUCCESS);

      if (size == data.size()) {
        EXPECT_EQ(JXL_DEC_BASIC_INFO, status);

        // All header bytes given so the decoder must have the basic info.
        EXPECT_EQ(true, have_basic_info);
        EXPECT_EQ(have_container[i], FROM_JXL_BOOL(info.have_container));
        EXPECT_EQ(alpha_bits[i], info.alpha_bits);
        // Orientations 5..8 swap the dimensions
        if (orientation[i] >= 5) {
          EXPECT_EQ(xsize[i], info.ysize);
          EXPECT_EQ(ysize[i], info.xsize);
        } else {
          EXPECT_EQ(xsize[i], info.xsize);
          EXPECT_EQ(ysize[i], info.ysize);
        }
        // The API should set the orientation to identity by default since it
        // already applies the transformation internally by default.
        EXPECT_EQ(1u, info.orientation);

        EXPECT_EQ(3u, info.num_color_channels);

        if (alpha_bits[i] != 0) {
          // Expect an extra channel
          EXPECT_EQ(1u, info.num_extra_channels);
          JxlExtraChannelInfo extra;
          EXPECT_EQ(0, JxlDecoderGetExtraChannelInfo(dec, 0, &extra));
          EXPECT_EQ(alpha_bits[i], extra.bits_per_sample);
          EXPECT_EQ(JXL_CHANNEL_ALPHA, extra.type);
          EXPECT_EQ(0, extra.alpha_premultiplied);
          // Verify the name "alpha_test" given to the alpha channel
          EXPECT_EQ(10u, extra.name_length);
          char name[11];
          EXPECT_EQ(0,
                    JxlDecoderGetExtraChannelName(dec, 0, name, sizeof(name)));
          EXPECT_EQ(std::string("alpha_test"), std::string(name));
        } else {
          EXPECT_EQ(0u, info.num_extra_channels);
        }

        EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));
      } else {
        // If we did not give the full header, the basic info should not be
        // available. Allow a few bytes of slack due to some bits for default
        // opsinmatrix/extension bits.
        if (size + 2 < data.size()) {
          EXPECT_EQ(false, have_basic_info);
          EXPECT_EQ(JXL_DEC_NEED_MORE_INPUT, status);
        }
      }

      // Test that decoder doesn't allow setting a setting required at beginning
      // unless it's reset
      EXPECT_EQ(JXL_DEC_ERROR,
                JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO));
      JxlDecoderReset(dec);
      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO));

      JxlDecoderDestroy(dec);
    }
  }
}

TEST(DecodeTest, BufferSizeTest) {
  size_t xsize = 33;
  size_t ysize = 77;
  size_t bits_per_sample = 8;
  size_t orientation = 1;
  size_t alpha_bits = 8;
  bool have_container = false;
  bool xyb_encoded = false;

  std::vector<uint8_t> header =
      GetTestHeader(xsize, ysize, bits_per_sample, orientation, alpha_bits,
                    xyb_encoded, have_container, /*metadata_default=*/false,
                    /*insert_extra_box=*/false, {});

  JxlDecoder* dec = JxlDecoderCreate(nullptr);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO));
  const uint8_t* next_in = header.data();
  size_t avail_in = header.size();
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));
  JxlDecoderStatus status = JxlDecoderProcessInput(dec);
  EXPECT_EQ(JXL_DEC_BASIC_INFO, status);

  JxlBasicInfo info;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
  EXPECT_EQ(xsize, info.xsize);
  EXPECT_EQ(ysize, info.ysize);

  JxlPixelFormat format = {4, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, 0};
  size_t image_out_size;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderImageOutBufferSize(dec, &format, &image_out_size));
  EXPECT_EQ(xsize * ysize * 4, image_out_size);

  JxlDecoderDestroy(dec);
}

TEST(DecodeTest, BasicInfoSizeHintTest) {
  // Test on a file where the size hint is too small initially due to inserting
  // a box before the codestream (something that is normally not recommended)
  size_t xsize = 50;
  size_t ysize = 50;
  size_t bits_per_sample = 16;
  size_t orientation = 1;
  size_t alpha_bits = 0;
  bool xyb_encoded = false;
  std::vector<uint8_t> data = GetTestHeader(
      xsize, ysize, bits_per_sample, orientation, alpha_bits, xyb_encoded,
      /*have_container=*/true, /*metadata_default=*/false,
      /*insert_extra_box=*/true, {});

  JxlDecoderStatus status;
  JxlDecoder* dec = JxlDecoderCreate(nullptr);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO));

  size_t hint0 = JxlDecoderSizeHintBasicInfo(dec);
  // Test that the test works as intended: we construct a file on purpose to
  // be larger than the first hint by having that extra box.
  EXPECT_LT(hint0, data.size());
  const uint8_t* next_in = data.data();
  // Do as if we have only as many bytes as indicated by the hint available
  size_t avail_in = std::min(hint0, data.size());
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));
  status = JxlDecoderProcessInput(dec);
  EXPECT_EQ(JXL_DEC_NEED_MORE_INPUT, status);
  // Basic info cannot be available yet due to the extra inserted box.
  EXPECT_EQ(false, !JxlDecoderGetBasicInfo(dec, nullptr));

  size_t num_read = avail_in - JxlDecoderReleaseInput(dec);
  EXPECT_LT(num_read, data.size());

  size_t hint1 = JxlDecoderSizeHintBasicInfo(dec);
  // The hint must be larger than the previous hint (taking already processed
  // bytes into account, the hint is a hint for the next avail_in) since the
  // decoder now knows there is a box in between.
  EXPECT_GT(hint1 + num_read, hint0);
  avail_in = std::min<size_t>(hint1, data.size() - num_read);
  next_in += num_read;

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));
  status = JxlDecoderProcessInput(dec);
  EXPECT_EQ(JXL_DEC_BASIC_INFO, status);
  JxlBasicInfo info;
  // We should have the basic info now, since we only added one box in-between,
  // and the decoder should have known its size, its implementation can return
  // a correct hint.
  EXPECT_EQ(true, !JxlDecoderGetBasicInfo(dec, &info));

  // Also test if the basic info is correct.
  EXPECT_EQ(1, info.have_container);
  EXPECT_EQ(xsize, info.xsize);
  EXPECT_EQ(ysize, info.ysize);
  EXPECT_EQ(orientation, info.orientation);
  EXPECT_EQ(bits_per_sample, info.bits_per_sample);

  JxlDecoderDestroy(dec);
}

std::vector<uint8_t> GetIccTestHeader(const jxl::IccBytes& icc_profile,
                                      bool xyb_encoded) {
  size_t xsize = 50;
  size_t ysize = 50;
  size_t bits_per_sample = 16;
  size_t orientation = 1;
  size_t alpha_bits = 0;
  return GetTestHeader(xsize, ysize, bits_per_sample, orientation, alpha_bits,
                       xyb_encoded,
                       /*have_container=*/false, /*metadata_default=*/false,
                       /*insert_extra_box=*/false, icc_profile);
}

// Tests the case where pixels and metadata ICC profile are the same
TEST(DecodeTest, IccProfileTestOriginal) {
  jxl::IccBytes icc_profile = GetIccTestProfile();
  bool xyb_encoded = false;
  std::vector<uint8_t> data = GetIccTestHeader(icc_profile, xyb_encoded);

  JxlDecoder* dec = JxlDecoderCreate(nullptr);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(
                dec, JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING));
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, data.data(), data.size()));

  EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));

  // Expect the opposite of xyb_encoded for uses_original_profile
  JxlBasicInfo info;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
  EXPECT_EQ(JXL_TRUE, info.uses_original_profile);

  EXPECT_EQ(JXL_DEC_COLOR_ENCODING, JxlDecoderProcessInput(dec));

  // the encoded color profile expected to be not available, since the image
  // has an ICC profile instead
  EXPECT_EQ(JXL_DEC_ERROR,
            JxlDecoderGetColorAsEncodedProfile(
                dec, JXL_COLOR_PROFILE_TARGET_ORIGINAL, nullptr));

  size_t dec_profile_size;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderGetICCProfileSize(dec, JXL_COLOR_PROFILE_TARGET_ORIGINAL,
                                        &dec_profile_size));

  // Check that can get return status with NULL size
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderGetICCProfileSize(dec, JXL_COLOR_PROFILE_TARGET_ORIGINAL,
                                        nullptr));

  // The profiles must be equal. This requires they have equal size, and if
  // they do, we can get the profile and compare the contents.
  EXPECT_EQ(icc_profile.size(), dec_profile_size);
  if (icc_profile.size() == dec_profile_size) {
    jxl::IccBytes icc_profile2(icc_profile.size());
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetColorAsICCProfile(
                                   dec, JXL_COLOR_PROFILE_TARGET_ORIGINAL,
                                   icc_profile2.data(), icc_profile2.size()));
    EXPECT_EQ(icc_profile, icc_profile2);
  }

  // the data is not xyb_encoded, so same result expected for the pixel data
  // color profile
  EXPECT_EQ(JXL_DEC_ERROR, JxlDecoderGetColorAsEncodedProfile(
                               dec, JXL_COLOR_PROFILE_TARGET_DATA, nullptr));

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderGetICCProfileSize(dec, JXL_COLOR_PROFILE_TARGET_DATA,
                                        &dec_profile_size));
  EXPECT_EQ(icc_profile.size(), dec_profile_size);

  JxlDecoderDestroy(dec);
}

// Tests the case where pixels and metadata ICC profile are different
TEST(DecodeTest, IccProfileTestXybEncoded) {
  jxl::IccBytes icc_profile = GetIccTestProfile();
  bool xyb_encoded = true;
  std::vector<uint8_t> data = GetIccTestHeader(icc_profile, xyb_encoded);

  JxlDecoder* dec = JxlDecoderCreate(nullptr);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(
                dec, JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING));

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, data.data(), data.size()));
  EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));

  // Expect the opposite of xyb_encoded for uses_original_profile
  JxlBasicInfo info;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
  EXPECT_EQ(JXL_FALSE, info.uses_original_profile);

  EXPECT_EQ(JXL_DEC_COLOR_ENCODING, JxlDecoderProcessInput(dec));

  // the encoded color profile expected to be not available, since the image
  // has an ICC profile instead
  EXPECT_EQ(JXL_DEC_ERROR,
            JxlDecoderGetColorAsEncodedProfile(
                dec, JXL_COLOR_PROFILE_TARGET_ORIGINAL, nullptr));

  // Check that can get return status with NULL size
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderGetICCProfileSize(dec, JXL_COLOR_PROFILE_TARGET_ORIGINAL,
                                        nullptr));

  size_t dec_profile_size;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderGetICCProfileSize(dec, JXL_COLOR_PROFILE_TARGET_ORIGINAL,
                                        &dec_profile_size));

  // The profiles must be equal. This requires they have equal size, and if
  // they do, we can get the profile and compare the contents.
  EXPECT_EQ(icc_profile.size(), dec_profile_size);
  if (icc_profile.size() == dec_profile_size) {
    jxl::IccBytes icc_profile2(icc_profile.size());
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetColorAsICCProfile(
                                   dec, JXL_COLOR_PROFILE_TARGET_ORIGINAL,
                                   icc_profile2.data(), icc_profile2.size()));
    EXPECT_EQ(icc_profile, icc_profile2);
  }

  // Data is xyb_encoded, so the data profile is a different profile, encoded
  // as structured profile.
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetColorAsEncodedProfile(
                                 dec, JXL_COLOR_PROFILE_TARGET_DATA, nullptr));
  JxlColorEncoding pixel_encoding;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderGetColorAsEncodedProfile(
                dec, JXL_COLOR_PROFILE_TARGET_DATA, &pixel_encoding));
  EXPECT_EQ(JXL_PRIMARIES_SRGB, pixel_encoding.primaries);
  // The API returns LINEAR by default when the colorspace cannot be represented
  // by enum values.
  EXPECT_EQ(JXL_TRANSFER_FUNCTION_LINEAR, pixel_encoding.transfer_function);

  // Test the same but with integer format.
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderGetColorAsEncodedProfile(
                dec, JXL_COLOR_PROFILE_TARGET_DATA, &pixel_encoding));
  EXPECT_EQ(JXL_PRIMARIES_SRGB, pixel_encoding.primaries);
  EXPECT_EQ(JXL_TRANSFER_FUNCTION_LINEAR, pixel_encoding.transfer_function);

  // Test after setting the preferred color profile to non-linear sRGB:
  // for XYB images with ICC profile, this setting is expected to take effect.
  jxl::ColorEncoding temp_jxl_srgb = jxl::ColorEncoding::SRGB(false);
  JxlColorEncoding pixel_encoding_srgb = temp_jxl_srgb.ToExternal();
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetPreferredColorProfile(dec, &pixel_encoding_srgb));
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderGetColorAsEncodedProfile(
                dec, JXL_COLOR_PROFILE_TARGET_DATA, &pixel_encoding));
  EXPECT_EQ(JXL_TRANSFER_FUNCTION_SRGB, pixel_encoding.transfer_function);

  // The decoder can also output this as a generated ICC profile anyway, and
  // we're certain that it will differ from the above defined profile since
  // the sRGB data should not have swapped R/G/B primaries.

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderGetICCProfileSize(dec, JXL_COLOR_PROFILE_TARGET_DATA,
                                        &dec_profile_size));
  // We don't need to dictate exactly what size the generated ICC profile
  // must be (since there are many ways to represent the same color space),
  // but it should not be zero.
  EXPECT_NE(0u, dec_profile_size);
  jxl::IccBytes icc_profile2(dec_profile_size);
  if (0 != dec_profile_size) {
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetColorAsICCProfile(
                                   dec, JXL_COLOR_PROFILE_TARGET_DATA,
                                   icc_profile2.data(), icc_profile2.size()));
    // expected not equal
    EXPECT_NE(icc_profile, icc_profile2);
  }

  // Test setting another different preferred profile, to verify that the
  // returned JXL_COLOR_PROFILE_TARGET_DATA ICC profile is correctly
  // updated.

  jxl::ColorEncoding temp_jxl_linear = jxl::ColorEncoding::LinearSRGB(false);
  JxlColorEncoding pixel_encoding_linear = temp_jxl_linear.ToExternal();

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetPreferredColorProfile(dec, &pixel_encoding_linear));
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderGetColorAsEncodedProfile(
                dec, JXL_COLOR_PROFILE_TARGET_DATA, &pixel_encoding));
  EXPECT_EQ(JXL_TRANSFER_FUNCTION_LINEAR, pixel_encoding.transfer_function);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderGetICCProfileSize(dec, JXL_COLOR_PROFILE_TARGET_DATA,
                                        &dec_profile_size));
  EXPECT_NE(0u, dec_profile_size);
  jxl::IccBytes icc_profile3(dec_profile_size);
  if (0 != dec_profile_size) {
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetColorAsICCProfile(
                                   dec, JXL_COLOR_PROFILE_TARGET_DATA,
                                   icc_profile3.data(), icc_profile3.size()));
    // expected not equal to the previously set preferred profile.
    EXPECT_NE(icc_profile2, icc_profile3);
  }

  JxlDecoderDestroy(dec);
}

// Test decoding ICC from partial files byte for byte.
// This test must pass also if JXL_CRASH_ON_ERROR is enabled, that is, the
// decoding of the ANS histogram and stream of the encoded ICC profile must also
// handle the case of not enough input bytes with StatusCode::kNotEnoughBytes
// rather than fatal error status codes.
TEST(DecodeTest, ICCPartialTest) {
  jxl::IccBytes icc_profile = GetIccTestProfile();
  std::vector<uint8_t> data = GetIccTestHeader(icc_profile, false);

  const uint8_t* next_in = data.data();
  size_t avail_in = 0;

  JxlDecoder* dec = JxlDecoderCreate(nullptr);

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(
                dec, JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING));

  bool seen_basic_info = false;
  bool seen_color_encoding = false;
  size_t total_size = 0;

  for (;;) {
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));
    JxlDecoderStatus status = JxlDecoderProcessInput(dec);
    size_t remaining = JxlDecoderReleaseInput(dec);
    EXPECT_LE(remaining, avail_in);
    next_in += avail_in - remaining;
    avail_in = remaining;
    if (status == JXL_DEC_NEED_MORE_INPUT) {
      if (total_size >= data.size()) {
        // End of partial codestream with codestrema headers and ICC profile
        // reached, it should not require more input since full image is not
        // requested
        FAIL();
        break;
      }
      size_t increment = 1;
      if (total_size + increment > data.size()) {
        increment = data.size() - total_size;
      }
      total_size += increment;
      avail_in += increment;
    } else if (status == JXL_DEC_BASIC_INFO) {
      EXPECT_FALSE(seen_basic_info);
      seen_basic_info = true;
    } else if (status == JXL_DEC_COLOR_ENCODING) {
      EXPECT_TRUE(seen_basic_info);
      EXPECT_FALSE(seen_color_encoding);
      seen_color_encoding = true;

      // Sanity check that the ICC profile was decoded correctly
      size_t dec_profile_size;
      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderGetICCProfileSize(
                    dec, JXL_COLOR_PROFILE_TARGET_ORIGINAL, &dec_profile_size));
      EXPECT_EQ(icc_profile.size(), dec_profile_size);

    } else if (status == JXL_DEC_SUCCESS) {
      EXPECT_TRUE(seen_color_encoding);
      break;
    } else {
      // We do not expect any other events or errors
      FAIL();
      break;
    }
  }

  EXPECT_TRUE(seen_basic_info);
  EXPECT_TRUE(seen_color_encoding);

  JxlDecoderDestroy(dec);
}

struct PixelTestConfig {
  // Input image definition.
  bool grayscale;
  bool include_alpha;
  size_t xsize;
  size_t ysize;
  jxl::PreviewMode preview_mode;
  bool add_intrinsic_size;
  // Output format.
  JxlEndianness endianness;
  JxlDataType data_type;
  uint32_t output_channels;
  // Container options.
  CodeStreamBoxFormat add_container;
  // Decoding mode.
  bool use_callback;
  bool set_buffer_early;
  bool use_resizable_runner;
  // Exif orientation, 1-8
  JxlOrientation orientation;
  bool keep_orientation;
  size_t upsampling;
};

class DecodeTestParam : public ::testing::TestWithParam<PixelTestConfig> {};

TEST_P(DecodeTestParam, PixelTest) {
  PixelTestConfig config = GetParam();
  JxlDecoder* dec = JxlDecoderCreate(nullptr);

  if (config.keep_orientation) {
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetKeepOrientation(dec, JXL_TRUE));
  }

  size_t num_pixels = config.xsize * config.ysize;
  uint32_t orig_channels =
      (config.grayscale ? 1 : 3) + (config.include_alpha ? 1 : 0);
  std::vector<uint8_t> pixels =
      jxl::test::GetSomeTestImage(config.xsize, config.ysize, orig_channels, 0);
  JxlPixelFormat format_orig = {orig_channels, JXL_TYPE_UINT16, JXL_BIG_ENDIAN,
                                0};
  jxl::TestCodestreamParams params;
  // Lossless to verify pixels exactly after roundtrip.
  params.cparams.SetLossless();
  params.cparams.speed_tier = jxl::SpeedTier::kThunder;
  params.cparams.resampling = config.upsampling;
  params.cparams.ec_resampling = config.upsampling;
  params.box_format = config.add_container;
  params.orientation = config.orientation;
  params.preview_mode = config.preview_mode;
  params.add_intrinsic_size = config.add_intrinsic_size;
  std::vector<uint8_t> compressed = jxl::CreateTestJXLCodestream(
      jxl::Bytes(pixels.data(), pixels.size()), config.xsize, config.ysize,
      orig_channels, params);

  JxlPixelFormat format = {config.output_channels, config.data_type,
                           config.endianness, 0};

  bool swap_xy = !config.keep_orientation && (config.orientation > 4);
  size_t xsize = swap_xy ? config.ysize : config.xsize;
  size_t ysize = swap_xy ? config.xsize : config.ysize;

  std::vector<uint8_t> pixels2 =
      jxl::DecodeWithAPI(dec, jxl::Bytes(compressed.data(), compressed.size()),
                         format, config.use_callback, config.set_buffer_early,
                         config.use_resizable_runner, /*require_boxes=*/false,
                         /*expect_success=*/true);
  JxlDecoderReset(dec);
  EXPECT_EQ(num_pixels * config.output_channels *
                jxl::test::GetDataBits(config.data_type) / jxl::kBitsPerByte,
            pixels2.size());

  // If an orientation transformation is expected, to compare the pixels, also
  // apply this transformation to the original pixels. ConvertToExternal is
  // used to achieve this, with a temporary conversion to CodecInOut and back.
  if (config.orientation > 1 && !config.keep_orientation) {
    jxl::Span<const uint8_t> bytes(pixels.data(), pixels.size());
    jxl::ColorEncoding color_encoding =
        jxl::ColorEncoding::SRGB(config.grayscale);

    jxl::CodecInOut io{jxl::test::MemoryManager()};
    if (config.include_alpha) io.metadata.m.SetAlphaBits(16);
    io.metadata.m.color_encoding = color_encoding;
    ASSERT_TRUE(io.SetSize(config.xsize, config.ysize));

    EXPECT_TRUE(ConvertFromExternal(bytes, config.xsize, config.ysize,
                                    color_encoding, 16, format_orig, nullptr,
                                    &io.Main()));

    for (uint8_t& pixel : pixels) pixel = 0;
    EXPECT_TRUE(ConvertToExternal(
        io.Main(), 16,
        /*float_out=*/false, orig_channels, JXL_BIG_ENDIAN,
        xsize * 2 * orig_channels, nullptr, pixels.data(), pixels.size(),
        /*out_callback=*/{},
        static_cast<jxl::Orientation>(config.orientation)));
  }
  if (config.upsampling == 1) {
    EXPECT_EQ(0u, jxl::test::ComparePixels(pixels.data(), pixels2.data(), xsize,
                                           ysize, format_orig, format));
  } else {
    // resampling is of course not lossless, so as a rough check:
    // count pixels that are more than off-by-25 in the 8-bit value of one of
    // the channels
    EXPECT_LE(
        jxl::test::ComparePixels(
            pixels.data(), pixels2.data(), xsize, ysize, format_orig, format,
            50.0 * (config.data_type == JXL_TYPE_UINT8 ? 1.0 : 256.0)),
        300u);
  }

  JxlDecoderDestroy(dec);
}

std::vector<PixelTestConfig> GeneratePixelTests() {
  std::vector<PixelTestConfig> all_tests;
  struct ChannelInfo {
    bool grayscale;
    bool include_alpha;
    size_t output_channels;
  };
  ChannelInfo ch_info[] = {
      {false, true, 4},   // RGBA -> RGBA
      {true, false, 1},   // G -> G
      {true, true, 1},    // GA -> G
      {true, true, 2},    // GA -> GA
      {false, false, 3},  // RGB -> RGB
      {false, true, 3},   // RGBA -> RGB
      {false, false, 4},  // RGB -> RGBA
  };

  struct OutputFormat {
    JxlEndianness endianness;
    JxlDataType data_type;
  };
  OutputFormat out_formats[] = {
      {JXL_NATIVE_ENDIAN, JXL_TYPE_UINT8},
      {JXL_LITTLE_ENDIAN, JXL_TYPE_UINT16},
      {JXL_BIG_ENDIAN, JXL_TYPE_UINT16},
      {JXL_NATIVE_ENDIAN, JXL_TYPE_FLOAT16},
      {JXL_LITTLE_ENDIAN, JXL_TYPE_FLOAT},
      {JXL_BIG_ENDIAN, JXL_TYPE_FLOAT},
  };

  auto make_test = [&](ChannelInfo ch, size_t xsize, size_t ysize,
                       jxl::PreviewMode preview_mode, bool intrinsic_size,
                       CodeStreamBoxFormat box, JxlOrientation orientation,
                       bool keep_orientation, OutputFormat format,
                       bool use_callback, bool set_buffer_early,
                       bool resizable_runner, size_t upsampling) {
    PixelTestConfig c;
    c.grayscale = ch.grayscale;
    c.include_alpha = ch.include_alpha;
    c.preview_mode = preview_mode;
    c.add_intrinsic_size = intrinsic_size;
    c.xsize = xsize;
    c.ysize = ysize;
    c.add_container = box;
    c.output_channels = ch.output_channels;
    c.data_type = format.data_type;
    c.endianness = format.endianness;
    c.use_callback = use_callback;
    c.set_buffer_early = set_buffer_early;
    c.use_resizable_runner = resizable_runner;
    c.orientation = orientation;
    c.keep_orientation = keep_orientation;
    c.upsampling = upsampling;
    all_tests.push_back(c);
  };

  // Test output formats and methods.
  for (ChannelInfo ch : ch_info) {
    for (bool use_callback : {false, true}) {
      for (size_t upsampling : {1, 2, 4, 8}) {
        for (OutputFormat fmt : out_formats) {
          make_test(ch, 301, 33, jxl::kNoPreview,
                    /*add_intrinsic_size=*/false,
                    CodeStreamBoxFormat::kCSBF_None, JXL_ORIENT_IDENTITY,
                    /*keep_orientation=*/false, fmt, use_callback,
                    /*set_buffer_early=*/false, /*resizable_runner=*/false,
                    upsampling);
        }
      }
    }
  }
  // Test codestream formats.
  for (size_t box = 1; box < kCSBF_NUM_ENTRIES; ++box) {
    make_test(ch_info[0], 77, 33, jxl::kNoPreview,
              /*add_intrinsic_size=*/false,
              static_cast<CodeStreamBoxFormat>(box), JXL_ORIENT_IDENTITY,
              /*keep_orientation=*/false, out_formats[0],
              /*use_callback=*/false,
              /*set_buffer_early=*/false, /*resizable_runner=*/false, 1);
  }
  // Test previews.
  for (int preview_mode = 0; preview_mode < jxl::kNumPreviewModes;
       preview_mode++) {
    make_test(ch_info[0], 77, 33, static_cast<jxl::PreviewMode>(preview_mode),
              /*add_intrinsic_size=*/false, CodeStreamBoxFormat::kCSBF_None,
              JXL_ORIENT_IDENTITY,
              /*keep_orientation=*/false, out_formats[0],
              /*use_callback=*/false, /*set_buffer_early=*/false,
              /*resizable_runner=*/false, 1);
  }
  // Test intrinsic sizes.
  for (bool add_intrinsic_size : {false, true}) {
    make_test(ch_info[0], 55, 34, jxl::kNoPreview, add_intrinsic_size,
              CodeStreamBoxFormat::kCSBF_None, JXL_ORIENT_IDENTITY,
              /*keep_orientation=*/false, out_formats[0],
              /*use_callback=*/false, /*set_buffer_early=*/false,
              /*resizable_runner=*/false, 1);
  }
  // Test setting buffers early.
  make_test(ch_info[0], 300, 33, jxl::kNoPreview,
            /*add_intrinsic_size=*/false, CodeStreamBoxFormat::kCSBF_None,
            JXL_ORIENT_IDENTITY,
            /*keep_orientation=*/false, out_formats[0],
            /*use_callback=*/false, /*set_buffer_early=*/true,
            /*resizable_runner=*/false, 1);

  // Test using the resizable runner
  for (size_t i = 0; i < 4; i++) {
    make_test(ch_info[0], 300 << i, 33 << i, jxl::kNoPreview,
              /*add_intrinsic_size=*/false, CodeStreamBoxFormat::kCSBF_None,
              JXL_ORIENT_IDENTITY,
              /*keep_orientation=*/false, out_formats[0],
              /*use_callback=*/false, /*set_buffer_early=*/false,
              /*resizable_runner=*/true, 1);
  }

  // Test orientations.
  for (int orientation = 2; orientation <= 8; ++orientation) {
    for (bool keep_orientation : {false, true}) {
      for (bool use_callback : {false, true}) {
        for (ChannelInfo ch : ch_info) {
          for (OutputFormat fmt : out_formats) {
            make_test(ch, 280, 12, jxl::kNoPreview,
                      /*add_intrinsic_size=*/false,
                      CodeStreamBoxFormat::kCSBF_None,
                      static_cast<JxlOrientation>(orientation),
                      /*keep_orientation=*/keep_orientation, fmt,
                      /*use_callback=*/use_callback, /*set_buffer_early=*/true,
                      /*resizable_runner=*/false, 1);
          }
        }
      }
    }
  }

  return all_tests;
}

std::ostream& operator<<(std::ostream& os, const PixelTestConfig& c) {
  os << c.xsize << "x" << c.ysize;
  const char* colors[] = {"", "G", "GA", "RGB", "RGBA"};
  os << colors[(c.grayscale ? 1 : 3) + (c.include_alpha ? 1 : 0)];
  os << "to";
  os << colors[c.output_channels];
  switch (c.data_type) {
    case JXL_TYPE_UINT8:
      os << "u8";
      break;
    case JXL_TYPE_UINT16:
      os << "u16";
      break;
    case JXL_TYPE_FLOAT:
      os << "f32";
      break;
    case JXL_TYPE_FLOAT16:
      os << "f16";
      break;
    default:
      ADD_FAILURE();
  };
  if (jxl::test::GetDataBits(c.data_type) > jxl::kBitsPerByte) {
    if (c.endianness == JXL_NATIVE_ENDIAN) {
      // add nothing
    } else if (c.endianness == JXL_BIG_ENDIAN) {
      os << "BE";
    } else if (c.endianness == JXL_LITTLE_ENDIAN) {
      os << "LE";
    }
  }
  if (c.add_container != CodeStreamBoxFormat::kCSBF_None) {
    os << "Box";
    os << static_cast<size_t>(c.add_container);
  }
  if (c.preview_mode == jxl::kSmallPreview) os << "Preview";
  if (c.preview_mode == jxl::kBigPreview) os << "BigPreview";
  if (c.add_intrinsic_size) os << "IntrinicSize";
  if (c.use_callback) os << "Callback";
  if (c.set_buffer_early) os << "EarlyBuffer";
  if (c.use_resizable_runner) os << "ResizableRunner";
  if (c.orientation != 1) os << "O" << c.orientation;
  if (c.keep_orientation) os << "Keep";
  if (c.upsampling > 1) os << "x" << c.upsampling;
  return os;
}

std::string PixelTestDescription(
    const testing::TestParamInfo<DecodeTestParam::ParamType>& info) {
  std::stringstream name;
  name << info.param;
  return name.str();
}

JXL_GTEST_INSTANTIATE_TEST_SUITE_P(DecodeTest, DecodeTestParam,
                                   testing::ValuesIn(GeneratePixelTests()),
                                   PixelTestDescription);

TEST(DecodeTest, PixelTestWithICCProfileLossless) {
  JxlDecoder* dec = JxlDecoderCreate(nullptr);

  size_t xsize = 123;
  size_t ysize = 77;
  size_t num_pixels = xsize * ysize;
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);
  JxlPixelFormat format_orig = {4, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
  jxl::TestCodestreamParams params;
  // Lossless to verify pixels exactly after roundtrip.
  params.cparams.SetLossless();
  params.cparams.speed_tier = jxl::SpeedTier::kThunder;
  params.add_icc_profile = true;
  // For variation: some have container and no preview, others have preview
  // and no container.
  std::vector<uint8_t> compressed = jxl::CreateTestJXLCodestream(
      jxl::Bytes(pixels.data(), pixels.size()), xsize, ysize, 4, params);

  for (uint32_t channels = 3; channels <= 4; ++channels) {
    {
      JxlPixelFormat format = {channels, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, 0};

      std::vector<uint8_t> pixels2 = jxl::DecodeWithAPI(
          dec, jxl::Bytes(compressed.data(), compressed.size()), format,
          /*use_callback=*/false, /*set_buffer_early=*/false,
          /*use_resizable_runner=*/false, /*require_boxes=*/false,
          /*expect_success=*/true);
      JxlDecoderReset(dec);
      EXPECT_EQ(num_pixels * channels, pixels2.size());
      EXPECT_EQ(0u,
                jxl::test::ComparePixels(pixels.data(), pixels2.data(), xsize,
                                         ysize, format_orig, format));
    }
    {
      JxlPixelFormat format = {channels, JXL_TYPE_UINT16, JXL_LITTLE_ENDIAN, 0};

      // Test with the container for one of the pixel formats.
      std::vector<uint8_t> pixels2 = jxl::DecodeWithAPI(
          dec, jxl::Bytes(compressed.data(), compressed.size()), format,
          /*use_callback=*/true, /*set_buffer_early=*/true,
          /*use_resizable_runner=*/false, /*require_boxes=*/false,
          /*expect_success=*/true);
      JxlDecoderReset(dec);
      EXPECT_EQ(num_pixels * channels * 2, pixels2.size());
      EXPECT_EQ(0u,
                jxl::test::ComparePixels(pixels.data(), pixels2.data(), xsize,
                                         ysize, format_orig, format));
    }

    {
      JxlPixelFormat format = {channels, JXL_TYPE_FLOAT, JXL_LITTLE_ENDIAN, 0};

      std::vector<uint8_t> pixels2 = jxl::DecodeWithAPI(
          dec, jxl::Bytes(compressed.data(), compressed.size()), format,
          /*use_callback=*/false, /*set_buffer_early=*/false,
          /*use_resizable_runner=*/false, /*require_boxes=*/false,
          /*expect_success=*/true);
      JxlDecoderReset(dec);
      EXPECT_EQ(num_pixels * channels * 4, pixels2.size());
      EXPECT_EQ(0u,
                jxl::test::ComparePixels(pixels.data(), pixels2.data(), xsize,
                                         ysize, format_orig, format));
    }
  }

  JxlDecoderDestroy(dec);
}

TEST(DecodeTest, PixelTestWithICCProfileLossy) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  JxlDecoder* dec = JxlDecoderCreate(nullptr);

  size_t xsize = 123;
  size_t ysize = 77;
  size_t num_pixels = xsize * ysize;
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 3, 0);
  JxlPixelFormat format_orig = {3, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
  jxl::TestCodestreamParams params;
  params.add_icc_profile = true;
  std::vector<uint8_t> compressed = jxl::CreateTestJXLCodestream(
      jxl::Bytes(pixels.data(), pixels.size()), xsize, ysize, 3, params);
  uint32_t channels = 3;

  JxlPixelFormat format = {channels, JXL_TYPE_FLOAT, JXL_LITTLE_ENDIAN, 0};

  std::vector<uint8_t> icc_data;
  std::vector<uint8_t> pixels2 = jxl::DecodeWithAPI(
      dec, jxl::Bytes(compressed.data(), compressed.size()), format,
      /*use_callback=*/false, /*set_buffer_early=*/true,
      /*use_resizable_runner=*/false, /*require_boxes=*/false,
      /*expect_success=*/true, /*icc=*/&icc_data);
  JxlDecoderReset(dec);
  EXPECT_EQ(num_pixels * channels * 4, pixels2.size());

  // The input pixels use the profile matching GetIccTestProfile, since we set
  // add_icc_profile for CreateTestJXLCodestream to true.
  jxl::ColorEncoding color_encoding0;
  EXPECT_TRUE(color_encoding0.SetICC(GetIccTestProfile(), JxlGetDefaultCms()));
  jxl::Span<const uint8_t> span0(pixels.data(), pixels.size());
  jxl::CodecInOut io0{memory_manager};
  ASSERT_TRUE(io0.SetSize(xsize, ysize));
  EXPECT_TRUE(ConvertFromExternal(span0, xsize, ysize, color_encoding0,
                                  /*bits_per_sample=*/16, format_orig,
                                  /*pool=*/nullptr, &io0.Main()));

  jxl::ColorEncoding color_encoding1;
  jxl::IccBytes icc;
  jxl::Bytes(icc_data).AppendTo(icc);
  EXPECT_TRUE(color_encoding1.SetICC(std::move(icc), JxlGetDefaultCms()));
  jxl::Span<const uint8_t> span1(pixels2.data(), pixels2.size());
  jxl::CodecInOut io1{memory_manager};
  ASSERT_TRUE(io1.SetSize(xsize, ysize));
  EXPECT_TRUE(ConvertFromExternal(span1, xsize, ysize, color_encoding1,
                                  /*bits_per_sample=*/32, format,
                                  /*pool=*/nullptr, &io1.Main()));

  jxl::ButteraugliParams butteraugli_params;
  EXPECT_SLIGHTLY_BELOW(
      ButteraugliDistance(io0.frames, io1.frames, butteraugli_params,
                          *JxlGetDefaultCms(),
                          /*distmap=*/nullptr, nullptr),
      0.58f);

  JxlDecoderDestroy(dec);
}

std::string ColorDescription(JxlColorEncoding c) {
  jxl::ColorEncoding color_encoding;
  EXPECT_TRUE(color_encoding.FromExternal(c));
  return Description(color_encoding);
}

std::string GetOrigProfile(JxlDecoder* dec) {
  JxlColorEncoding c;
  JxlColorProfileTarget target = JXL_COLOR_PROFILE_TARGET_ORIGINAL;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderGetColorAsEncodedProfile(dec, target, &c));
  return ColorDescription(c);
}

std::string GetDataProfile(JxlDecoder* dec) {
  JxlColorEncoding c;
  JxlColorProfileTarget target = JXL_COLOR_PROFILE_TARGET_DATA;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderGetColorAsEncodedProfile(dec, target, &c));
  return ColorDescription(c);
}

double ButteraugliDistance(size_t xsize, size_t ysize,
                           const std::vector<uint8_t>& pixels_in,
                           const jxl::ColorEncoding& color_in,
                           float intensity_in,
                           const std::vector<uint8_t>& pixels_out,
                           const jxl::ColorEncoding& color_out,
                           float intensity_out) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  jxl::CodecInOut in{memory_manager};
  in.metadata.m.color_encoding = color_in;
  in.metadata.m.SetIntensityTarget(intensity_in);
  JxlPixelFormat format_in = {static_cast<uint32_t>(color_in.Channels()),
                              JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
  EXPECT_TRUE(jxl::ConvertFromExternal(
      jxl::Bytes(pixels_in.data(), pixels_in.size()), xsize, ysize, color_in,
      /*bits_per_sample=*/16, format_in,
      /*pool=*/nullptr, &in.Main()));
  jxl::CodecInOut out{memory_manager};
  out.metadata.m.color_encoding = color_out;
  out.metadata.m.SetIntensityTarget(intensity_out);
  JxlPixelFormat format_out = {static_cast<uint32_t>(color_out.Channels()),
                               JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
  EXPECT_TRUE(jxl::ConvertFromExternal(
      jxl::Bytes(pixels_out.data(), pixels_out.size()), xsize, ysize, color_out,
      /*bits_per_sample=*/16, format_out,
      /*pool=*/nullptr, &out.Main()));
  return ButteraugliDistance(in.frames, out.frames, jxl::ButteraugliParams(),
                             *JxlGetDefaultCms(), nullptr, nullptr);
}

class DecodeAllEncodingsTest
    : public ::testing::TestWithParam<jxl::test::ColorEncodingDescriptor> {};
JXL_GTEST_INSTANTIATE_TEST_SUITE_P(
    DecodeAllEncodingsTestInstantiation, DecodeAllEncodingsTest,
    ::testing::ValuesIn(jxl::test::AllEncodings()));
TEST_P(DecodeAllEncodingsTest, PreserveOriginalProfileTest) {
  size_t xsize = 123;
  size_t ysize = 77;
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 3, 0);
  JxlPixelFormat format = {3, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
  int events = JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING | JXL_DEC_FULL_IMAGE;
  const auto& cdesc = GetParam();
  jxl::ColorEncoding c_in = jxl::test::ColorEncodingFromDescriptor(cdesc);
  if (c_in.GetRenderingIntent() != jxl::RenderingIntent::kRelative) return;
  std::string color_space_in = Description(c_in);
  float intensity_in = c_in.Tf().IsPQ() ? 10000 : 255;
  printf("Testing input color space %s\n", color_space_in.c_str());
  jxl::TestCodestreamParams params;
  params.color_space = color_space_in;
  params.intensity_target = intensity_in;
  std::vector<uint8_t> data = jxl::CreateTestJXLCodestream(
      jxl::Bytes(pixels.data(), pixels.size()), xsize, ysize, 3, params);
  JxlDecoder* dec = JxlDecoderCreate(nullptr);
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSubscribeEvents(dec, events));
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, data.data(), data.size()));
  EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
  JxlBasicInfo info;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
  EXPECT_EQ(xsize, info.xsize);
  EXPECT_EQ(ysize, info.ysize);
  EXPECT_FALSE(info.uses_original_profile);
  EXPECT_EQ(JXL_DEC_COLOR_ENCODING, JxlDecoderProcessInput(dec));
  EXPECT_EQ(GetOrigProfile(dec), color_space_in);
  EXPECT_EQ(GetDataProfile(dec), color_space_in);
  EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));
  std::vector<uint8_t> out(pixels.size());
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetImageOutBuffer(dec, &format, out.data(), out.size()));
  EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
  double dist = ButteraugliDistance(xsize, ysize, pixels, c_in, intensity_in,
                                    out, c_in, intensity_in);
  EXPECT_LT(dist, 1.29);
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));
  JxlDecoderDestroy(dec);
}

namespace {
void SetPreferredColorProfileTest(
    const jxl::test::ColorEncodingDescriptor& from, bool icc_dst,
    bool use_cms) {
  size_t xsize = 123;
  size_t ysize = 77;
  int events = JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING | JXL_DEC_FULL_IMAGE;
  jxl::ColorEncoding c_in = jxl::test::ColorEncodingFromDescriptor(from);
  if (c_in.GetRenderingIntent() != jxl::RenderingIntent::kRelative) return;
  if (c_in.GetWhitePointType() != jxl::WhitePoint::kD65) return;
  uint32_t num_channels = c_in.Channels();
  std::vector<uint8_t> pixels =
      jxl::test::GetSomeTestImage(xsize, ysize, num_channels, 0);

  JxlPixelFormat format = {num_channels, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
  std::string color_space_in = Description(c_in);
  float intensity_in = c_in.Tf().IsPQ() ? 10000 : 255;
  jxl::TestCodestreamParams params;
  params.color_space = color_space_in;
  params.intensity_target = intensity_in;
  std::vector<uint8_t> data =
      jxl::CreateTestJXLCodestream(jxl::Bytes(pixels.data(), pixels.size()),
                                   xsize, ysize, num_channels, params);
  auto all_encodings = jxl::test::AllEncodings();
  // TODO(firsching): understand why XYB does not work together with icc_dst.
  if (!icc_dst) {
    all_encodings.push_back(
        {jxl::ColorSpace::kXYB, jxl::WhitePoint::kD65, jxl::Primaries::kCustom,
         jxl::TransferFunction::kUnknown, jxl::RenderingIntent::kPerceptual});
  }
  for (const auto& c1 : all_encodings) {
    jxl::ColorEncoding c_out = jxl::test::ColorEncodingFromDescriptor(c1);
    float intensity_out = intensity_in;
    if (c_out.GetColorSpace() != jxl::ColorSpace::kXYB) {
      if (c_out.GetRenderingIntent() != jxl::RenderingIntent::kRelative) {
        continue;
      }
      if ((c_in.GetPrimariesType() == jxl::Primaries::k2100 &&
           c_out.GetPrimariesType() != jxl::Primaries::k2100) ||
          (c_in.GetPrimariesType() == jxl::Primaries::kP3 &&
           c_out.GetPrimariesType() == jxl::Primaries::kSRGB)) {
        // Converting to a narrower gamut does not work without gamut mapping.
        continue;
      }
    }
    if (c_out.Tf().IsHLG() && intensity_out > 300) {
      // The Linear->HLG OOTF function at this intensity level can push
      // saturated colors out of gamut, so we would need gamut mapping in
      // this case too.
      continue;
    }
    std::string color_space_out = Description(c_out);
    if (color_space_in == color_space_out) continue;
    printf("Testing input color space %s with output color space %s\n",
           color_space_in.c_str(), color_space_out.c_str());
    JxlDecoder* dec = JxlDecoderCreate(nullptr);
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSubscribeEvents(dec, events));
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderSetInput(dec, data.data(), data.size()));
    EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
    JxlBasicInfo info;
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
    EXPECT_EQ(xsize, info.xsize);
    EXPECT_EQ(ysize, info.ysize);
    EXPECT_FALSE(info.uses_original_profile);
    EXPECT_EQ(JXL_DEC_COLOR_ENCODING, JxlDecoderProcessInput(dec));
    EXPECT_EQ(GetOrigProfile(dec), color_space_in);
    JxlColorEncoding encoding_out;
    EXPECT_TRUE(jxl::ParseDescription(color_space_out, &encoding_out));
    if (c_out.GetColorSpace() == jxl::ColorSpace::kXYB &&
        (c_in.GetPrimariesType() != jxl::Primaries::kSRGB ||
         c_in.Tf().IsPQ())) {
      EXPECT_EQ(JXL_DEC_ERROR,
                JxlDecoderSetPreferredColorProfile(dec, &encoding_out));
      JxlDecoderDestroy(dec);
      continue;
    }
    if (use_cms) {
      JxlDecoderSetCms(dec, *JxlGetDefaultCms());
    }
    if (icc_dst) {
      jxl::ColorEncoding internal_encoding_out;
      EXPECT_TRUE(internal_encoding_out.FromExternal(encoding_out));
      EXPECT_TRUE(internal_encoding_out.CreateICC());
      std::vector<uint8_t> rewritten_icc = internal_encoding_out.ICC();

      EXPECT_EQ(use_cms ? JXL_DEC_SUCCESS : JXL_DEC_ERROR,
                JxlDecoderSetOutputColorProfile(
                    dec, nullptr, rewritten_icc.data(), rewritten_icc.size()));
      if (!use_cms) {
        // continue if we don't have a cms here
        JxlDecoderDestroy(dec);
        continue;
      }
    } else {
      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderSetPreferredColorProfile(dec, &encoding_out));
    }
    EXPECT_EQ(GetOrigProfile(dec), color_space_in);
    if (icc_dst) {
    } else {
      EXPECT_EQ(GetDataProfile(dec), color_space_out);
    }
    EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));
    size_t buffer_size;
    JxlPixelFormat out_format = format;
    out_format.num_channels = c_out.Channels();
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderImageOutBufferSize(dec, &out_format, &buffer_size));
    std::vector<uint8_t> out(buffer_size);
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                   dec, &out_format, out.data(), out.size()));
    EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
    double dist = ButteraugliDistance(xsize, ysize, pixels, c_in, intensity_in,
                                      out, c_out, intensity_out);

    if (c_in.GetWhitePointType() == c_out.GetWhitePointType()) {
      EXPECT_LT(dist, 1.29);
    } else {
      EXPECT_LT(dist, 4.0);
    }
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));
    JxlDecoderDestroy(dec);
  }
}
}  // namespace

TEST(DecodeTest, SetPreferredColorProfileTestFromGray) {
  jxl::test::ColorEncodingDescriptor gray = {
      jxl::ColorSpace::kGray, jxl::WhitePoint::kD65, jxl::Primaries::kSRGB,
      jxl::TransferFunction::kSRGB, jxl::RenderingIntent::kRelative};
  SetPreferredColorProfileTest(gray, true, true);
  SetPreferredColorProfileTest(gray, false, true);
  SetPreferredColorProfileTest(gray, true, false);
  SetPreferredColorProfileTest(gray, false, false);
}

static std::string DecodeAllEncodingsVariantsTestName(
    const ::testing::TestParamInfo<
        std::tuple<jxl::test::ColorEncodingDescriptor, bool, bool>>& info) {
  const auto& encoding = std::get<0>(info.param);
  bool icc_dst = std::get<1>(info.param);
  bool use_cms = std::get<2>(info.param);

  std::string encoding_name =
      Description(ColorEncodingFromDescriptor(encoding));

  return "From_" + encoding_name +
         (icc_dst ? "_with_icc_dst" : "_without_icc_dst") +
         (use_cms ? "_with_cms" : "_without_cms");
}

class DecodeAllEncodingsVariantsTest
    : public ::testing::TestWithParam<
          std::tuple<jxl::test::ColorEncodingDescriptor, bool, bool>> {};
JXL_GTEST_INSTANTIATE_TEST_SUITE_P(
    DecodeAllEncodingsVariantsTestInstantiation, DecodeAllEncodingsVariantsTest,
    ::testing::Combine(::testing::ValuesIn(jxl::test::AllEncodings()),
                       ::testing::Bool(), ::testing::Bool()),
    DecodeAllEncodingsVariantsTestName);
TEST_P(DecodeAllEncodingsVariantsTest, SetPreferredColorProfileTest) {
  const auto& from = std::get<0>(GetParam());
  bool icc_dst = std::get<1>(GetParam());
  bool use_cms = std::get<2>(GetParam());
  SetPreferredColorProfileTest(from, icc_dst, use_cms);
}

void DecodeImageWithColorEncoding(const std::vector<uint8_t>& compressed,
                                  jxl::ColorEncoding& color_encoding,
                                  bool with_cms, std::vector<uint8_t>& out,
                                  JxlBasicInfo& info) {
  JxlDecoder* dec = JxlDecoderCreate(nullptr);
  int events = JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING | JXL_DEC_FULL_IMAGE;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSubscribeEvents(dec, events));
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetInput(dec, compressed.data(), compressed.size()));
  EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
  EXPECT_EQ(JXL_DEC_COLOR_ENCODING, JxlDecoderProcessInput(dec));
  // TODO(eustas): why unused?
  std::string color_space_in = GetOrigProfile(dec);
  if (with_cms) {
    JxlDecoderSetCms(dec, *JxlGetDefaultCms());
    EXPECT_TRUE(color_encoding.CreateICC());
    std::vector<uint8_t> rewritten_icc = color_encoding.ICC();
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderSetOutputColorProfile(
                  dec, nullptr, rewritten_icc.data(), rewritten_icc.size()));
  } else {
    JxlColorEncoding external_color_encoding = color_encoding.ToExternal();
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetOutputColorProfile(
                                   dec, &external_color_encoding, nullptr, 0));
  }
  EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));

  size_t buffer_size;
  JxlPixelFormat format = {3, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};

  JxlPixelFormat out_format = format;
  out_format.num_channels = color_encoding.Channels();
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderImageOutBufferSize(dec, &out_format, &buffer_size));
  out.resize(buffer_size);
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                 dec, &out_format, out.data(), out.size()));
  EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
  JxlDecoderDestroy(dec);
}

class DecodeAllEncodingsWithCMSTest
    : public ::testing::TestWithParam<jxl::test::ColorEncodingDescriptor> {};

JXL_GTEST_INSTANTIATE_TEST_SUITE_P(
    AllEncodings, DecodeAllEncodingsWithCMSTest,
    testing::ValuesIn(jxl::test::AllEncodings()));

TEST_P(DecodeAllEncodingsWithCMSTest, DecodeWithCMS) {
  auto all_encodings = jxl::test::AllEncodings();
  uint32_t num_channels = 3;
  size_t xsize = 177;
  size_t ysize = 123;
  std::vector<uint8_t> pixels =
      jxl::test::GetSomeTestImage(xsize, ysize, num_channels, 0);
  jxl::TestCodestreamParams params;
  std::vector<uint8_t> data =
      jxl::CreateTestJXLCodestream(jxl::Bytes(pixels.data(), pixels.size()),
                                   xsize, ysize, num_channels, params);

  jxl::ColorEncoding color_encoding =
      jxl::test::ColorEncodingFromDescriptor(GetParam());
  fprintf(stderr, "color_description: %s\n",
          Description(color_encoding).c_str());

  std::vector<uint8_t> out_with_cms;
  JxlBasicInfo info_with_cms;
  DecodeImageWithColorEncoding(data, color_encoding, true, out_with_cms,
                               info_with_cms);

  std::vector<uint8_t> out_without_cms;
  JxlBasicInfo info_without_cms;
  DecodeImageWithColorEncoding(data, color_encoding, false, out_without_cms,
                               info_without_cms);

  EXPECT_EQ(info_with_cms.xsize, info_without_cms.xsize);
  EXPECT_EQ(info_with_cms.ysize, info_without_cms.ysize);
  EXPECT_EQ(out_with_cms.size(), out_without_cms.size());
  double dist = ButteraugliDistance(xsize, ysize, out_with_cms, color_encoding,
                                    255, out_without_cms, color_encoding, 255);

  EXPECT_LT(dist, .1);
}

// Tests the case of lossy sRGB image without alpha channel, decoded to RGB8
// and to RGBA8
TEST(DecodeTest, PixelTestOpaqueSrgbLossy) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  for (unsigned channels = 3; channels <= 4; channels++) {
    JxlDecoder* dec = JxlDecoderCreate(nullptr);

    size_t xsize = 123;
    size_t ysize = 77;
    size_t num_pixels = xsize * ysize;
    std::vector<uint8_t> pixels =
        jxl::test::GetSomeTestImage(xsize, ysize, 3, 0);
    JxlPixelFormat format_orig = {3, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
    std::vector<uint8_t> compressed = jxl::CreateTestJXLCodestream(
        jxl::Bytes(pixels.data(), pixels.size()), xsize, ysize, 3,
        jxl::TestCodestreamParams());

    JxlPixelFormat format = {channels, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, 0};

    std::vector<uint8_t> pixels2 = jxl::DecodeWithAPI(
        dec, jxl::Bytes(compressed.data(), compressed.size()), format,
        /*use_callback=*/true, /*set_buffer_early=*/false,
        /*use_resizable_runner=*/false, /*require_boxes=*/false,
        /*expect_success*/ true);
    JxlDecoderReset(dec);
    EXPECT_EQ(num_pixels * channels, pixels2.size());

    jxl::ColorEncoding color_encoding0 = jxl::ColorEncoding::SRGB(false);
    jxl::Span<const uint8_t> span0(pixels.data(), pixels.size());
    jxl::CodecInOut io0{memory_manager};
    ASSERT_TRUE(io0.SetSize(xsize, ysize));
    EXPECT_TRUE(ConvertFromExternal(span0, xsize, ysize, color_encoding0,
                                    /*bits_per_sample=*/16, format_orig,
                                    /*pool=*/nullptr, &io0.Main()));

    jxl::ColorEncoding color_encoding1 = jxl::ColorEncoding::SRGB(false);
    jxl::Span<const uint8_t> span1(pixels2.data(), pixels2.size());
    jxl::CodecInOut io1{memory_manager};
    EXPECT_TRUE(ConvertFromExternal(span1, xsize, ysize, color_encoding1,
                                    /*bits_per_sample=*/8, format,
                                    /*pool=*/nullptr, &io1.Main()));

    jxl::ButteraugliParams butteraugli_params;
    EXPECT_SLIGHTLY_BELOW(
        ButteraugliDistance(io0.frames, io1.frames, butteraugli_params,
                            *JxlGetDefaultCms(),
                            /*distmap=*/nullptr, nullptr),
        0.6f);

    JxlDecoderDestroy(dec);
  }
}

// Opaque image with noise enabled, decoded to RGB8 and RGBA8.
TEST(DecodeTest, PixelTestOpaqueSrgbLossyNoise) {
  for (unsigned channels = 3; channels <= 4; channels++) {
    JxlDecoder* dec = JxlDecoderCreate(nullptr);

    size_t xsize = 512;
    size_t ysize = 300;
    size_t num_pixels = xsize * ysize;
    std::vector<uint8_t> pixels =
        jxl::test::GetSomeTestImage(xsize, ysize, 3, 0);
    JxlPixelFormat format_orig = {3, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
    jxl::TestCodestreamParams params;
    params.cparams.noise = jxl::Override::kOn;
    std::vector<uint8_t> compressed = jxl::CreateTestJXLCodestream(
        jxl::Bytes(pixels.data(), pixels.size()), xsize, ysize, 3, params);

    JxlPixelFormat format = {channels, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, 0};

    std::vector<uint8_t> pixels2 = jxl::DecodeWithAPI(
        dec, jxl::Bytes(compressed.data(), compressed.size()), format,
        /*use_callback=*/false, /*set_buffer_early=*/true,
        /*use_resizable_runner=*/false, /*require_boxes=*/false,
        /*expect_success=*/true);
    JxlDecoderReset(dec);
    EXPECT_EQ(num_pixels * channels, pixels2.size());

    jxl::ColorEncoding color_encoding0 = jxl::ColorEncoding::SRGB(false);
    jxl::Span<const uint8_t> span0(pixels.data(), pixels.size());
    jxl::CodecInOut io0{jxl::test::MemoryManager()};
    ASSERT_TRUE(io0.SetSize(xsize, ysize));
    EXPECT_TRUE(ConvertFromExternal(span0, xsize, ysize, color_encoding0,
                                    /*bits_per_sample=*/16, format_orig,
                                    /*pool=*/nullptr, &io0.Main()));

    jxl::ColorEncoding color_encoding1 = jxl::ColorEncoding::SRGB(false);
    jxl::Span<const uint8_t> span1(pixels2.data(), pixels2.size());
    jxl::CodecInOut io1{jxl::test::MemoryManager()};
    EXPECT_TRUE(ConvertFromExternal(span1, xsize, ysize, color_encoding1,
                                    /*bits_per_sample=*/8, format,
                                    /*pool=*/nullptr, &io1.Main()));

    jxl::ButteraugliParams butteraugli_params;
    EXPECT_SLIGHTLY_BELOW(
        ButteraugliDistance(io0.frames, io1.frames, butteraugli_params,
                            *JxlGetDefaultCms(),
                            /*distmap=*/nullptr, nullptr),
        1.4f);

    JxlDecoderDestroy(dec);
  }
}

TEST(DecodeTest, ProcessEmptyInputWithBoxes) {
  size_t xsize = 123;
  size_t ysize = 77;
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 3, 0);
  jxl::CompressParams cparams;
  uint32_t channels = 3;
  JxlPixelFormat format = {channels, JXL_TYPE_FLOAT, JXL_LITTLE_ENDIAN, 0};
  for (int i = 0; i < kCSBF_NUM_ENTRIES; ++i) {
    JxlDecoder* dec = JxlDecoderCreate(nullptr);
    jxl::TestCodestreamParams params;
    params.box_format = static_cast<CodeStreamBoxFormat>(i);
    printf("Testing empty input with box format %d\n",
           static_cast<int>(params.box_format));
    std::vector<uint8_t> compressed = jxl::CreateTestJXLCodestream(
        jxl::Bytes(pixels.data(), pixels.size()), xsize, ysize, 3, params);
    const int events =
        JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE | JXL_DEC_COLOR_ENCODING;
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSubscribeEvents(dec, events));
    EXPECT_EQ(JXL_DEC_NEED_MORE_INPUT, JxlDecoderProcessInput(dec));
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderSetInput(dec, compressed.data(), compressed.size()));
    EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
    EXPECT_EQ(JXL_DEC_COLOR_ENCODING, JxlDecoderProcessInput(dec));
    size_t buffer_size;
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderImageOutBufferSize(dec, &format, &buffer_size));
    JxlBasicInfo info;
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
    const size_t remaining = JxlDecoderReleaseInput(dec);
    EXPECT_LE(remaining, compressed.size());
    EXPECT_EQ(JXL_DEC_NEED_MORE_INPUT, JxlDecoderProcessInput(dec));
    JxlDecoderDestroy(dec);
  }
}

TEST(DecodeTest, ExtraBytesAfterCompressedStream) {
  size_t xsize = 123;
  size_t ysize = 77;
  size_t num_pixels = xsize * ysize;
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 3, 0);
  jxl::CompressParams cparams;
  for (int i = 0; i < kCSBF_NUM_ENTRIES; ++i) {
    CodeStreamBoxFormat box_format = static_cast<CodeStreamBoxFormat>(i);
    if (box_format == kCSBF_Multi_Other_Zero_Terminated) continue;
    printf("Testing with box format %d\n", static_cast<int>(box_format));
    size_t last_unknown_box_size = 0;
    if (box_format == kCSBF_Single_Other) {
      last_unknown_box_size = unk1_box_size + 8;
    } else if (box_format == kCSBF_Multi_Other_Terminated) {
      last_unknown_box_size = unk3_box_size + 8;
    } else if (box_format == kCSBF_Multi_Last_Empty_Other) {
      // If boxes are not required, the decoder won't consume the last empty
      // jxlp box.
      last_unknown_box_size = 12 + unk3_box_size + 8;
    }
    jxl::TestCodestreamParams params;
    params.box_format = box_format;
    std::vector<uint8_t> compressed = jxl::CreateTestJXLCodestream(
        jxl::Bytes(pixels.data(), pixels.size()), xsize, ysize, 3, params);
    // Add some more bytes after compressed data.
    compressed.push_back(0);
    compressed.push_back(1);
    compressed.push_back(2);
    JxlDecoder* dec = JxlDecoderCreate(nullptr);
    uint32_t channels = 3;
    JxlPixelFormat format = {channels, JXL_TYPE_FLOAT, JXL_LITTLE_ENDIAN, 0};
    std::vector<uint8_t> pixels2 = jxl::DecodeWithAPI(
        dec, jxl::Bytes(compressed.data(), compressed.size()), format,
        /*use_callback=*/false, /*set_buffer_early=*/true,
        /*use_resizable_runner=*/false, /*require_boxes=*/false,
        /*expect_success=*/true);
    size_t unconsumed_bytes = JxlDecoderReleaseInput(dec);
    EXPECT_EQ(last_unknown_box_size + 3, unconsumed_bytes);
    EXPECT_EQ(num_pixels * channels * 4, pixels2.size());
    JxlDecoderDestroy(dec);
  }
}

TEST(DecodeTest, ExtraBytesAfterCompressedStreamRequireBoxes) {
  size_t xsize = 123;
  size_t ysize = 77;
  size_t num_pixels = xsize * ysize;
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 3, 0);
  jxl::CompressParams cparams;
  for (int i = 0; i < kCSBF_NUM_ENTRIES; ++i) {
    CodeStreamBoxFormat box_format = static_cast<CodeStreamBoxFormat>(i);
    if (box_format == kCSBF_Multi_Other_Zero_Terminated) continue;
    printf("Testing with box format %d\n", static_cast<int>(box_format));
    bool expect_success = (box_format == kCSBF_None ||
                           box_format == kCSBF_Single_Zero_Terminated ||
                           box_format == kCSBF_Multi_Zero_Terminated);
    jxl::TestCodestreamParams params;
    params.box_format = box_format;
    std::vector<uint8_t> compressed = jxl::CreateTestJXLCodestream(
        jxl::Bytes(pixels.data(), pixels.size()), xsize, ysize, 3, params);
    // Add some more bytes after compressed data.
    compressed.push_back(0);
    compressed.push_back(1);
    compressed.push_back(2);
    JxlDecoder* dec = JxlDecoderCreate(nullptr);
    uint32_t channels = 3;
    JxlPixelFormat format = {channels, JXL_TYPE_FLOAT, JXL_LITTLE_ENDIAN, 0};
    std::vector<uint8_t> pixels2 = jxl::DecodeWithAPI(
        dec, jxl::Bytes(compressed.data(), compressed.size()), format,
        /*use_callback=*/false, /*set_buffer_early=*/true,
        /*use_resizable_runner=*/false, /*require_boxes=*/true, expect_success);
    size_t unconsumed_bytes = JxlDecoderReleaseInput(dec);
    EXPECT_EQ(3, unconsumed_bytes);
    EXPECT_EQ(num_pixels * channels * 4, pixels2.size());
    JxlDecoderDestroy(dec);
  }
}

TEST(DecodeTest, ConcatenatedCompressedStreams) {
  size_t xsize = 123;
  size_t ysize = 77;
  size_t num_pixels = xsize * ysize;
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 3, 0);
  jxl::CompressParams cparams;
  for (int i = 0; i < kCSBF_NUM_ENTRIES; ++i) {
    CodeStreamBoxFormat first_box_format = static_cast<CodeStreamBoxFormat>(i);
    if (first_box_format == kCSBF_Multi_Other_Zero_Terminated) continue;
    jxl::TestCodestreamParams params1;
    params1.box_format = first_box_format;
    std::vector<uint8_t> compressed1 = jxl::CreateTestJXLCodestream(
        jxl::Bytes(pixels.data(), pixels.size()), xsize, ysize, 3, params1);
    for (int j = 0; j < kCSBF_NUM_ENTRIES; ++j) {
      CodeStreamBoxFormat second_box_format =
          static_cast<CodeStreamBoxFormat>(j);
      if (second_box_format == kCSBF_Multi_Other_Zero_Terminated) continue;
      printf("Testing with box format pair %d, %d\n",
             static_cast<int>(first_box_format),
             static_cast<int>(second_box_format));
      jxl::TestCodestreamParams params2;
      params2.box_format = second_box_format;
      std::vector<uint8_t> compressed2 = jxl::CreateTestJXLCodestream(
          jxl::Bytes(pixels.data(), pixels.size()), xsize, ysize, 3, params2);
      std::vector<uint8_t> concat;
      jxl::Bytes(compressed1).AppendTo(concat);
      jxl::Bytes(compressed2).AppendTo(concat);
      uint32_t channels = 3;
      JxlPixelFormat format = {channels, JXL_TYPE_FLOAT, JXL_LITTLE_ENDIAN, 0};
      size_t remaining = concat.size();
      for (int part = 0; part < 2; ++part) {
        printf("  Decoding part %d\n", part + 1);
        JxlDecoder* dec = JxlDecoderCreate(nullptr);
        size_t pos = concat.size() - remaining;
        bool expect_success =
            (part == 0 || second_box_format == kCSBF_None ||
             second_box_format == kCSBF_Single_Zero_Terminated ||
             second_box_format == kCSBF_Multi_Zero_Terminated);
        std::vector<uint8_t> pixels2 = jxl::DecodeWithAPI(
            dec, jxl::Bytes(concat.data() + pos, remaining), format,
            /*use_callback=*/false, /*set_buffer_early=*/true,
            /*use_resizable_runner=*/false, /*require_boxes=*/true,
            expect_success);
        EXPECT_EQ(num_pixels * channels * 4, pixels2.size());
        remaining = JxlDecoderReleaseInput(dec);
        JxlDecoderDestroy(dec);
      }
      EXPECT_EQ(0, remaining);
    }
  }
}

void TestPartialStream(bool reconstructible_jpeg) {
  size_t xsize = 123;
  size_t ysize = 77;
  uint32_t channels = 4;
  if (reconstructible_jpeg) {
    channels = 3;
  }
  std::vector<uint8_t> pixels =
      jxl::test::GetSomeTestImage(xsize, ysize, channels, 0);
  JxlPixelFormat format_orig = {channels, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
  jxl::TestCodestreamParams params;
  if (reconstructible_jpeg) {
    params.cparams.color_transform = jxl::ColorTransform::kNone;
  } else {
    // Lossless to verify pixels exactly after roundtrip.
    params.cparams.SetLossless();
  }

  std::vector<uint8_t> pixels2;
  pixels2.resize(pixels.size());

  std::vector<uint8_t> jpeg_output(64);
  size_t used_jpeg_output = 0;

  std::vector<std::vector<uint8_t>> codestreams(kCSBF_NUM_ENTRIES);
  std::vector<std::vector<uint8_t>> jpeg_codestreams(kCSBF_NUM_ENTRIES);
  for (size_t i = 0; i < kCSBF_NUM_ENTRIES; ++i) {
    params.box_format = static_cast<CodeStreamBoxFormat>(i);
    if (reconstructible_jpeg) {
      params.jpeg_codestream = &jpeg_codestreams[i];
    }
    codestreams[i] =
        jxl::CreateTestJXLCodestream(jxl::Bytes(pixels.data(), pixels.size()),
                                     xsize, ysize, channels, params);
  }

  // Test multiple step sizes, to test different combinations of the streaming
  // box parsing.
  std::vector<size_t> increments = {1, 3, 17, 23, 120, 700, 1050};

  for (size_t increment : increments) {
    for (size_t i = 0; i < kCSBF_NUM_ENTRIES; ++i) {
      if (reconstructible_jpeg && static_cast<CodeStreamBoxFormat>(i) ==
                                      CodeStreamBoxFormat::kCSBF_None) {
        continue;
      }
      const std::vector<uint8_t>& data = codestreams[i];
      const uint8_t* next_in = data.data();
      size_t avail_in = 0;

      JxlDecoder* dec = JxlDecoderCreate(nullptr);

      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderSubscribeEvents(
                    dec, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE |
                             JXL_DEC_JPEG_RECONSTRUCTION));

      bool seen_basic_info = false;
      bool seen_full_image = false;
      bool seen_jpeg_recon = false;

      size_t total_size = 0;

      for (;;) {
        EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));
        JxlDecoderStatus status = JxlDecoderProcessInput(dec);
        size_t remaining = JxlDecoderReleaseInput(dec);
        EXPECT_LE(remaining, avail_in);
        next_in += avail_in - remaining;
        avail_in = remaining;
        if (status == JXL_DEC_NEED_MORE_INPUT) {
          if (total_size >= data.size()) {
            // End of test data reached, it should have successfully decoded the
            // image now.
            FAIL();
            break;
          }

          // End of the file reached, should be the final test.
          if (total_size + increment > data.size()) {
            increment = data.size() - total_size;
          }
          total_size += increment;
          avail_in += increment;
        } else if (status == JXL_DEC_BASIC_INFO) {
          // This event should happen exactly once
          EXPECT_FALSE(seen_basic_info);
          if (seen_basic_info) break;
          seen_basic_info = true;
          JxlBasicInfo info;
          EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
          EXPECT_EQ(info.xsize, xsize);
          EXPECT_EQ(info.ysize, ysize);
        } else if (status == JXL_DEC_JPEG_RECONSTRUCTION) {
          EXPECT_FALSE(seen_basic_info);
          EXPECT_FALSE(seen_full_image);
          EXPECT_EQ(JXL_DEC_SUCCESS,
                    JxlDecoderSetJPEGBuffer(dec, jpeg_output.data(),
                                            jpeg_output.size()));
          seen_jpeg_recon = true;
        } else if (status == JXL_DEC_JPEG_NEED_MORE_OUTPUT) {
          EXPECT_TRUE(seen_jpeg_recon);
          used_jpeg_output =
              jpeg_output.size() - JxlDecoderReleaseJPEGBuffer(dec);
          jpeg_output.resize(jpeg_output.size() * 2);
          EXPECT_EQ(JXL_DEC_SUCCESS,
                    JxlDecoderSetJPEGBuffer(
                        dec, jpeg_output.data() + used_jpeg_output,
                        jpeg_output.size() - used_jpeg_output));
        } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
          EXPECT_EQ(JXL_DEC_SUCCESS,
                    JxlDecoderSetImageOutBuffer(
                        dec, &format_orig, pixels2.data(), pixels2.size()));
        } else if (status == JXL_DEC_FULL_IMAGE) {
          // This event should happen exactly once
          EXPECT_FALSE(seen_full_image);
          if (seen_full_image) break;
          // This event should happen after basic info
          EXPECT_TRUE(seen_basic_info);
          seen_full_image = true;
          if (reconstructible_jpeg) {
            used_jpeg_output =
                jpeg_output.size() - JxlDecoderReleaseJPEGBuffer(dec);
            EXPECT_EQ(used_jpeg_output, jpeg_codestreams[i].size());
            EXPECT_EQ(0, memcmp(jpeg_output.data(), jpeg_codestreams[i].data(),
                                used_jpeg_output));
          } else {
            EXPECT_EQ(pixels, pixels2);
          }
        } else if (status == JXL_DEC_SUCCESS) {
          EXPECT_TRUE(seen_full_image);
          break;
        } else {
          // We do not expect any other events or errors
          FAIL();
          break;
        }
      }

      // Ensure the decoder emitted the basic info and full image events
      EXPECT_TRUE(seen_basic_info);
      EXPECT_TRUE(seen_full_image);

      JxlDecoderDestroy(dec);
    }
  }
}

// Tests the return status when trying to decode pixels on incomplete file: it
// should return JXL_DEC_NEED_MORE_INPUT, not error.
TEST(DecodeTest, PixelPartialTest) { TestPartialStream(false); }

// Tests the return status when trying to decode JPEG bytes on incomplete file.
JXL_TRANSCODE_JPEG_TEST(DecodeTest, JPEGPartialTest) {
  TEST_LIBJPEG_SUPPORT();
  TestPartialStream(true);
}

// The DC event still exists, but is no longer implemented, it is deprecated.
TEST(DecodeTest, DCNotGettableTest) {
  // 1x1 pixel JXL image
  std::string compressed(
      "\377\n\0\20\260\23\0H\200("
      "\0\334\0U\17\0\0\250P\31e\334\340\345\\\317\227\37:,"
      "\246m\\gh\253m\vK\22E\306\261I\252C&pH\22\353 "
      "\363\6\22\bp\0\200\237\34\231W2d\255$\1",
      68);

  JxlDecoder* dec = JxlDecoderCreate(nullptr);

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO));
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetInput(
                dec, reinterpret_cast<const uint8_t*>(compressed.data()),
                compressed.size()));

  EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));

  // Since the image is only 1x1 pixel, there is only 1 group, the decoder is
  // unable to get DC size from this, and will not return the DC at all. Since
  // no full image is requested either, it is expected to return success.
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));

  JxlDecoderDestroy(dec);
}

TEST(DecodeTest, PreviewTest) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  size_t xsize = 77;
  size_t ysize = 120;
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 3, 0);
  JxlPixelFormat format_orig = {3, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
  for (jxl::PreviewMode mode : {jxl::kSmallPreview, jxl::kBigPreview}) {
    jxl::TestCodestreamParams params;
    params.preview_mode = mode;

    std::vector<uint8_t> compressed = jxl::CreateTestJXLCodestream(
        jxl::Bytes(pixels.data(), pixels.size()), xsize, ysize, 3, params);

    JxlPixelFormat format = {3, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, 0};

    JxlDecoder* dec = JxlDecoderCreate(nullptr);
    const uint8_t* next_in = compressed.data();
    size_t avail_in = compressed.size();

    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderSubscribeEvents(
                  dec, JXL_DEC_BASIC_INFO | JXL_DEC_PREVIEW_IMAGE));
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));

    EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
    JxlBasicInfo info;
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
    size_t buffer_size;
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderPreviewOutBufferSize(dec, &format, &buffer_size));

    jxl::ColorEncoding c_srgb = jxl::ColorEncoding::SRGB(false);
    jxl::CodecInOut io0{memory_manager};
    EXPECT_TRUE(jxl::ConvertFromExternal(
        jxl::Bytes(pixels.data(), pixels.size()), xsize, ysize, c_srgb,
        /*bits_per_sample=*/16, format_orig, /*pool=*/nullptr, &io0.Main()));
    GeneratePreview(params.preview_mode, &io0.Main());

    size_t xsize_preview = io0.Main().xsize();
    size_t ysize_preview = io0.Main().ysize();
    EXPECT_EQ(xsize_preview, info.preview.xsize);
    EXPECT_EQ(ysize_preview, info.preview.ysize);
    EXPECT_EQ(xsize_preview * ysize_preview * 3, buffer_size);

    EXPECT_EQ(JXL_DEC_NEED_PREVIEW_OUT_BUFFER, JxlDecoderProcessInput(dec));

    std::vector<uint8_t> preview(buffer_size);
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderSetPreviewOutBuffer(dec, &format, preview.data(),
                                            preview.size()));

    EXPECT_EQ(JXL_DEC_PREVIEW_IMAGE, JxlDecoderProcessInput(dec));

    jxl::CodecInOut io1{memory_manager};
    EXPECT_TRUE(
        jxl::ConvertFromExternal(jxl::Bytes(preview.data(), preview.size()),
                                 xsize_preview, ysize_preview, c_srgb,
                                 /*bits_per_sample=*/8, format,
                                 /*pool=*/nullptr, &io1.Main()));

    jxl::ButteraugliParams butteraugli_params;
    // TODO(lode): this ButteraugliDistance silently returns 0 (dangerous for
    // tests) if xsize or ysize is < 8, no matter how different the images, a
    // tiny size that could happen for a preview. ButteraugliDiffmap does
    // support smaller than 8x8, but jxl's ButteraugliDistance does not. Perhaps
    // move butteraugli's <8x8 handling from ButteraugliDiffmap to
    // ButteraugliComparator::Diffmap in butteraugli.cc.
    EXPECT_LE(ButteraugliDistance(io0.frames, io1.frames, butteraugli_params,
                                  *JxlGetDefaultCms(),
                                  /*distmap=*/nullptr, nullptr),
              mode == jxl::kSmallPreview ? 0.7f : 1.2f);

    JxlDecoderDestroy(dec);
  }
}

TEST(DecodeTest, AlignTest) {
  size_t xsize = 123;
  size_t ysize = 77;
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);
  JxlPixelFormat format_orig = {4, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};

  jxl::TestCodestreamParams params;
  // Lossless to verify pixels exactly after roundtrip.
  params.cparams.SetLossless();
  params.cparams.speed_tier = jxl::SpeedTier::kThunder;
  std::vector<uint8_t> compressed = jxl::CreateTestJXLCodestream(
      jxl::Bytes(pixels.data(), pixels.size()), xsize, ysize, 4, params);

  size_t align = 17;
  JxlPixelFormat format = {3, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, align};
  // On purpose not using jxl::RoundUpTo to test it independently.
  size_t expected_line_size_last = 1 * 3 * xsize;
  size_t expected_line_size =
      ((expected_line_size_last + align - 1) / align) * align;
  size_t expected_pixels_size =
      expected_line_size * (ysize - 1) + expected_line_size_last;

  for (bool use_callback : {false, true}) {
    std::vector<uint8_t> pixels2 = jxl::DecodeWithAPI(
        jxl::Bytes(compressed.data(), compressed.size()), format, use_callback,
        /*set_buffer_early=*/false,
        /*use_resizable_runner=*/false, /*require_boxes=*/false,
        /*expect_success=*/true);
    EXPECT_EQ(expected_pixels_size, pixels2.size());
    EXPECT_EQ(0u, jxl::test::ComparePixels(pixels.data(), pixels2.data(), xsize,
                                           ysize, format_orig, format));
  }
}

TEST(DecodeTest, AnimationTest) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  size_t xsize = 123;
  size_t ysize = 77;
  static const size_t num_frames = 2;
  std::vector<uint8_t> frames[2];
  frames[0] = jxl::test::GetSomeTestImage(xsize, ysize, 3, 0);
  frames[1] = jxl::test::GetSomeTestImage(xsize, ysize, 3, 1);
  JxlPixelFormat format = {3, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};

  jxl::CodecInOut io{memory_manager};
  ASSERT_TRUE(io.SetSize(xsize, ysize));
  io.metadata.m.SetUintSamples(16);
  io.metadata.m.color_encoding = jxl::ColorEncoding::SRGB(false);
  io.metadata.m.have_animation = true;
  io.frames.clear();
  io.frames.reserve(num_frames);
  ASSERT_TRUE(io.SetSize(xsize, ysize));

  std::vector<uint32_t> frame_durations(num_frames);
  for (size_t i = 0; i < num_frames; ++i) {
    frame_durations[i] = 5 + i;
  }

  for (size_t i = 0; i < num_frames; ++i) {
    jxl::ImageBundle bundle(memory_manager, &io.metadata.m);

    EXPECT_TRUE(ConvertFromExternal(
        jxl::Bytes(frames[i].data(), frames[i].size()), xsize, ysize,
        jxl::ColorEncoding::SRGB(/*is_gray=*/false),
        /*bits_per_sample=*/16, format,
        /*pool=*/nullptr, &bundle));
    bundle.duration = frame_durations[i];
    io.frames.push_back(std::move(bundle));
  }

  jxl::CompressParams cparams;
  cparams.SetLossless();  // Lossless to verify pixels exactly after roundtrip.
  cparams.speed_tier = jxl::SpeedTier::kThunder;
  std::vector<uint8_t> compressed;
  EXPECT_TRUE(jxl::test::EncodeFile(cparams, &io, &compressed));

  // Decode and test the animation frames

  JxlDecoder* dec = JxlDecoderCreate(nullptr);
  const uint8_t* next_in = compressed.data();
  size_t avail_in = compressed.size();

  void* runner = JxlThreadParallelRunnerCreate(
      nullptr, JxlThreadParallelRunnerDefaultNumWorkerThreads());
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetParallelRunner(dec, JxlThreadParallelRunner, runner));

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(
                dec, JXL_DEC_BASIC_INFO | JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE));
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));

  EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
  size_t buffer_size;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderImageOutBufferSize(dec, &format, &buffer_size));
  JxlBasicInfo info;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));

  for (size_t i = 0; i < num_frames; ++i) {
    std::vector<uint8_t> pixels(buffer_size);

    EXPECT_EQ(JXL_DEC_FRAME, JxlDecoderProcessInput(dec));

    JxlFrameHeader frame_header;
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetFrameHeader(dec, &frame_header));
    EXPECT_EQ(frame_durations[i], frame_header.duration);
    EXPECT_EQ(0u, frame_header.name_length);
    // For now, test with empty name, there's currently no easy way to encode
    // a jxl file with a frame name because ImageBundle doesn't have a
    // jxl::FrameHeader to set the name in. We can test the null termination
    // character though.
    char name;
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetFrameName(dec, &name, 1));
    EXPECT_EQ(0, name);

    EXPECT_EQ(i + 1 == num_frames, frame_header.is_last);

    EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));

    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                   dec, &format, pixels.data(), pixels.size()));

    EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
    EXPECT_EQ(0u, jxl::test::ComparePixels(frames[i].data(), pixels.data(),
                                           xsize, ysize, format, format));
  }

  // After all frames were decoded, JxlDecoderProcessInput should return
  // success to indicate all is done.
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));

  JxlThreadParallelRunnerDestroy(runner);
  JxlDecoderDestroy(dec);
}

TEST(DecodeTest, AnimationTestStreaming) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  size_t xsize = 123;
  size_t ysize = 77;
  static const size_t num_frames = 2;
  std::vector<uint8_t> frames[2];
  frames[0] = jxl::test::GetSomeTestImage(xsize, ysize, 3, 0);
  frames[1] = jxl::test::GetSomeTestImage(xsize, ysize, 3, 1);
  JxlPixelFormat format = {3, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};

  jxl::CodecInOut io{memory_manager};
  ASSERT_TRUE(io.SetSize(xsize, ysize));
  io.metadata.m.SetUintSamples(16);
  io.metadata.m.color_encoding = jxl::ColorEncoding::SRGB(false);
  io.metadata.m.have_animation = true;
  io.frames.clear();
  io.frames.reserve(num_frames);
  ASSERT_TRUE(io.SetSize(xsize, ysize));

  std::vector<uint32_t> frame_durations(num_frames);
  for (size_t i = 0; i < num_frames; ++i) {
    frame_durations[i] = 5 + i;
  }

  for (size_t i = 0; i < num_frames; ++i) {
    jxl::ImageBundle bundle(memory_manager, &io.metadata.m);

    EXPECT_TRUE(ConvertFromExternal(
        jxl::Bytes(frames[i].data(), frames[i].size()), xsize, ysize,
        jxl::ColorEncoding::SRGB(/*is_gray=*/false),
        /*bits_per_sample=*/16, format,
        /*pool=*/nullptr, &bundle));
    bundle.duration = frame_durations[i];
    io.frames.push_back(std::move(bundle));
  }

  jxl::CompressParams cparams;
  cparams.SetLossless();  // Lossless to verify pixels exactly after roundtrip.
  cparams.speed_tier = jxl::SpeedTier::kThunder;
  std::vector<uint8_t> compressed;
  EXPECT_TRUE(jxl::test::EncodeFile(cparams, &io, &compressed));

  // Decode and test the animation frames

  const size_t step_size = 16;

  JxlDecoder* dec = JxlDecoderCreate(nullptr);
  const uint8_t* next_in = compressed.data();
  size_t avail_in = 0;
  size_t frame_headers_seen = 0;
  size_t frames_seen = 0;
  bool seen_basic_info = false;

  void* runner = JxlThreadParallelRunnerCreate(
      nullptr, JxlThreadParallelRunnerDefaultNumWorkerThreads());
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetParallelRunner(dec, JxlThreadParallelRunner, runner));

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(
                dec, JXL_DEC_BASIC_INFO | JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE));

  std::vector<uint8_t> frames2[2];
  for (size_t i = 0; i < num_frames; ++i) {
    frames2[i].resize(frames[i].size());
  }

  size_t total_in = 0;
  size_t loop_count = 0;

  for (;;) {
    if (loop_count++ > compressed.size()) {
      fprintf(stderr, "Too many loops\n");
      FAIL();
      break;
    }

    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));
    auto status = JxlDecoderProcessInput(dec);
    size_t remaining = JxlDecoderReleaseInput(dec);
    EXPECT_LE(remaining, avail_in);
    next_in += avail_in - remaining;
    avail_in = remaining;

    if (status == JXL_DEC_SUCCESS) {
      break;
    } else if (status == JXL_DEC_ERROR) {
      FAIL();
    } else if (status == JXL_DEC_NEED_MORE_INPUT) {
      if (total_in >= compressed.size()) {
        fprintf(stderr, "Already gave all input data\n");
        FAIL();
        break;
      }
      size_t amount = step_size;
      if (total_in + amount > compressed.size()) {
        amount = compressed.size() - total_in;
      }
      avail_in += amount;
      total_in += amount;
    } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                     dec, &format, frames2[frames_seen].data(),
                                     frames2[frames_seen].size()));
    } else if (status == JXL_DEC_BASIC_INFO) {
      EXPECT_EQ(false, seen_basic_info);
      seen_basic_info = true;
      JxlBasicInfo info;
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
      EXPECT_EQ(xsize, info.xsize);
      EXPECT_EQ(ysize, info.ysize);
    } else if (status == JXL_DEC_FRAME) {
      EXPECT_EQ(true, seen_basic_info);
      frame_headers_seen++;
    } else if (status == JXL_DEC_FULL_IMAGE) {
      frames_seen++;
      EXPECT_EQ(frame_headers_seen, frames_seen);
    } else {
      fprintf(stderr, "Unexpected status: %d\n", static_cast<int>(status));
      FAIL();
    }
  }

  EXPECT_EQ(true, seen_basic_info);
  EXPECT_EQ(num_frames, frames_seen);
  EXPECT_EQ(num_frames, frame_headers_seen);
  for (size_t i = 0; i < num_frames; ++i) {
    EXPECT_EQ(frames[i], frames2[i]);
  }

  JxlThreadParallelRunnerDestroy(runner);
  JxlDecoderDestroy(dec);
}

TEST(DecodeTest, ExtraChannelTest) {
  size_t xsize = 55;
  size_t ysize = 257;
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);
  JxlPixelFormat format_orig = {4, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};

  jxl::TestCodestreamParams params;
  // Lossless to verify pixels exactly after roundtrip.
  params.cparams.SetLossless();
  params.cparams.speed_tier = jxl::SpeedTier::kThunder;
  std::vector<uint8_t> compressed = jxl::CreateTestJXLCodestream(
      jxl::Bytes(pixels.data(), pixels.size()), xsize, ysize, 4, params);

  size_t align = 17;
  JxlPixelFormat format = {3, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, align};

  JxlDecoder* dec = JxlDecoderCreate(nullptr);

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSubscribeEvents(
                                 dec, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE));

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetInput(dec, compressed.data(), compressed.size()));
  EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
  JxlBasicInfo info;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
  EXPECT_EQ(1u, info.num_extra_channels);
  EXPECT_EQ(JXL_FALSE, info.alpha_premultiplied);

  JxlExtraChannelInfo extra_info;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderGetExtraChannelInfo(dec, 0, &extra_info));
  EXPECT_EQ(0, extra_info.type);

  EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));
  size_t buffer_size;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderImageOutBufferSize(dec, &format, &buffer_size));
  size_t extra_size;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderExtraChannelBufferSize(dec, &format, &extra_size, 0));

  std::vector<uint8_t> image(buffer_size);
  std::vector<uint8_t> extra(extra_size);

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                 dec, &format, image.data(), image.size()));
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetExtraChannelBuffer(
                                 dec, &format, extra.data(), extra.size(), 0));

  EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));

  // After the full image was output, JxlDecoderProcessInput should return
  // success to indicate all is done.
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));
  JxlDecoderDestroy(dec);

  EXPECT_EQ(0u, jxl::test::ComparePixels(pixels.data(), image.data(), xsize,
                                         ysize, format_orig, format));

  // Compare the extracted extra channel with the original alpha channel

  std::vector<uint8_t> alpha(pixels.size() / 4);
  for (size_t i = 0; i < pixels.size(); i += 8) {
    size_t index_alpha = i / 4;
    alpha[index_alpha + 0] = pixels[i + 6];
    alpha[index_alpha + 1] = pixels[i + 7];
  }
  JxlPixelFormat format_alpha = format;
  format_alpha.num_channels = 1;
  JxlPixelFormat format_orig_alpha = format_orig;
  format_orig_alpha.num_channels = 1;

  EXPECT_EQ(0u,
            jxl::test::ComparePixels(alpha.data(), extra.data(), xsize, ysize,
                                     format_orig_alpha, format_alpha));
}

TEST(DecodeTest, SkipCurrentFrameTest) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  size_t xsize = 90;
  size_t ysize = 120;
  constexpr size_t num_frames = 7;
  std::vector<uint8_t> frames[num_frames];
  for (size_t i = 0; i < num_frames; i++) {
    frames[i] = jxl::test::GetSomeTestImage(xsize, ysize, 3, i);
  }
  JxlPixelFormat format = {3, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};

  jxl::CodecInOut io{memory_manager};
  ASSERT_TRUE(io.SetSize(xsize, ysize));
  io.metadata.m.SetUintSamples(16);
  io.metadata.m.color_encoding = jxl::ColorEncoding::SRGB(false);
  io.metadata.m.have_animation = true;
  io.frames.clear();
  io.frames.reserve(num_frames);
  ASSERT_TRUE(io.SetSize(xsize, ysize));

  std::vector<uint32_t> frame_durations(num_frames);
  for (size_t i = 0; i < num_frames; ++i) {
    frame_durations[i] = 5 + i;
  }

  for (size_t i = 0; i < num_frames; ++i) {
    jxl::ImageBundle bundle(memory_manager, &io.metadata.m);
    if (i & 1) {
      // Mark some frames as referenceable, others not.
      bundle.use_for_next_frame = true;
    }

    EXPECT_TRUE(ConvertFromExternal(
        jxl::Bytes(frames[i].data(), frames[i].size()), xsize, ysize,
        jxl::ColorEncoding::SRGB(/*is_gray=*/false),
        /*bits_per_sample=*/16, format,
        /*pool=*/nullptr, &bundle));
    bundle.duration = frame_durations[i];
    io.frames.push_back(std::move(bundle));
  }

  jxl::CompressParams cparams;
  cparams.speed_tier = jxl::SpeedTier::kThunder;
  std::vector<uint8_t> compressed;
  jxl::PassDefinition passes[] = {{2, 0, 4}, {4, 0, 4}, {8, 2, 2}, {8, 0, 1}};
  jxl::ProgressiveMode progressive_mode{passes};
  cparams.custom_progressive_mode = &progressive_mode;
  EXPECT_TRUE(jxl::test::EncodeFile(cparams, &io, &compressed));

  JxlDecoder* dec = JxlDecoderCreate(nullptr);
  const uint8_t* next_in = compressed.data();
  size_t avail_in = compressed.size();

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO | JXL_DEC_FRAME |
                                               JXL_DEC_FRAME_PROGRESSION |
                                               JXL_DEC_FULL_IMAGE));
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetProgressiveDetail(dec, kLastPasses));
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));

  EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
  size_t buffer_size;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderImageOutBufferSize(dec, &format, &buffer_size));
  JxlBasicInfo info;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));

  for (size_t i = 0; i < num_frames; ++i) {
    printf("Decoding frame %d\n", static_cast<int>(i));
    EXPECT_EQ(JXL_DEC_ERROR, JxlDecoderSkipCurrentFrame(dec));
    std::vector<uint8_t> pixels(buffer_size);
    EXPECT_EQ(JXL_DEC_FRAME, JxlDecoderProcessInput(dec));
    EXPECT_EQ(JXL_DEC_ERROR, JxlDecoderSkipCurrentFrame(dec));
    JxlFrameHeader frame_header;
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetFrameHeader(dec, &frame_header));
    EXPECT_EQ(frame_durations[i], frame_header.duration);
    EXPECT_EQ(i + 1 == num_frames, frame_header.is_last);
    EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                   dec, &format, pixels.data(), pixels.size()));
    if (i == 2) {
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSkipCurrentFrame(dec));
      continue;
    }
    EXPECT_EQ(JXL_DEC_FRAME_PROGRESSION, JxlDecoderProcessInput(dec));
    EXPECT_EQ(8, JxlDecoderGetIntendedDownsamplingRatio(dec));
    if (i == 3) {
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSkipCurrentFrame(dec));
      continue;
    }
    EXPECT_EQ(JXL_DEC_FRAME_PROGRESSION, JxlDecoderProcessInput(dec));
    EXPECT_EQ(4, JxlDecoderGetIntendedDownsamplingRatio(dec));
    if (i == 4) {
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSkipCurrentFrame(dec));
      continue;
    }
    EXPECT_EQ(JXL_DEC_FRAME_PROGRESSION, JxlDecoderProcessInput(dec));
    EXPECT_EQ(2, JxlDecoderGetIntendedDownsamplingRatio(dec));
    if (i == 5) {
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSkipCurrentFrame(dec));
      continue;
    }
    EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
    EXPECT_EQ(JXL_DEC_ERROR, JxlDecoderSkipCurrentFrame(dec));
  }

  // After all frames were decoded, JxlDecoderProcessInput should return
  // success to indicate all is done.
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));

  JxlDecoderDestroy(dec);
}

TEST(DecodeTest, SkipFrameTest) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  size_t xsize = 90;
  size_t ysize = 120;
  constexpr size_t num_frames = 16;
  std::vector<uint8_t> frames[num_frames];
  for (size_t i = 0; i < num_frames; i++) {
    frames[i] = jxl::test::GetSomeTestImage(xsize, ysize, 3, i);
  }
  JxlPixelFormat format = {3, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};

  jxl::CodecInOut io{memory_manager};
  ASSERT_TRUE(io.SetSize(xsize, ysize));
  io.metadata.m.SetUintSamples(16);
  io.metadata.m.color_encoding = jxl::ColorEncoding::SRGB(false);
  io.metadata.m.have_animation = true;
  io.frames.clear();
  io.frames.reserve(num_frames);
  ASSERT_TRUE(io.SetSize(xsize, ysize));

  std::vector<uint32_t> frame_durations(num_frames);
  for (size_t i = 0; i < num_frames; ++i) {
    frame_durations[i] = 5 + i;
  }

  for (size_t i = 0; i < num_frames; ++i) {
    jxl::ImageBundle bundle(memory_manager, &io.metadata.m);
    if (i & 1) {
      // Mark some frames as referenceable, others not.
      bundle.use_for_next_frame = true;
    }

    EXPECT_TRUE(ConvertFromExternal(
        jxl::Bytes(frames[i].data(), frames[i].size()), xsize, ysize,
        jxl::ColorEncoding::SRGB(/*is_gray=*/false),
        /*bits_per_sample=*/16, format,
        /*pool=*/nullptr, &bundle));
    bundle.duration = frame_durations[i];
    io.frames.push_back(std::move(bundle));
  }

  jxl::CompressParams cparams;
  cparams.SetLossless();  // Lossless to verify pixels exactly after roundtrip.
  cparams.speed_tier = jxl::SpeedTier::kThunder;
  std::vector<uint8_t> compressed;
  EXPECT_TRUE(jxl::test::EncodeFile(cparams, &io, &compressed));

  // Decode and test the animation frames

  JxlDecoder* dec = JxlDecoderCreate(nullptr);
  const uint8_t* next_in = compressed.data();
  size_t avail_in = compressed.size();

  void* runner = JxlThreadParallelRunnerCreate(
      nullptr, JxlThreadParallelRunnerDefaultNumWorkerThreads());
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetParallelRunner(dec, JxlThreadParallelRunner, runner));

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(
                dec, JXL_DEC_BASIC_INFO | JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE));
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));

  EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
  size_t buffer_size;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderImageOutBufferSize(dec, &format, &buffer_size));
  JxlBasicInfo info;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));

  for (size_t i = 0; i < num_frames; ++i) {
    if (i == 3) {
      JxlDecoderSkipFrames(dec, 5);
      i += 5;
    }
    std::vector<uint8_t> pixels(buffer_size);

    EXPECT_EQ(JXL_DEC_FRAME, JxlDecoderProcessInput(dec));

    JxlFrameHeader frame_header;
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetFrameHeader(dec, &frame_header));
    EXPECT_EQ(frame_durations[i], frame_header.duration);

    EXPECT_EQ(i + 1 == num_frames, frame_header.is_last);

    EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));

    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                   dec, &format, pixels.data(), pixels.size()));

    EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
    EXPECT_EQ(0u, jxl::test::ComparePixels(frames[i].data(), pixels.data(),
                                           xsize, ysize, format, format));
  }

  // After all frames were decoded, JxlDecoderProcessInput should return
  // success to indicate all is done.
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));

  // Test rewinding the decoder and skipping different frames

  JxlDecoderRewind(dec);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(dec, JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE));
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));

  for (size_t i = 0; i < num_frames; ++i) {
    int test_skipping = (i == 9) ? 3 : 0;
    std::vector<uint8_t> pixels(buffer_size);

    EXPECT_EQ(JXL_DEC_FRAME, JxlDecoderProcessInput(dec));

    // Since this is after JXL_DEC_FRAME but before JXL_DEC_FULL_IMAGE, this
    // should only skip the next frame, not the currently processed one.
    if (test_skipping) JxlDecoderSkipFrames(dec, test_skipping);

    JxlFrameHeader frame_header;
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetFrameHeader(dec, &frame_header));
    EXPECT_EQ(frame_durations[i], frame_header.duration);

    EXPECT_EQ(i + 1 == num_frames, frame_header.is_last);

    EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));

    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                   dec, &format, pixels.data(), pixels.size()));

    EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
    EXPECT_EQ(0u, jxl::test::ComparePixels(frames[i].data(), pixels.data(),
                                           xsize, ysize, format, format));

    if (test_skipping) i += test_skipping;
  }

  JxlThreadParallelRunnerDestroy(runner);
  JxlDecoderDestroy(dec);
}

TEST(DecodeTest, SkipFrameWithBlendingTest) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  size_t xsize = 90;
  size_t ysize = 120;
  constexpr size_t num_frames = 16;
  std::vector<uint8_t> frames[num_frames];
  JxlPixelFormat format = {3, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};

  jxl::CodecInOut io{memory_manager};
  ASSERT_TRUE(io.SetSize(xsize, ysize));
  io.metadata.m.SetUintSamples(16);
  io.metadata.m.color_encoding = jxl::ColorEncoding::SRGB(false);
  io.metadata.m.have_animation = true;
  io.frames.clear();
  io.frames.reserve(num_frames);
  ASSERT_TRUE(io.SetSize(xsize, ysize));

  std::vector<uint32_t> frame_durations(num_frames);

  for (size_t i = 0; i < num_frames; ++i) {
    if (i < 5) {
      std::vector<uint8_t> frame_internal =
          jxl::test::GetSomeTestImage(xsize, ysize, 3, i * 2 + 1);
      // An internal frame with 0 duration, and use_for_next_frame, this is a
      // frame that is not rendered and not output by the API, but on which the
      // rendered frames depend
      jxl::ImageBundle bundle_internal(memory_manager, &io.metadata.m);
      EXPECT_TRUE(ConvertFromExternal(
          jxl::Bytes(frame_internal.data(), frame_internal.size()), xsize,
          ysize, jxl::ColorEncoding::SRGB(/*is_gray=*/false),
          /*bits_per_sample=*/16, format,
          /*pool=*/nullptr, &bundle_internal));
      bundle_internal.duration = 0;
      bundle_internal.use_for_next_frame = true;
      io.frames.push_back(std::move(bundle_internal));
    }

    std::vector<uint8_t> frame =
        jxl::test::GetSomeTestImage(xsize, ysize, 3, i * 2);
    // Actual rendered frame
    frame_durations[i] = 5 + i;
    jxl::ImageBundle bundle(memory_manager, &io.metadata.m);
    EXPECT_TRUE(ConvertFromExternal(jxl::Bytes(frame.data(), frame.size()),
                                    xsize, ysize,
                                    jxl::ColorEncoding::SRGB(/*is_gray=*/false),
                                    /*bits_per_sample=*/16, format,
                                    /*pool=*/nullptr, &bundle));
    bundle.duration = frame_durations[i];
    // Create some variation in which frames depend on which.
    if (i != 3 && i != 9 && i != 10) {
      bundle.use_for_next_frame = true;
    }
    if (i != 12) {
      bundle.blend = true;
      // Choose a blend mode that depends on the pixels of the saved frame and
      // doesn't use alpha
      bundle.blendmode = jxl::BlendMode::kMul;
    }
    io.frames.push_back(std::move(bundle));
  }

  jxl::CompressParams cparams;
  cparams.SetLossless();  // Lossless to verify pixels exactly after roundtrip.
  cparams.speed_tier = jxl::SpeedTier::kThunder;
  std::vector<uint8_t> compressed;
  EXPECT_TRUE(jxl::test::EncodeFile(cparams, &io, &compressed));

  // Independently decode all frames without any skipping, to create the
  // expected blended frames, for the actual tests below to compare with.
  {
    JxlDecoder* dec = JxlDecoderCreate(nullptr);
    const uint8_t* next_in = compressed.data();
    size_t avail_in = compressed.size();

    void* runner = JxlThreadParallelRunnerCreate(
        nullptr, JxlThreadParallelRunnerDefaultNumWorkerThreads());
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetParallelRunner(
                                   dec, JxlThreadParallelRunner, runner));
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderSubscribeEvents(dec, JXL_DEC_FULL_IMAGE));
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));
    for (auto& frame : frames) {
      EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));
      frame.resize(xsize * ysize * 6);
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                     dec, &format, frame.data(), frame.size()));
      EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
    }

    // After all frames were decoded, JxlDecoderProcessInput should return
    // success to indicate all is done.
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));
    JxlThreadParallelRunnerDestroy(runner);
    JxlDecoderDestroy(dec);
  }

  JxlDecoder* dec = JxlDecoderCreate(nullptr);
  const uint8_t* next_in = compressed.data();
  size_t avail_in = compressed.size();

  void* runner = JxlThreadParallelRunnerCreate(
      nullptr, JxlThreadParallelRunnerDefaultNumWorkerThreads());
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetParallelRunner(dec, JxlThreadParallelRunner, runner));

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(
                dec, JXL_DEC_BASIC_INFO | JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE));
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));
  EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
  size_t buffer_size;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderImageOutBufferSize(dec, &format, &buffer_size));
  JxlBasicInfo info;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));

  for (size_t i = 0; i < num_frames; ++i) {
    std::vector<uint8_t> pixels(buffer_size);

    EXPECT_EQ(JXL_DEC_FRAME, JxlDecoderProcessInput(dec));

    JxlFrameHeader frame_header;
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetFrameHeader(dec, &frame_header));
    EXPECT_EQ(frame_durations[i], frame_header.duration);

    EXPECT_EQ(i + 1 == num_frames, frame_header.is_last);

    EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));

    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                   dec, &format, pixels.data(), pixels.size()));

    EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
    EXPECT_EQ(0u, jxl::test::ComparePixels(frames[i].data(), pixels.data(),
                                           xsize, ysize, format, format));

    // Test rewinding mid-way, not decoding all frames.
    if (i == 8) {
      break;
    }
  }

  JxlDecoderRewind(dec);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(dec, JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE));
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));

  for (size_t i = 0; i < num_frames; ++i) {
    if (i == 3) {
      JxlDecoderSkipFrames(dec, 5);
      i += 5;
    }
    std::vector<uint8_t> pixels(buffer_size);

    EXPECT_EQ(JXL_DEC_FRAME, JxlDecoderProcessInput(dec));

    JxlFrameHeader frame_header;
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetFrameHeader(dec, &frame_header));
    EXPECT_EQ(frame_durations[i], frame_header.duration);

    EXPECT_EQ(i + 1 == num_frames, frame_header.is_last);

    EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));

    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                   dec, &format, pixels.data(), pixels.size()));

    EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
    EXPECT_EQ(0u, jxl::test::ComparePixels(frames[i].data(), pixels.data(),
                                           xsize, ysize, format, format));
  }

  // After all frames were decoded, JxlDecoderProcessInput should return
  // success to indicate all is done.
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));

  // Test rewinding the decoder and skipping different frames

  JxlDecoderRewind(dec);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(dec, JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE));
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));

  for (size_t i = 0; i < num_frames; ++i) {
    int test_skipping = (i == 9) ? 3 : 0;
    std::vector<uint8_t> pixels(buffer_size);

    EXPECT_EQ(JXL_DEC_FRAME, JxlDecoderProcessInput(dec));

    // Since this is after JXL_DEC_FRAME but before JXL_DEC_FULL_IMAGE, this
    // should only skip the next frame, not the currently processed one.
    if (test_skipping) JxlDecoderSkipFrames(dec, test_skipping);

    JxlFrameHeader frame_header;
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetFrameHeader(dec, &frame_header));
    EXPECT_EQ(frame_durations[i], frame_header.duration);

    EXPECT_EQ(i + 1 == num_frames, frame_header.is_last);

    EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));

    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                   dec, &format, pixels.data(), pixels.size()));

    EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
    EXPECT_EQ(0u, jxl::test::ComparePixels(frames[i].data(), pixels.data(),
                                           xsize, ysize, format, format));

    if (test_skipping) i += test_skipping;
  }

  JxlThreadParallelRunnerDestroy(runner);
  JxlDecoderDestroy(dec);
}

TEST(DecodeTest, SkipFrameWithAlphaBlendingTest) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  size_t xsize = 90;
  size_t ysize = 120;
  constexpr size_t num_frames = 16;
  std::vector<uint8_t> frames[num_frames + 5];
  JxlPixelFormat format = {4, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};

  jxl::CodecInOut io{memory_manager};
  ASSERT_TRUE(io.SetSize(xsize, ysize));
  io.metadata.m.SetUintSamples(16);
  io.metadata.m.color_encoding = jxl::ColorEncoding::SRGB(false);
  io.metadata.m.have_animation = true;
  io.frames.clear();
  io.frames.reserve(num_frames + 5);
  ASSERT_TRUE(io.SetSize(xsize, ysize));

  std::vector<uint32_t> frame_durations_c;
  std::vector<uint32_t> frame_durations_nc;
  std::vector<uint32_t> frame_xsize;
  std::vector<uint32_t> frame_ysize;
  std::vector<uint32_t> frame_x0;
  std::vector<uint32_t> frame_y0;

  for (size_t i = 0; i < num_frames; ++i) {
    size_t cropxsize = 1 + xsize * 2 / (i + 1);
    size_t cropysize = 1 + ysize * 3 / (i + 2);
    int cropx0 = i * 3 - 8;
    int cropy0 = i * 4 - 7;
    if (i < 5) {
      std::vector<uint8_t> frame_internal =
          jxl::test::GetSomeTestImage(xsize / 2, ysize / 2, 4, i * 2 + 1);
      // An internal frame with 0 duration, and use_for_next_frame, this is a
      // frame that is not rendered and not output by default by the API, but on
      // which the rendered frames depend
      jxl::ImageBundle bundle_internal(memory_manager, &io.metadata.m);
      EXPECT_TRUE(ConvertFromExternal(
          jxl::Bytes(frame_internal.data(), frame_internal.size()), xsize / 2,
          ysize / 2, jxl::ColorEncoding::SRGB(/*is_gray=*/false),
          /*bits_per_sample=*/16, format,
          /*pool=*/nullptr, &bundle_internal));
      bundle_internal.duration = 0;
      bundle_internal.use_for_next_frame = true;
      bundle_internal.origin = {13, 17};
      io.frames.push_back(std::move(bundle_internal));
      frame_durations_nc.push_back(0);
      frame_xsize.push_back(xsize / 2);
      frame_ysize.push_back(ysize / 2);
      frame_x0.push_back(13);
      frame_y0.push_back(17);
    }

    std::vector<uint8_t> frame =
        jxl::test::GetSomeTestImage(cropxsize, cropysize, 4, i * 2);
    // Actual rendered frame
    jxl::ImageBundle bundle(memory_manager, &io.metadata.m);
    EXPECT_TRUE(ConvertFromExternal(jxl::Bytes(frame.data(), frame.size()),
                                    cropxsize, cropysize,
                                    jxl::ColorEncoding::SRGB(/*is_gray=*/false),
                                    /*bits_per_sample=*/16, format,
                                    /*pool=*/nullptr, &bundle));
    bundle.duration = 5 + i;
    frame_durations_nc.push_back(5 + i);
    frame_durations_c.push_back(5 + i);
    frame_xsize.push_back(cropxsize);
    frame_ysize.push_back(cropysize);
    frame_x0.push_back(cropx0);
    frame_y0.push_back(cropy0);
    bundle.origin = {cropx0, cropy0};
    // Create some variation in which frames depend on which.
    if (i != 3 && i != 9 && i != 10) {
      bundle.use_for_next_frame = true;
    }
    if (i != 12) {
      bundle.blend = true;
      bundle.blendmode = jxl::BlendMode::kBlend;
    }
    io.frames.push_back(std::move(bundle));
  }

  jxl::CompressParams cparams;
  cparams.SetLossless();  // Lossless to verify pixels exactly after roundtrip.
  cparams.speed_tier = jxl::SpeedTier::kThunder;
  std::vector<uint8_t> compressed;
  EXPECT_TRUE(jxl::test::EncodeFile(cparams, &io, &compressed));
  // try both with and without coalescing
  for (auto coalescing : {JXL_TRUE, JXL_FALSE}) {
    // Independently decode all frames without any skipping, to create the
    // expected blended frames, for the actual tests below to compare with.
    {
      JxlDecoder* dec = JxlDecoderCreate(nullptr);
      const uint8_t* next_in = compressed.data();
      size_t avail_in = compressed.size();
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetCoalescing(dec, coalescing));
      void* runner = JxlThreadParallelRunnerCreate(
          nullptr, JxlThreadParallelRunnerDefaultNumWorkerThreads());
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetParallelRunner(
                                     dec, JxlThreadParallelRunner, runner));
      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderSubscribeEvents(dec, JXL_DEC_FULL_IMAGE));
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));
      for (size_t i = 0; i < num_frames + (coalescing ? 0 : 5); ++i) {
        EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));
        size_t buffer_size;
        EXPECT_EQ(JXL_DEC_SUCCESS,
                  JxlDecoderImageOutBufferSize(dec, &format, &buffer_size));
        if (coalescing) {
          EXPECT_EQ(xsize * ysize * 8, buffer_size);
        } else {
          EXPECT_EQ(frame_xsize[i] * frame_ysize[i] * 8, buffer_size);
        }
        frames[i].resize(buffer_size);
        EXPECT_EQ(JXL_DEC_SUCCESS,
                  JxlDecoderSetImageOutBuffer(dec, &format, frames[i].data(),
                                              frames[i].size()));
        EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
      }

      // After all frames were decoded, JxlDecoderProcessInput should return
      // success to indicate all is done.
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));
      JxlThreadParallelRunnerDestroy(runner);
      JxlDecoderDestroy(dec);
    }

    JxlDecoder* dec = JxlDecoderCreate(nullptr);
    const uint8_t* next_in = compressed.data();
    size_t avail_in = compressed.size();

    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetCoalescing(dec, coalescing));
    void* runner = JxlThreadParallelRunnerCreate(
        nullptr, JxlThreadParallelRunnerDefaultNumWorkerThreads());
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetParallelRunner(
                                   dec, JxlThreadParallelRunner, runner));

    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSubscribeEvents(
                                   dec, JXL_DEC_BASIC_INFO | JXL_DEC_FRAME |
                                            JXL_DEC_FULL_IMAGE));
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));
    EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
    JxlBasicInfo info;
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));

    for (size_t i = 0; i < num_frames; ++i) {
      EXPECT_EQ(JXL_DEC_FRAME, JxlDecoderProcessInput(dec));

      size_t buffer_size;
      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderImageOutBufferSize(dec, &format, &buffer_size));
      std::vector<uint8_t> pixels(buffer_size);

      JxlFrameHeader frame_header;
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetFrameHeader(dec, &frame_header));
      EXPECT_EQ((coalescing ? frame_durations_c[i] : frame_durations_nc[i]),
                frame_header.duration);

      EXPECT_EQ(i + 1 == num_frames, frame_header.is_last);

      EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));

      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderSetImageOutBuffer(dec, &format, pixels.data(),
                                            pixels.size()));

      EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
      if (coalescing) {
        EXPECT_EQ(frame_header.layer_info.xsize, xsize);
      } else {
        EXPECT_EQ(frame_header.layer_info.xsize, frame_xsize[i]);
      }
      if (coalescing) {
        EXPECT_EQ(frame_header.layer_info.ysize, ysize);
      } else {
        EXPECT_EQ(frame_header.layer_info.ysize, frame_ysize[i]);
      }
      EXPECT_EQ(0u, jxl::test::ComparePixels(frames[i].data(), pixels.data(),
                                             frame_header.layer_info.xsize,
                                             frame_header.layer_info.ysize,
                                             format, format));

      // Test rewinding mid-way, not decoding all frames.
      if (i == 8) {
        break;
      }
    }

    JxlDecoderRewind(dec);
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSubscribeEvents(
                                   dec, JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE));
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));

    for (size_t i = 0; i < num_frames + (coalescing ? 0 : 5); ++i) {
      if (i == 3) {
        JxlDecoderSkipFrames(dec, 5);
        i += 5;
      }

      EXPECT_EQ(JXL_DEC_FRAME, JxlDecoderProcessInput(dec));
      size_t buffer_size;
      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderImageOutBufferSize(dec, &format, &buffer_size));
      std::vector<uint8_t> pixels(buffer_size);

      JxlFrameHeader frame_header;
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetFrameHeader(dec, &frame_header));
      EXPECT_EQ((coalescing ? frame_durations_c[i] : frame_durations_nc[i]),
                frame_header.duration);

      EXPECT_EQ(i + 1 == num_frames + (coalescing ? 0 : 5),
                frame_header.is_last);

      EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));

      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderSetImageOutBuffer(dec, &format, pixels.data(),
                                            pixels.size()));

      EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
      if (coalescing) {
        EXPECT_EQ(frame_header.layer_info.xsize, xsize);
        EXPECT_EQ(frame_header.layer_info.ysize, ysize);
        EXPECT_EQ(frame_header.layer_info.crop_x0, 0);
        EXPECT_EQ(frame_header.layer_info.crop_y0, 0);
      } else {
        EXPECT_EQ(frame_header.layer_info.xsize, frame_xsize[i]);
        EXPECT_EQ(frame_header.layer_info.ysize, frame_ysize[i]);
        EXPECT_EQ(frame_header.layer_info.crop_x0, frame_x0[i]);
        EXPECT_EQ(frame_header.layer_info.crop_y0, frame_y0[i]);
        EXPECT_EQ(frame_header.layer_info.blend_info.blendmode,
                  i != 12 + 5 && frame_header.duration != 0
                      ? 2
                      : 0);  // kBlend or the default kReplace
      }
      EXPECT_EQ(0u, jxl::test::ComparePixels(frames[i].data(), pixels.data(),
                                             frame_header.layer_info.xsize,
                                             frame_header.layer_info.ysize,
                                             format, format));
    }

    // After all frames were decoded, JxlDecoderProcessInput should return
    // success to indicate all is done.
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));

    // Test rewinding the decoder and skipping different frames

    JxlDecoderRewind(dec);
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSubscribeEvents(
                                   dec, JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE));
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));

    for (size_t i = 0; i < num_frames + (coalescing ? 0 : 5); ++i) {
      int test_skipping = (i == 9) ? 3 : 0;

      EXPECT_EQ(JXL_DEC_FRAME, JxlDecoderProcessInput(dec));
      size_t buffer_size;
      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderImageOutBufferSize(dec, &format, &buffer_size));
      std::vector<uint8_t> pixels(buffer_size);

      // Since this is after JXL_DEC_FRAME but before JXL_DEC_FULL_IMAGE, this
      // should only skip the next frame, not the currently processed one.
      if (test_skipping) JxlDecoderSkipFrames(dec, test_skipping);

      JxlFrameHeader frame_header;
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetFrameHeader(dec, &frame_header));
      EXPECT_EQ((coalescing ? frame_durations_c[i] : frame_durations_nc[i]),
                frame_header.duration);

      EXPECT_EQ(i + 1 == num_frames + (coalescing ? 0 : 5),
                frame_header.is_last);

      EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));

      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderSetImageOutBuffer(dec, &format, pixels.data(),
                                            pixels.size()));

      EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
      EXPECT_EQ(0u, jxl::test::ComparePixels(frames[i].data(), pixels.data(),
                                             frame_header.layer_info.xsize,
                                             frame_header.layer_info.ysize,
                                             format, format));

      if (test_skipping) i += test_skipping;
    }

    JxlThreadParallelRunnerDestroy(runner);
    JxlDecoderDestroy(dec);
  }
}

TEST(DecodeTest, OrientedCroppedFrameTest) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  const auto test = [&](bool keep_orientation, uint32_t orientation,
                        uint32_t resampling) {
    size_t xsize = 90;
    size_t ysize = 120;
    JxlPixelFormat format = {4, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
    size_t oxsize = (!keep_orientation && orientation > 4 ? ysize : xsize);
    size_t oysize = (!keep_orientation && orientation > 4 ? xsize : ysize);
    jxl::CodecInOut io{memory_manager};
    EXPECT_TRUE(io.SetSize(xsize, ysize));
    io.metadata.m.SetUintSamples(16);
    io.metadata.m.color_encoding = jxl::ColorEncoding::SRGB(false);
    io.metadata.m.orientation = orientation;
    io.frames.clear();
    EXPECT_TRUE(io.SetSize(xsize, ysize));

    for (size_t i = 0; i < 3; ++i) {
      size_t cropxsize = 1 + xsize * 2 / (i + 1);
      size_t cropysize = 1 + ysize * 3 / (i + 2);
      int cropx0 = i * 3 - 8;
      int cropy0 = i * 4 - 7;

      std::vector<uint8_t> frame =
          jxl::test::GetSomeTestImage(cropxsize, cropysize, 4, i * 2);
      jxl::ImageBundle bundle(memory_manager, &io.metadata.m);
      EXPECT_TRUE(ConvertFromExternal(
          jxl::Bytes(frame.data(), frame.size()), cropxsize, cropysize,
          jxl::ColorEncoding::SRGB(/*is_gray=*/false),
          /*bits_per_sample=*/16, format,
          /*pool=*/nullptr, &bundle));
      bundle.origin = {cropx0, cropy0};
      bundle.use_for_next_frame = true;
      io.frames.push_back(std::move(bundle));
    }

    jxl::CompressParams cparams;
    cparams
        .SetLossless();  // Lossless to verify pixels exactly after roundtrip.
    cparams.speed_tier = jxl::SpeedTier::kThunder;
    cparams.resampling = resampling;
    std::vector<uint8_t> compressed;
    EXPECT_TRUE(jxl::test::EncodeFile(cparams, &io, &compressed));

    // 0 is merged frame as decoded with coalescing enabled (default)
    // 1-3 are non-coalesced frames as decoded with coalescing disabled
    // 4 is the manually merged frame
    std::vector<uint8_t> frames[5];
    frames[4].resize(xsize * ysize * 8, 0);

    // try both with and without coalescing
    for (auto coalescing : {JXL_TRUE, JXL_FALSE}) {
      // Independently decode all frames without any skipping, to create the
      // expected blended frames, for the actual tests below to compare with.
      {
        JxlDecoder* dec = JxlDecoderCreate(nullptr);
        const uint8_t* next_in = compressed.data();
        size_t avail_in = compressed.size();
        EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetCoalescing(dec, coalescing));
        EXPECT_EQ(JXL_DEC_SUCCESS,
                  JxlDecoderSetKeepOrientation(dec, keep_orientation));
        void* runner = JxlThreadParallelRunnerCreate(
            nullptr, JxlThreadParallelRunnerDefaultNumWorkerThreads());
        EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetParallelRunner(
                                       dec, JxlThreadParallelRunner, runner));
        EXPECT_EQ(JXL_DEC_SUCCESS,
                  JxlDecoderSubscribeEvents(dec, JXL_DEC_FULL_IMAGE));
        EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));
        for (size_t i = (coalescing ? 0 : 1); i < (coalescing ? 1 : 4); ++i) {
          EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));
          JxlFrameHeader frame_header;
          EXPECT_EQ(JXL_DEC_SUCCESS,
                    JxlDecoderGetFrameHeader(dec, &frame_header));
          size_t buffer_size;
          EXPECT_EQ(JXL_DEC_SUCCESS,
                    JxlDecoderImageOutBufferSize(dec, &format, &buffer_size));
          if (coalescing) {
            EXPECT_EQ(xsize * ysize * 8, buffer_size);
          } else {
            EXPECT_EQ(frame_header.layer_info.xsize *
                          frame_header.layer_info.ysize * 8,
                      buffer_size);
          }
          frames[i].resize(buffer_size);
          EXPECT_EQ(JXL_DEC_SUCCESS,
                    JxlDecoderSetImageOutBuffer(dec, &format, frames[i].data(),
                                                frames[i].size()));
          EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
          EXPECT_EQ(frame_header.layer_info.blend_info.blendmode,
                    JXL_BLEND_REPLACE);
          if (coalescing) {
            EXPECT_EQ(frame_header.layer_info.xsize, oxsize);
            EXPECT_EQ(frame_header.layer_info.ysize, oysize);
            EXPECT_EQ(frame_header.layer_info.crop_x0, 0);
            EXPECT_EQ(frame_header.layer_info.crop_y0, 0);
          } else {
            // manually merge this layer
            int x0 = frame_header.layer_info.crop_x0;
            int y0 = frame_header.layer_info.crop_y0;
            int w = frame_header.layer_info.xsize;
            int h = frame_header.layer_info.ysize;
            for (int y = 0; y < static_cast<int>(oysize); y++) {
              if (y < y0 || y >= y0 + h) continue;
              // pointers do whole 16-bit RGBA pixels at a time
              uint64_t* row_merged = reinterpret_cast<uint64_t*>(
                  frames[4].data() + y * oxsize * 8);
              uint64_t* row_layer = reinterpret_cast<uint64_t*>(
                  frames[i].data() + (y - y0) * w * 8);
              for (int x = 0; x < static_cast<int>(oxsize); x++) {
                if (x < x0 || x >= x0 + w) continue;
                row_merged[x] = row_layer[x - x0];
              }
            }
          }
        }

        // After all frames were decoded, JxlDecoderProcessInput should return
        // success to indicate all is done.
        EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));
        JxlThreadParallelRunnerDestroy(runner);
        JxlDecoderDestroy(dec);
      }
    }

    EXPECT_EQ(0u, jxl::test::ComparePixels(frames[0].data(), frames[4].data(),
                                           oxsize, oysize, format, format));
  };

  for (bool keep_orientation : {true, false}) {
    for (uint32_t orientation = 1; orientation <= 8; orientation++) {
      for (uint32_t resampling : {1, 2, 4, 8}) {
        SCOPED_TRACE(testing::Message()
                     << "keep_orientation: " << keep_orientation << ", "
                     << "orientation: " << orientation << ", "
                     << "resampling: " << resampling);
        test(keep_orientation, orientation, resampling);
      }
    }
  }
}

struct FramePositions {
  size_t frame_start;
  size_t header_end;
  size_t toc_end;
  std::vector<size_t> section_end;
};

struct StreamPositions {
  size_t codestream_start;
  size_t codestream_end;
  size_t basic_info;
  size_t jbrd_end = 0;
  std::vector<size_t> box_start;
  std::vector<FramePositions> frames;
};

void AnalyzeCodestream(const std::vector<uint8_t>& data,
                       StreamPositions* streampos) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  // Unbox data to codestream and mark where it is broken up by boxes.
  std::vector<uint8_t> codestream;
  std::vector<std::pair<size_t, size_t>> breakpoints;
  bool codestream_end = false;
  ASSERT_LE(2, data.size());
  if (data[0] == 0xff && data[1] == 0x0a) {
    codestream = std::vector<uint8_t>(data.begin(), data.end());
    streampos->codestream_start = 0;
  } else {
    const uint8_t* in = data.data();
    size_t pos = 0;
    while (pos < data.size()) {
      ASSERT_LE(pos + 8, data.size());
      streampos->box_start.push_back(pos);
      size_t box_size = LoadBE32(in + pos);
      if (box_size == 0) box_size = data.size() - pos;
      ASSERT_LE(pos + box_size, data.size());
      if (memcmp(in + pos + 4, "jxlc", 4) == 0) {
        EXPECT_TRUE(codestream.empty());
        streampos->codestream_start = pos + 8;
        codestream.insert(codestream.end(), in + pos + 8, in + pos + box_size);
        codestream_end = true;
      } else if (memcmp(in + pos + 4, "jxlp", 4) == 0) {
        codestream_end = ((LoadBE32(in + pos + 8) & 0x80000000) != 0);
        if (codestream.empty()) {
          streampos->codestream_start = pos + 12;
        } else if (box_size > 12 || !codestream_end) {
          breakpoints.emplace_back(codestream.size(), 12);
        }
        codestream.insert(codestream.end(), in + pos + 12, in + pos + box_size);
      } else if (memcmp(in + pos + 4, "jbrd", 4) == 0) {
        EXPECT_TRUE(codestream.empty());
        streampos->jbrd_end = pos + box_size;
      } else if (!codestream.empty() && !codestream_end) {
        breakpoints.emplace_back(codestream.size(), box_size);
      }
      pos += box_size;
    }
    ASSERT_EQ(pos, data.size());
  }
  // Translate codestream positions to boxed stream positions.
  size_t offset = streampos->codestream_start;
  size_t bp = 0;
  auto add_offset = [&](size_t pos) {
    while (bp < breakpoints.size() && pos >= breakpoints[bp].first) {
      offset += breakpoints[bp++].second;
    }
    return pos + offset;
  };
  // Analyze the unboxed codestream.
  jxl::BitReader br(jxl::Bytes(codestream.data(), codestream.size()));
  ASSERT_EQ(br.ReadFixedBits<16>(), 0x0AFF);
  jxl::CodecMetadata metadata;
  ASSERT_TRUE(ReadSizeHeader(&br, &metadata.size));
  ASSERT_TRUE(ReadImageMetadata(&br, &metadata.m));
  streampos->basic_info =
      add_offset(br.TotalBitsConsumed() / jxl::kBitsPerByte);
  metadata.transform_data.nonserialized_xyb_encoded = metadata.m.xyb_encoded;
  ASSERT_TRUE(jxl::Bundle::Read(&br, &metadata.transform_data));
  if (metadata.m.color_encoding.WantICC()) {
    std::vector<uint8_t> icc;
    ASSERT_TRUE(jxl::test::ReadICC(&br, &icc));
    ASSERT_TRUE(!icc.empty());
    metadata.m.color_encoding.SetICCRaw(std::move(icc));
  }
  ASSERT_TRUE(br.JumpToByteBoundary());
  bool has_preview = metadata.m.have_preview;
  while (br.TotalBitsConsumed() < br.TotalBytes() * jxl::kBitsPerByte) {
    FramePositions p;
    p.frame_start = add_offset(br.TotalBitsConsumed() / jxl::kBitsPerByte);
    jxl::FrameHeader frame_header(&metadata);
    if (has_preview) {
      frame_header.nonserialized_is_preview = true;
      has_preview = false;
    }
    ASSERT_TRUE(ReadFrameHeader(&br, &frame_header));
    p.header_end =
        add_offset(jxl::DivCeil(br.TotalBitsConsumed(), jxl::kBitsPerByte));
    jxl::FrameDimensions frame_dim = frame_header.ToFrameDimensions();
    uint64_t groups_total_size;
    const size_t toc_entries =
        jxl::NumTocEntries(frame_dim.num_groups, frame_dim.num_dc_groups,
                           frame_header.passes.num_passes);
    std::vector<uint64_t> section_offsets;
    std::vector<uint32_t> section_sizes;
    ASSERT_TRUE(ReadGroupOffsets(memory_manager, toc_entries, &br,
                                 &section_offsets, &section_sizes,
                                 &groups_total_size));
    EXPECT_EQ(br.TotalBitsConsumed() % jxl::kBitsPerByte, 0);
    size_t sections_start = br.TotalBitsConsumed() / jxl::kBitsPerByte;
    p.toc_end = add_offset(sections_start);
    for (size_t i = 0; i < toc_entries; ++i) {
      size_t end = sections_start + section_offsets[i] + section_sizes[i];
      p.section_end.push_back(add_offset(end));
    }
    br.SkipBits(groups_total_size * jxl::kBitsPerByte);
    streampos->frames.push_back(p);
  }
  streampos->codestream_end = add_offset(codestream.size());
  EXPECT_EQ(br.TotalBitsConsumed(), br.TotalBytes() * jxl::kBitsPerByte);
  EXPECT_TRUE(br.Close());
}

enum ExpectedFlushState { NO_FLUSH, SAME_FLUSH, NEW_FLUSH };
struct Breakpoint {
  size_t file_pos;
  ExpectedFlushState expect_flush;
};

void VerifyProgression(size_t xsize, size_t ysize, uint32_t num_channels,
                       const std::vector<uint8_t>& pixels,
                       const std::vector<uint8_t>& data,
                       std::vector<Breakpoint> breakpoints) {
  // Size large enough for multiple groups, required to have progressive stages.
  ASSERT_LT(256, xsize);
  ASSERT_LT(256, ysize);
  std::vector<uint8_t> pixels2;
  pixels2.resize(pixels.size());
  JxlPixelFormat format = {num_channels, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
  JxlDecoder* dec = JxlDecoderCreate(nullptr);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(
                dec, JXL_DEC_BASIC_INFO | JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE));
  int bp = 0;
  const uint8_t* next_in = data.data();
  size_t avail_in = breakpoints[bp].file_pos;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));
  double prev_dist = 1.0;
  for (;;) {
    JxlDecoderStatus status = JxlDecoderProcessInput(dec);
    printf("bp: %d  status: 0x%x\n", bp, static_cast<int>(status));
    if (status == JXL_DEC_BASIC_INFO) {
      JxlBasicInfo info;
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
      EXPECT_EQ(info.xsize, xsize);
      EXPECT_EQ(info.ysize, ysize);
      // Output buffer/callback not yet set
      EXPECT_EQ(JXL_DEC_ERROR, JxlDecoderFlushImage(dec));
      size_t buffer_size;
      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderImageOutBufferSize(dec, &format, &buffer_size));
      EXPECT_EQ(pixels2.size(), buffer_size);
      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderSetImageOutBuffer(dec, &format, pixels2.data(),
                                            pixels2.size()));
    } else if (status == JXL_DEC_FRAME) {
      // Nothing to do.
    } else if (status == JXL_DEC_SUCCESS) {
      EXPECT_EQ(bp + 1, breakpoints.size());
      break;
    } else if (status == JXL_DEC_NEED_MORE_INPUT ||
               status == JXL_DEC_FULL_IMAGE) {
      if (breakpoints[bp].expect_flush == NO_FLUSH) {
        EXPECT_EQ(JXL_DEC_ERROR, JxlDecoderFlushImage(dec));
      } else {
        if (status != JXL_DEC_FULL_IMAGE) {
          EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderFlushImage(dec));
        }
        double dist = jxl::test::DistanceRMS(pixels2.data(), pixels.data(),
                                             xsize, ysize, format);
        if (breakpoints[bp].expect_flush == NEW_FLUSH) {
          EXPECT_LT(dist, prev_dist);
          prev_dist = dist;
        } else {
          EXPECT_EQ(dist, prev_dist);
        }
      }
      if (status == JXL_DEC_FULL_IMAGE) {
        EXPECT_EQ(bp + 1, breakpoints.size());
        continue;
      }
      ASSERT_LT(++bp, breakpoints.size());
      next_in += avail_in - JxlDecoderReleaseInput(dec);
      avail_in = breakpoints[bp].file_pos - (next_in - data.data());
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));
    } else {
      printf("Unexpected status: 0x%x\n", static_cast<int>(status));
      FAIL();  // unexpected returned status
    }
  }
  JxlDecoderDestroy(dec);
}

TEST(DecodeTest, ProgressionTest) {
  size_t xsize = 508;
  size_t ysize = 470;
  uint32_t num_channels = 3;
  std::vector<uint8_t> pixels =
      jxl::test::GetSomeTestImage(xsize, ysize, num_channels, 0);
  jxl::TestCodestreamParams params;
  params.cparams.progressive_dc = 1;
  params.preview_mode = jxl::kSmallPreview;
  std::vector<uint8_t> data =
      jxl::CreateTestJXLCodestream(jxl::Bytes(pixels.data(), pixels.size()),
                                   xsize, ysize, num_channels, params);
  StreamPositions streampos;
  AnalyzeCodestream(data, &streampos);
  const std::vector<FramePositions>& fp = streampos.frames;
  // We have preview, dc frame and regular frame.
  EXPECT_EQ(3, fp.size());
  EXPECT_EQ(7, fp[2].section_end.size());
  EXPECT_EQ(data.size(), fp[2].section_end[6]);
  std::vector<Breakpoint> breakpoints{
      {fp[0].frame_start, NO_FLUSH},           // headers
      {fp[1].frame_start, NO_FLUSH},           // preview
      {fp[2].frame_start, NO_FLUSH},           // dc frame
      {fp[2].section_end[0], NO_FLUSH},        // DC global
      {fp[2].section_end[1] - 1, NO_FLUSH},    // partial DC group
      {fp[2].section_end[1], NEW_FLUSH},       // DC group
      {fp[2].section_end[2], SAME_FLUSH},      // AC global
      {fp[2].section_end[3], NEW_FLUSH},       // AC group 0
      {fp[2].section_end[4] - 1, SAME_FLUSH},  // partial AC group 1
      {fp[2].section_end[4], NEW_FLUSH},       // AC group 1
      {fp[2].section_end[5], NEW_FLUSH},       // AC group 2
      {data.size() - 1, SAME_FLUSH},           // partial AC group 3
      {data.size(), NEW_FLUSH}};               // full image
  VerifyProgression(xsize, ysize, num_channels, pixels, data, breakpoints);
}

TEST(DecodeTest, ProgressionTestLosslessAlpha) {
  size_t xsize = 508;
  size_t ysize = 470;
  uint32_t num_channels = 4;
  std::vector<uint8_t> pixels =
      jxl::test::GetSomeTestImage(xsize, ysize, num_channels, 0);
  jxl::TestCodestreamParams params;
  params.cparams.SetLossless();
  params.cparams.speed_tier = jxl::SpeedTier::kThunder;
  params.cparams.responsive = 1;
  std::vector<uint8_t> data =
      jxl::CreateTestJXLCodestream(jxl::Bytes(pixels.data(), pixels.size()),
                                   xsize, ysize, num_channels, params);
  StreamPositions streampos;
  AnalyzeCodestream(data, &streampos);
  const std::vector<FramePositions>& fp = streampos.frames;
  // We have preview, dc frame and regular frame.
  EXPECT_EQ(1, fp.size());
  EXPECT_EQ(7, fp[0].section_end.size());
  EXPECT_EQ(data.size(), fp[0].section_end[6]);
  std::vector<Breakpoint> breakpoints{
      {fp[0].frame_start, NO_FLUSH},           // headers
      {fp[0].section_end[0] - 1, NO_FLUSH},    // partial DC global
      {fp[0].section_end[0], NEW_FLUSH},       // DC global
      {fp[0].section_end[1], SAME_FLUSH},      // DC group
      {fp[0].section_end[2], SAME_FLUSH},      // AC global
      {fp[0].section_end[3], NEW_FLUSH},       // AC group 0
      {fp[0].section_end[4] - 1, SAME_FLUSH},  // partial AC group 1
      {fp[0].section_end[4], NEW_FLUSH},       // AC group 1
      {fp[0].section_end[5], NEW_FLUSH},       // AC group 2
      {data.size() - 1, SAME_FLUSH},           // partial AC group 3
      {data.size(), NEW_FLUSH}};               // full image
  VerifyProgression(xsize, ysize, num_channels, pixels, data, breakpoints);
}

void VerifyFilePosition(size_t expected_pos, const std::vector<uint8_t>& data,
                        JxlDecoder* dec) {
  size_t remaining = JxlDecoderReleaseInput(dec);
  size_t pos = data.size() - remaining;
  EXPECT_EQ(expected_pos, pos);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetInput(dec, data.data() + pos, remaining));
}

TEST(DecodeTest, InputHandlingTestOneShot) {
  size_t xsize = 508;
  size_t ysize = 470;
  uint32_t num_channels = 3;
  std::vector<uint8_t> pixels =
      jxl::test::GetSomeTestImage(xsize, ysize, num_channels, 0);
  for (int i = 0; i < kCSBF_NUM_ENTRIES; ++i) {
    printf("Testing with box format %d\n", i);
    jxl::TestCodestreamParams params;
    params.cparams.progressive_dc = 1;
    params.preview_mode = jxl::kSmallPreview;
    params.box_format = static_cast<CodeStreamBoxFormat>(i);
    std::vector<uint8_t> data =
        jxl::CreateTestJXLCodestream(jxl::Bytes(pixels.data(), pixels.size()),
                                     xsize, ysize, num_channels, params);
    JxlPixelFormat format = {num_channels, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
    StreamPositions streampos;
    AnalyzeCodestream(data, &streampos);
    const std::vector<FramePositions>& fp = streampos.frames;
    // We have preview, dc frame and regular frame.
    EXPECT_EQ(3, fp.size());

    std::vector<uint8_t> pixels2;
    pixels2.resize(pixels.size());

    int kNumEvents = 6;
    int events[] = {
        JXL_DEC_BASIC_INFO, JXL_DEC_COLOR_ENCODING, JXL_DEC_PREVIEW_IMAGE,
        JXL_DEC_FRAME,      JXL_DEC_FULL_IMAGE,     JXL_DEC_FRAME_PROGRESSION,
    };
    size_t end_positions[] = {
        streampos.basic_info,     fp[0].frame_start,
        fp[1].frame_start,        fp[2].toc_end,
        streampos.codestream_end, streampos.codestream_end};
    int events_wanted = 0;
    for (int j = 0; j < kNumEvents; ++j) {
      events_wanted |= events[j];
      size_t end_pos = end_positions[j];
      JxlDecoder* dec = JxlDecoderCreate(nullptr);
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSubscribeEvents(dec, events_wanted));
      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderSetInput(dec, data.data(), data.size()));
      EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
      VerifyFilePosition(streampos.basic_info, data, dec);
      if (j >= 1) {
        EXPECT_EQ(JXL_DEC_COLOR_ENCODING, JxlDecoderProcessInput(dec));
        VerifyFilePosition(fp[0].frame_start, data, dec);
      }
      if (j >= 2) {
        EXPECT_EQ(JXL_DEC_NEED_PREVIEW_OUT_BUFFER, JxlDecoderProcessInput(dec));
        VerifyFilePosition(fp[0].toc_end, data, dec);
        size_t buffer_size;
        EXPECT_EQ(JXL_DEC_SUCCESS,
                  JxlDecoderPreviewOutBufferSize(dec, &format, &buffer_size));
        EXPECT_GE(pixels2.size(), buffer_size);
        EXPECT_EQ(JXL_DEC_SUCCESS,
                  JxlDecoderSetPreviewOutBuffer(dec, &format, pixels2.data(),
                                                buffer_size));
        EXPECT_EQ(JXL_DEC_PREVIEW_IMAGE, JxlDecoderProcessInput(dec));
        VerifyFilePosition(fp[1].frame_start, data, dec);
      }
      if (j >= 3) {
        EXPECT_EQ(JXL_DEC_FRAME, JxlDecoderProcessInput(dec));
        VerifyFilePosition(fp[2].toc_end, data, dec);
        if (j >= 5) {
          EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetProgressiveDetail(dec, kDC));
        }
      }
      if (j >= 4) {
        EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));
        VerifyFilePosition(fp[2].toc_end, data, dec);
        size_t buffer_size;
        EXPECT_EQ(JXL_DEC_SUCCESS,
                  JxlDecoderImageOutBufferSize(dec, &format, &buffer_size));
        EXPECT_EQ(pixels2.size(), buffer_size);
        EXPECT_EQ(JXL_DEC_SUCCESS,
                  JxlDecoderSetImageOutBuffer(dec, &format, pixels2.data(),
                                              pixels2.size()));
        if (j >= 5) {
          EXPECT_EQ(JXL_DEC_FRAME_PROGRESSION, JxlDecoderProcessInput(dec));
          VerifyFilePosition(fp[2].section_end[1], data, dec);
        }
        EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
        VerifyFilePosition(streampos.codestream_end, data, dec);
      }
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));
      VerifyFilePosition(end_pos, data, dec);
      JxlDecoderDestroy(dec);
    }
  }
}

JXL_TRANSCODE_JPEG_TEST(DecodeTest, InputHandlingTestJPEGOneshot) {
  TEST_LIBJPEG_SUPPORT();
  size_t xsize = 123;
  size_t ysize = 77;
  size_t channels = 3;
  std::vector<uint8_t> pixels =
      jxl::test::GetSomeTestImage(xsize, ysize, channels, /*seed=*/0);
  for (int i = 1; i < kCSBF_NUM_ENTRIES; ++i) {
    printf("Testing with box format %d\n", i);
    std::vector<uint8_t> jpeg_codestream;
    jxl::TestCodestreamParams params;
    params.cparams.color_transform = jxl::ColorTransform::kNone;
    params.jpeg_codestream = &jpeg_codestream;
    params.preview_mode = jxl::kSmallPreview;
    params.box_format = static_cast<CodeStreamBoxFormat>(i);
    std::vector<uint8_t> data =
        jxl::CreateTestJXLCodestream(jxl::Bytes(pixels.data(), pixels.size()),
                                     xsize, ysize, channels, params);
    JxlPixelFormat format = {3, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
    StreamPositions streampos;
    AnalyzeCodestream(data, &streampos);
    const std::vector<FramePositions>& fp = streampos.frames;
    // We have preview and regular frame.
    EXPECT_EQ(2, fp.size());
    EXPECT_LT(0, streampos.jbrd_end);

    std::vector<uint8_t> pixels2;
    pixels2.resize(pixels.size());

    int kNumEvents = 6;
    int events[] = {JXL_DEC_BASIC_INFO,     JXL_DEC_JPEG_RECONSTRUCTION,
                    JXL_DEC_COLOR_ENCODING, JXL_DEC_PREVIEW_IMAGE,
                    JXL_DEC_FRAME,          JXL_DEC_FULL_IMAGE};
    size_t end_positions[] = {streampos.basic_info, streampos.basic_info,
                              fp[0].frame_start,    fp[1].frame_start,
                              fp[1].toc_end,        streampos.codestream_end};
    int events_wanted = 0;
    for (int j = 0; j < kNumEvents; ++j) {
      printf("j = %d\n", j);
      events_wanted |= events[j];
      size_t end_pos = end_positions[j];
      JxlDecoder* dec = JxlDecoderCreate(nullptr);
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSubscribeEvents(dec, events_wanted));
      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderSetInput(dec, data.data(), data.size()));
      if (j >= 1) {
        EXPECT_EQ(JXL_DEC_JPEG_RECONSTRUCTION, JxlDecoderProcessInput(dec));
        VerifyFilePosition(streampos.jbrd_end, data, dec);
      }
      EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
      VerifyFilePosition(streampos.basic_info, data, dec);
      if (j >= 2) {
        EXPECT_EQ(JXL_DEC_COLOR_ENCODING, JxlDecoderProcessInput(dec));
        VerifyFilePosition(fp[0].frame_start, data, dec);
      }
      if (j >= 3) {
        EXPECT_EQ(JXL_DEC_NEED_PREVIEW_OUT_BUFFER, JxlDecoderProcessInput(dec));
        VerifyFilePosition(fp[0].toc_end, data, dec);
        size_t buffer_size;
        EXPECT_EQ(JXL_DEC_SUCCESS,
                  JxlDecoderPreviewOutBufferSize(dec, &format, &buffer_size));
        EXPECT_GE(pixels2.size(), buffer_size);
        EXPECT_EQ(JXL_DEC_SUCCESS,
                  JxlDecoderSetPreviewOutBuffer(dec, &format, pixels2.data(),
                                                buffer_size));
        EXPECT_EQ(JXL_DEC_PREVIEW_IMAGE, JxlDecoderProcessInput(dec));
        VerifyFilePosition(fp[1].frame_start, data, dec);
      }
      if (j >= 4) {
        EXPECT_EQ(JXL_DEC_FRAME, JxlDecoderProcessInput(dec));
        VerifyFilePosition(fp[1].toc_end, data, dec);
      }
      if (j >= 5) {
        EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));
        VerifyFilePosition(fp[1].toc_end, data, dec);
        size_t buffer_size;
        EXPECT_EQ(JXL_DEC_SUCCESS,
                  JxlDecoderImageOutBufferSize(dec, &format, &buffer_size));
        EXPECT_EQ(pixels2.size(), buffer_size);
        EXPECT_EQ(JXL_DEC_SUCCESS,
                  JxlDecoderSetImageOutBuffer(dec, &format, pixels2.data(),
                                              pixels2.size()));
        EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
        VerifyFilePosition(streampos.codestream_end, data, dec);
      }
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));
      VerifyFilePosition(end_pos, data, dec);
      JxlDecoderDestroy(dec);
    }
  }
}

TEST(DecodeTest, InputHandlingTestStreaming) {
  size_t xsize = 508;
  size_t ysize = 470;
  uint32_t num_channels = 3;
  std::vector<uint8_t> pixels =
      jxl::test::GetSomeTestImage(xsize, ysize, num_channels, 0);
  for (int i = 0; i < kCSBF_NUM_ENTRIES; ++i) {
    printf("Testing with box format %d\n", i);
    fflush(stdout);
    jxl::TestCodestreamParams params;
    params.cparams.progressive_dc = 1;
    params.box_format = static_cast<CodeStreamBoxFormat>(i);
    params.preview_mode = jxl::kSmallPreview;
    std::vector<uint8_t> data =
        jxl::CreateTestJXLCodestream(jxl::Bytes(pixels.data(), pixels.size()),
                                     xsize, ysize, num_channels, params);
    JxlPixelFormat format = {num_channels, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
    StreamPositions streampos;
    AnalyzeCodestream(data, &streampos);
    const std::vector<FramePositions>& fp = streampos.frames;
    // We have preview, dc frame and regular frame.
    EXPECT_EQ(3, fp.size());
    std::vector<uint8_t> pixels2;
    pixels2.resize(pixels.size());
    int events_wanted =
        (JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING | JXL_DEC_PREVIEW_IMAGE |
         JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE | JXL_DEC_FRAME_PROGRESSION |
         JXL_DEC_BOX);
    for (size_t increment : {1, 7, 27, 1024}) {
      JxlDecoder* dec = JxlDecoderCreate(nullptr);
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSubscribeEvents(dec, events_wanted));
      size_t file_pos = 0;
      size_t box_index = 0;
      size_t avail_in = 0;
      for (;;) {
        const uint8_t* next_in = data.data() + file_pos;
        EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));
        JxlDecoderStatus status = JxlDecoderProcessInput(dec);
        size_t remaining = JxlDecoderReleaseInput(dec);
        size_t consumed = avail_in - remaining;
        file_pos += consumed;
        avail_in += increment;
        avail_in = std::min<size_t>(avail_in, data.size() - file_pos);
        if (status == JXL_DEC_BASIC_INFO) {
          EXPECT_EQ(file_pos, streampos.basic_info);
        } else if (status == JXL_DEC_COLOR_ENCODING) {
          EXPECT_EQ(file_pos, streampos.frames[0].frame_start);
        } else if (status == JXL_DEC_NEED_PREVIEW_OUT_BUFFER) {
          EXPECT_EQ(file_pos, streampos.frames[0].toc_end);
          size_t buffer_size;
          EXPECT_EQ(JXL_DEC_SUCCESS,
                    JxlDecoderPreviewOutBufferSize(dec, &format, &buffer_size));
          EXPECT_GE(pixels2.size(), buffer_size);
          EXPECT_EQ(JXL_DEC_SUCCESS,
                    JxlDecoderSetPreviewOutBuffer(dec, &format, pixels2.data(),
                                                  buffer_size));
        } else if (status == JXL_DEC_PREVIEW_IMAGE) {
          EXPECT_EQ(file_pos, streampos.frames[1].frame_start);
        } else if (status == JXL_DEC_FRAME) {
          EXPECT_EQ(file_pos, streampos.frames[2].toc_end);
          EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetProgressiveDetail(dec, kDC));
        } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
          EXPECT_EQ(file_pos, streampos.frames[2].toc_end);
          size_t buffer_size;
          EXPECT_EQ(JXL_DEC_SUCCESS,
                    JxlDecoderImageOutBufferSize(dec, &format, &buffer_size));
          EXPECT_EQ(pixels2.size(), buffer_size);
          EXPECT_EQ(JXL_DEC_SUCCESS,
                    JxlDecoderSetImageOutBuffer(dec, &format, pixels2.data(),
                                                pixels2.size()));
        } else if (status == JXL_DEC_FRAME_PROGRESSION) {
          EXPECT_EQ(file_pos, streampos.frames[2].section_end[1]);
        } else if (status == JXL_DEC_FULL_IMAGE) {
          EXPECT_EQ(file_pos, streampos.codestream_end);
        } else if (status == JXL_DEC_SUCCESS) {
          EXPECT_EQ(file_pos, streampos.codestream_end);
          break;
        } else if (status == JXL_DEC_NEED_MORE_INPUT) {
          EXPECT_LT(remaining, 12);
          if ((i == kCSBF_None && file_pos >= 2) ||
              (box_index > 0 && box_index < streampos.box_start.size() &&
               file_pos >= streampos.box_start[box_index - 1] + 12 &&
               file_pos < streampos.box_start[box_index])) {
            EXPECT_EQ(remaining, 0);
          }
          if (file_pos == data.size()) break;
        } else if (status == JXL_DEC_BOX) {
          ASSERT_LT(box_index, streampos.box_start.size());
          EXPECT_EQ(file_pos, streampos.box_start[box_index++]);
        } else {
          printf("Unexpected status: 0x%x\n", static_cast<int>(status));
          FAIL();
        }
      }
      JxlDecoderDestroy(dec);
    }
  }
}

TEST(DecodeTest, FlushTest) {
  // Size large enough for multiple groups, required to have progressive
  // stages
  size_t xsize = 333;
  size_t ysize = 300;
  uint32_t num_channels = 3;
  std::vector<uint8_t> pixels =
      jxl::test::GetSomeTestImage(xsize, ysize, num_channels, 0);
  jxl::TestCodestreamParams params;
  params.preview_mode = jxl::kSmallPreview;
  std::vector<uint8_t> data =
      jxl::CreateTestJXLCodestream(jxl::Bytes(pixels.data(), pixels.size()),
                                   xsize, ysize, num_channels, params);
  JxlPixelFormat format = {num_channels, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};

  std::vector<uint8_t> pixels2;
  pixels2.resize(pixels.size());

  JxlDecoder* dec = JxlDecoderCreate(nullptr);

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(
                dec, JXL_DEC_BASIC_INFO | JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE));

  // Ensure that the first part contains at least the full DC of the image,
  // otherwise flush does not work.
  size_t first_part = data.size() - 1;

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, data.data(), first_part));

  EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
  JxlBasicInfo info;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
  EXPECT_EQ(info.xsize, xsize);
  EXPECT_EQ(info.ysize, ysize);

  EXPECT_EQ(JXL_DEC_FRAME, JxlDecoderProcessInput(dec));

  // Output buffer not yet set
  EXPECT_EQ(JXL_DEC_ERROR, JxlDecoderFlushImage(dec));

  size_t buffer_size;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderImageOutBufferSize(dec, &format, &buffer_size));
  EXPECT_EQ(pixels2.size(), buffer_size);
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                 dec, &format, pixels2.data(), pixels2.size()));

  // Must process input further until we get JXL_DEC_NEED_MORE_INPUT, even if
  // data was already input before, since the processing of the frame only
  // happens at the JxlDecoderProcessInput call after JXL_DEC_FRAME.
  EXPECT_EQ(JXL_DEC_NEED_MORE_INPUT, JxlDecoderProcessInput(dec));

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderFlushImage(dec));

  // Crude test of actual pixel data: pixel threshold of about 4% (2560/65535).
  // 29000 pixels can be above the threshold
  EXPECT_LE(jxl::test::ComparePixels(pixels2.data(), pixels.data(), xsize,
                                     ysize, format, format, 2560.0),
            29000u);

  EXPECT_EQ(JXL_DEC_NEED_MORE_INPUT, JxlDecoderProcessInput(dec));

  size_t consumed = first_part - JxlDecoderReleaseInput(dec);

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, data.data() + consumed,
                                                data.size() - consumed));
  EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
  // Lower threshold for the final (still lossy) image
  EXPECT_LE(jxl::test::ComparePixels(pixels2.data(), pixels.data(), xsize,
                                     ysize, format, format, 2560.0),
            11000u);

  JxlDecoderDestroy(dec);
}

TEST(DecodeTest, FlushTestImageOutCallback) {
  // Size large enough for multiple groups, required to have progressive
  // stages
  size_t xsize = 333;
  size_t ysize = 300;
  uint32_t num_channels = 3;
  std::vector<uint8_t> pixels =
      jxl::test::GetSomeTestImage(xsize, ysize, num_channels, 0);
  jxl::TestCodestreamParams params;
  params.preview_mode = jxl::kSmallPreview;
  std::vector<uint8_t> data =
      jxl::CreateTestJXLCodestream(jxl::Bytes(pixels.data(), pixels.size()),
                                   xsize, ysize, num_channels, params);
  JxlPixelFormat format = {num_channels, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};

  std::vector<uint8_t> pixels2;
  pixels2.resize(pixels.size());

  size_t bytes_per_pixel = format.num_channels * 2;
  size_t stride = bytes_per_pixel * xsize;
  auto callback = [&](size_t x, size_t y, size_t num_pixels,
                      const void* pixels_row) {
    memcpy(pixels2.data() + stride * y + bytes_per_pixel * x, pixels_row,
           num_pixels * bytes_per_pixel);
  };

  JxlDecoder* dec = JxlDecoderCreate(nullptr);

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(
                dec, JXL_DEC_BASIC_INFO | JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE));

  // Ensure that the first part contains at least the full DC of the image,
  // otherwise flush does not work.
  size_t first_part = data.size() - 1;

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, data.data(), first_part));

  EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
  JxlBasicInfo info;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
  EXPECT_EQ(info.xsize, xsize);
  EXPECT_EQ(info.ysize, ysize);

  EXPECT_EQ(JXL_DEC_FRAME, JxlDecoderProcessInput(dec));

  // Output callback not yet set
  EXPECT_EQ(JXL_DEC_ERROR, JxlDecoderFlushImage(dec));

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutCallback(
                                 dec, &format,
                                 [](void* opaque, size_t x, size_t y,
                                    size_t xsize, const void* pixels_row) {
                                   auto cb =
                                       static_cast<decltype(&callback)>(opaque);
                                   (*cb)(x, y, xsize, pixels_row);
                                 },
                                 /*opaque=*/&callback));

  // Must process input further until we get JXL_DEC_NEED_MORE_INPUT, even if
  // data was already input before, since the processing of the frame only
  // happens at the JxlDecoderProcessInput call after JXL_DEC_FRAME.
  EXPECT_EQ(JXL_DEC_NEED_MORE_INPUT, JxlDecoderProcessInput(dec));

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderFlushImage(dec));

  // Crude test of actual pixel data: pixel threshold of about 4% (2560/65535).
  // 29000 pixels can be above the threshold
  EXPECT_LE(jxl::test::ComparePixels(pixels2.data(), pixels.data(), xsize,
                                     ysize, format, format, 2560.0),
            29000u);

  EXPECT_EQ(JXL_DEC_NEED_MORE_INPUT, JxlDecoderProcessInput(dec));

  size_t consumed = first_part - JxlDecoderReleaseInput(dec);

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, data.data() + consumed,
                                                data.size() - consumed));
  EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
  // Lower threshold for the final (still lossy) image
  EXPECT_LE(jxl::test::ComparePixels(pixels2.data(), pixels.data(), xsize,
                                     ysize, format, format, 2560.0),
            11000u);

  JxlDecoderDestroy(dec);
}

TEST(DecodeTest, FlushTestLossyProgressiveAlpha) {
  // Size large enough for multiple groups, required to have progressive
  // stages
  size_t xsize = 333;
  size_t ysize = 300;
  uint32_t num_channels = 4;
  std::vector<uint8_t> pixels =
      jxl::test::GetSomeTestImage(xsize, ysize, num_channels, 0);
  jxl::TestCodestreamParams params;
  params.preview_mode = jxl::kSmallPreview;
  std::vector<uint8_t> data =
      jxl::CreateTestJXLCodestream(jxl::Bytes(pixels.data(), pixels.size()),
                                   xsize, ysize, num_channels, params);
  JxlPixelFormat format = {num_channels, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};

  std::vector<uint8_t> pixels2;
  pixels2.resize(pixels.size());

  JxlDecoder* dec = JxlDecoderCreate(nullptr);

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(
                dec, JXL_DEC_BASIC_INFO | JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE));

  // Ensure that the first part contains at least the full DC of the image,
  // otherwise flush does not work.
  size_t first_part = data.size() - 1;

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, data.data(), first_part));

  EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
  JxlBasicInfo info;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
  EXPECT_EQ(info.xsize, xsize);
  EXPECT_EQ(info.ysize, ysize);

  EXPECT_EQ(JXL_DEC_FRAME, JxlDecoderProcessInput(dec));

  // Output buffer not yet set
  EXPECT_EQ(JXL_DEC_ERROR, JxlDecoderFlushImage(dec));

  size_t buffer_size;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderImageOutBufferSize(dec, &format, &buffer_size));
  EXPECT_EQ(pixels2.size(), buffer_size);
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                 dec, &format, pixels2.data(), pixels2.size()));

  // Must process input further until we get JXL_DEC_NEED_MORE_INPUT, even if
  // data was already input before, since the processing of the frame only
  // happens at the JxlDecoderProcessInput call after JXL_DEC_FRAME.
  EXPECT_EQ(JXL_DEC_NEED_MORE_INPUT, JxlDecoderProcessInput(dec));

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderFlushImage(dec));

  EXPECT_LE(jxl::test::ComparePixels(pixels2.data(), pixels.data(), xsize,
                                     ysize, format, format, 2560.0),
            30000u);

  EXPECT_EQ(JXL_DEC_NEED_MORE_INPUT, JxlDecoderProcessInput(dec));

  size_t consumed = first_part - JxlDecoderReleaseInput(dec);

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, data.data() + consumed,
                                                data.size() - consumed));

  EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
  EXPECT_LE(jxl::test::ComparePixels(pixels2.data(), pixels.data(), xsize,
                                     ysize, format, format, 2560.0),
            11000u);

  JxlDecoderDestroy(dec);
}
TEST(DecodeTest, FlushTestLossyProgressiveAlphaUpsampling) {
  size_t xsize = 533;
  size_t ysize = 401;
  uint32_t num_channels = 4;
  std::vector<uint8_t> pixels =
      jxl::test::GetSomeTestImage(xsize, ysize, num_channels, 0);
  jxl::TestCodestreamParams params;
  params.cparams.resampling = 2;
  params.cparams.ec_resampling = 4;
  params.preview_mode = jxl::kSmallPreview;
  std::vector<uint8_t> data =
      jxl::CreateTestJXLCodestream(jxl::Bytes(pixels.data(), pixels.size()),
                                   xsize, ysize, num_channels, params);
  JxlPixelFormat format = {num_channels, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};

  std::vector<uint8_t> pixels2;
  pixels2.resize(pixels.size());

  JxlDecoder* dec = JxlDecoderCreate(nullptr);

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(
                dec, JXL_DEC_BASIC_INFO | JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE));

  // Ensure that the first part contains at least the full DC of the image,
  // otherwise flush does not work.
  size_t first_part = data.size() * 2 / 3;

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, data.data(), first_part));

  EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
  JxlBasicInfo info;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
  EXPECT_EQ(info.xsize, xsize);
  EXPECT_EQ(info.ysize, ysize);

  EXPECT_EQ(JXL_DEC_FRAME, JxlDecoderProcessInput(dec));

  // Output buffer not yet set
  EXPECT_EQ(JXL_DEC_ERROR, JxlDecoderFlushImage(dec));

  size_t buffer_size;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderImageOutBufferSize(dec, &format, &buffer_size));
  EXPECT_EQ(pixels2.size(), buffer_size);
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                 dec, &format, pixels2.data(), pixels2.size()));

  // Must process input further until we get JXL_DEC_NEED_MORE_INPUT, even if
  // data was already input before, since the processing of the frame only
  // happens at the JxlDecoderProcessInput call after JXL_DEC_FRAME.
  EXPECT_EQ(JXL_DEC_NEED_MORE_INPUT, JxlDecoderProcessInput(dec));

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderFlushImage(dec));

  EXPECT_LE(jxl::test::ComparePixels(pixels2.data(), pixels.data(), xsize,
                                     ysize, format, format, 2560.0),
            125000u);

  EXPECT_EQ(JXL_DEC_NEED_MORE_INPUT, JxlDecoderProcessInput(dec));

  size_t consumed = first_part - JxlDecoderReleaseInput(dec);

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, data.data() + consumed,
                                                data.size() - consumed));

  EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
  EXPECT_LE(jxl::test::ComparePixels(pixels2.data(), pixels.data(), xsize,
                                     ysize, format, format, 2560.0),
            70000u);

  JxlDecoderDestroy(dec);
}
TEST(DecodeTest, FlushTestLosslessProgressiveAlpha) {
  // Size large enough for multiple groups, required to have progressive
  // stages
  size_t xsize = 333;
  size_t ysize = 300;
  uint32_t num_channels = 4;
  std::vector<uint8_t> pixels =
      jxl::test::GetSomeTestImage(xsize, ysize, num_channels, 0);
  jxl::TestCodestreamParams params;
  params.cparams.SetLossless();
  params.cparams.speed_tier = jxl::SpeedTier::kThunder;
  params.cparams.responsive = 1;
  params.cparams.modular_group_size_shift = 1;
  params.preview_mode = jxl::kSmallPreview;
  std::vector<uint8_t> data =
      jxl::CreateTestJXLCodestream(jxl::Bytes(pixels.data(), pixels.size()),
                                   xsize, ysize, num_channels, params);
  JxlPixelFormat format = {num_channels, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};

  std::vector<uint8_t> pixels2;
  pixels2.resize(pixels.size());

  JxlDecoder* dec = JxlDecoderCreate(nullptr);

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(
                dec, JXL_DEC_BASIC_INFO | JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE));

  // Ensure that the first part contains at least the full DC of the image,
  // otherwise flush does not work.
  size_t first_part = data.size() / 2;

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, data.data(), first_part));

  EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
  JxlBasicInfo info;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
  EXPECT_EQ(info.xsize, xsize);
  EXPECT_EQ(info.ysize, ysize);

  EXPECT_EQ(JXL_DEC_FRAME, JxlDecoderProcessInput(dec));

  // Output buffer not yet set
  EXPECT_EQ(JXL_DEC_ERROR, JxlDecoderFlushImage(dec));

  size_t buffer_size;
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderImageOutBufferSize(dec, &format, &buffer_size));
  EXPECT_EQ(pixels2.size(), buffer_size);
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                 dec, &format, pixels2.data(), pixels2.size()));

  // Must process input further until we get JXL_DEC_NEED_MORE_INPUT, even if
  // data was already input before, since the processing of the frame only
  // happens at the JxlDecoderProcessInput call after JXL_DEC_FRAME.
  EXPECT_EQ(JXL_DEC_NEED_MORE_INPUT, JxlDecoderProcessInput(dec));

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderFlushImage(dec));

  EXPECT_LE(jxl::test::ComparePixels(pixels2.data(), pixels.data(), xsize,
                                     ysize, format, format, 2560.0),
            2700u);

  EXPECT_EQ(JXL_DEC_NEED_MORE_INPUT, JxlDecoderProcessInput(dec));

  size_t consumed = first_part - JxlDecoderReleaseInput(dec);

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, data.data() + consumed,
                                                data.size() - consumed));

  EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));
  EXPECT_LE(jxl::test::ComparePixels(pixels2.data(), pixels.data(), xsize,
                                     ysize, format, format),
            0u);

  JxlDecoderDestroy(dec);
}

class DecodeProgressiveTest : public ::testing::TestWithParam<int> {};
JXL_GTEST_INSTANTIATE_TEST_SUITE_P(DecodeProgressiveTestInstantiation,
                                   DecodeProgressiveTest,
                                   ::testing::Range(0, 8));
TEST_P(DecodeProgressiveTest, ProgressiveEventTest) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  const int params = GetParam();
  bool single_group = ((params & 1) != 0);
  bool lossless = (((params >> 1) & 1) != 0);
  uint32_t num_channels = 3 + ((params >> 2) & 1);
  bool has_alpha = ((num_channels & 1) == 0);
  std::set<JxlProgressiveDetail> progressive_details = {kDC, kLastPasses,
                                                        kPasses};
  for (auto prog_detail : progressive_details) {
    // Only few combinations are expected to support outputting
    // intermediate flushes for complete DC and complete passes.
    // The test can be updated if more cases are expected to support it.
    bool expect_flush = !has_alpha && !lossless;
    size_t xsize;
    size_t ysize;
    if (single_group) {
      // An image smaller than 256x256 ensures it contains only 1 group.
      xsize = 99;
      ysize = 100;
    } else {
      xsize = 277;
      ysize = 280;
    }
    std::vector<uint8_t> pixels =
        jxl::test::GetSomeTestImage(xsize, ysize, num_channels, 0);
    JxlPixelFormat format = {num_channels, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
    jxl::ColorEncoding color_encoding = jxl::ColorEncoding::SRGB(false);
    jxl::CodecInOut io{memory_manager};
    EXPECT_TRUE(jxl::ConvertFromExternal(
        jxl::Bytes(pixels.data(), pixels.size()), xsize, ysize, color_encoding,
        /*bits_per_sample=*/16, format,
        /*pool=*/nullptr, &io.Main()));
    jxl::TestCodestreamParams params;
    if (lossless) {
      params.cparams.SetLossless();
    } else {
      params.cparams.butteraugli_distance = 0.5f;
    }
    jxl::PassDefinition passes[] = {
        {2, 0, 4}, {4, 0, 4}, {8, 2, 2}, {8, 1, 2}, {8, 0, 1}};
    const int kNumPasses = 5;
    jxl::ProgressiveMode progressive_mode{passes};
    params.cparams.custom_progressive_mode = &progressive_mode;
    std::vector<uint8_t> data =
        jxl::CreateTestJXLCodestream(jxl::Bytes(pixels.data(), pixels.size()),
                                     xsize, ysize, num_channels, params);

    for (size_t increment : {static_cast<size_t>(1), data.size()}) {
      printf(
          "Testing with single_group=%s, lossless=%s, "
          "num_channels=%d, prog_detail=%d, increment=%d\n",
          BoolToCStr(single_group), BoolToCStr(lossless),
          static_cast<int>(num_channels), static_cast<int>(prog_detail),
          static_cast<int>(increment));
      std::vector<std::vector<uint8_t>> passes(kNumPasses + 1);
      for (int i = 0; i <= kNumPasses; ++i) {
        passes[i].resize(pixels.size());
      }

      JxlDecoder* dec = JxlDecoderCreate(nullptr);

      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderSubscribeEvents(
                    dec, JXL_DEC_BASIC_INFO | JXL_DEC_FRAME |
                             JXL_DEC_FULL_IMAGE | JXL_DEC_FRAME_PROGRESSION));
      EXPECT_EQ(JXL_DEC_ERROR, JxlDecoderSetProgressiveDetail(dec, kFrames));
      EXPECT_EQ(JXL_DEC_ERROR,
                JxlDecoderSetProgressiveDetail(dec, kDCProgressive));
      EXPECT_EQ(JXL_DEC_ERROR, JxlDecoderSetProgressiveDetail(dec, kDCGroups));
      EXPECT_EQ(JXL_DEC_ERROR, JxlDecoderSetProgressiveDetail(dec, kGroups));
      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderSetProgressiveDetail(dec, prog_detail));

      uint8_t* next_in = data.data();
      size_t avail_in = 0;
      size_t pos = 0;

      auto process_input = [&]() {
        for (;;) {
          EXPECT_EQ(JXL_DEC_SUCCESS,
                    JxlDecoderSetInput(dec, next_in, avail_in));
          JxlDecoderStatus status = JxlDecoderProcessInput(dec);
          size_t remaining = JxlDecoderReleaseInput(dec);
          EXPECT_LE(remaining, avail_in);
          next_in += avail_in - remaining;
          avail_in = remaining;
          if (status == JXL_DEC_NEED_MORE_INPUT && pos < data.size()) {
            size_t chunk = std::min<size_t>(increment, data.size() - pos);
            pos += chunk;
            avail_in += chunk;
            continue;
          }
          return status;
        }
      };

      EXPECT_EQ(JXL_DEC_BASIC_INFO, process_input());
      JxlBasicInfo info;
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
      EXPECT_EQ(info.xsize, xsize);
      EXPECT_EQ(info.ysize, ysize);

      EXPECT_EQ(JXL_DEC_FRAME, process_input());

      size_t buffer_size;
      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderImageOutBufferSize(dec, &format, &buffer_size));
      EXPECT_EQ(pixels.size(), buffer_size);
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                     dec, &format, passes[kNumPasses].data(),
                                     passes[kNumPasses].size()));

      auto next_pass = [&](int pass) {
        if (prog_detail <= kDC) return kNumPasses;
        if (prog_detail <= kLastPasses) {
          return std::min(pass + 2, kNumPasses);
        }
        return pass + 1;
      };

      if (expect_flush) {
        // Return a particular downsampling ratio only after the last
        // pass for that downsampling was processed.
        int expected_downsampling_ratios[] = {8, 8, 4, 4, 2};
        for (int p = 0; p < kNumPasses; p = next_pass(p)) {
          EXPECT_EQ(JXL_DEC_FRAME_PROGRESSION, process_input());
          EXPECT_EQ(expected_downsampling_ratios[p],
                    JxlDecoderGetIntendedDownsamplingRatio(dec));
          EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderFlushImage(dec));
          passes[p] = passes[kNumPasses];
        }
      }

      EXPECT_EQ(JXL_DEC_FULL_IMAGE, process_input());
      EXPECT_EQ(JXL_DEC_SUCCESS, process_input());

      JxlDecoderDestroy(dec);

      if (!expect_flush) {
        continue;
      }
      jxl::ButteraugliParams butteraugli_params;
      std::vector<float> distances(kNumPasses + 1);
      for (int p = 0;; p = next_pass(p)) {
        jxl::CodecInOut io1{memory_manager};
        EXPECT_TRUE(jxl::ConvertFromExternal(
            jxl::Bytes(passes[p].data(), passes[p].size()), xsize, ysize,
            color_encoding,
            /*bits_per_sample=*/16, format,
            /*pool=*/nullptr, &io1.Main()));
        distances[p] =
            ButteraugliDistance(io.frames, io1.frames, butteraugli_params,
                                *JxlGetDefaultCms(), nullptr, nullptr);
        if (p == kNumPasses) break;
      }
      const float kMaxDistance[kNumPasses + 1] = {30.0f, 20.0f, 10.0f,
                                                  5.0f,  3.0f,  2.0f};
      EXPECT_LT(distances[kNumPasses], kMaxDistance[kNumPasses]);
      for (int p = 0; p < kNumPasses;) {
        int next_p = next_pass(p);
        EXPECT_LT(distances[p], kMaxDistance[p]);
        // Verify that the returned pass image is actually not the
        // same as the next pass image, by checking that it has a bit
        // worse butteraugli score.
        EXPECT_LT(distances[next_p] * 1.1f, distances[p]);
        p = next_p;
      }
    }
  }
}

void VerifyJPEGReconstruction(jxl::Span<const uint8_t> container,
                              jxl::Span<const uint8_t> jpeg_bytes) {
  JxlDecoderPtr dec = JxlDecoderMake(nullptr);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(
                dec.get(), JXL_DEC_JPEG_RECONSTRUCTION | JXL_DEC_FULL_IMAGE));
  JxlDecoderSetInput(dec.get(), container.data(), container.size());
  EXPECT_EQ(JXL_DEC_JPEG_RECONSTRUCTION, JxlDecoderProcessInput(dec.get()));
  std::vector<uint8_t> reconstructed_buffer(128);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetJPEGBuffer(dec.get(), reconstructed_buffer.data(),
                                    reconstructed_buffer.size()));
  size_t used = 0;
  JxlDecoderStatus process_result = JXL_DEC_JPEG_NEED_MORE_OUTPUT;
  while (process_result == JXL_DEC_JPEG_NEED_MORE_OUTPUT) {
    used = reconstructed_buffer.size() - JxlDecoderReleaseJPEGBuffer(dec.get());
    reconstructed_buffer.resize(reconstructed_buffer.size() * 2);
    EXPECT_EQ(
        JXL_DEC_SUCCESS,
        JxlDecoderSetJPEGBuffer(dec.get(), reconstructed_buffer.data() + used,
                                reconstructed_buffer.size() - used));
    process_result = JxlDecoderProcessInput(dec.get());
  }
  ASSERT_EQ(JXL_DEC_FULL_IMAGE, process_result);
  used = reconstructed_buffer.size() - JxlDecoderReleaseJPEGBuffer(dec.get());
  ASSERT_EQ(used, jpeg_bytes.size());
  EXPECT_EQ(0, memcmp(reconstructed_buffer.data(), jpeg_bytes.data(), used));
}

JXL_TRANSCODE_JPEG_TEST(DecodeTest, JPEGReconstructTestCodestream) {
  TEST_LIBJPEG_SUPPORT();
  size_t xsize = 123;
  size_t ysize = 77;
  size_t channels = 3;
  std::vector<uint8_t> pixels =
      jxl::test::GetSomeTestImage(xsize, ysize, channels, /*seed=*/0);
  std::vector<uint8_t> jpeg_codestream;
  jxl::TestCodestreamParams params;
  params.cparams.color_transform = jxl::ColorTransform::kNone;
  params.box_format = kCSBF_Single;
  params.jpeg_codestream = &jpeg_codestream;
  params.preview_mode = jxl::kSmallPreview;
  std::vector<uint8_t> compressed = jxl::CreateTestJXLCodestream(
      jxl::Bytes(pixels.data(), pixels.size()), xsize, ysize, channels, params);
  VerifyJPEGReconstruction(jxl::Bytes(compressed), jxl::Bytes(jpeg_codestream));
}

JXL_TRANSCODE_JPEG_TEST(DecodeTest, JPEGReconstructionTest) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  const std::string jpeg_path = "jxl/flower/flower.png.im_q85_420.jpg";
  const std::vector<uint8_t> orig = jxl::test::ReadTestData(jpeg_path);
  jxl::CodecInOut orig_io{memory_manager};
  ASSERT_TRUE(jxl::jpeg::DecodeImageJPG(jxl::Bytes(orig), &orig_io));
  jxl::jpeg::JPEGData jpeg_data_copy = *orig_io.Main().jpeg_data;
  orig_io.metadata.m.xyb_encoded = false;
  jxl::BitWriter writer{memory_manager};
  ASSERT_TRUE(WriteCodestreamHeaders(&orig_io.metadata, &writer, nullptr));
  writer.ZeroPadToByte();
  jxl::CompressParams cparams;
  cparams.color_transform = jxl::ColorTransform::kNone;
  ASSERT_TRUE(jxl::EncodeFrame(memory_manager, cparams, jxl::FrameInfo{},
                               &orig_io.metadata, orig_io.Main(),
                               *JxlGetDefaultCms(),
                               /*pool=*/nullptr, &writer,
                               /*aux_out=*/nullptr));

  std::vector<uint8_t> jpeg_data;
  ASSERT_TRUE(
      EncodeJPEGData(memory_manager, jpeg_data_copy, &jpeg_data, cparams));
  std::vector<uint8_t> container;
  jxl::Bytes(jxl::kContainerHeader).AppendTo(container);
  jxl::AppendBoxHeader(jxl::MakeBoxType("jbrd"), jpeg_data.size(), false,
                       &container);
  jxl::Bytes(jpeg_data).AppendTo(container);
  jxl::AppendBoxHeader(jxl::MakeBoxType("jxlc"), 0, true, &container);
  jxl::PaddedBytes codestream = std::move(writer).TakeBytes();
  jxl::Bytes(codestream).AppendTo(container);
  VerifyJPEGReconstruction(jxl::Bytes(container), jxl::Bytes(orig));
}

JXL_TRANSCODE_JPEG_TEST(DecodeTest, JPEGReconstructionMetadataTest) {
  const std::string jpeg_path = "jxl/jpeg_reconstruction/1x1_exif_xmp.jpg";
  const std::string jxl_path = "jxl/jpeg_reconstruction/1x1_exif_xmp.jxl";
  const std::vector<uint8_t> jpeg = jxl::test::ReadTestData(jpeg_path);
  const std::vector<uint8_t> jxl = jxl::test::ReadTestData(jxl_path);
  VerifyJPEGReconstruction(jxl::Bytes(jxl), jxl::Bytes(jpeg));
}

TEST(DecodeTest, ContinueFinalNonEssentialBoxTest) {
  size_t xsize = 80;
  size_t ysize = 90;
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);
  jxl::TestCodestreamParams params;
  params.box_format = kCSBF_Multi_Other_Terminated;
  params.add_icc_profile = true;
  std::vector<uint8_t> compressed = jxl::CreateTestJXLCodestream(
      jxl::Bytes(pixels.data(), pixels.size()), xsize, ysize, 4, params);
  StreamPositions streampos;
  AnalyzeCodestream(compressed, &streampos);

  // The non-essential final box size including 8-byte header
  size_t final_box_size = unk3_box_size + 8;
  size_t last_box_begin = compressed.size() - final_box_size;
  // Verify that the test is indeed setup correctly to be at the beginning of
  // the 'unkn' box header.
  ASSERT_EQ(compressed[last_box_begin + 3], final_box_size);
  ASSERT_EQ(compressed[last_box_begin + 4], 'u');
  ASSERT_EQ(compressed[last_box_begin + 5], 'n');
  ASSERT_EQ(compressed[last_box_begin + 6], 'k');
  ASSERT_EQ(compressed[last_box_begin + 7], '3');

  JxlDecoder* dec = JxlDecoderCreate(nullptr);

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO | JXL_DEC_FRAME));

  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetInput(dec, compressed.data(), last_box_begin));

  EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
  EXPECT_EQ(JXL_DEC_FRAME, JxlDecoderProcessInput(dec));
  // The decoder returns success despite not having seen the final unknown box
  // yet. This is because calling JxlDecoderCloseInput is not mandatory for
  // backwards compatibility, so it doesn't know more bytes follow, the current
  // bytes ended at a perfectly valid place.
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));

  size_t remaining = JxlDecoderReleaseInput(dec);
  // Since the test was set up to end exactly at the boundary of the final
  // codestream box, and the decoder returned success, all bytes are expected to
  // be consumed until the end of the  frame header.
  EXPECT_EQ(remaining, last_box_begin - streampos.frames[0].toc_end);

  // Now set the remaining non-codestream box as input.
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSetInput(dec, compressed.data() + last_box_begin,
                               compressed.size() - last_box_begin));
  // Even though JxlDecoderProcessInput already returned JXL_DEC_SUCCESS before,
  // when calling it again now after setting more input, success is expected, no
  // event occurs but the box has been successfully skipped.
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));

  JxlDecoderDestroy(dec);
}

namespace {
bool BoxTypeEquals(const std::string& type_string, const JxlBoxType type) {
  return type_string.size() == 4 && type_string[0] == type[0] &&
         type_string[1] == type[1] && type_string[2] == type[2] &&
         type_string[3] == type[3];
}
}  // namespace

TEST(DecodeTest, ExtendedBoxSizeTest) {
  const std::string jxl_path = "jxl/boxes/square-extended-size-container.jxl";
  const std::vector<uint8_t> orig = jxl::test::ReadTestData(jxl_path);
  JxlDecoder* dec = JxlDecoderCreate(nullptr);

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSubscribeEvents(dec, JXL_DEC_BOX));

  JxlBoxType type;
  uint64_t box_size;
  uint64_t contents_size;
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, orig.data(), orig.size()));
  EXPECT_EQ(JXL_DEC_BOX, JxlDecoderProcessInput(dec));
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBoxType(dec, type, JXL_FALSE));
  EXPECT_TRUE(BoxTypeEquals("JXL ", type));
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBoxSizeRaw(dec, &box_size));
  EXPECT_EQ(12, box_size);
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBoxSizeContents(dec, &contents_size));
  EXPECT_EQ(contents_size + 8, box_size);
  EXPECT_EQ(JXL_DEC_BOX, JxlDecoderProcessInput(dec));
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBoxType(dec, type, JXL_FALSE));
  EXPECT_TRUE(BoxTypeEquals("ftyp", type));
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBoxSizeRaw(dec, &box_size));
  EXPECT_EQ(20, box_size);
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBoxSizeContents(dec, &contents_size));
  EXPECT_EQ(contents_size + 8, box_size);
  EXPECT_EQ(JXL_DEC_BOX, JxlDecoderProcessInput(dec));
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBoxType(dec, type, JXL_FALSE));
  EXPECT_TRUE(BoxTypeEquals("jxlc", type));
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBoxSizeRaw(dec, &box_size));
  EXPECT_EQ(72, box_size);
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBoxSizeContents(dec, &contents_size));
  // This is an extended box, hence the difference between `box_size` and
  // `contents_size` is 16.
  EXPECT_EQ(contents_size + 8 + 8, box_size);

  JxlDecoderDestroy(dec);
}

JXL_BOXES_TEST(DecodeTest, BoxTest) {
  size_t xsize = 1;
  size_t ysize = 1;
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);
  jxl::TestCodestreamParams params;
  params.box_format = kCSBF_Multi_Other_Terminated;
  params.add_icc_profile = true;
  std::vector<uint8_t> compressed = jxl::CreateTestJXLCodestream(
      jxl::Bytes(pixels.data(), pixels.size()), xsize, ysize, 4, params);

  JxlDecoder* dec = JxlDecoderCreate(nullptr);

  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSubscribeEvents(dec, JXL_DEC_BOX));

  std::vector<std::string> expected_box_types = {
      "JXL ", "ftyp", "jxlp", "unk1", "unk2", "jxlp", "jxlp", "jxlp", "unk3"};

  // Value 0 means to not test the size: codestream is not required to be a
  // particular exact size.
  std::vector<size_t> expected_box_sizes = {12, 20, 0, 34, 18, 0, 0, 0, 20};

  JxlBoxType type;
  uint64_t box_size;
  uint64_t contents_size;
  std::vector<uint8_t> contents(50);
  size_t expected_release_size = 0;

  // Cannot get these when decoding didn't start yet
  EXPECT_EQ(JXL_DEC_ERROR, JxlDecoderGetBoxType(dec, type, JXL_FALSE));
  EXPECT_EQ(JXL_DEC_ERROR, JxlDecoderGetBoxSizeRaw(dec, &box_size));

  uint8_t* next_in = compressed.data();
  size_t avail_in = compressed.size();
  for (size_t i = 0; i < expected_box_types.size(); i++) {
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));
    EXPECT_EQ(JXL_DEC_BOX, JxlDecoderProcessInput(dec));
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBoxType(dec, type, JXL_FALSE));
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBoxSizeRaw(dec, &box_size));
    EXPECT_TRUE(BoxTypeEquals(expected_box_types[i], type));
    if (expected_box_sizes[i]) {
      EXPECT_EQ(expected_box_sizes[i], box_size);
      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderGetBoxSizeContents(dec, &contents_size));
      EXPECT_EQ(contents_size + 8, box_size);
    }

    if (expected_release_size > 0) {
      EXPECT_EQ(expected_release_size, JxlDecoderReleaseBoxBuffer(dec));
      expected_release_size = 0;
    }

    if (type[0] == 'u' && type[1] == 'n' && type[2] == 'k') {
      JxlDecoderSetBoxBuffer(dec, contents.data(), contents.size());
      size_t expected_box_contents_size =
          type[3] == '1' ? unk1_box_size
                         : (type[3] == '2' ? unk2_box_size : unk3_box_size);
      expected_release_size = contents.size() - expected_box_contents_size;
    }
    size_t consumed = avail_in - JxlDecoderReleaseInput(dec);
    next_in += consumed;
    avail_in -= consumed;
  }

  // After the last DEC_BOX event, check that the input position is exactly at
  // the stat of the box header.
  EXPECT_EQ(avail_in, expected_box_sizes.back());

  // Even though all input is given, the decoder cannot assume there aren't
  // more boxes if the input was not closed.
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec, next_in, avail_in));
  EXPECT_EQ(JXL_DEC_NEED_MORE_INPUT, JxlDecoderProcessInput(dec));
  JxlDecoderCloseInput(dec);
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));

  JxlDecoderDestroy(dec);
}

JXL_BOXES_TEST(DecodeTest, ExifBrobBoxTest) {
  size_t xsize = 1;
  size_t ysize = 1;
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);
  jxl::TestCodestreamParams params;
  // Lossless to verify pixels exactly after roundtrip.
  params.cparams.SetLossless();
  params.box_format = kCSBF_Brob_Exif;
  params.add_icc_profile = true;
  std::vector<uint8_t> compressed = jxl::CreateTestJXLCodestream(
      jxl::Bytes(pixels.data(), pixels.size()), xsize, ysize, 4, params);

  // Test raw brob box, not brotli-decompressing
  for (int streaming = 0; streaming < 2; ++streaming) {
    JxlDecoder* dec = JxlDecoderCreate(nullptr);

    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSubscribeEvents(dec, JXL_DEC_BOX));
    if (!streaming) {
      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderSetInput(dec, compressed.data(), compressed.size()));
      JxlDecoderCloseInput(dec);
    }
    // for streaming input case
    const uint8_t* next_in = compressed.data();
    size_t avail_in = 0;
    size_t total_in = 0;
    size_t step_size = 64;

    std::vector<uint8_t> box_buffer;
    size_t box_num_output;
    bool seen_brob_begin = false;
    bool seen_brob_end = false;

    for (;;) {
      JxlDecoderStatus status = JxlDecoderProcessInput(dec);
      if (status == JXL_DEC_NEED_MORE_INPUT) {
        if (streaming) {
          size_t remaining = JxlDecoderReleaseInput(dec);
          EXPECT_LE(remaining, avail_in);
          next_in += avail_in - remaining;
          avail_in = remaining;
          size_t amount = step_size;
          if (total_in + amount > compressed.size()) {
            amount = compressed.size() - total_in;
          }
          avail_in += amount;
          total_in += amount;
          EXPECT_EQ(JXL_DEC_SUCCESS,
                    JxlDecoderSetInput(dec, next_in, avail_in));
          if (total_in == compressed.size()) JxlDecoderCloseInput(dec);
        } else {
          FAIL();
          break;
        }
      } else if (status == JXL_DEC_BOX || status == JXL_DEC_SUCCESS) {
        if (!box_buffer.empty()) {
          EXPECT_EQ(false, seen_brob_end);
          seen_brob_end = true;
          size_t remaining = JxlDecoderReleaseBoxBuffer(dec);
          box_num_output = box_buffer.size() - remaining;
          EXPECT_EQ(box_num_output, box_brob_exif_size - 8);
          EXPECT_EQ(
              0, memcmp(box_buffer.data(), box_brob_exif + 8, box_num_output));
          box_buffer.clear();
        }
        if (status == JXL_DEC_SUCCESS) break;
        JxlBoxType type;
        EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBoxType(dec, type, JXL_FALSE));
        if (BoxTypeEquals("brob", type)) {
          EXPECT_EQ(false, seen_brob_begin);
          seen_brob_begin = true;
          box_buffer.resize(8);
          JxlDecoderSetBoxBuffer(dec, box_buffer.data(), box_buffer.size());
        }
      } else if (status == JXL_DEC_BOX_NEED_MORE_OUTPUT) {
        size_t remaining = JxlDecoderReleaseBoxBuffer(dec);
        box_num_output = box_buffer.size() - remaining;
        box_buffer.resize(box_buffer.size() * 2);
        JxlDecoderSetBoxBuffer(dec, box_buffer.data() + box_num_output,
                               box_buffer.size() - box_num_output);
      } else {
        // We do not expect any other events or errors
        FAIL();
        break;
      }
    }

    EXPECT_EQ(true, seen_brob_begin);
    EXPECT_EQ(true, seen_brob_end);

    JxlDecoderDestroy(dec);
  }

  // Test decompressed brob box
  for (int streaming = 0; streaming < 2; ++streaming) {
    JxlDecoder* dec = JxlDecoderCreate(nullptr);

    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSubscribeEvents(dec, JXL_DEC_BOX));
    if (!streaming) {
      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderSetInput(dec, compressed.data(), compressed.size()));
      JxlDecoderCloseInput(dec);
    }
    // for streaming input case
    const uint8_t* next_in = compressed.data();
    size_t avail_in = 0;
    size_t total_in = 0;
    size_t step_size = 64;

    std::vector<uint8_t> box_buffer;
    size_t box_num_output;
    bool seen_exif_begin = false;
    bool seen_exif_end = false;

    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetDecompressBoxes(dec, JXL_TRUE));

    for (;;) {
      JxlDecoderStatus status = JxlDecoderProcessInput(dec);
      if (status == JXL_DEC_NEED_MORE_INPUT) {
        if (streaming) {
          size_t remaining = JxlDecoderReleaseInput(dec);
          EXPECT_LE(remaining, avail_in);
          next_in += avail_in - remaining;
          avail_in = remaining;
          size_t amount = step_size;
          if (total_in + amount > compressed.size()) {
            amount = compressed.size() - total_in;
          }
          avail_in += amount;
          total_in += amount;
          EXPECT_EQ(JXL_DEC_SUCCESS,
                    JxlDecoderSetInput(dec, next_in, avail_in));
          if (total_in == compressed.size()) JxlDecoderCloseInput(dec);
        } else {
          FAIL();
          break;
        }
      } else if (status == JXL_DEC_BOX || status == JXL_DEC_SUCCESS) {
        if (!box_buffer.empty()) {
          EXPECT_EQ(false, seen_exif_end);
          seen_exif_end = true;
          size_t remaining = JxlDecoderReleaseBoxBuffer(dec);
          box_num_output = box_buffer.size() - remaining;
          // Expect that the output has the same size and contents as the
          // uncompressed exif data. Only check contents if the sizes match to
          // avoid comparing uninitialized memory in the test.
          EXPECT_EQ(box_num_output, exif_uncompressed_size);
          if (box_num_output == exif_uncompressed_size) {
            EXPECT_EQ(0, memcmp(box_buffer.data(), exif_uncompressed,
                                exif_uncompressed_size));
          }
          box_buffer.clear();
        }
        if (status == JXL_DEC_SUCCESS) break;
        JxlBoxType type;
        EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBoxType(dec, type, JXL_TRUE));
        if (BoxTypeEquals("Exif", type)) {
          EXPECT_EQ(false, seen_exif_begin);
          seen_exif_begin = true;
          box_buffer.resize(8);
          JxlDecoderSetBoxBuffer(dec, box_buffer.data(), box_buffer.size());
        }
      } else if (status == JXL_DEC_BOX_NEED_MORE_OUTPUT) {
        size_t remaining = JxlDecoderReleaseBoxBuffer(dec);
        box_num_output = box_buffer.size() - remaining;
        box_buffer.resize(box_buffer.size() * 2);
        JxlDecoderSetBoxBuffer(dec, box_buffer.data() + box_num_output,
                               box_buffer.size() - box_num_output);
      } else {
        // We do not expect any other events or errors
        FAIL();
        break;
      }
    }

    EXPECT_EQ(true, seen_exif_begin);
    EXPECT_EQ(true, seen_exif_end);

    JxlDecoderDestroy(dec);
  }
}

JXL_BOXES_TEST(DecodeTest, PartialCodestreamBoxTest) {
  size_t xsize = 23;
  size_t ysize = 81;
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);
  JxlPixelFormat format_orig = {4, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
  // Lossless to verify pixels exactly after roundtrip.
  jxl::TestCodestreamParams params;
  params.cparams.SetLossless();
  params.cparams.speed_tier = jxl::SpeedTier::kThunder;
  params.box_format = kCSBF_Multi;
  params.add_icc_profile = true;
  std::vector<uint8_t> compressed = jxl::CreateTestJXLCodestream(
      jxl::Bytes(pixels.data(), pixels.size()), xsize, ysize, 4, params);

  std::vector<uint8_t> extracted_codestream;

  {
    JxlDecoder* dec = JxlDecoderCreate(nullptr);

    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderSubscribeEvents(
                  dec, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE | JXL_DEC_BOX));
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderSetInput(dec, compressed.data(), compressed.size()));
    JxlDecoderCloseInput(dec);

    size_t num_jxlp = 0;

    std::vector<uint8_t> pixels2;
    pixels2.resize(pixels.size());

    std::vector<uint8_t> box_buffer;
    size_t box_num_output;

    for (;;) {
      JxlDecoderStatus status = JxlDecoderProcessInput(dec);
      if (status == JXL_DEC_NEED_MORE_INPUT) {
        FAIL();
        break;
      } else if (status == JXL_DEC_BASIC_INFO) {
        JxlBasicInfo info;
        EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
        EXPECT_EQ(info.xsize, xsize);
        EXPECT_EQ(info.ysize, ysize);
      } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
        EXPECT_EQ(JXL_DEC_SUCCESS,
                  JxlDecoderSetImageOutBuffer(dec, &format_orig, pixels2.data(),
                                              pixels2.size()));
      } else if (status == JXL_DEC_FULL_IMAGE) {
        continue;
      } else if (status == JXL_DEC_BOX || status == JXL_DEC_SUCCESS) {
        if (!box_buffer.empty()) {
          size_t remaining = JxlDecoderReleaseBoxBuffer(dec);
          box_num_output = box_buffer.size() - remaining;
          EXPECT_GE(box_num_output, 4);
          // Do not insert the first 4 bytes, which are not part of the
          // codestream, but the partial codestream box index
          extracted_codestream.insert(extracted_codestream.end(),
                                      box_buffer.begin() + 4,
                                      box_buffer.begin() + box_num_output);
          box_buffer.clear();
        }
        if (status == JXL_DEC_SUCCESS) break;
        JxlBoxType type;
        EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBoxType(dec, type, JXL_FALSE));
        if (BoxTypeEquals("jxlp", type)) {
          num_jxlp++;
          box_buffer.resize(8);
          JxlDecoderSetBoxBuffer(dec, box_buffer.data(), box_buffer.size());
        }
      } else if (status == JXL_DEC_BOX_NEED_MORE_OUTPUT) {
        size_t remaining = JxlDecoderReleaseBoxBuffer(dec);
        box_num_output = box_buffer.size() - remaining;
        box_buffer.resize(box_buffer.size() * 2);
        JxlDecoderSetBoxBuffer(dec, box_buffer.data() + box_num_output,
                               box_buffer.size() - box_num_output);
      } else {
        // We do not expect any other events or errors
        FAIL();
        break;
      }
    }

    // The test file created with kCSBF_Multi is expected to have 4 jxlp boxes.
    EXPECT_EQ(4, num_jxlp);

    EXPECT_EQ(0u, jxl::test::ComparePixels(pixels.data(), pixels2.data(), xsize,
                                           ysize, format_orig, format_orig));

    JxlDecoderDestroy(dec);
  }

  // Now test whether the codestream extracted from the jxlp boxes can itself
  // also be decoded and gives the same pixels
  {
    JxlDecoder* dec = JxlDecoderCreate(nullptr);

    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderSubscribeEvents(
                  dec, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE | JXL_DEC_BOX));
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderSetInput(dec, extracted_codestream.data(),
                                 extracted_codestream.size()));
    JxlDecoderCloseInput(dec);

    size_t num_boxes = 0;

    std::vector<uint8_t> pixels2;
    pixels2.resize(pixels.size());

    std::vector<uint8_t> box_buffer;
    size_t box_num_output;

    for (;;) {
      JxlDecoderStatus status = JxlDecoderProcessInput(dec);
      if (status == JXL_DEC_NEED_MORE_INPUT) {
        FAIL();
        break;
      } else if (status == JXL_DEC_BASIC_INFO) {
        JxlBasicInfo info;
        EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &info));
        EXPECT_EQ(info.xsize, xsize);
        EXPECT_EQ(info.ysize, ysize);
      } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
        EXPECT_EQ(JXL_DEC_SUCCESS,
                  JxlDecoderSetImageOutBuffer(dec, &format_orig, pixels2.data(),
                                              pixels2.size()));
      } else if (status == JXL_DEC_FULL_IMAGE) {
        continue;
      } else if (status == JXL_DEC_BOX) {
        num_boxes++;
      } else if (status == JXL_DEC_BOX_NEED_MORE_OUTPUT) {
        size_t remaining = JxlDecoderReleaseBoxBuffer(dec);
        box_num_output = box_buffer.size() - remaining;
        box_buffer.resize(box_buffer.size() * 2);
        JxlDecoderSetBoxBuffer(dec, box_buffer.data() + box_num_output,
                               box_buffer.size() - box_num_output);
      } else if (status == JXL_DEC_SUCCESS) {
        break;
      } else {
        // We do not expect any other events or errors
        FAIL();
        break;
      }
    }

    EXPECT_EQ(0, num_boxes);  // The data does not use the container format.
    EXPECT_EQ(0u, jxl::test::ComparePixels(pixels.data(), pixels2.data(), xsize,
                                           ysize, format_orig, format_orig));

    JxlDecoderDestroy(dec);
  }
}

TEST(DecodeTest, SpotColorTest) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  jxl::CodecInOut io{memory_manager};
  size_t xsize = 55;
  size_t ysize = 257;
  io.metadata.m.color_encoding = jxl::ColorEncoding::LinearSRGB();
  JXL_TEST_ASSIGN_OR_DIE(Image3F main,
                         Image3F::Create(memory_manager, xsize, ysize));
  JXL_TEST_ASSIGN_OR_DIE(ImageF spot,
                         ImageF::Create(memory_manager, xsize, ysize));
  jxl::ZeroFillImage(&main);
  jxl::ZeroFillImage(&spot);

  for (size_t y = 0; y < ysize; y++) {
    float* JXL_RESTRICT rowm = main.PlaneRow(1, y);
    float* JXL_RESTRICT rows = spot.Row(y);
    for (size_t x = 0; x < xsize; x++) {
      rowm[x] = (x + y) * (1.f / 255.f);
      rows[x] = ((x ^ y) & 255) * (1.f / 255.f);
    }
  }
  ASSERT_TRUE(
      io.SetFromImage(std::move(main), jxl::ColorEncoding::LinearSRGB()));
  jxl::ExtraChannelInfo info;
  info.bit_depth.bits_per_sample = 8;
  info.dim_shift = 0;
  info.type = jxl::ExtraChannel::kSpotColor;
  info.spot_color[0] = 0.5f;
  info.spot_color[1] = 0.2f;
  info.spot_color[2] = 1.f;
  info.spot_color[3] = 0.5f;

  io.metadata.m.extra_channel_info.push_back(info);
  std::vector<ImageF> ec;
  ec.push_back(std::move(spot));
  ASSERT_TRUE(io.frames[0].SetExtraChannels(std::move(ec)));

  jxl::CompressParams cparams;
  cparams.speed_tier = jxl::SpeedTier::kLightning;
  cparams.modular_mode = true;
  cparams.color_transform = jxl::ColorTransform::kNone;
  cparams.butteraugli_distance = 0.f;

  std::vector<uint8_t> compressed;
  EXPECT_TRUE(jxl::test::EncodeFile(cparams, &io, &compressed));

  for (size_t render_spot = 0; render_spot < 2; render_spot++) {
    JxlPixelFormat format = {3, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, 0};

    JxlDecoder* dec = JxlDecoderCreate(nullptr);

    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderSubscribeEvents(
                  dec, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE));
    if (!render_spot) {
      EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetRenderSpotcolors(dec, JXL_FALSE));
    }

    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderSetInput(dec, compressed.data(), compressed.size()));
    EXPECT_EQ(JXL_DEC_BASIC_INFO, JxlDecoderProcessInput(dec));
    JxlBasicInfo binfo;
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBasicInfo(dec, &binfo));
    EXPECT_EQ(1u, binfo.num_extra_channels);
    EXPECT_EQ(xsize, binfo.xsize);
    EXPECT_EQ(ysize, binfo.ysize);

    JxlExtraChannelInfo extra_info;
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderGetExtraChannelInfo(dec, 0, &extra_info));
    EXPECT_EQ(static_cast<unsigned int>(jxl::ExtraChannel::kSpotColor),
              extra_info.type);

    EXPECT_EQ(JXL_DEC_NEED_IMAGE_OUT_BUFFER, JxlDecoderProcessInput(dec));
    size_t buffer_size;
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderImageOutBufferSize(dec, &format, &buffer_size));
    size_t extra_size;
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderExtraChannelBufferSize(dec, &format, &extra_size, 0));

    std::vector<uint8_t> image(buffer_size);
    std::vector<uint8_t> extra(extra_size);
    size_t bytes_per_pixel = format.num_channels *
                             jxl::test::GetDataBits(format.data_type) /
                             jxl::kBitsPerByte;
    size_t stride = bytes_per_pixel * binfo.xsize;

    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetImageOutBuffer(
                                   dec, &format, image.data(), image.size()));
    EXPECT_EQ(JXL_DEC_SUCCESS,
              JxlDecoderSetExtraChannelBuffer(dec, &format, extra.data(),
                                              extra.size(), 0));

    EXPECT_EQ(JXL_DEC_FULL_IMAGE, JxlDecoderProcessInput(dec));

    // After the full image was output, JxlDecoderProcessInput should return
    // success to indicate all is done.
    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderProcessInput(dec));
    JxlDecoderDestroy(dec);

    for (size_t y = 0; y < ysize; y++) {
      uint8_t* JXL_RESTRICT rowm = image.data() + stride * y;
      uint8_t* JXL_RESTRICT rows = extra.data() + xsize * y;
      for (size_t x = 0; x < xsize; x++) {
        if (!render_spot) {
          // if spot color isn't rendered, main image should be as we made it
          // (red and blue are all zeroes)

          EXPECT_EQ(rowm[x * 3 + 0], 0);
          EXPECT_EQ(rowm[x * 3 + 1], (x + y > 255 ? 255 : x + y));
          EXPECT_EQ(rowm[x * 3 + 2], 0);
        }
        if (render_spot) {
          // if spot color is rendered, expect red and blue to look like the
          // spot color channel
          EXPECT_LT(abs(rowm[x * 3 + 0] - (rows[x] * 0.25f)), 1);
          EXPECT_LT(abs(rowm[x * 3 + 2] - (rows[x] * 0.5f)), 1);
        }
        EXPECT_EQ(rows[x], ((x ^ y) & 255));
      }
    }
  }
}

TEST(DecodeTest, CloseInput) {
  std::vector<uint8_t> partial_file = {0xff};

  JxlDecoderPtr dec = JxlDecoderMake(nullptr);
  EXPECT_EQ(JXL_DEC_SUCCESS,
            JxlDecoderSubscribeEvents(dec.get(),
                                      JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE));
  EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSetInput(dec.get(), partial_file.data(),
                                                partial_file.size()));
  EXPECT_EQ(JXL_DEC_NEED_MORE_INPUT, JxlDecoderProcessInput(dec.get()));
  EXPECT_EQ(JXL_DEC_NEED_MORE_INPUT, JxlDecoderProcessInput(dec.get()));
  JxlDecoderCloseInput(dec.get());
  EXPECT_EQ(JXL_DEC_ERROR, JxlDecoderProcessInput(dec.get()));
}
