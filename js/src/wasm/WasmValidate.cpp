/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Copyright 2016 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "wasm/WasmValidate.h"

#include "mozilla/CheckedInt.h"
#include "mozilla/Unused.h"
#include "mozilla/Utf8.h"

#include "builtin/TypedObject.h"
#include "jit/JitOptions.h"
#include "js/Printf.h"
#include "vm/JSContext.h"
#include "vm/Realm.h"
#include "wasm/WasmOpIter.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

using mozilla::CheckedInt;
using mozilla::CheckedInt32;
using mozilla::IsValidUtf8;
using mozilla::Unused;

// Decoder implementation.

bool Decoder::failf(const char* msg, ...) {
  va_list ap;
  va_start(ap, msg);
  UniqueChars str(JS_vsmprintf(msg, ap));
  va_end(ap);
  if (!str) {
    return false;
  }

  return fail(str.get());
}

void Decoder::warnf(const char* msg, ...) {
  if (!warnings_) {
    return;
  }

  va_list ap;
  va_start(ap, msg);
  UniqueChars str(JS_vsmprintf(msg, ap));
  va_end(ap);
  if (!str) {
    return;
  }

  Unused << warnings_->append(std::move(str));
}

bool Decoder::fail(size_t errorOffset, const char* msg) {
  MOZ_ASSERT(error_);
  UniqueChars strWithOffset(JS_smprintf("at offset %zu: %s", errorOffset, msg));
  if (!strWithOffset) {
    return false;
  }

  *error_ = std::move(strWithOffset);
  return false;
}

bool Decoder::readSectionHeader(uint8_t* id, SectionRange* range) {
  if (!readFixedU8(id)) {
    return false;
  }

  uint32_t size;
  if (!readVarU32(&size)) {
    return false;
  }

  range->start = currentOffset();
  range->size = size;
  return true;
}

bool Decoder::startSection(SectionId id, ModuleEnvironment* env,
                           MaybeSectionRange* range, const char* sectionName) {
  MOZ_ASSERT(!*range);

  // Record state at beginning of section to allow rewinding to this point
  // if, after skipping through several custom sections, we don't find the
  // section 'id'.
  const uint8_t* const initialCur = cur_;
  const size_t initialCustomSectionsLength = env->customSections.length();

  // Maintain a pointer to the current section that gets updated as custom
  // sections are skipped.
  const uint8_t* currentSectionStart = cur_;

  // Only start a section with 'id', skipping any custom sections before it.

  uint8_t idValue;
  if (!readFixedU8(&idValue)) {
    goto rewind;
  }

  while (idValue != uint8_t(id)) {
    if (idValue != uint8_t(SectionId::Custom)) {
      goto rewind;
    }

    // Rewind to the beginning of the current section since this is what
    // skipCustomSection() assumes.
    cur_ = currentSectionStart;
    if (!skipCustomSection(env)) {
      return false;
    }

    // Having successfully skipped a custom section, consider the next
    // section.
    currentSectionStart = cur_;
    if (!readFixedU8(&idValue)) {
      goto rewind;
    }
  }

  // Don't check the size since the range of bytes being decoded might not
  // contain the section body. (This is currently the case when streaming: the
  // code section header is decoded with the module environment bytes, the
  // body of the code section is streamed in separately.)

  uint32_t size;
  if (!readVarU32(&size)) {
    goto fail;
  }

  range->emplace();
  (*range)->start = currentOffset();
  (*range)->size = size;
  return true;

rewind:
  cur_ = initialCur;
  env->customSections.shrinkTo(initialCustomSectionsLength);
  return true;

fail:
  return failf("failed to start %s section", sectionName);
}

bool Decoder::finishSection(const SectionRange& range,
                            const char* sectionName) {
  if (resilientMode_) {
    return true;
  }
  if (range.size != currentOffset() - range.start) {
    return failf("byte size mismatch in %s section", sectionName);
  }
  return true;
}

bool Decoder::startCustomSection(const char* expected, size_t expectedLength,
                                 ModuleEnvironment* env,
                                 MaybeSectionRange* range) {
  // Record state at beginning of section to allow rewinding to this point
  // if, after skipping through several custom sections, we don't find the
  // section 'id'.
  const uint8_t* const initialCur = cur_;
  const size_t initialCustomSectionsLength = env->customSections.length();

  while (true) {
    // Try to start a custom section. If we can't, rewind to the beginning
    // since we may have skipped several custom sections already looking for
    // 'expected'.
    if (!startSection(SectionId::Custom, env, range, "custom")) {
      return false;
    }
    if (!*range) {
      goto rewind;
    }

    if (bytesRemain() < (*range)->size) {
      goto fail;
    }

    CustomSectionEnv sec;
    if (!readVarU32(&sec.nameLength) || sec.nameLength > bytesRemain()) {
      goto fail;
    }

    sec.nameOffset = currentOffset();
    sec.payloadOffset = sec.nameOffset + sec.nameLength;

    uint32_t payloadEnd = (*range)->start + (*range)->size;
    if (sec.payloadOffset > payloadEnd) {
      goto fail;
    }

    sec.payloadLength = payloadEnd - sec.payloadOffset;

    // Now that we have a valid custom section, record its offsets in the
    // metadata which can be queried by the user via Module.customSections.
    // Note: after an entry is appended, it may be popped if this loop or
    // the loop in startSection needs to rewind.
    if (!env->customSections.append(sec)) {
      return false;
    }

    // If this is the expected custom section, we're done.
    if (!expected || (expectedLength == sec.nameLength &&
                      !memcmp(cur_, expected, sec.nameLength))) {
      cur_ += sec.nameLength;
      return true;
    }

    // Otherwise, blindly skip the custom section and keep looking.
    skipAndFinishCustomSection(**range);
    range->reset();
  }
  MOZ_CRASH("unreachable");

rewind:
  cur_ = initialCur;
  env->customSections.shrinkTo(initialCustomSectionsLength);
  return true;

fail:
  return fail("failed to start custom section");
}

void Decoder::finishCustomSection(const char* name, const SectionRange& range) {
  MOZ_ASSERT(cur_ >= beg_);
  MOZ_ASSERT(cur_ <= end_);

  if (error_ && *error_) {
    warnf("in the '%s' custom section: %s", name, error_->get());
    skipAndFinishCustomSection(range);
    return;
  }

  uint32_t actualSize = currentOffset() - range.start;
  if (range.size != actualSize) {
    if (actualSize < range.size) {
      warnf("in the '%s' custom section: %" PRIu32 " unconsumed bytes", name,
            uint32_t(range.size - actualSize));
    } else {
      warnf("in the '%s' custom section: %" PRIu32
            " bytes consumed past the end",
            name, uint32_t(actualSize - range.size));
    }
    skipAndFinishCustomSection(range);
    return;
  }

  // Nothing to do! (c.f. skipAndFinishCustomSection())
}

void Decoder::skipAndFinishCustomSection(const SectionRange& range) {
  MOZ_ASSERT(cur_ >= beg_);
  MOZ_ASSERT(cur_ <= end_);
  cur_ = (beg_ + (range.start - offsetInModule_)) + range.size;
  MOZ_ASSERT(cur_ <= end_);
  clearError();
}

bool Decoder::skipCustomSection(ModuleEnvironment* env) {
  MaybeSectionRange range;
  if (!startCustomSection(nullptr, 0, env, &range)) {
    return false;
  }
  if (!range) {
    return fail("expected custom section");
  }

  skipAndFinishCustomSection(*range);
  return true;
}

bool Decoder::startNameSubsection(NameType nameType,
                                  Maybe<uint32_t>* endOffset) {
  MOZ_ASSERT(!*endOffset);

  const uint8_t* const initialPosition = cur_;

  uint8_t nameTypeValue;
  if (!readFixedU8(&nameTypeValue)) {
    goto rewind;
  }

  if (nameTypeValue != uint8_t(nameType)) {
    goto rewind;
  }

  uint32_t payloadLength;
  if (!readVarU32(&payloadLength) || payloadLength > bytesRemain()) {
    return fail("bad name subsection payload length");
  }

  *endOffset = Some(currentOffset() + payloadLength);
  return true;

rewind:
  cur_ = initialPosition;
  return true;
}

bool Decoder::finishNameSubsection(uint32_t expected) {
  uint32_t actual = currentOffset();
  if (expected != actual) {
    return failf("bad name subsection length (expected: %" PRIu32
                 ", actual: %" PRIu32 ")",
                 expected, actual);
  }

  return true;
}

bool Decoder::skipNameSubsection() {
  uint8_t nameTypeValue;
  if (!readFixedU8(&nameTypeValue)) {
    return fail("unable to read name subsection id");
  }

  switch (nameTypeValue) {
    case uint8_t(NameType::Module):
    case uint8_t(NameType::Function):
      return fail("out of order name subsections");
    default:
      break;
  }

  uint32_t payloadLength;
  if (!readVarU32(&payloadLength) || !readBytes(payloadLength)) {
    return fail("bad name subsection payload length");
  }

  return true;
}

// Misc helpers.

bool wasm::EncodeLocalEntries(Encoder& e, const ValTypeVector& locals) {
  if (locals.length() > MaxLocals) {
    return false;
  }

  uint32_t numLocalEntries = 0;
  ValType prev;
  for (ValType t : locals) {
    if (t != prev) {
      numLocalEntries++;
      prev = t;
    }
  }

  if (!e.writeVarU32(numLocalEntries)) {
    return false;
  }

  if (numLocalEntries) {
    prev = locals[0];
    uint32_t count = 1;
    for (uint32_t i = 1; i < locals.length(); i++, count++) {
      if (prev != locals[i]) {
        if (!e.writeVarU32(count)) {
          return false;
        }
        if (!e.writeValType(prev)) {
          return false;
        }
        prev = locals[i];
        count = 0;
      }
    }
    if (!e.writeVarU32(count)) {
      return false;
    }
    if (!e.writeValType(prev)) {
      return false;
    }
  }

  return true;
}

static bool DecodeValType(Decoder& d, ModuleKind kind, uint32_t numTypes,
                          HasGcTypes gcTypesEnabled, ValType* type) {
  uint8_t uncheckedCode;
  uint32_t uncheckedRefTypeIndex;
  if (!d.readValType(&uncheckedCode, &uncheckedRefTypeIndex)) {
    return false;
  }

  switch (uncheckedCode) {
    case uint8_t(ValType::I32):
    case uint8_t(ValType::F32):
    case uint8_t(ValType::F64):
    case uint8_t(ValType::I64):
      *type = ValType(ValType::Code(uncheckedCode));
      return true;
    case uint8_t(ValType::AnyRef):
      if (gcTypesEnabled == HasGcTypes::False) {
        return d.fail("reference types not enabled");
      }
      *type = ValType(ValType::Code(uncheckedCode));
      return true;
    case uint8_t(ValType::Ref): {
      if (gcTypesEnabled == HasGcTypes::False) {
        return d.fail("reference types not enabled");
      }
      if (uncheckedRefTypeIndex >= numTypes) {
        return d.fail("ref index out of range");
      }
      // We further validate ref types in the caller.
      *type = ValType(ValType::Code(uncheckedCode), uncheckedRefTypeIndex);
      return true;
    }
    default:
      break;
  }
  return d.fail("bad type");
}

static bool ValidateRefType(Decoder& d, const TypeDefVector& types,
                            ValType type) {
  if (type.isRef() && !types[type.refTypeIndex()].isStructType()) {
    return d.fail("ref does not reference a struct type");
  }
  return true;
}

bool wasm::DecodeLocalEntries(Decoder& d, ModuleKind kind,
                              const TypeDefVector& types,
                              HasGcTypes gcTypesEnabled,
                              ValTypeVector* locals) {
  uint32_t numLocalEntries;
  if (!d.readVarU32(&numLocalEntries)) {
    return d.fail("failed to read number of local entries");
  }

  for (uint32_t i = 0; i < numLocalEntries; i++) {
    uint32_t count;
    if (!d.readVarU32(&count)) {
      return d.fail("failed to read local entry count");
    }

    if (MaxLocals - locals->length() < count) {
      return d.fail("too many locals");
    }

    ValType type;
    if (!DecodeValType(d, kind, types.length(), gcTypesEnabled, &type)) {
      return false;
    }
    if (!ValidateRefType(d, types, type)) {
      return false;
    }

    if (!locals->appendN(type, count)) {
      return false;
    }
  }

  return true;
}

bool wasm::DecodeValidatedLocalEntries(Decoder& d, ValTypeVector* locals) {
  uint32_t numLocalEntries;
  MOZ_ALWAYS_TRUE(d.readVarU32(&numLocalEntries));

  for (uint32_t i = 0; i < numLocalEntries; i++) {
    uint32_t count;
    MOZ_ALWAYS_TRUE(d.readVarU32(&count));
    MOZ_ASSERT(MaxLocals - locals->length() >= count);

    uint8_t uncheckedCode;
    uint32_t uncheckedRefTypeIndex;
    MOZ_ALWAYS_TRUE(d.readValType(&uncheckedCode, &uncheckedRefTypeIndex));

    ValType type = ValType(ValType::Code(uncheckedCode), uncheckedRefTypeIndex);
    if (!locals->appendN(type, count)) {
      return false;
    }
  }

  return true;
}

// Function body validation.

struct ValidatingPolicy {
  typedef Nothing Value;
  typedef Nothing ControlItem;
};

typedef OpIter<ValidatingPolicy> ValidatingOpIter;

static bool DecodeFunctionBodyExprs(const ModuleEnvironment& env,
                                    const FuncType& funcType,
                                    const ValTypeVector& locals,
                                    ExclusiveDeferredValidationState& dvs,
                                    const uint8_t* bodyEnd, Decoder* d) {
  ValidatingOpIter iter(env, *d, dvs);

  if (!iter.readFunctionStart(funcType.ret())) {
    return false;
  }

#define CHECK(c)          \
  if (!(c)) return false; \
  break

  while (true) {
    OpBytes op;
    if (!iter.readOp(&op)) {
      return false;
    }

    Nothing nothing;

    switch (op.b0) {
      case uint16_t(Op::End): {
        LabelKind unusedKind;
        ExprType unusedType;
        if (!iter.readEnd(&unusedKind, &unusedType, &nothing)) {
          return false;
        }
        iter.popEnd();
        if (iter.controlStackEmpty()) {
          return iter.readFunctionEnd(bodyEnd);
        }
        break;
      }
      case uint16_t(Op::Nop):
        CHECK(iter.readNop());
      case uint16_t(Op::Drop):
        CHECK(iter.readDrop());
      case uint16_t(Op::Call): {
        uint32_t unusedIndex;
        ValidatingOpIter::ValueVector unusedArgs;
        CHECK(iter.readCall(&unusedIndex, &unusedArgs));
      }
      case uint16_t(Op::CallIndirect): {
        uint32_t unusedIndex, unusedIndex2;
        ValidatingOpIter::ValueVector unusedArgs;
        CHECK(iter.readCallIndirect(&unusedIndex, &unusedIndex2, &nothing,
                                    &unusedArgs));
      }
      case uint16_t(Op::I32Const): {
        int32_t unused;
        CHECK(iter.readI32Const(&unused));
      }
      case uint16_t(Op::I64Const): {
        int64_t unused;
        CHECK(iter.readI64Const(&unused));
      }
      case uint16_t(Op::F32Const): {
        float unused;
        CHECK(iter.readF32Const(&unused));
      }
      case uint16_t(Op::F64Const): {
        double unused;
        CHECK(iter.readF64Const(&unused));
      }
      case uint16_t(Op::GetLocal): {
        uint32_t unused;
        CHECK(iter.readGetLocal(locals, &unused));
      }
      case uint16_t(Op::SetLocal): {
        uint32_t unused;
        CHECK(iter.readSetLocal(locals, &unused, &nothing));
      }
      case uint16_t(Op::TeeLocal): {
        uint32_t unused;
        CHECK(iter.readTeeLocal(locals, &unused, &nothing));
      }
      case uint16_t(Op::GetGlobal): {
        uint32_t unused;
        CHECK(iter.readGetGlobal(&unused));
      }
      case uint16_t(Op::SetGlobal): {
        uint32_t unused;
        CHECK(iter.readSetGlobal(&unused, &nothing));
      }
      case uint16_t(Op::Select): {
        StackType unused;
        CHECK(iter.readSelect(&unused, &nothing, &nothing, &nothing));
      }
      case uint16_t(Op::Block):
        CHECK(iter.readBlock());
      case uint16_t(Op::Loop):
        CHECK(iter.readLoop());
      case uint16_t(Op::If):
        CHECK(iter.readIf(&nothing));
      case uint16_t(Op::Else): {
        ExprType type;
        CHECK(iter.readElse(&type, &nothing));
      }
      case uint16_t(Op::I32Clz):
      case uint16_t(Op::I32Ctz):
      case uint16_t(Op::I32Popcnt):
        CHECK(iter.readUnary(ValType::I32, &nothing));
      case uint16_t(Op::I64Clz):
      case uint16_t(Op::I64Ctz):
      case uint16_t(Op::I64Popcnt):
        CHECK(iter.readUnary(ValType::I64, &nothing));
      case uint16_t(Op::F32Abs):
      case uint16_t(Op::F32Neg):
      case uint16_t(Op::F32Ceil):
      case uint16_t(Op::F32Floor):
      case uint16_t(Op::F32Sqrt):
      case uint16_t(Op::F32Trunc):
      case uint16_t(Op::F32Nearest):
        CHECK(iter.readUnary(ValType::F32, &nothing));
      case uint16_t(Op::F64Abs):
      case uint16_t(Op::F64Neg):
      case uint16_t(Op::F64Ceil):
      case uint16_t(Op::F64Floor):
      case uint16_t(Op::F64Sqrt):
      case uint16_t(Op::F64Trunc):
      case uint16_t(Op::F64Nearest):
        CHECK(iter.readUnary(ValType::F64, &nothing));
      case uint16_t(Op::I32Add):
      case uint16_t(Op::I32Sub):
      case uint16_t(Op::I32Mul):
      case uint16_t(Op::I32DivS):
      case uint16_t(Op::I32DivU):
      case uint16_t(Op::I32RemS):
      case uint16_t(Op::I32RemU):
      case uint16_t(Op::I32And):
      case uint16_t(Op::I32Or):
      case uint16_t(Op::I32Xor):
      case uint16_t(Op::I32Shl):
      case uint16_t(Op::I32ShrS):
      case uint16_t(Op::I32ShrU):
      case uint16_t(Op::I32Rotl):
      case uint16_t(Op::I32Rotr):
        CHECK(iter.readBinary(ValType::I32, &nothing, &nothing));
      case uint16_t(Op::I64Add):
      case uint16_t(Op::I64Sub):
      case uint16_t(Op::I64Mul):
      case uint16_t(Op::I64DivS):
      case uint16_t(Op::I64DivU):
      case uint16_t(Op::I64RemS):
      case uint16_t(Op::I64RemU):
      case uint16_t(Op::I64And):
      case uint16_t(Op::I64Or):
      case uint16_t(Op::I64Xor):
      case uint16_t(Op::I64Shl):
      case uint16_t(Op::I64ShrS):
      case uint16_t(Op::I64ShrU):
      case uint16_t(Op::I64Rotl):
      case uint16_t(Op::I64Rotr):
        CHECK(iter.readBinary(ValType::I64, &nothing, &nothing));
      case uint16_t(Op::F32Add):
      case uint16_t(Op::F32Sub):
      case uint16_t(Op::F32Mul):
      case uint16_t(Op::F32Div):
      case uint16_t(Op::F32Min):
      case uint16_t(Op::F32Max):
      case uint16_t(Op::F32CopySign):
        CHECK(iter.readBinary(ValType::F32, &nothing, &nothing));
      case uint16_t(Op::F64Add):
      case uint16_t(Op::F64Sub):
      case uint16_t(Op::F64Mul):
      case uint16_t(Op::F64Div):
      case uint16_t(Op::F64Min):
      case uint16_t(Op::F64Max):
      case uint16_t(Op::F64CopySign):
        CHECK(iter.readBinary(ValType::F64, &nothing, &nothing));
      case uint16_t(Op::I32Eq):
      case uint16_t(Op::I32Ne):
      case uint16_t(Op::I32LtS):
      case uint16_t(Op::I32LtU):
      case uint16_t(Op::I32LeS):
      case uint16_t(Op::I32LeU):
      case uint16_t(Op::I32GtS):
      case uint16_t(Op::I32GtU):
      case uint16_t(Op::I32GeS):
      case uint16_t(Op::I32GeU):
        CHECK(iter.readComparison(ValType::I32, &nothing, &nothing));
      case uint16_t(Op::I64Eq):
      case uint16_t(Op::I64Ne):
      case uint16_t(Op::I64LtS):
      case uint16_t(Op::I64LtU):
      case uint16_t(Op::I64LeS):
      case uint16_t(Op::I64LeU):
      case uint16_t(Op::I64GtS):
      case uint16_t(Op::I64GtU):
      case uint16_t(Op::I64GeS):
      case uint16_t(Op::I64GeU):
        CHECK(iter.readComparison(ValType::I64, &nothing, &nothing));
      case uint16_t(Op::F32Eq):
      case uint16_t(Op::F32Ne):
      case uint16_t(Op::F32Lt):
      case uint16_t(Op::F32Le):
      case uint16_t(Op::F32Gt):
      case uint16_t(Op::F32Ge):
        CHECK(iter.readComparison(ValType::F32, &nothing, &nothing));
      case uint16_t(Op::F64Eq):
      case uint16_t(Op::F64Ne):
      case uint16_t(Op::F64Lt):
      case uint16_t(Op::F64Le):
      case uint16_t(Op::F64Gt):
      case uint16_t(Op::F64Ge):
        CHECK(iter.readComparison(ValType::F64, &nothing, &nothing));
      case uint16_t(Op::I32Eqz):
        CHECK(iter.readConversion(ValType::I32, ValType::I32, &nothing));
      case uint16_t(Op::I64Eqz):
      case uint16_t(Op::I32WrapI64):
        CHECK(iter.readConversion(ValType::I64, ValType::I32, &nothing));
      case uint16_t(Op::I32TruncSF32):
      case uint16_t(Op::I32TruncUF32):
      case uint16_t(Op::I32ReinterpretF32):
        CHECK(iter.readConversion(ValType::F32, ValType::I32, &nothing));
      case uint16_t(Op::I32TruncSF64):
      case uint16_t(Op::I32TruncUF64):
        CHECK(iter.readConversion(ValType::F64, ValType::I32, &nothing));
      case uint16_t(Op::I64ExtendSI32):
      case uint16_t(Op::I64ExtendUI32):
        CHECK(iter.readConversion(ValType::I32, ValType::I64, &nothing));
      case uint16_t(Op::I64TruncSF32):
      case uint16_t(Op::I64TruncUF32):
        CHECK(iter.readConversion(ValType::F32, ValType::I64, &nothing));
      case uint16_t(Op::I64TruncSF64):
      case uint16_t(Op::I64TruncUF64):
      case uint16_t(Op::I64ReinterpretF64):
        CHECK(iter.readConversion(ValType::F64, ValType::I64, &nothing));
      case uint16_t(Op::F32ConvertSI32):
      case uint16_t(Op::F32ConvertUI32):
      case uint16_t(Op::F32ReinterpretI32):
        CHECK(iter.readConversion(ValType::I32, ValType::F32, &nothing));
      case uint16_t(Op::F32ConvertSI64):
      case uint16_t(Op::F32ConvertUI64):
        CHECK(iter.readConversion(ValType::I64, ValType::F32, &nothing));
      case uint16_t(Op::F32DemoteF64):
        CHECK(iter.readConversion(ValType::F64, ValType::F32, &nothing));
      case uint16_t(Op::F64ConvertSI32):
      case uint16_t(Op::F64ConvertUI32):
        CHECK(iter.readConversion(ValType::I32, ValType::F64, &nothing));
      case uint16_t(Op::F64ConvertSI64):
      case uint16_t(Op::F64ConvertUI64):
      case uint16_t(Op::F64ReinterpretI64):
        CHECK(iter.readConversion(ValType::I64, ValType::F64, &nothing));
      case uint16_t(Op::F64PromoteF32):
        CHECK(iter.readConversion(ValType::F32, ValType::F64, &nothing));
      case uint16_t(Op::I32Extend8S):
      case uint16_t(Op::I32Extend16S):
        CHECK(iter.readConversion(ValType::I32, ValType::I32, &nothing));
      case uint16_t(Op::I64Extend8S):
      case uint16_t(Op::I64Extend16S):
      case uint16_t(Op::I64Extend32S):
        CHECK(iter.readConversion(ValType::I64, ValType::I64, &nothing));
      case uint16_t(Op::I32Load8S):
      case uint16_t(Op::I32Load8U): {
        LinearMemoryAddress<Nothing> addr;
        CHECK(iter.readLoad(ValType::I32, 1, &addr));
      }
      case uint16_t(Op::I32Load16S):
      case uint16_t(Op::I32Load16U): {
        LinearMemoryAddress<Nothing> addr;
        CHECK(iter.readLoad(ValType::I32, 2, &addr));
      }
      case uint16_t(Op::I32Load): {
        LinearMemoryAddress<Nothing> addr;
        CHECK(iter.readLoad(ValType::I32, 4, &addr));
      }
      case uint16_t(Op::I64Load8S):
      case uint16_t(Op::I64Load8U): {
        LinearMemoryAddress<Nothing> addr;
        CHECK(iter.readLoad(ValType::I64, 1, &addr));
      }
      case uint16_t(Op::I64Load16S):
      case uint16_t(Op::I64Load16U): {
        LinearMemoryAddress<Nothing> addr;
        CHECK(iter.readLoad(ValType::I64, 2, &addr));
      }
      case uint16_t(Op::I64Load32S):
      case uint16_t(Op::I64Load32U): {
        LinearMemoryAddress<Nothing> addr;
        CHECK(iter.readLoad(ValType::I64, 4, &addr));
      }
      case uint16_t(Op::I64Load): {
        LinearMemoryAddress<Nothing> addr;
        CHECK(iter.readLoad(ValType::I64, 8, &addr));
      }
      case uint16_t(Op::F32Load): {
        LinearMemoryAddress<Nothing> addr;
        CHECK(iter.readLoad(ValType::F32, 4, &addr));
      }
      case uint16_t(Op::F64Load): {
        LinearMemoryAddress<Nothing> addr;
        CHECK(iter.readLoad(ValType::F64, 8, &addr));
      }
      case uint16_t(Op::I32Store8): {
        LinearMemoryAddress<Nothing> addr;
        CHECK(iter.readStore(ValType::I32, 1, &addr, &nothing));
      }
      case uint16_t(Op::I32Store16): {
        LinearMemoryAddress<Nothing> addr;
        CHECK(iter.readStore(ValType::I32, 2, &addr, &nothing));
      }
      case uint16_t(Op::I32Store): {
        LinearMemoryAddress<Nothing> addr;
        CHECK(iter.readStore(ValType::I32, 4, &addr, &nothing));
      }
      case uint16_t(Op::I64Store8): {
        LinearMemoryAddress<Nothing> addr;
        CHECK(iter.readStore(ValType::I64, 1, &addr, &nothing));
      }
      case uint16_t(Op::I64Store16): {
        LinearMemoryAddress<Nothing> addr;
        CHECK(iter.readStore(ValType::I64, 2, &addr, &nothing));
      }
      case uint16_t(Op::I64Store32): {
        LinearMemoryAddress<Nothing> addr;
        CHECK(iter.readStore(ValType::I64, 4, &addr, &nothing));
      }
      case uint16_t(Op::I64Store): {
        LinearMemoryAddress<Nothing> addr;
        CHECK(iter.readStore(ValType::I64, 8, &addr, &nothing));
      }
      case uint16_t(Op::F32Store): {
        LinearMemoryAddress<Nothing> addr;
        CHECK(iter.readStore(ValType::F32, 4, &addr, &nothing));
      }
      case uint16_t(Op::F64Store): {
        LinearMemoryAddress<Nothing> addr;
        CHECK(iter.readStore(ValType::F64, 8, &addr, &nothing));
      }
      case uint16_t(Op::GrowMemory):
        CHECK(iter.readGrowMemory(&nothing));
      case uint16_t(Op::CurrentMemory):
        CHECK(iter.readCurrentMemory());
      case uint16_t(Op::Br): {
        uint32_t unusedDepth;
        ExprType unusedType;
        CHECK(iter.readBr(&unusedDepth, &unusedType, &nothing));
      }
      case uint16_t(Op::BrIf): {
        uint32_t unusedDepth;
        ExprType unusedType;
        CHECK(iter.readBrIf(&unusedDepth, &unusedType, &nothing, &nothing));
      }
      case uint16_t(Op::BrTable): {
        Uint32Vector unusedDepths;
        uint32_t unusedDefault;
        ExprType unusedType;
        CHECK(iter.readBrTable(&unusedDepths, &unusedDefault, &unusedType,
                               &nothing, &nothing));
      }
      case uint16_t(Op::Return):
        CHECK(iter.readReturn(&nothing));
      case uint16_t(Op::Unreachable):
        CHECK(iter.readUnreachable());
      case uint16_t(Op::MiscPrefix): {
        switch (op.b1) {
          case uint16_t(MiscOp::I32TruncSSatF32):
          case uint16_t(MiscOp::I32TruncUSatF32):
            CHECK(iter.readConversion(ValType::F32, ValType::I32, &nothing));
          case uint16_t(MiscOp::I32TruncSSatF64):
          case uint16_t(MiscOp::I32TruncUSatF64):
            CHECK(iter.readConversion(ValType::F64, ValType::I32, &nothing));
          case uint16_t(MiscOp::I64TruncSSatF32):
          case uint16_t(MiscOp::I64TruncUSatF32):
            CHECK(iter.readConversion(ValType::F32, ValType::I64, &nothing));
          case uint16_t(MiscOp::I64TruncSSatF64):
          case uint16_t(MiscOp::I64TruncUSatF64):
            CHECK(iter.readConversion(ValType::F64, ValType::I64, &nothing));
#ifdef ENABLE_WASM_BULKMEM_OPS
          case uint16_t(MiscOp::MemCopy): {
            uint32_t unusedDestMemIndex;
            uint32_t unusedSrcMemIndex;
            CHECK(iter.readMemOrTableCopy(/*isMem=*/true, &unusedDestMemIndex,
                                          &nothing, &unusedSrcMemIndex,
                                          &nothing, &nothing));
          }
          case uint16_t(MiscOp::MemDrop): {
            uint32_t unusedSegIndex;
            CHECK(iter.readMemOrTableDrop(/*isMem=*/true, &unusedSegIndex));
          }
          case uint16_t(MiscOp::MemFill):
            CHECK(iter.readMemFill(&nothing, &nothing, &nothing));
          case uint16_t(MiscOp::MemInit): {
            uint32_t unusedSegIndex;
            uint32_t unusedTableIndex;
            CHECK(iter.readMemOrTableInit(/*isMem=*/true, &unusedSegIndex,
                                          &unusedTableIndex, &nothing, &nothing,
                                          &nothing));
          }
          case uint16_t(MiscOp::TableCopy): {
            uint32_t unusedDestTableIndex;
            uint32_t unusedSrcTableIndex;
            CHECK(iter.readMemOrTableCopy(
                /*isMem=*/false, &unusedDestTableIndex, &nothing,
                &unusedSrcTableIndex, &nothing, &nothing));
          }
          case uint16_t(MiscOp::TableDrop): {
            uint32_t unusedSegIndex;
            CHECK(iter.readMemOrTableDrop(/*isMem=*/false, &unusedSegIndex));
          }
          case uint16_t(MiscOp::TableInit): {
            uint32_t unusedSegIndex;
            uint32_t unusedTableIndex;
            CHECK(iter.readMemOrTableInit(/*isMem=*/false, &unusedSegIndex,
                                          &unusedTableIndex, &nothing, &nothing,
                                          &nothing));
          }
#endif
#ifdef ENABLE_WASM_GENERALIZED_TABLES
          case uint16_t(MiscOp::TableGet): {
            uint32_t unusedTableIndex;
            CHECK(iter.readTableGet(&unusedTableIndex, &nothing));
          }
          case uint16_t(MiscOp::TableGrow): {
            uint32_t unusedTableIndex;
            CHECK(iter.readTableGrow(&unusedTableIndex, &nothing, &nothing));
          }
          case uint16_t(MiscOp::TableSet): {
            uint32_t unusedTableIndex;
            CHECK(iter.readTableSet(&unusedTableIndex, &nothing, &nothing));
          }
          case uint16_t(MiscOp::TableSize): {
            uint32_t unusedTableIndex;
            CHECK(iter.readTableSize(&unusedTableIndex));
          }
#endif
#ifdef ENABLE_WASM_GC
          case uint16_t(MiscOp::StructNew): {
            if (env.gcTypesEnabled() == HasGcTypes::False) {
              return iter.unrecognizedOpcode(&op);
            }
            uint32_t unusedUint;
            ValidatingOpIter::ValueVector unusedArgs;
            CHECK(iter.readStructNew(&unusedUint, &unusedArgs));
          }
          case uint16_t(MiscOp::StructGet): {
            if (env.gcTypesEnabled() == HasGcTypes::False) {
              return iter.unrecognizedOpcode(&op);
            }
            uint32_t unusedUint1, unusedUint2;
            CHECK(iter.readStructGet(&unusedUint1, &unusedUint2, &nothing));
          }
          case uint16_t(MiscOp::StructSet): {
            if (env.gcTypesEnabled() == HasGcTypes::False) {
              return iter.unrecognizedOpcode(&op);
            }
            uint32_t unusedUint1, unusedUint2;
            CHECK(iter.readStructSet(&unusedUint1, &unusedUint2, &nothing,
                                     &nothing));
          }
          case uint16_t(MiscOp::StructNarrow): {
            if (env.gcTypesEnabled() == HasGcTypes::False) {
              return iter.unrecognizedOpcode(&op);
            }
            ValType unusedTy, unusedTy2;
            CHECK(iter.readStructNarrow(&unusedTy, &unusedTy2, &nothing));
          }
#endif
          default:
            return iter.unrecognizedOpcode(&op);
        }
        break;
      }
#ifdef ENABLE_WASM_GC
      case uint16_t(Op::RefEq): {
        if (env.gcTypesEnabled() == HasGcTypes::False) {
          return iter.unrecognizedOpcode(&op);
        }
        CHECK(iter.readComparison(ValType::AnyRef, &nothing, &nothing));
        break;
      }
      case uint16_t(Op::RefNull): {
        if (env.gcTypesEnabled() == HasGcTypes::False) {
          return iter.unrecognizedOpcode(&op);
        }
        CHECK(iter.readRefNull());
        break;
      }
      case uint16_t(Op::RefIsNull): {
        if (env.gcTypesEnabled() == HasGcTypes::False) {
          return iter.unrecognizedOpcode(&op);
        }
        CHECK(iter.readConversion(ValType::AnyRef, ValType::I32, &nothing));
        break;
      }
#endif
      case uint16_t(Op::ThreadPrefix): {
        switch (op.b1) {
          case uint16_t(ThreadOp::Wake): {
            LinearMemoryAddress<Nothing> addr;
            CHECK(iter.readWake(&addr, &nothing));
          }
          case uint16_t(ThreadOp::I32Wait): {
            LinearMemoryAddress<Nothing> addr;
            CHECK(iter.readWait(&addr, ValType::I32, 4, &nothing, &nothing));
          }
          case uint16_t(ThreadOp::I64Wait): {
            LinearMemoryAddress<Nothing> addr;
            CHECK(iter.readWait(&addr, ValType::I64, 8, &nothing, &nothing));
          }
          case uint16_t(ThreadOp::I32AtomicLoad): {
            LinearMemoryAddress<Nothing> addr;
            CHECK(iter.readAtomicLoad(&addr, ValType::I32, 4));
          }
          case uint16_t(ThreadOp::I64AtomicLoad): {
            LinearMemoryAddress<Nothing> addr;
            CHECK(iter.readAtomicLoad(&addr, ValType::I64, 8));
          }
          case uint16_t(ThreadOp::I32AtomicLoad8U): {
            LinearMemoryAddress<Nothing> addr;
            CHECK(iter.readAtomicLoad(&addr, ValType::I32, 1));
          }
          case uint16_t(ThreadOp::I32AtomicLoad16U): {
            LinearMemoryAddress<Nothing> addr;
            CHECK(iter.readAtomicLoad(&addr, ValType::I32, 2));
          }
          case uint16_t(ThreadOp::I64AtomicLoad8U): {
            LinearMemoryAddress<Nothing> addr;
            CHECK(iter.readAtomicLoad(&addr, ValType::I64, 1));
          }
          case uint16_t(ThreadOp::I64AtomicLoad16U): {
            LinearMemoryAddress<Nothing> addr;
            CHECK(iter.readAtomicLoad(&addr, ValType::I64, 2));
          }
          case uint16_t(ThreadOp::I64AtomicLoad32U): {
            LinearMemoryAddress<Nothing> addr;
            CHECK(iter.readAtomicLoad(&addr, ValType::I64, 4));
          }
          case uint16_t(ThreadOp::I32AtomicStore): {
            LinearMemoryAddress<Nothing> addr;
            CHECK(iter.readAtomicStore(&addr, ValType::I32, 4, &nothing));
          }
          case uint16_t(ThreadOp::I64AtomicStore): {
            LinearMemoryAddress<Nothing> addr;
            CHECK(iter.readAtomicStore(&addr, ValType::I64, 8, &nothing));
          }
          case uint16_t(ThreadOp::I32AtomicStore8U): {
            LinearMemoryAddress<Nothing> addr;
            CHECK(iter.readAtomicStore(&addr, ValType::I32, 1, &nothing));
          }
          case uint16_t(ThreadOp::I32AtomicStore16U): {
            LinearMemoryAddress<Nothing> addr;
            CHECK(iter.readAtomicStore(&addr, ValType::I32, 2, &nothing));
          }
          case uint16_t(ThreadOp::I64AtomicStore8U): {
            LinearMemoryAddress<Nothing> addr;
            CHECK(iter.readAtomicStore(&addr, ValType::I64, 1, &nothing));
          }
          case uint16_t(ThreadOp::I64AtomicStore16U): {
            LinearMemoryAddress<Nothing> addr;
            CHECK(iter.readAtomicStore(&addr, ValType::I64, 2, &nothing));
          }
          case uint16_t(ThreadOp::I64AtomicStore32U): {
            LinearMemoryAddress<Nothing> addr;
            CHECK(iter.readAtomicStore(&addr, ValType::I64, 4, &nothing));
          }
          case uint16_t(ThreadOp::I32AtomicAdd):
          case uint16_t(ThreadOp::I32AtomicSub):
          case uint16_t(ThreadOp::I32AtomicAnd):
          case uint16_t(ThreadOp::I32AtomicOr):
          case uint16_t(ThreadOp::I32AtomicXor):
          case uint16_t(ThreadOp::I32AtomicXchg): {
            LinearMemoryAddress<Nothing> addr;
            CHECK(iter.readAtomicRMW(&addr, ValType::I32, 4, &nothing));
          }
          case uint16_t(ThreadOp::I64AtomicAdd):
          case uint16_t(ThreadOp::I64AtomicSub):
          case uint16_t(ThreadOp::I64AtomicAnd):
          case uint16_t(ThreadOp::I64AtomicOr):
          case uint16_t(ThreadOp::I64AtomicXor):
          case uint16_t(ThreadOp::I64AtomicXchg): {
            LinearMemoryAddress<Nothing> addr;
            CHECK(iter.readAtomicRMW(&addr, ValType::I64, 8, &nothing));
          }
          case uint16_t(ThreadOp::I32AtomicAdd8U):
          case uint16_t(ThreadOp::I32AtomicSub8U):
          case uint16_t(ThreadOp::I32AtomicAnd8U):
          case uint16_t(ThreadOp::I32AtomicOr8U):
          case uint16_t(ThreadOp::I32AtomicXor8U):
          case uint16_t(ThreadOp::I32AtomicXchg8U): {
            LinearMemoryAddress<Nothing> addr;
            CHECK(iter.readAtomicRMW(&addr, ValType::I32, 1, &nothing));
          }
          case uint16_t(ThreadOp::I32AtomicAdd16U):
          case uint16_t(ThreadOp::I32AtomicSub16U):
          case uint16_t(ThreadOp::I32AtomicAnd16U):
          case uint16_t(ThreadOp::I32AtomicOr16U):
          case uint16_t(ThreadOp::I32AtomicXor16U):
          case uint16_t(ThreadOp::I32AtomicXchg16U): {
            LinearMemoryAddress<Nothing> addr;
            CHECK(iter.readAtomicRMW(&addr, ValType::I32, 2, &nothing));
          }
          case uint16_t(ThreadOp::I64AtomicAdd8U):
          case uint16_t(ThreadOp::I64AtomicSub8U):
          case uint16_t(ThreadOp::I64AtomicAnd8U):
          case uint16_t(ThreadOp::I64AtomicOr8U):
          case uint16_t(ThreadOp::I64AtomicXor8U):
          case uint16_t(ThreadOp::I64AtomicXchg8U): {
            LinearMemoryAddress<Nothing> addr;
            CHECK(iter.readAtomicRMW(&addr, ValType::I64, 1, &nothing));
          }
          case uint16_t(ThreadOp::I64AtomicAdd16U):
          case uint16_t(ThreadOp::I64AtomicSub16U):
          case uint16_t(ThreadOp::I64AtomicAnd16U):
          case uint16_t(ThreadOp::I64AtomicOr16U):
          case uint16_t(ThreadOp::I64AtomicXor16U):
          case uint16_t(ThreadOp::I64AtomicXchg16U): {
            LinearMemoryAddress<Nothing> addr;
            CHECK(iter.readAtomicRMW(&addr, ValType::I64, 2, &nothing));
          }
          case uint16_t(ThreadOp::I64AtomicAdd32U):
          case uint16_t(ThreadOp::I64AtomicSub32U):
          case uint16_t(ThreadOp::I64AtomicAnd32U):
          case uint16_t(ThreadOp::I64AtomicOr32U):
          case uint16_t(ThreadOp::I64AtomicXor32U):
          case uint16_t(ThreadOp::I64AtomicXchg32U): {
            LinearMemoryAddress<Nothing> addr;
            CHECK(iter.readAtomicRMW(&addr, ValType::I64, 4, &nothing));
          }
          case uint16_t(ThreadOp::I32AtomicCmpXchg): {
            LinearMemoryAddress<Nothing> addr;
            CHECK(iter.readAtomicCmpXchg(&addr, ValType::I32, 4, &nothing,
                                         &nothing));
          }
          case uint16_t(ThreadOp::I64AtomicCmpXchg): {
            LinearMemoryAddress<Nothing> addr;
            CHECK(iter.readAtomicCmpXchg(&addr, ValType::I64, 8, &nothing,
                                         &nothing));
          }
          case uint16_t(ThreadOp::I32AtomicCmpXchg8U): {
            LinearMemoryAddress<Nothing> addr;
            CHECK(iter.readAtomicCmpXchg(&addr, ValType::I32, 1, &nothing,
                                         &nothing));
          }
          case uint16_t(ThreadOp::I32AtomicCmpXchg16U): {
            LinearMemoryAddress<Nothing> addr;
            CHECK(iter.readAtomicCmpXchg(&addr, ValType::I32, 2, &nothing,
                                         &nothing));
          }
          case uint16_t(ThreadOp::I64AtomicCmpXchg8U): {
            LinearMemoryAddress<Nothing> addr;
            CHECK(iter.readAtomicCmpXchg(&addr, ValType::I64, 1, &nothing,
                                         &nothing));
          }
          case uint16_t(ThreadOp::I64AtomicCmpXchg16U): {
            LinearMemoryAddress<Nothing> addr;
            CHECK(iter.readAtomicCmpXchg(&addr, ValType::I64, 2, &nothing,
                                         &nothing));
          }
          case uint16_t(ThreadOp::I64AtomicCmpXchg32U): {
            LinearMemoryAddress<Nothing> addr;
            CHECK(iter.readAtomicCmpXchg(&addr, ValType::I64, 4, &nothing,
                                         &nothing));
          }
          default:
            return iter.unrecognizedOpcode(&op);
        }
        break;
      }
      case uint16_t(Op::MozPrefix):
        return iter.unrecognizedOpcode(&op);
      default:
        return iter.unrecognizedOpcode(&op);
    }
  }

  MOZ_CRASH("unreachable");

#undef CHECK
}

bool wasm::ValidateFunctionBody(const ModuleEnvironment& env,
                                uint32_t funcIndex, uint32_t bodySize,
                                Decoder& d,
                                ExclusiveDeferredValidationState& dvs) {
  const FuncType& funcType = *env.funcTypes[funcIndex];

  ValTypeVector locals;
  if (!locals.appendAll(funcType.args())) {
    return false;
  }

  const uint8_t* bodyBegin = d.currentPosition();

  if (!DecodeLocalEntries(d, ModuleKind::Wasm, env.types, env.gcTypesEnabled(),
                          &locals)) {
    return false;
  }

  if (!DecodeFunctionBodyExprs(env, funcType, locals, dvs, bodyBegin + bodySize,
                               &d)) {
    return false;
  }

  return true;
}

// Section macros.

static bool DecodePreamble(Decoder& d) {
  if (d.bytesRemain() > MaxModuleBytes) {
    return d.fail("module too big");
  }

  uint32_t u32;
  if (!d.readFixedU32(&u32) || u32 != MagicNumber) {
    return d.fail("failed to match magic number");
  }

  if (!d.readFixedU32(&u32) || u32 != EncodingVersion) {
    return d.failf("binary version 0x%" PRIx32
                   " does not match expected version 0x%" PRIx32,
                   u32, EncodingVersion);
  }

  return true;
}

enum class TypeState { None, Struct, ForwardStruct, Func };

typedef Vector<TypeState, 0, SystemAllocPolicy> TypeStateVector;

static bool ValidateRefType(Decoder& d, TypeStateVector* typeState,
                            ValType type) {
  if (!type.isRef()) {
    return true;
  }

  uint32_t refTypeIndex = type.refTypeIndex();
  switch ((*typeState)[refTypeIndex]) {
    case TypeState::None:
      (*typeState)[refTypeIndex] = TypeState::ForwardStruct;
      break;
    case TypeState::Struct:
    case TypeState::ForwardStruct:
      break;
    case TypeState::Func:
      return d.fail("ref does not reference a struct type");
  }
  return true;
}

#ifdef WASM_PRIVATE_REFTYPES
static bool FuncTypeIsJSCompatible(Decoder& d, const FuncType& ft) {
  if (ft.exposesRef()) {
    return d.fail("cannot expose reference type");
  }
  return true;
}
#endif

static bool DecodeFuncType(Decoder& d, ModuleEnvironment* env,
                           TypeStateVector* typeState, uint32_t typeIndex) {
  uint32_t numArgs;
  if (!d.readVarU32(&numArgs)) {
    return d.fail("bad number of function args");
  }

  if (numArgs > MaxParams) {
    return d.fail("too many arguments in signature");
  }

  ValTypeVector args;
  if (!args.resize(numArgs)) {
    return false;
  }

  for (uint32_t i = 0; i < numArgs; i++) {
    if (!DecodeValType(d, ModuleKind::Wasm, env->types.length(),
                       env->gcTypesEnabled(), &args[i])) {
      return false;
    }
    if (!ValidateRefType(d, typeState, args[i])) {
      return false;
    }
  }

  uint32_t numRets;
  if (!d.readVarU32(&numRets)) {
    return d.fail("bad number of function returns");
  }

  if (numRets > 1) {
    return d.fail("too many returns in signature");
  }

  ExprType result = ExprType::Void;

  if (numRets == 1) {
    ValType type;
    if (!DecodeValType(d, ModuleKind::Wasm, env->types.length(),
                       env->gcTypesEnabled(), &type)) {
      return false;
    }
    if (!ValidateRefType(d, typeState, type)) {
      return false;
    }

    result = ExprType(type);
  }

  if ((*typeState)[typeIndex] != TypeState::None) {
    return d.fail("function type entry referenced as struct");
  }

  env->types[typeIndex] = TypeDef(FuncType(std::move(args), result));
  (*typeState)[typeIndex] = TypeState::Func;

  return true;
}

static bool DecodeStructType(Decoder& d, ModuleEnvironment* env,
                             TypeStateVector* typeState, uint32_t typeIndex) {
  if (env->gcTypesEnabled() == HasGcTypes::False) {
    return d.fail("Structure types not enabled");
  }

  uint32_t numFields;
  if (!d.readVarU32(&numFields)) {
    return d.fail("Bad number of fields");
  }

  if (numFields > MaxStructFields) {
    return d.fail("too many fields in structure");
  }

  StructFieldVector fields;
  if (!fields.resize(numFields)) {
    return false;
  }

  StructMetaTypeDescr::Layout layout;
  for (uint32_t i = 0; i < numFields; i++) {
    uint8_t flags;
    if (!d.readFixedU8(&flags)) {
      return d.fail("expected flag");
    }
    if ((flags & ~uint8_t(FieldFlags::AllowedMask)) != 0) {
      return d.fail("garbage flag bits");
    }
    fields[i].isMutable = flags & uint8_t(FieldFlags::Mutable);
    if (!DecodeValType(d, ModuleKind::Wasm, env->types.length(),
                       env->gcTypesEnabled(), &fields[i].type)) {
      return false;
    }
    if (!ValidateRefType(d, typeState, fields[i].type)) {
      return false;
    }

    CheckedInt32 offset;
    switch (fields[i].type.code()) {
      case ValType::I32:
        offset = layout.addScalar(Scalar::Int32);
        break;
      case ValType::I64:
        offset = layout.addScalar(Scalar::Int64);
        break;
      case ValType::F32:
        offset = layout.addScalar(Scalar::Float32);
        break;
      case ValType::F64:
        offset = layout.addScalar(Scalar::Float64);
        break;
      case ValType::Ref:
      case ValType::AnyRef:
        offset = layout.addReference(ReferenceType::TYPE_OBJECT);
        break;
      default:
        MOZ_CRASH("Unknown type");
    }
    if (!offset.isValid()) {
      return d.fail("Object too large");
    }

    fields[i].offset = offset.value();
  }

  CheckedInt32 totalSize = layout.close();
  if (!totalSize.isValid()) {
    return d.fail("Object too large");
  }

  bool isInline = InlineTypedObject::canAccommodateSize(totalSize.value());
  uint32_t offsetBy = isInline ? InlineTypedObject::offsetOfDataStart() : 0;

  for (StructField& f : fields) {
    f.offset += offsetBy;
  }

  if ((*typeState)[typeIndex] != TypeState::None &&
      (*typeState)[typeIndex] != TypeState::ForwardStruct) {
    return d.fail("struct type entry referenced as function");
  }

  env->types[typeIndex] =
      TypeDef(StructType(std::move(fields), env->numStructTypes, isInline));
  (*typeState)[typeIndex] = TypeState::Struct;
  env->numStructTypes++;

  return true;
}

#ifdef ENABLE_WASM_GC
static bool DecodeGCFeatureOptInSection(Decoder& d, ModuleEnvironment* env) {
  MaybeSectionRange range;
  if (!d.startSection(SectionId::GcFeatureOptIn, env, &range, "type")) {
    return false;
  }
  if (!range) {
    return true;
  }

  uint32_t version;
  if (!d.readVarU32(&version)) {
    return d.fail("expected gc feature version");
  }

  // For documentation of what's in the various versions, see
  // https://github.com/lars-t-hansen/moz-gc-experiments
  //
  // Version 1 is complete.
  // Version 2 is in progress.

  switch (version) {
    case 1:
      return d.fail(
          "Wasm GC feature version 1 is no longer supported by this engine.\n"
          "The current version is 2, which is not backward-compatible:\n"
          " - The old encoding of ref.null is no longer accepted.");
    case 2:
      break;
    default:
      return d.fail(
          "The specified Wasm GC feature version is unknown.\n"
          "The current version is 2.");
  }

  env->gcFeatureOptIn = HasGcTypes::True;
  return d.finishSection(*range, "gcfeatureoptin");
}
#endif

static bool DecodeTypeSection(Decoder& d, ModuleEnvironment* env) {
  MaybeSectionRange range;
  if (!d.startSection(SectionId::Type, env, &range, "type")) {
    return false;
  }
  if (!range) {
    return true;
  }

  uint32_t numTypes;
  if (!d.readVarU32(&numTypes)) {
    return d.fail("expected number of types");
  }

  if (numTypes > MaxTypes) {
    return d.fail("too many types");
  }

  if (!env->types.resize(numTypes)) {
    return false;
  }

  TypeStateVector typeState;
  if (!typeState.appendN(TypeState::None, numTypes)) {
    return false;
  }

  for (uint32_t typeIndex = 0; typeIndex < numTypes; typeIndex++) {
    uint8_t form;
    if (!d.readFixedU8(&form)) {
      return d.fail("expected type form");
    }

    switch (form) {
      case uint8_t(TypeCode::Func):
        if (!DecodeFuncType(d, env, &typeState, typeIndex)) {
          return false;
        }
        break;
      case uint8_t(TypeCode::Struct):
        if (!DecodeStructType(d, env, &typeState, typeIndex)) {
          return false;
        }
        break;
      default:
        return d.fail("expected type form");
    }
  }

  return d.finishSection(*range, "type");
}

static UniqueChars DecodeName(Decoder& d) {
  uint32_t numBytes;
  if (!d.readVarU32(&numBytes)) {
    return nullptr;
  }

  if (numBytes > MaxStringBytes) {
    return nullptr;
  }

  const uint8_t* bytes;
  if (!d.readBytes(numBytes, &bytes)) {
    return nullptr;
  }

  if (!IsValidUtf8(bytes, numBytes)) {
    return nullptr;
  }

  UniqueChars name(js_pod_malloc<char>(numBytes + 1));
  if (!name) {
    return nullptr;
  }

  memcpy(name.get(), bytes, numBytes);
  name[numBytes] = '\0';

  return name;
}

static bool DecodeSignatureIndex(Decoder& d, const TypeDefVector& types,
                                 uint32_t* funcTypeIndex) {
  if (!d.readVarU32(funcTypeIndex)) {
    return d.fail("expected signature index");
  }

  if (*funcTypeIndex >= types.length()) {
    return d.fail("signature index out of range");
  }

  if (!types[*funcTypeIndex].isFuncType()) {
    return d.fail("signature index references non-signature");
  }

  return true;
}

static bool DecodeLimits(Decoder& d, Limits* limits,
                         Shareable allowShared = Shareable::False) {
  uint8_t flags;
  if (!d.readFixedU8(&flags)) {
    return d.fail("expected flags");
  }

  uint8_t mask = allowShared == Shareable::True
                     ? uint8_t(MemoryMasks::AllowShared)
                     : uint8_t(MemoryMasks::AllowUnshared);

  if (flags & ~uint8_t(mask)) {
    return d.failf("unexpected bits set in flags: %" PRIu32,
                   (flags & ~uint8_t(mask)));
  }

  if (!d.readVarU32(&limits->initial)) {
    return d.fail("expected initial length");
  }

  if (flags & uint8_t(MemoryTableFlags::HasMaximum)) {
    uint32_t maximum;
    if (!d.readVarU32(&maximum)) {
      return d.fail("expected maximum length");
    }

    if (limits->initial > maximum) {
      return d.failf(
          "memory size minimum must not be greater than maximum; "
          "maximum length %" PRIu32 " is less than initial length %" PRIu32,
          maximum, limits->initial);
    }

    limits->maximum.emplace(maximum);
  }

  limits->shared = Shareable::False;

  if (allowShared == Shareable::True) {
    if ((flags & uint8_t(MemoryTableFlags::IsShared)) &&
        !(flags & uint8_t(MemoryTableFlags::HasMaximum))) {
      return d.fail("maximum length required for shared memory");
    }

    limits->shared = (flags & uint8_t(MemoryTableFlags::IsShared))
                         ? Shareable::True
                         : Shareable::False;
  }

  return true;
}

static bool DecodeTableTypeAndLimits(Decoder& d, HasGcTypes gcTypesEnabled,
                                     TableDescVector* tables) {
  uint8_t elementType;
  if (!d.readFixedU8(&elementType)) {
    return d.fail("expected table element type");
  }

  TableKind tableKind;
  if (elementType == uint8_t(TypeCode::AnyFunc)) {
    tableKind = TableKind::AnyFunction;
#ifdef ENABLE_WASM_GENERALIZED_TABLES
  } else if (elementType == uint8_t(TypeCode::AnyRef)) {
    if (gcTypesEnabled == HasGcTypes::False) {
      return d.fail("reference types not enabled");
    }
    tableKind = TableKind::AnyRef;
#endif
  } else {
#ifdef ENABLE_WASM_GENERALIZED_TABLES
    return d.fail("expected 'anyfunc' or 'anyref' element type");
#else
    return d.fail("expected 'anyfunc' element type");
#endif
  }

  Limits limits;
  if (!DecodeLimits(d, &limits)) {
    return false;
  }

  // If there's a maximum, check it is in range.  The check to exclude
  // initial > maximum is carried out by the DecodeLimits call above, so
  // we don't repeat it here.
  if (limits.initial > MaxTableInitialLength ||
      ((limits.maximum.isSome() &&
        limits.maximum.value() > MaxTableMaximumLength)))
    return d.fail("too many table elements");

  if (tables->length() >= MaxTables) {
    return d.fail("too many tables");
  }

  return tables->emplaceBack(tableKind, limits);
}

static bool GlobalIsJSCompatible(Decoder& d, ValType type, bool isMutable) {
  switch (type.code()) {
    case ValType::I32:
    case ValType::F32:
    case ValType::F64:
    case ValType::I64:
    case ValType::AnyRef:
      break;
#ifdef WASM_PRIVATE_REFTYPES
    case ValType::Ref:
      return d.fail("cannot expose reference type");
#endif
    default:
      return d.fail("unexpected variable type in global import/export");
  }

  return true;
}

static bool DecodeGlobalType(Decoder& d, const TypeDefVector& types,
                             HasGcTypes gcTypesEnabled, ValType* type,
                             bool* isMutable) {
  if (!DecodeValType(d, ModuleKind::Wasm, types.length(), gcTypesEnabled,
                     type)) {
    return false;
  }
  if (!ValidateRefType(d, types, *type)) {
    return false;
  }

  uint8_t flags;
  if (!d.readFixedU8(&flags)) {
    return d.fail("expected global flags");
  }

  if (flags & ~uint8_t(GlobalTypeImmediate::AllowedMask)) {
    return d.fail("unexpected bits set in global flags");
  }

  *isMutable = flags & uint8_t(GlobalTypeImmediate::IsMutable);
  return true;
}

void wasm::ConvertMemoryPagesToBytes(Limits* memory) {
  CheckedInt<uint32_t> initialBytes = memory->initial;
  initialBytes *= PageSize;

  static_assert(MaxMemoryInitialPages < UINT16_MAX,
                "multiplying by PageSize can't overflow");
  MOZ_ASSERT(initialBytes.isValid(), "can't overflow by above assertion");

  memory->initial = initialBytes.value();

  if (!memory->maximum) {
    return;
  }

  MOZ_ASSERT(*memory->maximum <= MaxMemoryMaximumPages);

  CheckedInt<uint32_t> maximumBytes = *memory->maximum;
  maximumBytes *= PageSize;

  // Clamp the maximum memory value to UINT32_MAX; it's not semantically
  // visible since growing will fail for values greater than INT32_MAX.
  memory->maximum =
      Some(maximumBytes.isValid() ? maximumBytes.value() : UINT32_MAX);

  MOZ_ASSERT(memory->initial <= *memory->maximum);
}

static bool DecodeMemoryLimits(Decoder& d, ModuleEnvironment* env) {
  if (env->usesMemory()) {
    return d.fail("already have default memory");
  }

  Limits memory;
  if (!DecodeLimits(d, &memory, Shareable::True)) {
    return false;
  }

  if (memory.initial > MaxMemoryInitialPages) {
    return d.fail("initial memory size too big");
  }

  if (memory.maximum && *memory.maximum > MaxMemoryMaximumPages) {
    return d.fail("maximum memory size too big");
  }

  ConvertMemoryPagesToBytes(&memory);

  if (memory.shared == Shareable::True &&
      env->sharedMemoryEnabled == Shareable::False) {
    return d.fail("shared memory is disabled");
  }

  env->memoryUsage = memory.shared == Shareable::True ? MemoryUsage::Shared
                                                      : MemoryUsage::Unshared;
  env->minMemoryLength = memory.initial;
  env->maxMemoryLength = memory.maximum;
  return true;
}

static bool DecodeImport(Decoder& d, ModuleEnvironment* env) {
  UniqueChars moduleName = DecodeName(d);
  if (!moduleName) {
    return d.fail("expected valid import module name");
  }

  UniqueChars funcName = DecodeName(d);
  if (!funcName) {
    return d.fail("expected valid import func name");
  }

  uint8_t rawImportKind;
  if (!d.readFixedU8(&rawImportKind)) {
    return d.fail("failed to read import kind");
  }

  DefinitionKind importKind = DefinitionKind(rawImportKind);

  switch (importKind) {
    case DefinitionKind::Function: {
      uint32_t funcTypeIndex;
      if (!DecodeSignatureIndex(d, env->types, &funcTypeIndex)) {
        return false;
      }
#ifdef WASM_PRIVATE_REFTYPES
      if (!FuncTypeIsJSCompatible(d, env->types[funcTypeIndex].funcType())) {
        return false;
      }
#endif
      if (!env->funcTypes.append(&env->types[funcTypeIndex].funcType())) {
        return false;
      }
      if (env->funcTypes.length() > MaxFuncs) {
        return d.fail("too many functions");
      }
      break;
    }
    case DefinitionKind::Table: {
      if (!DecodeTableTypeAndLimits(d, env->gcTypesEnabled(), &env->tables)) {
        return false;
      }
      env->tables.back().importedOrExported = true;
      break;
    }
    case DefinitionKind::Memory: {
      if (!DecodeMemoryLimits(d, env)) {
        return false;
      }
      break;
    }
    case DefinitionKind::Global: {
      ValType type;
      bool isMutable;
      if (!DecodeGlobalType(d, env->types, env->gcTypesEnabled(), &type,
                            &isMutable)) {
        return false;
      }
      if (!GlobalIsJSCompatible(d, type, isMutable)) {
        return false;
      }
      if (!env->globals.append(
              GlobalDesc(type, isMutable, env->globals.length()))) {
        return false;
      }
      if (env->globals.length() > MaxGlobals) {
        return d.fail("too many globals");
      }
      break;
    }
    default:
      return d.fail("unsupported import kind");
  }

  return env->imports.emplaceBack(std::move(moduleName), std::move(funcName),
                                  importKind);
}

static bool DecodeImportSection(Decoder& d, ModuleEnvironment* env) {
  MaybeSectionRange range;
  if (!d.startSection(SectionId::Import, env, &range, "import")) {
    return false;
  }
  if (!range) {
    return true;
  }

  uint32_t numImports;
  if (!d.readVarU32(&numImports)) {
    return d.fail("failed to read number of imports");
  }

  if (numImports > MaxImports) {
    return d.fail("too many imports");
  }

  for (uint32_t i = 0; i < numImports; i++) {
    if (!DecodeImport(d, env)) {
      return false;
    }
  }

  if (!d.finishSection(*range, "import")) {
    return false;
  }

  // The global data offsets will be filled in by ModuleGenerator::init.
  if (!env->funcImportGlobalDataOffsets.resize(env->funcTypes.length())) {
    return false;
  }

  return true;
}

static bool DecodeFunctionSection(Decoder& d, ModuleEnvironment* env) {
  MaybeSectionRange range;
  if (!d.startSection(SectionId::Function, env, &range, "function")) {
    return false;
  }
  if (!range) {
    return true;
  }

  uint32_t numDefs;
  if (!d.readVarU32(&numDefs)) {
    return d.fail("expected number of function definitions");
  }

  CheckedInt<uint32_t> numFuncs = env->funcTypes.length();
  numFuncs += numDefs;
  if (!numFuncs.isValid() || numFuncs.value() > MaxFuncs) {
    return d.fail("too many functions");
  }

  if (!env->funcTypes.reserve(numFuncs.value())) {
    return false;
  }

  for (uint32_t i = 0; i < numDefs; i++) {
    uint32_t funcTypeIndex;
    if (!DecodeSignatureIndex(d, env->types, &funcTypeIndex)) {
      return false;
    }
    env->funcTypes.infallibleAppend(&env->types[funcTypeIndex].funcType());
  }

  return d.finishSection(*range, "function");
}

static bool DecodeTableSection(Decoder& d, ModuleEnvironment* env) {
  MaybeSectionRange range;
  if (!d.startSection(SectionId::Table, env, &range, "table")) {
    return false;
  }
  if (!range) {
    return true;
  }

  uint32_t numTables;
  if (!d.readVarU32(&numTables)) {
    return d.fail("failed to read number of tables");
  }

  for (uint32_t i = 0; i < numTables; ++i) {
    if (!DecodeTableTypeAndLimits(d, env->gcTypesEnabled(), &env->tables)) {
      return false;
    }
  }

  return d.finishSection(*range, "table");
}

static bool DecodeMemorySection(Decoder& d, ModuleEnvironment* env) {
  MaybeSectionRange range;
  if (!d.startSection(SectionId::Memory, env, &range, "memory")) {
    return false;
  }
  if (!range) {
    return true;
  }

  uint32_t numMemories;
  if (!d.readVarU32(&numMemories)) {
    return d.fail("failed to read number of memories");
  }

  if (numMemories > 1) {
    return d.fail("the number of memories must be at most one");
  }

  for (uint32_t i = 0; i < numMemories; ++i) {
    if (!DecodeMemoryLimits(d, env)) {
      return false;
    }
  }

  return d.finishSection(*range, "memory");
}

static bool DecodeInitializerExpression(Decoder& d, ModuleEnvironment* env,
                                        ValType expected, InitExpr* init) {
  OpBytes op;
  if (!d.readOp(&op)) {
    return d.fail("failed to read initializer type");
  }

  switch (op.b0) {
    case uint16_t(Op::I32Const): {
      int32_t i32;
      if (!d.readVarS32(&i32)) {
        return d.fail("failed to read initializer i32 expression");
      }
      *init = InitExpr(LitVal(uint32_t(i32)));
      break;
    }
    case uint16_t(Op::I64Const): {
      int64_t i64;
      if (!d.readVarS64(&i64)) {
        return d.fail("failed to read initializer i64 expression");
      }
      *init = InitExpr(LitVal(uint64_t(i64)));
      break;
    }
    case uint16_t(Op::F32Const): {
      float f32;
      if (!d.readFixedF32(&f32)) {
        return d.fail("failed to read initializer f32 expression");
      }
      *init = InitExpr(LitVal(f32));
      break;
    }
    case uint16_t(Op::F64Const): {
      double f64;
      if (!d.readFixedF64(&f64)) {
        return d.fail("failed to read initializer f64 expression");
      }
      *init = InitExpr(LitVal(f64));
      break;
    }
    case uint16_t(Op::RefNull): {
      if (env->gcTypesEnabled() == HasGcTypes::False) {
        return d.fail("unexpected initializer expression");
      }
      if (!expected.isReference()) {
        return d.fail(
            "type mismatch: initializer type and expected type don't match");
      }
      *init = InitExpr(LitVal(expected, nullptr));
      break;
    }
    case uint16_t(Op::GetGlobal): {
      uint32_t i;
      const GlobalDescVector& globals = env->globals;
      if (!d.readVarU32(&i)) {
        return d.fail(
            "failed to read get_global index in initializer expression");
      }
      if (i >= globals.length()) {
        return d.fail("global index out of range in initializer expression");
      }
      if (!globals[i].isImport() || globals[i].isMutable()) {
        return d.fail(
            "initializer expression must reference a global immutable import");
      }
      if (expected.isReference()) {
        if (!(env->gcTypesEnabled() == HasGcTypes::True &&
              globals[i].type().isReference() &&
              env->isRefSubtypeOf(globals[i].type(), expected))) {
          return d.fail(
              "type mismatch: initializer type and expected type don't match");
        }
        *init = InitExpr(i, expected);
      } else {
        *init = InitExpr(i, globals[i].type());
      }
      break;
    }
    default: { return d.fail("unexpected initializer expression"); }
  }

  if (expected != init->type()) {
    return d.fail(
        "type mismatch: initializer type and expected type don't match");
  }

  OpBytes end;
  if (!d.readOp(&end) || end.b0 != uint16_t(Op::End)) {
    return d.fail("failed to read end of initializer expression");
  }

  return true;
}

static bool DecodeGlobalSection(Decoder& d, ModuleEnvironment* env) {
  MaybeSectionRange range;
  if (!d.startSection(SectionId::Global, env, &range, "global")) {
    return false;
  }
  if (!range) {
    return true;
  }

  uint32_t numDefs;
  if (!d.readVarU32(&numDefs)) {
    return d.fail("expected number of globals");
  }

  CheckedInt<uint32_t> numGlobals = env->globals.length();
  numGlobals += numDefs;
  if (!numGlobals.isValid() || numGlobals.value() > MaxGlobals) {
    return d.fail("too many globals");
  }

  if (!env->globals.reserve(numGlobals.value())) {
    return false;
  }

  for (uint32_t i = 0; i < numDefs; i++) {
    ValType type;
    bool isMutable;
    if (!DecodeGlobalType(d, env->types, env->gcTypesEnabled(), &type,
                          &isMutable)) {
      return false;
    }

    InitExpr initializer;
    if (!DecodeInitializerExpression(d, env, type, &initializer)) {
      return false;
    }

    env->globals.infallibleAppend(GlobalDesc(initializer, isMutable));
  }

  return d.finishSection(*range, "global");
}

typedef HashSet<const char*, mozilla::CStringHasher, SystemAllocPolicy>
    CStringSet;

static UniqueChars DecodeExportName(Decoder& d, CStringSet* dupSet) {
  UniqueChars exportName = DecodeName(d);
  if (!exportName) {
    d.fail("expected valid export name");
    return nullptr;
  }

  CStringSet::AddPtr p = dupSet->lookupForAdd(exportName.get());
  if (p) {
    d.fail("duplicate export");
    return nullptr;
  }

  if (!dupSet->add(p, exportName.get())) {
    return nullptr;
  }

  return exportName;
}

static bool DecodeExport(Decoder& d, ModuleEnvironment* env,
                         CStringSet* dupSet) {
  UniqueChars fieldName = DecodeExportName(d, dupSet);
  if (!fieldName) {
    return false;
  }

  uint8_t exportKind;
  if (!d.readFixedU8(&exportKind)) {
    return d.fail("failed to read export kind");
  }

  switch (DefinitionKind(exportKind)) {
    case DefinitionKind::Function: {
      uint32_t funcIndex;
      if (!d.readVarU32(&funcIndex)) {
        return d.fail("expected function index");
      }

      if (funcIndex >= env->numFuncs()) {
        return d.fail("exported function index out of bounds");
      }
#ifdef WASM_PRIVATE_REFTYPES
      if (!FuncTypeIsJSCompatible(d, *env->funcTypes[funcIndex])) {
        return false;
      }
#endif

      return env->exports.emplaceBack(std::move(fieldName), funcIndex,
                                      DefinitionKind::Function);
    }
    case DefinitionKind::Table: {
      uint32_t tableIndex;
      if (!d.readVarU32(&tableIndex)) {
        return d.fail("expected table index");
      }

      if (tableIndex >= env->tables.length()) {
        return d.fail("exported table index out of bounds");
      }
      env->tables[tableIndex].importedOrExported = true;
      return env->exports.emplaceBack(std::move(fieldName), tableIndex,
                                      DefinitionKind::Table);
    }
    case DefinitionKind::Memory: {
      uint32_t memoryIndex;
      if (!d.readVarU32(&memoryIndex)) {
        return d.fail("expected memory index");
      }

      if (memoryIndex > 0 || !env->usesMemory()) {
        return d.fail("exported memory index out of bounds");
      }

      return env->exports.emplaceBack(std::move(fieldName),
                                      DefinitionKind::Memory);
    }
    case DefinitionKind::Global: {
      uint32_t globalIndex;
      if (!d.readVarU32(&globalIndex)) {
        return d.fail("expected global index");
      }

      if (globalIndex >= env->globals.length()) {
        return d.fail("exported global index out of bounds");
      }

      GlobalDesc* global = &env->globals[globalIndex];
      global->setIsExport();
      if (!GlobalIsJSCompatible(d, global->type(), global->isMutable())) {
        return false;
      }

      return env->exports.emplaceBack(std::move(fieldName), globalIndex,
                                      DefinitionKind::Global);
    }
    default:
      return d.fail("unexpected export kind");
  }

  MOZ_CRASH("unreachable");
}

static bool DecodeExportSection(Decoder& d, ModuleEnvironment* env) {
  MaybeSectionRange range;
  if (!d.startSection(SectionId::Export, env, &range, "export")) {
    return false;
  }
  if (!range) {
    return true;
  }

  CStringSet dupSet;

  uint32_t numExports;
  if (!d.readVarU32(&numExports)) {
    return d.fail("failed to read number of exports");
  }

  if (numExports > MaxExports) {
    return d.fail("too many exports");
  }

  for (uint32_t i = 0; i < numExports; i++) {
    if (!DecodeExport(d, env, &dupSet)) {
      return false;
    }
  }

  return d.finishSection(*range, "export");
}

static bool DecodeStartSection(Decoder& d, ModuleEnvironment* env) {
  MaybeSectionRange range;
  if (!d.startSection(SectionId::Start, env, &range, "start")) {
    return false;
  }
  if (!range) {
    return true;
  }

  uint32_t funcIndex;
  if (!d.readVarU32(&funcIndex)) {
    return d.fail("failed to read start func index");
  }

  if (funcIndex >= env->numFuncs()) {
    return d.fail("unknown start function");
  }

  const FuncType& funcType = *env->funcTypes[funcIndex];
  if (!IsVoid(funcType.ret())) {
    return d.fail("start function must not return anything");
  }

  if (funcType.args().length()) {
    return d.fail("start function must be nullary");
  }

  env->startFuncIndex = Some(funcIndex);

  return d.finishSection(*range, "start");
}

static bool DecodeElemSection(Decoder& d, ModuleEnvironment* env) {
  MaybeSectionRange range;
  if (!d.startSection(SectionId::Elem, env, &range, "elem")) {
    return false;
  }
  if (!range) {
    return true;
  }

  uint32_t numSegments;
  if (!d.readVarU32(&numSegments)) {
    return d.fail("failed to read number of elem segments");
  }

  if (numSegments > MaxElemSegments) {
    return d.fail("too many elem segments");
  }

  if (!env->elemSegments.reserve(numSegments)) {
    return false;
  }

  for (uint32_t i = 0; i < numSegments; i++) {
    uint32_t initializerKindVal;
    if (!d.readVarU32(&initializerKindVal)) {
      return d.fail("expected elem initializer-kind field");
    }
    switch (initializerKindVal) {
      case uint32_t(InitializerKind::Active):
      case uint32_t(InitializerKind::Passive):
      case uint32_t(InitializerKind::ActiveWithIndex):
        break;
      default:
        return d.fail("invalid elem initializer-kind field");
    }

    InitializerKind initializerKind = InitializerKind(initializerKindVal);

    if (env->tables.length() == 0) {
      return d.fail("elem segment requires a table section");
    }

    MutableElemSegment seg = js_new<ElemSegment>();
    if (!seg) {
      return false;
    }

    uint32_t tableIndex = 0;
    if (initializerKind == InitializerKind::ActiveWithIndex) {
      if (!d.readVarU32(&tableIndex)) {
        return d.fail("expected table index");
      }
    }
    if (tableIndex >= env->tables.length()) {
      return d.fail("table index out of range for element segment");
    }
    if (initializerKind == InitializerKind::Passive) {
      // Too many bugs result from keeping this value zero.  For passive
      // segments, there really is no segment index, and we should never
      // touch the field.
      tableIndex = (uint32_t)-1;
    } else if (env->tables[tableIndex].kind != TableKind::AnyFunction) {
      return d.fail("only tables of 'anyfunc' may have element segments");
    }

    seg->tableIndex = tableIndex;

    if (initializerKind == InitializerKind::Active ||
        initializerKind == InitializerKind::ActiveWithIndex) {
      InitExpr offset;
      if (!DecodeInitializerExpression(d, env, ValType::I32, &offset)) {
        return false;
      }
      seg->offsetIfActive.emplace(offset);
    }

    uint32_t numElems;
    if (!d.readVarU32(&numElems)) {
      return d.fail("expected segment size");
    }

    if (numElems > MaxTableInitialLength) {
      return d.fail("too many table elements");
    }

    if (!seg->elemFuncIndices.reserve(numElems)) {
      return false;
    }

#ifdef WASM_PRIVATE_REFTYPES
    // We assume that passive segments may be applied to external tables.
    // We can do slightly better: if there are no external tables in the
    // module then we don't need to worry about passive segments either.
    // But this is a temporary restriction.
    bool exportedTable = initializerKind == InitializerKind::Passive ||
                         env->tables[tableIndex].importedOrExported;
#endif

    for (uint32_t i = 0; i < numElems; i++) {
      uint32_t funcIndex;
      if (!d.readVarU32(&funcIndex)) {
        return d.fail("failed to read element function index");
      }

      if (funcIndex >= env->numFuncs()) {
        return d.fail("table element out of range");
      }

#ifdef WASM_PRIVATE_REFTYPES
      if (exportedTable &&
          !FuncTypeIsJSCompatible(d, *env->funcTypes[funcIndex])) {
        return false;
      }
#endif

      seg->elemFuncIndices.infallibleAppend(funcIndex);
    }

    env->elemSegments.infallibleAppend(std::move(seg));
  }

  return d.finishSection(*range, "elem");
}

bool wasm::StartsCodeSection(const uint8_t* begin, const uint8_t* end,
                             SectionRange* codeSection) {
  UniqueChars unused;
  Decoder d(begin, end, 0, &unused);

  if (!DecodePreamble(d)) {
    return false;
  }

  while (!d.done()) {
    uint8_t id;
    SectionRange range;
    if (!d.readSectionHeader(&id, &range)) {
      return false;
    }

    if (id == uint8_t(SectionId::Code)) {
      *codeSection = range;
      return true;
    }

    if (!d.readBytes(range.size)) {
      return false;
    }
  }

  return false;
}

bool wasm::DecodeModuleEnvironment(Decoder& d, ModuleEnvironment* env) {
  if (!DecodePreamble(d)) {
    return false;
  }

#ifdef ENABLE_WASM_GC
  if (!DecodeGCFeatureOptInSection(d, env)) {
    return false;
  }
  HasGcTypes gcFeatureOptIn = env->gcFeatureOptIn;
#else
  HasGcTypes gcFeatureOptIn = HasGcTypes::False;
#endif

  env->compilerEnv->computeParameters(d, gcFeatureOptIn);

  if (!DecodeTypeSection(d, env)) {
    return false;
  }

  if (!DecodeImportSection(d, env)) {
    return false;
  }

  if (!DecodeFunctionSection(d, env)) {
    return false;
  }

  if (!DecodeTableSection(d, env)) {
    return false;
  }

  if (!DecodeMemorySection(d, env)) {
    return false;
  }

  if (!DecodeGlobalSection(d, env)) {
    return false;
  }

  if (!DecodeExportSection(d, env)) {
    return false;
  }

  if (!DecodeStartSection(d, env)) {
    return false;
  }

  if (!DecodeElemSection(d, env)) {
    return false;
  }

  if (!d.startSection(SectionId::Code, env, &env->codeSection, "code")) {
    return false;
  }

  if (env->codeSection && env->codeSection->size > MaxCodeSectionBytes) {
    return d.fail("code section too big");
  }

  return true;
}

static bool DecodeFunctionBody(Decoder& d, const ModuleEnvironment& env,
                               ExclusiveDeferredValidationState& dvs,
                               uint32_t funcIndex) {
  uint32_t bodySize;
  if (!d.readVarU32(&bodySize)) {
    return d.fail("expected number of function body bytes");
  }

  if (bodySize > MaxFunctionBytes) {
    return d.fail("function body too big");
  }

  if (d.bytesRemain() < bodySize) {
    return d.fail("function body length too big");
  }

  if (!ValidateFunctionBody(env, funcIndex, bodySize, d, dvs)) {
    return false;
  }

  return true;
}

static bool DecodeCodeSection(Decoder& d, ModuleEnvironment* env,
                              ExclusiveDeferredValidationState& dvs) {
  if (!env->codeSection) {
    if (env->numFuncDefs() != 0) {
      return d.fail("expected code section");
    }
    return true;
  }

  uint32_t numFuncDefs;
  if (!d.readVarU32(&numFuncDefs)) {
    return d.fail("expected function body count");
  }

  if (numFuncDefs != env->numFuncDefs()) {
    return d.fail(
        "function body count does not match function signature count");
  }

  for (uint32_t funcDefIndex = 0; funcDefIndex < numFuncDefs; funcDefIndex++) {
    if (!DecodeFunctionBody(d, *env, dvs,
                            env->numFuncImports() + funcDefIndex)) {
      return false;
    }
  }

  return d.finishSection(*env->codeSection, "code");
}

static bool DecodeDataSection(Decoder& d, ModuleEnvironment* env) {
  MaybeSectionRange range;
  if (!d.startSection(SectionId::Data, env, &range, "data")) {
    return false;
  }
  if (!range) {
    return true;
  }

  uint32_t numSegments;
  if (!d.readVarU32(&numSegments)) {
    return d.fail("failed to read number of data segments");
  }

  if (numSegments > MaxDataSegments) {
    return d.fail("too many data segments");
  }

  for (uint32_t i = 0; i < numSegments; i++) {
    uint32_t initializerKindVal;
    if (!d.readVarU32(&initializerKindVal)) {
      return d.fail("expected data initializer-kind field");
    }

    switch (initializerKindVal) {
      case uint32_t(InitializerKind::Active):
      case uint32_t(InitializerKind::Passive):
      case uint32_t(InitializerKind::ActiveWithIndex):
        break;
      default:
        return d.fail("invalid data initializer-kind field");
    }

    InitializerKind initializerKind = InitializerKind(initializerKindVal);

    if (!env->usesMemory()) {
      return d.fail("data segment requires a memory section");
    }

    uint32_t memIndex = 0;
    if (initializerKind == InitializerKind::ActiveWithIndex) {
      if (!d.readVarU32(&memIndex)) {
        return d.fail("expected memory index");
      }
      if (memIndex > 0) {
        return d.fail("memory index must be zero");
      }
    }

    DataSegmentEnv seg;
    if (initializerKind == InitializerKind::Active ||
        initializerKind == InitializerKind::ActiveWithIndex) {
      InitExpr segOffset;
      if (!DecodeInitializerExpression(d, env, ValType::I32, &segOffset)) {
        return false;
      }
      seg.offsetIfActive.emplace(segOffset);
    }

    if (!d.readVarU32(&seg.length)) {
      return d.fail("expected segment size");
    }

    if (seg.length > MaxMemoryInitialPages * PageSize) {
      return d.fail("segment size too big");
    }

    seg.bytecodeOffset = d.currentOffset();

    if (!d.readBytes(seg.length)) {
      return d.fail("data segment shorter than declared");
    }

    if (!env->dataSegments.append(seg)) {
      return false;
    }
  }

  return d.finishSection(*range, "data");
}

static bool DecodeModuleNameSubsection(Decoder& d,
                                       const CustomSectionEnv& nameSection,
                                       ModuleEnvironment* env) {
  Maybe<uint32_t> endOffset;
  if (!d.startNameSubsection(NameType::Module, &endOffset)) {
    return false;
  }
  if (!endOffset) {
    return true;
  }

  Name moduleName;
  if (!d.readVarU32(&moduleName.length)) {
    return d.fail("failed to read module name length");
  }

  MOZ_ASSERT(d.currentOffset() >= nameSection.payloadOffset);
  moduleName.offsetInNamePayload =
      d.currentOffset() - nameSection.payloadOffset;

  const uint8_t* bytes;
  if (!d.readBytes(moduleName.length, &bytes)) {
    return d.fail("failed to read module name bytes");
  }

  env->moduleName.emplace(moduleName);

  return d.finishNameSubsection(*endOffset);
}

static bool DecodeFunctionNameSubsection(Decoder& d,
                                         const CustomSectionEnv& nameSection,
                                         ModuleEnvironment* env) {
  Maybe<uint32_t> endOffset;
  if (!d.startNameSubsection(NameType::Function, &endOffset)) {
    return false;
  }
  if (!endOffset) {
    return true;
  }

  uint32_t nameCount = 0;
  if (!d.readVarU32(&nameCount) || nameCount > MaxFuncs) {
    return d.fail("bad function name count");
  }

  NameVector funcNames;

  for (uint32_t i = 0; i < nameCount; ++i) {
    uint32_t funcIndex = 0;
    if (!d.readVarU32(&funcIndex)) {
      return d.fail("unable to read function index");
    }

    // Names must refer to real functions and be given in ascending order.
    if (funcIndex >= env->numFuncs() || funcIndex < funcNames.length()) {
      return d.fail("invalid function index");
    }

    Name funcName;
    if (!d.readVarU32(&funcName.length) || funcName.length > MaxStringLength) {
      return d.fail("unable to read function name length");
    }

    if (!funcName.length) {
      continue;
    }

    if (!funcNames.resize(funcIndex + 1)) {
      return false;
    }

    MOZ_ASSERT(d.currentOffset() >= nameSection.payloadOffset);
    funcName.offsetInNamePayload =
        d.currentOffset() - nameSection.payloadOffset;

    if (!d.readBytes(funcName.length)) {
      return d.fail("unable to read function name bytes");
    }

    funcNames[funcIndex] = funcName;
  }

  if (!d.finishNameSubsection(*endOffset)) {
    return false;
  }

  // To encourage fully valid function names subsections; only save names if
  // the entire subsection decoded correctly.
  env->funcNames = std::move(funcNames);
  return true;
}

static bool DecodeNameSection(Decoder& d, ModuleEnvironment* env) {
  MaybeSectionRange range;
  if (!d.startCustomSection(NameSectionName, env, &range)) {
    return false;
  }
  if (!range) {
    return true;
  }

  env->nameCustomSectionIndex = Some(env->customSections.length() - 1);
  const CustomSectionEnv& nameSection = env->customSections.back();

  // Once started, custom sections do not report validation errors.

  if (!DecodeModuleNameSubsection(d, nameSection, env)) {
    goto finish;
  }

  if (!DecodeFunctionNameSubsection(d, nameSection, env)) {
    goto finish;
  }

  while (d.currentOffset() < range->end()) {
    if (!d.skipNameSubsection()) {
      goto finish;
    }
  }

finish:
  d.finishCustomSection(NameSectionName, *range);
  return true;
}

void DeferredValidationState::notifyDataSegmentIndex(uint32_t segIndex,
                                                     size_t offsetInModule) {
  // If |segIndex| is larger than any previously observed use, or this is
  // the first index use to be notified, make a note of it and the module
  // offset it appeared at.  That way, if we have to report it later as an
  // error, we can at least report a correct offset.
  if (!haveHighestDataSegIndex || segIndex > highestDataSegIndex) {
    highestDataSegIndex = segIndex;
    highestDataSegIndexOffset = offsetInModule;
  }

  haveHighestDataSegIndex = true;
}

bool DeferredValidationState::performDeferredValidation(
    const ModuleEnvironment& env, UniqueChars* error) {
  if (haveHighestDataSegIndex &&
      highestDataSegIndex >= env.dataSegments.length()) {
    UniqueChars str(
        JS_smprintf("at offset %zu: memory.{drop,init} index out of range",
                    highestDataSegIndexOffset));
    *error = std::move(str);
    return false;
  }

  return true;
}

bool wasm::DecodeModuleTail(Decoder& d, ModuleEnvironment* env,
                            ExclusiveDeferredValidationState& dvs) {
  if (!DecodeDataSection(d, env)) {
    return false;
  }

  if (!DecodeNameSection(d, env)) {
    return false;
  }

  while (!d.done()) {
    if (!d.skipCustomSection(env)) {
      if (d.resilientMode()) {
        d.clearError();
        return true;
      }
      return false;
    }
  }

  return dvs.lock()->performDeferredValidation(*env, d.error());
}

// Validate algorithm.

bool wasm::Validate(JSContext* cx, const ShareableBytes& bytecode,
                    UniqueChars* error) {
  Decoder d(bytecode.bytes, 0, error);

#ifdef ENABLE_WASM_GC
  HasGcTypes gcTypesConfigured =
      cx->options().wasmGc() ? HasGcTypes::True : HasGcTypes::False;
#else
  HasGcTypes gcTypesConfigured = HasGcTypes::False;
#endif

  CompilerEnvironment compilerEnv(CompileMode::Once, Tier::Optimized,
                                  OptimizedBackend::Ion, DebugEnabled::False,
                                  gcTypesConfigured);
  ModuleEnvironment env(
      gcTypesConfigured, &compilerEnv,
      cx->realm()->creationOptions().getSharedMemoryAndAtomicsEnabled()
          ? Shareable::True
          : Shareable::False);
  if (!DecodeModuleEnvironment(d, &env)) {
    return false;
  }

  ExclusiveDeferredValidationState dvs(mutexid::WasmDeferredValidation);

  if (!DecodeCodeSection(d, &env, dvs)) {
    return false;
  }

  if (!DecodeModuleTail(d, &env, dvs)) {
    return false;
  }

  MOZ_ASSERT(!*error, "unreported error in decoding");
  return true;
}
