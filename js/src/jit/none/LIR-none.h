/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_none_LIR_none_h
#define jit_none_LIR_none_h

namespace js {
namespace jit {

class LUnboxFloatingPoint : public LInstruction {
 public:
  LIR_HEADER(UnboxFloatingPoint)
  static const size_t Input = 0;

  MUnbox* mir() const { MOZ_CRASH(); }

  const LDefinition* output() const { MOZ_CRASH(); }
};

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
class LModI : public LBinaryMath<1> {
 public:
  LModI(const LAllocation&, const LAllocation&, const LDefinition&)
      : LBinaryMath(Opcode::Invalid) {
    MOZ_CRASH();
  }

  const LDefinition* callTemp() { MOZ_CRASH(); }
  MMod* mir() const { MOZ_CRASH(); }
};

class LMulI : public LInstruction {};

}  // namespace jit
}  // namespace js

#endif /* jit_none_LIR_none_h */
