// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/dec/decode.h"

#include <locale>

#include "lib/extras/dec/apng.h"
#include "lib/extras/dec/exr.h"
#include "lib/extras/dec/gif.h"
#include "lib/extras/dec/jpg.h"
#include "lib/extras/dec/jxl.h"
#include "lib/extras/dec/pgx.h"
#include "lib/extras/dec/pnm.h"

namespace jxl {
namespace extras {
namespace {

// Any valid encoding is larger (ensures codecs can read the first few bytes)
constexpr size_t kMinBytes = 9;

std::string GetExtension(const std::string& path) {
  // Pattern: "name.png"
  size_t pos = path.find_last_of('.');
  if (pos != std::string::npos) {
    return path.substr(pos);
  }

  // Extension not found
  return "";
}

}  // namespace

Codec CodecFromPath(const std::string& path,
                    size_t* JXL_RESTRICT bits_per_sample,
                    std::string* extension) {
  std::string ext = GetExtension(path);
  if (extension) {
    if (extension->empty()) {
      *extension = ext;
    } else {
      ext = *extension;
    }
  }
  std::transform(ext.begin(), ext.end(), ext.begin(), [](char c) {
    return std::tolower(c, std::locale::classic());
  });
  if (ext == ".png") return Codec::kPNG;

  if (ext == ".jpg") return Codec::kJPG;
  if (ext == ".jpeg") return Codec::kJPG;

  if (ext == ".pgx") return Codec::kPGX;

  if (ext == ".pam") return Codec::kPNM;
  if (ext == ".pnm") return Codec::kPNM;
  if (ext == ".pgm") return Codec::kPNM;
  if (ext == ".ppm") return Codec::kPNM;
  if (ext == ".pfm") {
    if (bits_per_sample != nullptr) *bits_per_sample = 32;
    return Codec::kPNM;
  }

  if (ext == ".gif") return Codec::kGIF;

  if (ext == ".exr") return Codec::kEXR;

  return Codec::kUnknown;
}

bool CanDecode(Codec codec) {
  switch (codec) {
    case Codec::kEXR:
      return CanDecodeEXR();
    case Codec::kGIF:
      return CanDecodeGIF();
    case Codec::kJPG:
      return CanDecodeJPG();
    case Codec::kPNG:
      return CanDecodeAPNG();
    case Codec::kPNM:
    case Codec::kPGX:
    case Codec::kJXL:
      return true;
    default:
      return false;
  }
}

std::string ListOfDecodeCodecs() {
  std::string list_of_codecs("JXL, PPM, PNM, PFM, PAM, PGX");
  if (CanDecode(Codec::kPNG)) list_of_codecs.append(", PNG, APNG");
  if (CanDecode(Codec::kGIF)) list_of_codecs.append(", GIF");
  if (CanDecode(Codec::kJPG)) list_of_codecs.append(", JPEG");
  if (CanDecode(Codec::kEXR)) list_of_codecs.append(", EXR");
  return list_of_codecs;
}

Status DecodeBytes(const Span<const uint8_t> bytes,
                   const ColorHints& color_hints, extras::PackedPixelFile* ppf,
                   const SizeConstraints* constraints, Codec* orig_codec) {
  if (bytes.size() < kMinBytes) return JXL_FAILURE("Too few bytes");

  *ppf = extras::PackedPixelFile();

  // Default values when not set by decoders.
  ppf->info.uses_original_profile = JXL_TRUE;
  ppf->info.orientation = JXL_ORIENT_IDENTITY;

  const auto choose_codec = [&]() -> Codec {
    if (DecodeImageAPNG(bytes, color_hints, ppf, constraints)) {
      return Codec::kPNG;
    }
    if (DecodeImagePGX(bytes, color_hints, ppf, constraints)) {
      return Codec::kPGX;
    }
    if (DecodeImagePNM(bytes, color_hints, ppf, constraints)) {
      return Codec::kPNM;
    }
    JXLDecompressParams dparams = {};
    for (const uint32_t num_channels : {1, 2, 3, 4}) {
      dparams.accepted_formats.push_back(
          {num_channels, JXL_TYPE_FLOAT, JXL_LITTLE_ENDIAN, /*align=*/0});
    }
    dparams.output_bitdepth.type = JXL_BIT_DEPTH_FROM_CODESTREAM;
    size_t decoded_bytes;
    if (DecodeImageJXL(bytes.data(), bytes.size(), dparams, &decoded_bytes,
                       ppf) &&
        ApplyColorHints(color_hints, true, ppf->info.num_color_channels == 1,
                        ppf)) {
      return Codec::kJXL;
    }
    if (DecodeImageGIF(bytes, color_hints, ppf, constraints)) {
      return Codec::kGIF;
    }
    if (DecodeImageJPG(bytes, color_hints, ppf, constraints)) {
      return Codec::kJPG;
    }
    if (DecodeImageEXR(bytes, color_hints, ppf, constraints)) {
      return Codec::kEXR;
    }
    return Codec::kUnknown;
  };

  Codec codec = choose_codec();
  if (codec == Codec::kUnknown) {
    return JXL_FAILURE("Codecs failed to decode");
  }
  if (orig_codec) *orig_codec = codec;

  return true;
}

}  // namespace extras
}  // namespace jxl
