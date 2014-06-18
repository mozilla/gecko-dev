/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/LIR.h"

#include <ctype.h>

#include "jsprf.h"

#include "jit/IonSpewer.h"
#include "jit/MIR.h"
#include "jit/MIRGenerator.h"

using namespace js;
using namespace js::jit;

LIRGraph::LIRGraph(MIRGraph *mir)
  : blocks_(),
    constantPool_(mir->alloc()),
    constantPoolMap_(mir->alloc()),
    safepoints_(mir->alloc()),
    nonCallSafepoints_(mir->alloc()),
    numVirtualRegisters_(0),
    numInstructions_(1), // First id is 1.
    localSlotCount_(0),
    argumentSlotCount_(0),
    entrySnapshot_(nullptr),
    mir_(*mir)
{
}

bool
LIRGraph::addConstantToPool(const Value &v, uint32_t *index)
{
    JS_ASSERT(constantPoolMap_.initialized());

    ConstantPoolMap::AddPtr p = constantPoolMap_.lookupForAdd(v);
    if (p) {
        *index = p->value();
        return true;
    }
    *index = constantPool_.length();
    return constantPool_.append(v) && constantPoolMap_.add(p, v, *index);
}

bool
LIRGraph::noteNeedsSafepoint(LInstruction *ins)
{
    // Instructions with safepoints must be in linear order.
    JS_ASSERT_IF(!safepoints_.empty(), safepoints_.back()->id() < ins->id());
    if (!ins->isCall() && !nonCallSafepoints_.append(ins))
        return false;
    return safepoints_.append(ins);
}

void
LIRGraph::dump(FILE *fp) const
{
    for (size_t i = 0; i < numBlocks(); i++) {
        getBlock(i)->dump(fp);
        fprintf(fp, "\n");
    }
}

void
LIRGraph::dump() const
{
    dump(stderr);
}

LBlock *
LBlock::New(TempAllocator &alloc, MBasicBlock *from)
{
    LBlock *block = new(alloc) LBlock(from);
    if (!block)
        return nullptr;

    // Count the number of LPhis we'll need.
    size_t numLPhis = 0;
    for (MPhiIterator i(from->phisBegin()), e(from->phisEnd()); i != e; ++i) {
        MPhi *phi = *i;
        numLPhis += (phi->type() == MIRType_Value) ? BOX_PIECES : 1;
    }

    // Allocate space for the LPhis.
    if (!block->phis_.init(alloc, numLPhis))
        return nullptr;

    // For each MIR phi, set up LIR phis as appropriate. We'll fill in their
    // operands on each incoming edge, and set their definitions at the start of
    // their defining block.
    size_t phiIndex = 0;
    size_t numPreds = from->numPredecessors();
    for (MPhiIterator i(from->phisBegin()), e(from->phisEnd()); i != e; ++i) {
        MPhi *phi = *i;
        MOZ_ASSERT(phi->numOperands() == numPreds);

        int numPhis = (phi->type() == MIRType_Value) ? BOX_PIECES : 1;
        for (int i = 0; i < numPhis; i++) {
            void *array = alloc.allocateArray<sizeof(LAllocation)>(numPreds);
            LAllocation *inputs = static_cast<LAllocation *>(array);
            if (!inputs)
                return nullptr;

            new (&block->phis_[phiIndex++]) LPhi(phi, inputs);
        }
    }
    return block;
}

uint32_t
LBlock::firstId()
{
    if (phis_.length()) {
        return phis_[0].id();
    } else {
        for (LInstructionIterator i(instructions_.begin()); i != instructions_.end(); i++) {
            if (i->id())
                return i->id();
        }
    }
    return 0;
}
uint32_t
LBlock::lastId()
{
    LInstruction *last = *instructions_.rbegin();
    JS_ASSERT(last->id());
    // The last instruction is a control flow instruction which does not have
    // any output.
    JS_ASSERT(last->numDefs() == 0);
    return last->id();
}

LMoveGroup *
LBlock::getEntryMoveGroup(TempAllocator &alloc)
{
    if (entryMoveGroup_)
        return entryMoveGroup_;
    entryMoveGroup_ = LMoveGroup::New(alloc);
    if (begin()->isLabel())
        insertAfter(*begin(), entryMoveGroup_);
    else
        insertBefore(*begin(), entryMoveGroup_);
    return entryMoveGroup_;
}

LMoveGroup *
LBlock::getExitMoveGroup(TempAllocator &alloc)
{
    if (exitMoveGroup_)
        return exitMoveGroup_;
    exitMoveGroup_ = LMoveGroup::New(alloc);
    insertBefore(*rbegin(), exitMoveGroup_);
    return exitMoveGroup_;
}

void
LBlock::dump(FILE *fp)
{
    fprintf(fp, "block%u:\n", mir()->id());
    for (size_t i = 0; i < numPhis(); ++i) {
        getPhi(i)->dump(fp);
        fprintf(fp, "\n");
    }
    for (LInstructionIterator iter = begin(); iter != end(); iter++) {
        iter->dump(fp);
        fprintf(fp, "\n");
    }
}

void
LBlock::dump()
{
    dump(stderr);
}

static size_t
TotalOperandCount(LRecoverInfo *recoverInfo)
{
    LRecoverInfo::OperandIter it(recoverInfo->begin());
    LRecoverInfo::OperandIter end(recoverInfo->end());
    size_t accum = 0;

    for (; it != end; ++it) {
        if (!it->isRecoveredOnBailout())
            accum++;
    }
    return accum;
}

LRecoverInfo::LRecoverInfo(TempAllocator &alloc)
  : instructions_(alloc),
    recoverOffset_(INVALID_RECOVER_OFFSET)
{ }

LRecoverInfo *
LRecoverInfo::New(MIRGenerator *gen, MResumePoint *mir)
{
    LRecoverInfo *recoverInfo = new(gen->alloc()) LRecoverInfo(gen->alloc());
    if (!recoverInfo || !recoverInfo->init(mir))
        return nullptr;

    IonSpew(IonSpew_Snapshots, "Generating LIR recover info %p from MIR (%p)",
            (void *)recoverInfo, (void *)mir);

    return recoverInfo;
}

bool
LRecoverInfo::appendOperands(MNode *ins)
{
    for (size_t i = 0, end = ins->numOperands(); i < end; i++) {
        MDefinition *def = ins->getOperand(i);

        // As there is no cycle in the data-flow (without MPhi), checking for
        // isInWorkList implies that the definition is already in the
        // instruction vector, and not processed by a caller of the current
        // function.
        if (def->isRecoveredOnBailout() && !def->isInWorklist()) {
            if (!appendDefinition(def))
                return false;
        }
    }

    return true;
}

bool
LRecoverInfo::appendDefinition(MDefinition *def)
{
    MOZ_ASSERT(def->isRecoveredOnBailout());
    def->setInWorklist();
    if (!appendOperands(def))
        return false;
    return instructions_.append(def);
}

bool
LRecoverInfo::appendResumePoint(MResumePoint *rp)
{
    if (rp->caller() && !appendResumePoint(rp->caller()))
        return false;

    if (!appendOperands(rp))
        return false;

    return instructions_.append(rp);
}

bool
LRecoverInfo::init(MResumePoint *rp)
{
    // Sort operations in the order in which we need to restore the stack. This
    // implies that outer frames, as well as operations needed to recover the
    // current frame, are located before the current frame. The inner-most
    // resume point should be the last element in the list.
    if (!appendResumePoint(rp))
        return false;

    // Remove temporary flags from all definitions.
    for (MNode **it = begin(); it != end(); it++) {
        if (!(*it)->isDefinition())
            continue;

        (*it)->toDefinition()->setNotInWorklist();
    }

    MOZ_ASSERT(mir() == rp);
    return true;
}

LSnapshot::LSnapshot(LRecoverInfo *recoverInfo, BailoutKind kind)
  : numSlots_(TotalOperandCount(recoverInfo) * BOX_PIECES),
    slots_(nullptr),
    recoverInfo_(recoverInfo),
    snapshotOffset_(INVALID_SNAPSHOT_OFFSET),
    bailoutId_(INVALID_BAILOUT_ID),
    bailoutKind_(kind)
{ }

bool
LSnapshot::init(MIRGenerator *gen)
{
    slots_ = gen->allocate<LAllocation>(numSlots_);
    return !!slots_;
}

LSnapshot *
LSnapshot::New(MIRGenerator *gen, LRecoverInfo *recover, BailoutKind kind)
{
    LSnapshot *snapshot = new(gen->alloc()) LSnapshot(recover, kind);
    if (!snapshot || !snapshot->init(gen))
        return nullptr;

    IonSpew(IonSpew_Snapshots, "Generating LIR snapshot %p from recover (%p)",
            (void *)snapshot, (void *)recover);

    return snapshot;
}

void
LSnapshot::rewriteRecoveredInput(LUse input)
{
    // Mark any operands to this snapshot with the same value as input as being
    // equal to the instruction's result.
    for (size_t i = 0; i < numEntries(); i++) {
        if (getEntry(i)->isUse() && getEntry(i)->toUse()->virtualRegister() == input.virtualRegister())
            setEntry(i, LUse(input.virtualRegister(), LUse::RECOVERED_INPUT));
    }
}

void
LInstruction::printName(FILE *fp, Opcode op)
{
    static const char * const names[] =
    {
#define LIROP(x) #x,
        LIR_OPCODE_LIST(LIROP)
#undef LIROP
    };
    const char *name = names[op];
    size_t len = strlen(name);
    for (size_t i = 0; i < len; i++)
        fprintf(fp, "%c", tolower(name[i]));
}

void
LInstruction::printName(FILE *fp)
{
    printName(fp, op());
}

static const char * const TypeChars[] =
{
    "g",            // GENERAL
    "i",            // INT32
    "o",            // OBJECT
    "s",            // SLOTS
    "f",            // FLOAT32
    "d",            // DOUBLE
#ifdef JS_NUNBOX32
    "t",            // TYPE
    "p"             // PAYLOAD
#elif JS_PUNBOX64
    "x"             // BOX
#endif
};

static void
PrintDefinition(FILE *fp, const LDefinition &def)
{
    fprintf(fp, "[%s", TypeChars[def.type()]);
    if (def.virtualRegister())
        fprintf(fp, ":%d", def.virtualRegister());
    if (def.policy() == LDefinition::PRESET) {
        fprintf(fp, " (%s)", def.output()->toString());
    } else if (def.policy() == LDefinition::MUST_REUSE_INPUT) {
        fprintf(fp, " (!)");
    } else if (def.policy() == LDefinition::PASSTHROUGH) {
        fprintf(fp, " (-)");
    }
    fprintf(fp, "]");
}

#ifdef DEBUG
static void
PrintUse(char *buf, size_t size, const LUse *use)
{
    switch (use->policy()) {
      case LUse::REGISTER:
        JS_snprintf(buf, size, "v%d:r", use->virtualRegister());
        break;
      case LUse::FIXED:
        // Unfortunately, we don't know here whether the virtual register is a
        // float or a double. Should we steal a bit in LUse for help? For now,
        // nothing defines any fixed xmm registers.
        JS_snprintf(buf, size, "v%d:%s", use->virtualRegister(),
                    Registers::GetName(Registers::Code(use->registerCode())));
        break;
      case LUse::ANY:
        JS_snprintf(buf, size, "v%d:r?", use->virtualRegister());
        break;
      case LUse::KEEPALIVE:
        JS_snprintf(buf, size, "v%d:*", use->virtualRegister());
        break;
      case LUse::RECOVERED_INPUT:
        JS_snprintf(buf, size, "v%d:**", use->virtualRegister());
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("invalid use policy");
    }
}

const char *
LAllocation::toString() const
{
    // Not reentrant!
    static char buf[40];

    switch (kind()) {
      case LAllocation::CONSTANT_VALUE:
      case LAllocation::CONSTANT_INDEX:
        return "c";
      case LAllocation::GPR:
        JS_snprintf(buf, sizeof(buf), "=%s", toGeneralReg()->reg().name());
        return buf;
      case LAllocation::FPU:
        JS_snprintf(buf, sizeof(buf), "=%s", toFloatReg()->reg().name());
        return buf;
      case LAllocation::STACK_SLOT:
        JS_snprintf(buf, sizeof(buf), "stack:%d", toStackSlot()->slot());
        return buf;
      case LAllocation::ARGUMENT_SLOT:
        JS_snprintf(buf, sizeof(buf), "arg:%d", toArgument()->index());
        return buf;
      case LAllocation::USE:
        PrintUse(buf, sizeof(buf), toUse());
        return buf;
      default:
        MOZ_ASSUME_UNREACHABLE("what?");
    }
}
#endif // DEBUG

void
LAllocation::dump() const
{
    fprintf(stderr, "%s\n", toString());
}

void
LInstruction::printOperands(FILE *fp)
{
    for (size_t i = 0, e = numOperands(); i < e; i++) {
        fprintf(fp, " (%s)", getOperand(i)->toString());
        if (i != numOperands() - 1)
            fprintf(fp, ",");
    }
}

void
LInstruction::assignSnapshot(LSnapshot *snapshot)
{
    JS_ASSERT(!snapshot_);
    snapshot_ = snapshot;

#ifdef DEBUG
    if (IonSpewEnabled(IonSpew_Snapshots)) {
        IonSpewHeader(IonSpew_Snapshots);
        fprintf(IonSpewFile, "Assigning snapshot %p to instruction %p (",
                (void *)snapshot, (void *)this);
        printName(IonSpewFile);
        fprintf(IonSpewFile, ")\n");
    }
#endif
}

void
LInstruction::dump(FILE *fp)
{
    if (numDefs() != 0) {
        fprintf(fp, "{");
        for (size_t i = 0; i < numDefs(); i++) {
            PrintDefinition(fp, *getDef(i));
            if (i != numDefs() - 1)
                fprintf(fp, ", ");
        }
        fprintf(fp, "} <- ");
    }

    printName(fp);
    printInfo(fp);

    if (numTemps()) {
        fprintf(fp, " t=(");
        for (size_t i = 0; i < numTemps(); i++) {
            PrintDefinition(fp, *getTemp(i));
            if (i != numTemps() - 1)
                fprintf(fp, ", ");
        }
        fprintf(fp, ")");
    }

    if (numSuccessors()) {
        fprintf(fp, " s=(");
        for (size_t i = 0; i < numSuccessors(); i++) {
            fprintf(fp, "block%u", getSuccessor(i)->id());
            if (i != numSuccessors() - 1)
                fprintf(fp, ", ");
        }
        fprintf(fp, ")");
    }
}

void
LInstruction::dump()
{
    dump(stderr);
    fprintf(stderr, "\n");
}

void
LInstruction::initSafepoint(TempAllocator &alloc)
{
    JS_ASSERT(!safepoint_);
    safepoint_ = new(alloc) LSafepoint(alloc);
    JS_ASSERT(safepoint_);
}

bool
LMoveGroup::add(LAllocation *from, LAllocation *to, LDefinition::Type type)
{
#ifdef DEBUG
    JS_ASSERT(*from != *to);
    for (size_t i = 0; i < moves_.length(); i++)
        JS_ASSERT(*to != *moves_[i].to());
#endif
    return moves_.append(LMove(from, to, type));
}

bool
LMoveGroup::addAfter(LAllocation *from, LAllocation *to, LDefinition::Type type)
{
    // Transform the operands to this move so that performing the result
    // simultaneously with existing moves in the group will have the same
    // effect as if the original move took place after the existing moves.

    for (size_t i = 0; i < moves_.length(); i++) {
        if (*moves_[i].to() == *from) {
            from = moves_[i].from();
            break;
        }
    }

    if (*from == *to)
        return true;

    for (size_t i = 0; i < moves_.length(); i++) {
        if (*to == *moves_[i].to()) {
            moves_[i] = LMove(from, to, type);
            return true;
        }
    }

    return add(from, to, type);
}

void
LMoveGroup::printOperands(FILE *fp)
{
    for (size_t i = 0; i < numMoves(); i++) {
        const LMove &move = getMove(i);
        // Use two printfs, as LAllocation::toString is not reentrant.
        fprintf(fp, "[%s", move.from()->toString());
        fprintf(fp, " -> %s]", move.to()->toString());
        if (i != numMoves() - 1)
            fprintf(fp, ", ");
    }
}
