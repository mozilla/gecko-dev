/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef wasm_WasmBinaryTypes_h
#define wasm_WasmBinaryTypes_h

#include "mozilla/Maybe.h"

#include "wasm/WasmSerialize.h"

namespace js {
namespace wasm {

// This struct captures the bytecode offset of a section's payload (so not
// including the header) and the size of the payload.

struct SectionRange {
  uint32_t start;
  uint32_t size;

  WASM_CHECK_CACHEABLE_POD(start, size);

  uint32_t end() const { return start + size; }
  bool operator==(const SectionRange& rhs) const {
    return start == rhs.start && size == rhs.size;
  }
};

WASM_DECLARE_CACHEABLE_POD(SectionRange);

using MaybeSectionRange = mozilla::Maybe<SectionRange>;

}  // namespace wasm
}  // namespace js

#endif /* wasm_WasmBinaryTypes_h */
