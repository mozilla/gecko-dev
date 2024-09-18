// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/dec/jxl.h"

#include <jxl/cms.h>
#include <jxl/codestream_header.h>
#include <jxl/decode.h>
#include <jxl/decode_cxx.h>
#include <jxl/types.h>

#include <cinttypes>  // PRIu32
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <vector>

#include "lib/extras/common.h"
#include "lib/extras/dec/color_description.h"
#include "lib/jxl/base/common.h"
#include "lib/jxl/base/exif.h"
#include "lib/jxl/base/printf_macros.h"
#include "lib/jxl/base/status.h"

namespace jxl {
namespace extras {
namespace {

#define QUIT(M)               \
  fprintf(stderr, "%s\n", M); \
  return false;

struct BoxProcessor {
  explicit BoxProcessor(JxlDecoder* dec) : dec_(dec) { Reset(); }

  bool InitializeOutput(std::vector<uint8_t>* out) {
    if (out == nullptr) {
      fprintf(stderr, "internal: out == nullptr\n");
      return false;
    }
    box_data_ = out;
    return AddMoreOutput();
  }

  bool AddMoreOutput() {
    if (box_data_ == nullptr) {
      fprintf(stderr, "internal: box_data_ == nullptr\n");
      return false;
    }
    Flush();
    static const size_t kBoxOutputChunkSize = 1 << 16;
    box_data_->resize(box_data_->size() + kBoxOutputChunkSize);
    next_out_ = box_data_->data() + total_size_;
    avail_out_ = box_data_->size() - total_size_;
    if (JXL_DEC_SUCCESS !=
        JxlDecoderSetBoxBuffer(dec_, next_out_, avail_out_)) {
      fprintf(stderr, "JxlDecoderSetBoxBuffer failed\n");
      return false;
    }
    return true;
  }

  void FinalizeOutput() {
    if (box_data_ == nullptr) return;
    Flush();
    box_data_->resize(total_size_);
    Reset();
  }

 private:
  JxlDecoder* dec_;
  std::vector<uint8_t>* box_data_;
  uint8_t* next_out_;
  size_t avail_out_;
  size_t total_size_;

  void Reset() {
    box_data_ = nullptr;
    next_out_ = nullptr;
    avail_out_ = 0;
    total_size_ = 0;
  }
  void Flush() {
    if (box_data_ == nullptr) return;
    size_t remaining = JxlDecoderReleaseBoxBuffer(dec_);
    size_t bytes_written = avail_out_ - remaining;
    next_out_ += bytes_written;
    avail_out_ -= bytes_written;
    total_size_ += bytes_written;
  }
};

void SetBitDepthFromDataType(JxlDataType data_type, uint32_t* bits_per_sample,
                             uint32_t* exponent_bits_per_sample) {
  switch (data_type) {
    case JXL_TYPE_UINT8:
      *bits_per_sample = 8;
      *exponent_bits_per_sample = 0;
      break;
    case JXL_TYPE_UINT16:
      *bits_per_sample = 16;
      *exponent_bits_per_sample = 0;
      break;
    case JXL_TYPE_FLOAT16:
      *bits_per_sample = 16;
      *exponent_bits_per_sample = 5;
      break;
    case JXL_TYPE_FLOAT:
      *bits_per_sample = 32;
      *exponent_bits_per_sample = 8;
      break;
  }
}

template <typename T>
void UpdateBitDepth(JxlBitDepth bit_depth, JxlDataType data_type, T* info) {
  if (bit_depth.type == JXL_BIT_DEPTH_FROM_PIXEL_FORMAT) {
    SetBitDepthFromDataType(data_type, &info->bits_per_sample,
                            &info->exponent_bits_per_sample);
  } else if (bit_depth.type == JXL_BIT_DEPTH_CUSTOM) {
    info->bits_per_sample = bit_depth.bits_per_sample;
    info->exponent_bits_per_sample = bit_depth.exponent_bits_per_sample;
  }
}

}  // namespace

bool DecodeImageJXL(const uint8_t* bytes, size_t bytes_size,
                    const JXLDecompressParams& dparams, size_t* decoded_bytes,
                    PackedPixelFile* ppf, std::vector<uint8_t>* jpeg_bytes) {
  JxlSignature sig = JxlSignatureCheck(bytes, bytes_size);
  // silently return false if this is not a JXL file
  if (sig == JXL_SIG_INVALID) return false;

  auto decoder = JxlDecoderMake(dparams.memory_manager);
  JxlDecoder* dec = decoder.get();
  ppf->frames.clear();

  if (dparams.runner_opaque != nullptr &&
      JXL_DEC_SUCCESS != JxlDecoderSetParallelRunner(dec, dparams.runner,
                                                     dparams.runner_opaque)) {
    fprintf(stderr, "JxlEncoderSetParallelRunner failed\n");
    return false;
  }

  JxlPixelFormat format = {};  // Initialize to calm down clang-tidy.
  std::vector<JxlPixelFormat> accepted_formats = dparams.accepted_formats;

  JxlColorEncoding color_encoding;
  size_t num_color_channels = 0;
  if (!dparams.color_space.empty()) {
    if (!jxl::ParseDescription(dparams.color_space, &color_encoding)) {
      fprintf(stderr, "Failed to parse color space %s.\n",
              dparams.color_space.c_str());
      return false;
    }
    num_color_channels =
        color_encoding.color_space == JXL_COLOR_SPACE_GRAY ? 1 : 3;
  }

  bool can_reconstruct_jpeg = false;
  std::vector<uint8_t> jpeg_data_chunk;
  if (jpeg_bytes != nullptr) {
    // This bound is very likely to be enough to hold the entire
    // reconstructed JPEG, to avoid having to do expensive retries.
    jpeg_data_chunk.resize(bytes_size * 3 / 2 + 1024);
    jpeg_bytes->resize(0);
  }

  int events = (JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE);

  bool max_passes_defined =
      (dparams.max_passes < std::numeric_limits<uint32_t>::max());
  if (max_passes_defined || dparams.max_downsampling > 1) {
    events |= JXL_DEC_FRAME_PROGRESSION;
    if (max_passes_defined) {
      JxlDecoderSetProgressiveDetail(dec, JxlProgressiveDetail::kPasses);
    } else {
      JxlDecoderSetProgressiveDetail(dec, JxlProgressiveDetail::kLastPasses);
    }
  }
  if (jpeg_bytes != nullptr) {
    events |= JXL_DEC_JPEG_RECONSTRUCTION;
  } else {
    events |= (JXL_DEC_COLOR_ENCODING | JXL_DEC_FRAME | JXL_DEC_PREVIEW_IMAGE |
               JXL_DEC_BOX);
    if (accepted_formats.empty()) {
      // decoding just the metadata, not the pixel data
      events ^= (JXL_DEC_FULL_IMAGE | JXL_DEC_PREVIEW_IMAGE);
    }
  }
  if (JXL_DEC_SUCCESS != JxlDecoderSubscribeEvents(dec, events)) {
    fprintf(stderr, "JxlDecoderSubscribeEvents failed\n");
    return false;
  }
  if (jpeg_bytes == nullptr) {
    if (JXL_DEC_SUCCESS != JxlDecoderSetRenderSpotcolors(
                               dec, TO_JXL_BOOL(dparams.render_spotcolors))) {
      fprintf(stderr, "JxlDecoderSetRenderSpotColors failed\n");
      return false;
    }
    if (JXL_DEC_SUCCESS != JxlDecoderSetKeepOrientation(
                               dec, TO_JXL_BOOL(dparams.keep_orientation))) {
      fprintf(stderr, "JxlDecoderSetKeepOrientation failed\n");
      return false;
    }
    if (JXL_DEC_SUCCESS != JxlDecoderSetUnpremultiplyAlpha(
                               dec, TO_JXL_BOOL(dparams.unpremultiply_alpha))) {
      fprintf(stderr, "JxlDecoderSetUnpremultiplyAlpha failed\n");
      return false;
    }
    if (dparams.display_nits > 0 &&
        JXL_DEC_SUCCESS !=
            JxlDecoderSetDesiredIntensityTarget(dec, dparams.display_nits)) {
      fprintf(stderr, "Decoder failed to set desired intensity target\n");
      return false;
    }
    if (JXL_DEC_SUCCESS != JxlDecoderSetDecompressBoxes(dec, JXL_TRUE)) {
      fprintf(stderr, "JxlDecoderSetDecompressBoxes failed\n");
      return false;
    }
  }
  if (JXL_DEC_SUCCESS != JxlDecoderSetInput(dec, bytes, bytes_size)) {
    fprintf(stderr, "Decoder failed to set input\n");
    return false;
  }
  uint32_t progression_index = 0;
  bool codestream_done = jpeg_bytes == nullptr && accepted_formats.empty();
  BoxProcessor boxes(dec);
  for (;;) {
    JxlDecoderStatus status = JxlDecoderProcessInput(dec);
    if (status == JXL_DEC_ERROR) {
      fprintf(stderr, "Failed to decode image\n");
      return false;
    } else if (status == JXL_DEC_NEED_MORE_INPUT) {
      if (codestream_done) {
        break;
      }
      if (dparams.allow_partial_input) {
        if (JXL_DEC_SUCCESS != JxlDecoderFlushImage(dec)) {
          fprintf(stderr,
                  "Input file is truncated and there is no preview "
                  "available yet.\n");
          return false;
        }
        break;
      }
      size_t released_size = JxlDecoderReleaseInput(dec);
      fprintf(stderr,
              "Input file is truncated (total bytes: %" PRIuS
              ", processed bytes: %" PRIuS
              ") and --allow_partial_files is not present.\n",
              bytes_size, bytes_size - released_size);
      return false;
    } else if (status == JXL_DEC_BOX) {
      boxes.FinalizeOutput();
      JxlBoxType box_type;
      if (JXL_DEC_SUCCESS != JxlDecoderGetBoxType(dec, box_type, JXL_TRUE)) {
        fprintf(stderr, "JxlDecoderGetBoxType failed\n");
        return false;
      }
      std::vector<uint8_t>* box_data = nullptr;
      if (memcmp(box_type, "Exif", 4) == 0) {
        box_data = &ppf->metadata.exif;
      } else if (memcmp(box_type, "iptc", 4) == 0) {
        box_data = &ppf->metadata.iptc;
      } else if (memcmp(box_type, "jumb", 4) == 0) {
        box_data = &ppf->metadata.jumbf;
      } else if (memcmp(box_type, "jhgm", 4) == 0) {
        box_data = &ppf->metadata.jhgm;
      } else if (memcmp(box_type, "xml ", 4) == 0) {
        box_data = &ppf->metadata.xmp;
      }
      if (box_data) {
        if (!boxes.InitializeOutput(box_data)) {
          return false;
        }
      }
    } else if (status == JXL_DEC_BOX_NEED_MORE_OUTPUT) {
      if (!boxes.AddMoreOutput()) {
        return false;
      }
    } else if (status == JXL_DEC_JPEG_RECONSTRUCTION) {
      can_reconstruct_jpeg = true;
      // Decoding to JPEG.
      if (JXL_DEC_SUCCESS != JxlDecoderSetJPEGBuffer(dec,
                                                     jpeg_data_chunk.data(),
                                                     jpeg_data_chunk.size())) {
        fprintf(stderr, "Decoder failed to set JPEG Buffer\n");
        return false;
      }
    } else if (status == JXL_DEC_JPEG_NEED_MORE_OUTPUT) {
      if (jpeg_bytes == nullptr) {
        fprintf(stderr, "internal: jpeg_bytes == nullptr\n");
        return false;
      }
      // Decoded a chunk to JPEG.
      size_t used_jpeg_output =
          jpeg_data_chunk.size() - JxlDecoderReleaseJPEGBuffer(dec);
      jpeg_bytes->insert(jpeg_bytes->end(), jpeg_data_chunk.data(),
                         jpeg_data_chunk.data() + used_jpeg_output);
      if (used_jpeg_output == 0) {
        // Chunk is too small.
        jpeg_data_chunk.resize(jpeg_data_chunk.size() * 2);
      }
      if (JXL_DEC_SUCCESS != JxlDecoderSetJPEGBuffer(dec,
                                                     jpeg_data_chunk.data(),
                                                     jpeg_data_chunk.size())) {
        fprintf(stderr, "Decoder failed to set JPEG Buffer\n");
        return false;
      }
    } else if (status == JXL_DEC_BASIC_INFO) {
      if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo(dec, &ppf->info)) {
        fprintf(stderr, "JxlDecoderGetBasicInfo failed\n");
        return false;
      }
      if (accepted_formats.empty()) continue;
      if (num_color_channels != 0) {
        // Mark the change in number of color channels due to the requested
        // color space.
        ppf->info.num_color_channels = num_color_channels;
      }
      if (dparams.output_bitdepth.type == JXL_BIT_DEPTH_CUSTOM) {
        // Select format based on custom bits per sample.
        ppf->info.bits_per_sample = dparams.output_bitdepth.bits_per_sample;
      }
      // Select format according to accepted formats.
      if (!jxl::extras::SelectFormat(accepted_formats, ppf->info, &format)) {
        fprintf(stderr, "SelectFormat failed\n");
        return false;
      }
      bool have_alpha = (format.num_channels == 2 || format.num_channels == 4);
      if (!have_alpha) {
        // Mark in the basic info that alpha channel was dropped.
        ppf->info.alpha_bits = 0;
      } else {
        if (dparams.unpremultiply_alpha) {
          // Mark in the basic info that alpha was unpremultiplied.
          ppf->info.alpha_premultiplied = JXL_FALSE;
        }
      }
      bool alpha_found = false;
      for (uint32_t i = 0; i < ppf->info.num_extra_channels; ++i) {
        JxlExtraChannelInfo eci;
        if (JXL_DEC_SUCCESS != JxlDecoderGetExtraChannelInfo(dec, i, &eci)) {
          fprintf(stderr, "JxlDecoderGetExtraChannelInfo failed\n");
          return false;
        }
        if (eci.type == JXL_CHANNEL_ALPHA && have_alpha && !alpha_found) {
          // Skip the first alpha channels because it is already present in the
          // interleaved image.
          alpha_found = true;
          continue;
        }
        std::string name(eci.name_length + 1, 0);
        if (JXL_DEC_SUCCESS !=
            JxlDecoderGetExtraChannelName(
                dec, i, const_cast<char*>(name.data()), name.size())) {
          fprintf(stderr, "JxlDecoderGetExtraChannelName failed\n");
          return false;
        }
        name.resize(eci.name_length);
        ppf->extra_channels_info.push_back({eci, i, name});
      }
    } else if (status == JXL_DEC_COLOR_ENCODING) {
      if (!dparams.color_space.empty()) {
        if (ppf->info.uses_original_profile) {
          fprintf(stderr,
                  "Warning: --color_space ignored because the image is "
                  "not XYB encoded.\n");
        } else {
          JxlDecoderSetCms(dec, *JxlGetDefaultCms());
          if (JXL_DEC_SUCCESS !=
              JxlDecoderSetPreferredColorProfile(dec, &color_encoding)) {
            fprintf(stderr, "Failed to set color space.\n");
            return false;
          }
        }
      }
      size_t icc_size = 0;
      JxlColorProfileTarget target = JXL_COLOR_PROFILE_TARGET_DATA;
      if (JXL_DEC_SUCCESS !=
          JxlDecoderGetICCProfileSize(dec, target, &icc_size)) {
        fprintf(stderr, "JxlDecoderGetICCProfileSize failed\n");
      }
      if (icc_size != 0) {
        ppf->primary_color_representation = PackedPixelFile::kIccIsPrimary;
        ppf->icc.resize(icc_size);
        if (JXL_DEC_SUCCESS != JxlDecoderGetColorAsICCProfile(
                                   dec, target, ppf->icc.data(), icc_size)) {
          fprintf(stderr, "JxlDecoderGetColorAsICCProfile failed\n");
          return false;
        }
      }
      if (JXL_DEC_SUCCESS == JxlDecoderGetColorAsEncodedProfile(
                                 dec, target, &ppf->color_encoding)) {
        ppf->primary_color_representation =
            PackedPixelFile::kColorEncodingIsPrimary;
      } else {
        ppf->color_encoding.color_space = JXL_COLOR_SPACE_UNKNOWN;
      }
      icc_size = 0;
      target = JXL_COLOR_PROFILE_TARGET_ORIGINAL;
      if (JXL_DEC_SUCCESS !=
          JxlDecoderGetICCProfileSize(dec, target, &icc_size)) {
        fprintf(stderr, "JxlDecoderGetICCProfileSize failed\n");
      }
      if (icc_size != 0) {
        ppf->orig_icc.resize(icc_size);
        if (JXL_DEC_SUCCESS !=
            JxlDecoderGetColorAsICCProfile(dec, target, ppf->orig_icc.data(),
                                           icc_size)) {
          fprintf(stderr, "JxlDecoderGetColorAsICCProfile failed\n");
          return false;
        }
      }
    } else if (status == JXL_DEC_FRAME) {
      auto frame_or = jxl::extras::PackedFrame::Create(ppf->info.xsize,
                                                       ppf->info.ysize, format);
      JXL_ASSIGN_OR_QUIT(jxl::extras::PackedFrame frame,
                         jxl::extras::PackedFrame::Create(
                             ppf->info.xsize, ppf->info.ysize, format),
                         "Failed to create image frame.");
      if (JXL_DEC_SUCCESS != JxlDecoderGetFrameHeader(dec, &frame.frame_info)) {
        fprintf(stderr, "JxlDecoderGetFrameHeader failed\n");
        return false;
      }
      frame.name.resize(frame.frame_info.name_length + 1, 0);
      if (JXL_DEC_SUCCESS !=
          JxlDecoderGetFrameName(dec, const_cast<char*>(frame.name.data()),
                                 frame.name.size())) {
        fprintf(stderr, "JxlDecoderGetFrameName failed\n");
        return false;
      }
      frame.name.resize(frame.frame_info.name_length);
      ppf->frames.emplace_back(std::move(frame));
      progression_index = 0;
    } else if (status == JXL_DEC_FRAME_PROGRESSION) {
      size_t downsampling = JxlDecoderGetIntendedDownsamplingRatio(dec);
      if ((max_passes_defined && progression_index >= dparams.max_passes) ||
          (!max_passes_defined && downsampling <= dparams.max_downsampling)) {
        if (JXL_DEC_SUCCESS != JxlDecoderFlushImage(dec)) {
          fprintf(stderr, "JxlDecoderFlushImage failed\n");
          return false;
        }
        if (ppf->frames.back().frame_info.is_last) {
          break;
        }
        if (JXL_DEC_SUCCESS != JxlDecoderSkipCurrentFrame(dec)) {
          fprintf(stderr, "JxlDecoderSkipCurrentFrame failed\n");
          return false;
        }
      }
      ++progression_index;
    } else if (status == JXL_DEC_NEED_PREVIEW_OUT_BUFFER) {
      size_t buffer_size;
      if (JXL_DEC_SUCCESS !=
          JxlDecoderPreviewOutBufferSize(dec, &format, &buffer_size)) {
        fprintf(stderr, "JxlDecoderPreviewOutBufferSize failed\n");
        return false;
      }
      JXL_ASSIGN_OR_QUIT(
          jxl::extras::PackedImage preview_image,
          jxl::extras::PackedImage::Create(ppf->info.preview.xsize,
                                           ppf->info.preview.ysize, format),
          "Failed to create preview image.");
      ppf->preview_frame =
          jxl::make_unique<jxl::extras::PackedFrame>(std::move(preview_image));
      if (buffer_size != ppf->preview_frame->color.pixels_size) {
        fprintf(stderr, "Invalid out buffer size %" PRIuS " %" PRIuS "\n",
                buffer_size, ppf->preview_frame->color.pixels_size);
        return false;
      }
      if (JXL_DEC_SUCCESS !=
          JxlDecoderSetPreviewOutBuffer(
              dec, &format, ppf->preview_frame->color.pixels(), buffer_size)) {
        fprintf(stderr, "JxlDecoderSetPreviewOutBuffer failed\n");
        return false;
      }
    } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
      if (jpeg_bytes != nullptr) {
        break;
      }
      size_t buffer_size;
      if (JXL_DEC_SUCCESS !=
          JxlDecoderImageOutBufferSize(dec, &format, &buffer_size)) {
        fprintf(stderr, "JxlDecoderImageOutBufferSize failed\n");
        return false;
      }
      jxl::extras::PackedFrame& frame = ppf->frames.back();
      if (buffer_size != frame.color.pixels_size) {
        fprintf(stderr, "Invalid out buffer size %" PRIuS " %" PRIuS "\n",
                buffer_size, frame.color.pixels_size);
        return false;
      }

      if (dparams.use_image_callback) {
        auto callback = [](void* opaque, size_t x, size_t y, size_t num_pixels,
                           const void* pixels) {
          auto* ppf = reinterpret_cast<jxl::extras::PackedPixelFile*>(opaque);
          jxl::extras::PackedImage& color = ppf->frames.back().color;
          uint8_t* pixels_buffer = reinterpret_cast<uint8_t*>(color.pixels());
          size_t sample_size = color.pixel_stride();
          memcpy(pixels_buffer + (color.stride * y + sample_size * x), pixels,
                 num_pixels * sample_size);
        };
        if (JXL_DEC_SUCCESS !=
            JxlDecoderSetImageOutCallback(dec, &format, callback, ppf)) {
          fprintf(stderr, "JxlDecoderSetImageOutCallback failed\n");
          return false;
        }
      } else {
        if (JXL_DEC_SUCCESS != JxlDecoderSetImageOutBuffer(dec, &format,
                                                           frame.color.pixels(),
                                                           buffer_size)) {
          fprintf(stderr, "JxlDecoderSetImageOutBuffer failed\n");
          return false;
        }
      }
      if (JXL_DEC_SUCCESS !=
          JxlDecoderSetImageOutBitDepth(dec, &dparams.output_bitdepth)) {
        fprintf(stderr, "JxlDecoderSetImageOutBitDepth failed\n");
        return false;
      }
      UpdateBitDepth(dparams.output_bitdepth, format.data_type, &ppf->info);
      bool have_alpha = (format.num_channels == 2 || format.num_channels == 4);
      if (have_alpha) {
        // Interleaved alpha channels has the same bit depth as color channels.
        ppf->info.alpha_bits = ppf->info.bits_per_sample;
        ppf->info.alpha_exponent_bits = ppf->info.exponent_bits_per_sample;
      }
      JxlPixelFormat ec_format = format;
      ec_format.num_channels = 1;
      for (auto& eci : ppf->extra_channels_info) {
        JXL_ASSIGN_OR_QUIT(jxl::extras::PackedImage image,
                           jxl::extras::PackedImage::Create(
                               ppf->info.xsize, ppf->info.ysize, ec_format),
                           "Failed to create extra channel image.");
        frame.extra_channels.emplace_back(std::move(image));
        auto& ec = frame.extra_channels.back();
        size_t buffer_size;
        if (JXL_DEC_SUCCESS != JxlDecoderExtraChannelBufferSize(
                                   dec, &ec_format, &buffer_size, eci.index)) {
          fprintf(stderr, "JxlDecoderExtraChannelBufferSize failed\n");
          return false;
        }
        if (buffer_size != ec.pixels_size) {
          fprintf(stderr,
                  "Invalid extra channel buffer size"
                  " %" PRIuS " %" PRIuS "\n",
                  buffer_size, ec.pixels_size);
          return false;
        }
        if (JXL_DEC_SUCCESS !=
            JxlDecoderSetExtraChannelBuffer(dec, &ec_format, ec.pixels(),
                                            buffer_size, eci.index)) {
          fprintf(stderr, "JxlDecoderSetExtraChannelBuffer failed\n");
          return false;
        }
        UpdateBitDepth(dparams.output_bitdepth, ec_format.data_type,
                       &eci.ec_info);
      }
    } else if (status == JXL_DEC_SUCCESS) {
      // Decoding finished successfully.
      break;
    } else if (status == JXL_DEC_PREVIEW_IMAGE) {
      // Nothing to do.
    } else if (status == JXL_DEC_FULL_IMAGE) {
      if (jpeg_bytes != nullptr || ppf->frames.back().frame_info.is_last) {
        codestream_done = true;
      }
    } else {
      fprintf(stderr, "Error: unexpected status: %d\n",
              static_cast<int>(status));
      return false;
    }
  }
  boxes.FinalizeOutput();
  if (!ppf->metadata.exif.empty()) {
    // Verify that Exif box has a valid TIFF header at the specified offset.
    // Discard bytes preceding the header.
    if (ppf->metadata.exif.size() >= 4) {
      uint32_t offset = LoadBE32(ppf->metadata.exif.data());
      if (offset <= ppf->metadata.exif.size() - 8) {
        std::vector<uint8_t> exif(ppf->metadata.exif.begin() + 4 + offset,
                                  ppf->metadata.exif.end());
        bool bigendian;
        if (IsExif(exif, &bigendian)) {
          ppf->metadata.exif = std::move(exif);
        } else {
          fprintf(stderr, "Warning: invalid TIFF header in Exif\n");
        }
      } else {
        fprintf(stderr, "Warning: invalid Exif offset: %" PRIu32 "\n", offset);
      }
    } else {
      fprintf(stderr, "Warning: invalid Exif length: %" PRIuS "\n",
              ppf->metadata.exif.size());
    }
  }
  if (jpeg_bytes != nullptr) {
    if (!can_reconstruct_jpeg) return false;
    size_t used_jpeg_output =
        jpeg_data_chunk.size() - JxlDecoderReleaseJPEGBuffer(dec);
    jpeg_bytes->insert(jpeg_bytes->end(), jpeg_data_chunk.data(),
                       jpeg_data_chunk.data() + used_jpeg_output);
  }
  if (decoded_bytes) {
    *decoded_bytes = bytes_size - JxlDecoderReleaseInput(dec);
  }
  return true;
}

}  // namespace extras
}  // namespace jxl
