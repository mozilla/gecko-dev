/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/shared/MacroAssembler-x86-shared.h"

#include "jit/IonFrames.h"
#include "jit/IonMacroAssembler.h"

using namespace js;
using namespace js::jit;

void
MacroAssembler::PushRegsInMask(RegisterSet set)
{
    int32_t diffF = set.fpus().size() * sizeof(double);
    int32_t diffG = set.gprs().size() * sizeof(intptr_t);

    // On x86, always use push to push the integer registers, as it's fast
    // on modern hardware and it's a small instruction.
    for (GeneralRegisterBackwardIterator iter(set.gprs()); iter.more(); iter++) {
        diffG -= sizeof(intptr_t);
        Push(*iter);
    }
    JS_ASSERT(diffG == 0);

    reserveStack(diffF);
    for (FloatRegisterBackwardIterator iter(set.fpus()); iter.more(); iter++) {
        diffF -= sizeof(double);
        storeDouble(*iter, Address(StackPointer, diffF));
    }
    JS_ASSERT(diffF == 0);
}

void
MacroAssembler::PopRegsInMaskIgnore(RegisterSet set, RegisterSet ignore)
{
    int32_t diffG = set.gprs().size() * sizeof(intptr_t);
    int32_t diffF = set.fpus().size() * sizeof(double);
    const int32_t reservedG = diffG;
    const int32_t reservedF = diffF;

    for (FloatRegisterBackwardIterator iter(set.fpus()); iter.more(); iter++) {
        diffF -= sizeof(double);
        if (!ignore.has(*iter))
            loadDouble(Address(StackPointer, diffF), *iter);
    }
    freeStack(reservedF);
    JS_ASSERT(diffF == 0);

    // On x86, use pop to pop the integer registers, if we're not going to
    // ignore any slots, as it's fast on modern hardware and it's a small
    // instruction.
    if (ignore.empty(false)) {
        for (GeneralRegisterForwardIterator iter(set.gprs()); iter.more(); iter++) {
            diffG -= sizeof(intptr_t);
            Pop(*iter);
        }
    } else {
        for (GeneralRegisterBackwardIterator iter(set.gprs()); iter.more(); iter++) {
            diffG -= sizeof(intptr_t);
            if (!ignore.has(*iter))
                loadPtr(Address(StackPointer, diffG), *iter);
        }
        freeStack(reservedG);
    }
    JS_ASSERT(diffG == 0);
}

// Note: this function clobbers the input register.
void
MacroAssembler::clampDoubleToUint8(FloatRegister input, Register output)
{
    JS_ASSERT(input != ScratchDoubleReg);
    Label positive, done;

    // <= 0 or NaN --> 0
    zeroDouble(ScratchDoubleReg);
    branchDouble(DoubleGreaterThan, input, ScratchDoubleReg, &positive);
    {
        move32(Imm32(0), output);
        jump(&done);
    }

    bind(&positive);

    // Add 0.5 and truncate.
    loadConstantDouble(0.5, ScratchDoubleReg);
    addDouble(ScratchDoubleReg, input);

    Label outOfRange;

    // Truncate to int32 and ensure the result <= 255. This relies on the
    // processor setting output to a value > 255 for doubles outside the int32
    // range (for instance 0x80000000).
    cvttsd2si(input, output);
    branch32(Assembler::Above, output, Imm32(255), &outOfRange);
    {
        // Check if we had a tie.
        convertInt32ToDouble(output, ScratchDoubleReg);
        branchDouble(DoubleNotEqual, input, ScratchDoubleReg, &done);

        // It was a tie. Mask out the ones bit to get an even value.
        // See also js_TypedArray_uint8_clamp_double.
        and32(Imm32(~1), output);
        jump(&done);
    }

    // > 255 --> 255
    bind(&outOfRange);
    {
        move32(Imm32(255), output);
    }

    bind(&done);
}

// Builds an exit frame on the stack, with a return address to an internal
// non-function. Returns offset to be passed to markSafepointAt().
bool
MacroAssemblerX86Shared::buildFakeExitFrame(Register scratch, uint32_t *offset)
{
    mozilla::DebugOnly<uint32_t> initialDepth = framePushed();

    CodeLabel cl;
    mov(cl.dest(), scratch);

    uint32_t descriptor = MakeFrameDescriptor(framePushed(), JitFrame_IonJS);
    Push(Imm32(descriptor));
    Push(scratch);

    bind(cl.src());
    *offset = currentOffset();

    JS_ASSERT(framePushed() == initialDepth + IonExitFrameLayout::Size());
    return addCodeLabel(cl);
}

void
MacroAssemblerX86Shared::callWithExitFrame(JitCode *target)
{
    uint32_t descriptor = MakeFrameDescriptor(framePushed(), JitFrame_IonJS);
    Push(Imm32(descriptor));
    call(target);
}

bool
MacroAssemblerX86Shared::buildOOLFakeExitFrame(void *fakeReturnAddr)
{
    uint32_t descriptor = MakeFrameDescriptor(framePushed(), JitFrame_IonJS);
    Push(Imm32(descriptor));
    Push(ImmPtr(fakeReturnAddr));
    return true;
}

void
MacroAssemblerX86Shared::branchNegativeZero(FloatRegister reg,
                                            Register scratch,
                                            Label *label)
{
    // Determines whether the low double contained in the XMM register reg
    // is equal to -0.0.

#if defined(JS_CODEGEN_X86)
    Label nonZero;

    // Compare to zero. Lets through {0, -0}.
    xorpd(ScratchDoubleReg, ScratchDoubleReg);

    // If reg is non-zero, jump to nonZero.
    branchDouble(DoubleNotEqual, reg, ScratchDoubleReg, &nonZero);

    // Input register is either zero or negative zero. Retrieve sign of input.
    movmskpd(reg, scratch);

    // If reg is 1 or 3, input is negative zero.
    // If reg is 0 or 2, input is a normal zero.
    branchTest32(NonZero, scratch, Imm32(1), label);

    bind(&nonZero);
#elif defined(JS_CODEGEN_X64)
    movq(reg, scratch);
    cmpq(scratch, Imm32(1));
    j(Overflow, label);
#endif
}

void
MacroAssemblerX86Shared::branchNegativeZeroFloat32(FloatRegister reg,
                                                   Register scratch,
                                                   Label *label)
{
    movd(reg, scratch);
    cmpl(scratch, Imm32(1));
    j(Overflow, label);
}
