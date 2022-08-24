/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_riscv64_Assembler_riscv64_h
#define jit_riscv64_Assembler_riscv64_h

#include "mozilla/Assertions.h"

#include <stdint.h>

#include "jit/CompactBuffer.h"
#include "jit/JitCode.h"
#include "jit/JitSpewer.h"
#include "jit/Registers.h"
#include "jit/RegisterSets.h"
#include "jit/riscv64/Architecture-riscv64.h"
#include "jit/riscv64/constant/Constant-riscv64.h"
#include "jit/riscv64/extension/base-assembler-riscv.h"
#include "jit/riscv64/extension/base-riscv-i.h"
#include "jit/riscv64/extension/extension-riscv-a.h"
#include "jit/riscv64/extension/extension-riscv-c.h"
#include "jit/riscv64/extension/extension-riscv-d.h"
#include "jit/riscv64/extension/extension-riscv-f.h"
#include "jit/riscv64/extension/extension-riscv-m.h"
#include "jit/riscv64/extension/extension-riscv-v.h"
#include "jit/riscv64/extension/extension-riscv-zicsr.h"
#include "jit/riscv64/extension/extension-riscv-zifencei.h"
#include "jit/riscv64/Register-riscv64.h"
#include "jit/shared/Assembler-shared.h"
#include "jit/shared/Disassembler-shared.h"
#include "jit/shared/IonAssemblerBuffer.h"
#include "wasm/WasmTypeDecls.h"
namespace js {
namespace jit {

#define DEBUG_PRINTF(...) \
  if (FLAG_riscv_debug) { \
    printf(__VA_ARGS__);  \
  }

// Difference between address of current opcode and value read from pc
// register.
static constexpr int kPcLoadDelta = 4;

// Bits available for offset field in branches
static constexpr int kBranchOffsetBits = 13;

// Bits available for offset field in jump
static constexpr int kJumpOffsetBits = 21;

// Bits available for offset field in compresed jump
static constexpr int kCJalOffsetBits = 12;

// Bits available for offset field in compressed branch
static constexpr int kCBranchOffsetBits = 9;

// Max offset for b instructions with 12-bit offset field (multiple of 2)
static constexpr int kMaxBranchOffset = (1 << (13 - 1)) - 1;

// Max offset for jal instruction with 20-bit offset field (multiple of 2)
static constexpr int kMaxJumpOffset = (1 << (21 - 1)) - 1;

static constexpr int kTrampolineSlotsSize = 2 * kInstrSize;
struct ScratchFloat32Scope : public AutoFloatRegisterScope {
  explicit ScratchFloat32Scope(MacroAssembler& masm)
      : AutoFloatRegisterScope(masm, ScratchFloat32Reg) {}
};

struct ScratchDoubleScope : public AutoFloatRegisterScope {
  explicit ScratchDoubleScope(MacroAssembler& masm)
      : AutoFloatRegisterScope(masm, ScratchDoubleReg) {}
};

class MacroAssembler;


static constexpr uint32_t ABIStackAlignment = 16;
static constexpr uint32_t CodeAlignment = 16;
static constexpr uint32_t JitStackAlignment = 16;
static constexpr uint32_t JitStackValueAlignment =
    JitStackAlignment / sizeof(Value);

static const Scale ScalePointer = TimesEight;


static constexpr int32_t SliceSize = 1024;
typedef js::jit::AssemblerBuffer<SliceSize, Instruction> Buffer;
class Assembler : public AssemblerShared,
                  public AssemblerRISCVI,
                  public AssemblerRISCVA,
                  public AssemblerRISCVF,
                  public AssemblerRISCVD,
                  public AssemblerRISCVM,
                  public AssemblerRISCVC,
                  public AssemblerRISCVZicsr,
                  public AssemblerRISCVZifencei {
 Buffer m_buffer;
 CompactBufferWriter jumpRelocations_;
 CompactBufferWriter dataRelocations_;
 GeneralRegisterSet scratch_register_list_;

  // One trampoline consists of:
  // - space for trampoline slots,
  // - space for labels.
  //
  // Space for trampoline slots is equal to slot_count * 2 * kInstrSize.
  // Space for trampoline slots precedes space for labels. Each label is of one
  // instruction size, so total amount for labels is equal to
  // label_count *  kInstrSize.
  class Trampoline {
   public:
    Trampoline() {
      start_ = 0;
      next_slot_ = 0;
      free_slot_count_ = 0;
      end_ = 0;
    }
    Trampoline(int start, int slot_count) {
      start_ = start;
      next_slot_ = start;
      free_slot_count_ = slot_count;
      end_ = start + slot_count * kTrampolineSlotsSize;
    }
    int start() { return start_; }
    int end() { return end_; }
    int take_slot() {
      int trampoline_slot = kInvalidSlotPos;
      if (free_slot_count_ <= 0) {
        // We have run out of space on trampolines.
        // Make sure we fail in debug mode, so we become aware of each case
        // when this happens.
        MOZ_ASSERT(0);
        // Internal exception will be caught.
      } else {
        trampoline_slot = next_slot_;
        free_slot_count_--;
        next_slot_ += kTrampolineSlotsSize;
      }
      return trampoline_slot;
    }

   private:
    int start_;
    int end_;
    int next_slot_;
    int free_slot_count_;
  };

 uint32_t next_buffer_check_;  // pc offset of next buffer check.
 // Automatic growth of the assembly buffer may be blocked for some sequences.
 bool block_buffer_growth_;  // Block growth when true.
 // Emission of the trampoline pool may be blocked in some code sequences.
 int trampoline_pool_blocked_nesting_;  // Block emission if this is not zero.
 uint32_t no_trampoline_pool_before_;        // Block emission before this pc offset.

 // Keep track of the last emitted pool to guarantee a maximal distance.
 int last_trampoline_pool_end_;  // pc offset of the end of the last pool.

 int unbound_labels_count_;
 // After trampoline is emitted, long branches are used in generated code for
 // the forward branches whose target offsets could be beyond reach of branch
 // instruction. We use this information to trigger different mode of
 // branch instruction generation, where we use jump instructions rather
 // than regular branch instructions.
 bool trampoline_emitted_ = false;
 static constexpr int kInvalidSlotPos = -1;

 Trampoline trampoline_;

#ifdef JS_JITSPEW
  Sprinter* printer;
#endif

 protected:
  bool isFinished = false;
  int32_t get_trampoline_entry(int32_t pos);
  Instruction* editSrc(BufferOffset bo) { return m_buffer.getInst(bo); }

 public:

  static bool FLAG_riscv_debug;

  Assembler()
      : m_buffer(),
        scratch_register_list_((1 << t3.code()) | (1 << t5.code()) |
                               (1 << s10.code()) | (1 << s11.code())),
#ifdef JS_JITSPEW
        printer(nullptr),
#endif
        isFinished(false) {
    last_trampoline_pool_end_ = 0;
    no_trampoline_pool_before_ = 0;
    trampoline_pool_blocked_nesting_ = 0;
    // We leave space (16 * kTrampolineSlotsSize)
    // for BlockTrampolinePoolScope buffer.
    next_buffer_check_ = kMaxBranchOffset - kTrampolineSlotsSize * 16;
    trampoline_emitted_ = false;
    unbound_labels_count_ = 0;
    block_buffer_growth_ = false;
  }
  bool oom() const;
  BufferOffset nextOffset() { return m_buffer.nextOffset(); }

#ifdef JS_JITSPEW
  inline void spew(const char* fmt, ...) MOZ_FORMAT_PRINTF(2, 3) {
    if (MOZ_UNLIKELY(printer || JitSpewEnabled(JitSpew_Codegen))) {
      va_list va;
      va_start(va, fmt);
      spew(fmt, va);
      va_end(va);
    }
  }

#else
  MOZ_ALWAYS_INLINE void spew(const char* fmt, ...) MOZ_FORMAT_PRINTF(2, 3) {}
#endif

  enum Condition {
    Overflow = overflow,
    Below = Uless,
    BelowOrEqual = Uless_equal,
    Above = Ugreater,
    AboveOrEqual = Ugreater_equal,
    Equal = equal,
    NotEqual = not_equal,
    GreaterThan = greater,
    GreaterThanOrEqual = greater_equal,
    LessThan = less,
    LessThanOrEqual = less_equal,
    Always = cc_always,
    CarrySet,
    CarryClear,
    Signed,
    NotSigned,
    Zero,
    NonZero,
  };

    // Returns the equivalent of !cc.
  inline Condition NegateCondition(Condition cc) {
    MOZ_ASSERT(cc != Always);
    return static_cast<Condition>(cc ^ 1);
  }

  enum DoubleCondition {
    // These conditions will only evaluate to true if the comparison is ordered
    // - i.e. neither operand is NaN.
    DoubleOrdered,
    DoubleEqual,
    DoubleNotEqual,
    DoubleGreaterThan,
    DoubleGreaterThanOrEqual,
    DoubleLessThan,
    DoubleLessThanOrEqual,
    // If either operand is NaN, these conditions always evaluate to true.
    DoubleUnordered,
    DoubleEqualOrUnordered,
    DoubleNotEqualOrUnordered,
    DoubleGreaterThanOrUnordered,
    DoubleGreaterThanOrEqualOrUnordered,
    DoubleLessThanOrUnordered,
    DoubleLessThanOrEqualOrUnordered
  };

  
  Register getStackPointer() const { return StackPointer; }
  
  void disassembleInstr(Instr instr);
  int target_at(BufferOffset pos, bool is_internal);
  uint32_t next_link(Label* label, bool is_internal);
  void target_at_put(BufferOffset pos, BufferOffset target_pos);
  virtual int32_t branch_offset_helper(Label* L, OffsetSize bits);
  int32_t branch_long_offset(Label* L);

  // Determines if Label is bound and near enough so that branch instruction
  // can be used to reach it, instead of jump instruction.
  bool is_near(Label* L);
  bool is_near(Label* L, OffsetSize bits);
  bool is_near_branch(Label* L);

  virtual void emit(Instr x) { MOZ_CRASH(); }
  virtual void emit(ShortInstr x) { MOZ_CRASH(); }
  virtual void emit(uint64_t x) { MOZ_CRASH(); }

  virtual void BlockTrampolinePoolFor(int instructions) { MOZ_CRASH(); }

  static Condition InvertCondition(Condition) { MOZ_CRASH(); }

  static DoubleCondition InvertCondition(DoubleCondition) { MOZ_CRASH(); }

  template <typename T, typename S>
  static void PatchDataWithValueCheck(CodeLocationLabel, T, S) {
    MOZ_CRASH();
  }
  static void PatchWrite_Imm32(CodeLocationLabel, Imm32) { MOZ_CRASH(); }

  static void PatchWrite_NearCall(CodeLocationLabel, CodeLocationLabel) {
    MOZ_CRASH();
  }
  static uint32_t PatchWrite_NearCallSize() { MOZ_CRASH(); }

  static void ToggleToJmp(CodeLocationLabel) { MOZ_CRASH(); }
  static void ToggleToCmp(CodeLocationLabel) { MOZ_CRASH(); }
  static void ToggleCall(CodeLocationLabel, bool) { MOZ_CRASH(); }

  static void Bind(uint8_t*, const CodeLabel&) { MOZ_CRASH(); }
  // label operations
  void bind(Label* label, BufferOffset boff = BufferOffset());
  void bind(CodeLabel* label) {
    label->target()->bind(currentOffset());
  }
  uint32_t currentOffset() { return nextOffset().getOffset(); }

  static uintptr_t GetPointer(uint8_t*) { MOZ_CRASH(); }

  static bool HasRoundInstruction(RoundingMode) { return false; }

  void verifyHeapAccessDisassembly(uint32_t begin, uint32_t end,
                                   const Disassembler::HeapAccess& heapAccess) {
    MOZ_CRASH();
  }

  void setUnlimitedBuffer() { MOZ_CRASH(); }

  GeneralRegisterSet* GetScratchRegisterList() { return &scratch_register_list_; }

  bool is_trampoline_emitted() const { return trampoline_emitted_; }

  void CheckTrampolinePool();

  void CheckTrampolinePoolQuick(uint32_t extra_instructions = 0) {
    DEBUG_PRINTF("\tpc_offset:%d %d\n", currentOffset(),
                 next_buffer_check_ - extra_instructions * kInstrSize);
    if (currentOffset() >= next_buffer_check_ - extra_instructions * kInstrSize) {
      CheckTrampolinePool();
    }
  }

  void StartBlockTrampolinePool() {
    DEBUG_PRINTF("\tStartBlockTrampolinePool\n");
    trampoline_pool_blocked_nesting_++;
  }

  void EndBlockTrampolinePool() {
    trampoline_pool_blocked_nesting_--;
    DEBUG_PRINTF("\ttrampoline_pool_blocked_nesting:%d\n",
                 trampoline_pool_blocked_nesting_);
    if (trampoline_pool_blocked_nesting_ == 0) {
      CheckTrampolinePoolQuick(1);
    }
  }

  bool is_trampoline_pool_blocked() const {
    return trampoline_pool_blocked_nesting_ > 0;
  }

  // Block the emission of the trampoline pool before pc_offset.
  void BlockTrampolinePoolBefore(uint32_t pc_offset) {
    if (no_trampoline_pool_before_ < pc_offset)
      no_trampoline_pool_before_ = pc_offset;
  }

  void EmitConstPoolWithJumpIfNeeded(size_t margin = 0) {
    
  }
};


class ABIArgGenerator {
 public:
  ABIArgGenerator()
      : intRegIndex_(0), floatRegIndex_(0), stackOffset_(0), current_() {}
  ABIArg next(MIRType);
  ABIArg& current() { return current_; }
  uint32_t stackBytesConsumedSoFar() const { return stackOffset_; }
  void increaseStackOffset(uint32_t bytes) { stackOffset_ += bytes; }
 protected:
  unsigned intRegIndex_;
  unsigned floatRegIndex_;
  uint32_t stackOffset_;
  ABIArg current_;
};

// Helper classes for ScratchRegister usage. Asserts that only one piece
// of code thinks it has exclusive ownership of each scratch register.
struct ScratchRegisterScope : public AutoRegisterScope {
  explicit ScratchRegisterScope(MacroAssembler& masm)
      : AutoRegisterScope(masm, ScratchRegister) {}
};

struct SecondScratchRegisterScope : public AutoRegisterScope {
  explicit SecondScratchRegisterScope(MacroAssembler& masm)
      : AutoRegisterScope(masm, SecondScratchReg) {}
};

static const uint32_t NumIntArgRegs = 8;
static const uint32_t NumFloatArgRegs = 8;

static inline bool GetIntArgReg(uint32_t usedIntArgs, Register& out) {
  if (usedIntArgs < NumIntArgRegs) {
    out = Register::FromCode(a0.code() + usedIntArgs);
    return true;
  }
  return false;
}

static inline bool GetFloatArgReg(uint32_t usedFloatArgs, FloatRegister* out) {
  if (usedFloatArgs < NumFloatArgRegs) {
    *out = FloatRegister::FromCode(fa0.code() + usedFloatArgs);
    return true;
  }
  return false;
}


class BlockTrampolinePoolScope {
  public:
  explicit BlockTrampolinePoolScope(Assembler* assem, int margin = 0)
      : assem_(assem) {
    assem_->StartBlockTrampolinePool();
  }
  ~BlockTrampolinePoolScope() { assem_->EndBlockTrampolinePool(); }

  private:
  Assembler* assem_;
  BlockTrampolinePoolScope() = delete;
  BlockTrampolinePoolScope(const BlockTrampolinePoolScope&) = delete;
  BlockTrampolinePoolScope& operator=(const BlockTrampolinePoolScope&) = delete;
};
class  UseScratchRegisterScope {
 public:
  explicit UseScratchRegisterScope(Assembler* assembler);
  ~UseScratchRegisterScope();

  Register Acquire();
  bool hasAvailable() const;
  void Include(const GeneralRegisterSet& list) {
    *available_ = GeneralRegisterSet::Intersect(*available_, list);
  }
  void Exclude(const GeneralRegisterSet& list) {
    *available_ = GeneralRegisterSet::Subtract(*available_, list);
  }
 private:
  GeneralRegisterSet* available_;
  GeneralRegisterSet old_available_;
};

// Class Operand represents a shifter operand in data processing instructions.
class Operand {
 public:
  enum Tag { REG, FREG, MEM, IMM };
  Operand(FloatRegister freg) : tag(FREG), rm_(freg.code()) {}

  explicit Operand(Register base, Imm32 off)
      : tag(MEM), rm_(base.code()), offset(off.value) {}

  explicit Operand(Register base, int32_t off)
      : tag(MEM), rm_(base.code()), offset(off) {}

  explicit Operand(const Address& addr)
      : tag(MEM), rm_(addr.base.code()), offset(addr.offset) {}

  explicit Operand(intptr_t immediate) : tag(IMM), rm_() { value_ = immediate; }
  // Register.
  Operand(const Register rm) : tag(REG), rm_(rm.code()) {}
  // Return true if this is a register operand.
  bool is_reg() const { return tag == REG; }
  bool is_freg() const { return tag == FREG; }
  bool is_mem() const { return tag == MEM; }
  bool is_imm() const { return tag == IMM; }
  inline intptr_t immediate() const {
    MOZ_ASSERT(is_imm());
    return value_;
  }
  bool IsImmediate() const { return !is_reg(); }
  Register rm() const { return Register::FromCode(rm_); }

 private:
  Tag tag;
  uint32_t rm_;
  int32_t offset;
  intptr_t value_;                                 // valid if rm_ == no_reg

  friend class Assembler;
  friend class MacroAssembler;
};

}  // namespace jit
}  // namespace js
#endif /* jit_riscv64_Assembler_riscv64_h */
