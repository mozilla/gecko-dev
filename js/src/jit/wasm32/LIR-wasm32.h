/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_wasm32_LIR_wasm32_h
#define jit_wasm32_LIR_wasm32_h

namespace js::jit {

class LUnbox : public LInstructionHelper<1, 2, 0> {
 public:
  MUnbox* mir() const { MOZ_CRASH(); }
  const LAllocation* payload() { MOZ_CRASH(); }
  const LAllocation* type() { MOZ_CRASH(); }
  const char* extraName() const { MOZ_CRASH(); }
};
class LDivPowTwoI : public LInstructionHelper<1, 1, 0> {
 public:
  LDivPowTwoI(const LAllocation&, int32_t)
      : LInstructionHelper(Opcode::Invalid) {
    MOZ_CRASH();
  }
  const LAllocation* numerator() { MOZ_CRASH(); }
  int32_t shift() { MOZ_CRASH(); }
  MDiv* mir() const { MOZ_CRASH(); }
};

}  // namespace js::jit

#endif /* jit_wasm32_LIR_wasm32_h */
