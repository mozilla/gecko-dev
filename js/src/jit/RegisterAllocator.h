/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_RegisterAllocator_h
#define jit_RegisterAllocator_h

#include "mozilla/Attributes.h"
#include "mozilla/MathAlgorithms.h"

#include "jit/LIR.h"
#include "jit/MIRGenerator.h"
#include "jit/MIRGraph.h"

// Generic structures and functions for use by register allocators.

namespace js {
namespace jit {

class LIRGenerator;

// Structure for running a liveness analysis on a finished register allocation.
// This analysis can be used for two purposes:
//
// - Check the integrity of the allocation, i.e. that the reads and writes of
//   physical values preserve the semantics of the original virtual registers.
//
// - Populate safepoints with live registers, GC thing and value data, to
//   streamline the process of prototyping new allocators.
struct AllocationIntegrityState
{
    explicit AllocationIntegrityState(const LIRGraph &graph)
      : graph(graph)
    {}

    // Record all virtual registers in the graph. This must be called before
    // register allocation, to pick up the original LUses.
    bool record();

    // Perform the liveness analysis on the graph, and assert on an invalid
    // allocation. This must be called after register allocation, to pick up
    // all assigned physical values. If populateSafepoints is specified,
    // safepoints will be filled in with liveness information.
    bool check(bool populateSafepoints);

  private:

    const LIRGraph &graph;

    // For all instructions and phis in the graph, keep track of the virtual
    // registers for all inputs and outputs of the nodes. These are overwritten
    // in place during register allocation. This information is kept on the
    // side rather than in the instructions and phis themselves to avoid
    // debug-builds-only bloat in the size of the involved structures.

    struct InstructionInfo {
        Vector<LAllocation, 2, SystemAllocPolicy> inputs;
        Vector<LDefinition, 0, SystemAllocPolicy> temps;
        Vector<LDefinition, 1, SystemAllocPolicy> outputs;

        InstructionInfo()
        { }

        InstructionInfo(const InstructionInfo &o)
        {
            inputs.appendAll(o.inputs);
            temps.appendAll(o.temps);
            outputs.appendAll(o.outputs);
        }
    };
    Vector<InstructionInfo, 0, SystemAllocPolicy> instructions;

    struct BlockInfo {
        Vector<InstructionInfo, 5, SystemAllocPolicy> phis;
        BlockInfo() {}
        BlockInfo(const BlockInfo &o) {
            phis.appendAll(o.phis);
        }
    };
    Vector<BlockInfo, 0, SystemAllocPolicy> blocks;

    Vector<LDefinition*, 20, SystemAllocPolicy> virtualRegisters;

    // Describes a correspondence that should hold at the end of a block.
    // The value which was written to vreg in the original LIR should be
    // physically stored in alloc after the register allocation.
    struct IntegrityItem
    {
        LBlock *block;
        uint32_t vreg;
        LAllocation alloc;

        // Order of insertion into seen, for sorting.
        uint32_t index;

        typedef IntegrityItem Lookup;
        static HashNumber hash(const IntegrityItem &item) {
            HashNumber hash = item.alloc.hash();
            hash = mozilla::RotateLeft(hash, 4) ^ item.vreg;
            hash = mozilla::RotateLeft(hash, 4) ^ HashNumber(item.block->mir()->id());
            return hash;
        }
        static bool match(const IntegrityItem &one, const IntegrityItem &two) {
            return one.block == two.block
                && one.vreg == two.vreg
                && one.alloc == two.alloc;
        }
    };

    // Items still to be processed.
    Vector<IntegrityItem, 10, SystemAllocPolicy> worklist;

    // Set of all items that have already been processed.
    typedef HashSet<IntegrityItem, IntegrityItem, SystemAllocPolicy> IntegrityItemSet;
    IntegrityItemSet seen;

    bool checkIntegrity(LBlock *block, LInstruction *ins, uint32_t vreg, LAllocation alloc,
                        bool populateSafepoints);
    bool checkSafepointAllocation(LInstruction *ins, uint32_t vreg, LAllocation alloc,
                                  bool populateSafepoints);
    bool addPredecessor(LBlock *block, uint32_t vreg, LAllocation alloc);

    void dump();
};

// Represents with better-than-instruction precision a position in the
// instruction stream.
//
// An issue comes up when performing register allocation as to how to represent
// information such as "this register is only needed for the input of
// this instruction, it can be clobbered in the output". Just having ranges
// of instruction IDs is insufficiently expressive to denote all possibilities.
// This class solves this issue by associating an extra bit with the instruction
// ID which indicates whether the position is the input half or output half of
// an instruction.
class CodePosition
{
  private:
    MOZ_CONSTEXPR explicit CodePosition(uint32_t bits)
      : bits_(bits)
    { }

    static const unsigned int INSTRUCTION_SHIFT = 1;
    static const unsigned int SUBPOSITION_MASK = 1;
    uint32_t bits_;

  public:
    static const CodePosition MAX;
    static const CodePosition MIN;

    // This is the half of the instruction this code position represents, as
    // described in the huge comment above.
    enum SubPosition {
        INPUT,
        OUTPUT
    };

    MOZ_CONSTEXPR CodePosition() : bits_(0)
    { }

    CodePosition(uint32_t instruction, SubPosition where) {
        JS_ASSERT(instruction < 0x80000000u);
        JS_ASSERT(((uint32_t)where & SUBPOSITION_MASK) == (uint32_t)where);
        bits_ = (instruction << INSTRUCTION_SHIFT) | (uint32_t)where;
    }

    uint32_t ins() const {
        return bits_ >> INSTRUCTION_SHIFT;
    }

    uint32_t bits() const {
        return bits_;
    }

    SubPosition subpos() const {
        return (SubPosition)(bits_ & SUBPOSITION_MASK);
    }

    bool operator <(CodePosition other) const {
        return bits_ < other.bits_;
    }

    bool operator <=(CodePosition other) const {
        return bits_ <= other.bits_;
    }

    bool operator !=(CodePosition other) const {
        return bits_ != other.bits_;
    }

    bool operator ==(CodePosition other) const {
        return bits_ == other.bits_;
    }

    bool operator >(CodePosition other) const {
        return bits_ > other.bits_;
    }

    bool operator >=(CodePosition other) const {
        return bits_ >= other.bits_;
    }

    uint32_t operator -(CodePosition other) const {
        JS_ASSERT(bits_ >= other.bits_);
        return bits_ - other.bits_;
    }

    CodePosition previous() const {
        JS_ASSERT(*this != MIN);
        return CodePosition(bits_ - 1);
    }
    CodePosition next() const {
        JS_ASSERT(*this != MAX);
        return CodePosition(bits_ + 1);
    }
};

// Structure to track moves inserted before or after an instruction.
class InstructionData
{
    LInstruction *ins_;
    LBlock *block_;
    LMoveGroup *inputMoves_;
    LMoveGroup *movesAfter_;

  public:
    void init(LInstruction *ins, LBlock *block) {
        JS_ASSERT(!ins_);
        JS_ASSERT(!block_);
        ins_ = ins;
        block_ = block;
    }
    LInstruction *ins() const {
        return ins_;
    }
    LBlock *block() const {
        return block_;
    }
    void setInputMoves(LMoveGroup *moves) {
        inputMoves_ = moves;
    }
    LMoveGroup *inputMoves() const {
        return inputMoves_;
    }
    void setMovesAfter(LMoveGroup *moves) {
        movesAfter_ = moves;
    }
    LMoveGroup *movesAfter() const {
        return movesAfter_;
    }
};

// Structure to track all moves inserted next to instructions in a graph.
class InstructionDataMap
{
    FixedList<InstructionData> insData_;

  public:
    InstructionDataMap()
      : insData_()
    { }

    bool init(MIRGenerator *gen, uint32_t numInstructions) {
        if (!insData_.init(gen->alloc(), numInstructions))
            return false;
        memset(&insData_[0], 0, sizeof(InstructionData) * numInstructions);
        return true;
    }

    InstructionData &operator[](CodePosition pos) {
        return operator[](pos.ins());
    }
    const InstructionData &operator[](CodePosition pos) const {
        return operator[](pos.ins());
    }
    InstructionData &operator[](LInstruction *ins) {
        return operator[](ins->id());
    }
    const InstructionData &operator[](LInstruction *ins) const {
        return operator[](ins->id());
    }
    InstructionData &operator[](uint32_t ins) {
        return insData_[ins];
    }
    const InstructionData &operator[](uint32_t ins) const {
        return insData_[ins];
    }
};

// Common superclass for register allocators.
class RegisterAllocator
{
    void operator=(const RegisterAllocator &) MOZ_DELETE;
    RegisterAllocator(const RegisterAllocator &) MOZ_DELETE;

  protected:
    // Context
    MIRGenerator *mir;
    LIRGenerator *lir;
    LIRGraph &graph;

    // Pool of all registers that should be considered allocateable
    RegisterSet allRegisters_;

    // Computed data
    InstructionDataMap insData;

    RegisterAllocator(MIRGenerator *mir, LIRGenerator *lir, LIRGraph &graph)
      : mir(mir),
        lir(lir),
        graph(graph),
        allRegisters_(RegisterSet::All())
    {
        if (FramePointer != InvalidReg && mir->instrumentedProfiling())
            allRegisters_.take(AnyRegister(FramePointer));
#if defined(JS_CODEGEN_X64)
        if (mir->compilingAsmJS())
            allRegisters_.take(AnyRegister(HeapReg));
#elif defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS)
        if (mir->compilingAsmJS()) {
            allRegisters_.take(AnyRegister(HeapReg));
            allRegisters_.take(AnyRegister(GlobalReg));
            allRegisters_.take(AnyRegister(NANReg));
        }
#endif
    }

    bool init();

    TempAllocator &alloc() const {
        return mir->alloc();
    }

    CodePosition outputOf(uint32_t pos) const {
        // All phis in a block write their outputs after all of them have
        // read their inputs. Consequently, it doesn't make sense to talk
        // about code positions in the middle of a series of phis.
        if (insData[pos].ins()->isPhi()) {
            while (insData[pos + 1].ins()->isPhi())
                ++pos;
        }
        return CodePosition(pos, CodePosition::OUTPUT);
    }
    CodePosition outputOf(const LInstruction *ins) const {
        return outputOf(ins->id());
    }
    CodePosition inputOf(uint32_t pos) const {
        // All phis in a block read their inputs before any of them write their
        // outputs. Consequently, it doesn't make sense to talk about code
        // positions in the middle of a series of phis.
        if (insData[pos].ins()->isPhi()) {
            while (pos > 0 && insData[pos - 1].ins()->isPhi())
                --pos;
        }
        return CodePosition(pos, CodePosition::INPUT);
    }
    CodePosition inputOf(const LInstruction *ins) const {
        return inputOf(ins->id());
    }
    CodePosition entryOf(const LBlock *block) {
        return inputOf(block->firstId());
    }
    CodePosition exitOf(const LBlock *block) {
        return outputOf(block->lastId());
    }

    LMoveGroup *getInputMoveGroup(uint32_t ins);
    LMoveGroup *getMoveGroupAfter(uint32_t ins);

    LMoveGroup *getInputMoveGroup(CodePosition pos) {
        return getInputMoveGroup(pos.ins());
    }
    LMoveGroup *getMoveGroupAfter(CodePosition pos) {
        return getMoveGroupAfter(pos.ins());
    }

    CodePosition minimalDefEnd(LInstruction *ins) {
        // Compute the shortest interval that captures vregs defined by ins.
        // Watch for instructions that are followed by an OSI point and/or Nop.
        // If moves are introduced between the instruction and the OSI point then
        // safepoint information for the instruction may be incorrect.
        while (true) {
            LInstruction *next = insData[outputOf(ins).next()].ins();
            if (!next->isNop() && !next->isOsiPoint())
                break;
            ins = next;
        }

        return outputOf(ins);
    }

    void dumpInstructions();
};

static inline AnyRegister
GetFixedRegister(const LDefinition *def, const LUse *use)
{
    return def->isFloatReg()
           ? AnyRegister(FloatRegister::FromCode(use->registerCode()))
           : AnyRegister(Register::FromCode(use->registerCode()));
}

} // namespace jit
} // namespace js

#endif /* jit_RegisterAllocator_h */
