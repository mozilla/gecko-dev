/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Copyright (C) 2008 Apple Inc.
 * Copyright (C) 2009, 2010 University of Szeged
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef assembler_assembler_MacroAssemblerARM_h
#define assembler_assembler_MacroAssemblerARM_h

#include "assembler/wtf/Platform.h"

#if ENABLE_ASSEMBLER && WTF_CPU_ARM_TRADITIONAL

#include "assembler/assembler/ARMAssembler.h"
#include "assembler/assembler/AbstractMacroAssembler.h"

namespace JSC {

class MacroAssemblerARM : public AbstractMacroAssembler<ARMAssembler> {
    static const int DoubleConditionMask = 0x0f;
    static const int DoubleConditionBitSpecial = 0x8;
public:
    enum Condition {
        Equal = ARMAssembler::EQ,
        NotEqual = ARMAssembler::NE,
        Above = ARMAssembler::HI,
        AboveOrEqual = ARMAssembler::CS,
        Below = ARMAssembler::CC,
        BelowOrEqual = ARMAssembler::LS,
        GreaterThan = ARMAssembler::GT,
        GreaterThanOrEqual = ARMAssembler::GE,
        LessThan = ARMAssembler::LT,
        LessThanOrEqual = ARMAssembler::LE,
        Overflow = ARMAssembler::VS,
        Signed = ARMAssembler::MI,
        Zero = ARMAssembler::EQ,
        NonZero = ARMAssembler::NE
    };

    enum DoubleCondition {
        // These conditions will only evaluate to true if the comparison is ordered - i.e. neither operand is NaN.
        DoubleEqual = ARMAssembler::EQ,
        DoubleNotEqual = ARMAssembler::NE | DoubleConditionBitSpecial,
        DoubleGreaterThan = ARMAssembler::GT,
        DoubleGreaterThanOrEqual = ARMAssembler::GE,
        DoubleLessThan = ARMAssembler::CC,
        DoubleLessThanOrEqual = ARMAssembler::LS,
        // If either operand is NaN, these conditions always evaluate to true.
        DoubleEqualOrUnordered = ARMAssembler::EQ | DoubleConditionBitSpecial,
        DoubleNotEqualOrUnordered = ARMAssembler::NE,
        DoubleGreaterThanOrUnordered = ARMAssembler::HI,
        DoubleGreaterThanOrEqualOrUnordered = ARMAssembler::CS,
        DoubleLessThanOrUnordered = ARMAssembler::LT,
        DoubleLessThanOrEqualOrUnordered = ARMAssembler::LE
    };

    static const RegisterID stackPointerRegister = ARMRegisters::sp;
    static const RegisterID linkRegister = ARMRegisters::lr;

    static const Scale ScalePtr = TimesFour;
    static const unsigned int TotalRegisters = 16;

    void add32(RegisterID src, RegisterID dest)
    {
        m_assembler.adds_r(dest, dest, src);
    }

    void add32(TrustedImm32 imm, Address address)
    {
        load32(address, ARMRegisters::S1);
        add32(imm, ARMRegisters::S1);
        store32(ARMRegisters::S1, address);
    }

    void add32(TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.adds_r(dest, dest, m_assembler.getImm(imm.m_value, ARMRegisters::S0));
    }

    void add32(Address src, RegisterID dest)
    {
        load32(src, ARMRegisters::S1);
        add32(ARMRegisters::S1, dest);
    }

    void and32(Address src, RegisterID dest)
    {
        load32(src, ARMRegisters::S1);
        and32(ARMRegisters::S1, dest);
    }

    void and32(RegisterID src, RegisterID dest)
    {
        m_assembler.ands_r(dest, dest, src);
    }

    void and32(Imm32 imm, RegisterID dest)
    {
        ARMWord w = m_assembler.getImm(imm.m_value, ARMRegisters::S0, true);
        if (w & ARMAssembler::OP2_INV_IMM)
            m_assembler.bics_r(dest, dest, w & ~ARMAssembler::OP2_INV_IMM);
        else
            m_assembler.ands_r(dest, dest, w);
    }

    void lshift32(RegisterID shift_amount, RegisterID dest)
    {
        ARMWord w = ARMAssembler::getOp2(0x1f);
        ASSERT(w != ARMAssembler::INVALID_IMM);
        m_assembler.and_r(ARMRegisters::S0, shift_amount, w);

        m_assembler.movs_r(dest, m_assembler.lsl_r(dest, ARMRegisters::S0));
    }

    void lshift32(Imm32 imm, RegisterID dest)
    {
        m_assembler.movs_r(dest, m_assembler.lsl(dest, imm.m_value & 0x1f));
    }

    void mul32(RegisterID src, RegisterID dest)
    {
        if (src == dest) {
            move(src, ARMRegisters::S0);
            src = ARMRegisters::S0;
        }
        m_assembler.muls_r(dest, dest, src);
    }

    void mul32(Imm32 imm, RegisterID src, RegisterID dest)
    {
        move(imm, ARMRegisters::S0);
        m_assembler.muls_r(dest, src, ARMRegisters::S0);
    }

    void neg32(RegisterID srcDest)
    {
        m_assembler.rsbs_r(srcDest, srcDest, ARMAssembler::getOp2(0));
    }

    void not32(RegisterID dest)
    {
        m_assembler.mvns_r(dest, dest);
    }

    void or32(RegisterID src, RegisterID dest)
    {
        m_assembler.orrs_r(dest, dest, src);
    }

    void or32(TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.orrs_r(dest, dest, m_assembler.getImm(imm.m_value, ARMRegisters::S0));
    }

    void rshift32(RegisterID shift_amount, RegisterID dest)
    {
        ARMWord w = ARMAssembler::getOp2(0x1f);
        ASSERT(w != ARMAssembler::INVALID_IMM);
        m_assembler.and_r(ARMRegisters::S0, shift_amount, w);

        m_assembler.movs_r(dest, m_assembler.asr_r(dest, ARMRegisters::S0));
    }

    void rshift32(Imm32 imm, RegisterID dest)
    {
        m_assembler.movs_r(dest, m_assembler.asr(dest, imm.m_value & 0x1f));
    }

    void urshift32(RegisterID shift_amount, RegisterID dest)
    {
        ARMWord w = ARMAssembler::getOp2(0x1f);
        ASSERT(w != ARMAssembler::INVALID_IMM);
        m_assembler.and_r(ARMRegisters::S0, shift_amount, w);

        m_assembler.movs_r(dest, m_assembler.lsr_r(dest, ARMRegisters::S0));
    }

    void urshift32(Imm32 imm, RegisterID dest)
    {
        m_assembler.movs_r(dest, m_assembler.lsr(dest, imm.m_value & 0x1f));
    }

    void sub32(RegisterID src, RegisterID dest)
    {
        m_assembler.subs_r(dest, dest, src);
    }

    void sub32(TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.subs_r(dest, dest, m_assembler.getImm(imm.m_value, ARMRegisters::S0));
    }

    void sub32(TrustedImm32 imm, Address address)
    {
        load32(address, ARMRegisters::S1);
        sub32(imm, ARMRegisters::S1);
        store32(ARMRegisters::S1, address);
    }

    void sub32(Address src, RegisterID dest)
    {
        load32(src, ARMRegisters::S1);
        sub32(ARMRegisters::S1, dest);
    }

    void or32(Address address, RegisterID dest)
    {
        load32(address, ARMRegisters::S1);
        or32(ARMRegisters::S1, dest);
    }

    void xor32(RegisterID src, RegisterID dest)
    {
        m_assembler.eors_r(dest, dest, src);
    }

    void xor32(TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.eors_r(dest, dest, m_assembler.getImm(imm.m_value, ARMRegisters::S0));
    }

    void xor32(Address src, RegisterID dest)
    {
        load32(src, ARMRegisters::S1);
        m_assembler.eors_r(dest, dest, ARMRegisters::S1);
    }

    void load8(BaseIndex address, RegisterID dest)
    {
        load8ZeroExtend(address, dest);
    }

    void load8SignExtend(ImplicitAddress address, RegisterID dest)
    {
        m_assembler.dataTransferN(true, true, 8, dest, address.base, address.offset);
    }

    void load8ZeroExtend(ImplicitAddress address, RegisterID dest)
    {
        m_assembler.dataTransferN(true, false, 8, dest, address.base, address.offset);
    }

    void load8SignExtend(BaseIndex address, RegisterID dest)
    {
        m_assembler.baseIndexTransferN(true, true, 8, dest,
                                       address.base, address.index, address.scale, address.offset);
    }

    void load8ZeroExtend(BaseIndex address, RegisterID dest)
    {
        m_assembler.baseIndexTransferN(true, false, 8, dest,
                                       address.base, address.index, address.scale, address.offset);
    }

    /* this is *identical* to the zero extending case*/
    void load8(ImplicitAddress address, RegisterID dest)
    {
        load8ZeroExtend(address, dest);
    }

    void load16Unaligned(BaseIndex address, RegisterID dest)
    {
        load16(address, dest);
    }

    void load16SignExtend(ImplicitAddress address, RegisterID dest)
    {
        m_assembler.dataTransferN(true, true, 16, dest, address.base, address.offset);
    }

    void load16ZeroExtend(ImplicitAddress address, RegisterID dest)
    {
        m_assembler.dataTransferN(true, false, 16, dest, address.base, address.offset);
    }
    void load16SignExtend(BaseIndex address, RegisterID dest)
    {
        m_assembler.baseIndexTransferN(true, true, 16, dest,
                                       address.base, address.index, address.scale, address.offset);
    }
    void load16ZeroExtend(BaseIndex address, RegisterID dest)
    {
        m_assembler.baseIndexTransferN(true, false, 16, dest,
                                       address.base, address.index, address.scale, address.offset);
    }

    void load32(ImplicitAddress address, RegisterID dest)
    {
        m_assembler.dataTransfer32(true, dest, address.base, address.offset);
    }

    void load32(BaseIndex address, RegisterID dest)
    {
        m_assembler.baseIndexTransfer32(true, dest, address.base, address.index, static_cast<int>(address.scale), address.offset);
    }

#if WTF_CPU_ARMV5_OR_LOWER
    void load32WithUnalignedHalfWords(BaseIndex address, RegisterID dest);
#else
    void load32WithUnalignedHalfWords(BaseIndex address, RegisterID dest)
    {
        load32(address, dest);
    }
#endif

    DataLabel32 load32WithAddressOffsetPatch(Address address, RegisterID dest)
    {
        ASSERT(address.base != ARMRegisters::S0);
        DataLabel32 dataLabel(this);
        m_assembler.ldr_un_imm(ARMRegisters::S0, 0);
        m_assembler.dtr_ur(true, dest, address.base, ARMRegisters::S0);
        return dataLabel;
    }

    DataLabel32 load64WithAddressOffsetPatch(Address address, RegisterID hi, RegisterID lo)
    {
        ASSERT(address.base != ARMRegisters::S0);
        ASSERT(lo != ARMRegisters::S0);
        DataLabel32 dataLabel(this);
        m_assembler.ldr_un_imm(ARMRegisters::S0, 0);
        m_assembler.add_r(ARMRegisters::S0, ARMRegisters::S0, address.base);
        m_assembler.dtr_u(true, lo, ARMRegisters::S0, 0);
        m_assembler.dtr_u(true, hi, ARMRegisters::S0, 4);
        return dataLabel;
    }

    Label loadPtrWithPatchToLEA(Address address, RegisterID dest)
    {
        Label label(this);
        load32(address, dest);
        return label;
    }

    void load16(BaseIndex address, RegisterID dest)
    {
        m_assembler.add_r(ARMRegisters::S1, address.base, m_assembler.lsl(address.index, address.scale));
        load16(Address(ARMRegisters::S1, address.offset), dest);
    }

    void load16(ImplicitAddress address, RegisterID dest)
    {
        if (address.offset >= 0)
            m_assembler.ldrh_u(dest, address.base, m_assembler.getOffsetForHalfwordDataTransfer(address.offset, ARMRegisters::S0));
        else
            m_assembler.ldrh_d(dest, address.base, m_assembler.getOffsetForHalfwordDataTransfer(-address.offset, ARMRegisters::S0));
    }

    DataLabel32 store32WithAddressOffsetPatch(RegisterID src, Address address)
    {
        ASSERT(address.base != ARMRegisters::S0);
        DataLabel32 dataLabel(this);
        m_assembler.ldr_un_imm(ARMRegisters::S0, 0);
        m_assembler.dtr_ur(false, src, address.base, ARMRegisters::S0);
        return dataLabel;
    }

    DataLabel32 store64WithAddressOffsetPatch(RegisterID hi, RegisterID lo, Address address)
    {
        ASSERT(hi != ARMRegisters::S0);
        ASSERT(lo != ARMRegisters::S0);
        ASSERT(address.base != ARMRegisters::S0);
        DataLabel32 dataLabel(this);
        m_assembler.ldr_un_imm(ARMRegisters::S0, address.offset);
        m_assembler.add_r(ARMRegisters::S0, ARMRegisters::S0, address.base);
        m_assembler.dtr_u(false, lo, ARMRegisters::S0, 0);
        m_assembler.dtr_u(false, hi, ARMRegisters::S0, 4);
        return dataLabel;
    }

    DataLabel32 store64WithAddressOffsetPatch(Imm32 hi, RegisterID lo, Address address)
    {
        ASSERT(lo != ARMRegisters::S0);
        ASSERT(lo != ARMRegisters::S1);
        ASSERT(lo != address.base);
        ASSERT(address.base != ARMRegisters::S0);
        ASSERT(address.base != ARMRegisters::S1);
        DataLabel32 dataLabel(this);
        m_assembler.ldr_un_imm(ARMRegisters::S0, address.offset);
        m_assembler.moveImm(hi.m_value, ARMRegisters::S1);
        m_assembler.add_r(ARMRegisters::S0, ARMRegisters::S0, address.base);
        m_assembler.dtr_u(false, lo, ARMRegisters::S0, 0);
        m_assembler.dtr_u(false, ARMRegisters::S1, ARMRegisters::S0, 4);
        return dataLabel;
    }

    DataLabel32 store64WithAddressOffsetPatch(Imm32 hi, Imm32 lo, Address address)
    {
        ASSERT(address.base != ARMRegisters::S0);
        ASSERT(address.base != ARMRegisters::S1);
        DataLabel32 dataLabel(this);
        m_assembler.ldr_un_imm(ARMRegisters::S0, address.offset);
        m_assembler.add_r(ARMRegisters::S0, ARMRegisters::S0, address.base);
        m_assembler.moveImm(lo.m_value, ARMRegisters::S1);
        m_assembler.dtr_u(false, ARMRegisters::S1, ARMRegisters::S0, 0);
        /* TODO: improve this by getting another scratch register. */
        m_assembler.moveImm(hi.m_value, ARMRegisters::S1);
        m_assembler.dtr_u(false, ARMRegisters::S1, ARMRegisters::S0, 4);
        return dataLabel;
    }

    void store32(RegisterID src, ImplicitAddress address)
    {
        m_assembler.dataTransfer32(false, src, address.base, address.offset);
    }

    void store32(RegisterID src, BaseIndex address)
    {
        m_assembler.baseIndexTransfer32(false, src, address.base, address.index, static_cast<int>(address.scale), address.offset);
    }

    void store32(TrustedImm32 imm, BaseIndex address)
    {
        if (imm.m_isPointer)
            m_assembler.ldr_un_imm(ARMRegisters::S1, imm.m_value);
        else
            move(imm, ARMRegisters::S1);
        store32(ARMRegisters::S1, address);
    }

    void store32(TrustedImm32 imm, ImplicitAddress address)
    {
        if (imm.m_isPointer)
            m_assembler.ldr_un_imm(ARMRegisters::S1, imm.m_value);
        else
            move(imm, ARMRegisters::S1);
        store32(ARMRegisters::S1, address);
    }

    void store32(RegisterID src, void* address)
    {
        m_assembler.ldr_un_imm(ARMRegisters::S0, reinterpret_cast<ARMWord>(address));
        m_assembler.dtr_u(false, src, ARMRegisters::S0, 0);
    }

    void store32(TrustedImm32 imm, void* address)
    {
        m_assembler.ldr_un_imm(ARMRegisters::S0, reinterpret_cast<ARMWord>(address));
        if (imm.m_isPointer)
            m_assembler.ldr_un_imm(ARMRegisters::S1, imm.m_value);
        else
            m_assembler.moveImm(imm.m_value, ARMRegisters::S1);
        m_assembler.dtr_u(false, ARMRegisters::S1, ARMRegisters::S0, 0);
    }

    void store16(RegisterID src, ImplicitAddress address)
    {
        m_assembler.dataTransferN(false, false, 16,  src, address.base, address.offset);
    }
    void store16(RegisterID src, BaseIndex address)
    {
        m_assembler.baseIndexTransferN(false, false, 16, src, address.base, address.index, static_cast<int>(address.scale), address.offset);
    }

    void store16(TrustedImm32 imm, BaseIndex address)
    {
        if (imm.m_isPointer)
            MOZ_ASSUME_UNREACHABLE("What are you trying to do with 16 bits of a pointer?");
        else
            move(imm, ARMRegisters::S1);
        store16(ARMRegisters::S1, address);
    }
    void store16(TrustedImm32 imm, ImplicitAddress address)
    {
        if (imm.m_isPointer)
            MOZ_ASSUME_UNREACHABLE("What are you trying to do with 16 bits of a pointer?");
        else
            move(imm, ARMRegisters::S1);
        store16(ARMRegisters::S1, address);
    }

    void store16(RegisterID src, void* address)
    {
        m_assembler.ldr_un_imm(ARMRegisters::S0, reinterpret_cast<ARMWord>(address));
        m_assembler.mem_imm_off(false, false, 16, true, src, ARMRegisters::S0, 0);
    }

    void store16(TrustedImm32 imm, void* address)
    {
        m_assembler.ldr_un_imm(ARMRegisters::S0, reinterpret_cast<ARMWord>(address));
        if (imm.m_isPointer)
            MOZ_ASSUME_UNREACHABLE("What are you trying to do with 16 bits of a pointer?");
        else
            m_assembler.moveImm(imm.m_value, ARMRegisters::S1);
        m_assembler.mem_imm_off(false, false, 16, true, ARMRegisters::S1, ARMRegisters::S0, 0);
    }

    void store8(RegisterID src, ImplicitAddress address)
    {
        m_assembler.dataTransferN(false, false, 8,  src, address.base, address.offset);
    }

    void store8(RegisterID src, BaseIndex address)
    {
        m_assembler.baseIndexTransferN(false, false, 8, src, address.base, address.index, static_cast<int>(address.scale), address.offset);
    }

    void store8(TrustedImm32 imm, BaseIndex address)
    {
        if (imm.m_isPointer)
            MOZ_ASSUME_UNREACHABLE("What are you trying to do with 8 bits of a pointer?");
        else
            move(imm, ARMRegisters::S1);
        store8(ARMRegisters::S1, address);
    }

    void store8(TrustedImm32 imm, ImplicitAddress address)
    {
        if (imm.m_isPointer)
            MOZ_ASSUME_UNREACHABLE("What are you trying to do with 16 bits of a pointer?");
        else
            move(imm, ARMRegisters::S1);
        store8(ARMRegisters::S1, address);
    }

    void store8(RegisterID src, void* address)
    {
        m_assembler.ldr_un_imm(ARMRegisters::S0, reinterpret_cast<ARMWord>(address));
        m_assembler.mem_imm_off(false, false, 8, true, src, ARMRegisters::S0, 0);
    }

    void store8(TrustedImm32 imm, void* address)
    {
        m_assembler.ldr_un_imm(ARMRegisters::S0, reinterpret_cast<ARMWord>(address));
        if (imm.m_isPointer)
            MOZ_ASSUME_UNREACHABLE("What are you trying to do with 16 bits of a pointer?");
        else
            m_assembler.moveImm(imm.m_value, ARMRegisters::S1);
        m_assembler.mem_imm_off(false, false, 8, true, ARMRegisters::S1, ARMRegisters::S0, 0);
    }

    void pop(RegisterID dest)
    {
        m_assembler.pop_r(dest);
    }

    void push(RegisterID src)
    {
        m_assembler.push_r(src);
    }

    void push(Address address)
    {
        load32(address, ARMRegisters::S1);
        push(ARMRegisters::S1);
    }

    void push(Imm32 imm)
    {
        move(imm, ARMRegisters::S0);
        push(ARMRegisters::S0);
    }

    void move(TrustedImm32 imm, RegisterID dest)
    {
        if (imm.m_isPointer)
            m_assembler.ldr_un_imm(dest, imm.m_value);
        else
            m_assembler.moveImm(imm.m_value, dest);
    }

    void move(RegisterID src, RegisterID dest)
    {
        m_assembler.mov_r(dest, src);
    }

    void move(TrustedImmPtr imm, RegisterID dest)
    {
        move(Imm32(imm), dest);
    }

    void swap(RegisterID reg1, RegisterID reg2)
    {
        m_assembler.mov_r(ARMRegisters::S0, reg1);
        m_assembler.mov_r(reg1, reg2);
        m_assembler.mov_r(reg2, ARMRegisters::S0);
    }

    void signExtend32ToPtr(RegisterID src, RegisterID dest)
    {
        if (src != dest)
            move(src, dest);
    }

    void zeroExtend32ToPtr(RegisterID src, RegisterID dest)
    {
        if (src != dest)
            move(src, dest);
    }

    Jump branch8(Condition cond, Address left, Imm32 right)
    {
        load8(left, ARMRegisters::S1);
        return branch32(cond, ARMRegisters::S1, right);
    }

    Jump branch32(Condition cond, RegisterID left, RegisterID right, int useConstantPool = 0)
    {
        m_assembler.cmp_r(left, right);
        return Jump(m_assembler.jmp(ARMCondition(cond), useConstantPool));
    }

    Jump branch32(Condition cond, RegisterID left, TrustedImm32 right, int useConstantPool = 0)
    {
        ASSERT(left != ARMRegisters::S0);
        if (right.m_isPointer) {
            m_assembler.ldr_un_imm(ARMRegisters::S0, right.m_value);
            m_assembler.cmp_r(left, ARMRegisters::S0);
        } else {
            // This is a rather cute (if not confusing) pattern.
            // unfortunately, it is not quite conducive to switching from
            // cmp to cmn, so I'm doing so manually.
            // m_assembler.cmp_r(left, m_assembler.getImm(right.m_value, ARMRegisters::S0));

            // try to shoehorn the immediate into the compare instruction
            ARMWord arg = m_assembler.getOp2(right.m_value);
            if (arg != m_assembler.INVALID_IMM) {
                m_assembler.cmp_r(left, arg);
            } else {
                // if it does not fit, try to shoehorn a negative in, and use a negated compare
                // p.s. why couldn't arm just include the sign bit in the imm, rather than the inst.
                arg = m_assembler.getOp2(-right.m_value);
                if (arg != m_assembler.INVALID_IMM) {
                    m_assembler.cmn_r(left, arg);
                } else {
                    // If we get here, we *need* to use a temp register and any way of loading a value
                    // will enable us to load a negative easily, so there is no reason to switch from
                    // cmp to cmn.
                    m_assembler.cmp_r(left, m_assembler.getImm(right.m_value, ARMRegisters::S0));
                }
            }
        }
        return Jump(m_assembler.jmp(ARMCondition(cond), useConstantPool));
    }

    // Like branch32, but emit a consistently-structured sequence such that the
    // number of instructions emitted is constant, regardless of the argument
    // values. For ARM, this is identical to branch32WithPatch, except that it
    // does not generate a DataLabel32.
    Jump branch32FixedLength(Condition cond, RegisterID left, TrustedImm32 right)
    {
        m_assembler.ldr_un_imm(ARMRegisters::S1, right.m_value);
        return branch32(cond, left, ARMRegisters::S1, true);
    }

    // As branch32_force32, but allow the value ('right') to be patched.
    Jump branch32WithPatch(Condition cond, RegisterID left, TrustedImm32 right, DataLabel32 &dataLabel)
    {
        ASSERT(left != ARMRegisters::S1);
        dataLabel = moveWithPatch(right, ARMRegisters::S1);
        return branch32(cond, left, ARMRegisters::S1, true);
    }

    Jump branch32WithPatch(Condition cond, Address left, TrustedImm32 right, DataLabel32 &dataLabel)
    {
        ASSERT(left.base != ARMRegisters::S1);
        load32(left, ARMRegisters::S1);
        dataLabel = moveWithPatch(right, ARMRegisters::S0);
        return branch32(cond, ARMRegisters::S1, ARMRegisters::S0, true);
    }

    Jump branch32(Condition cond, RegisterID left, Address right)
    {
        /*If the load only takes a single instruction, then we could just do a load into*/
        load32(right, ARMRegisters::S1);
        return branch32(cond, left, ARMRegisters::S1);
    }

    Jump branch32(Condition cond, Address left, RegisterID right)
    {
        load32(left, ARMRegisters::S1);
        return branch32(cond, ARMRegisters::S1, right);
    }

    Jump branch32(Condition cond, Address left, TrustedImm32 right)
    {
        load32(left, ARMRegisters::S1);
        return branch32(cond, ARMRegisters::S1, right);
    }

    Jump branch32(Condition cond, BaseIndex left, TrustedImm32 right)
    {
        load32(left, ARMRegisters::S1);
        return branch32(cond, ARMRegisters::S1, right);
    }

    Jump branch32WithUnalignedHalfWords(Condition cond, BaseIndex left, TrustedImm32 right)
    {
        load32WithUnalignedHalfWords(left, ARMRegisters::S1);
        return branch32(cond, ARMRegisters::S1, right);
    }

    Jump branch16(Condition cond, BaseIndex left, RegisterID right)
    {
        (void)(cond);
        (void)(left);
        (void)(right);
        ASSERT_NOT_REACHED();
        return jump();
    }

    Jump branch16(Condition cond, BaseIndex left, Imm32 right)
    {
        load16(left, ARMRegisters::S0);
        move(right, ARMRegisters::S1);
        m_assembler.cmp_r(ARMRegisters::S0, ARMRegisters::S1);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchTest8(Condition cond, Address address, Imm32 mask = Imm32(-1))
    {
        load8(address, ARMRegisters::S1);
        return branchTest32(cond, ARMRegisters::S1, mask);
    }

    Jump branchTest32(Condition cond, RegisterID reg, RegisterID mask)
    {
        ASSERT((cond == Zero) || (cond == NonZero));
        m_assembler.tst_r(reg, mask);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchTest32(Condition cond, RegisterID reg, Imm32 mask = Imm32(-1))
    {
        ASSERT((cond == Zero) || (cond == NonZero));
        ARMWord w = m_assembler.getImm(mask.m_value, ARMRegisters::S0, true);
        if (w & ARMAssembler::OP2_INV_IMM)
            m_assembler.bics_r(ARMRegisters::S0, reg, w & ~ARMAssembler::OP2_INV_IMM);
        else
            m_assembler.tst_r(reg, w);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchTest32(Condition cond, Address address, Imm32 mask = Imm32(-1))
    {
        load32(address, ARMRegisters::S1);
        return branchTest32(cond, ARMRegisters::S1, mask);
    }

    Jump branchTest32(Condition cond, BaseIndex address, Imm32 mask = Imm32(-1))
    {
        load32(address, ARMRegisters::S1);
        return branchTest32(cond, ARMRegisters::S1, mask);
    }

    Jump jump()
    {
        return Jump(m_assembler.jmp());
    }

    void jump(RegisterID target)
    {
        m_assembler.bx(target);
    }

    void jump(Address address)
    {
        load32(address, ARMRegisters::pc);
    }

    Jump branchAdd32(Condition cond, RegisterID src, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        add32(src, dest);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchAdd32(Condition cond, Imm32 imm, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        add32(imm, dest);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchAdd32(Condition cond, Address src, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        add32(src, dest);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    void mull32(RegisterID src1, RegisterID src2, RegisterID dest)
    {
        if (src1 == dest) {
            move(src1, ARMRegisters::S0);
            src1 = ARMRegisters::S0;
        }
        m_assembler.mull_r(ARMRegisters::S1, dest, src2, src1);
        m_assembler.cmp_r(ARMRegisters::S1, m_assembler.asr(dest, 31));
    }

    Jump branchMul32(Condition cond, RegisterID src, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        if (cond == Overflow) {
            mull32(src, dest, dest);
            cond = NonZero;
        }
        else
            mul32(src, dest);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchMul32(Condition cond, Imm32 imm, RegisterID src, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        if (cond == Overflow) {
            move(imm, ARMRegisters::S0);
            mull32(ARMRegisters::S0, src, dest);
            cond = NonZero;
        }
        else
            mul32(imm, src, dest);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchSub32(Condition cond, RegisterID src, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        sub32(src, dest);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchSub32(Condition cond, Imm32 imm, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        sub32(imm, dest);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchSub32(Condition cond, Address src, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        sub32(src, dest);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchSub32(Condition cond, Imm32 imm, Address dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        sub32(imm, dest);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchNeg32(Condition cond, RegisterID srcDest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        neg32(srcDest);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchOr32(Condition cond, RegisterID src, RegisterID dest)
    {
        ASSERT((cond == Signed) || (cond == Zero) || (cond == NonZero));
        or32(src, dest);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    // Encode a NOP using "MOV rX, rX", where 'X' is defined by 'tag', and is
    // in the range r0-r14.
    void nop(int tag)
    {
        ASSERT((tag >= 0) && (tag <= 14));
        m_assembler.mov_r(tag, tag);
    }

    void breakpoint()
    {
        m_assembler.bkpt(0);
    }

    Call nearCall()
    {
#if WTF_ARM_ARCH_VERSION >= 5
        Call    call(m_assembler.loadBranchTarget(ARMRegisters::S1, ARMAssembler::AL, true), Call::LinkableNear);
        m_assembler.blx(ARMRegisters::S1);
        return call;
#else
        prepareCall();
        return Call(m_assembler.jmp(ARMAssembler::AL, true), Call::LinkableNear);
#endif
    }

    Call call(RegisterID target)
    {
        m_assembler.blx(target);
        JmpSrc jmpSrc;
        return Call(jmpSrc, Call::None);
    }

    void call(Address address)
    {
        call32(address.base, address.offset);
    }

    void ret()
    {
        m_assembler.bx(linkRegister);
    }

    void set32(Condition cond, Address left, RegisterID right, RegisterID dest)
    {
        load32(left, ARMRegisters::S1);
        set32(cond, ARMRegisters::S1, right, dest);
    }

    void set32(Condition cond, RegisterID left, Address right, RegisterID dest)
    {
        load32(right, ARMRegisters::S1);
        set32(cond, left, ARMRegisters::S1, dest);
    }

    void set32(Condition cond, RegisterID left, RegisterID right, RegisterID dest)
    {
        m_assembler.cmp_r(left, right);
        m_assembler.mov_r(dest, ARMAssembler::getOp2(0));
        m_assembler.mov_r(dest, ARMAssembler::getOp2(1), ARMCondition(cond));
    }

    void set32(Condition cond, RegisterID left, Imm32 right, RegisterID dest)
    {
        m_assembler.cmp_r(left, m_assembler.getImm(right.m_value, ARMRegisters::S0));
        m_assembler.mov_r(dest, ARMAssembler::getOp2(0));
        m_assembler.mov_r(dest, ARMAssembler::getOp2(1), ARMCondition(cond));
    }

    void set32(Condition cond, Address left, Imm32 right, RegisterID dest)
    {
        load32(left, ARMRegisters::S1);
        set32(cond, ARMRegisters::S1, right, dest);
    }

    void set8(Condition cond, RegisterID left, RegisterID right, RegisterID dest)
    {
        // ARM doesn't have byte registers
        set32(cond, left, right, dest);
    }

    void set8(Condition cond, Address left, RegisterID right, RegisterID dest)
    {
        // ARM doesn't have byte registers
        load32(left, ARMRegisters::S1);
        set32(cond, ARMRegisters::S1, right, dest);
    }

    void set8(Condition cond, RegisterID left, Imm32 right, RegisterID dest)
    {
        // ARM doesn't have byte registers
        set32(cond, left, right, dest);
    }

    void setTest32(Condition cond, Address address, Imm32 mask, RegisterID dest)
    {
        load32(address, ARMRegisters::S1);
        if (mask.m_value == -1)
            m_assembler.cmp_r(0, ARMRegisters::S1);
        else
            m_assembler.tst_r(ARMRegisters::S1, m_assembler.getImm(mask.m_value, ARMRegisters::S0));
        m_assembler.mov_r(dest, ARMAssembler::getOp2(0));
        m_assembler.mov_r(dest, ARMAssembler::getOp2(1), ARMCondition(cond));
    }

    void setTest8(Condition cond, Address address, Imm32 mask, RegisterID dest)
    {
        // ARM doesn't have byte registers
        setTest32(cond, address, mask, dest);
    }

    void add32(TrustedImm32 imm, RegisterID src, RegisterID dest)
    {
        m_assembler.add_r(dest, src, m_assembler.getImm(imm.m_value, ARMRegisters::S0));
    }

    void lea(Address address, RegisterID dest)
    {
        m_assembler.add_r(dest, address.base, m_assembler.getImm(address.offset, ARMRegisters::S0));
    }

    void lea(BaseIndex address, RegisterID dest)
    {
        /* This could be better? */
        move(address.index, ARMRegisters::S1);
        if (address.scale != 0)
            lshift32(Imm32(address.scale), ARMRegisters::S1);
        if (address.offset)
            add32(Imm32(address.offset), ARMRegisters::S1);
        add32(address.base, ARMRegisters::S1);
        move(ARMRegisters::S1, dest);
    }

    void add32(TrustedImm32 imm, AbsoluteAddress address)
    {
        m_assembler.ldr_un_imm(ARMRegisters::S1, reinterpret_cast<ARMWord>(address.m_ptr));
        m_assembler.dtr_u(true, ARMRegisters::S1, ARMRegisters::S1, 0);
        add32(imm, ARMRegisters::S1);
        m_assembler.ldr_un_imm(ARMRegisters::S0, reinterpret_cast<ARMWord>(address.m_ptr));
        m_assembler.dtr_u(false, ARMRegisters::S1, ARMRegisters::S0, 0);
    }

    void sub32(TrustedImm32 imm, AbsoluteAddress address)
    {
        m_assembler.ldr_un_imm(ARMRegisters::S1, reinterpret_cast<ARMWord>(address.m_ptr));
        m_assembler.dtr_u(true, ARMRegisters::S1, ARMRegisters::S1, 0);
        sub32(imm, ARMRegisters::S1);
        m_assembler.ldr_un_imm(ARMRegisters::S0, reinterpret_cast<ARMWord>(address.m_ptr));
        m_assembler.dtr_u(false, ARMRegisters::S1, ARMRegisters::S0, 0);
    }

    void load32(const void* address, RegisterID dest)
    {
        m_assembler.ldr_un_imm(ARMRegisters::S0, reinterpret_cast<ARMWord>(address));
        m_assembler.dtr_u(true, dest, ARMRegisters::S0, 0);
    }

    Jump branch32(Condition cond, AbsoluteAddress left, RegisterID right)
    {
        load32(left.m_ptr, ARMRegisters::S1);
        return branch32(cond, ARMRegisters::S1, right);
    }

    Jump branch32(Condition cond, AbsoluteAddress left, TrustedImm32 right)
    {
        load32(left.m_ptr, ARMRegisters::S1);
        return branch32(cond, ARMRegisters::S1, right);
    }

    Call call()
    {
#if WTF_ARM_ARCH_VERSION >= 5
        Call    call(m_assembler.loadBranchTarget(ARMRegisters::S1, ARMAssembler::AL, true), Call::Linkable);
        m_assembler.blx(ARMRegisters::S1);
        return call;
#else
        prepareCall();
        return Call(m_assembler.jmp(ARMAssembler::AL, true), Call::Linkable);
#endif
    }

    Call tailRecursiveCall()
    {
        return Call::fromTailJump(jump());
    }

    Call makeTailRecursiveCall(Jump oldJump)
    {
        return Call::fromTailJump(oldJump);
    }

    DataLabelPtr moveWithPatch(TrustedImmPtr initialValue, RegisterID dest)
    {
        DataLabelPtr dataLabel(this);
        m_assembler.ldr_un_imm(dest, reinterpret_cast<ARMWord>(initialValue.m_value));
        return dataLabel;
    }

    DataLabel32 moveWithPatch(TrustedImm32 initialValue, RegisterID dest)
    {
        DataLabel32 dataLabel(this);
        m_assembler.ldr_un_imm(dest, initialValue.m_value);
        return dataLabel;
    }

    Jump branchPtrWithPatch(Condition cond, RegisterID left, DataLabelPtr& dataLabel, ImmPtr initialRightValue = ImmPtr(0))
    {
        dataLabel = moveWithPatch(initialRightValue, ARMRegisters::S1);
        Jump jump = branch32(cond, left, ARMRegisters::S1, true);
        return jump;
    }

    Jump branchPtrWithPatch(Condition cond, Address left, DataLabelPtr& dataLabel, ImmPtr initialRightValue = ImmPtr(0))
    {
        load32(left, ARMRegisters::S1);
        dataLabel = moveWithPatch(initialRightValue, ARMRegisters::S0);
        Jump jump = branch32(cond, ARMRegisters::S0, ARMRegisters::S1, true);
        return jump;
    }

    DataLabelPtr storePtrWithPatch(TrustedImmPtr initialValue, ImplicitAddress address)
    {
        DataLabelPtr dataLabel = moveWithPatch(initialValue, ARMRegisters::S1);
        store32(ARMRegisters::S1, address);
        return dataLabel;
    }

    DataLabelPtr storePtrWithPatch(ImplicitAddress address)
    {
        return storePtrWithPatch(ImmPtr(0), address);
    }

    // Floating point operators
    static bool supportsFloatingPoint()
    {
        return s_isVFPPresent;
    }

    static bool supportsFloatingPointTruncate()
    {
        return true;
    }

    static bool supportsFloatingPointSqrt()
    {
        return s_isVFPPresent;
    }

    void moveDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.fcpyd_r(dest, src);
    }

    void loadDouble(ImplicitAddress address, FPRegisterID dest)
    {
        // Load a double from base+offset.
        m_assembler.doubleTransfer(true, dest, address.base, address.offset);
    }

    void loadDouble(BaseIndex address, FPRegisterID dest)
    {
        m_assembler.baseIndexFloatTransfer(true, true, dest,
                                           address.base, address.index,
                                           address.scale, address.offset);
    }

    DataLabelPtr loadDouble(const void* address, FPRegisterID dest)
    {
        DataLabelPtr label = moveWithPatch(ImmPtr(address), ARMRegisters::S0);
        m_assembler.doubleTransfer(true, dest, ARMRegisters::S0, 0);
        return label;
    }

    void fastLoadDouble(RegisterID lo, RegisterID hi, FPRegisterID fpReg) {
        m_assembler.vmov64(false, true, lo, hi, fpReg);
    }

    void loadFloat(ImplicitAddress address, FPRegisterID dest)
    {
        ASSERT((address.offset & 0x3) == 0);
        // as long as this is a sane mapping, (*2) should just work
        m_assembler.floatTransfer(true, floatShadow(dest), address.base, address.offset);
        m_assembler.vcvt(m_assembler.FloatReg32, m_assembler.FloatReg64, floatShadow(dest), dest);
    }
    void loadFloat(BaseIndex address, FPRegisterID dest)
    {
        FPRegisterID dest_s = floatShadow(dest);
        m_assembler.baseIndexFloatTransfer(true, false, dest_s,
                                           address.base, address.index,
                                           address.scale, address.offset);
        m_assembler.vcvt(m_assembler.FloatReg32, m_assembler.FloatReg64, dest_s, dest);
    }

    DataLabelPtr loadFloat(const void* address, FPRegisterID dest)
    {
        FPRegisterID dest_s = floatShadow(dest);
        DataLabelPtr label = moveWithPatch(ImmPtr(address), ARMRegisters::S0);
        m_assembler.fmem_imm_off(true, false, true, dest_s, ARMRegisters::S0, 0);
        m_assembler.vcvt(m_assembler.FloatReg32, m_assembler.FloatReg64, dest_s, dest);
        return label;
    }

    void storeDouble(FPRegisterID src, ImplicitAddress address)
    {
        // Store a double at base+offset.
        m_assembler.doubleTransfer(false, src, address.base, address.offset);
    }

    void storeDouble(FPRegisterID src, BaseIndex address)
    {
        m_assembler.baseIndexFloatTransfer(false, true, src,
                                           address.base, address.index,
                                           address.scale, address.offset);
    }

    void storeDouble(ImmDouble imm, Address address)
    {
        store32(Imm32(imm.u.s.lsb), address);
        store32(Imm32(imm.u.s.msb), Address(address.base, address.offset + 4));
    }

    void storeDouble(ImmDouble imm, BaseIndex address)
    {
        store32(Imm32(imm.u.s.lsb), address);
        store32(Imm32(imm.u.s.msb),
                BaseIndex(address.base, address.index, address.scale, address.offset + 4));
    }
    void fastStoreDouble(FPRegisterID fpReg, RegisterID lo, RegisterID hi) {
        m_assembler.vmov64(true, true, lo, hi, fpReg);
    }

    // the StoreFloat functions take an FPRegisterID that is really of the corresponding Double register.
    // but the double has already been converted into a float
    void storeFloat(FPRegisterID src, ImplicitAddress address)
    {
        m_assembler.floatTransfer(false, floatShadow(src), address.base, address.offset);
    }

    void storeFloat(FPRegisterID src, BaseIndex address)
    {
        m_assembler.baseIndexFloatTransfer(false, false, floatShadow(src),
                                           address.base, address.index,
                                           address.scale, address.offset);
    }
    void storeFloat(ImmDouble imm, Address address)
    {
        union {
            float f;
            uint32_t u32;
        } u;
        u.f = imm.u.d;
        store32(Imm32(u.u32), address);
    }

    void storeFloat(ImmDouble imm, BaseIndex address)
    {
        union {
            float f;
            uint32_t u32;
        } u;
        u.f = imm.u.d;
        store32(Imm32(u.u32), address);
    }

    void addDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.faddd_r(dest, dest, src);
    }

    void addDouble(Address src, FPRegisterID dest)
    {
        loadDouble(src, ARMRegisters::SD0);
        addDouble(ARMRegisters::SD0, dest);
    }

    void divDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.fdivd_r(dest, dest, src);
    }

    void divDouble(Address src, FPRegisterID dest)
    {
        ASSERT_NOT_REACHED(); // Untested
        loadDouble(src, ARMRegisters::SD0);
        divDouble(ARMRegisters::SD0, dest);
    }

    void subDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.fsubd_r(dest, dest, src);
    }

    void subDouble(Address src, FPRegisterID dest)
    {
        loadDouble(src, ARMRegisters::SD0);
        subDouble(ARMRegisters::SD0, dest);
    }

    void mulDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.fmuld_r(dest, dest, src);
    }

    void mulDouble(Address src, FPRegisterID dest)
    {
        loadDouble(src, ARMRegisters::SD0);
        mulDouble(ARMRegisters::SD0, dest);
    }

    void negDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.fnegd_r(dest, src);
    }

    void absDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.fabsd_r(dest, src);
    }

    void sqrtDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.fsqrtd_r(dest, src);
    }

    void convertInt32ToDouble(RegisterID src, FPRegisterID dest)
    {
        m_assembler.fmsr_r(floatShadow(dest), src);
        m_assembler.fsitod_r(dest, floatShadow(dest));
    }

    void convertUInt32ToDouble(RegisterID src, FPRegisterID dest)
    {
        m_assembler.fmsr_r(floatShadow(dest), src);
        m_assembler.fuitod_r(dest, floatShadow(dest));
    }

    void convertInt32ToDouble(Address src, FPRegisterID dest)
    {
        // flds does not worth the effort here
        load32(src, ARMRegisters::S1);
        convertInt32ToDouble(ARMRegisters::S1, dest);
    }

    void convertInt32ToDouble(AbsoluteAddress src, FPRegisterID dest)
    {
        ASSERT_NOT_REACHED(); // Untested
        // flds does not worth the effort here
        m_assembler.ldr_un_imm(ARMRegisters::S1, (ARMWord)src.m_ptr);
        m_assembler.dtr_u(true, ARMRegisters::S1, ARMRegisters::S1, 0);
        convertInt32ToDouble(ARMRegisters::S1, dest);
    }

    void convertDoubleToFloat(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.vcvt(m_assembler.FloatReg64, m_assembler.FloatReg32, src, floatShadow(dest));
    }

    Jump branchDouble(DoubleCondition cond, FPRegisterID left, FPRegisterID right)
    {
        m_assembler.fcmpd_r(left, right);
        m_assembler.fmstat();
        if (cond & DoubleConditionBitSpecial)
            m_assembler.cmp_r(ARMRegisters::S0, ARMRegisters::S0, ARMAssembler::VS);
        return Jump(m_assembler.jmp(static_cast<ARMAssembler::Condition>(cond & ~DoubleConditionMask)));
    }

    // Truncates 'src' to an integer, and places the resulting 'dest'.
    // If the result is not representable as a 32 bit value, branch.
    // May also branch for some values that are representable in 32 bits
    Jump branchTruncateDoubleToInt32(FPRegisterID src, RegisterID dest)
    {
        m_assembler.ftosizd_r(floatShadow(ARMRegisters::SD0), src);
        // If FTOSIZD (VCVT.S32.F64) can't fit the result into a 32-bit
        // integer, it saturates at INT_MAX or INT_MIN. Testing this is
        // probably quicker than testing FPSCR for exception.
        m_assembler.fmrs_r(dest, floatShadow(ARMRegisters::SD0));
        m_assembler.cmn_r(dest, ARMAssembler::getOp2(-0x7fffffff));
        m_assembler.cmp_r(dest, ARMAssembler::getOp2(0x80000000), ARMCondition(NonZero));
        return Jump(m_assembler.jmp(ARMCondition(Zero)));
    }

    // Convert 'src' to an integer, and places the resulting 'dest'.
    // If the result is not representable as a 32 bit value, branch.
    // May also branch for some values that are representable in 32 bits
    // (specifically, in this case, 0).
    void branchConvertDoubleToInt32(FPRegisterID src, RegisterID dest, JumpList& failureCases, FPRegisterID fpTemp)
    {
        m_assembler.ftosid_r(floatShadow(ARMRegisters::SD0), src);
        m_assembler.fmrs_r(dest, floatShadow(ARMRegisters::SD0));

        // Convert the integer result back to float & compare to the original value - if not equal or unordered (NaN) then jump.
        m_assembler.fsitod_r(ARMRegisters::SD0, floatShadow(ARMRegisters::SD0));
        failureCases.append(branchDouble(DoubleNotEqualOrUnordered, src, ARMRegisters::SD0));

        // If the result is zero, it might have been -0.0, and 0.0 equals to -0.0
        failureCases.append(branchTest32(Zero, dest));
    }

    void zeroDouble(FPRegisterID srcDest)
    {
        m_assembler.mov_r(ARMRegisters::S0, ARMAssembler::getOp2(0));
        convertInt32ToDouble(ARMRegisters::S0, srcDest);
    }

    void ensureSpace(int space)
    {
        m_assembler.ensureSpace(space);
    }

    void forceFlushConstantPool()
    {
        m_assembler.forceFlushConstantPool();
    }

    int flushCount()
    {
        return m_assembler.flushCount();
    }

protected:
    ARMAssembler::Condition ARMCondition(Condition cond)
    {
        return static_cast<ARMAssembler::Condition>(cond);
    }

    void ensureSpace(int insnSpace, int constSpace)
    {
        m_assembler.ensureSpace(insnSpace, constSpace);
    }

    int sizeOfConstantPool()
    {
        return m_assembler.sizeOfConstantPool();
    }

#if WTF_ARM_ARCH_VERSION < 5
    void prepareCall()
    {
#if WTF_ARM_ARCH_VERSION < 5
        ensureSpace(2 * sizeof(ARMWord), sizeof(ARMWord));

        m_assembler.mov_r(linkRegister, ARMRegisters::pc);
#endif
    }
#endif

#if WTF_ARM_ARCH_VERSION < 5
    void call32(RegisterID base, int32_t offset)
    {
#if WTF_ARM_ARCH_VERSION >= 5
        int targetReg = ARMRegisters::S1;
#else
        int targetReg = ARMRegisters::pc;
#endif
        int tmpReg = ARMRegisters::S1;

        if (base == ARMRegisters::sp)
            offset += 4;

        if (offset >= 0) {
            if (offset <= 0xfff) {
                prepareCall();
                m_assembler.dtr_u(true, targetReg, base, offset);
            } else if (offset <= 0xfffff) {
                m_assembler.add_r(tmpReg, base, ARMAssembler::OP2_IMM | (offset >> 12) | (10 << 8));
                prepareCall();
                m_assembler.dtr_u(true, targetReg, tmpReg, offset & 0xfff);
            } else {
                ARMWord reg = m_assembler.getImm(offset, tmpReg);
                prepareCall();
                m_assembler.dtr_ur(true, targetReg, base, reg);
            }
        } else  {
            offset = -offset;
            if (offset <= 0xfff) {
                prepareCall();
                m_assembler.dtr_d(true, targetReg, base, offset);
            } else if (offset <= 0xfffff) {
                m_assembler.sub_r(tmpReg, base, ARMAssembler::OP2_IMM | (offset >> 12) | (10 << 8));
                prepareCall();
                m_assembler.dtr_d(true, targetReg, tmpReg, offset & 0xfff);
            } else {
                ARMWord reg = m_assembler.getImm(offset, tmpReg);
                prepareCall();
                m_assembler.dtr_dr(true, targetReg, base, reg);
            }
        }
#if WTF_ARM_ARCH_VERSION >= 5
        m_assembler.blx(targetReg);
#endif
    }
#else
    void call32(RegisterID base, int32_t offset)
    {
        // TODO: Why is SP special?
        if (base == ARMRegisters::sp)
            offset += 4;

        // Branch to the address stored in base+offset, using one of the
        // following sequences:
        // ----
        //  LDR     ip, [base, ±offset]
        //  BLX     ip
        // ----
        //  ADD/SUB ip, base, #(offset & 0xff000)
        //  LDR     ip, [ip, #(offset & 0xfff)]
        //  BLX     ip
        // ----
        //  LDR     ip, =offset
        //  LDR     ip, [base, ±ip]
        //  BLX     ip

        if (offset >= 0) {
            if (offset <= 0xfff) {
                m_assembler.dtr_u(true, ARMRegisters::S0, base, offset);
            } else if (offset <= 0xfffff) {
                m_assembler.add_r(ARMRegisters::S0, base, ARMAssembler::OP2_IMM | (offset >> 12) | (10 << 8));
                m_assembler.dtr_u(true, ARMRegisters::S0, ARMRegisters::S0, offset & 0xfff);
            } else {
                m_assembler.moveImm(offset, ARMRegisters::S0);
                m_assembler.dtr_ur(true, ARMRegisters::S0, base, ARMRegisters::S0);
            }
        } else  {
            offset = -offset;
            if (offset <= 0xfff) {
                m_assembler.dtr_d(true, ARMRegisters::S0, base, offset);
            } else if (offset <= 0xfffff) {
                m_assembler.sub_r(ARMRegisters::S0, base, ARMAssembler::OP2_IMM | (offset >> 12) | (10 << 8));
                m_assembler.dtr_d(true, ARMRegisters::S0, ARMRegisters::S0, offset & 0xfff);
            } else {
                m_assembler.moveImm(offset, ARMRegisters::S0);
                m_assembler.dtr_dr(true, ARMRegisters::S0, base, ARMRegisters::S0);
            }
        }
        m_assembler.blx(ARMRegisters::S0);
    }
#endif

private:
    friend class LinkBuffer;
    friend class RepatchBuffer;

    static void linkCall(void* code, Call call, FunctionPtr function)
    {
        ARMAssembler::linkCall(code, call.m_jmp, function.value());
    }

    static void repatchCall(CodeLocationCall call, CodeLocationLabel destination)
    {
        ARMAssembler::relinkCall(call.dataLocation(), destination.executableAddress());
    }

    static void repatchCall(CodeLocationCall call, FunctionPtr destination)
    {
        ARMAssembler::relinkCall(call.dataLocation(), destination.executableAddress());
    }

    static const bool s_isVFPPresent;
};

}

#endif // ENABLE(ASSEMBLER) && CPU(ARM_TRADITIONAL)

#endif /* assembler_assembler_MacroAssemblerARM_h */
