/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_riscv64_MoveEmitter_riscv64_h
#define jit_riscv64_MoveEmitter_riscv64_h

#include "mozilla/Assertions.h"

namespace js {
namespace jit {

class MacroAssemblerRiscv64;
class MoveResolver;
struct Register;

class MoveEmitterRiscv64 {
  uint32_t inCycle_;
  MacroAssembler& masm;

  // Original stack push value.
  uint32_t pushedAtStart_;

  // These store stack offsets to spill locations, snapshotting
  // codegen->framePushed_ at the time they were allocated. They are -1 if no
  // stack space has been allocated for that particular spill.
  int32_t pushedAtCycle_;
  int32_t pushedAtSpill_;

  // These are registers that are available for temporary use. They may be
  // assigned InvalidReg. If no corresponding spill space has been assigned,
  // then these registers do not need to be spilled.
  Register spilledReg_;
  FloatRegister spilledFloatReg_;
 public:
  explicit MoveEmitterRiscv64(MacroAssemblerRiscv64&)
      : inCycle_(0),
        masm(masm),
        pushedAtStart_(masm.framePushed()),
        pushedAtCycle_(-1),
        pushedAtSpill_(-1),
        spilledReg_(InvalidReg),
        spilledFloatReg_(InvalidFloatReg) {
    MOZ_CRASH();
  }
  void emit(const MoveResolver&) { MOZ_CRASH(); }
  void finish() { MOZ_CRASH(); }
  void setScratchRegister(Register) { MOZ_CRASH(); }
};

typedef MoveEmitterRiscv64 MoveEmitter;

}  // namespace jit
}  // namespace js

#endif /* jit_riscv64_MoveEmitter_riscv64_h */
