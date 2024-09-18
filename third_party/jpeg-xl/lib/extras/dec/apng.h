// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_EXTRAS_DEC_APNG_H_
#define LIB_EXTRAS_DEC_APNG_H_

// Decodes APNG images in memory.

#include <cstdint>

#include "lib/extras/dec/color_hints.h"
#include "lib/extras/packed_image.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"

namespace jxl {

struct SizeConstraints;

namespace extras {

bool CanDecodeAPNG();

// Decodes `bytes` into `ppf`.
Status DecodeImageAPNG(Span<const uint8_t> bytes, const ColorHints& color_hints,
                       PackedPixelFile* ppf,
                       const SizeConstraints* constraints = nullptr);

}  // namespace extras
}  // namespace jxl

#endif  // LIB_EXTRAS_DEC_APNG_H_
