/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/riscv64/MoveEmitter-riscv64.h"

#include "jit/MacroAssembler-inl.h"

using namespace js;
using namespace js::jit;

void MoveEmitterRiscv64::breakCycle(const MoveOperand& from,
                                    const MoveOperand& to,
                                    MoveOp::Type type,
                                    uint32_t slotId) {
  // There is some pattern:
  //   (A -> B)
  //   (B -> A)
  //
  // This case handles (A -> B), which we reach first. We save B, then allow
  // the original move to continue.
  switch (type) {
    case MoveOp::FLOAT32:
      if (to.isMemory()) {
        ScratchFloat32Scope fpscratch32(masm);
        masm.loadFloat32(getAdjustedAddress(to), fpscratch32);
        masm.storeFloat32(fpscratch32, cycleSlot(slotId));
      } else {
        masm.storeFloat32(to.floatReg(), cycleSlot(slotId));
      }
      break;
    case MoveOp::DOUBLE:
      if (to.isMemory()) {
        ScratchDoubleScope fpscratch64(masm);
        masm.loadDouble(getAdjustedAddress(to), fpscratch64);
        masm.storeDouble(fpscratch64, cycleSlot(slotId));
      } else {
        masm.storeDouble(to.floatReg(), cycleSlot(slotId));
      }
      break;
    case MoveOp::INT32:
      if (to.isMemory()) {
        UseScratchRegisterScope temps(&masm);
        Register scratch2 = temps.Acquire();
        masm.load32(getAdjustedAddress(to), scratch2);
        masm.store32(scratch2, cycleSlot(0));
      } else {
        masm.store32(to.reg(), cycleSlot(0));
      }
      break;
    case MoveOp::GENERAL:
      if (to.isMemory()) {
        UseScratchRegisterScope temps(&masm);
        Register scratch2 = temps.Acquire();
        masm.loadPtr(getAdjustedAddress(to), scratch2);
        masm.storePtr(scratch2, cycleSlot(0));
      } else {
        masm.storePtr(to.reg(), cycleSlot(0));
      }
      break;
    default:
      MOZ_CRASH("Unexpected move type");
  }
}

void MoveEmitterRiscv64::completeCycle(const MoveOperand& from,
                                       const MoveOperand& to,
                                       MoveOp::Type type,
                                       uint32_t slotId) {
  // There is some pattern:
  //   (A -> B)
  //   (B -> A)
  //
  // This case handles (B -> A), which we reach last. We emit a move from the
  // saved value of B, to A.
  switch (type) {
    case MoveOp::FLOAT32:
      if (to.isMemory()) {
        ScratchFloat32Scope fpscratch32(masm);
        masm.loadFloat32(cycleSlot(slotId), fpscratch32);
        masm.storeFloat32(fpscratch32, getAdjustedAddress(to));
      } else {
        masm.loadFloat32(cycleSlot(slotId), to.floatReg());
      }
      break;
    case MoveOp::DOUBLE:
      if (to.isMemory()) {
        ScratchDoubleScope fpscratch64(masm);
        masm.loadDouble(cycleSlot(slotId), fpscratch64);
        masm.storeDouble(fpscratch64, getAdjustedAddress(to));
      } else {
        masm.loadDouble(cycleSlot(slotId), to.floatReg());
      }
      break;
    case MoveOp::INT32:
      MOZ_ASSERT(slotId == 0);
      if (to.isMemory()) {
        UseScratchRegisterScope temps(&masm);
        Register scratch2 = temps.Acquire();
        masm.load32(cycleSlot(0), scratch2);
        masm.store32(scratch2, getAdjustedAddress(to));
      } else {
        masm.load32(cycleSlot(0), to.reg());
      }
      break;
    case MoveOp::GENERAL:
      MOZ_ASSERT(slotId == 0);
      if (to.isMemory()) {
        UseScratchRegisterScope temps(&masm);
        Register scratch2 = temps.Acquire();
        masm.loadPtr(cycleSlot(0), scratch2);
        masm.storePtr(scratch2, getAdjustedAddress(to));
      } else {
        masm.loadPtr(cycleSlot(0), to.reg());
      }
      break;
    default:
      MOZ_CRASH("Unexpected move type");
  }
}

void MoveEmitterRiscv64::emit(const MoveResolver& moves) {
  MOZ_CRASH("Unimplement on riscv");
}

Address MoveEmitterRiscv64::cycleSlot(uint32_t slot, uint32_t subslot) const {
   MOZ_CRASH("Unimplement on riscv");
}

int32_t MoveEmitterRiscv64::getAdjustedOffset(const MoveOperand& operand) {
  MOZ_ASSERT(operand.isMemoryOrEffectiveAddress());
  if (operand.base() != StackPointer) {
    return operand.disp();
  }

  // Adjust offset if stack pointer has been moved.
  return operand.disp() + masm.framePushed() - pushedAtStart_;
}

Address MoveEmitterRiscv64::getAdjustedAddress(const MoveOperand& operand) {
  return Address(operand.base(), getAdjustedOffset(operand));
}

// void MoveEmitterRiscv64::emitMove(const MoveOperand& from,
//                                   const MoveOperand& to) {
//   if (from.isGeneralReg()) {
//     if (to.isGeneralReg()) {
//       masm.movePtr(from.reg(), to.reg());
//     } else if (to.isMemory()) {
//       masm.storePtr(from.reg(), getAdjustedAddress(to));
//     } else {
//       MOZ_CRASH("Invalid emitMove arguments.");
//     }
//   } else if (from.isMemory()) {
//     if (to.isGeneralReg()) {
//       masm.loadPtr(getAdjustedAddress(from), to.reg());
//     } else if (to.isMemory()) {
//       UseScratchRegisterScope temps(&masm);
//       Register scratch2 = temps.Acquire();
//       masm.loadPtr(getAdjustedAddress(from), scratch2);
//       masm.storePtr(scratch2, getAdjustedAddress(to));
//     } else {
//       MOZ_CRASH("Invalid emitMove arguments.");
//     }
//   } else if (from.isEffectiveAddress()) {
//     if (to.isGeneralReg()) {
//       masm.computeEffectiveAddress(getAdjustedAddress(from), to.reg());
//     } else if (to.isMemory()) {
//       UseScratchRegisterScope temps(&masm);
//       Register scratch2 = temps.Acquire();
//       masm.computeEffectiveAddress(getAdjustedAddress(from), scratch2);
//       masm.storePtr(scratch2, getAdjustedAddress(to));
//     } else {
//       MOZ_CRASH("Invalid emitMove arguments.");
//     }
//   } else {
//     MOZ_CRASH("Invalid emitMove arguments.");
//   }
// }

// void MoveEmitterRiscv64::emitInt32Move(const MoveOperand& from,
//                                        const MoveOperand& to) {
//   if (from.isGeneralReg()) {
//     if (to.isGeneralReg()) {
//       masm.move32(from.reg(), to.reg());
//     } else if (to.isMemory()) {
//       masm.store32(from.reg(), getAdjustedAddress(to));
//     } else {
//       MOZ_CRASH("Invalid emitInt32Move arguments.");
//     }
//   } else if (from.isMemory()) {
//     if (to.isGeneralReg()) {
//       masm.load32(getAdjustedAddress(from), to.reg());
//     } else if (to.isMemory()) {
//       UseScratchRegisterScope temps(&masm);
//       Register scratch2 = temps.Acquire();
//       masm.load32(getAdjustedAddress(from), scratch2);
//       masm.store32(scratch2, getAdjustedAddress(to));
//     } else {
//       MOZ_CRASH("Invalid emitInt32Move arguments.");
//     }
//   } else if (from.isEffectiveAddress()) {
//     if (to.isGeneralReg()) {
//       masm.computeEffectiveAddress(getAdjustedAddress(from), to.reg());
//     } else if (to.isMemory()) {
//       UseScratchRegisterScope temps(&masm);
//       Register scratch2 = temps.Acquire();
//       masm.computeEffectiveAddress(getAdjustedAddress(from), scratch2);
//       masm.store32(scratch2, getAdjustedAddress(to));
//     } else {
//       MOZ_CRASH("Invalid emitInt32Move arguments.");
//     }
//   } else {
//     MOZ_CRASH("Invalid emitInt32Move arguments.");
//   }
// }

// void MoveEmitterRiscv64::emitFloat32Move(const MoveOperand& from,
//                                          const MoveOperand& to) {
//   if (from.isFloatReg()) {
//     if (to.isFloatReg()) {
//       masm.moveFloat32(from.floatReg(), to.floatReg());
//     } else if (to.isGeneralReg()) {
//       // This should only be used when passing float parameter in a1,a2,a3
//       MOZ_ASSERT(to.reg() == a1 || to.reg() == a2 || to.reg() == a3);
//       masm.moveFromFloat32(from.floatReg(), to.reg());
//     } else {
//       MOZ_ASSERT(to.isMemory());
//       masm.storeFloat32(from.floatReg(), getAdjustedAddress(to));
//     }
//   } else if (to.isFloatReg()) {
//     MOZ_ASSERT(from.isMemory());
//     masm.loadFloat32(getAdjustedAddress(from), to.floatReg());
//   } else if (to.isGeneralReg()) {
//     MOZ_ASSERT(from.isMemory());
//     // This should only be used when passing float parameter in a1,a2,a3
//     MOZ_ASSERT(to.reg() == a1 || to.reg() == a2 || to.reg() == a3);
//     masm.loadPtr(getAdjustedAddress(from), to.reg());
//   } else {
//     MOZ_ASSERT(from.isMemory());
//     MOZ_ASSERT(to.isMemory());
//     ScratchFloat32Scope fpscratch32(masm);
//     masm.loadFloat32(getAdjustedAddress(from), fpscratch32);
//     masm.storeFloat32(fpscratch32, getAdjustedAddress(to));
//   }
// }

// void MoveEmitterRiscv64::emitDoubleMove(const MoveOperand& from,
//                                         const MoveOperand& to) {
//   if (from.isFloatReg()) {
//     if (to.isFloatReg()) {
//       masm.moveDouble(from.floatReg(), to.floatReg());
//     } else if (to.isGeneralReg()) {
//       masm.moveFromDouble(from.floatReg(), to.reg());
//     } else {
//       MOZ_ASSERT(to.isMemory());
//       masm.storeDouble(from.floatReg(), getAdjustedAddress(to));
//     }
//   } else if (to.isFloatReg()) {
//     if (from.isMemory()) {
//       masm.loadDouble(getAdjustedAddress(from), to.floatReg());
//     } else {
//       masm.moveToDouble(from.reg(), to.floatReg());
//     }
//   } else {
//     MOZ_ASSERT(from.isMemory());
//     MOZ_ASSERT(to.isMemory());
//     ScratchDoubleScope fpscratch64(masm);
//     masm.loadDouble(getAdjustedAddress(from), fpscratch64);
//     masm.storeDouble(fpscratch64, getAdjustedAddress(to));
//   }
// }

void MoveEmitterRiscv64::assertDone() {
  MOZ_ASSERT(inCycle_ == 0);
}

void MoveEmitterRiscv64::finish() {
  assertDone();

  masm.freeStack(masm.framePushed() - pushedAtStart_);
}
