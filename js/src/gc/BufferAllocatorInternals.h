/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_BufferAllocatorInternals_h
#define gc_BufferAllocatorInternals_h

namespace js::gc {

// Encode a size with a single byte by using separate value and shift parts.
//
// This is essentially a floating point representation with the value being the
// mantissa and the shift being the exponent.
template <size_t GranularityShift = 4>
struct EncodedSize {
  static constexpr size_t ShiftBits = 4;
  static constexpr size_t ValueBits = 4;
  static_assert(ShiftBits + ValueBits == 8);

  static constexpr size_t Granularity = Bit(GranularityShift);
  static constexpr size_t MaxSize = BitMask(ValueBits + 1)
                                    << (BitMask(ShiftBits) - 1 + GranularityShift);

  uint8_t bits = 0;

  EncodedSize() = default;

  explicit EncodedSize(size_t bytes) { set(bytes); }

  void set(size_t bytes) {
    MOZ_ASSERT(bytes < MaxSize);

    // Round up to granularity and shift.
    size_t i = (bytes + BitMask(GranularityShift)) >> GranularityShift;

    // Sizes that fit in ValueBits are represented directly.
    if (i < Bit(ValueBits)) {
      bits = i;
      return;
    }

    // Larger sizes store only ValueBits of the value plus a shift. The MSB is
    // implied and is not stored as part of value.
    MOZ_ASSERT(i <= UINT32_MAX);
    size_t topBit = 31 - mozilla::CountLeadingZeroes32(i);
    MOZ_ASSERT(i & Bit(topBit));

    size_t shift = topBit - ValueBits;
    size_t value = (i >> shift) & BitMask(ValueBits);
    bits = ((shift + 1) << ValueBits) | value;
    if (i & BitMask(shift)) {
      bits++;  // Value overflows into shift.
    }
  }

  size_t get() const {
    size_t shift = shiftPart();
    size_t value = valuePart();

    size_t i;
    if (shift == 0) {
      i = value;
    } else {
      i = (Bit(ValueBits) | value) << (shift - 1);
    }
    return i << GranularityShift;
  }

  size_t valuePart() const { return bits & BitMask(ValueBits); }
  size_t shiftPart() const { return bits >> ValueBits; }
};

}  // namespace js::gc

#endif  // gc_BufferAllocatorInternals_h
