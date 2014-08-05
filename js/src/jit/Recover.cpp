/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/Recover.h"

#include "jscntxt.h"
#include "jsmath.h"
#include "jsobj.h"
#include "jsstr.h"

#include "builtin/TypedObject.h"

#include "jit/IonSpewer.h"
#include "jit/JitFrameIterator.h"
#include "jit/MIR.h"
#include "jit/MIRGraph.h"
#include "jit/VMFunctions.h"

#include "vm/Interpreter.h"
#include "vm/Interpreter-inl.h"

using namespace js;
using namespace js::jit;

bool
MNode::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSUME_UNREACHABLE("This instruction is not serializable");
    return false;
}

void
RInstruction::readRecoverData(CompactBufferReader &reader, RInstructionStorage *raw)
{
    uint32_t op = reader.readUnsigned();
    switch (Opcode(op)) {
#   define MATCH_OPCODES_(op)                                           \
      case Recover_##op:                                                \
        static_assert(sizeof(R##op) <= sizeof(RInstructionStorage),     \
                      "Storage space is too small to decode R" #op " instructions."); \
        new (raw->addr()) R##op(reader);                                \
        break;

        RECOVER_OPCODE_LIST(MATCH_OPCODES_)
#   undef DEFINE_OPCODES_

      case Recover_Invalid:
      default:
        MOZ_ASSUME_UNREACHABLE("Bad decoding of the previous instruction?");
        break;
    }
}

bool
MResumePoint::writeRecoverData(CompactBufferWriter &writer) const
{
    writer.writeUnsigned(uint32_t(RInstruction::Recover_ResumePoint));

    MBasicBlock *bb = block();
    JSFunction *fun = bb->info().funMaybeLazy();
    JSScript *script = bb->info().script();
    uint32_t exprStack = stackDepth() - bb->info().ninvoke();

#ifdef DEBUG
    // Ensure that all snapshot which are encoded can safely be used for
    // bailouts.
    if (GetIonContext()->cx) {
        uint32_t stackDepth;
        bool reachablePC;
        jsbytecode *bailPC = pc();

        if (mode() == MResumePoint::ResumeAfter)
            bailPC = GetNextPc(pc());

        if (!ReconstructStackDepth(GetIonContext()->cx, script,
                                   bailPC, &stackDepth, &reachablePC))
        {
            return false;
        }

        if (reachablePC) {
            if (JSOp(*bailPC) == JSOP_FUNCALL) {
                // For fun.call(this, ...); the reconstructStackDepth will
                // include the this. When inlining that is not included.  So the
                // exprStackSlots will be one less.
                MOZ_ASSERT(stackDepth - exprStack <= 1);
            } else if (JSOp(*bailPC) != JSOP_FUNAPPLY &&
                       !IsGetPropPC(bailPC) && !IsSetPropPC(bailPC))
            {
                // For fun.apply({}, arguments) the reconstructStackDepth will
                // have stackdepth 4, but it could be that we inlined the
                // funapply. In that case exprStackSlots, will have the real
                // arguments in the slots and not be 4.

                // With accessors, we have different stack depths depending on
                // whether or not we inlined the accessor, as the inlined stack
                // contains a callee function that should never have been there
                // and we might just be capturing an uneventful property site,
                // in which case there won't have been any violence.
                MOZ_ASSERT(exprStack == stackDepth);
            }
        }
    }
#endif

    // Test if we honor the maximum of arguments at all times.  This is a sanity
    // check and not an algorithm limit. So check might be a bit too loose.  +4
    // to account for scope chain, return value, this value and maybe
    // arguments_object.
    MOZ_ASSERT(CountArgSlots(script, fun) < SNAPSHOT_MAX_NARGS + 4);

    uint32_t implicit = StartArgSlot(script);
    uint32_t formalArgs = CountArgSlots(script, fun);
    uint32_t nallocs = formalArgs + script->nfixed() + exprStack;

    IonSpew(IonSpew_Snapshots, "Starting frame; implicit %u, formals %u, fixed %u, exprs %u",
            implicit, formalArgs - implicit, script->nfixed(), exprStack);

    uint32_t pcoff = script->pcToOffset(pc());
    IonSpew(IonSpew_Snapshots, "Writing pc offset %u, nslots %u", pcoff, nallocs);
    writer.writeUnsigned(pcoff);
    writer.writeUnsigned(nallocs);
    return true;
}

RResumePoint::RResumePoint(CompactBufferReader &reader)
{
    pcOffset_ = reader.readUnsigned();
    numOperands_ = reader.readUnsigned();
    IonSpew(IonSpew_Snapshots, "Read RResumePoint (pc offset %u, nslots %u)",
            pcOffset_, numOperands_);
}

bool
RResumePoint::recover(JSContext *cx, SnapshotIterator &iter) const
{
    MOZ_ASSUME_UNREACHABLE("This instruction is not recoverable.");
}

bool
MBitNot::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_BitNot));
    return true;
}

RBitNot::RBitNot(CompactBufferReader &reader)
{ }

bool
RBitNot::recover(JSContext *cx, SnapshotIterator &iter) const
{
    RootedValue operand(cx, iter.read());

    int32_t result;
    if (!js::BitNot(cx, operand, &result))
        return false;

    RootedValue rootedResult(cx, js::Int32Value(result));
    iter.storeInstructionResult(rootedResult);
    return true;
}

bool
MBitAnd::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_BitAnd));
    return true;
}

RBitAnd::RBitAnd(CompactBufferReader &reader)
{ }

bool
RBitAnd::recover(JSContext *cx, SnapshotIterator &iter) const
{
    RootedValue lhs(cx, iter.read());
    RootedValue rhs(cx, iter.read());
    int32_t result;
    MOZ_ASSERT(!lhs.isObject() && !rhs.isObject());

    if (!js::BitAnd(cx, lhs, rhs, &result))
        return false;

    RootedValue rootedResult(cx, js::Int32Value(result));
    iter.storeInstructionResult(rootedResult);
    return true;
}

bool
MBitOr::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_BitOr));
    return true;
}

RBitOr::RBitOr(CompactBufferReader &reader)
{}

bool
RBitOr::recover(JSContext *cx, SnapshotIterator &iter) const
{
    RootedValue lhs(cx, iter.read());
    RootedValue rhs(cx, iter.read());
    int32_t result;
    MOZ_ASSERT(!lhs.isObject() && !rhs.isObject());

    if (!js::BitOr(cx, lhs, rhs, &result))
        return false;

    RootedValue asValue(cx, js::Int32Value(result));
    iter.storeInstructionResult(asValue);
    return true;
}

bool
MBitXor::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_BitXor));
    return true;
}

RBitXor::RBitXor(CompactBufferReader &reader)
{ }

bool
RBitXor::recover(JSContext *cx, SnapshotIterator &iter) const
{
    RootedValue lhs(cx, iter.read());
    RootedValue rhs(cx, iter.read());

    int32_t result;
    if (!js::BitXor(cx, lhs, rhs, &result))
        return false;

    RootedValue rootedResult(cx, js::Int32Value(result));
    iter.storeInstructionResult(rootedResult);
    return true;
}

bool
MLsh::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_Lsh));
    return true;
}

RLsh::RLsh(CompactBufferReader &reader)
{}

bool
RLsh::recover(JSContext *cx, SnapshotIterator &iter) const
{
    RootedValue lhs(cx, iter.read());
    RootedValue rhs(cx, iter.read());
    int32_t result;
    MOZ_ASSERT(!lhs.isObject() && !rhs.isObject());

    if (!js::BitLsh(cx, lhs, rhs, &result))
        return false;

    RootedValue asValue(cx, js::Int32Value(result));
    iter.storeInstructionResult(asValue);
    return true;
}

bool
MRsh::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_Rsh));
    return true;
}

RRsh::RRsh(CompactBufferReader &reader)
{ }

bool
RRsh::recover(JSContext *cx, SnapshotIterator &iter) const
{
    RootedValue lhs(cx, iter.read());
    RootedValue rhs(cx, iter.read());
    MOZ_ASSERT(!lhs.isObject() && !rhs.isObject());

    int32_t result;
    if (!js::BitRsh(cx, lhs, rhs, &result))
        return false;

    RootedValue rootedResult(cx, js::Int32Value(result));
    iter.storeInstructionResult(rootedResult);
    return true;
}

bool
MUrsh::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_Ursh));
    return true;
}

RUrsh::RUrsh(CompactBufferReader &reader)
{ }

bool
RUrsh::recover(JSContext *cx, SnapshotIterator &iter) const
{
    RootedValue lhs(cx, iter.read());
    RootedValue rhs(cx, iter.read());
    MOZ_ASSERT(!lhs.isObject() && !rhs.isObject());

    RootedValue result(cx);
    if (!js::UrshOperation(cx, lhs, rhs, &result))
        return false;

    iter.storeInstructionResult(result);
    return true;
}

bool
MAdd::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_Add));
    writer.writeByte(specialization_ == MIRType_Float32);
    return true;
}

RAdd::RAdd(CompactBufferReader &reader)
{
    isFloatOperation_ = reader.readByte();
}

bool
RAdd::recover(JSContext *cx, SnapshotIterator &iter) const
{
    RootedValue lhs(cx, iter.read());
    RootedValue rhs(cx, iter.read());
    RootedValue result(cx);

    MOZ_ASSERT(!lhs.isObject() && !rhs.isObject());
    if (!js::AddValues(cx, &lhs, &rhs, &result))
        return false;

    // MIRType_Float32 is a specialization embedding the fact that the result is
    // rounded to a Float32.
    if (isFloatOperation_ && !RoundFloat32(cx, result, &result))
        return false;

    iter.storeInstructionResult(result);
    return true;
}

bool
MSub::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_Sub));
    writer.writeByte(specialization_ == MIRType_Float32);
    return true;
}

RSub::RSub(CompactBufferReader &reader)
{
    isFloatOperation_ = reader.readByte();
}

bool
RSub::recover(JSContext *cx, SnapshotIterator &iter) const
{
    RootedValue lhs(cx, iter.read());
    RootedValue rhs(cx, iter.read());
    RootedValue result(cx);

    MOZ_ASSERT(!lhs.isObject() && !rhs.isObject());
    if (!js::SubValues(cx, &lhs, &rhs, &result))
        return false;

    // MIRType_Float32 is a specialization embedding the fact that the result is
    // rounded to a Float32.
    if (isFloatOperation_ && !RoundFloat32(cx, result, &result))
        return false;

    iter.storeInstructionResult(result);
    return true;
}

bool
MMul::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_Mul));
    writer.writeByte(specialization_ == MIRType_Float32);
    return true;
}

RMul::RMul(CompactBufferReader &reader)
{
    isFloatOperation_ = reader.readByte();
}

bool
RMul::recover(JSContext *cx, SnapshotIterator &iter) const
{
    RootedValue lhs(cx, iter.read());
    RootedValue rhs(cx, iter.read());
    RootedValue result(cx);

    if (!js::MulValues(cx, &lhs, &rhs, &result))
        return false;

    // MIRType_Float32 is a specialization embedding the fact that the result is
    // rounded to a Float32.
    if (isFloatOperation_ && !RoundFloat32(cx, result, &result))
        return false;

    iter.storeInstructionResult(result);
    return true;
}

bool
MDiv::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_Div));
    writer.writeByte(specialization_ == MIRType_Float32);
    return true;
}

RDiv::RDiv(CompactBufferReader &reader)
{
    isFloatOperation_ = reader.readByte();
}

bool
RDiv::recover(JSContext *cx, SnapshotIterator &iter) const
{
    RootedValue lhs(cx, iter.read());
    RootedValue rhs(cx, iter.read());
    RootedValue result(cx);

    if (!js::DivValues(cx, &lhs, &rhs, &result))
        return false;

    // MIRType_Float32 is a specialization embedding the fact that the result is
    // rounded to a Float32.
    if (isFloatOperation_ && !RoundFloat32(cx, result, &result))
        return false;

    iter.storeInstructionResult(result);
    return true;
}

bool
MMod::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_Mod));
    return true;
}

RMod::RMod(CompactBufferReader &reader)
{ }

bool
RMod::recover(JSContext *cx, SnapshotIterator &iter) const
{
    RootedValue lhs(cx, iter.read());
    RootedValue rhs(cx, iter.read());
    RootedValue result(cx);

    MOZ_ASSERT(!lhs.isObject() && !rhs.isObject());
    if (!js::ModValues(cx, &lhs, &rhs, &result))
        return false;

    iter.storeInstructionResult(result);
    return true;
}

bool
MNot::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_Not));
    return true;
}

RNot::RNot(CompactBufferReader &reader)
{ }

bool
RNot::recover(JSContext *cx, SnapshotIterator &iter) const
{
    RootedValue v(cx, iter.read());
    RootedValue result(cx);

    result.setBoolean(!ToBoolean(v));

    iter.storeInstructionResult(result);
    return true;
}

bool
MConcat::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_Concat));
    return true;
}

RConcat::RConcat(CompactBufferReader &reader)
{}

bool
RConcat::recover(JSContext *cx, SnapshotIterator &iter) const
{
    RootedValue lhs(cx, iter.read());
    RootedValue rhs(cx, iter.read());
    RootedValue result(cx);

    MOZ_ASSERT(!lhs.isObject() && !rhs.isObject());
    if (!js::AddValues(cx, &lhs, &rhs, &result))
        return false;

    iter.storeInstructionResult(result);
    return true;
}

RStringLength::RStringLength(CompactBufferReader &reader)
{}

bool
RStringLength::recover(JSContext *cx, SnapshotIterator &iter) const
{
    RootedValue operand(cx, iter.read());
    RootedValue result(cx);

    MOZ_ASSERT(!operand.isObject());
    if (!js::GetLengthProperty(operand, &result))
        return false;

    iter.storeInstructionResult(result);
    return true;
}

bool
MStringLength::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_StringLength));
    return true;
}

bool
MArgumentsLength::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_ArgumentsLength));
    return true;
}

RArgumentsLength::RArgumentsLength(CompactBufferReader &reader)
{ }

bool
RArgumentsLength::recover(JSContext *cx, SnapshotIterator &iter) const
{
    RootedValue result(cx);

    result.setInt32(iter.readOuterNumActualArgs());

    iter.storeInstructionResult(result);
    return true;
}

bool
MFloor::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_Floor));
    return true;
}

RFloor::RFloor(CompactBufferReader &reader)
{ }

bool RFloor::recover(JSContext *cx, SnapshotIterator &iter) const
{
    RootedValue v(cx, iter.read());
    RootedValue result(cx);

    if (!js::math_floor_handle(cx, v, &result))
        return false;

    iter.storeInstructionResult(result);
    return true;
}

bool
MRound::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_Round));
    return true;
}

RRound::RRound(CompactBufferReader &reader)
{}

bool
RRound::recover(JSContext *cx, SnapshotIterator &iter) const
{
    RootedValue arg(cx, iter.read());
    RootedValue result(cx);

    MOZ_ASSERT(!arg.isObject());
    if(!js::math_round_handle(cx, arg, &result))
        return false;

    iter.storeInstructionResult(result);
    return true;
}

bool
MCharCodeAt::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_CharCodeAt));
    return true;
}

RCharCodeAt::RCharCodeAt(CompactBufferReader &reader)
{}

bool
RCharCodeAt::recover(JSContext *cx, SnapshotIterator &iter) const
{
    RootedString lhs(cx, iter.read().toString());
    RootedValue rhs(cx, iter.read());
    RootedValue result(cx);

    if (!js::str_charCodeAt_impl(cx, lhs, rhs, &result))
        return false;

    iter.storeInstructionResult(result);
    return true;
}

bool
MFromCharCode::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_FromCharCode));
    return true;
}

RFromCharCode::RFromCharCode(CompactBufferReader &reader)
{}

bool
RFromCharCode::recover(JSContext *cx, SnapshotIterator &iter) const
{
    RootedValue operand(cx, iter.read());
    RootedValue result(cx);

    MOZ_ASSERT(!operand.isObject());
    if (!js::str_fromCharCode_one_arg(cx, operand, &result))
        return false;

    iter.storeInstructionResult(result);
    return true;
}

bool
MPow::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_Pow));
    return true;
}

RPow::RPow(CompactBufferReader &reader)
{ }

bool
RPow::recover(JSContext *cx, SnapshotIterator &iter) const
{
    RootedValue base(cx, iter.read());
    RootedValue power(cx, iter.read());
    RootedValue result(cx);

    MOZ_ASSERT(base.isNumber() && power.isNumber());
    if (!js::math_pow_handle(cx, base, power, &result))
        return false;

    iter.storeInstructionResult(result);
    return true;
}

bool
MPowHalf::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_PowHalf));
    return true;
}

RPowHalf::RPowHalf(CompactBufferReader &reader)
{ }

bool
RPowHalf::recover(JSContext *cx, SnapshotIterator &iter) const
{
    RootedValue base(cx, iter.read());
    RootedValue power(cx);
    RootedValue result(cx);
    power.setNumber(0.5);

    MOZ_ASSERT(base.isNumber());
    if (!js::math_pow_handle(cx, base, power, &result))
        return false;

    iter.storeInstructionResult(result);
    return true;
}

bool
MMinMax::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_MinMax));
    writer.writeByte(isMax_);
    return true;
}

RMinMax::RMinMax(CompactBufferReader &reader)
{
    isMax_ = reader.readByte();
}

bool
RMinMax::recover(JSContext *cx, SnapshotIterator &iter) const
{
    RootedValue a(cx, iter.read());
    RootedValue b(cx, iter.read());
    RootedValue result(cx);

    if (!js::minmax_impl(cx, isMax_, a, b, &result))
        return false;

    iter.storeInstructionResult(result);
    return true;
}

bool
MAbs::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_Abs));
    return true;
}

RAbs::RAbs(CompactBufferReader &reader)
{ }

bool
RAbs::recover(JSContext *cx, SnapshotIterator &iter) const
{
    RootedValue v(cx, iter.read());
    RootedValue result(cx);

    if (!js::math_abs_handle(cx, v, &result))
        return false;

    iter.storeInstructionResult(result);
    return true;
}

bool
MSqrt::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_Sqrt));
    writer.writeByte(type() == MIRType_Float32);
    return true;
}

RSqrt::RSqrt(CompactBufferReader &reader)
{
    isFloatOperation_ = reader.readByte();
}

bool
RSqrt::recover(JSContext *cx, SnapshotIterator &iter) const
{
    RootedValue num(cx, iter.read());
    RootedValue result(cx);

    MOZ_ASSERT(num.isNumber());
    if (!math_sqrt_handle(cx, num, &result))
        return false;

    // MIRType_Float32 is a specialization embedding the fact that the result is
    // rounded to a Float32.
    if (isFloatOperation_ && !RoundFloat32(cx, result, &result))
        return false;

    iter.storeInstructionResult(result);
    return true;
}

bool
MAtan2::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_Atan2));
    return true;
}

RAtan2::RAtan2(CompactBufferReader &reader)
{ }

bool
RAtan2::recover(JSContext *cx, SnapshotIterator &iter) const
{
    RootedValue y(cx, iter.read());
    RootedValue x(cx, iter.read());
    RootedValue result(cx);

    if(!math_atan2_handle(cx, y, x, &result))
        return false;

    iter.storeInstructionResult(result);
    return true;
}

bool
MMathFunction::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    switch (function_) {
      case Round:
        writer.writeUnsigned(uint32_t(RInstruction::Recover_Round));
        return true;
      default:
        MOZ_ASSUME_UNREACHABLE("Unknown math function.");
        return false;
    }
}

bool
MStringSplit::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_StringSplit));
    return true;
}

RStringSplit::RStringSplit(CompactBufferReader &reader)
{}

bool
RStringSplit::recover(JSContext *cx, SnapshotIterator &iter) const
{
    RootedString str(cx, iter.read().toString());
    RootedString sep(cx, iter.read().toString());
    RootedTypeObject typeObj(cx, iter.read().toObject().type());

    RootedValue result(cx);

    JSObject *res = str_split_string(cx, typeObj, str, sep);
    if (!res)
        return false;

    result.setObject(*res);
    iter.storeInstructionResult(result);
    return true;
}

bool
MNewObject::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_NewObject));
    writer.writeByte(templateObjectIsClassPrototype_);
    return true;
}

RNewObject::RNewObject(CompactBufferReader &reader)
{
    templateObjectIsClassPrototype_ = reader.readByte();
}

bool
RNewObject::recover(JSContext *cx, SnapshotIterator &iter) const
{
    RootedObject templateObject(cx, &iter.read().toObject());
    RootedValue result(cx);
    JSObject *resultObject = nullptr;

    // Use AutoEnterAnalysis to avoid invoking the object metadata callback
    // while bailing out, which could try to walk the stack.
    types::AutoEnterAnalysis enter(cx);

    // See CodeGenerator::visitNewObjectVMCall
    if (templateObjectIsClassPrototype_)
        resultObject = NewInitObjectWithClassPrototype(cx, templateObject);
    else
        resultObject = NewInitObject(cx, templateObject);

    if (!resultObject)
        return false;

    result.setObject(*resultObject);
    iter.storeInstructionResult(result);
    return true;
}

bool
MNewArray::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_NewArray));
    writer.writeUnsigned(count());
    writer.writeByte(isAllocating());
    return true;
}

RNewArray::RNewArray(CompactBufferReader &reader)
{
    count_ = reader.readUnsigned();
    isAllocating_ = reader.readByte();
}

bool
RNewArray::recover(JSContext *cx, SnapshotIterator &iter) const
{
    RootedObject templateObject(cx, &iter.read().toObject());
    RootedValue result(cx);
    RootedTypeObject type(cx);

    // See CodeGenerator::visitNewArrayCallVM
    if (!templateObject->hasSingletonType())
        type = templateObject->type();

    JSObject *resultObject = NewDenseArray(cx, count_, type, isAllocating_);
    if (!resultObject)
        return false;

    result.setObject(*resultObject);
    iter.storeInstructionResult(result);
    return true;
}

bool
MNewDerivedTypedObject::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_NewDerivedTypedObject));
    return true;
}

RNewDerivedTypedObject::RNewDerivedTypedObject(CompactBufferReader &reader)
{ }

bool
RNewDerivedTypedObject::recover(JSContext *cx, SnapshotIterator &iter) const
{
    Rooted<SizedTypeDescr *> descr(cx, &iter.read().toObject().as<SizedTypeDescr>());
    Rooted<TypedObject *> owner(cx, &iter.read().toObject().as<TypedObject>());
    int32_t offset = iter.read().toInt32();

    // Use AutoEnterAnalysis to avoid invoking the object metadata callback
    // while bailing out, which could try to walk the stack.
    types::AutoEnterAnalysis enter(cx);

    JSObject *obj = TypedObject::createDerived(cx, descr, owner, offset);
    if (!obj)
        return false;

    RootedValue result(cx, ObjectValue(*obj));
    iter.storeInstructionResult(result);
    return true;
}

bool
MObjectState::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_ObjectState));
    writer.writeUnsigned(numSlots());
    return true;
}

RObjectState::RObjectState(CompactBufferReader &reader)
{
    numSlots_ = reader.readUnsigned();
}

bool
RObjectState::recover(JSContext *cx, SnapshotIterator &iter) const
{
    RootedObject object(cx, &iter.read().toObject());
    MOZ_ASSERT(object->slotSpan() == numSlots());

    RootedValue val(cx);
    for (size_t i = 0; i < numSlots(); i++) {
        val = iter.read();
        object->nativeSetSlot(i, val);
    }

    val.setObject(*object);
    iter.storeInstructionResult(val);
    return true;
}

bool
MArrayState::writeRecoverData(CompactBufferWriter &writer) const
{
    MOZ_ASSERT(canRecoverOnBailout());
    writer.writeUnsigned(uint32_t(RInstruction::Recover_ArrayState));
    writer.writeUnsigned(numElements());
    return true;
}

RArrayState::RArrayState(CompactBufferReader &reader)
{
    numElements_ = reader.readUnsigned();
}

bool
RArrayState::recover(JSContext *cx, SnapshotIterator &iter) const
{
    RootedValue result(cx);
    JSObject *object = &iter.read().toObject();
    uint32_t initLength = iter.read().toInt32();

    object->setDenseInitializedLength(initLength);
    for (size_t index = 0; index < numElements(); index++) {
        Value val = iter.read();

        if (index >= initLength) {
            MOZ_ASSERT(val.isUndefined());
            continue;
        }

        object->initDenseElement(index, val);
    }

    result.setObject(*object);
    iter.storeInstructionResult(result);
    return true;
}
