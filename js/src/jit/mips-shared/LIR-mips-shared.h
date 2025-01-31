/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_mips_shared_LIR_mips_shared_h
#define jit_mips_shared_LIR_mips_shared_h

namespace js {
namespace jit {

class LDivPowTwoI : public LInstructionHelper<1, 1, 1> {
  const int32_t shift_;

 public:
  LIR_HEADER(DivPowTwoI)

  LDivPowTwoI(const LAllocation& lhs, int32_t shift, const LDefinition& temp)
      : LInstructionHelper(classOpcode), shift_(shift) {
    setOperand(0, lhs);
    setTemp(0, temp);
  }

  const LAllocation* numerator() { return getOperand(0); }
  int32_t shift() const { return shift_; }
  MDiv* mir() const { return mir_->toDiv(); }
};

class LUDivOrMod : public LBinaryMath<0> {
 public:
  LIR_HEADER(UDivOrMod);

  LUDivOrMod() : LBinaryMath(classOpcode) {}

  MBinaryArithInstruction* mir() const {
    MOZ_ASSERT(mir_->isDiv() || mir_->isMod());
    return static_cast<MBinaryArithInstruction*>(mir_);
  }

  bool canBeDivideByZero() const {
    if (mir_->isMod()) {
      return mir_->toMod()->canBeDivideByZero();
    }
    return mir_->toDiv()->canBeDivideByZero();
  }

  bool trapOnError() const {
    if (mir_->isMod()) {
      return mir_->toMod()->trapOnError();
    }
    return mir_->toDiv()->trapOnError();
  }

  wasm::TrapSiteDesc trapSiteDesc() const {
    MOZ_ASSERT(mir_->isDiv() || mir_->isMod());
    if (mir_->isMod()) {
      return mir_->toMod()->trapSiteDesc();
    }
    return mir_->toDiv()->trapSiteDesc();
  }
};

namespace details {

// Base class for the int64 and non-int64 variants.
template <size_t NumDefs>
class LWasmUnalignedLoadBase : public details::LWasmLoadBase<NumDefs, 2> {
 public:
  typedef LWasmLoadBase<NumDefs, 2> Base;

  explicit LWasmUnalignedLoadBase(LNode::Opcode opcode, const LAllocation& ptr,
                                  const LAllocation& memoryBase,
                                  const LDefinition& valueHelper)
      : Base(opcode, ptr, memoryBase) {
    Base::setTemp(0, LDefinition::BogusTemp());
    Base::setTemp(1, valueHelper);
  }

  const LAllocation* ptr() { return Base::getOperand(0); }
  const LDefinition* ptrCopy() { return Base::getTemp(0); }
};

}  // namespace details

class LWasmUnalignedLoad : public details::LWasmUnalignedLoadBase<1> {
 public:
  LIR_HEADER(WasmUnalignedLoad);

  explicit LWasmUnalignedLoad(const LAllocation& ptr,
                              const LAllocation& memoryBase,
                              const LDefinition& valueHelper)
      : LWasmUnalignedLoadBase(classOpcode, ptr, memoryBase, valueHelper) {}
};

class LWasmUnalignedLoadI64
    : public details::LWasmUnalignedLoadBase<INT64_PIECES> {
 public:
  LIR_HEADER(WasmUnalignedLoadI64);

  explicit LWasmUnalignedLoadI64(const LAllocation& ptr,
                                 const LAllocation& memoryBase,
                                 const LDefinition& valueHelper)
      : LWasmUnalignedLoadBase(classOpcode, ptr, memoryBase, valueHelper) {}
};

namespace details {

// Base class for the int64 and non-int64 variants.
template <size_t NumOps>
class LWasmUnalignedStoreBase : public LInstructionHelper<0, NumOps, 2> {
 public:
  typedef LInstructionHelper<0, NumOps, 2> Base;

  static const size_t PtrIndex = 0;
  static const size_t ValueIndex = 1;

  LWasmUnalignedStoreBase(LNode::Opcode opcode, const LAllocation& ptr,
                          const LDefinition& valueHelper)
      : Base(opcode) {
    Base::setOperand(0, ptr);
    Base::setTemp(0, LDefinition::BogusTemp());
    Base::setTemp(1, valueHelper);
  }

  MWasmStore* mir() const { return Base::mir_->toWasmStore(); }
  const LAllocation* ptr() { return Base::getOperand(PtrIndex); }
  const LDefinition* ptrCopy() { return Base::getTemp(0); }
};

}  // namespace details

class LWasmUnalignedStore : public details::LWasmUnalignedStoreBase<3> {
 public:
  LIR_HEADER(WasmUnalignedStore);

  LWasmUnalignedStore(const LAllocation& ptr, const LAllocation& value,
                      const LAllocation& memoryBase,
                      const LDefinition& valueHelper)
      : LWasmUnalignedStoreBase(classOpcode, ptr, valueHelper) {
    setOperand(1, value);
    setOperand(2, memoryBase);
  }

  const LAllocation* value() { return Base::getOperand(ValueIndex); }
  const LAllocation* memoryBase() { return Base::getOperand(ValueIndex + 1); }
};

class LWasmUnalignedStoreI64
    : public details::LWasmUnalignedStoreBase<2 + INT64_PIECES> {
 public:
  LIR_HEADER(WasmUnalignedStoreI64);
  LWasmUnalignedStoreI64(const LAllocation& ptr, const LInt64Allocation& value,
                         const LAllocation& memoryBase,
                         const LDefinition& valueHelper)
      : LWasmUnalignedStoreBase(classOpcode, ptr, valueHelper) {
    setInt64Operand(1, value);
    setOperand(1 + INT64_PIECES, memoryBase);
  }

  LInt64Allocation value() { return getInt64Operand(ValueIndex); }
  const LAllocation* memoryBase() {
    return Base::getOperand(ValueIndex + INT64_PIECES);
  }
};

}  // namespace jit
}  // namespace js

#endif /* jit_mips_shared_LIR_mips_shared_h */
