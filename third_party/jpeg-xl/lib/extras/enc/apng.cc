// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/enc/apng.h"

// Parts of this code are taken from apngdis, which has the following license:
/* APNG Disassembler 2.8
 *
 * Deconstructs APNG files into individual frames.
 *
 * http://apngdis.sourceforge.net
 *
 * Copyright (c) 2010-2015 Max Stepin
 * maxst at users.sourceforge.net
 *
 * zlib license
 * ------------
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 */

#include <cstring>
#include <string>
#include <vector>

#include "lib/extras/exif.h"
#include "lib/jxl/base/byte_order.h"
#include "lib/jxl/base/printf_macros.h"
#if JPEGXL_ENABLE_APNG
#include "png.h" /* original (unpatched) libpng is ok */
#endif

namespace jxl {
namespace extras {

#if JPEGXL_ENABLE_APNG
namespace {

constexpr unsigned char kExifSignature[6] = {0x45, 0x78, 0x69,
                                             0x66, 0x00, 0x00};

class APNGEncoder : public Encoder {
 public:
  std::vector<JxlPixelFormat> AcceptedFormats() const override {
    std::vector<JxlPixelFormat> formats;
    for (const uint32_t num_channels : {1, 2, 3, 4}) {
      for (const JxlDataType data_type :
           {JXL_TYPE_UINT8, JXL_TYPE_UINT16, JXL_TYPE_FLOAT}) {
        for (JxlEndianness endianness : {JXL_BIG_ENDIAN, JXL_LITTLE_ENDIAN}) {
          formats.push_back(
              JxlPixelFormat{num_channels, data_type, endianness, /*align=*/0});
        }
      }
    }
    return formats;
  }
  Status Encode(const PackedPixelFile& ppf, EncodedImage* encoded_image,
                ThreadPool* pool) const override {
    // Encode main image frames
    JXL_RETURN_IF_ERROR(VerifyBasicInfo(ppf.info));
    encoded_image->icc.clear();
    encoded_image->bitstreams.resize(1);
    JXL_RETURN_IF_ERROR(EncodePackedPixelFileToAPNG(
        ppf, pool, &encoded_image->bitstreams.front()));

    // Encode extra channels
    for (size_t i = 0; i < ppf.extra_channels_info.size(); ++i) {
      encoded_image->extra_channel_bitstreams.emplace_back();
      auto& ec_bitstreams = encoded_image->extra_channel_bitstreams.back();
      ec_bitstreams.emplace_back();
      JXL_RETURN_IF_ERROR(EncodePackedPixelFileToAPNG(
          ppf, pool, &ec_bitstreams.back(), true, i));
    }
    return true;
  }

 private:
  Status EncodePackedPixelFileToAPNG(const PackedPixelFile& ppf,
                                     ThreadPool* pool,
                                     std::vector<uint8_t>* bytes,
                                     bool encode_extra_channels = false,
                                     size_t extra_channel_index = 0) const;
};

void PngWrite(png_structp png_ptr, png_bytep data, png_size_t length) {
  std::vector<uint8_t>* bytes =
      static_cast<std::vector<uint8_t>*>(png_get_io_ptr(png_ptr));
  bytes->insert(bytes->end(), data, data + length);
}

// Stores XMP and EXIF/IPTC into key/value strings for PNG
class BlobsWriterPNG {
 public:
  static Status Encode(const PackedMetadata& blobs,
                       std::vector<std::string>* strings) {
    if (!blobs.exif.empty()) {
      // PNG viewers typically ignore Exif orientation but not all of them do
      // (and e.g. cjxl doesn't), so we overwrite the Exif orientation to the
      // identity to avoid repeated orientation.
      std::vector<uint8_t> exif = blobs.exif;
      ResetExifOrientation(exif);
      // By convention, the data is prefixed with "Exif\0\0" when stored in
      // the legacy (and non-standard) "Raw profile type exif" text chunk
      // currently used here.
      // TODO(user): Store Exif data in an eXIf chunk instead, which always
      //             begins with the TIFF header.
      if (exif.size() >= sizeof kExifSignature &&
          memcmp(exif.data(), kExifSignature, sizeof kExifSignature) != 0) {
        exif.insert(exif.begin(), kExifSignature,
                    kExifSignature + sizeof kExifSignature);
      }
      JXL_RETURN_IF_ERROR(EncodeBase16("exif", exif, strings));
    }
    if (!blobs.iptc.empty()) {
      JXL_RETURN_IF_ERROR(EncodeBase16("iptc", blobs.iptc, strings));
    }
    if (!blobs.xmp.empty()) {
      // TODO(user): Store XMP data in an "XML:com.adobe.xmp" text chunk
      //             instead.
      JXL_RETURN_IF_ERROR(EncodeBase16("xmp", blobs.xmp, strings));
    }
    return true;
  }

 private:
  // TODO(eustas): use array
  static JXL_INLINE char EncodeNibble(const uint8_t nibble) {
    if (nibble < 16) {
      return (nibble < 10) ? '0' + nibble : 'a' + nibble - 10;
    } else {
      JXL_DEBUG_ABORT("Internal logic error");
      return 0;
    }
  }

  static Status EncodeBase16(const std::string& type,
                             const std::vector<uint8_t>& bytes,
                             std::vector<std::string>* strings) {
    // Encoding: base16 with newline after 72 chars.
    const size_t base16_size =
        2 * bytes.size() + DivCeil(bytes.size(), static_cast<size_t>(36)) + 1;
    std::string base16;
    base16.reserve(base16_size);
    for (size_t i = 0; i < bytes.size(); ++i) {
      if (i % 36 == 0) base16.push_back('\n');
      base16.push_back(EncodeNibble(bytes[i] >> 4));
      base16.push_back(EncodeNibble(bytes[i] & 0x0F));
    }
    base16.push_back('\n');
    JXL_ENSURE(base16.length() == base16_size);

    char key[30];
    snprintf(key, sizeof(key), "Raw profile type %s", type.c_str());

    char header[30];
    snprintf(header, sizeof(header), "\n%s\n%8" PRIuS, type.c_str(),
             bytes.size());

    strings->emplace_back(key);
    strings->push_back(std::string(header) + base16);
    return true;
  }
};

void MaybeAddCICP(const JxlColorEncoding& c_enc, png_structp png_ptr,
                  png_infop info_ptr) {
  png_byte cicp_data[4] = {};
  png_unknown_chunk cicp_chunk;
  if (c_enc.color_space != JXL_COLOR_SPACE_RGB) {
    return;
  }
  if (c_enc.primaries == JXL_PRIMARIES_P3) {
    if (c_enc.white_point == JXL_WHITE_POINT_D65) {
      cicp_data[0] = 12;
    } else if (c_enc.white_point == JXL_WHITE_POINT_DCI) {
      cicp_data[0] = 11;
    } else {
      return;
    }
  } else if (c_enc.primaries != JXL_PRIMARIES_CUSTOM &&
             c_enc.white_point == JXL_WHITE_POINT_D65) {
    cicp_data[0] = static_cast<png_byte>(c_enc.primaries);
  } else {
    return;
  }
  if (c_enc.transfer_function == JXL_TRANSFER_FUNCTION_UNKNOWN ||
      c_enc.transfer_function == JXL_TRANSFER_FUNCTION_GAMMA) {
    return;
  }
  cicp_data[1] = static_cast<png_byte>(c_enc.transfer_function);
  cicp_data[2] = 0;
  cicp_data[3] = 1;
  cicp_chunk.data = cicp_data;
  cicp_chunk.size = sizeof(cicp_data);
  cicp_chunk.location = PNG_HAVE_IHDR;
  memcpy(cicp_chunk.name, "cICP", 5);
  png_set_keep_unknown_chunks(png_ptr, PNG_HANDLE_CHUNK_ALWAYS,
                              reinterpret_cast<const png_byte*>("cICP"), 1);
  png_set_unknown_chunks(png_ptr, info_ptr, &cicp_chunk, 1);
}

bool MaybeAddSRGB(const JxlColorEncoding& c_enc, png_structp png_ptr,
                  png_infop info_ptr) {
  if (c_enc.transfer_function == JXL_TRANSFER_FUNCTION_SRGB &&
      (c_enc.color_space == JXL_COLOR_SPACE_GRAY ||
       (c_enc.color_space == JXL_COLOR_SPACE_RGB &&
        c_enc.primaries == JXL_PRIMARIES_SRGB &&
        c_enc.white_point == JXL_WHITE_POINT_D65))) {
    png_set_sRGB(png_ptr, info_ptr, c_enc.rendering_intent);
    png_set_cHRM_fixed(png_ptr, info_ptr, 31270, 32900, 64000, 33000, 30000,
                       60000, 15000, 6000);
    png_set_gAMA_fixed(png_ptr, info_ptr, 45455);
    return true;
  }
  return false;
}

void MaybeAddCHRM(const JxlColorEncoding& c_enc, png_structp png_ptr,
                  png_infop info_ptr) {
  if (c_enc.color_space != JXL_COLOR_SPACE_RGB) return;
  if (c_enc.primaries == 0) return;
  png_set_cHRM(png_ptr, info_ptr, c_enc.white_point_xy[0],
               c_enc.white_point_xy[1], c_enc.primaries_red_xy[0],
               c_enc.primaries_red_xy[1], c_enc.primaries_green_xy[0],
               c_enc.primaries_green_xy[1], c_enc.primaries_blue_xy[0],
               c_enc.primaries_blue_xy[1]);
}

void MaybeAddGAMA(const JxlColorEncoding& c_enc, png_structp png_ptr,
                  png_infop info_ptr) {
  switch (c_enc.transfer_function) {
    case JXL_TRANSFER_FUNCTION_LINEAR:
      png_set_gAMA_fixed(png_ptr, info_ptr, PNG_FP_1);
      break;
    case JXL_TRANSFER_FUNCTION_SRGB:
      png_set_gAMA_fixed(png_ptr, info_ptr, 45455);
      break;
    case JXL_TRANSFER_FUNCTION_GAMMA:
      png_set_gAMA(png_ptr, info_ptr, c_enc.gamma);
      break;

    default:;
      // No gAMA chunk.
  }
}

void MaybeAddCLLi(const JxlColorEncoding& c_enc, const float intensity_target,
                  png_structp png_ptr, png_infop info_ptr) {
  if (c_enc.transfer_function != JXL_TRANSFER_FUNCTION_PQ) return;

  const uint32_t max_content_light_level =
      static_cast<uint32_t>(10000.f * Clamp1(intensity_target, 0.f, 10000.f));
  png_byte chunk_data[8] = {};
  png_save_uint_32(chunk_data, max_content_light_level);
  // Leave MaxFALL set to 0.
  png_unknown_chunk chunk;
  memcpy(chunk.name, "cLLi", 5);
  chunk.data = chunk_data;
  chunk.size = sizeof chunk_data;
  chunk.location = PNG_HAVE_IHDR;
  png_set_keep_unknown_chunks(png_ptr, PNG_HANDLE_CHUNK_ALWAYS,
                              reinterpret_cast<const png_byte*>("cLLi"), 1);
  png_set_unknown_chunks(png_ptr, info_ptr, &chunk, 1);
}

Status APNGEncoder::EncodePackedPixelFileToAPNG(
    const PackedPixelFile& ppf, ThreadPool* pool, std::vector<uint8_t>* bytes,
    bool encode_extra_channels, size_t extra_channel_index) const {
  JxlExtraChannelInfo ec_info{};
  if (encode_extra_channels) {
    if (ppf.extra_channels_info.size() <= extra_channel_index) {
      return JXL_FAILURE("Invalid index for extra channel");
    }
    ec_info = ppf.extra_channels_info[extra_channel_index].ec_info;
  }

  bool has_alpha = !encode_extra_channels && (ppf.info.alpha_bits != 0);
  bool is_gray = encode_extra_channels || (ppf.info.num_color_channels == 1);
  size_t color_channels =
      encode_extra_channels ? 1 : ppf.info.num_color_channels;
  size_t num_channels = color_channels + (has_alpha ? 1 : 0);

  if (!ppf.info.have_animation && ppf.frames.size() != 1) {
    return JXL_FAILURE("Invalid number of frames");
  }

  size_t count = 0;
  size_t anim_chunks = 0;

  for (const auto& frame : ppf.frames) {
    const PackedImage& color = encode_extra_channels
                                   ? frame.extra_channels[extra_channel_index]
                                   : frame.color;

    size_t xsize = color.xsize;
    size_t ysize = color.ysize;
    size_t num_samples = num_channels * xsize * ysize;

    uint32_t bits_per_sample = encode_extra_channels ? ec_info.bits_per_sample
                                                     : ppf.info.bits_per_sample;
    if (!encode_extra_channels) {
      JXL_RETURN_IF_ERROR(VerifyPackedImage(color, ppf.info));
    } else {
      JXL_RETURN_IF_ERROR(VerifyFormat(color.format));
      JXL_RETURN_IF_ERROR(VerifyBitDepth(color.format.data_type,
                                         bits_per_sample,
                                         ec_info.exponent_bits_per_sample));
    }
    const JxlPixelFormat format = color.format;
    const uint8_t* in = reinterpret_cast<const uint8_t*>(color.pixels());
    JXL_RETURN_IF_ERROR(PackedImage::ValidateDataType(format.data_type));
    size_t data_bits_per_sample = PackedImage::BitsPerChannel(format.data_type);
    size_t bytes_per_sample = data_bits_per_sample / 8;
    size_t out_bytes_per_sample = bytes_per_sample > 1 ? 2 : 1;
    size_t out_stride = xsize * num_channels * out_bytes_per_sample;
    size_t out_size = ysize * out_stride;
    std::vector<uint8_t> out(out_size);

    if (format.data_type == JXL_TYPE_UINT8) {
      if (bits_per_sample < 8) {
        float mul = 255.0 / ((1u << bits_per_sample) - 1);
        for (size_t i = 0; i < num_samples; ++i) {
          out[i] = static_cast<uint8_t>(std::lroundf(in[i] * mul));
        }
      } else {
        memcpy(out.data(), in, out_size);
      }
    } else if (format.data_type == JXL_TYPE_UINT16) {
      if (bits_per_sample < 16 || format.endianness != JXL_BIG_ENDIAN) {
        float mul = 65535.0 / ((1u << bits_per_sample) - 1);
        const uint8_t* p_in = in;
        uint8_t* p_out = out.data();
        for (size_t i = 0; i < num_samples; ++i, p_in += 2, p_out += 2) {
          uint32_t val = (format.endianness == JXL_BIG_ENDIAN ? LoadBE16(p_in)
                                                              : LoadLE16(p_in));
          StoreBE16(static_cast<uint32_t>(std::lroundf(val * mul)), p_out);
        }
      } else {
        memcpy(out.data(), in, out_size);
      }
    } else if (format.data_type == JXL_TYPE_FLOAT) {
      constexpr float kMul = 65535.0;
      const uint8_t* p_in = in;
      uint8_t* p_out = out.data();
      for (size_t i = 0; i < num_samples;
           ++i, p_in += sizeof(float), p_out += 2) {
        float val =
            Clamp1(format.endianness == JXL_BIG_ENDIAN ? LoadBEFloat(p_in)
                   : format.endianness == JXL_LITTLE_ENDIAN
                       ? LoadLEFloat(p_in)
                       : *reinterpret_cast<const float*>(p_in),
                   0.f, 1.f);
        StoreBE16(static_cast<uint32_t>(std::lroundf(val * kMul)), p_out);
      }
    }
    png_structp png_ptr;
    png_infop info_ptr;

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr,
                                      nullptr);

    if (!png_ptr) return JXL_FAILURE("Could not init png encoder");

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) return JXL_FAILURE("Could not init png info struct");

    png_set_write_fn(png_ptr, bytes, PngWrite, nullptr);
    png_set_flush(png_ptr, 0);

    int width = xsize;
    int height = ysize;

    png_byte color_type = (is_gray ? PNG_COLOR_TYPE_GRAY : PNG_COLOR_TYPE_RGB);
    if (has_alpha) color_type |= PNG_COLOR_MASK_ALPHA;
    png_byte bit_depth = out_bytes_per_sample * 8;

    png_set_IHDR(png_ptr, info_ptr, width, height, bit_depth, color_type,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
                 PNG_FILTER_TYPE_BASE);
    if (count == 0 && !encode_extra_channels) {
      if (!MaybeAddSRGB(ppf.color_encoding, png_ptr, info_ptr)) {
        MaybeAddCICP(ppf.color_encoding, png_ptr, info_ptr);
        if (!ppf.icc.empty()) {
          png_set_benign_errors(png_ptr, 1);
          png_set_iCCP(png_ptr, info_ptr, "1", 0, ppf.icc.data(),
                       ppf.icc.size());
        }
        MaybeAddCHRM(ppf.color_encoding, png_ptr, info_ptr);
        MaybeAddGAMA(ppf.color_encoding, png_ptr, info_ptr);
      }
      MaybeAddCLLi(ppf.color_encoding, ppf.info.intensity_target, png_ptr,
                   info_ptr);

      std::vector<std::string> textstrings;
      JXL_RETURN_IF_ERROR(BlobsWriterPNG::Encode(ppf.metadata, &textstrings));
      for (size_t kk = 0; kk + 1 < textstrings.size(); kk += 2) {
        png_text text;
        text.key = const_cast<png_charp>(textstrings[kk].c_str());
        text.text = const_cast<png_charp>(textstrings[kk + 1].c_str());
        text.compression = PNG_TEXT_COMPRESSION_zTXt;
        png_set_text(png_ptr, info_ptr, &text, 1);
      }

      png_write_info(png_ptr, info_ptr);
    } else {
      // fake writing a header, otherwise libpng gets confused
      size_t pos = bytes->size();
      png_write_info(png_ptr, info_ptr);
      bytes->resize(pos);
    }

    if (ppf.info.have_animation) {
      if (count == 0) {
        png_byte adata[8];
        png_save_uint_32(adata, ppf.frames.size());
        png_save_uint_32(adata + 4, ppf.info.animation.num_loops);
        png_byte actl[5] = "acTL";
        png_write_chunk(png_ptr, actl, adata, 8);
      }
      png_byte fdata[26];
      // TODO(jon): also make this work for the non-coalesced case
      png_save_uint_32(fdata, anim_chunks++);
      png_save_uint_32(fdata + 4, width);
      png_save_uint_32(fdata + 8, height);
      png_save_uint_32(fdata + 12, 0);
      png_save_uint_32(fdata + 16, 0);
      png_save_uint_16(fdata + 20, frame.frame_info.duration *
                                       ppf.info.animation.tps_denominator);
      png_save_uint_16(fdata + 22, ppf.info.animation.tps_numerator);
      fdata[24] = 1;
      fdata[25] = 0;
      png_byte fctl[5] = "fcTL";
      png_write_chunk(png_ptr, fctl, fdata, 26);
    }

    std::vector<uint8_t*> rows(height);
    for (int y = 0; y < height; ++y) {
      rows[y] = out.data() + y * out_stride;
    }

    png_write_flush(png_ptr);
    const size_t pos = bytes->size();
    png_write_image(png_ptr, rows.data());
    png_write_flush(png_ptr);
    if (count > 0) {
      std::vector<uint8_t> fdata(4);
      png_save_uint_32(fdata.data(), anim_chunks++);
      size_t p = pos;
      while (p + 8 < bytes->size()) {
        size_t len = png_get_uint_32(bytes->data() + p);
        JXL_ENSURE(bytes->operator[](p + 4) == 'I');
        JXL_ENSURE(bytes->operator[](p + 5) == 'D');
        JXL_ENSURE(bytes->operator[](p + 6) == 'A');
        JXL_ENSURE(bytes->operator[](p + 7) == 'T');
        fdata.insert(fdata.end(), bytes->data() + p + 8,
                     bytes->data() + p + 8 + len);
        p += len + 12;
      }
      bytes->resize(pos);

      png_byte fdat[5] = "fdAT";
      png_write_chunk(png_ptr, fdat, fdata.data(), fdata.size());
    }

    count++;
    if (count == ppf.frames.size() || !ppf.info.have_animation) {
      png_write_end(png_ptr, nullptr);
    }

    png_destroy_write_struct(&png_ptr, &info_ptr);
  }

  return true;
}

}  // namespace
#endif

std::unique_ptr<Encoder> GetAPNGEncoder() {
#if JPEGXL_ENABLE_APNG
  return jxl::make_unique<APNGEncoder>();
#else
  return nullptr;
#endif
}

}  // namespace extras
}  // namespace jxl
