/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/MIRGraph.h"

#include "jit/AsmJS.h"
#include "jit/BytecodeAnalysis.h"
#include "jit/Ion.h"
#include "jit/IonSpewer.h"
#include "jit/MIR.h"
#include "jit/MIRGenerator.h"

using namespace js;
using namespace js::jit;

MIRGenerator::MIRGenerator(CompileCompartment *compartment, const JitCompileOptions &options,
                           TempAllocator *alloc, MIRGraph *graph, CompileInfo *info,
                           const OptimizationInfo *optimizationInfo)
  : compartment(compartment),
    info_(info),
    optimizationInfo_(optimizationInfo),
    alloc_(alloc),
    graph_(graph),
    abortReason_(AbortReason_NoAbort),
    error_(false),
    cancelBuild_(false),
    maxAsmJSStackArgBytes_(0),
    performsCall_(false),
    needsInitialStackAlignment_(false),
    minAsmJSHeapLength_(AsmJSAllocationGranularity),
    modifiesFrameArguments_(false),
    options(options)
{ }

bool
MIRGenerator::abortFmt(const char *message, va_list ap)
{
    IonSpewVA(IonSpew_Abort, message, ap);
    error_ = true;
    return false;
}

bool
MIRGenerator::abort(const char *message, ...)
{
    va_list ap;
    va_start(ap, message);
    abortFmt(message, ap);
    va_end(ap);
    return false;
}

void
MIRGraph::addBlock(MBasicBlock *block)
{
    JS_ASSERT(block);
    block->setId(blockIdGen_++);
    blocks_.pushBack(block);
    numBlocks_++;
}

void
MIRGraph::insertBlockAfter(MBasicBlock *at, MBasicBlock *block)
{
    block->setId(blockIdGen_++);
    blocks_.insertAfter(at, block);
    numBlocks_++;
}

void
MIRGraph::removeBlocksAfter(MBasicBlock *start)
{
    MBasicBlockIterator iter(begin());
    iter++;
    while (iter != end()) {
        MBasicBlock *block = *iter;
        iter++;

        if (block->id() <= start->id())
            continue;

        removeBlock(block);
    }
}

void
MIRGraph::removeBlock(MBasicBlock *block)
{
    // Remove a block from the graph. It will also cleanup the block.

    if (block == osrBlock_)
        osrBlock_ = nullptr;

    if (returnAccumulator_) {
        size_t i = 0;
        while (i < returnAccumulator_->length()) {
            if ((*returnAccumulator_)[i] == block)
                returnAccumulator_->erase(returnAccumulator_->begin() + i);
            else
                i++;
        }
    }

    block->discardAllResumePoints();
    block->discardAllInstructions();

    // Note: phis are disconnected from the rest of the graph, but are not
    // removed entirely. If the block being removed is a loop header then
    // IonBuilder may need to access these phis to more quickly converge on the
    // possible types in the graph. See IonBuilder::analyzeNewLoopTypes.
    block->discardAllPhiOperands();

    block->markAsDead();
    blocks_.remove(block);
    numBlocks_--;
}

void
MIRGraph::unmarkBlocks()
{
    for (MBasicBlockIterator i(blocks_.begin()); i != blocks_.end(); i++)
        i->unmark();
}

MDefinition *
MIRGraph::forkJoinContext()
{
    // Search the entry block to find a ForkJoinContext instruction. If we do
    // not find one, add one after the Start instruction.
    //
    // Note: the original design used a field in MIRGraph to cache the
    // forkJoinContext rather than searching for it again.  However, this
    // could become out of date due to DCE.  Given that we do not generally
    // have to search very far to find the ForkJoinContext instruction if it
    // exists, and that we don't look for it that often, I opted to simply
    // eliminate the cache and search anew each time, so that it is that much
    // easier to keep the IR coherent. - nmatsakis

    MBasicBlock *entry = entryBlock();
    JS_ASSERT(entry->info().executionMode() == ParallelExecution);

    MInstruction *start = nullptr;
    for (MInstructionIterator ins(entry->begin()); ins != entry->end(); ins++) {
        if (ins->isForkJoinContext())
            return *ins;
        else if (ins->isStart())
            start = *ins;
    }
    JS_ASSERT(start);

    MForkJoinContext *cx = MForkJoinContext::New(alloc());
    entry->insertAfter(start, cx);
    return cx;
}

MBasicBlock *
MBasicBlock::New(MIRGraph &graph, BytecodeAnalysis *analysis, CompileInfo &info,
                 MBasicBlock *pred, const BytecodeSite &site, Kind kind)
{
    JS_ASSERT(site.pc() != nullptr);

    MBasicBlock *block = new(graph.alloc()) MBasicBlock(graph, info, site, kind);
    if (!block->init())
        return nullptr;

    if (!block->inherit(graph.alloc(), analysis, pred, 0))
        return nullptr;

    return block;
}

MBasicBlock *
MBasicBlock::NewPopN(MIRGraph &graph, CompileInfo &info,
                     MBasicBlock *pred, const BytecodeSite &site, Kind kind, uint32_t popped)
{
    MBasicBlock *block = new(graph.alloc()) MBasicBlock(graph, info, site, kind);
    if (!block->init())
        return nullptr;

    if (!block->inherit(graph.alloc(), nullptr, pred, popped))
        return nullptr;

    return block;
}

MBasicBlock *
MBasicBlock::NewWithResumePoint(MIRGraph &graph, CompileInfo &info,
                                MBasicBlock *pred, const BytecodeSite &site,
                                MResumePoint *resumePoint)
{
    MBasicBlock *block = new(graph.alloc()) MBasicBlock(graph, info, site, NORMAL);

    resumePoint->block_ = block;
    block->entryResumePoint_ = resumePoint;

    if (!block->init())
        return nullptr;

    if (!block->inheritResumePoint(pred))
        return nullptr;

    return block;
}

MBasicBlock *
MBasicBlock::NewPendingLoopHeader(MIRGraph &graph, CompileInfo &info,
                                  MBasicBlock *pred, const BytecodeSite &site,
                                  unsigned stackPhiCount)
{
    JS_ASSERT(site.pc() != nullptr);

    MBasicBlock *block = new(graph.alloc()) MBasicBlock(graph, info, site, PENDING_LOOP_HEADER);
    if (!block->init())
        return nullptr;

    if (!block->inherit(graph.alloc(), nullptr, pred, 0, stackPhiCount))
        return nullptr;

    return block;
}

MBasicBlock *
MBasicBlock::NewSplitEdge(MIRGraph &graph, CompileInfo &info, MBasicBlock *pred)
{
    return pred->pc()
           ? MBasicBlock::New(graph, nullptr, info, pred,
                              BytecodeSite(pred->trackedTree(), pred->pc()), SPLIT_EDGE)
           : MBasicBlock::NewAsmJS(graph, info, pred, SPLIT_EDGE);
}

MBasicBlock *
MBasicBlock::NewAsmJS(MIRGraph &graph, CompileInfo &info, MBasicBlock *pred, Kind kind)
{
    MBasicBlock *block = new(graph.alloc()) MBasicBlock(graph, info, BytecodeSite(), kind);
    if (!block->init())
        return nullptr;

    if (pred) {
        block->stackPosition_ = pred->stackPosition_;

        if (block->kind_ == PENDING_LOOP_HEADER) {
            size_t nphis = block->stackPosition_;

            TempAllocator &alloc = graph.alloc();
            MPhi *phis = (MPhi*)alloc.allocateArray<sizeof(MPhi)>(nphis);
            if (!phis)
                return nullptr;

            // Note: Phis are inserted in the same order as the slots.
            for (size_t i = 0; i < nphis; i++) {
                MDefinition *predSlot = pred->getSlot(i);

                JS_ASSERT(predSlot->type() != MIRType_Value);
                MPhi *phi = new(phis + i) MPhi(alloc, predSlot->type());

                JS_ALWAYS_TRUE(phi->reserveLength(2));
                phi->addInput(predSlot);

                // Add append Phis in the block.
                block->addPhi(phi);
                block->setSlot(i, phi);
            }
        } else {
            block->copySlots(pred);
        }

        if (!block->predecessors_.append(pred))
            return nullptr;
    }

    return block;
}

MBasicBlock::MBasicBlock(MIRGraph &graph, CompileInfo &info, const BytecodeSite &site, Kind kind)
  : unreachable_(false),
    graph_(graph),
    info_(info),
    predecessors_(graph.alloc()),
    stackPosition_(info_.firstStackSlot()),
    pc_(site.pc()),
    lir_(nullptr),
    entryResumePoint_(nullptr),
    successorWithPhis_(nullptr),
    positionInPhiSuccessor_(0),
    kind_(kind),
    loopDepth_(0),
    mark_(false),
    immediatelyDominated_(graph.alloc()),
    immediateDominator_(nullptr),
    numDominated_(0),
    trackedSite_(site)
#if defined (JS_ION_PERF)
    , lineno_(0u),
    columnIndex_(0u)
#endif
{
}

bool
MBasicBlock::init()
{
    return slots_.init(graph_.alloc(), info_.nslots());
}

bool
MBasicBlock::increaseSlots(size_t num)
{
    return slots_.growBy(graph_.alloc(), num);
}

bool
MBasicBlock::ensureHasSlots(size_t num)
{
    size_t depth = stackDepth() + num;
    if (depth > nslots()) {
        if (!increaseSlots(depth - nslots()))
            return false;
    }
    return true;
}

void
MBasicBlock::copySlots(MBasicBlock *from)
{
    JS_ASSERT(stackPosition_ <= from->stackPosition_);

    for (uint32_t i = 0; i < stackPosition_; i++)
        slots_[i] = from->slots_[i];
}

bool
MBasicBlock::inherit(TempAllocator &alloc, BytecodeAnalysis *analysis, MBasicBlock *pred,
                     uint32_t popped, unsigned stackPhiCount)
{
    if (pred) {
        stackPosition_ = pred->stackPosition_;
        JS_ASSERT(stackPosition_ >= popped);
        stackPosition_ -= popped;
        if (kind_ != PENDING_LOOP_HEADER)
            copySlots(pred);
    } else {
        uint32_t stackDepth = analysis->info(pc()).stackDepth;
        stackPosition_ = info().firstStackSlot() + stackDepth;
        JS_ASSERT(stackPosition_ >= popped);
        stackPosition_ -= popped;
    }

    JS_ASSERT(info_.nslots() >= stackPosition_);
    JS_ASSERT(!entryResumePoint_);

    // Propagate the caller resume point from the inherited block.
    MResumePoint *callerResumePoint = pred ? pred->callerResumePoint() : nullptr;

    // Create a resume point using our initial stack state.
    entryResumePoint_ = new(alloc) MResumePoint(this, pc(), callerResumePoint, MResumePoint::ResumeAt);
    if (!entryResumePoint_->init(alloc))
        return false;

    if (pred) {
        if (!predecessors_.append(pred))
            return false;

        if (kind_ == PENDING_LOOP_HEADER) {
            size_t i = 0;
            for (i = 0; i < info().firstStackSlot(); i++) {
                MPhi *phi = MPhi::New(alloc);
                if (!phi->addInputSlow(pred->getSlot(i)))
                    return false;
                addPhi(phi);
                setSlot(i, phi);
                entryResumePoint()->initOperand(i, phi);
            }

            JS_ASSERT(stackPhiCount <= stackDepth());
            JS_ASSERT(info().firstStackSlot() <= stackDepth() - stackPhiCount);

            // Avoid creating new phis for stack values that aren't part of the
            // loop.  Note that for loop headers that can OSR, all values on the
            // stack are part of the loop.
            for (; i < stackDepth() - stackPhiCount; i++) {
                MDefinition *val = pred->getSlot(i);
                setSlot(i, val);
                entryResumePoint()->initOperand(i, val);
            }

            for (; i < stackDepth(); i++) {
                MPhi *phi = MPhi::New(alloc);
                if (!phi->addInputSlow(pred->getSlot(i)))
                    return false;
                addPhi(phi);
                setSlot(i, phi);
                entryResumePoint()->initOperand(i, phi);
            }
        } else {
            for (size_t i = 0; i < stackDepth(); i++)
                entryResumePoint()->initOperand(i, getSlot(i));
        }
    } else {
        /*
         * Don't leave the operands uninitialized for the caller, as it may not
         * initialize them later on.
         */
        for (size_t i = 0; i < stackDepth(); i++)
            entryResumePoint()->clearOperand(i);
    }

    return true;
}

bool
MBasicBlock::inheritResumePoint(MBasicBlock *pred)
{
    // Copy slots from the resume point.
    stackPosition_ = entryResumePoint_->numOperands();
    for (uint32_t i = 0; i < stackPosition_; i++)
        slots_[i] = entryResumePoint_->getOperand(i);

    JS_ASSERT(info_.nslots() >= stackPosition_);
    JS_ASSERT(kind_ != PENDING_LOOP_HEADER);
    JS_ASSERT(pred != nullptr);

    if (!predecessors_.append(pred))
        return false;

    return true;
}

void
MBasicBlock::inheritSlots(MBasicBlock *parent)
{
    stackPosition_ = parent->stackPosition_;
    copySlots(parent);
}

bool
MBasicBlock::initEntrySlots(TempAllocator &alloc)
{
    // Create a resume point using our initial stack state.
    entryResumePoint_ = MResumePoint::New(alloc, this, pc(), callerResumePoint(),
                                          MResumePoint::ResumeAt);
    if (!entryResumePoint_)
        return false;
    return true;
}

MDefinition *
MBasicBlock::getSlot(uint32_t index)
{
    JS_ASSERT(index < stackPosition_);
    return slots_[index];
}

void
MBasicBlock::initSlot(uint32_t slot, MDefinition *ins)
{
    slots_[slot] = ins;
    if (entryResumePoint())
        entryResumePoint()->initOperand(slot, ins);
}

void
MBasicBlock::shimmySlots(int discardDepth)
{
    // Move all slots above the given depth down by one,
    // overwriting the MDefinition at discardDepth.

    JS_ASSERT(discardDepth < 0);
    JS_ASSERT(stackPosition_ + discardDepth >= info_.firstStackSlot());

    for (int i = discardDepth; i < -1; i++)
        slots_[stackPosition_ + i] = slots_[stackPosition_ + i + 1];

    --stackPosition_;
}

void
MBasicBlock::linkOsrValues(MStart *start)
{
    JS_ASSERT(start->startType() == MStart::StartType_Osr);

    MResumePoint *res = start->resumePoint();

    for (uint32_t i = 0; i < stackDepth(); i++) {
        MDefinition *def = slots_[i];
        if (i == info().scopeChainSlot()) {
            if (def->isOsrScopeChain())
                def->toOsrScopeChain()->setResumePoint(res);
        } else if (i == info().returnValueSlot()) {
            if (def->isOsrReturnValue())
                def->toOsrReturnValue()->setResumePoint(res);
        } else if (info().hasArguments() && i == info().argsObjSlot()) {
            JS_ASSERT(def->isConstant() || def->isOsrArgumentsObject());
            JS_ASSERT_IF(def->isConstant(), def->toConstant()->value() == UndefinedValue());
            if (def->isOsrArgumentsObject())
                def->toOsrArgumentsObject()->setResumePoint(res);
        } else {
            JS_ASSERT(def->isOsrValue() || def->isGetArgumentsObjectArg() || def->isConstant() ||
                      def->isParameter());

            // A constant Undefined can show up here for an argument slot when the function uses
            // a heavyweight argsobj, but the argument in question is stored on the scope chain.
            JS_ASSERT_IF(def->isConstant(), def->toConstant()->value() == UndefinedValue());

            if (def->isOsrValue())
                def->toOsrValue()->setResumePoint(res);
            else if (def->isGetArgumentsObjectArg())
                def->toGetArgumentsObjectArg()->setResumePoint(res);
            else if (def->isParameter())
                def->toParameter()->setResumePoint(res);
        }
    }
}

void
MBasicBlock::setSlot(uint32_t slot, MDefinition *ins)
{
    slots_[slot] = ins;
}

void
MBasicBlock::setVariable(uint32_t index)
{
    JS_ASSERT(stackPosition_ > info_.firstStackSlot());
    setSlot(index, slots_[stackPosition_ - 1]);
}

void
MBasicBlock::setArg(uint32_t arg)
{
    setVariable(info_.argSlot(arg));
}

void
MBasicBlock::setLocal(uint32_t local)
{
    setVariable(info_.localSlot(local));
}

void
MBasicBlock::setSlot(uint32_t slot)
{
    setVariable(slot);
}

void
MBasicBlock::rewriteSlot(uint32_t slot, MDefinition *ins)
{
    setSlot(slot, ins);
}

void
MBasicBlock::rewriteAtDepth(int32_t depth, MDefinition *ins)
{
    JS_ASSERT(depth < 0);
    JS_ASSERT(stackPosition_ + depth >= info_.firstStackSlot());
    rewriteSlot(stackPosition_ + depth, ins);
}

void
MBasicBlock::push(MDefinition *ins)
{
    JS_ASSERT(stackPosition_ < nslots());
    slots_[stackPosition_++] = ins;
}

void
MBasicBlock::pushVariable(uint32_t slot)
{
    push(slots_[slot]);
}

void
MBasicBlock::pushArg(uint32_t arg)
{
    pushVariable(info_.argSlot(arg));
}

void
MBasicBlock::pushLocal(uint32_t local)
{
    pushVariable(info_.localSlot(local));
}

void
MBasicBlock::pushSlot(uint32_t slot)
{
    pushVariable(slot);
}

MDefinition *
MBasicBlock::pop()
{
    JS_ASSERT(stackPosition_ > info_.firstStackSlot());
    return slots_[--stackPosition_];
}

void
MBasicBlock::popn(uint32_t n)
{
    JS_ASSERT(stackPosition_ - n >= info_.firstStackSlot());
    JS_ASSERT(stackPosition_ >= stackPosition_ - n);
    stackPosition_ -= n;
}

MDefinition *
MBasicBlock::scopeChain()
{
    return getSlot(info().scopeChainSlot());
}

MDefinition *
MBasicBlock::argumentsObject()
{
    return getSlot(info().argsObjSlot());
}

void
MBasicBlock::setScopeChain(MDefinition *scopeObj)
{
    setSlot(info().scopeChainSlot(), scopeObj);
}

void
MBasicBlock::setArgumentsObject(MDefinition *argsObj)
{
    setSlot(info().argsObjSlot(), argsObj);
}

void
MBasicBlock::pick(int32_t depth)
{
    // pick take an element and move it to the top.
    // pick(-2):
    //   A B C D E
    //   A B D C E [ swapAt(-2) ]
    //   A B D E C [ swapAt(-1) ]
    for (; depth < 0; depth++)
        swapAt(depth);
}

void
MBasicBlock::swapAt(int32_t depth)
{
    uint32_t lhsDepth = stackPosition_ + depth - 1;
    uint32_t rhsDepth = stackPosition_ + depth;

    MDefinition *temp = slots_[lhsDepth];
    slots_[lhsDepth] = slots_[rhsDepth];
    slots_[rhsDepth] = temp;
}

MDefinition *
MBasicBlock::peek(int32_t depth)
{
    JS_ASSERT(depth < 0);
    JS_ASSERT(stackPosition_ + depth >= info_.firstStackSlot());
    return getSlot(stackPosition_ + depth);
}

void
MBasicBlock::discardLastIns()
{
    discard(lastIns());
}

void
MBasicBlock::addFromElsewhere(MInstruction *ins)
{
    JS_ASSERT(ins->block() != this);

    // Remove |ins| from its containing block.
    ins->block()->instructions_.remove(ins);

    // Add it to this block.
    add(ins);
}

void
MBasicBlock::moveBefore(MInstruction *at, MInstruction *ins)
{
    // Remove |ins| from the current block.
    JS_ASSERT(ins->block() == this);
    instructions_.remove(ins);

    // Insert into new block, which may be distinct.
    // Uses and operands are untouched.
    ins->setBlock(at->block());
    at->block()->instructions_.insertBefore(at, ins);
    ins->setTrackedSite(at->trackedSite());
}

static inline void
AssertSafelyDiscardable(MDefinition *def)
{
#ifdef DEBUG
    // Instructions captured by resume points cannot be safely discarded, since
    // they are necessary for interpreter frame reconstruction in case of bailout.
    JS_ASSERT(!def->hasUses());
#endif
}

void
MBasicBlock::discard(MInstruction *ins)
{
    AssertSafelyDiscardable(ins);
    for (size_t i = 0, e = ins->numOperands(); i < e; i++)
        ins->discardOperand(i);

    instructions_.remove(ins);
}

MInstructionIterator
MBasicBlock::discardAt(MInstructionIterator &iter)
{
    AssertSafelyDiscardable(*iter);
    for (size_t i = 0, e = iter->numOperands(); i < e; i++)
        iter->discardOperand(i);

    return instructions_.removeAt(iter);
}

MInstructionReverseIterator
MBasicBlock::discardAt(MInstructionReverseIterator &iter)
{
    AssertSafelyDiscardable(*iter);
    for (size_t i = 0, e = iter->numOperands(); i < e; i++)
        iter->discardOperand(i);

    return instructions_.removeAt(iter);
}

MDefinitionIterator
MBasicBlock::discardDefAt(MDefinitionIterator &old)
{
    MDefinitionIterator iter(old);

    if (iter.atPhi())
        iter.phiIter_ = iter.block_->discardPhiAt(iter.phiIter_);
    else
        iter.iter_ = iter.block_->discardAt(iter.iter_);

    return iter;
}

void
MBasicBlock::discardAllInstructions()
{
    MInstructionIterator iter = begin();
    discardAllInstructionsStartingAt(iter);

}

void
MBasicBlock::discardAllInstructionsStartingAt(MInstructionIterator &iter)
{
    while (iter != end()) {
        for (size_t i = 0, e = iter->numOperands(); i < e; i++)
            iter->discardOperand(i);
        iter = instructions_.removeAt(iter);
    }
}

void
MBasicBlock::discardAllPhiOperands()
{
    for (MPhiIterator iter = phisBegin(); iter != phisEnd(); iter++)
        iter->removeAllOperands();

    for (MBasicBlock **pred = predecessors_.begin(); pred != predecessors_.end(); pred++)
        (*pred)->setSuccessorWithPhis(nullptr, 0);
}

void
MBasicBlock::discardAllPhis()
{
    discardAllPhiOperands();
    phis_.clear();
}

void
MBasicBlock::discardAllResumePoints(bool discardEntry)
{
    for (MResumePointIterator iter = resumePointsBegin(); iter != resumePointsEnd(); ) {
        MResumePoint *rp = *iter;
        if (rp == entryResumePoint() && !discardEntry) {
            iter++;
        } else {
            rp->discardUses();
            iter = resumePoints_.removeAt(iter);
        }
    }
    if (discardEntry)
        clearEntryResumePoint();
}

void
MBasicBlock::insertBefore(MInstruction *at, MInstruction *ins)
{
    JS_ASSERT(at->block() == this);
    ins->setBlock(this);
    graph().allocDefinitionId(ins);
    instructions_.insertBefore(at, ins);
    ins->setTrackedSite(at->trackedSite());
}

void
MBasicBlock::insertAfter(MInstruction *at, MInstruction *ins)
{
    JS_ASSERT(at->block() == this);
    ins->setBlock(this);
    graph().allocDefinitionId(ins);
    instructions_.insertAfter(at, ins);
    ins->setTrackedSite(at->trackedSite());
}

void
MBasicBlock::add(MInstruction *ins)
{
    JS_ASSERT(!hasLastIns());
    ins->setBlock(this);
    graph().allocDefinitionId(ins);
    instructions_.pushBack(ins);
    ins->setTrackedSite(trackedSite_);
}

void
MBasicBlock::end(MControlInstruction *ins)
{
    JS_ASSERT(!hasLastIns()); // Existing control instructions should be removed first.
    JS_ASSERT(ins);
    add(ins);
}

void
MBasicBlock::addPhi(MPhi *phi)
{
    phis_.pushBack(phi);
    phi->setBlock(this);
    graph().allocDefinitionId(phi);
}

MPhiIterator
MBasicBlock::discardPhiAt(MPhiIterator &at)
{
    JS_ASSERT(!phis_.empty());

    at->removeAllOperands();

    MPhiIterator result = phis_.removeAt(at);

    if (phis_.empty()) {
        for (MBasicBlock **pred = predecessors_.begin(); pred != predecessors_.end(); pred++)
            (*pred)->setSuccessorWithPhis(nullptr, 0);
    }
    return result;
}

bool
MBasicBlock::addPredecessor(TempAllocator &alloc, MBasicBlock *pred)
{
    return addPredecessorPopN(alloc, pred, 0);
}

bool
MBasicBlock::addPredecessorPopN(TempAllocator &alloc, MBasicBlock *pred, uint32_t popped)
{
    JS_ASSERT(pred);
    JS_ASSERT(predecessors_.length() > 0);

    // Predecessors must be finished, and at the correct stack depth.
    JS_ASSERT(pred->hasLastIns());
    JS_ASSERT(pred->stackPosition_ == stackPosition_ + popped);

    for (uint32_t i = 0; i < stackPosition_; i++) {
        MDefinition *mine = getSlot(i);
        MDefinition *other = pred->getSlot(i);

        if (mine != other) {
            // If the current instruction is a phi, and it was created in this
            // basic block, then we have already placed this phi and should
            // instead append to its operands.
            if (mine->isPhi() && mine->block() == this) {
                JS_ASSERT(predecessors_.length());
                if (!mine->toPhi()->addInputSlow(other))
                    return false;
            } else {
                // Otherwise, create a new phi node.
                MPhi *phi;
                if (mine->type() == other->type())
                    phi = MPhi::New(alloc, mine->type());
                else
                    phi = MPhi::New(alloc);
                addPhi(phi);

                // Prime the phi for each predecessor, so input(x) comes from
                // predecessor(x).
                if (!phi->reserveLength(predecessors_.length() + 1))
                    return false;

                for (size_t j = 0; j < predecessors_.length(); j++) {
                    JS_ASSERT(predecessors_[j]->getSlot(i) == mine);
                    phi->addInput(mine);
                }
                phi->addInput(other);

                setSlot(i, phi);
                if (entryResumePoint())
                    entryResumePoint()->replaceOperand(i, phi);
            }
        }
    }

    return predecessors_.append(pred);
}

bool
MBasicBlock::addPredecessorWithoutPhis(MBasicBlock *pred)
{
    // Predecessors must be finished.
    JS_ASSERT(pred && pred->hasLastIns());
    return predecessors_.append(pred);
}

bool
MBasicBlock::addImmediatelyDominatedBlock(MBasicBlock *child)
{
    return immediatelyDominated_.append(child);
}

void
MBasicBlock::assertUsesAreNotWithin(MUseIterator use, MUseIterator end)
{
#ifdef DEBUG
    for (; use != end; use++) {
        JS_ASSERT_IF(use->consumer()->isDefinition(),
                     use->consumer()->toDefinition()->block()->id() < id());
    }
#endif
}

AbortReason
MBasicBlock::setBackedge(MBasicBlock *pred)
{
    // Predecessors must be finished, and at the correct stack depth.
    JS_ASSERT(hasLastIns());
    JS_ASSERT(pred->hasLastIns());
    JS_ASSERT(pred->stackDepth() == entryResumePoint()->stackDepth());

    // We must be a pending loop header
    JS_ASSERT(kind_ == PENDING_LOOP_HEADER);

    bool hadTypeChange = false;

    // Add exit definitions to each corresponding phi at the entry.
    if (!inheritPhisFromBackedge(pred, &hadTypeChange))
        return AbortReason_Alloc;

    if (hadTypeChange) {
        for (MPhiIterator phi = phisBegin(); phi != phisEnd(); phi++)
            phi->removeOperand(phi->numOperands() - 1);
        return AbortReason_Disable;
    }

    // We are now a loop header proper
    kind_ = LOOP_HEADER;

    if (!predecessors_.append(pred))
        return AbortReason_Alloc;

    return AbortReason_NoAbort;
}

bool
MBasicBlock::setBackedgeAsmJS(MBasicBlock *pred)
{
    // Predecessors must be finished, and at the correct stack depth.
    JS_ASSERT(hasLastIns());
    JS_ASSERT(pred->hasLastIns());
    JS_ASSERT(stackDepth() == pred->stackDepth());

    // We must be a pending loop header
    JS_ASSERT(kind_ == PENDING_LOOP_HEADER);

    // Add exit definitions to each corresponding phi at the entry.
    // Note: Phis are inserted in the same order as the slots. (see
    // MBasicBlock::NewAsmJS)
    size_t slot = 0;
    for (MPhiIterator phi = phisBegin(); phi != phisEnd(); phi++, slot++) {
        MPhi *entryDef = *phi;
        MDefinition *exitDef = pred->getSlot(slot);

        // Assert that we already placed phis for each slot.
        JS_ASSERT(entryDef->block() == this);

        // Assert that the phi already has the correct type.
        JS_ASSERT(entryDef->type() == exitDef->type());
        JS_ASSERT(entryDef->type() != MIRType_Value);

        if (entryDef == exitDef) {
            // If the exit def is the same as the entry def, make a redundant
            // phi. Since loop headers have exactly two incoming edges, we
            // know that that's just the first input.
            //
            // Note that we eliminate later rather than now, to avoid any
            // weirdness around pending continue edges which might still hold
            // onto phis.
            exitDef = entryDef->getOperand(0);
        }

        // MBasicBlock::NewAsmJS calls reserveLength(2) for loop header phis.
        entryDef->addInput(exitDef);

        MOZ_ASSERT(slot < pred->stackDepth());
        setSlot(slot, entryDef);
    }

    // We are now a loop header proper
    kind_ = LOOP_HEADER;

    return predecessors_.append(pred);
}

void
MBasicBlock::clearLoopHeader()
{
    JS_ASSERT(isLoopHeader());
    kind_ = NORMAL;
}

size_t
MBasicBlock::numSuccessors() const
{
    JS_ASSERT(lastIns());
    return lastIns()->numSuccessors();
}

MBasicBlock *
MBasicBlock::getSuccessor(size_t index) const
{
    JS_ASSERT(lastIns());
    return lastIns()->getSuccessor(index);
}

size_t
MBasicBlock::getSuccessorIndex(MBasicBlock *block) const
{
    JS_ASSERT(lastIns());
    for (size_t i = 0; i < numSuccessors(); i++) {
        if (getSuccessor(i) == block)
            return i;
    }
    MOZ_ASSUME_UNREACHABLE("Invalid successor");
}

void
MBasicBlock::replaceSuccessor(size_t pos, MBasicBlock *split)
{
    JS_ASSERT(lastIns());

    // Note, during split-critical-edges, successors-with-phis is not yet set.
    // During PAA, this case is handled before we enter.
    JS_ASSERT_IF(successorWithPhis_, successorWithPhis_ != getSuccessor(pos));

    lastIns()->replaceSuccessor(pos, split);
}

void
MBasicBlock::replacePredecessor(MBasicBlock *old, MBasicBlock *split)
{
    for (size_t i = 0; i < numPredecessors(); i++) {
        if (getPredecessor(i) == old) {
            predecessors_[i] = split;

#ifdef DEBUG
            // The same block should not appear twice in the predecessor list.
            for (size_t j = i; j < numPredecessors(); j++)
                JS_ASSERT(predecessors_[j] != old);
#endif

            return;
        }
    }

    MOZ_ASSUME_UNREACHABLE("predecessor was not found");
}

void
MBasicBlock::clearDominatorInfo()
{
    setImmediateDominator(nullptr);
    immediatelyDominated_.clear();
    numDominated_ = 0;
}

void
MBasicBlock::removePredecessor(MBasicBlock *pred)
{
    // If we're removing the last backedge, this is no longer a loop.
    if (isLoopHeader() && hasUniqueBackedge() && backedge() == pred)
        clearLoopHeader();

    for (size_t i = 0; i < numPredecessors(); i++) {
        if (getPredecessor(i) != pred)
            continue;

        // Adjust phis.  Note that this can leave redundant phis
        // behind.
        if (!phisEmpty()) {
            JS_ASSERT(pred->successorWithPhis());
            JS_ASSERT(pred->positionInPhiSuccessor() == i);
            for (MPhiIterator iter = phisBegin(); iter != phisEnd(); iter++)
                iter->removeOperand(i);
            pred->setSuccessorWithPhis(nullptr, 0);
            for (size_t j = i+1; j < numPredecessors(); j++)
                getPredecessor(j)->setSuccessorWithPhis(this, j - 1);
        }

        // Remove from pred list.
        MBasicBlock **ptr = predecessors_.begin() + i;
        predecessors_.erase(ptr);
        return;
    }

    MOZ_ASSUME_UNREACHABLE("predecessor was not found");
}

void
MBasicBlock::inheritPhis(MBasicBlock *header)
{
    MResumePoint *headerRp = header->entryResumePoint();
    size_t stackDepth = headerRp->numOperands();
    for (size_t slot = 0; slot < stackDepth; slot++) {
        MDefinition *exitDef = getSlot(slot);
        MDefinition *loopDef = headerRp->getOperand(slot);
        if (loopDef->block() != header) {
            MOZ_ASSERT(loopDef->block()->id() < header->id());
            MOZ_ASSERT(loopDef == exitDef);
            continue;
        }

        // Phis are allocated by NewPendingLoopHeader.
        MPhi *phi = loopDef->toPhi();
        MOZ_ASSERT(phi->numOperands() == 2);

        // The entry definition is always the leftmost input to the phi.
        MDefinition *entryDef = phi->getOperand(0);

        if (entryDef != exitDef)
            continue;

        // If the entryDef is the same as exitDef, then we must propagate the
        // phi down to this successor. This chance was missed as part of
        // setBackedge() because exits are not captured in resume points.
        setSlot(slot, phi);
    }
}

bool
MBasicBlock::inheritPhisFromBackedge(MBasicBlock *backedge, bool *hadTypeChange)
{
    // We must be a pending loop header
    MOZ_ASSERT(kind_ == PENDING_LOOP_HEADER);

    size_t stackDepth = entryResumePoint()->numOperands();
    for (size_t slot = 0; slot < stackDepth; slot++) {
        // Get the value stack-slot of the back edge.
        MDefinition *exitDef = backedge->getSlot(slot);

        // Get the value of the loop header.
        MDefinition *loopDef = entryResumePoint()->getOperand(slot);
        if (loopDef->block() != this) {
            // If we are finishing a pending loop header, then we need to ensure
            // that all operands are phis. This is usualy the case, except for
            // object/arrays build with generators, in which case we share the
            // same allocations across all blocks.
            MOZ_ASSERT(loopDef->block()->id() < id());
            MOZ_ASSERT(loopDef == exitDef);
            continue;
        }

        // Phis are allocated by NewPendingLoopHeader.
        MPhi *entryDef = loopDef->toPhi();
        MOZ_ASSERT(entryDef->block() == this);

        if (entryDef == exitDef) {
            // If the exit def is the same as the entry def, make a redundant
            // phi. Since loop headers have exactly two incoming edges, we
            // know that that's just the first input.
            //
            // Note that we eliminate later rather than now, to avoid any
            // weirdness around pending continue edges which might still hold
            // onto phis.
            exitDef = entryDef->getOperand(0);
        }

        bool typeChange = false;

        if (!entryDef->addInputSlow(exitDef, &typeChange))
            return false;

        *hadTypeChange |= typeChange;
        setSlot(slot, entryDef);
    }

    return true;
}

bool
MBasicBlock::specializePhis()
{
    for (MPhiIterator iter = phisBegin(); iter != phisEnd(); iter++) {
        MPhi *phi = *iter;
        if (!phi->specializeType())
            return false;
    }
    return true;
}

void
MBasicBlock::dumpStack(FILE *fp)
{
#ifdef DEBUG
    fprintf(fp, " %-3s %-16s %-6s %-10s\n", "#", "name", "copyOf", "first/next");
    fprintf(fp, "-------------------------------------------\n");
    for (uint32_t i = 0; i < stackPosition_; i++) {
        fprintf(fp, " %-3d", i);
        fprintf(fp, " %-16p\n", (void *)slots_[i]);
    }
#endif
}

MTest *
MBasicBlock::immediateDominatorBranch(BranchDirection *pdirection)
{
    *pdirection = FALSE_BRANCH;

    if (numPredecessors() != 1)
        return nullptr;

    MBasicBlock *dom = immediateDominator();
    if (dom != getPredecessor(0))
        return nullptr;

    // Look for a trailing MTest branching to this block.
    MInstruction *ins = dom->lastIns();
    if (ins->isTest()) {
        MTest *test = ins->toTest();

        JS_ASSERT(test->ifTrue() == this || test->ifFalse() == this);
        if (test->ifTrue() == this && test->ifFalse() == this)
            return nullptr;

        *pdirection = (test->ifTrue() == this) ? TRUE_BRANCH : FALSE_BRANCH;
        return test;
    }

    return nullptr;
}

void
MIRGraph::dump(FILE *fp)
{
#ifdef DEBUG
    for (MBasicBlockIterator iter(begin()); iter != end(); iter++) {
        iter->dump(fp);
        fprintf(fp, "\n");
    }
#endif
}

void
MIRGraph::dump()
{
    dump(stderr);
}

void
MBasicBlock::dump(FILE *fp)
{
#ifdef DEBUG
    fprintf(fp, "block%u:\n", id());
    if (MResumePoint *resume = entryResumePoint()) {
        resume->dump();
    }
    for (MPhiIterator iter(phisBegin()); iter != phisEnd(); iter++) {
        iter->dump(fp);
    }
    for (MInstructionIterator iter(begin()); iter != end(); iter++) {
        iter->dump(fp);
    }
#endif
}

void
MBasicBlock::dump()
{
    dump(stderr);
}
