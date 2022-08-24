/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_riscv64_MacroAssembler_riscv64_h
#define jit_riscv64_MacroAssembler_riscv64_h

#include <iterator>

#include "jit/MoveResolver.h"
#include "jit/riscv64/Assembler-riscv64.h"
#include "wasm/WasmTypeDecls.h"

namespace js {
namespace jit {

static Register CallReg = t6;

enum LiFlags {
  Li64 = 0,
  Li48 = 1,
};

class CompactBufferReader;
enum LoadStoreSize {
  SizeByte = 8,
  SizeHalfWord = 16,
  SizeWord = 32,
  SizeDouble = 64
};

enum LoadStoreExtension { ZeroExtend = 0, SignExtend = 1 };
enum JumpKind { LongJump = 0, ShortJump = 1 };
class ScratchTagScope {
 public:
  ScratchTagScope(MacroAssembler&, const ValueOperand) {}
  operator Register() { MOZ_CRASH(); }
  void release() { MOZ_CRASH(); }
  void reacquire() { MOZ_CRASH(); }
};

class ScratchTagScopeRelease {
 public:
  explicit ScratchTagScopeRelease(ScratchTagScope*) {}
};

class MacroAssemblerRiscv64 : public Assembler {
 public:
  MacroAssemblerRiscv64() {}

  MoveResolver moveResolver_;

  size_t size() const { MOZ_CRASH(); }
  size_t bytesNeeded() const { MOZ_CRASH(); }
  size_t jumpRelocationTableBytes() const { MOZ_CRASH(); }
  size_t dataRelocationTableBytes() const { MOZ_CRASH(); }
  size_t preBarrierTableBytes() const { MOZ_CRASH(); }

  size_t numCodeLabels() const { MOZ_CRASH(); }
  CodeLabel codeLabel(size_t) { MOZ_CRASH(); }

  bool reserve(size_t size) { MOZ_CRASH(); }
  bool appendRawCode(const uint8_t* code, size_t numBytes) { MOZ_CRASH(); }
  bool swapBuffer(wasm::Bytes& bytes) { MOZ_CRASH(); }

  void assertNoGCThings() const { MOZ_CRASH(); }

  static void TraceJumpRelocations(JSTracer*, JitCode*, CompactBufferReader&) {
    MOZ_CRASH();
  }
  static void TraceDataRelocations(JSTracer*, JitCode*, CompactBufferReader&) {
    MOZ_CRASH();
  }

  static bool SupportsFloatingPoint() { return true; }
  static bool SupportsUnalignedAccesses() { return true; }
  static bool SupportsFastUnalignedFPAccesses() { return true; }

  void executableCopy(void*, bool = true) { MOZ_CRASH(); }
  void copyJumpRelocationTable(uint8_t*) { MOZ_CRASH(); }
  void copyDataRelocationTable(uint8_t*) { MOZ_CRASH(); }
  void copyPreBarrierTable(uint8_t*) { MOZ_CRASH(); }
  void processCodeLabels(uint8_t*) { MOZ_CRASH(); }

  void flushBuffer() { MOZ_CRASH(); }

  template <typename T>
  void j(Condition, T) {
    MOZ_CRASH();
  }
  void haltingAlign(size_t) { MOZ_CRASH(); }
  void nopAlign(size_t) { MOZ_CRASH(); }

  // TODO(RISCV) Reorder parameters so out parameters come last.
  bool CalculateOffset(Label* L, int32_t* offset, OffsetSize bits);
  int32_t GetOffset(int32_t offset, Label* L, OffsetSize bits);

  void finish() { MOZ_CRASH(); }

  inline void GenPCRelativeJump(Register rd, int32_t imm32) {
    MOZ_ASSERT(is_int32(imm32 + 0x800));
    int32_t Hi20 = ((imm32 + 0x800) >> 12);
    int32_t Lo12 = imm32 << 20 >> 20;
    auipc(rd, Hi20);  // Read PC + Hi20 into scratch.
    jr(rd, Lo12);     // jump PC + Hi20 + Lo12
  }

  // load
  void ma_load(Register dest,
               Address address,
               LoadStoreSize size = SizeWord,
               LoadStoreExtension extension = SignExtend);
  void ma_load(Register dest,
               const BaseIndex& src,
               LoadStoreSize size = SizeWord,
               LoadStoreExtension extension = SignExtend);
  // store
  void ma_store(Register data,
                Address address,
                LoadStoreSize size = SizeWord,
                LoadStoreExtension extension = SignExtend);
  void ma_liPatchable(Register dest, ImmPtr imm);
  void ma_liPatchable(Register dest, ImmWord imm, LiFlags flags = Li48);
  void ma_li(Register dest, ImmGCPtr ptr);
  void ma_li(Register dest, Imm32 imm);
  void ma_li(Register dest, CodeLabel* label);
  void ma_li(Register dest, ImmWord imm);

  // branches when done from within la-specific code
  void ma_b(Register lhs,
            Register rhs,
            Label* l,
            Condition c,
            JumpKind jumpKind = LongJump);
  void ma_b(Register lhs,
            Imm32 imm,
            Label* l,
            Condition c,
            JumpKind jumpKind = LongJump);

  // arithmetic based ops
  // add
  void ma_add_d(Register rd, Register rj, Imm32 imm);
  void ma_add32TestOverflow(Register rd,
                            Register rj,
                            Register rk,
                            Label* overflow);
  void ma_add32TestOverflow(Register rd,
                            Register rj,
                            Imm32 imm,
                            Label* overflow);
  void ma_addPtrTestOverflow(Register rd,
                             Register rj,
                             Register rk,
                             Label* overflow);
  void ma_addPtrTestOverflow(Register rd,
                             Register rj,
                             Imm32 imm,
                             Label* overflow);
  void ma_addPtrTestOverflow(Register rd,
                             Register rj,
                             ImmWord imm,
                             Label* overflow);
  void ma_addPtrTestCarry(Condition cond,
                          Register rd,
                          Register rj,
                          Register rk,
                          Label* overflow);
  void ma_addPtrTestCarry(Condition cond,
                          Register rd,
                          Register rj,
                          Imm32 imm,
                          Label* overflow);
  void ma_addPtrTestCarry(Condition cond,
                          Register rd,
                          Register rj,
                          ImmWord imm,
                          Label* overflow);

  // subtract
  void ma_sub_d(Register rd, Register rj, Imm32 imm);
  void ma_sub32TestOverflow(Register rd,
                            Register rj,
                            Register rk,
                            Label* overflow);
  void ma_subPtrTestOverflow(Register rd,
                             Register rj,
                             Register rk,
                             Label* overflow);
  void ma_subPtrTestOverflow(Register rd,
                             Register rj,
                             Imm32 imm,
                             Label* overflow);

  // multiplies.  For now, there are only few that we care about.
  void ma_mul_d(Register rd, Register rj, Imm32 imm);
  void ma_mulh_d(Register rd, Register rj, Imm32 imm);
  void ma_mulPtrTestOverflow(Register rd,
                             Register rj,
                             Register rk,
                             Label* overflow);

  // stack
  void ma_pop(Register r);
  void ma_push(Register r);

  void branchWithCode(InstImm code, Label* label, JumpKind jumpKind);
  // branches when done from within la-specific code
  void ma_b(Register lhs,
            ImmWord imm,
            Label* l,
            Condition c,
            JumpKind jumpKind = LongJump);
  void ma_b(Register lhs,
            Address addr,
            Label* l,
            Condition c,
            JumpKind jumpKind = LongJump);
  void ma_b(Address addr,
            Imm32 imm,
            Label* l,
            Condition c,
            JumpKind jumpKind = LongJump);
  void ma_b(Address addr,
            ImmGCPtr imm,
            Label* l,
            Condition c,
            JumpKind jumpKind = LongJump);
  void ma_b(Address addr,
            Register rhs,
            Label* l,
            Condition c,
            JumpKind jumpKind = LongJump) {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    MOZ_ASSERT(rhs != scratch);
    ma_ld_d(scratch, addr);
    ma_b(scratch, rhs, l, c, jumpKind);
  }

  void ma_branch(Label* target,
                 Condition cond,
                 Register r1,
                 const Operand& r2,
                 JumpKind jumpKind = LongJump);

  void ma_bl(Label* l);

  // fp instructions
  void ma_lid(FloatRegister dest, double value);

  void ma_mv(FloatRegister src, ValueOperand dest);
  void ma_mv(ValueOperand src, FloatRegister dest);

  void ma_fld_s(FloatRegister ft, Address address);
  void ma_fld_d(FloatRegister ft, Address address);
  void ma_fst_d(FloatRegister ft, Address address);
  void ma_fst_s(FloatRegister ft, Address address);

  void ma_pop(FloatRegister f);
  void ma_push(FloatRegister f);

  void ma_cmp_set(Register dst, Register lhs, ImmWord imm, Condition c);
  void ma_cmp_set(Register dst, Register lhs, ImmPtr imm, Condition c);
  void ma_cmp_set(Register dst, Address address, Imm32 imm, Condition c);
  void ma_cmp_set(Register dst, Address address, ImmWord imm, Condition c);

  void ma_rotr_w(Register rd, Register rj, Imm32 shift);

  void ma_fmovz(FloatFormat fmt,
                FloatRegister fd,
                FloatRegister fj,
                Register rk);
  void ma_fmovn(FloatFormat fmt,
                FloatRegister fd,
                FloatRegister fj,
                Register rk);

  void ma_and(Register rd, Register rj, Imm32 imm, bool bit32 = false);

  void ma_or(Register rd, Register rj, Imm32 imm, bool bit32 = false);

  void ma_xor(Register rd, Register rj, Imm32 imm, bool bit32 = false);

  // arithmetic based ops
  // add
  void ma_add_w(Register rd, Register rj, Imm32 imm);
  void ma_add32TestCarry(Condition cond,
                         Register rd,
                         Register rj,
                         Register rk,
                         Label* overflow);
  void ma_add32TestCarry(Condition cond,
                         Register rd,
                         Register rj,
                         Imm32 imm,
                         Label* overflow);

  // subtract
  void ma_sub_w(Register rd, Register rj, Imm32 imm);
  void ma_sub_w(Register rd, Register rj, Register rk);
  void ma_sub32TestOverflow(Register rd,
                            Register rj,
                            Imm32 imm,
                            Label* overflow);

  // multiplies.  For now, there are only few that we care about.
  void ma_mul(Register rd, Register rj, Imm32 imm);
  void ma_mul32TestOverflow(Register rd,
                            Register rj,
                            Register rk,
                            Label* overflow);
  void ma_mul32TestOverflow(Register rd,
                            Register rj,
                            Imm32 imm,
                            Label* overflow);

  // divisions
  void ma_div_branch_overflow(Register rd,
                              Register rj,
                              Register rk,
                              Label* overflow);
  void ma_div_branch_overflow(Register rd,
                              Register rj,
                              Imm32 imm,
                              Label* overflow);

  // fast mod, uses scratch registers, and thus needs to be in the assembler
  // implicitly assumes that we can overwrite dest at the beginning of the
  // sequence
  void ma_mod_mask(Register src,
                   Register dest,
                   Register hold,
                   Register remain,
                   int32_t shift,
                   Label* negZero = nullptr);

  // fp instructions
  void ma_lis(FloatRegister dest, float value);

  void ma_fst_d(FloatRegister src, BaseIndex address);
  void ma_fst_s(FloatRegister src, BaseIndex address);

  void ma_fld_d(FloatRegister dest, const BaseIndex& src);
  void ma_fld_s(FloatRegister dest, const BaseIndex& src);

  // FP branches
  void ma_bc_s(FloatRegister lhs,
               FloatRegister rhs,
               Label* label,
               DoubleCondition c,
               JumpKind jumpKind = LongJump,
               FPConditionBit fcc = FCC0);
  void ma_bc_d(FloatRegister lhs,
               FloatRegister rhs,
               Label* label,
               DoubleCondition c,
               JumpKind jumpKind = LongJump,
               FPConditionBit fcc = FCC0);

  void ma_call(ImmPtr dest);

  void ma_jump(ImmPtr dest);

  void ma_cmp_set(Register dst, Register lhs, Register rhs, Condition c);
  void ma_cmp_set(Register dst, Register lhs, Imm32 imm, Condition c);
  void ma_cmp_set_double(Register dst,
                         FloatRegister lhs,
                         FloatRegister rhs,
                         DoubleCondition c);
  void ma_cmp_set_float32(Register dst,
                          FloatRegister lhs,
                          FloatRegister rhs,
                          DoubleCondition c);

  void BranchShort(Label* L);

  void BranchShort(int32_t offset,
                   Condition cond,
                   Register rs,
                   const Operand& rt);
  void BranchShort(Label* L, Condition cond, Register rs, const Operand& rt);
  void BranchShortHelper(int32_t offset, Label* L);
  bool BranchShortHelper(int32_t offset,
                         Label* L,
                         Condition cond,
                         Register rs,
                         const Operand& rt);
  bool BranchShortCheck(int32_t offset,
                        Label* L,
                        Condition cond,
                        Register rs,
                        const Operand& rt);
  void BranchLong(Label* L);

  // Bit field starts at bit pos and extending for size bits is extracted from
  // rs and stored zero/sign-extended and right-justified in rt
  void ExtractBits(Register rt, Register rs, uint16_t pos, uint16_t size,
                   bool sign_extend = false);
  void ExtractBits(Register dest, Register source, Register pos, int size,
                   bool sign_extend = false) {
    sra(dest, source, pos);
    ExtractBits(dest, dest, 0, size, sign_extend);
  }

  // Insert bits [0, size) of source to bits [pos, pos+size) of dest
  void InsertBits(Register dest, Register source, Register pos, int size);

  // Insert bits [0, size) of source to bits [pos, pos+size) of dest
  void InsertBits(Register dest, Register source, int pos, int size);
};

class MacroAssemblerRiscv64Compat : public MacroAssemblerRiscv64 {
 public:
  using MacroAssemblerRiscv64::call;

  MacroAssemblerRiscv64Compat() {}

  void convertBoolToInt32(Register src, Register dest) {
    ma_and(dest, src, Imm32(0xff));
  };
  void convertInt32ToDouble(Register src, FloatRegister dest) {
    fcvt_d_w(dest, src);
  };
  void convertInt32ToDouble(const Address& src, FloatRegister dest) {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    ma_load(scratch, src, SizeWord, SignExtend);
    fcvt_d_w(dest, scratch);
  };
  void convertInt32ToDouble(const BaseIndex& src, FloatRegister dest) {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    MOZ_ASSERT(scratch != src.base);
    MOZ_ASSERT(scratch != src.index);
    computeScaledAddress(src, scratch);
    convertInt32ToDouble(Address(scratch, src.offset), dest);
  };
  void convertUInt32ToDouble(Register src, FloatRegister dest);
  void convertUInt32ToFloat32(Register src, FloatRegister dest);
  void convertDoubleToFloat32(FloatRegister src, FloatRegister dest);
  void convertDoubleToInt32(FloatRegister src,
                            Register dest,
                            Label* fail,
                            bool negativeZeroCheck = true);
  void convertDoubleToPtr(FloatRegister src,
                          Register dest,
                          Label* fail,
                          bool negativeZeroCheck = true);
  void convertFloat32ToInt32(FloatRegister src,
                             Register dest,
                             Label* fail,
                             bool negativeZeroCheck = true);

  void convertFloat32ToDouble(FloatRegister src, FloatRegister dest);
  void convertInt32ToFloat32(Register src, FloatRegister dest);
  void convertInt32ToFloat32(const Address& src, FloatRegister dest);

  void movq(Register rj, Register rd);

  void computeScaledAddress(const BaseIndex& address, Register dest);

  void computeEffectiveAddress(const Address& address, Register dest) {
    ma_add_d(dest, address.base, Imm32(address.offset));
  }

  void computeEffectiveAddress(const BaseIndex& address, Register dest) {
    computeScaledAddress(address, dest);
    if (address.offset) {
      ma_add_d(dest, dest, Imm32(address.offset));
    }
  }

  void j(Label* dest) { ma_b(dest); }

  void mov(Register src, Register dest) { addi(dest, src, 0); }
  void mov(ImmWord imm, Register dest) { ma_li(dest, imm); }
  void mov(ImmPtr imm, Register dest) {
    mov(ImmWord(uintptr_t(imm.value)), dest);
  }
  void mov(CodeLabel* label, Register dest) { ma_li(dest, label); }
  void mov(Register src, Address dest) { MOZ_CRASH("NYI-IC"); }
  void mov(Address src, Register dest) { MOZ_CRASH("NYI-IC"); }

  void writeDataRelocation(const Value& val) {
    // Raw GC pointer relocations and Value relocations both end up in
    // TraceOneDataRelocation.
    if (val.isGCThing()) {
      gc::Cell* cell = val.toGCThing();
      if (cell && gc::IsInsideNursery(cell)) {
        embedsNurseryPointers_ = true;
      }
      dataRelocations_.writeUnsigned(currentOffset());
    }
  }

  void branch(JitCode* c) {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    BufferOffset bo = m_buffer.nextOffset();
    addPendingJump(bo, ImmPtr(c->raw()), RelocationKind::JITCODE);
    ma_liPatchable(scratch, ImmPtr(c->raw()));
    jr(scratch);
  }
  void branch(const Register reg) { jr(reg); }
  void ret() {
    ma_pop(ra);
    jalr(zero_reg, ra, 0); 
  }
  inline void retn(Imm32 n);
  void push(Imm32 imm) {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    ma_li(scratch, imm);
    ma_push(scratch);
  }
  void push(ImmWord imm) {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    ma_li(scratch, imm);
    ma_push(scratch);
  }
  void push(ImmGCPtr imm) {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    ma_li(scratch, imm);
    ma_push(scratch);
  }
  void push(const Address& address) {
    SecondScratchRegisterScope scratch2(asMasm());
    loadPtr(address, scratch2);
    ma_push(scratch2);
  }
  void push(Register reg) { ma_push(reg); }
  void push(FloatRegister reg) { ma_push(reg); }
  void pop(Register reg) { ma_pop(reg); }
  void pop(FloatRegister reg) { ma_pop(reg); }

  // Emit a branch that can be toggled to a non-operation. On LOONG64 we use
  // "andi" instruction to toggle the branch.
  // See ToggleToJmp(), ToggleToCmp().
  CodeOffset toggledJump(Label* label);

  // Emit a "jalr" or "nop" instruction. ToggleCall can be used to patch
  // this instruction.
  CodeOffset toggledCall(JitCode* target, bool enabled);

  static size_t ToggledCallSize(uint8_t* code) {
    // Four instructions used in: MacroAssemblerRiscv64Compat::toggledCall
    return 4 * sizeof(uint32_t);
  }

  CodeOffset pushWithPatch(ImmWord imm) {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    CodeOffset offset = movWithPatch(imm, scratch);
    ma_push(scratch);
    return offset;
  }

  CodeOffset movWithPatch(ImmWord imm, Register dest) {
    CodeOffset offset = CodeOffset(currentOffset());
    ma_liPatchable(dest, imm, Li64);
    return offset;
  }
  CodeOffset movWithPatch(ImmPtr imm, Register dest) {
    CodeOffset offset = CodeOffset(currentOffset());
    ma_liPatchable(dest, imm);
    return offset;
  }

  void writeCodePointer(CodeLabel* label) {
    label->patchAt()->bind(currentOffset());
    label->setLinkMode(CodeLabel::RawPointer);
    m_buffer.ensureSpace(sizeof(void*));
    emit(uint64_t(-1));
  }

  void jump(Label* label) { ma_b(label); }
  void jump(Register reg) { jr(reg); }
  void jump(const Address& address) {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    loadPtr(address, scratch);
    jr(scratch);
  }

  void jump(JitCode* code) { branch(code); }

  void jump(ImmPtr ptr) {
    BufferOffset bo = m_buffer.nextOffset();
    addPendingJump(bo, ptr, RelocationKind::HARDCODED);
    ma_jump(ptr);
  }

  void jump(TrampolinePtr code) { jump(ImmPtr(code.value)); }

  void splitTag(Register src, Register dest) {
    srli(dest, src, JSVAL_TAG_SHIFT);
  }

  void splitTag(const ValueOperand& operand, Register dest) {
    splitTag(operand.valueReg(), dest);
  }

  void splitTagForTest(const ValueOperand& value, ScratchTagScope& tag) {
    splitTag(value, tag);
  }

  // unboxing code
  void unboxNonDouble(const ValueOperand& operand,
                      Register dest,
                      JSValueType type) {
    unboxNonDouble(operand.valueReg(), dest, type);
  }

  template <typename T>
  void unboxNonDouble(T src, Register dest, JSValueType type) {
    MOZ_ASSERT(type != JSVAL_TYPE_DOUBLE);
    if (type == JSVAL_TYPE_INT32 || type == JSVAL_TYPE_BOOLEAN) {
      load32(src, dest);
      return;
    }
    loadPtr(src, dest);
    unboxNonDouble(dest, dest, type);
  }

  void unboxNonDouble(Register src, Register dest, JSValueType type) {
    MOZ_ASSERT(type != JSVAL_TYPE_DOUBLE);
    if (type == JSVAL_TYPE_INT32 || type == JSVAL_TYPE_BOOLEAN) {
      slliw(dest, src, 0);
      return;
    }
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    MOZ_ASSERT(scratch != src);
    mov(ImmWord(JSVAL_TYPE_TO_SHIFTED_TAG(type)), scratch);
    xor_(dest, src, scratch);
  }

  template <typename T>
  void unboxObjectOrNull(const T& src, Register dest) {
    unboxNonDouble(src, dest, JSVAL_TYPE_OBJECT);
    static_assert(JS::detail::ValueObjectOrNullBit ==
                  (uint64_t(0x8) << JSVAL_TAG_SHIFT));
    InsertBits(dest, zero, JSVAL_TAG_SHIFT + 3, JSVAL_TAG_SHIFT + 3);
  }

  void unboxGCThingForGCBarrier(const Address& src, Register dest) {
    loadPtr(src, dest);
    as_bstrpick_d(dest, dest, JSVAL_TAG_SHIFT - 1, 0);
  }
  void unboxGCThingForGCBarrier(const ValueOperand& src, Register dest) {
    as_bstrpick_d(dest, src.valueReg(), JSVAL_TAG_SHIFT - 1, 0);
  }

  void unboxInt32(const ValueOperand& operand, Register dest);
  void unboxInt32(Register src, Register dest);
  void unboxInt32(const Address& src, Register dest);
  void unboxInt32(const BaseIndex& src, Register dest);
  void unboxBoolean(const ValueOperand& operand, Register dest);
  void unboxBoolean(Register src, Register dest);
  void unboxBoolean(const Address& src, Register dest);
  void unboxBoolean(const BaseIndex& src, Register dest);
  void unboxDouble(const ValueOperand& operand, FloatRegister dest);
  void unboxDouble(Register src, Register dest);
  void unboxDouble(const Address& src, FloatRegister dest);
  void unboxDouble(const BaseIndex& src, FloatRegister dest);
  void unboxString(const ValueOperand& operand, Register dest);
  void unboxString(Register src, Register dest);
  void unboxString(const Address& src, Register dest);
  void unboxSymbol(const ValueOperand& src, Register dest);
  void unboxSymbol(Register src, Register dest);
  void unboxSymbol(const Address& src, Register dest);
  void unboxBigInt(const ValueOperand& operand, Register dest);
  void unboxBigInt(Register src, Register dest);
  void unboxBigInt(const Address& src, Register dest);
  void unboxObject(const ValueOperand& src, Register dest);
  void unboxObject(Register src, Register dest);
  void unboxObject(const Address& src, Register dest);
  void unboxObject(const BaseIndex& src, Register dest) {
    unboxNonDouble(src, dest, JSVAL_TYPE_OBJECT);
  }
  void unboxValue(const ValueOperand& src, AnyRegister dest, JSValueType type);

  void notBoolean(const ValueOperand& val) {
    as_xori(val.valueReg(), val.valueReg(), 1);
  }

  // boxing code
  void boxDouble(FloatRegister src, const ValueOperand& dest, FloatRegister);
  void boxNonDouble(JSValueType type, Register src, const ValueOperand& dest);

  // Extended unboxing API. If the payload is already in a register, returns
  // that register. Otherwise, provides a move to the given scratch register,
  // and returns that.
  [[nodiscard]] Register extractObject(const Address& address,
                                       Register scratch);
  [[nodiscard]] Register extractObject(const ValueOperand& value,
                                       Register scratch) {
    unboxObject(value, scratch);
    return scratch;
  }
  [[nodiscard]] Register extractString(const ValueOperand& value,
                                       Register scratch) {
    unboxString(value, scratch);
    return scratch;
  }
  [[nodiscard]] Register extractSymbol(const ValueOperand& value,
                                       Register scratch) {
    unboxSymbol(value, scratch);
    return scratch;
  }
  [[nodiscard]] Register extractInt32(const ValueOperand& value,
                                      Register scratch) {
    unboxInt32(value, scratch);
    return scratch;
  }
  [[nodiscard]] Register extractBoolean(const ValueOperand& value,
                                        Register scratch) {
    unboxBoolean(value, scratch);
    return scratch;
  }
  [[nodiscard]] Register extractTag(const Address& address, Register scratch);
  [[nodiscard]] Register extractTag(const BaseIndex& address, Register scratch);
  [[nodiscard]] Register extractTag(const ValueOperand& value,
                                    Register scratch) {
    splitTag(value, scratch);
    return scratch;
  }

  inline void ensureDouble(const ValueOperand& source,
                           FloatRegister dest,
                           Label* failure);

  void boolValueToDouble(const ValueOperand& operand, FloatRegister dest);
  void int32ValueToDouble(const ValueOperand& operand, FloatRegister dest);
  void loadInt32OrDouble(const Address& src, FloatRegister dest);
  void loadInt32OrDouble(const BaseIndex& addr, FloatRegister dest);
  void loadConstantDouble(double dp, FloatRegister dest);

  void boolValueToFloat32(const ValueOperand& operand, FloatRegister dest);
  void int32ValueToFloat32(const ValueOperand& operand, FloatRegister dest);
  void loadConstantFloat32(float f, FloatRegister dest);

  void testNullSet(Condition cond, const ValueOperand& value, Register dest);

  void testObjectSet(Condition cond, const ValueOperand& value, Register dest);

  void testUndefinedSet(Condition cond,
                        const ValueOperand& value,
                        Register dest);

  // higher level tag testing code
  Address ToPayload(Address value) { return value; }

  template <typename T>
  void loadUnboxedValue(const T& address, MIRType type, AnyRegister dest) {
    if (dest.isFloat()) {
      loadInt32OrDouble(address, dest.fpu());
    } else {
      unboxNonDouble(address, dest.gpr(), ValueTypeFromMIRType(type));
    }
  }

  void storeUnboxedPayload(ValueOperand value,
                           BaseIndex address,
                           size_t nbytes,
                           JSValueType type) {
    switch (nbytes) {
      case 8: {
        UseScratchRegisterScope temps(this);
        Register scratch = temps.Acquire();
        SecondScratchRegisterScope scratch2(asMasm());
        if (type == JSVAL_TYPE_OBJECT) {
          unboxObjectOrNull(value, scratch2);
        } else {
          unboxNonDouble(value, scratch2, type);
        }
        computeEffectiveAddress(address, scratch);
        as_st_d(scratch2, scratch, 0);
        return;
      }
      case 4:
        store32(value.valueReg(), address);
        return;
      case 1:
        store8(value.valueReg(), address);
        return;
      default:
        MOZ_CRASH("Bad payload width");
    }
  }

  void storeUnboxedPayload(ValueOperand value,
                           Address address,
                           size_t nbytes,
                           JSValueType type) {
    switch (nbytes) {
      case 8: {
        SecondScratchRegisterScope scratch2(asMasm());
        if (type == JSVAL_TYPE_OBJECT) {
          unboxObjectOrNull(value, scratch2);
        } else {
          unboxNonDouble(value, scratch2, type);
        }
        storePtr(scratch2, address);
        return;
      }
      case 4:
        store32(value.valueReg(), address);
        return;
      case 1:
        store8(value.valueReg(), address);
        return;
      default:
        MOZ_CRASH("Bad payload width");
    }
  }

  void boxValue(JSValueType type, Register src, Register dest) {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    if (src == dest) {
      as_ori(scratch, src, 0);
      src = scratch;
    }
#ifdef DEBUG
    if (type == JSVAL_TYPE_INT32 || type == JSVAL_TYPE_BOOLEAN) {
      Label upper32BitsSignExtended;
      as_slli_w(dest, src, 0);
      ma_b(src, dest, &upper32BitsSignExtended, Equal, ShortJump);
      breakpoint();
      bind(&upper32BitsSignExtended);
    }
#endif
    ma_li(dest, ImmWord(JSVAL_TYPE_TO_SHIFTED_TAG(type)));
    if (type == JSVAL_TYPE_INT32 || type == JSVAL_TYPE_BOOLEAN) {
      as_bstrins_d(dest, src, 31, 0);
    } else {
      as_bstrins_d(dest, src, JSVAL_TAG_SHIFT - 1, 0);
    }
  }

  void storeValue(ValueOperand val, const Address& dest);
  void storeValue(ValueOperand val, const BaseIndex& dest);
  void storeValue(JSValueType type, Register reg, Address dest);
  void storeValue(JSValueType type, Register reg, BaseIndex dest);
  void storeValue(const Value& val, Address dest);
  void storeValue(const Value& val, BaseIndex dest);
  void storeValue(const Address& src, const Address& dest, Register temp) {
    loadPtr(src, temp);
    storePtr(temp, dest);
  }

  void storePrivateValue(Register src, const Address& dest) {
    storePtr(src, dest);
  }
  void storePrivateValue(ImmGCPtr imm, const Address& dest) {
    storePtr(imm, dest);
  }

  void loadValue(Address src, ValueOperand val);
  void loadValue(const BaseIndex& src, ValueOperand val);

  void loadUnalignedValue(const Address& src, ValueOperand dest) {
    loadValue(src, dest);
  }

  void tagValue(JSValueType type, Register payload, ValueOperand dest);

  void pushValue(ValueOperand val);
  void popValue(ValueOperand val);
  void pushValue(const Value& val) {
    if (val.isGCThing()) {
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      writeDataRelocation(val);
      movWithPatch(ImmWord(val.asRawBits()), scratch);
      push(scratch);
    } else {
      push(ImmWord(val.asRawBits()));
    }
  }
  void pushValue(JSValueType type, Register reg) {
    SecondScratchRegisterScope scratch2(asMasm());
    boxValue(type, reg, scratch2);
    push(scratch2);
  }
  void pushValue(const Address& addr);

  void handleFailureWithHandlerTail(Label* profilerExitTail,
                                    Label* bailoutTail);

  /////////////////////////////////////////////////////////////////
  // Common interface.
  /////////////////////////////////////////////////////////////////
 public:
  // The following functions are exposed for use in platform-shared code.

  inline void incrementInt32Value(const Address& addr);

  void move32(Imm32 imm, Register dest);
  void move32(Register src, Register dest);

  void movePtr(Register src, Register dest);
  void movePtr(ImmWord imm, Register dest);
  void movePtr(ImmPtr imm, Register dest);
  void movePtr(wasm::SymbolicAddress imm, Register dest);
  void movePtr(ImmGCPtr imm, Register dest);

  void load8SignExtend(const Address& address, Register dest);
  void load8SignExtend(const BaseIndex& src, Register dest);

  void load8ZeroExtend(const Address& address, Register dest);
  void load8ZeroExtend(const BaseIndex& src, Register dest);

  void load16SignExtend(const Address& address, Register dest);
  void load16SignExtend(const BaseIndex& src, Register dest);

  template <typename S>
  void load16UnalignedSignExtend(const S& src, Register dest) {
    load16SignExtend(src, dest);
  }

  void load16ZeroExtend(const Address& address, Register dest);
  void load16ZeroExtend(const BaseIndex& src, Register dest);

  template <typename S>
  void load16UnalignedZeroExtend(const S& src, Register dest) {
    load16ZeroExtend(src, dest);
  }

  void load32(const Address& address, Register dest);
  void load32(const BaseIndex& address, Register dest);
  void load32(AbsoluteAddress address, Register dest);
  void load32(wasm::SymbolicAddress address, Register dest);

  template <typename S>
  void load32Unaligned(const S& src, Register dest) {
    load32(src, dest);
  }

  void load64(const Address& address, Register64 dest) {
    loadPtr(address, dest.reg);
  }
  void load64(const BaseIndex& address, Register64 dest) {
    loadPtr(address, dest.reg);
  }

  template <typename S>
  void load64Unaligned(const S& src, Register64 dest) {
    load64(src, dest);
  }

  void loadPtr(const Address& address, Register dest);
  void loadPtr(const BaseIndex& src, Register dest);
  void loadPtr(AbsoluteAddress address, Register dest);
  void loadPtr(wasm::SymbolicAddress address, Register dest);

  void loadPrivate(const Address& address, Register dest);

  void store8(Register src, const Address& address);
  void store8(Imm32 imm, const Address& address);
  void store8(Register src, const BaseIndex& address);
  void store8(Imm32 imm, const BaseIndex& address);

  void store16(Register src, const Address& address);
  void store16(Imm32 imm, const Address& address);
  void store16(Register src, const BaseIndex& address);
  void store16(Imm32 imm, const BaseIndex& address);

  template <typename T>
  void store16Unaligned(Register src, const T& dest) {
    store16(src, dest);
  }

  void store32(Register src, AbsoluteAddress address);
  void store32(Register src, const Address& address);
  void store32(Register src, const BaseIndex& address);
  void store32(Imm32 src, const Address& address);
  void store32(Imm32 src, const BaseIndex& address);

  // NOTE: This will use second scratch on LOONG64. Only ARM needs the
  // implementation without second scratch.
  void store32_NoSecondScratch(Imm32 src, const Address& address) {
    store32(src, address);
  }

  template <typename T>
  void store32Unaligned(Register src, const T& dest) {
    store32(src, dest);
  }

  void store64(Imm64 imm, Address address) {
    storePtr(ImmWord(imm.value), address);
  }
  void store64(Imm64 imm, const BaseIndex& address) {
    storePtr(ImmWord(imm.value), address);
  }

  void store64(Register64 src, Address address) {
    storePtr(src.reg, address);
  }
  void store64(Register64 src, const BaseIndex& address) {
    storePtr(src.reg, address);
  }

  template <typename T>
  void store64Unaligned(Register64 src, const T& dest) {
    store64(src, dest);
  }

  template <typename T>
  void storePtr(ImmWord imm, T address);
  template <typename T>
  void storePtr(ImmPtr imm, T address);
  template <typename T>
  void storePtr(ImmGCPtr imm, T address);
  void storePtr(Register src, const Address& address);
  void storePtr(Register src, const BaseIndex& address);
  void storePtr(Register src, AbsoluteAddress dest);

  void moveDouble(FloatRegister src, FloatRegister dest) {
    as_fmov_d(dest, src);
  }

  void zeroDouble(FloatRegister reg) {
    moveToDouble(zero, reg);
  }

  void convertUInt64ToDouble(Register src, FloatRegister dest);

  void breakpoint(uint32_t value = 0);

  void checkStackAlignment() {
#ifdef DEBUG
    Label aligned;
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    as_andi(scratch, sp, ABIStackAlignment - 1);
    ma_b(scratch, zero, &aligned, Equal, ShortJump);
    breakpoint();
    bind(&aligned);
#endif
  };

  static void calculateAlignedStackPointer(void** stackPointer);

  void cmpPtrSet(Assembler::Condition cond,
                 Address lhs,
                 ImmPtr rhs,
                 Register dest);
  void cmpPtrSet(Assembler::Condition cond,
                 Register lhs,
                 Address rhs,
                 Register dest);
  void cmpPtrSet(Assembler::Condition cond,
                 Address lhs,
                 Register rhs,
                 Register dest);

  void cmp32Set(Assembler::Condition cond,
                Register lhs,
                Address rhs,
                Register dest);

 protected:
  bool buildOOLFakeExitFrame(void* fakeReturnAddr);

  void wasmLoadI64Impl(const wasm::MemoryAccessDesc& access,
                       Register memoryBase,
                       Register ptr,
                       Register ptrScratch,
                       Register64 output,
                       Register tmp);
  void wasmStoreI64Impl(const wasm::MemoryAccessDesc& access,
                        Register64 value,
                        Register memoryBase,
                        Register ptr,
                        Register ptrScratch,
                        Register tmp);

 public:
  void lea(Operand addr, Register dest) {
    ma_add_d(dest, addr.baseReg(), Imm32(addr.disp()));
  }

  void abiret() {
    as_jirl(zero, ra, BOffImm16(0));
  }

  void moveFloat32(FloatRegister src, FloatRegister dest) {
    as_fmov_s(dest, src);
  }

  // Instrumentation for entering and leaving the profiler.
  void profilerEnterFrame(Register framePtr, Register scratch);
  void profilerExitFrame();
};

typedef MacroAssemblerRiscv64Compat MacroAssemblerSpecific;

static inline bool GetTempRegForIntArg(uint32_t, uint32_t, Register*) {
  MOZ_CRASH();
}

}  // namespace jit
}  // namespace js

#endif /* jit_riscv64_MacroAssembler_riscv64_h */
