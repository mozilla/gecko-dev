// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/test_utils.h"

#include <jxl/cms.h>
#include <jxl/cms_interface.h>
#include <jxl/memory_manager.h>
#include <jxl/types.h>

#include <cstddef>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "lib/extras/metrics.h"
#include "lib/extras/packed_image_convert.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/float.h"
#include "lib/jxl/base/printf_macros.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/enc_aux_out.h"
#include "lib/jxl/enc_bit_writer.h"
#include "lib/jxl/enc_butteraugli_comparator.h"
#include "lib/jxl/enc_cache.h"
#include "lib/jxl/enc_external_image.h"
#include "lib/jxl/enc_fields.h"
#include "lib/jxl/enc_frame.h"
#include "lib/jxl/enc_icc_codec.h"
#include "lib/jxl/enc_params.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/icc_codec.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/padded_bytes.h"
#include "lib/jxl/test_memory_manager.h"

#if !defined(TEST_DATA_PATH)
#include "tools/cpp/runfiles/runfiles.h"
#endif

namespace jxl {
namespace test {

void Check(bool ok) {
  if (!ok) {
    JXL_CRASH();
  }
}

#if defined(TEST_DATA_PATH)
std::string GetTestDataPath(const std::string& filename) {
  return std::string(TEST_DATA_PATH "/") + filename;
}
#else
using ::bazel::tools::cpp::runfiles::Runfiles;
const std::unique_ptr<Runfiles> kRunfiles(Runfiles::Create(""));
std::string GetTestDataPath(const std::string& filename) {
  std::string root(JPEGXL_ROOT_PACKAGE "/testdata/");
  return kRunfiles->Rlocation(root + filename);
}
#endif

jxl::IccBytes GetIccTestProfile() {
  return ReadTestData("external/Compact-ICC-Profiles/profiles/scRGB-v2.icc");
}

std::vector<uint8_t> GetCompressedIccTestProfile() {
  BitWriter writer(MemoryManager());
  const IccBytes icc = GetIccTestProfile();
  Check(
      WriteICC(Span<const uint8_t>(icc), &writer, LayerType::Header, nullptr));
  writer.ZeroPadToByte();
  jxl::Bytes bytes = writer.GetSpan();
  return std::vector<uint8_t>(bytes.begin(), bytes.end());
}

std::vector<uint8_t> ReadTestData(const std::string& filename) {
  std::string full_path = GetTestDataPath(filename);
  fprintf(stderr, "ReadTestData %s\n", full_path.c_str());
  std::ifstream file(full_path, std::ios::binary);
  std::vector<char> str((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
  Check(file.good());
  const uint8_t* raw = reinterpret_cast<const uint8_t*>(str.data());
  std::vector<uint8_t> data(raw, raw + str.size());
  printf("Test data %s is %d bytes long.\n", filename.c_str(),
         static_cast<int>(data.size()));
  return data;
}

void DefaultAcceptedFormats(extras::JXLDecompressParams& dparams) {
  if (dparams.accepted_formats.empty()) {
    for (const uint32_t num_channels : {1, 2, 3, 4}) {
      dparams.accepted_formats.push_back(
          {num_channels, JXL_TYPE_FLOAT, JXL_LITTLE_ENDIAN, /*align=*/0});
    }
  }
}

Status DecodeFile(extras::JXLDecompressParams dparams,
                  const Span<const uint8_t> file, CodecInOut* JXL_RESTRICT io,
                  ThreadPool* pool) {
  DefaultAcceptedFormats(dparams);
  SetThreadParallelRunner(dparams, pool);
  extras::PackedPixelFile ppf;
  JXL_RETURN_IF_ERROR(DecodeImageJXL(file.data(), file.size(), dparams,
                                     /*decoded_bytes=*/nullptr, &ppf));
  JXL_RETURN_IF_ERROR(ConvertPackedPixelFileToCodecInOut(ppf, pool, io));
  return true;
}

void JxlBasicInfoSetFromPixelFormat(JxlBasicInfo* basic_info,
                                    const JxlPixelFormat* pixel_format) {
  JxlEncoderInitBasicInfo(basic_info);
  switch (pixel_format->data_type) {
    case JXL_TYPE_FLOAT:
      basic_info->bits_per_sample = 32;
      basic_info->exponent_bits_per_sample = 8;
      break;
    case JXL_TYPE_FLOAT16:
      basic_info->bits_per_sample = 16;
      basic_info->exponent_bits_per_sample = 5;
      break;
    case JXL_TYPE_UINT8:
      basic_info->bits_per_sample = 8;
      basic_info->exponent_bits_per_sample = 0;
      break;
    case JXL_TYPE_UINT16:
      basic_info->bits_per_sample = 16;
      basic_info->exponent_bits_per_sample = 0;
      break;
    default:
      Check(false);
  }
  if (pixel_format->num_channels < 3) {
    basic_info->num_color_channels = 1;
  } else {
    basic_info->num_color_channels = 3;
  }
  if (pixel_format->num_channels == 2 || pixel_format->num_channels == 4) {
    basic_info->alpha_exponent_bits = basic_info->exponent_bits_per_sample;
    basic_info->alpha_bits = basic_info->bits_per_sample;
    basic_info->num_extra_channels = 1;
  } else {
    basic_info->alpha_exponent_bits = 0;
    basic_info->alpha_bits = 0;
  }
}

ColorEncoding ColorEncodingFromDescriptor(const ColorEncodingDescriptor& desc) {
  ColorEncoding c;
  c.SetColorSpace(desc.color_space);
  if (desc.color_space != ColorSpace::kXYB) {
    Check(c.SetWhitePointType(desc.white_point));
    if (desc.color_space != ColorSpace::kGray) {
      Check(c.SetPrimariesType(desc.primaries));
    }
    c.Tf().SetTransferFunction(desc.tf);
  }
  c.SetRenderingIntent(desc.rendering_intent);
  Check(c.CreateICC());
  return c;
}

namespace {
void CheckSameEncodings(const std::vector<ColorEncoding>& a,
                        const std::vector<ColorEncoding>& b,
                        const std::string& check_name,
                        std::stringstream& failures) {
  Check(a.size() == b.size());
  for (size_t i = 0; i < a.size(); ++i) {
    if ((a[i].ICC() == b[i].ICC()) ||
        ((a[i].GetPrimariesType() == b[i].GetPrimariesType()) &&
         a[i].Tf().IsSame(b[i].Tf()))) {
      continue;
    }
    failures << "CheckSameEncodings " << check_name << ": " << i
             << "-th encoding mismatch\n";
  }
}
}  // namespace

bool Roundtrip(CodecInOut* io, const CompressParams& cparams,
               extras::JXLDecompressParams dparams,
               CodecInOut* JXL_RESTRICT io2, std::stringstream& failures,
               size_t* compressed_size, ThreadPool* pool) {
  DefaultAcceptedFormats(dparams);
  if (compressed_size) {
    *compressed_size = static_cast<size_t>(-1);
  }
  std::vector<uint8_t> compressed;

  std::vector<ColorEncoding> original_metadata_encodings;
  std::vector<ColorEncoding> original_current_encodings;
  std::vector<ColorEncoding> metadata_encodings_1;
  std::vector<ColorEncoding> metadata_encodings_2;
  std::vector<ColorEncoding> current_encodings_2;
  original_metadata_encodings.reserve(io->frames.size());
  original_current_encodings.reserve(io->frames.size());
  metadata_encodings_1.reserve(io->frames.size());
  metadata_encodings_2.reserve(io->frames.size());
  current_encodings_2.reserve(io->frames.size());

  for (const ImageBundle& ib : io->frames) {
    // Remember original encoding, will be returned by decoder.
    original_metadata_encodings.push_back(ib.metadata()->color_encoding);
    // c_current should not change during encoding.
    original_current_encodings.push_back(ib.c_current());
  }

  Check(test::EncodeFile(cparams, io, &compressed, pool));

  for (const ImageBundle& ib1 : io->frames) {
    metadata_encodings_1.push_back(ib1.metadata()->color_encoding);
  }

  // Should still be in the same color space after encoding.
  CheckSameEncodings(metadata_encodings_1, original_metadata_encodings,
                     "original vs after encoding", failures);

  Check(DecodeFile(dparams, Bytes(compressed), io2, pool));
  Check(io2->frames.size() == io->frames.size());

  for (const ImageBundle& ib2 : io2->frames) {
    metadata_encodings_2.push_back(ib2.metadata()->color_encoding);
    current_encodings_2.push_back(ib2.c_current());
  }

  // We always produce the original color encoding if a color transform hook is
  // set.
  CheckSameEncodings(current_encodings_2, original_current_encodings,
                     "current: original vs decoded", failures);

  // Decoder returns the originals passed to the encoder.
  CheckSameEncodings(metadata_encodings_2, original_metadata_encodings,
                     "metadata: original vs decoded", failures);

  if (compressed_size) {
    *compressed_size = compressed.size();
  }

  return failures.str().empty();
}

size_t Roundtrip(const extras::PackedPixelFile& ppf_in,
                 const extras::JXLCompressParams& cparams,
                 extras::JXLDecompressParams dparams, ThreadPool* pool,
                 extras::PackedPixelFile* ppf_out) {
  DefaultAcceptedFormats(dparams);
  SetThreadParallelRunner(cparams, pool);
  SetThreadParallelRunner(dparams, pool);
  std::vector<uint8_t> compressed;
  Check(extras::EncodeImageJXL(cparams, ppf_in, /*jpeg_bytes=*/nullptr,
                               &compressed));
  size_t decoded_bytes = 0;
  Check(extras::DecodeImageJXL(compressed.data(), compressed.size(), dparams,
                               &decoded_bytes, ppf_out));
  Check(decoded_bytes == compressed.size());
  return compressed.size();
}

std::vector<ColorEncodingDescriptor> AllEncodings() {
  std::vector<ColorEncodingDescriptor> all_encodings;
  all_encodings.reserve(300);

  for (ColorSpace cs : Values<ColorSpace>()) {
    if (cs == ColorSpace::kUnknown || cs == ColorSpace::kXYB ||
        cs == ColorSpace::kGray) {
      continue;
    }

    for (WhitePoint wp : Values<WhitePoint>()) {
      if (wp == WhitePoint::kCustom) continue;
      for (Primaries primaries : Values<Primaries>()) {
        if (primaries == Primaries::kCustom) continue;
        for (TransferFunction tf : Values<TransferFunction>()) {
          if (tf == TransferFunction::kUnknown) continue;
          for (RenderingIntent ri : Values<RenderingIntent>()) {
            ColorEncodingDescriptor cdesc;
            cdesc.color_space = cs;
            cdesc.white_point = wp;
            cdesc.primaries = primaries;
            cdesc.tf = tf;
            cdesc.rendering_intent = ri;
            all_encodings.push_back(cdesc);
          }
        }
      }
    }
  }

  return all_encodings;
}

jxl::CodecInOut SomeTestImageToCodecInOut(const std::vector<uint8_t>& buf,
                                          size_t num_channels, size_t xsize,
                                          size_t ysize) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  jxl::CodecInOut io{memory_manager};
  Check(io.SetSize(xsize, ysize));
  io.metadata.m.SetAlphaBits(16);
  io.metadata.m.color_encoding = jxl::ColorEncoding::SRGB(
      /*is_gray=*/num_channels == 1 || num_channels == 2);
  JxlPixelFormat format = {static_cast<uint32_t>(num_channels), JXL_TYPE_UINT16,
                           JXL_BIG_ENDIAN, 0};
  Check(ConvertFromExternal(
      jxl::Bytes(buf.data(), buf.size()), xsize, ysize,
      jxl::ColorEncoding::SRGB(/*is_gray=*/num_channels < 3),
      /*bits_per_sample=*/16, format,
      /*pool=*/nullptr,
      /*ib=*/&io.Main()));
  return io;
}

bool Near(double expected, double value, double max_dist) {
  double dist = expected > value ? expected - value : value - expected;
  return dist <= max_dist;
}

float LoadLEFloat16(const uint8_t* p) {
  uint16_t bits16 = LoadLE16(p);
  return detail::LoadFloat16(bits16);
}

float LoadBEFloat16(const uint8_t* p) {
  uint16_t bits16 = LoadBE16(p);
  return detail::LoadFloat16(bits16);
}

size_t GetPrecision(JxlDataType data_type) {
  switch (data_type) {
    case JXL_TYPE_UINT8:
      return 8;
    case JXL_TYPE_UINT16:
      return 16;
    case JXL_TYPE_FLOAT:
      // Floating point mantissa precision
      return 24;
    case JXL_TYPE_FLOAT16:
      return 11;
    default:
      Check(false);
      return 0;
  }
}

size_t GetDataBits(JxlDataType data_type) {
  switch (data_type) {
    case JXL_TYPE_UINT8:
      return 8;
    case JXL_TYPE_UINT16:
      return 16;
    case JXL_TYPE_FLOAT:
      return 32;
    case JXL_TYPE_FLOAT16:
      return 16;
    default:
      Check(false);
      return 0;
  }
}

std::vector<double> ConvertToRGBA32(const uint8_t* pixels, size_t xsize,
                                    size_t ysize, const JxlPixelFormat& format,
                                    double factor) {
  std::vector<double> result(xsize * ysize * 4);
  size_t num_channels = format.num_channels;
  bool gray = num_channels == 1 || num_channels == 2;
  bool alpha = num_channels == 2 || num_channels == 4;
  JxlEndianness endianness = format.endianness;
  // Compute actual type:
  if (endianness == JXL_NATIVE_ENDIAN) {
    endianness = IsLittleEndian() ? JXL_LITTLE_ENDIAN : JXL_BIG_ENDIAN;
  }

  size_t stride =
      xsize * jxl::DivCeil(GetDataBits(format.data_type) * num_channels,
                           jxl::kBitsPerByte);
  if (format.align > 1) stride = jxl::RoundUpTo(stride, format.align);

  if (format.data_type == JXL_TYPE_UINT8) {
    // Multiplier to bring to 0-1.0 range
    double mul = factor > 0.0 ? factor : 1.0 / 255.0;
    for (size_t y = 0; y < ysize; ++y) {
      for (size_t x = 0; x < xsize; ++x) {
        size_t j = (y * xsize + x) * 4;
        size_t i = y * stride + x * num_channels;
        double r = pixels[i];
        double g = gray ? r : pixels[i + 1];
        double b = gray ? r : pixels[i + 2];
        double a = alpha ? pixels[i + num_channels - 1] : 255;
        result[j + 0] = r * mul;
        result[j + 1] = g * mul;
        result[j + 2] = b * mul;
        result[j + 3] = a * mul;
      }
    }
  } else if (format.data_type == JXL_TYPE_UINT16) {
    Check(endianness != JXL_NATIVE_ENDIAN);
    // Multiplier to bring to 0-1.0 range
    double mul = factor > 0.0 ? factor : 1.0 / 65535.0;
    for (size_t y = 0; y < ysize; ++y) {
      for (size_t x = 0; x < xsize; ++x) {
        size_t j = (y * xsize + x) * 4;
        size_t i = y * stride + x * num_channels * 2;
        double r;
        double g;
        double b;
        double a;
        if (endianness == JXL_BIG_ENDIAN) {
          r = (pixels[i + 0] << 8) + pixels[i + 1];
          g = gray ? r : (pixels[i + 2] << 8) + pixels[i + 3];
          b = gray ? r : (pixels[i + 4] << 8) + pixels[i + 5];
          a = alpha ? (pixels[i + num_channels * 2 - 2] << 8) +
                          pixels[i + num_channels * 2 - 1]
                    : 65535;
        } else {
          r = (pixels[i + 1] << 8) + pixels[i + 0];
          g = gray ? r : (pixels[i + 3] << 8) + pixels[i + 2];
          b = gray ? r : (pixels[i + 5] << 8) + pixels[i + 4];
          a = alpha ? (pixels[i + num_channels * 2 - 1] << 8) +
                          pixels[i + num_channels * 2 - 2]
                    : 65535;
        }
        result[j + 0] = r * mul;
        result[j + 1] = g * mul;
        result[j + 2] = b * mul;
        result[j + 3] = a * mul;
      }
    }
  } else if (format.data_type == JXL_TYPE_FLOAT) {
    Check(endianness != JXL_NATIVE_ENDIAN);
    for (size_t y = 0; y < ysize; ++y) {
      for (size_t x = 0; x < xsize; ++x) {
        size_t j = (y * xsize + x) * 4;
        size_t i = y * stride + x * num_channels * 4;
        double r;
        double g;
        double b;
        double a;
        if (endianness == JXL_BIG_ENDIAN) {
          r = LoadBEFloat(pixels + i);
          g = gray ? r : LoadBEFloat(pixels + i + 4);
          b = gray ? r : LoadBEFloat(pixels + i + 8);
          a = alpha ? LoadBEFloat(pixels + i + num_channels * 4 - 4) : 1.0;
        } else {
          r = LoadLEFloat(pixels + i);
          g = gray ? r : LoadLEFloat(pixels + i + 4);
          b = gray ? r : LoadLEFloat(pixels + i + 8);
          a = alpha ? LoadLEFloat(pixels + i + num_channels * 4 - 4) : 1.0;
        }
        result[j + 0] = r;
        result[j + 1] = g;
        result[j + 2] = b;
        result[j + 3] = a;
      }
    }
  } else if (format.data_type == JXL_TYPE_FLOAT16) {
    Check(endianness != JXL_NATIVE_ENDIAN);
    for (size_t y = 0; y < ysize; ++y) {
      for (size_t x = 0; x < xsize; ++x) {
        size_t j = (y * xsize + x) * 4;
        size_t i = y * stride + x * num_channels * 2;
        double r;
        double g;
        double b;
        double a;
        if (endianness == JXL_BIG_ENDIAN) {
          r = LoadBEFloat16(pixels + i);
          g = gray ? r : LoadBEFloat16(pixels + i + 2);
          b = gray ? r : LoadBEFloat16(pixels + i + 4);
          a = alpha ? LoadBEFloat16(pixels + i + num_channels * 2 - 2) : 1.0;
        } else {
          r = LoadLEFloat16(pixels + i);
          g = gray ? r : LoadLEFloat16(pixels + i + 2);
          b = gray ? r : LoadLEFloat16(pixels + i + 4);
          a = alpha ? LoadLEFloat16(pixels + i + num_channels * 2 - 2) : 1.0;
        }
        result[j + 0] = r;
        result[j + 1] = g;
        result[j + 2] = b;
        result[j + 3] = a;
      }
    }
  } else {
    Check(false);  // Unsupported type
  }
  return result;
}

size_t ComparePixels(const uint8_t* a, const uint8_t* b, size_t xsize,
                     size_t ysize, const JxlPixelFormat& format_a,
                     const JxlPixelFormat& format_b,
                     double threshold_multiplier) {
  // Convert both images to equal full precision for comparison.
  std::vector<double> a_full = ConvertToRGBA32(a, xsize, ysize, format_a);
  std::vector<double> b_full = ConvertToRGBA32(b, xsize, ysize, format_b);
  bool gray_a = format_a.num_channels < 3;
  bool gray_b = format_b.num_channels < 3;
  bool alpha_a = ((format_a.num_channels & 1) == 0);
  bool alpha_b = ((format_b.num_channels & 1) == 0);
  size_t bits_a = GetPrecision(format_a.data_type);
  size_t bits_b = GetPrecision(format_b.data_type);
  size_t bits = std::min(bits_a, bits_b);
  // How much distance is allowed in case of pixels with lower bit depths, given
  // that the double precision float images use range 0-1.0.
  // E.g. in case of 1-bit this is 0.5 since 0.499 must map to 0 and 0.501 must
  // map to 1.
  double precision = 0.5 * threshold_multiplier / ((1ull << bits) - 1ull);
  if (format_a.data_type == JXL_TYPE_FLOAT16 ||
      format_b.data_type == JXL_TYPE_FLOAT16) {
    // Lower the precision for float16, because it currently looks like the
    // scalar and wasm implementations of hwy have 1 less bit of precision
    // than the x86 implementations.
    // TODO(lode): Set the required precision back to 11 bits when possible.
    precision = 0.5 * threshold_multiplier / ((1ull << (bits - 1)) - 1ull);
  }
  if (format_b.data_type == JXL_TYPE_UINT8) {
    // Increase the threshold by the maximum difference introduced by dithering.
    precision += 63.0 / 128.0;
  }
  size_t numdiff = 0;
  for (size_t y = 0; y < ysize; y++) {
    for (size_t x = 0; x < xsize; x++) {
      size_t i = (y * xsize + x) * 4;
      bool ok = true;
      if (gray_a || gray_b) {
        if (!Near(a_full[i + 0], b_full[i + 0], precision)) ok = false;
        // If the input was grayscale and the output not, then the output must
        // have all channels equal.
        if (gray_a && b_full[i + 0] != b_full[i + 1] &&
            b_full[i + 2] != b_full[i + 2]) {
          ok = false;
        }
      } else {
        if (!Near(a_full[i + 0], b_full[i + 0], precision) ||
            !Near(a_full[i + 1], b_full[i + 1], precision) ||
            !Near(a_full[i + 2], b_full[i + 2], precision)) {
          ok = false;
        }
      }
      if (alpha_a && alpha_b) {
        if (!Near(a_full[i + 3], b_full[i + 3], precision)) ok = false;
      } else {
        // If the input had no alpha channel, the output should be opaque
        // after roundtrip.
        if (alpha_b && !Near(1.0, b_full[i + 3], precision)) ok = false;
      }
      if (!ok) numdiff++;
    }
  }
  return numdiff;
}

double DistanceRMS(const uint8_t* a, const uint8_t* b, size_t xsize,
                   size_t ysize, const JxlPixelFormat& format) {
  // Convert both images to equal full precision for comparison.
  std::vector<double> a_full = ConvertToRGBA32(a, xsize, ysize, format);
  std::vector<double> b_full = ConvertToRGBA32(b, xsize, ysize, format);
  double sum = 0.0;
  for (size_t y = 0; y < ysize; y++) {
    double row_sum = 0.0;
    for (size_t x = 0; x < xsize; x++) {
      size_t i = (y * xsize + x) * 4;
      for (size_t c = 0; c < format.num_channels; ++c) {
        double diff = a_full[i + c] - b_full[i + c];
        row_sum += diff * diff;
      }
    }
    sum += row_sum;
  }
  sum /= (xsize * ysize);
  return sqrt(sum);
}

float ButteraugliDistance(const extras::PackedPixelFile& a,
                          const extras::PackedPixelFile& b, ThreadPool* pool) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  CodecInOut io0{memory_manager};
  Check(ConvertPackedPixelFileToCodecInOut(a, pool, &io0));
  CodecInOut io1{memory_manager};
  Check(ConvertPackedPixelFileToCodecInOut(b, pool, &io1));
  // TODO(eustas): simplify?
  return ButteraugliDistance(io0.frames, io1.frames, ButteraugliParams(),
                             *JxlGetDefaultCms(),
                             /*distmap=*/nullptr, pool);
}

float ButteraugliDistance(const ImageBundle& rgb0, const ImageBundle& rgb1,
                          const ButteraugliParams& params,
                          const JxlCmsInterface& cms, ImageF* distmap,
                          ThreadPool* pool, bool ignore_alpha) {
  JxlButteraugliComparator comparator(params, cms);
  float distance;
  Check(ComputeScore(rgb0, rgb1, &comparator, cms, &distance, distmap, pool,
                     ignore_alpha));
  return distance;
}

float ButteraugliDistance(const std::vector<ImageBundle>& frames0,
                          const std::vector<ImageBundle>& frames1,
                          const ButteraugliParams& params,
                          const JxlCmsInterface& cms, ImageF* distmap,
                          ThreadPool* pool) {
  JxlButteraugliComparator comparator(params, cms);
  Check(frames0.size() == frames1.size());
  float max_dist = 0.0f;
  for (size_t i = 0; i < frames0.size(); ++i) {
    float frame_score;
    Check(ComputeScore(frames0[i], frames1[i], &comparator, cms, &frame_score,
                       distmap, pool));
    max_dist = std::max(max_dist, frame_score);
  }
  return max_dist;
}

float Butteraugli3Norm(const extras::PackedPixelFile& a,
                       const extras::PackedPixelFile& b, ThreadPool* pool) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  CodecInOut io0{memory_manager};
  Check(ConvertPackedPixelFileToCodecInOut(a, pool, &io0));
  CodecInOut io1{memory_manager};
  Check(ConvertPackedPixelFileToCodecInOut(b, pool, &io1));
  ButteraugliParams butteraugli_params;
  ImageF distmap;
  ButteraugliDistance(io0.frames, io1.frames, butteraugli_params,
                      *JxlGetDefaultCms(), &distmap, pool);
  return ComputeDistanceP(distmap, butteraugli_params, 3);
}

float ComputeDistance2(const extras::PackedPixelFile& a,
                       const extras::PackedPixelFile& b) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  CodecInOut io0{memory_manager};
  Check(ConvertPackedPixelFileToCodecInOut(a, nullptr, &io0));
  CodecInOut io1{memory_manager};
  Check(ConvertPackedPixelFileToCodecInOut(b, nullptr, &io1));
  return ComputeDistance2(io0.Main(), io1.Main(), *JxlGetDefaultCms());
}

float ComputePSNR(const extras::PackedPixelFile& a,
                  const extras::PackedPixelFile& b) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  CodecInOut io0{memory_manager};
  Check(ConvertPackedPixelFileToCodecInOut(a, nullptr, &io0));
  CodecInOut io1{memory_manager};
  Check(ConvertPackedPixelFileToCodecInOut(b, nullptr, &io1));
  return ComputePSNR(io0.Main(), io1.Main(), *JxlGetDefaultCms());
}

bool SameAlpha(const extras::PackedPixelFile& a,
               const extras::PackedPixelFile& b) {
  Check(a.info.xsize == b.info.xsize);
  Check(a.info.ysize == b.info.ysize);
  Check(a.info.alpha_bits == b.info.alpha_bits);
  Check(a.info.alpha_exponent_bits == b.info.alpha_exponent_bits);
  Check(a.info.alpha_bits > 0);
  Check(a.frames.size() == b.frames.size());
  for (size_t i = 0; i < a.frames.size(); ++i) {
    const extras::PackedImage& color_a = a.frames[i].color;
    const extras::PackedImage& color_b = b.frames[i].color;
    Check(color_a.format.num_channels == color_b.format.num_channels);
    Check(color_a.format.data_type == color_b.format.data_type);
    Check(color_a.format.endianness == color_b.format.endianness);
    Check(color_a.pixels_size == color_b.pixels_size);
    size_t pwidth =
        extras::PackedImage::BitsPerChannel(color_a.format.data_type) / 8;
    size_t num_color = color_a.format.num_channels < 3 ? 1 : 3;
    const uint8_t* p_a = reinterpret_cast<const uint8_t*>(color_a.pixels());
    const uint8_t* p_b = reinterpret_cast<const uint8_t*>(color_b.pixels());
    for (size_t y = 0; y < a.info.ysize; ++y) {
      for (size_t x = 0; x < a.info.xsize; ++x) {
        size_t idx =
            ((y * a.info.xsize + x) * color_a.format.num_channels + num_color) *
            pwidth;
        if (memcmp(&p_a[idx], &p_b[idx], pwidth) != 0) {
          return false;
        }
      }
    }
  }
  return true;
}

bool SamePixels(const extras::PackedImage& a, const extras::PackedImage& b) {
  Check(a.xsize == b.xsize);
  Check(a.ysize == b.ysize);
  Check(a.format.num_channels == b.format.num_channels);
  Check(a.format.data_type == b.format.data_type);
  Check(a.format.endianness == b.format.endianness);
  Check(a.pixels_size == b.pixels_size);
  const uint8_t* p_a = reinterpret_cast<const uint8_t*>(a.pixels());
  const uint8_t* p_b = reinterpret_cast<const uint8_t*>(b.pixels());
  for (size_t y = 0; y < a.ysize; ++y) {
    for (size_t x = 0; x < a.xsize; ++x) {
      size_t idx = (y * a.xsize + x) * a.pixel_stride();
      if (memcmp(&p_a[idx], &p_b[idx], a.pixel_stride()) != 0) {
        printf("Mismatch at row %" PRIuS " col %" PRIuS "\n", y, x);
        printf("  a: ");
        for (size_t j = 0; j < a.pixel_stride(); ++j) {
          printf(" %3u", p_a[idx + j]);
        }
        printf("\n  b: ");
        for (size_t j = 0; j < a.pixel_stride(); ++j) {
          printf(" %3u", p_b[idx + j]);
        }
        printf("\n");
        return false;
      }
    }
  }
  return true;
}

bool SamePixels(const extras::PackedPixelFile& a,
                const extras::PackedPixelFile& b) {
  Check(a.info.xsize == b.info.xsize);
  Check(a.info.ysize == b.info.ysize);
  Check(a.info.bits_per_sample == b.info.bits_per_sample);
  Check(a.info.exponent_bits_per_sample == b.info.exponent_bits_per_sample);
  Check(a.frames.size() == b.frames.size());
  for (size_t i = 0; i < a.frames.size(); ++i) {
    const auto& frame_a = a.frames[i];
    const auto& frame_b = b.frames[i];
    if (!SamePixels(frame_a.color, frame_b.color)) {
      return false;
    }
    Check(frame_a.extra_channels.size() == frame_b.extra_channels.size());
    for (size_t j = 0; j < frame_a.extra_channels.size(); ++j) {
      if (!SamePixels(frame_a.extra_channels[i], frame_b.extra_channels[i])) {
        return false;
      }
    }
  }
  return true;
}

Status ReadICC(BitReader* JXL_RESTRICT reader,
               std::vector<uint8_t>* JXL_RESTRICT icc) {
  JxlMemoryManager* memort_manager = jxl::test::MemoryManager();
  icc->clear();
  ICCReader icc_reader{memort_manager};
  PaddedBytes icc_buffer{memort_manager};
  JXL_RETURN_IF_ERROR(icc_reader.Init(reader));
  JXL_RETURN_IF_ERROR(icc_reader.Process(reader, &icc_buffer));
  Bytes(icc_buffer).AppendTo(*icc);
  return true;
}

namespace {  // For EncodeFile
Status PrepareCodecMetadataFromIO(const CompressParams& cparams,
                                  const CodecInOut* io,
                                  CodecMetadata* metadata) {
  *metadata = io->metadata;
  size_t ups = 1;
  if (cparams.already_downsampled) ups = cparams.resampling;

  JXL_RETURN_IF_ERROR(metadata->size.Set(io->xsize() * ups, io->ysize() * ups));

  // Keep ICC profile in lossless modes because a reconstructed profile may be
  // slightly different (quantization).
  // Also keep ICC in JPEG reconstruction mode as we need byte-exact profiles.
  if (!cparams.IsLossless() && !io->Main().IsJPEG() && cparams.cms_set) {
    metadata->m.color_encoding.DecideIfWantICC(cparams.cms);
  }

  metadata->m.xyb_encoded =
      cparams.color_transform == ColorTransform::kXYB ? true : false;

  // TODO(firsching): move this EncodeFile to test_utils / re-implement this
  // using API functions
  return true;
}

Status EncodePreview(const CompressParams& cparams, ImageBundle& ib,
                     const CodecMetadata* metadata, const JxlCmsInterface& cms,
                     ThreadPool* pool, BitWriter* JXL_RESTRICT writer) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  BitWriter preview_writer{memory_manager};
  // TODO(janwas): also support generating preview by downsampling
  if (ib.HasColor()) {
    AuxOut aux_out;
    // TODO(lode): check if we want all extra channels and matching xyb_encoded
    // for the preview, such that using the main ImageMetadata object for
    // encoding this frame is warrented.
    FrameInfo frame_info;
    frame_info.is_preview = true;
    JXL_RETURN_IF_ERROR(EncodeFrame(memory_manager, cparams, frame_info,
                                    metadata, ib, cms, pool, &preview_writer,
                                    &aux_out));
    preview_writer.ZeroPadToByte();
  }

  if (preview_writer.BitsWritten() != 0) {
    writer->ZeroPadToByte();
    JXL_RETURN_IF_ERROR(writer->AppendByteAligned(preview_writer.GetSpan()));
  }

  return true;
}

}  // namespace

Status EncodeFile(const CompressParams& params, CodecInOut* io,
                  std::vector<uint8_t>* compressed, ThreadPool* pool) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  compressed->clear();
  const JxlCmsInterface& cms = *JxlGetDefaultCms();
  JXL_RETURN_IF_ERROR(io->CheckMetadata());
  BitWriter writer{memory_manager};

  CompressParams cparams = params;
  if (io->Main().color_transform != ColorTransform::kNone) {
    // Set the color transform to YCbCr or XYB if the original image is such.
    cparams.color_transform = io->Main().color_transform;
  }

  JXL_RETURN_IF_ERROR(ParamsPostInit(&cparams));

  std::unique_ptr<CodecMetadata> metadata = jxl::make_unique<CodecMetadata>();
  JXL_RETURN_IF_ERROR(PrepareCodecMetadataFromIO(cparams, io, metadata.get()));
  JXL_RETURN_IF_ERROR(
      WriteCodestreamHeaders(metadata.get(), &writer, /*aux_out*/ nullptr));

  // Only send ICC (at least several hundred bytes) if fields aren't enough.
  if (metadata->m.color_encoding.WantICC()) {
    JXL_RETURN_IF_ERROR(
        WriteICC(Span<const uint8_t>(metadata->m.color_encoding.ICC()), &writer,
                 LayerType::Header, /* aux_out */ nullptr));
  }

  if (metadata->m.have_preview) {
    JXL_RETURN_IF_ERROR(EncodePreview(cparams, io->preview_frame,
                                      metadata.get(), cms, pool, &writer));
  }

  // Each frame should start on byte boundaries.
  JXL_RETURN_IF_ERROR(
      writer.WithMaxBits(8, LayerType::Header, /*aux_out=*/nullptr, [&] {
        writer.ZeroPadToByte();
        return true;
      }));

  for (size_t i = 0; i < io->frames.size(); i++) {
    FrameInfo info;
    info.is_last = i == io->frames.size() - 1;
    if (io->frames[i].use_for_next_frame) {
      info.save_as_reference = 1;
    }
    JXL_RETURN_IF_ERROR(EncodeFrame(memory_manager, cparams, info,
                                    metadata.get(), io->frames[i], cms, pool,
                                    &writer,
                                    /* aux_out */ nullptr));
  }

  PaddedBytes output = std::move(writer).TakeBytes();
  Bytes(output).AppendTo(*compressed);
  return true;
}

}  // namespace test

bool operator==(const jxl::Bytes& a, const jxl::Bytes& b) {
  if (a.size() != b.size()) return false;
  if (memcmp(a.data(), b.data(), a.size()) != 0) return false;
  return true;
}

// Allow using EXPECT_EQ on jxl::Bytes
bool operator!=(const jxl::Bytes& a, const jxl::Bytes& b) { return !(a == b); }

}  // namespace jxl
