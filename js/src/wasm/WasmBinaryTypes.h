/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef wasm_WasmBinaryTypes_h
#define wasm_WasmBinaryTypes_h

#include "mozilla/Maybe.h"
#include "mozilla/Span.h"
#include "mozilla/Vector.h"

#include "js/AllocPolicy.h"

#include "wasm/WasmSerialize.h"

namespace js {
namespace wasm {

using BytecodeSpan = mozilla::Span<const uint8_t>;

// This struct captures a range of bytecode.
struct BytecodeRange {
  BytecodeRange() = default;
  BytecodeRange(uint32_t start, uint32_t size) : start(start), size(size) {}

  uint32_t start = 0;
  uint32_t size = 0;

  WASM_CHECK_CACHEABLE_POD(start, size);

  uint32_t end() const { return start + size; }

  // Returns whether a range is a non-strict subset of this range.
  bool contains(const BytecodeRange& other) const {
    return other.start >= start && other.end() <= end();
  }

  // Returns whether an offset is contained in this range.
  bool containsOffset(uint32_t bytecodeOffset) const {
    return bytecodeOffset >= start && bytecodeOffset < end();
  }

  // Compare where an offset falls relative to this range. This returns `0` if
  // it is contained in this range, `-1` if it falls before the range, and `1`
  // if it is after the range.
  int compareOffset(uint32_t bytecodeOffset) const {
    if (containsOffset(bytecodeOffset)) {
      return 0;
    }
    if (bytecodeOffset < start) {
      return -1;
    }
    MOZ_ASSERT(bytecodeOffset >= end());
    return 1;
  }

  bool operator==(const BytecodeRange& rhs) const {
    return start == rhs.start && size == rhs.size;
  }

  // Returns a range that represents `this` relative to `other`. `this` must
  // be wholly contained in `other`, no partial overlap is allowed.
  BytecodeRange relativeTo(const BytecodeRange& other) const {
    MOZ_RELEASE_ASSERT(other.contains(*this));
    return BytecodeRange(start - other.start, size);
  }

  // Gets the span that this range represents from a vector-like bytecode.
  template <typename T>
  BytecodeSpan toSpan(const T& bytecode) const {
    MOZ_RELEASE_ASSERT(end() <= bytecode.length());
    return BytecodeSpan(bytecode.begin() + start, bytecode.begin() + end());
  }
};

WASM_DECLARE_CACHEABLE_POD(BytecodeRange);

using MaybeBytecodeRange = mozilla::Maybe<BytecodeRange>;
using BytecodeRangeVector =
    mozilla::Vector<BytecodeRange, 0, SystemAllocPolicy>;

}  // namespace wasm
}  // namespace js

#endif /* wasm_WasmBinaryTypes_h */
