/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_riscv64_constant_Constant_riscv64_h
#define jit_riscv64_constant_Constant_riscv64_h
#include "mozilla/Assertions.h"
#include "mozilla/Types.h"

#include <stdio.h>

#include "jit/riscv64/constant/Base-constant-riscv.h"
#include "jit/riscv64/constant/Constant-riscv-a.h"
#include "jit/riscv64/constant/Constant-riscv-c.h"
#include "jit/riscv64/constant/Constant-riscv-d.h"
#include "jit/riscv64/constant/Constant-riscv-f.h"
#include "jit/riscv64/constant/Constant-riscv-i.h"
#include "jit/riscv64/constant/Constant-riscv-m.h"
#include "jit/riscv64/constant/Constant-riscv-v.h"
#include "jit/riscv64/constant/Constant-riscv-zicsr.h"
#include "jit/riscv64/constant/Constant-riscv-zifencei.h"

namespace js {
namespace jit {
namespace disasm {


// A reasonable (ie, safe) buffer size for the disassembly of a single
// instruction.
const int ReasonableBufferSize = 256;

// Vector as used by the original code to allow for minimal modification.
// Functions exactly like a character array with helper methods.
template <typename T>
class V8Vector {
 public:
  V8Vector() : start_(nullptr), length_(0) {}
  V8Vector(T* data, int length) : start_(data), length_(length) {
    MOZ_ASSERT(length == 0 || (length > 0 && data != nullptr));
  }

  // Returns the length of the vector.
  int length() const { return length_; }

  // Returns the pointer to the start of the data in the vector.
  T* start() const { return start_; }

  // Access individual vector elements - checks bounds in debug mode.
  T& operator[](int index) const {
    MOZ_ASSERT(0 <= index && index < length_);
    return start_[index];
  }

  inline V8Vector<T> operator+(int offset) {
    MOZ_ASSERT(offset < length_);
    return V8Vector<T>(start_ + offset, length_ - offset);
  }

 private:
  T* start_;
  int length_;
};

template <typename T, int kSize>
class EmbeddedVector : public V8Vector<T> {
 public:
  EmbeddedVector() : V8Vector<T>(buffer_, kSize) {}

  explicit EmbeddedVector(T initial_value) : V8Vector<T>(buffer_, kSize) {
    for (int i = 0; i < kSize; ++i) {
      buffer_[i] = initial_value;
    }
  }

  // When copying, make underlying Vector to reference our buffer.
  EmbeddedVector(const EmbeddedVector& rhs) : V8Vector<T>(rhs) {
    MemCopy(buffer_, rhs.buffer_, sizeof(T) * kSize);
    this->set_start(buffer_);
  }

  EmbeddedVector& operator=(const EmbeddedVector& rhs) {
    if (this == &rhs)
      return *this;
    V8Vector<T>::operator=(rhs);
    MemCopy(buffer_, rhs.buffer_, sizeof(T) * kSize);
    this->set_start(buffer_);
    return *this;
  }

 private:
  T buffer_[kSize];
};

}  // namespace disasm
}  // namespace jit
}  // namespace js

#endif  // jit_riscv64_constant_Constant_riscv64_h