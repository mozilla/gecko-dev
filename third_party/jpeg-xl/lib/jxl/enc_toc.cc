// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/enc_toc.h"

#include <memory>

#include "lib/jxl/base/common.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/enc_aux_out.h"
#include "lib/jxl/enc_coeff_order.h"
#include "lib/jxl/field_encodings.h"
#include "lib/jxl/fields.h"
#include "lib/jxl/toc.h"

namespace jxl {
Status WriteGroupOffsets(
    const std::vector<std::unique_ptr<BitWriter>>& group_codes,
    const std::vector<coeff_order_t>& permutation,
    BitWriter* JXL_RESTRICT writer, AuxOut* aux_out) {
  return writer->WithMaxBits(
      MaxBits(group_codes.size()), LayerType::Toc, aux_out, [&]() -> Status {
        if (!permutation.empty() && !group_codes.empty()) {
          // Don't write a permutation at all for an empty group_codes.
          writer->Write(1, 1);  // permutation
          JXL_ENSURE(permutation.size() == group_codes.size());
          JXL_RETURN_IF_ERROR(EncodePermutation(permutation.data(), /*skip=*/0,
                                                permutation.size(), writer,
                                                LayerType::Header, aux_out));

        } else {
          writer->Write(1, 0);  // no permutation
        }
        writer->ZeroPadToByte();  // before TOC entries

        for (const auto& bw : group_codes) {
          JXL_ENSURE(bw->BitsWritten() % kBitsPerByte == 0);
          const size_t group_size = bw->BitsWritten() / kBitsPerByte;
          JXL_RETURN_IF_ERROR(U32Coder::Write(kTocDist, group_size, writer));
        }
        writer->ZeroPadToByte();  // before first group
        return true;
      });
}

}  // namespace jxl
