/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/IonAnalysis.h"

#include "jit/BaselineInspector.h"
#include "jit/BaselineJIT.h"
#include "jit/Ion.h"
#include "jit/IonBuilder.h"
#include "jit/IonOptimizationLevels.h"
#include "jit/LIR.h"
#include "jit/Lowering.h"
#include "jit/MIRGraph.h"

#include "jsinferinlines.h"
#include "jsobjinlines.h"
#include "jsopcodeinlines.h"

using namespace js;
using namespace js::jit;

using mozilla::DebugOnly;

// A critical edge is an edge which is neither its successor's only predecessor
// nor its predecessor's only successor. Critical edges must be split to
// prevent copy-insertion and code motion from affecting other edges.
bool
jit::SplitCriticalEdges(MIRGraph &graph)
{
    for (MBasicBlockIterator block(graph.begin()); block != graph.end(); block++) {
        if (block->numSuccessors() < 2)
            continue;
        for (size_t i = 0; i < block->numSuccessors(); i++) {
            MBasicBlock *target = block->getSuccessor(i);
            if (target->numPredecessors() < 2)
                continue;

            // Create a new block inheriting from the predecessor.
            MBasicBlock *split = MBasicBlock::NewSplitEdge(graph, block->info(), *block);
            if (!split)
                return false;
            split->setLoopDepth(block->loopDepth());
            graph.insertBlockAfter(*block, split);
            split->end(MGoto::New(graph.alloc(), target));

            block->replaceSuccessor(i, split);
            target->replacePredecessor(*block, split);
        }
    }
    return true;
}

// Operands to a resume point which are dead at the point of the resume can be
// replaced with a magic value. This analysis supports limited detection of
// dead operands, pruning those which are defined in the resume point's basic
// block and have no uses outside the block or at points later than the resume
// point.
//
// This is intended to ensure that extra resume points within a basic block
// will not artificially extend the lifetimes of any SSA values. This could
// otherwise occur if the new resume point captured a value which is created
// between the old and new resume point and is dead at the new resume point.
bool
jit::EliminateDeadResumePointOperands(MIRGenerator *mir, MIRGraph &graph)
{
    // If we are compiling try blocks, locals and arguments may be observable
    // from catch or finally blocks (which Ion does not compile). For now just
    // disable the pass in this case.
    if (graph.hasTryBlock())
        return true;

    for (PostorderIterator block = graph.poBegin(); block != graph.poEnd(); block++) {
        if (mir->shouldCancel("Eliminate Dead Resume Point Operands (main loop)"))
            return false;

        // The logic below can get confused on infinite loops.
        if (block->isLoopHeader() && block->backedge() == *block)
            continue;

        for (MInstructionIterator ins = block->begin(); ins != block->end(); ins++) {
            // No benefit to replacing constant operands with other constants.
            if (ins->isConstant())
                continue;

            // Scanning uses does not give us sufficient information to tell
            // where instructions that are involved in box/unbox operations or
            // parameter passing might be live. Rewriting uses of these terms
            // in resume points may affect the interpreter's behavior. Rather
            // than doing a more sophisticated analysis, just ignore these.
            if (ins->isUnbox() || ins->isParameter() || ins->isTypeBarrier() || ins->isComputeThis())
                continue;

            // TypedObject intermediate values captured by resume points may
            // be legitimately dead in Ion code, but are still needed if we
            // bail out. They can recover on bailout.
            if (ins->isNewDerivedTypedObject()) {
                MOZ_ASSERT(ins->canRecoverOnBailout());
                continue;
            }

            // If the instruction's behavior has been constant folded into a
            // separate instruction, we can't determine precisely where the
            // instruction becomes dead and can't eliminate its uses.
            if (ins->isImplicitlyUsed())
                continue;

            // Check if this instruction's result is only used within the
            // current block, and keep track of its last use in a definition
            // (not resume point). This requires the instructions in the block
            // to be numbered, ensured by running this immediately after alias
            // analysis.
            uint32_t maxDefinition = 0;
            for (MUseIterator uses(ins->usesBegin()); uses != ins->usesEnd(); uses++) {
                MNode *consumer = uses->consumer();
                if (consumer->isResumePoint()) {
                    // If the instruction's is captured by one of the resume point, then
                    // it might be observed indirectly while the frame is live on the
                    // stack, so it has to be computed.
                    MResumePoint *resume = consumer->toResumePoint();
                    if (resume->isObservableOperand(*uses)) {
                        maxDefinition = UINT32_MAX;
                        break;
                    }
                    continue;
                }

                MDefinition *def = consumer->toDefinition();
                if (def->block() != *block || def->isBox() || def->isPhi()) {
                    maxDefinition = UINT32_MAX;
                    break;
                }
                maxDefinition = Max(maxDefinition, def->id());
            }
            if (maxDefinition == UINT32_MAX)
                continue;

            // Walk the uses a second time, removing any in resume points after
            // the last use in a definition.
            for (MUseIterator uses(ins->usesBegin()); uses != ins->usesEnd(); ) {
                MUse *use = *uses++;
                if (use->consumer()->isDefinition())
                    continue;
                MResumePoint *mrp = use->consumer()->toResumePoint();
                if (mrp->block() != *block ||
                    !mrp->instruction() ||
                    mrp->instruction() == *ins ||
                    mrp->instruction()->id() <= maxDefinition)
                {
                    continue;
                }

                // Store an optimized out magic value in place of all dead
                // resume point operands. Making any such substitution can in
                // general alter the interpreter's behavior, even though the
                // code is dead, as the interpreter will still execute opcodes
                // whose effects cannot be observed. If the magic value value
                // were to flow to, say, a dead property access the
                // interpreter could throw an exception; we avoid this problem
                // by removing dead operands before removing dead code.
                MConstant *constant = MConstant::New(graph.alloc(), MagicValue(JS_OPTIMIZED_OUT));
                block->insertBefore(*(block->begin()), constant);
                use->replaceProducer(constant);
            }
        }
    }

    return true;
}

// Instructions are useless if they are unused and have no side effects.
// This pass eliminates useless instructions.
// The graph itself is unchanged.
bool
jit::EliminateDeadCode(MIRGenerator *mir, MIRGraph &graph)
{
    // Traverse in postorder so that we hit uses before definitions.
    // Traverse instruction list backwards for the same reason.
    for (PostorderIterator block = graph.poBegin(); block != graph.poEnd(); block++) {
        if (mir->shouldCancel("Eliminate Dead Code (main loop)"))
            return false;

        // Remove unused instructions.
        for (MInstructionReverseIterator inst = block->rbegin(); inst != block->rend(); ) {
            if (!inst->isEffectful() && !inst->resumePoint() &&
                !inst->hasUses() && !inst->isGuard() &&
                !inst->isControlInstruction()) {
                inst = block->discardAt(inst);
            } else if (!inst->hasLiveDefUses() && inst->canRecoverOnBailout()) {
                inst->setRecoveredOnBailout();
                inst++;
            } else {
                inst++;
            }
        }
    }

    return true;
}

static inline bool
IsPhiObservable(MPhi *phi, Observability observe)
{
    // If the phi has uses which are not reflected in SSA, then behavior in the
    // interpreter may be affected by removing the phi.
    if (phi->isImplicitlyUsed())
        return true;

    // Check for uses of this phi node outside of other phi nodes.
    // Note that, initially, we skip reading resume points, which we
    // don't count as actual uses. If the only uses are resume points,
    // then the SSA name is never consumed by the program.  However,
    // after optimizations have been performed, it's possible that the
    // actual uses in the program have been (incorrectly) optimized
    // away, so we must be more conservative and consider resume
    // points as well.
    for (MUseIterator iter(phi->usesBegin()); iter != phi->usesEnd(); iter++) {
        MNode *consumer = iter->consumer();
        if (consumer->isResumePoint()) {
            MResumePoint *resume = consumer->toResumePoint();
            if (observe == ConservativeObservability)
                return true;
            if (resume->isObservableOperand(*iter))
                return true;
        } else {
            MDefinition *def = consumer->toDefinition();
            if (!def->isPhi())
                return true;
        }
    }

    return false;
}

// Handles cases like:
//    x is phi(a, x) --> a
//    x is phi(a, a) --> a
static inline MDefinition *
IsPhiRedundant(MPhi *phi)
{
    MDefinition *first = phi->operandIfRedundant();
    if (first == nullptr)
        return nullptr;

    // Propagate the ImplicitlyUsed flag if |phi| is replaced with another phi.
    if (phi->isImplicitlyUsed())
        first->setImplicitlyUsedUnchecked();

    return first;
}

bool
jit::EliminatePhis(MIRGenerator *mir, MIRGraph &graph,
                   Observability observe)
{
    // Eliminates redundant or unobservable phis from the graph.  A
    // redundant phi is something like b = phi(a, a) or b = phi(a, b),
    // both of which can be replaced with a.  An unobservable phi is
    // one that whose value is never used in the program.
    //
    // Note that we must be careful not to eliminate phis representing
    // values that the interpreter will require later.  When the graph
    // is first constructed, we can be more aggressive, because there
    // is a greater correspondence between the CFG and the bytecode.
    // After optimizations such as GVN have been performed, however,
    // the bytecode and CFG may not correspond as closely to one
    // another.  In that case, we must be more conservative.  The flag
    // |conservativeObservability| is used to indicate that eliminate
    // phis is being run after some optimizations have been performed,
    // and thus we should use more conservative rules about
    // observability.  The particular danger is that we can optimize
    // away uses of a phi because we think they are not executable,
    // but the foundation for that assumption is false TI information
    // that will eventually be invalidated.  Therefore, if
    // |conservativeObservability| is set, we will consider any use
    // from a resume point to be observable.  Otherwise, we demand a
    // use from an actual instruction.

    Vector<MPhi *, 16, SystemAllocPolicy> worklist;

    // Add all observable phis to a worklist. We use the "in worklist" bit to
    // mean "this phi is live".
    for (PostorderIterator block = graph.poBegin(); block != graph.poEnd(); block++) {
        if (mir->shouldCancel("Eliminate Phis (populate loop)"))
            return false;

        MPhiIterator iter = block->phisBegin();
        while (iter != block->phisEnd()) {
            // Flag all as unused, only observable phis would be marked as used
            // when processed by the work list.
            iter->setUnused();

            // If the phi is redundant, remove it here.
            if (MDefinition *redundant = IsPhiRedundant(*iter)) {
                iter->replaceAllUsesWith(redundant);
                iter = block->discardPhiAt(iter);
                continue;
            }

            // Enqueue observable Phis.
            if (IsPhiObservable(*iter, observe)) {
                iter->setInWorklist();
                if (!worklist.append(*iter))
                    return false;
            }
            iter++;
        }
    }

    // Iteratively mark all phis reachable from live phis.
    while (!worklist.empty()) {
        if (mir->shouldCancel("Eliminate Phis (worklist)"))
            return false;

        MPhi *phi = worklist.popCopy();
        JS_ASSERT(phi->isUnused());
        phi->setNotInWorklist();

        // The removal of Phis can produce newly redundant phis.
        if (MDefinition *redundant = IsPhiRedundant(phi)) {
            // Add to the worklist the used phis which are impacted.
            for (MUseDefIterator it(phi); it; it++) {
                if (it.def()->isPhi()) {
                    MPhi *use = it.def()->toPhi();
                    if (!use->isUnused()) {
                        use->setUnusedUnchecked();
                        use->setInWorklist();
                        if (!worklist.append(use))
                            return false;
                    }
                }
            }
            phi->replaceAllUsesWith(redundant);
        } else {
            // Otherwise flag them as used.
            phi->setNotUnused();
        }

        // The current phi is/was used, so all its operands are used.
        for (size_t i = 0, e = phi->numOperands(); i < e; i++) {
            MDefinition *in = phi->getOperand(i);
            if (!in->isPhi() || !in->isUnused() || in->isInWorklist())
                continue;
            in->setInWorklist();
            if (!worklist.append(in->toPhi()))
                return false;
        }
    }

    // Sweep dead phis.
    for (PostorderIterator block = graph.poBegin(); block != graph.poEnd(); block++) {
        MPhiIterator iter = block->phisBegin();
        while (iter != block->phisEnd()) {
            if (iter->isUnused())
                iter = block->discardPhiAt(iter);
            else
                iter++;
        }
    }

    return true;
}

namespace {

// The type analysis algorithm inserts conversions and box/unbox instructions
// to make the IR graph well-typed for future passes.
//
// Phi adjustment: If a phi's inputs are all the same type, the phi is
// specialized to return that type.
//
// Input adjustment: Each input is asked to apply conversion operations to its
// inputs. This may include Box, Unbox, or other instruction-specific type
// conversion operations.
//
class TypeAnalyzer
{
    MIRGenerator *mir;
    MIRGraph &graph;
    Vector<MPhi *, 0, SystemAllocPolicy> phiWorklist_;

    TempAllocator &alloc() const {
        return graph.alloc();
    }

    bool addPhiToWorklist(MPhi *phi) {
        if (phi->isInWorklist())
            return true;
        if (!phiWorklist_.append(phi))
            return false;
        phi->setInWorklist();
        return true;
    }
    MPhi *popPhi() {
        MPhi *phi = phiWorklist_.popCopy();
        phi->setNotInWorklist();
        return phi;
    }

    bool respecialize(MPhi *phi, MIRType type);
    bool propagateSpecialization(MPhi *phi);
    bool specializePhis();
    void replaceRedundantPhi(MPhi *phi);
    void adjustPhiInputs(MPhi *phi);
    bool adjustInputs(MDefinition *def);
    bool insertConversions();

    bool checkFloatCoherency();
    bool graphContainsFloat32();
    bool markPhiConsumers();
    bool markPhiProducers();
    bool specializeValidFloatOps();
    bool tryEmitFloatOperations();

  public:
    TypeAnalyzer(MIRGenerator *mir, MIRGraph &graph)
      : mir(mir), graph(graph)
    { }

    bool analyze();
};

} /* anonymous namespace */

// Try to specialize this phi based on its non-cyclic inputs.
static MIRType
GuessPhiType(MPhi *phi, bool *hasInputsWithEmptyTypes)
{
#ifdef DEBUG
    // Check that different magic constants aren't flowing together. Ignore
    // JS_OPTIMIZED_OUT, since an operand could be legitimately optimized
    // away.
    MIRType magicType = MIRType_None;
    for (size_t i = 0; i < phi->numOperands(); i++) {
        MDefinition *in = phi->getOperand(i);
        if (in->type() == MIRType_MagicOptimizedArguments ||
            in->type() == MIRType_MagicHole ||
            in->type() == MIRType_MagicIsConstructing)
        {
            if (magicType == MIRType_None)
                magicType = in->type();
            MOZ_ASSERT(magicType == in->type());
        }
    }
#endif

    *hasInputsWithEmptyTypes = false;

    MIRType type = MIRType_None;
    bool convertibleToFloat32 = false;
    bool hasPhiInputs = false;
    for (size_t i = 0, e = phi->numOperands(); i < e; i++) {
        MDefinition *in = phi->getOperand(i);
        if (in->isPhi()) {
            hasPhiInputs = true;
            if (!in->toPhi()->triedToSpecialize())
                continue;
            if (in->type() == MIRType_None) {
                // The operand is a phi we tried to specialize, but we were
                // unable to guess its type. propagateSpecialization will
                // propagate the type to this phi when it becomes known.
                continue;
            }
        }

        // Ignore operands which we've never observed.
        if (in->resultTypeSet() && in->resultTypeSet()->empty()) {
            *hasInputsWithEmptyTypes = true;
            continue;
        }

        if (type == MIRType_None) {
            type = in->type();
            if (in->canProduceFloat32())
                convertibleToFloat32 = true;
            continue;
        }
        if (type != in->type()) {
            if (convertibleToFloat32 && in->type() == MIRType_Float32) {
                // If we only saw definitions that can be converted into Float32 before and
                // encounter a Float32 value, promote previous values to Float32
                type = MIRType_Float32;
            } else if (IsNumberType(type) && IsNumberType(in->type())) {
                // Specialize phis with int32 and double operands as double.
                type = MIRType_Double;
                convertibleToFloat32 &= in->canProduceFloat32();
            } else {
                return MIRType_Value;
            }
        }
    }

    if (type == MIRType_None && !hasPhiInputs) {
        // All inputs are non-phis with empty typesets. Use MIRType_Value
        // in this case, as it's impossible to get better type information.
        JS_ASSERT(*hasInputsWithEmptyTypes);
        type = MIRType_Value;
    }

    return type;
}

bool
TypeAnalyzer::respecialize(MPhi *phi, MIRType type)
{
    if (phi->type() == type)
        return true;
    phi->specialize(type);
    return addPhiToWorklist(phi);
}

bool
TypeAnalyzer::propagateSpecialization(MPhi *phi)
{
    JS_ASSERT(phi->type() != MIRType_None);

    // Verify that this specialization matches any phis depending on it.
    for (MUseDefIterator iter(phi); iter; iter++) {
        if (!iter.def()->isPhi())
            continue;
        MPhi *use = iter.def()->toPhi();
        if (!use->triedToSpecialize())
            continue;
        if (use->type() == MIRType_None) {
            // We tried to specialize this phi, but were unable to guess its
            // type. Now that we know the type of one of its operands, we can
            // specialize it.
            if (!respecialize(use, phi->type()))
                return false;
            continue;
        }
        if (use->type() != phi->type()) {
            // Specialize phis with int32 that can be converted to float and float operands as floats.
            if ((use->type() == MIRType_Int32 && use->canProduceFloat32() && phi->type() == MIRType_Float32) ||
                (phi->type() == MIRType_Int32 && phi->canProduceFloat32() && use->type() == MIRType_Float32))
            {
                if (!respecialize(use, MIRType_Float32))
                    return false;
                continue;
            }

            // Specialize phis with int32 and double operands as double.
            if (IsNumberType(use->type()) && IsNumberType(phi->type())) {
                if (!respecialize(use, MIRType_Double))
                    return false;
                continue;
            }

            // This phi in our use chain can now no longer be specialized.
            if (!respecialize(use, MIRType_Value))
                return false;
        }
    }

    return true;
}

bool
TypeAnalyzer::specializePhis()
{
    Vector<MPhi *, 0, SystemAllocPolicy> phisWithEmptyInputTypes;

    for (PostorderIterator block(graph.poBegin()); block != graph.poEnd(); block++) {
        if (mir->shouldCancel("Specialize Phis (main loop)"))
            return false;

        for (MPhiIterator phi(block->phisBegin()); phi != block->phisEnd(); phi++) {
            bool hasInputsWithEmptyTypes;
            MIRType type = GuessPhiType(*phi, &hasInputsWithEmptyTypes);
            phi->specialize(type);
            if (type == MIRType_None) {
                // We tried to guess the type but failed because all operands are
                // phis we still have to visit. Set the triedToSpecialize flag but
                // don't propagate the type to other phis, propagateSpecialization
                // will do that once we know the type of one of the operands.

                // Edge case: when this phi has a non-phi input with an empty
                // typeset, it's possible for two phis to have a cyclic
                // dependency and they will both have MIRType_None. Specialize
                // such phis to MIRType_Value later on.
                if (hasInputsWithEmptyTypes && !phisWithEmptyInputTypes.append(*phi))
                    return false;
                continue;
            }
            if (!propagateSpecialization(*phi))
                return false;
        }
    }

    do {
        while (!phiWorklist_.empty()) {
            if (mir->shouldCancel("Specialize Phis (worklist)"))
                return false;

            MPhi *phi = popPhi();
            if (!propagateSpecialization(phi))
                return false;
        }

        // When two phis have a cyclic dependency and inputs that have an empty
        // typeset (which are ignored by GuessPhiType), we may still have to
        // specialize these to MIRType_Value.
        while (!phisWithEmptyInputTypes.empty()) {
            if (mir->shouldCancel("Specialize Phis (phisWithEmptyInputTypes)"))
                return false;

            MPhi *phi = phisWithEmptyInputTypes.popCopy();
            if (phi->type() == MIRType_None) {
                phi->specialize(MIRType_Value);
                if (!propagateSpecialization(phi))
                    return false;
            }
        }
    } while (!phiWorklist_.empty());

    return true;
}

void
TypeAnalyzer::adjustPhiInputs(MPhi *phi)
{
    MIRType phiType = phi->type();
    JS_ASSERT(phiType != MIRType_None);

    // If we specialized a type that's not Value, there are 3 cases:
    // 1. Every input is of that type.
    // 2. Every observed input is of that type (i.e., some inputs haven't been executed yet).
    // 3. Inputs were doubles and int32s, and was specialized to double.
    if (phiType != MIRType_Value) {
        for (size_t i = 0, e = phi->numOperands(); i < e; i++) {
            MDefinition *in = phi->getOperand(i);
            if (in->type() == phiType)
                continue;

            if (in->isBox() && in->toBox()->input()->type() == phiType) {
                phi->replaceOperand(i, in->toBox()->input());
            } else {
                MInstruction *replacement;

                if (phiType == MIRType_Double && IsFloatType(in->type())) {
                    // Convert int32 operands to double.
                    replacement = MToDouble::New(alloc(), in);
                } else if (phiType == MIRType_Float32) {
                    if (in->type() == MIRType_Int32 || in->type() == MIRType_Double) {
                        replacement = MToFloat32::New(alloc(), in);
                    } else {
                        // See comment below
                        if (in->type() != MIRType_Value) {
                            MBox *box = MBox::New(alloc(), in);
                            in->block()->insertBefore(in->block()->lastIns(), box);
                            in = box;
                        }

                        MUnbox *unbox = MUnbox::New(alloc(), in, MIRType_Double, MUnbox::Fallible);
                        in->block()->insertBefore(in->block()->lastIns(), unbox);
                        replacement = MToFloat32::New(alloc(), in);
                    }
                } else {
                    // If we know this branch will fail to convert to phiType,
                    // insert a box that'll immediately fail in the fallible unbox
                    // below.
                    if (in->type() != MIRType_Value) {
                        MBox *box = MBox::New(alloc(), in);
                        in->block()->insertBefore(in->block()->lastIns(), box);
                        in = box;
                    }

                    // Be optimistic and insert unboxes when the operand is a
                    // value.
                    replacement = MUnbox::New(alloc(), in, phiType, MUnbox::Fallible);
                }

                in->block()->insertBefore(in->block()->lastIns(), replacement);
                phi->replaceOperand(i, replacement);
            }
        }

        return;
    }

    // Box every typed input.
    for (size_t i = 0, e = phi->numOperands(); i < e; i++) {
        MDefinition *in = phi->getOperand(i);
        if (in->type() == MIRType_Value)
            continue;

        if (in->isUnbox() && phi->typeIncludes(in->toUnbox()->input())) {
            // The input is being explicitly unboxed, so sneak past and grab
            // the original box.
            phi->replaceOperand(i, in->toUnbox()->input());
        } else {
            MDefinition *box = BoxInputsPolicy::alwaysBoxAt(alloc(), in->block()->lastIns(), in);
            phi->replaceOperand(i, box);
        }
    }
}

bool
TypeAnalyzer::adjustInputs(MDefinition *def)
{
    TypePolicy *policy = def->typePolicy();
    if (policy && !policy->adjustInputs(alloc(), def->toInstruction()))
        return false;
    return true;
}

void
TypeAnalyzer::replaceRedundantPhi(MPhi *phi)
{
    MBasicBlock *block = phi->block();
    js::Value v;
    switch (phi->type()) {
      case MIRType_Undefined:
        v = UndefinedValue();
        break;
      case MIRType_Null:
        v = NullValue();
        break;
      case MIRType_MagicOptimizedArguments:
        v = MagicValue(JS_OPTIMIZED_ARGUMENTS);
        break;
      case MIRType_MagicOptimizedOut:
        v = MagicValue(JS_OPTIMIZED_OUT);
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("unexpected type");
    }
    MConstant *c = MConstant::New(alloc(), v);
    // The instruction pass will insert the box
    block->insertBefore(*(block->begin()), c);
    phi->replaceAllUsesWith(c);
}

bool
TypeAnalyzer::insertConversions()
{
    // Instructions are processed in reverse postorder: all uses are defs are
    // seen before uses. This ensures that output adjustment (which may rewrite
    // inputs of uses) does not conflict with input adjustment.
    for (ReversePostorderIterator block(graph.rpoBegin()); block != graph.rpoEnd(); block++) {
        if (mir->shouldCancel("Insert Conversions"))
            return false;

        for (MPhiIterator phi(block->phisBegin()); phi != block->phisEnd();) {
            if (phi->type() == MIRType_Undefined ||
                phi->type() == MIRType_Null ||
                phi->type() == MIRType_MagicOptimizedArguments ||
                phi->type() == MIRType_MagicOptimizedOut)
            {
                replaceRedundantPhi(*phi);
                phi = block->discardPhiAt(phi);
            } else {
                adjustPhiInputs(*phi);
                phi++;
            }
        }
        for (MInstructionIterator iter(block->begin()); iter != block->end(); iter++) {
            if (!adjustInputs(*iter))
                return false;
        }
    }
    return true;
}

// This function tries to emit Float32 specialized operations whenever it's possible.
// MIR nodes are flagged as:
// - Producers, when they can create Float32 that might need to be coerced into a Double.
//   Loads in Float32 arrays and conversions to Float32 are producers.
// - Consumers, when they can have Float32 as inputs and validate a legal use of a Float32.
//   Stores in Float32 arrays and conversions to Float32 are consumers.
// - Float32 commutative, when using the Float32 instruction instead of the Double instruction
//   does not result in a compound loss of precision. This is the case for +, -, /, * with 2
//   operands, for instance. However, an addition with 3 operands is not commutative anymore,
//   so an intermediate coercion is needed.
// Except for phis, all these flags are known after Ion building, so they cannot change during
// the process.
//
// The idea behind the algorithm is easy: whenever we can prove that a commutative operation
// has only producers as inputs and consumers as uses, we can specialize the operation as a
// float32 operation. Otherwise, we have to convert all float32 inputs to doubles. Even
// if a lot of conversions are produced, GVN will take care of eliminating the redundant ones.
//
// Phis have a special status. Phis need to be flagged as producers or consumers as they can
// be inputs or outputs of commutative instructions. Fortunately, producers and consumers
// properties are such that we can deduce the property using all non phis inputs first (which form
// an initial phi graph) and then propagate all properties from one phi to another using a
// fixed point algorithm. The algorithm is ensured to terminate as each iteration has less or as
// many flagged phis as the previous iteration (so the worst steady state case is all phis being
// flagged as false).
//
// In a nutshell, the algorithm applies three passes:
// 1 - Determine which phis are consumers. Each phi gets an initial value by making a global AND on
// all its non-phi inputs. Then each phi propagates its value to other phis. If after propagation,
// the flag value changed, we have to reapply the algorithm on all phi operands, as a phi is a
// consumer if all of its uses are consumers.
// 2 - Determine which phis are producers. It's the same algorithm, except that we have to reapply
// the algorithm on all phi uses, as a phi is a producer if all of its operands are producers.
// 3 - Go through all commutative operations and ensure their inputs are all producers and their
// uses are all consumers.
bool
TypeAnalyzer::markPhiConsumers()
{
    JS_ASSERT(phiWorklist_.empty());

    // Iterate in postorder so worklist is initialized to RPO.
    for (PostorderIterator block(graph.poBegin()); block != graph.poEnd(); ++block) {
        if (mir->shouldCancel("Ensure Float32 commutativity - Consumer Phis - Initial state"))
            return false;

        for (MPhiIterator phi(block->phisBegin()); phi != block->phisEnd(); ++phi) {
            JS_ASSERT(!phi->isInWorklist());
            bool canConsumeFloat32 = true;
            for (MUseDefIterator use(*phi); canConsumeFloat32 && use; use++) {
                MDefinition *usedef = use.def();
                canConsumeFloat32 &= usedef->isPhi() || usedef->canConsumeFloat32(use.use());
            }
            phi->setCanConsumeFloat32(canConsumeFloat32);
            if (canConsumeFloat32 && !addPhiToWorklist(*phi))
                return false;
        }
    }

    while (!phiWorklist_.empty()) {
        if (mir->shouldCancel("Ensure Float32 commutativity - Consumer Phis - Fixed point"))
            return false;

        MPhi *phi = popPhi();
        JS_ASSERT(phi->canConsumeFloat32(nullptr /* unused */));

        bool validConsumer = true;
        for (MUseDefIterator use(phi); use; use++) {
            MDefinition *def = use.def();
            if (def->isPhi() && !def->canConsumeFloat32(use.use())) {
                validConsumer = false;
                break;
            }
        }

        if (validConsumer)
            continue;

        // Propagate invalidated phis
        phi->setCanConsumeFloat32(false);
        for (size_t i = 0, e = phi->numOperands(); i < e; ++i) {
            MDefinition *input = phi->getOperand(i);
            if (input->isPhi() && !input->isInWorklist() && input->canConsumeFloat32(nullptr /* unused */))
            {
                if (!addPhiToWorklist(input->toPhi()))
                    return false;
            }
        }
    }
    return true;
}

bool
TypeAnalyzer::markPhiProducers()
{
    JS_ASSERT(phiWorklist_.empty());

    // Iterate in reverse postorder so worklist is initialized to PO.
    for (ReversePostorderIterator block(graph.rpoBegin()); block != graph.rpoEnd(); ++block) {
        if (mir->shouldCancel("Ensure Float32 commutativity - Producer Phis - initial state"))
            return false;

        for (MPhiIterator phi(block->phisBegin()); phi != block->phisEnd(); ++phi) {
            JS_ASSERT(!phi->isInWorklist());
            bool canProduceFloat32 = true;
            for (size_t i = 0, e = phi->numOperands(); canProduceFloat32 && i < e; ++i) {
                MDefinition *input = phi->getOperand(i);
                canProduceFloat32 &= input->isPhi() || input->canProduceFloat32();
            }
            phi->setCanProduceFloat32(canProduceFloat32);
            if (canProduceFloat32 && !addPhiToWorklist(*phi))
                return false;
        }
    }

    while (!phiWorklist_.empty()) {
        if (mir->shouldCancel("Ensure Float32 commutativity - Producer Phis - Fixed point"))
            return false;

        MPhi *phi = popPhi();
        JS_ASSERT(phi->canProduceFloat32());

        bool validProducer = true;
        for (size_t i = 0, e = phi->numOperands(); i < e; ++i) {
            MDefinition *input = phi->getOperand(i);
            if (input->isPhi() && !input->canProduceFloat32()) {
                validProducer = false;
                break;
            }
        }

        if (validProducer)
            continue;

        // Propagate invalidated phis
        phi->setCanProduceFloat32(false);
        for (MUseDefIterator use(phi); use; use++) {
            MDefinition *def = use.def();
            if (def->isPhi() && !def->isInWorklist() && def->canProduceFloat32())
            {
                if (!addPhiToWorklist(def->toPhi()))
                    return false;
            }
        }
    }
    return true;
}

bool
TypeAnalyzer::specializeValidFloatOps()
{
    for (ReversePostorderIterator block(graph.rpoBegin()); block != graph.rpoEnd(); ++block) {
        if (mir->shouldCancel("Ensure Float32 commutativity - Instructions"))
            return false;

        for (MInstructionIterator ins(block->begin()); ins != block->end(); ++ins) {
            if (!ins->isFloat32Commutative())
                continue;

            if (ins->type() == MIRType_Float32)
                continue;

            // This call will try to specialize the instruction iff all uses are consumers and
            // all inputs are producers.
            ins->trySpecializeFloat32(alloc());
        }
    }
    return true;
}

bool
TypeAnalyzer::graphContainsFloat32()
{
    for (ReversePostorderIterator block(graph.rpoBegin()); block != graph.rpoEnd(); ++block) {
        if (mir->shouldCancel("Ensure Float32 commutativity - Graph contains Float32"))
            return false;

        for (MDefinitionIterator def(*block); def; def++) {
            if (def->type() == MIRType_Float32)
                return true;
        }
    }
    return false;
}

bool
TypeAnalyzer::tryEmitFloatOperations()
{
    // Backends that currently don't know how to generate Float32 specialized instructions
    // shouldn't run this pass and just let all instructions as specialized for Double.
    if (!LIRGenerator::allowFloat32Optimizations())
        return true;

    // Asm.js uses the ahead of time type checks to specialize operations, no need to check
    // them again at this point.
    if (mir->compilingAsmJS())
        return true;

    // Check ahead of time that there is at least one definition typed as Float32, otherwise we
    // don't need this pass.
    if (!graphContainsFloat32())
        return true;

    if (!markPhiConsumers())
       return false;
    if (!markPhiProducers())
       return false;
    if (!specializeValidFloatOps())
       return false;
    return true;
}

bool
TypeAnalyzer::checkFloatCoherency()
{
#ifdef DEBUG
    // Asserts that all Float32 instructions are flowing into Float32 consumers or specialized
    // operations
    for (ReversePostorderIterator block(graph.rpoBegin()); block != graph.rpoEnd(); ++block) {
        if (mir->shouldCancel("Check Float32 coherency"))
            return false;

        for (MDefinitionIterator def(*block); def; def++) {
            if (def->type() != MIRType_Float32)
                continue;

            for (MUseDefIterator use(*def); use; use++) {
                MDefinition *consumer = use.def();
                JS_ASSERT(consumer->isConsistentFloat32Use(use.use()));
            }
        }
    }
#endif
    return true;
}

bool
TypeAnalyzer::analyze()
{
    if (!tryEmitFloatOperations())
        return false;
    if (!specializePhis())
        return false;
    if (!insertConversions())
        return false;
    if (!checkFloatCoherency())
        return false;
    return true;
}

bool
jit::ApplyTypeInformation(MIRGenerator *mir, MIRGraph &graph)
{
    TypeAnalyzer analyzer(mir, graph);

    if (!analyzer.analyze())
        return false;

    return true;
}

bool
jit::MakeMRegExpHoistable(MIRGraph &graph)
{
    for (ReversePostorderIterator block(graph.rpoBegin()); block != graph.rpoEnd(); block++) {
        for (MDefinitionIterator iter(*block); iter; iter++) {
            if (!iter->isRegExp())
                continue;

            MRegExp *regexp = iter->toRegExp();

            // Test if MRegExp is hoistable by looking at all uses.
            bool hoistable = true;
            for (MUseIterator i = regexp->usesBegin(); i != regexp->usesEnd(); i++) {
                // Ignore resume points. At this point all uses are listed.
                // No DCE or GVN or something has happened.
                if (i->consumer()->isResumePoint())
                    continue;

                JS_ASSERT(i->consumer()->isDefinition());

                // All MRegExp* MIR's don't adjust the regexp.
                MDefinition *use = i->consumer()->toDefinition();
                if (use->isRegExpReplace())
                    continue;
                if (use->isRegExpExec())
                    continue;
                if (use->isRegExpTest())
                    continue;

                hoistable = false;
                break;
            }

            if (!hoistable)
                continue;

            // Make MRegExp hoistable
            regexp->setMovable();

            // That would be incorrect for global/sticky, because lastIndex could be wrong.
            // Therefore setting the lastIndex to 0. That is faster than a not movable regexp.
            RegExpObject *source = regexp->source();
            if (source->sticky() || source->global()) {
                JS_ASSERT(regexp->mustClone());
                MConstant *zero = MConstant::New(graph.alloc(), Int32Value(0));
                regexp->block()->insertAfter(regexp, zero);

                MStoreFixedSlot *lastIndex =
                    MStoreFixedSlot::New(graph.alloc(), regexp, RegExpObject::lastIndexSlot(), zero);
                regexp->block()->insertAfter(zero, lastIndex);
            }
        }
    }

    return true;
}

bool
jit::RenumberBlocks(MIRGraph &graph)
{
    size_t id = 0;
    for (ReversePostorderIterator block(graph.rpoBegin()); block != graph.rpoEnd(); block++)
        block->setId(id++);

    return true;
}

// A Simple, Fast Dominance Algorithm by Cooper et al.
// Modified to support empty intersections for OSR, and in RPO.
static MBasicBlock *
IntersectDominators(MBasicBlock *block1, MBasicBlock *block2)
{
    MBasicBlock *finger1 = block1;
    MBasicBlock *finger2 = block2;

    JS_ASSERT(finger1);
    JS_ASSERT(finger2);

    // In the original paper, the block ID comparisons are on the postorder index.
    // This implementation iterates in RPO, so the comparisons are reversed.

    // For this function to be called, the block must have multiple predecessors.
    // If a finger is then found to be self-dominating, it must therefore be
    // reachable from multiple roots through non-intersecting control flow.
    // nullptr is returned in this case, to denote an empty intersection.

    while (finger1->id() != finger2->id()) {
        while (finger1->id() > finger2->id()) {
            MBasicBlock *idom = finger1->immediateDominator();
            if (idom == finger1)
                return nullptr; // Empty intersection.
            finger1 = idom;
        }

        while (finger2->id() > finger1->id()) {
            MBasicBlock *idom = finger2->immediateDominator();
            if (idom == finger2)
                return nullptr; // Empty intersection.
            finger2 = idom;
        }
    }
    return finger1;
}

static void
ComputeImmediateDominators(MIRGraph &graph)
{
    // The default start block is a root and therefore only self-dominates.
    MBasicBlock *startBlock = graph.entryBlock();
    startBlock->setImmediateDominator(startBlock);

    // Any OSR block is a root and therefore only self-dominates.
    MBasicBlock *osrBlock = graph.osrBlock();
    if (osrBlock)
        osrBlock->setImmediateDominator(osrBlock);

    bool changed = true;

    while (changed) {
        changed = false;

        ReversePostorderIterator block = graph.rpoBegin();

        // For each block in RPO, intersect all dominators.
        for (; block != graph.rpoEnd(); block++) {
            // If a node has once been found to have no exclusive dominator,
            // it will never have an exclusive dominator, so it may be skipped.
            if (block->immediateDominator() == *block)
                continue;

            MBasicBlock *newIdom = block->getPredecessor(0);

            // Find the first common dominator.
            for (size_t i = 1; i < block->numPredecessors(); i++) {
                MBasicBlock *pred = block->getPredecessor(i);
                if (pred->immediateDominator() == nullptr)
                    continue;

                newIdom = IntersectDominators(pred, newIdom);

                // If there is no common dominator, the block self-dominates.
                if (newIdom == nullptr) {
                    block->setImmediateDominator(*block);
                    changed = true;
                    break;
                }
            }

            if (newIdom && block->immediateDominator() != newIdom) {
                block->setImmediateDominator(newIdom);
                changed = true;
            }
        }
    }

#ifdef DEBUG
    // Assert that all blocks have dominator information.
    for (MBasicBlockIterator block(graph.begin()); block != graph.end(); block++) {
        JS_ASSERT(block->immediateDominator() != nullptr);
    }
#endif
}

bool
jit::BuildDominatorTree(MIRGraph &graph)
{
    ComputeImmediateDominators(graph);

    Vector<MBasicBlock *, 4, IonAllocPolicy> worklist(graph.alloc());

    // Traversing through the graph in post-order means that every non-phi use
    // of a definition is visited before the def itself. Since a def
    // dominates its uses, by the time we reach a particular
    // block, we have processed all of its dominated children, so
    // block->numDominated() is accurate.
    for (PostorderIterator i(graph.poBegin()); i != graph.poEnd(); i++) {
        MBasicBlock *child = *i;
        MBasicBlock *parent = child->immediateDominator();

        // Domininace is defined such that blocks always dominate themselves.
        child->addNumDominated(1);

        // If the block only self-dominates, it has no definite parent.
        // Add it to the worklist as a root for pre-order traversal.
        // This includes all roots. Order does not matter.
        if (child == parent) {
            if (!worklist.append(child))
                return false;
            continue;
        }

        if (!parent->addImmediatelyDominatedBlock(child))
            return false;

        parent->addNumDominated(child->numDominated());
    }

#ifdef DEBUG
    // If compiling with OSR, many blocks will self-dominate.
    // Without OSR, there is only one root block which dominates all.
    if (!graph.osrBlock())
        JS_ASSERT(graph.entryBlock()->numDominated() == graph.numBlocks());
#endif
    // Now, iterate through the dominator tree in pre-order and annotate every
    // block with its index in the traversal.
    size_t index = 0;
    while (!worklist.empty()) {
        MBasicBlock *block = worklist.popCopy();
        block->setDomIndex(index);

        if (!worklist.append(block->immediatelyDominatedBlocksBegin(),
                             block->immediatelyDominatedBlocksEnd())) {
            return false;
        }
        index++;
    }

    return true;
}

bool
jit::BuildPhiReverseMapping(MIRGraph &graph)
{
    // Build a mapping such that given a basic block, whose successor has one or
    // more phis, we can find our specific input to that phi. To make this fast
    // mapping work we rely on a specific property of our structured control
    // flow graph: For a block with phis, its predecessors each have only one
    // successor with phis. Consider each case:
    //   * Blocks with less than two predecessors cannot have phis.
    //   * Breaks. A break always has exactly one successor, and the break
    //             catch block has exactly one predecessor for each break, as
    //             well as a final predecessor for the actual loop exit.
    //   * Continues. A continue always has exactly one successor, and the
    //             continue catch block has exactly one predecessor for each
    //             continue, as well as a final predecessor for the actual
    //             loop continuation. The continue itself has exactly one
    //             successor.
    //   * An if. Each branch as exactly one predecessor.
    //   * A switch. Each branch has exactly one predecessor.
    //   * Loop tail. A new block is always created for the exit, and if a
    //             break statement is present, the exit block will forward
    //             directly to the break block.
    for (MBasicBlockIterator block(graph.begin()); block != graph.end(); block++) {
        if (block->phisEmpty())
            continue;

        // Assert on the above.
        for (size_t j = 0; j < block->numPredecessors(); j++) {
            MBasicBlock *pred = block->getPredecessor(j);

#ifdef DEBUG
            size_t numSuccessorsWithPhis = 0;
            for (size_t k = 0; k < pred->numSuccessors(); k++) {
                MBasicBlock *successor = pred->getSuccessor(k);
                if (!successor->phisEmpty())
                    numSuccessorsWithPhis++;
            }
            JS_ASSERT(numSuccessorsWithPhis <= 1);
#endif

            pred->setSuccessorWithPhis(*block, j);
        }
    }

    return true;
}

#ifdef DEBUG
static bool
CheckSuccessorImpliesPredecessor(MBasicBlock *A, MBasicBlock *B)
{
    // Assuming B = succ(A), verify A = pred(B).
    for (size_t i = 0; i < B->numPredecessors(); i++) {
        if (A == B->getPredecessor(i))
            return true;
    }
    return false;
}

static bool
CheckPredecessorImpliesSuccessor(MBasicBlock *A, MBasicBlock *B)
{
    // Assuming B = pred(A), verify A = succ(B).
    for (size_t i = 0; i < B->numSuccessors(); i++) {
        if (A == B->getSuccessor(i))
            return true;
    }
    return false;
}

static bool
CheckOperandImpliesUse(MNode *n, MDefinition *operand)
{
    for (MUseIterator i = operand->usesBegin(); i != operand->usesEnd(); i++) {
        if (i->consumer() == n)
            return true;
    }
    return false;
}

static bool
CheckUseImpliesOperand(MDefinition *def, MUse *use)
{
    return use->consumer()->getOperand(use->index()) == def;
}
#endif // DEBUG

void
jit::AssertBasicGraphCoherency(MIRGraph &graph)
{
#ifdef DEBUG
    JS_ASSERT(graph.entryBlock()->numPredecessors() == 0);
    JS_ASSERT(graph.entryBlock()->phisEmpty());
    JS_ASSERT(!graph.entryBlock()->unreachable());

    if (MBasicBlock *osrBlock = graph.osrBlock()) {
        JS_ASSERT(osrBlock->numPredecessors() == 0);
        JS_ASSERT(osrBlock->phisEmpty());
        JS_ASSERT(osrBlock != graph.entryBlock());
        JS_ASSERT(!osrBlock->unreachable());
    }

    if (MResumePoint *resumePoint = graph.entryResumePoint())
        JS_ASSERT(resumePoint->block() == graph.entryBlock());

    // Assert successor and predecessor list coherency.
    uint32_t count = 0;
    for (MBasicBlockIterator block(graph.begin()); block != graph.end(); block++) {
        count++;

        JS_ASSERT(&block->graph() == &graph);

        for (size_t i = 0; i < block->numSuccessors(); i++)
            JS_ASSERT(CheckSuccessorImpliesPredecessor(*block, block->getSuccessor(i)));

        for (size_t i = 0; i < block->numPredecessors(); i++)
            JS_ASSERT(CheckPredecessorImpliesSuccessor(*block, block->getPredecessor(i)));

        for (MResumePointIterator iter(block->resumePointsBegin()); iter != block->resumePointsEnd(); iter++) {
            for (uint32_t i = 0, e = iter->numOperands(); i < e; i++) {
                if (iter->getUseFor(i)->hasProducer())
                    JS_ASSERT(CheckOperandImpliesUse(*iter, iter->getOperand(i)));
            }
        }
        for (MPhiIterator phi(block->phisBegin()); phi != block->phisEnd(); phi++) {
            JS_ASSERT(phi->numOperands() == block->numPredecessors());
            MOZ_ASSERT(!phi->isRecoveredOnBailout());
        }
        for (MDefinitionIterator iter(*block); iter; iter++) {
            JS_ASSERT(iter->block() == *block);

            // Assert that use chains are valid for this instruction.
            for (uint32_t i = 0, end = iter->numOperands(); i < end; i++)
                JS_ASSERT(CheckOperandImpliesUse(*iter, iter->getOperand(i)));
            for (MUseIterator use(iter->usesBegin()); use != iter->usesEnd(); use++)
                JS_ASSERT(CheckUseImpliesOperand(*iter, *use));

            if (iter->isInstruction()) {
                if (MResumePoint *resume = iter->toInstruction()->resumePoint()) {
                    if (MInstruction *ins = resume->instruction())
                        JS_ASSERT(ins->block() == iter->block());
                }
            }

            if (iter->isRecoveredOnBailout())
                MOZ_ASSERT(!iter->hasLiveDefUses());
        }
    }

    JS_ASSERT(graph.numBlocks() == count);
#endif
}

#ifdef DEBUG
static void
AssertReversePostorder(MIRGraph &graph)
{
    // Check that every block is visited after all its predecessors (except backedges).
    for (ReversePostorderIterator block(graph.rpoBegin()); block != graph.rpoEnd(); block++) {
        JS_ASSERT(!block->isMarked());

        for (size_t i = 0; i < block->numPredecessors(); i++) {
            MBasicBlock *pred = block->getPredecessor(i);
            if (!pred->isMarked()) {
                JS_ASSERT(pred->isLoopBackedge());
                JS_ASSERT(block->backedge() == pred);
            }
        }

        block->mark();
    }

    graph.unmarkBlocks();
}
#endif

#ifdef DEBUG
static void
AssertDominatorTree(MIRGraph &graph)
{
    // Check dominators.

    JS_ASSERT(graph.entryBlock()->immediateDominator() == graph.entryBlock());
    if (MBasicBlock *osrBlock = graph.osrBlock())
        JS_ASSERT(osrBlock->immediateDominator() == osrBlock);
    else
        JS_ASSERT(graph.entryBlock()->numDominated() == graph.numBlocks());

    size_t i = graph.numBlocks();
    size_t totalNumDominated = 0;
    for (MBasicBlockIterator block(graph.begin()); block != graph.end(); block++) {
        JS_ASSERT(block->dominates(*block));

        MBasicBlock *idom = block->immediateDominator();
        JS_ASSERT(idom->dominates(*block));
        JS_ASSERT(idom == *block || idom->id() < block->id());

        if (idom == *block) {
            totalNumDominated += block->numDominated();
        } else {
            bool foundInParent = false;
            for (size_t j = 0; j < idom->numImmediatelyDominatedBlocks(); j++) {
                if (idom->getImmediatelyDominatedBlock(j) == *block) {
                    foundInParent = true;
                    break;
                }
            }
            JS_ASSERT(foundInParent);
        }

        size_t numDominated = 1;
        for (size_t j = 0; j < block->numImmediatelyDominatedBlocks(); j++) {
            MBasicBlock *dom = block->getImmediatelyDominatedBlock(j);
            JS_ASSERT(block->dominates(dom));
            JS_ASSERT(dom->id() > block->id());
            JS_ASSERT(dom->immediateDominator() == *block);

            numDominated += dom->numDominated();
        }
        JS_ASSERT(block->numDominated() == numDominated);
        JS_ASSERT(block->numDominated() <= i);
        JS_ASSERT(block->numSuccessors() != 0 || block->numDominated() == 1);
        i--;
    }
    JS_ASSERT(i == 0);
    JS_ASSERT(totalNumDominated == graph.numBlocks());
}
#endif

void
jit::AssertGraphCoherency(MIRGraph &graph)
{
#ifdef DEBUG
    if (!js_JitOptions.checkGraphConsistency)
        return;
    AssertBasicGraphCoherency(graph);
    AssertReversePostorder(graph);
#endif
}

void
jit::AssertExtendedGraphCoherency(MIRGraph &graph)
{
    // Checks the basic GraphCoherency but also other conditions that
    // do not hold immediately (such as the fact that critical edges
    // are split)

#ifdef DEBUG
    if (!js_JitOptions.checkGraphConsistency)
        return;
    AssertGraphCoherency(graph);

    uint32_t idx = 0;
    for (MBasicBlockIterator block(graph.begin()); block != graph.end(); block++) {
        JS_ASSERT(block->id() == idx++);

        // No critical edges:
        if (block->numSuccessors() > 1)
            for (size_t i = 0; i < block->numSuccessors(); i++)
                JS_ASSERT(block->getSuccessor(i)->numPredecessors() == 1);

        if (block->isLoopHeader()) {
            JS_ASSERT(block->numPredecessors() == 2);
            MBasicBlock *backedge = block->getPredecessor(1);
            JS_ASSERT(backedge->id() >= block->id());
            JS_ASSERT(backedge->numSuccessors() == 1);
            JS_ASSERT(backedge->getSuccessor(0) == *block);
        }

        if (!block->phisEmpty()) {
            for (size_t i = 0; i < block->numPredecessors(); i++) {
                MBasicBlock *pred = block->getPredecessor(i);
                JS_ASSERT(pred->successorWithPhis() == *block);
                JS_ASSERT(pred->positionInPhiSuccessor() == i);
            }
        }

        uint32_t successorWithPhis = 0;
        for (size_t i = 0; i < block->numSuccessors(); i++)
            if (!block->getSuccessor(i)->phisEmpty())
                successorWithPhis++;

        JS_ASSERT(successorWithPhis <= 1);
        JS_ASSERT((successorWithPhis != 0) == (block->successorWithPhis() != nullptr));
    }

    AssertDominatorTree(graph);
#endif
}


struct BoundsCheckInfo
{
    MBoundsCheck *check;
    uint32_t validEnd;
};

typedef HashMap<uint32_t,
                BoundsCheckInfo,
                DefaultHasher<uint32_t>,
                IonAllocPolicy> BoundsCheckMap;

// Compute a hash for bounds checks which ignores constant offsets in the index.
static HashNumber
BoundsCheckHashIgnoreOffset(MBoundsCheck *check)
{
    SimpleLinearSum indexSum = ExtractLinearSum(check->index());
    uintptr_t index = indexSum.term ? uintptr_t(indexSum.term) : 0;
    uintptr_t length = uintptr_t(check->length());
    return index ^ length;
}

static MBoundsCheck *
FindDominatingBoundsCheck(BoundsCheckMap &checks, MBoundsCheck *check, size_t index)
{
    // See the comment in ValueNumberer::findDominatingDef.
    HashNumber hash = BoundsCheckHashIgnoreOffset(check);
    BoundsCheckMap::Ptr p = checks.lookup(hash);
    if (!p || index >= p->value().validEnd) {
        // We didn't find a dominating bounds check.
        BoundsCheckInfo info;
        info.check = check;
        info.validEnd = index + check->block()->numDominated();

        if(!checks.put(hash, info))
            return nullptr;

        return check;
    }

    return p->value().check;
}

// Extract a linear sum from ins, if possible (otherwise giving the sum 'ins + 0').
SimpleLinearSum
jit::ExtractLinearSum(MDefinition *ins)
{
    if (ins->isBeta())
        ins = ins->getOperand(0);

    if (ins->type() != MIRType_Int32)
        return SimpleLinearSum(ins, 0);

    if (ins->isConstant()) {
        const Value &v = ins->toConstant()->value();
        JS_ASSERT(v.isInt32());
        return SimpleLinearSum(nullptr, v.toInt32());
    } else if (ins->isAdd() || ins->isSub()) {
        MDefinition *lhs = ins->getOperand(0);
        MDefinition *rhs = ins->getOperand(1);
        if (lhs->type() == MIRType_Int32 && rhs->type() == MIRType_Int32) {
            SimpleLinearSum lsum = ExtractLinearSum(lhs);
            SimpleLinearSum rsum = ExtractLinearSum(rhs);

            if (lsum.term && rsum.term)
                return SimpleLinearSum(ins, 0);

            // Check if this is of the form <SUM> + n, n + <SUM> or <SUM> - n.
            if (ins->isAdd()) {
                int32_t constant;
                if (!SafeAdd(lsum.constant, rsum.constant, &constant))
                    return SimpleLinearSum(ins, 0);
                return SimpleLinearSum(lsum.term ? lsum.term : rsum.term, constant);
            } else if (lsum.term) {
                int32_t constant;
                if (!SafeSub(lsum.constant, rsum.constant, &constant))
                    return SimpleLinearSum(ins, 0);
                return SimpleLinearSum(lsum.term, constant);
            }
        }
    }

    return SimpleLinearSum(ins, 0);
}

// Extract a linear inequality holding when a boolean test goes in the
// specified direction, of the form 'lhs + lhsN <= rhs' (or >=).
bool
jit::ExtractLinearInequality(MTest *test, BranchDirection direction,
                             SimpleLinearSum *plhs, MDefinition **prhs, bool *plessEqual)
{
    if (!test->getOperand(0)->isCompare())
        return false;

    MCompare *compare = test->getOperand(0)->toCompare();

    MDefinition *lhs = compare->getOperand(0);
    MDefinition *rhs = compare->getOperand(1);

    // TODO: optimize Compare_UInt32
    if (!compare->isInt32Comparison())
        return false;

    JS_ASSERT(lhs->type() == MIRType_Int32);
    JS_ASSERT(rhs->type() == MIRType_Int32);

    JSOp jsop = compare->jsop();
    if (direction == FALSE_BRANCH)
        jsop = NegateCompareOp(jsop);

    SimpleLinearSum lsum = ExtractLinearSum(lhs);
    SimpleLinearSum rsum = ExtractLinearSum(rhs);

    if (!SafeSub(lsum.constant, rsum.constant, &lsum.constant))
        return false;

    // Normalize operations to use <= or >=.
    switch (jsop) {
      case JSOP_LE:
        *plessEqual = true;
        break;
      case JSOP_LT:
        /* x < y ==> x + 1 <= y */
        if (!SafeAdd(lsum.constant, 1, &lsum.constant))
            return false;
        *plessEqual = true;
        break;
      case JSOP_GE:
        *plessEqual = false;
        break;
      case JSOP_GT:
        /* x > y ==> x - 1 >= y */
        if (!SafeSub(lsum.constant, 1, &lsum.constant))
            return false;
        *plessEqual = false;
        break;
      default:
        return false;
    }

    *plhs = lsum;
    *prhs = rsum.term;

    return true;
}

static bool
TryEliminateBoundsCheck(BoundsCheckMap &checks, size_t blockIndex, MBoundsCheck *dominated, bool *eliminated)
{
    JS_ASSERT(!*eliminated);

    // Replace all uses of the bounds check with the actual index.
    // This is (a) necessary, because we can coalesce two different
    // bounds checks and would otherwise use the wrong index and
    // (b) helps register allocation. Note that this is safe since
    // no other pass after bounds check elimination moves instructions.
    dominated->replaceAllUsesWith(dominated->index());

    if (!dominated->isMovable())
        return true;

    MBoundsCheck *dominating = FindDominatingBoundsCheck(checks, dominated, blockIndex);
    if (!dominating)
        return false;

    if (dominating == dominated) {
        // We didn't find a dominating bounds check.
        return true;
    }

    // We found two bounds checks with the same hash number, but we still have
    // to make sure the lengths and index terms are equal.
    if (dominating->length() != dominated->length())
        return true;

    SimpleLinearSum sumA = ExtractLinearSum(dominating->index());
    SimpleLinearSum sumB = ExtractLinearSum(dominated->index());

    // Both terms should be nullptr or the same definition.
    if (sumA.term != sumB.term)
        return true;

    // This bounds check is redundant.
    *eliminated = true;

    // Normalize the ranges according to the constant offsets in the two indexes.
    int32_t minimumA, maximumA, minimumB, maximumB;
    if (!SafeAdd(sumA.constant, dominating->minimum(), &minimumA) ||
        !SafeAdd(sumA.constant, dominating->maximum(), &maximumA) ||
        !SafeAdd(sumB.constant, dominated->minimum(), &minimumB) ||
        !SafeAdd(sumB.constant, dominated->maximum(), &maximumB))
    {
        return false;
    }

    // Update the dominating check to cover both ranges, denormalizing the
    // result per the constant offset in the index.
    int32_t newMinimum, newMaximum;
    if (!SafeSub(Min(minimumA, minimumB), sumA.constant, &newMinimum) ||
        !SafeSub(Max(maximumA, maximumB), sumA.constant, &newMaximum))
    {
        return false;
    }

    dominating->setMinimum(newMinimum);
    dominating->setMaximum(newMaximum);
    return true;
}

static void
TryEliminateTypeBarrierFromTest(MTypeBarrier *barrier, bool filtersNull, bool filtersUndefined,
                                MTest *test, BranchDirection direction, bool *eliminated)
{
    JS_ASSERT(filtersNull || filtersUndefined);

    // Watch for code patterns similar to 'if (x.f) { ... = x.f }'.  If x.f
    // is either an object or null/undefined, there will be a type barrier on
    // the latter read as the null/undefined value is never realized there.
    // The type barrier can be eliminated, however, by looking at tests
    // performed on the result of the first operation that filter out all
    // types that have been seen in the first access but not the second.

    // A test 'if (x.f)' filters both null and undefined.

    // Disregard the possible unbox added before the Typebarrier for checking.
    MDefinition *input = barrier->input();
    MUnbox *inputUnbox = nullptr;
    if (input->isUnbox() && input->toUnbox()->mode() != MUnbox::Fallible) {
        inputUnbox = input->toUnbox();
        input = inputUnbox->input();
    }

    MDefinition *subject = nullptr;
    bool removeUndefined;
    bool removeNull;
    test->filtersUndefinedOrNull(direction == TRUE_BRANCH, &subject, &removeUndefined, &removeNull);

    // The Test doesn't filter undefined nor null.
    if (!subject)
        return;

    // Make sure the subject equals the input to the TypeBarrier.
    if (subject != input)
        return;

    // When the TypeBarrier filters undefined, the test must at least also do,
    // this, before the TypeBarrier can get removed.
    if (!removeUndefined && filtersUndefined)
        return;

    // When the TypeBarrier filters null, the test must at least also do,
    // this, before the TypeBarrier can get removed.
    if (!removeNull && filtersNull)
        return;

    // Eliminate the TypeBarrier. The possible TypeBarrier unboxing is kept,
    // but made infallible.
    *eliminated = true;
    if (inputUnbox)
        inputUnbox->makeInfallible();
    barrier->replaceAllUsesWith(barrier->input());
}

static bool
TryEliminateTypeBarrier(MTypeBarrier *barrier, bool *eliminated)
{
    JS_ASSERT(!*eliminated);

    const types::TemporaryTypeSet *barrierTypes = barrier->resultTypeSet();
    const types::TemporaryTypeSet *inputTypes = barrier->input()->resultTypeSet();

    // Disregard the possible unbox added before the Typebarrier.
    if (barrier->input()->isUnbox() && barrier->input()->toUnbox()->mode() != MUnbox::Fallible)
        inputTypes = barrier->input()->toUnbox()->input()->resultTypeSet();

    if (!barrierTypes || !inputTypes)
        return true;

    bool filtersNull = barrierTypes->filtersType(inputTypes, types::Type::NullType());
    bool filtersUndefined = barrierTypes->filtersType(inputTypes, types::Type::UndefinedType());

    if (!filtersNull && !filtersUndefined)
        return true;

    MBasicBlock *block = barrier->block();
    while (true) {
        BranchDirection direction;
        MTest *test = block->immediateDominatorBranch(&direction);

        if (test) {
            TryEliminateTypeBarrierFromTest(barrier, filtersNull, filtersUndefined,
                                            test, direction, eliminated);
        }

        MBasicBlock *previous = block->immediateDominator();
        if (previous == block)
            break;
        block = previous;
    }

    return true;
}

// Eliminate checks which are redundant given each other or other instructions.
//
// A type barrier is considered redundant if all missing types have been tested
// for by earlier control instructions.
//
// A bounds check is considered redundant if it's dominated by another bounds
// check with the same length and the indexes differ by only a constant amount.
// In this case we eliminate the redundant bounds check and update the other one
// to cover the ranges of both checks.
//
// Bounds checks are added to a hash map and since the hash function ignores
// differences in constant offset, this offers a fast way to find redundant
// checks.
bool
jit::EliminateRedundantChecks(MIRGraph &graph)
{
    BoundsCheckMap checks(graph.alloc());

    if (!checks.init())
        return false;

    // Stack for pre-order CFG traversal.
    Vector<MBasicBlock *, 1, IonAllocPolicy> worklist(graph.alloc());

    // The index of the current block in the CFG traversal.
    size_t index = 0;

    // Add all self-dominating blocks to the worklist.
    // This includes all roots. Order does not matter.
    for (MBasicBlockIterator i(graph.begin()); i != graph.end(); i++) {
        MBasicBlock *block = *i;
        if (block->immediateDominator() == block) {
            if (!worklist.append(block))
                return false;
        }
    }

    // Starting from each self-dominating block, traverse the CFG in pre-order.
    while (!worklist.empty()) {
        MBasicBlock *block = worklist.popCopy();

        // Add all immediate dominators to the front of the worklist.
        if (!worklist.append(block->immediatelyDominatedBlocksBegin(),
                             block->immediatelyDominatedBlocksEnd())) {
            return false;
        }

        for (MDefinitionIterator iter(block); iter; ) {
            bool eliminated = false;

            if (iter->isBoundsCheck()) {
                if (!TryEliminateBoundsCheck(checks, index, iter->toBoundsCheck(), &eliminated))
                    return false;
            } else if (iter->isTypeBarrier()) {
                if (!TryEliminateTypeBarrier(iter->toTypeBarrier(), &eliminated))
                    return false;
            } else if (iter->isConvertElementsToDoubles()) {
                // Now that code motion passes have finished, replace any
                // ConvertElementsToDoubles with the actual elements.
                MConvertElementsToDoubles *ins = iter->toConvertElementsToDoubles();
                ins->replaceAllUsesWith(ins->elements());
            }

            if (eliminated)
                iter = block->discardDefAt(iter);
            else
                iter++;
        }
        index++;
    }

    JS_ASSERT(index == graph.numBlocks());
    return true;
}

bool
LinearSum::multiply(int32_t scale)
{
    for (size_t i = 0; i < terms_.length(); i++) {
        if (!SafeMul(scale, terms_[i].scale, &terms_[i].scale))
            return false;
    }
    return SafeMul(scale, constant_, &constant_);
}

bool
LinearSum::add(const LinearSum &other)
{
    for (size_t i = 0; i < other.terms_.length(); i++) {
        if (!add(other.terms_[i].term, other.terms_[i].scale))
            return false;
    }
    return add(other.constant_);
}

bool
LinearSum::add(MDefinition *term, int32_t scale)
{
    JS_ASSERT(term);

    if (scale == 0)
        return true;

    if (term->isConstant()) {
        int32_t constant = term->toConstant()->value().toInt32();
        if (!SafeMul(constant, scale, &constant))
            return false;
        return add(constant);
    }

    for (size_t i = 0; i < terms_.length(); i++) {
        if (term == terms_[i].term) {
            if (!SafeAdd(scale, terms_[i].scale, &terms_[i].scale))
                return false;
            if (terms_[i].scale == 0) {
                terms_[i] = terms_.back();
                terms_.popBack();
            }
            return true;
        }
    }

    terms_.append(LinearTerm(term, scale));
    return true;
}

bool
LinearSum::add(int32_t constant)
{
    return SafeAdd(constant, constant_, &constant_);
}

void
LinearSum::print(Sprinter &sp) const
{
    for (size_t i = 0; i < terms_.length(); i++) {
        int32_t scale = terms_[i].scale;
        int32_t id = terms_[i].term->id();
        JS_ASSERT(scale);
        if (scale > 0) {
            if (i)
                sp.printf("+");
            if (scale == 1)
                sp.printf("#%d", id);
            else
                sp.printf("%d*#%d", scale, id);
        } else if (scale == -1) {
            sp.printf("-#%d", id);
        } else {
            sp.printf("%d*#%d", scale, id);
        }
    }
    if (constant_ > 0)
        sp.printf("+%d", constant_);
    else if (constant_ < 0)
        sp.printf("%d", constant_);
}

void
LinearSum::dump(FILE *fp) const
{
    Sprinter sp(GetIonContext()->cx);
    sp.init();
    print(sp);
    fprintf(fp, "%s\n", sp.string());
}

void
LinearSum::dump() const
{
    dump(stderr);
}

static bool
AnalyzePoppedThis(JSContext *cx, types::TypeObject *type,
                  MDefinition *thisValue, MInstruction *ins, bool definitelyExecuted,
                  HandleObject baseobj,
                  Vector<types::TypeNewScript::Initializer> *initializerList,
                  Vector<PropertyName *> *accessedProperties,
                  bool *phandled)
{
    // Determine the effect that a use of the |this| value when calling |new|
    // on a script has on the properties definitely held by the new object.

    if (ins->isCallSetProperty()) {
        MCallSetProperty *setprop = ins->toCallSetProperty();

        if (setprop->object() != thisValue)
            return true;

        // Don't use GetAtomId here, we need to watch for SETPROP on
        // integer properties and bail out. We can't mark the aggregate
        // JSID_VOID type property as being in a definite slot.
        if (setprop->name() == cx->names().prototype ||
            setprop->name() == cx->names().proto ||
            setprop->name() == cx->names().constructor)
        {
            return true;
        }

        // Ignore assignments to properties that were already written to.
        if (baseobj->nativeLookup(cx, NameToId(setprop->name()))) {
            *phandled = true;
            return true;
        }

        // Don't add definite properties for properties that were already
        // read in the constructor.
        for (size_t i = 0; i < accessedProperties->length(); i++) {
            if ((*accessedProperties)[i] == setprop->name())
                return true;
        }

        // Don't add definite properties to an object which won't fit in its
        // fixed slots.
        if (GetGCKindSlots(gc::GetGCObjectKind(baseobj->slotSpan() + 1)) <= baseobj->slotSpan())
            return true;

        // Assignments to new properties must always execute.
        if (!definitelyExecuted)
            return true;

        RootedId id(cx, NameToId(setprop->name()));
        if (!types::AddClearDefiniteGetterSetterForPrototypeChain(cx, type, id)) {
            // The prototype chain already contains a getter/setter for this
            // property, or type information is too imprecise.
            return true;
        }

        DebugOnly<unsigned> slotSpan = baseobj->slotSpan();
        if (!DefineNativeProperty(cx, baseobj, id, UndefinedHandleValue, nullptr, nullptr,
                                  JSPROP_ENUMERATE))
        {
            return false;
        }
        JS_ASSERT(baseobj->slotSpan() != slotSpan);
        JS_ASSERT(!baseobj->inDictionaryMode());

        Vector<MResumePoint *> callerResumePoints(cx);
        for (MResumePoint *rp = ins->block()->callerResumePoint();
             rp;
             rp = rp->block()->callerResumePoint())
        {
            if (!callerResumePoints.append(rp))
                return false;
        }

        for (int i = callerResumePoints.length() - 1; i >= 0; i--) {
            MResumePoint *rp = callerResumePoints[i];
            JSScript *script = rp->block()->info().script();
            types::TypeNewScript::Initializer entry(types::TypeNewScript::Initializer::SETPROP_FRAME,
                                                    script->pcToOffset(rp->pc()));
            if (!initializerList->append(entry))
                return false;
        }

        JSScript *script = ins->block()->info().script();
        types::TypeNewScript::Initializer entry(types::TypeNewScript::Initializer::SETPROP,
                                                script->pcToOffset(setprop->resumePoint()->pc()));
        if (!initializerList->append(entry))
            return false;

        *phandled = true;
        return true;
    }

    if (ins->isCallGetProperty()) {
        MCallGetProperty *get = ins->toCallGetProperty();

        /*
         * Properties can be read from the 'this' object if the following hold:
         *
         * - The read is not on a getter along the prototype chain, which
         *   could cause 'this' to escape.
         *
         * - The accessed property is either already a definite property or
         *   is not later added as one. Since the definite properties are
         *   added to the object at the point of its creation, reading a
         *   definite property before it is assigned could incorrectly hit.
         */
        RootedId id(cx, NameToId(get->name()));
        if (!baseobj->nativeLookup(cx, id) && !accessedProperties->append(get->name()))
            return false;

        if (!types::AddClearDefiniteGetterSetterForPrototypeChain(cx, type, id)) {
            // The |this| value can escape if any property reads it does go
            // through a getter.
            return true;
        }

        *phandled = true;
        return true;
    }

    if (ins->isPostWriteBarrier()) {
        *phandled = true;
        return true;
    }

    return true;
}

static int
CmpInstructions(const void *a, const void *b)
{
    return (*static_cast<MInstruction * const *>(a))->id() -
           (*static_cast<MInstruction * const *>(b))->id();
}

bool
jit::AnalyzeNewScriptProperties(JSContext *cx, JSFunction *fun,
                                types::TypeObject *type, HandleObject baseobj,
                                Vector<types::TypeNewScript::Initializer> *initializerList)
{
    JS_ASSERT(cx->compartment()->activeAnalysis);

    // When invoking 'new' on the specified script, try to find some properties
    // which will definitely be added to the created object before it has a
    // chance to escape and be accessed elsewhere.

    RootedScript script(cx, fun->getOrCreateScript(cx));
    if (!script)
        return false;

    if (!jit::IsIonEnabled(cx) || !jit::IsBaselineEnabled(cx) ||
        !script->compileAndGo() || !script->canBaselineCompile())
    {
        return true;
    }

    static const uint32_t MAX_SCRIPT_SIZE = 2000;
    if (script->length() > MAX_SCRIPT_SIZE)
        return true;

    Vector<PropertyName *> accessedProperties(cx);

    LifoAlloc alloc(types::TypeZone::TYPE_LIFO_ALLOC_PRIMARY_CHUNK_SIZE);

    TempAllocator temp(&alloc);
    IonContext ictx(cx, &temp);

    if (!cx->compartment()->ensureJitCompartmentExists(cx))
        return false;

    if (!script->hasBaselineScript()) {
        MethodStatus status = BaselineCompile(cx, script);
        if (status == Method_Error)
            return false;
        if (status != Method_Compiled)
            return true;
    }

    types::TypeScript::SetThis(cx, script, types::Type::ObjectType(type));

    MIRGraph graph(&temp);
    InlineScriptTree *inlineScriptTree = InlineScriptTree::New(&temp, nullptr, nullptr, script);
    if (!inlineScriptTree)
        return false;

    CompileInfo info(script, fun,
                     /* osrPc = */ nullptr, /* constructing = */ false,
                     DefinitePropertiesAnalysis,
                     script->needsArgsObj(),
                     inlineScriptTree);

    AutoTempAllocatorRooter root(cx, &temp);

    const OptimizationInfo *optimizationInfo = js_IonOptimizations.get(Optimization_Normal);

    types::CompilerConstraintList *constraints = types::NewCompilerConstraintList(temp);
    if (!constraints) {
        js_ReportOutOfMemory(cx);
        return false;
    }

    BaselineInspector inspector(script);
    const JitCompileOptions options(cx);

    IonBuilder builder(cx, CompileCompartment::get(cx->compartment()), options, &temp, &graph, constraints,
                       &inspector, &info, optimizationInfo, /* baselineFrame = */ nullptr);

    if (!builder.build()) {
        if (builder.abortReason() == AbortReason_Alloc)
            return false;
        return true;
    }

    types::FinishDefinitePropertiesAnalysis(cx, constraints);

    if (!SplitCriticalEdges(graph))
        return false;

    if (!RenumberBlocks(graph))
        return false;

    if (!BuildDominatorTree(graph))
        return false;

    if (!EliminatePhis(&builder, graph, AggressiveObservability))
        return false;

    MDefinition *thisValue = graph.entryBlock()->getSlot(info.thisSlot());

    // Get a list of instructions using the |this| value in the order they
    // appear in the graph.
    Vector<MInstruction *> instructions(cx);

    for (MUseDefIterator uses(thisValue); uses; uses++) {
        MDefinition *use = uses.def();

        // Don't track |this| through assignments to phis.
        if (!use->isInstruction())
            return true;

        if (!instructions.append(use->toInstruction()))
            return false;
    }

    // Sort the instructions to visit in increasing order.
    qsort(instructions.begin(), instructions.length(),
          sizeof(MInstruction *), CmpInstructions);

    // Find all exit blocks in the graph.
    Vector<MBasicBlock *> exitBlocks(cx);
    for (MBasicBlockIterator block(graph.begin()); block != graph.end(); block++) {
        if (!block->numSuccessors() && !exitBlocks.append(*block))
            return false;
    }

    // id of the last block which added a new property.
    size_t lastAddedBlock = 0;

    for (size_t i = 0; i < instructions.length(); i++) {
        MInstruction *ins = instructions[i];

        // Track whether the use of |this| is in unconditional code, i.e.
        // the block dominates all graph exits.
        bool definitelyExecuted = true;
        for (size_t i = 0; i < exitBlocks.length(); i++) {
            for (MBasicBlock *exit = exitBlocks[i];
                 exit != ins->block();
                 exit = exit->immediateDominator())
            {
                if (exit == exit->immediateDominator()) {
                    definitelyExecuted = false;
                    break;
                }
            }
        }

        // Also check to see if the instruction is inside a loop body. Even if
        // an access will always execute in the script, if it executes multiple
        // times then we can get confused when rolling back objects while
        // clearing the new script information.
        if (ins->block()->loopDepth() != 0)
            definitelyExecuted = false;

        bool handled = false;
        size_t slotSpan = baseobj->slotSpan();
        if (!AnalyzePoppedThis(cx, type, thisValue, ins, definitelyExecuted,
                               baseobj, initializerList, &accessedProperties, &handled))
        {
            return false;
        }
        if (!handled)
            break;

        if (slotSpan != baseobj->slotSpan()) {
            JS_ASSERT(ins->block()->id() >= lastAddedBlock);
            lastAddedBlock = ins->block()->id();
        }
    }

    if (baseobj->slotSpan() != 0) {
        // We found some definite properties, but their correctness is still
        // contingent on the correct frames being inlined. Add constraints to
        // invalidate the definite properties if additional functions could be
        // called at the inline frame sites.
        Vector<MBasicBlock *> exitBlocks(cx);
        for (MBasicBlockIterator block(graph.begin()); block != graph.end(); block++) {
            // Inlining decisions made after the last new property was added to
            // the object don't need to be frozen.
            if (block->id() > lastAddedBlock)
                break;
            if (MResumePoint *rp = block->callerResumePoint()) {
                if (block->numPredecessors() == 1 && block->getPredecessor(0) == rp->block()) {
                    JSScript *script = rp->block()->info().script();
                    if (!types::AddClearDefiniteFunctionUsesInScript(cx, type, script, block->info().script()))
                        return false;
                }
            }
        }
    }

    return true;
}

static bool
ArgumentsUseCanBeLazy(JSContext *cx, JSScript *script, MInstruction *ins, size_t index,
                      bool *argumentsContentsObserved)
{
    // We can read the frame's arguments directly for f.apply(x, arguments).
    if (ins->isCall()) {
        if (*ins->toCall()->resumePoint()->pc() == JSOP_FUNAPPLY &&
            ins->toCall()->numActualArgs() == 2 &&
            index == MCall::IndexOfArgument(1))
        {
            *argumentsContentsObserved = true;
            return true;
        }
    }

    // arguments[i] can read fp->canonicalActualArg(i) directly.
    if (ins->isCallGetElement() && index == 0) {
        *argumentsContentsObserved = true;
        return true;
    }

    // MGetArgumentsObjectArg needs to be considered as a use that allows laziness.
    if (ins->isGetArgumentsObjectArg() && index == 0)
        return true;

    // arguments.length length can read fp->numActualArgs() directly.
    if (ins->isCallGetProperty() && index == 0 && ins->toCallGetProperty()->name() == cx->names().length)
        return true;

    return false;
}

bool
jit::AnalyzeArgumentsUsage(JSContext *cx, JSScript *scriptArg)
{
    RootedScript script(cx, scriptArg);
    types::AutoEnterAnalysis enter(cx);

    JS_ASSERT(!script->analyzedArgsUsage());

    // Treat the script as needing an arguments object until we determine it
    // does not need one. This both allows us to easily see where the arguments
    // object can escape through assignments to the function's named arguments,
    // and also simplifies handling of early returns.
    script->setNeedsArgsObj(true);

    // Always construct arguments objects when in debug mode and for generator
    // scripts (generators can be suspended when speculation fails).
    //
    // FIXME: Don't build arguments for ES6 generator expressions.
    if (cx->compartment()->debugMode() || script->isGenerator())
        return true;

    // If the script has dynamic name accesses which could reach 'arguments',
    // the parser will already have checked to ensure there are no explicit
    // uses of 'arguments' in the function. If there are such uses, the script
    // will be marked as definitely needing an arguments object.
    //
    // New accesses on 'arguments' can occur through 'eval' or the debugger
    // statement. In the former case, we will dynamically detect the use and
    // mark the arguments optimization as having failed.
    if (script->bindingsAccessedDynamically()) {
        script->setNeedsArgsObj(false);
        return true;
    }

    if (!jit::IsIonEnabled(cx) || !script->compileAndGo())
        return true;

    static const uint32_t MAX_SCRIPT_SIZE = 10000;
    if (script->length() > MAX_SCRIPT_SIZE)
        return true;

    if (!script->ensureHasTypes(cx))
        return false;

    LifoAlloc alloc(types::TypeZone::TYPE_LIFO_ALLOC_PRIMARY_CHUNK_SIZE);

    TempAllocator temp(&alloc);
    IonContext ictx(cx, &temp);

    if (!cx->compartment()->ensureJitCompartmentExists(cx))
        return false;

    MIRGraph graph(&temp);
    InlineScriptTree *inlineScriptTree = InlineScriptTree::New(&temp, nullptr, nullptr, script);
    if (!inlineScriptTree)
        return false;
    CompileInfo info(script, script->functionNonDelazifying(),
                     /* osrPc = */ nullptr, /* constructing = */ false,
                     ArgumentsUsageAnalysis,
                     /* needsArgsObj = */ true,
                     inlineScriptTree);

    AutoTempAllocatorRooter root(cx, &temp);

    const OptimizationInfo *optimizationInfo = js_IonOptimizations.get(Optimization_Normal);

    types::CompilerConstraintList *constraints = types::NewCompilerConstraintList(temp);
    if (!constraints)
        return false;

    BaselineInspector inspector(script);
    const JitCompileOptions options(cx);

    IonBuilder builder(nullptr, CompileCompartment::get(cx->compartment()), options, &temp, &graph, constraints,
                       &inspector, &info, optimizationInfo, /* baselineFrame = */ nullptr);

    if (!builder.build()) {
        if (builder.abortReason() == AbortReason_Alloc)
            return false;
        return true;
    }

    if (!SplitCriticalEdges(graph))
        return false;

    if (!RenumberBlocks(graph))
        return false;

    if (!BuildDominatorTree(graph))
        return false;

    if (!EliminatePhis(&builder, graph, AggressiveObservability))
        return false;

    MDefinition *argumentsValue = graph.entryBlock()->getSlot(info.argsObjSlot());

    bool argumentsContentsObserved = false;

    for (MUseDefIterator uses(argumentsValue); uses; uses++) {
        MDefinition *use = uses.def();

        // Don't track |arguments| through assignments to phis.
        if (!use->isInstruction())
            return true;

        if (!ArgumentsUseCanBeLazy(cx, script, use->toInstruction(), use->indexOf(uses.use()),
                                   &argumentsContentsObserved))
        {
            return true;
        }
    }

    // If a script explicitly accesses the contents of 'arguments', and has
    // formals which may be stored as part of a call object, don't use lazy
    // arguments. The compiler can then assume that accesses through
    // arguments[i] will be on unaliased variables.
    if (script->funHasAnyAliasedFormal() && argumentsContentsObserved)
        return true;

    script->setNeedsArgsObj(false);
    return true;
}

// Mark all the blocks that are in the loop with the given header.
// Returns the number of blocks marked. Set *canOsr to true if the loop is
// reachable from both the normal entry and the OSR entry.
size_t
jit::MarkLoopBlocks(MIRGraph &graph, MBasicBlock *header, bool *canOsr)
{
#ifdef DEBUG
    for (ReversePostorderIterator i = graph.rpoBegin(), e = graph.rpoEnd(); i != e; ++i)
        MOZ_ASSERT(!i->isMarked(), "Some blocks already marked");
#endif

    MBasicBlock *osrBlock = graph.osrBlock();
    *canOsr = false;

    // The blocks are in RPO; start at the loop backedge, which is marks the
    // bottom of the loop, and walk up until we get to the header. Loops may be
    // discontiguous, so we trace predecessors to determine which blocks are
    // actually part of the loop. The backedge is always part of the loop, and
    // so are its predecessors, transitively, up to the loop header or an OSR
    // entry.
    MBasicBlock *backedge = header->backedge();
    backedge->mark();
    size_t numMarked = 1;
    for (PostorderIterator i = graph.poBegin(backedge); ; ++i) {
        MOZ_ASSERT(i != graph.poEnd(),
                   "Reached the end of the graph while searching for the loop header");
        MBasicBlock *block = *i;
        // A block not marked by the time we reach it is not in the loop.
        if (!block->isMarked())
            continue;
        // If we've reached the loop header, we're done.
        if (block == header)
            break;
        // This block is in the loop; trace to its predecessors.
        for (size_t p = 0, e = block->numPredecessors(); p != e; ++p) {
            MBasicBlock *pred = block->getPredecessor(p);
            if (pred->isMarked())
                continue;

            // Blocks dominated by the OSR entry are not part of the loop
            // (unless they aren't reachable from the normal entry).
            if (osrBlock && pred != header && osrBlock->dominates(pred)) {
                *canOsr = true;
                continue;
            }

            MOZ_ASSERT(pred->id() >= header->id() && pred->id() <= backedge->id(),
                       "Loop block not between loop header and loop backedge");

            pred->mark();
            ++numMarked;

            // A nested loop may not exit back to the enclosing loop at its
            // bottom. If we just marked its header, then the whole nested loop
            // is part of the enclosing loop.
            if (pred->isLoopHeader()) {
                MBasicBlock *innerBackedge = pred->backedge();
                if (!innerBackedge->isMarked()) {
                    // Mark its backedge so that we add all of its blocks to the
                    // outer loop as we walk upwards.
                    innerBackedge->mark();
                    ++numMarked;

                    // If the nested loop is not contiguous, we may have already
                    // passed its backedge. If this happens, back up.
                    if (backedge->id() > block->id()) {
                        i = graph.poBegin(innerBackedge);
                        --i;
                    }
                }
            }
        }
    }
    MOZ_ASSERT(header->isMarked(), "Loop header should be part of the loop");
    return numMarked;
}

// Unmark all the blocks that are in the loop with the given header.
void
jit::UnmarkLoopBlocks(MIRGraph &graph, MBasicBlock *header)
{
    MBasicBlock *backedge = header->backedge();
    for (ReversePostorderIterator i = graph.rpoBegin(header); ; ++i) {
        MOZ_ASSERT(i != graph.rpoEnd(),
                   "Reached the end of the graph while searching for the backedge");
        MBasicBlock *block = *i;
        if (block->isMarked()) {
            block->unmark();
            if (block == backedge)
                break;
        }
    }

#ifdef DEBUG
    for (ReversePostorderIterator i = graph.rpoBegin(), e = graph.rpoEnd(); i != e; ++i)
        MOZ_ASSERT(!i->isMarked(), "Not all blocks got unmarked");
#endif
}

// Reorder the blocks in the loop starting at the given header to be contiguous.
static void
MakeLoopContiguous(MIRGraph &graph, MBasicBlock *header, size_t numMarked)
{
    MBasicBlock *backedge = header->backedge();

    MOZ_ASSERT(header->isMarked(), "Loop header is not part of loop");
    MOZ_ASSERT(backedge->isMarked(), "Loop backedge is not part of loop");

    // If there are any blocks between the loop header and the loop backedge
    // that are not part of the loop, prepare to move them to the end. We keep
    // them in order, which preserves RPO.
    ReversePostorderIterator insertIter = graph.rpoBegin(backedge);
    insertIter++;
    MBasicBlock *insertPt = *insertIter;

    // Visit all the blocks from the loop header to the loop backedge.
    size_t headerId = header->id();
    size_t inLoopId = headerId;
    size_t notInLoopId = inLoopId + numMarked;
    ReversePostorderIterator i = graph.rpoBegin(header);
    for (;;) {
        MBasicBlock *block = *i++;
        MOZ_ASSERT(block->id() >= header->id() && block->id() <= backedge->id(),
                   "Loop backedge should be last block in loop");

        if (block->isMarked()) {
            // This block is in the loop.
            block->unmark();
            block->setId(inLoopId++);
            // If we've reached the loop backedge, we're done!
            if (block == backedge)
                break;
        } else {
            // This block is not in the loop. Move it to the end.
            graph.moveBlockBefore(insertPt, block);
            block->setId(notInLoopId++);
        }
    }
    MOZ_ASSERT(header->id() == headerId, "Loop header id changed");
    MOZ_ASSERT(inLoopId == headerId + numMarked, "Wrong number of blocks kept in loop");
    MOZ_ASSERT(notInLoopId == (insertIter != graph.rpoEnd() ? insertPt->id() : graph.numBlocks()),
               "Wrong number of blocks moved out of loop");
}

// Reorder the blocks in the graph so that loops are contiguous.
bool
jit::MakeLoopsContiguous(MIRGraph &graph)
{
    // Visit all loop headers (in any order).
    for (MBasicBlockIterator i(graph.begin()); i != graph.end(); i++) {
        MBasicBlock *header = *i;
        if (!header->isLoopHeader())
            continue;

        // Mark all blocks that are actually part of the loop.
        bool canOsr;
        size_t numMarked = MarkLoopBlocks(graph, header, &canOsr);

        // Move all blocks between header and backedge that aren't marked to
        // the end of the loop, making the loop itself contiguous.
        MakeLoopContiguous(graph, header, numMarked);
    }

    return true;
}
