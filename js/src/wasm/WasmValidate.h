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

#ifndef wasm_validate_h
#define wasm_validate_h

#include "mozilla/TypeTraits.h"

#include "wasm/WasmCode.h"
#include "wasm/WasmTypes.h"

namespace js {
namespace wasm {

// This struct captures the bytecode offset of a section's payload (so not
// including the header) and the size of the payload.

struct SectionRange {
  uint32_t start;
  uint32_t size;

  uint32_t end() const { return start + size; }
  bool operator==(const SectionRange& rhs) const {
    return start == rhs.start && size == rhs.size;
  }
};

typedef Maybe<SectionRange> MaybeSectionRange;

// CompilerEnvironment holds any values that will be needed to compute
// compilation parameters once the module's feature opt-in sections have been
// parsed.
//
// Subsequent to construction a computeParameters() call will compute the final
// compilation parameters, and the object can then be queried for their values.

struct CompileArgs;
class Decoder;

struct CompilerEnvironment {
  // The object starts in one of two "initial" states; computeParameters moves
  // it into the "computed" state.
  enum State { InitialWithArgs, InitialWithModeTierDebug, Computed };

  State state_;
  union {
    // Value if the state_ == InitialWithArgs.
    const CompileArgs* args_;

    // Value in the other two states.
    struct {
      CompileMode mode_;
      Tier tier_;
      OptimizedBackend optimizedBackend_;
      DebugEnabled debug_;
      HasGcTypes gcTypes_;
    };
  };

 public:
  // Retain a reference to the CompileArgs.  A subsequent computeParameters()
  // will compute all parameters from the CompileArgs and additional values.
  explicit CompilerEnvironment(const CompileArgs& args);

  // Save the provided values for mode, tier, and debug, and the initial value
  // for gcTypes.  A subsequent computeParameters() will compute the final
  // value of gcTypes.
  CompilerEnvironment(CompileMode mode, Tier tier,
                      OptimizedBackend optimizedBackend,
                      DebugEnabled debugEnabled, HasGcTypes gcTypesConfigured);

  // Compute any remaining compilation parameters.
  void computeParameters(Decoder& d, HasGcTypes gcFeatureOptIn);

  // Compute any remaining compilation parameters.  Only use this method if
  // the CompilerEnvironment was created with values for mode, tier, and
  // debug.
  void computeParameters(HasGcTypes gcFeatureOptIn);

  bool isComputed() const { return state_ == Computed; }
  CompileMode mode() const {
    MOZ_ASSERT(isComputed());
    return mode_;
  }
  Tier tier() const {
    MOZ_ASSERT(isComputed());
    return tier_;
  }
  OptimizedBackend optimizedBackend() const {
    MOZ_ASSERT(isComputed());
    return optimizedBackend_;
  }
  DebugEnabled debug() const {
    MOZ_ASSERT(isComputed());
    return debug_;
  }
  HasGcTypes gcTypes() const {
    MOZ_ASSERT(isComputed());
    return gcTypes_;
  }
};

// ModuleEnvironment contains all the state necessary to process or render
// functions, and all of the state necessary to validate aspects of the
// functions that do not require looking forwards in the bytecode stream.
// The remaining validation state is accumulated in DeferredValidationState
// and is checked at the end of a module's bytecode.
//
// A ModuleEnvironment is created by decoding all the sections before the wasm
// code section and then used immutably during. When compiling a module using a
// ModuleGenerator, the ModuleEnvironment holds state shared between the
// ModuleGenerator thread and background compile threads. All the threads
// are given a read-only view of the ModuleEnvironment, thus preventing race
// conditions.

struct ModuleEnvironment {
  // Constant parameters for the entire compilation:
  const ModuleKind kind;
  const Shareable sharedMemoryEnabled;
  // `gcTypesConfigured` reflects the value of the flags --wasm-gc and
  // javascript.options.wasm_gc.  These flags will disappear eventually, thus
  // allowing the removal of this variable and its replacement everywhere by
  // the value HasGcTypes::True.
  //
  // For now, the value is used to control whether we emit code to suppress GC
  // while wasm activations are on the stack.
  const HasGcTypes gcTypesConfigured;
  CompilerEnvironment* const compilerEnv;

  // Module fields decoded from the module environment (or initialized while
  // validating an asm.js module) and immutable during compilation:
#ifdef ENABLE_WASM_GC
  // `gcFeatureOptIn` reflects the presence in a module of a GcFeatureOptIn
  // section.  This variable will be removed eventually, allowing it to be
  // replaced everywhere by the value HasGcTypes::True.
  //
  // The flag is used in the value of gcTypesEnabled(), which controls whether
  // ref types and struct types and associated instructions are accepted
  // during validation.
  HasGcTypes gcFeatureOptIn;
#endif
  MemoryUsage memoryUsage;
  uint32_t minMemoryLength;
  Maybe<uint32_t> maxMemoryLength;
  uint32_t numStructTypes;
  TypeDefVector types;
  FuncTypeWithIdPtrVector funcTypes;
  Uint32Vector funcImportGlobalDataOffsets;
  GlobalDescVector globals;
  TableDescVector tables;
  Uint32Vector asmJSSigToTableIndex;
  ImportVector imports;
  ExportVector exports;
  Maybe<uint32_t> startFuncIndex;
  ElemSegmentVector elemSegments;
  MaybeSectionRange codeSection;

  // Fields decoded as part of the wasm module tail:
  DataSegmentEnvVector dataSegments;
  CustomSectionEnvVector customSections;
  Maybe<uint32_t> nameCustomSectionIndex;
  Maybe<Name> moduleName;
  NameVector funcNames;

  explicit ModuleEnvironment(HasGcTypes gcTypesConfigured,
                             CompilerEnvironment* compilerEnv,
                             Shareable sharedMemoryEnabled,
                             ModuleKind kind = ModuleKind::Wasm)
      : kind(kind),
        sharedMemoryEnabled(sharedMemoryEnabled),
        gcTypesConfigured(gcTypesConfigured),
        compilerEnv(compilerEnv),
#ifdef ENABLE_WASM_GC
        gcFeatureOptIn(HasGcTypes::False),
#endif
        memoryUsage(MemoryUsage::None),
        minMemoryLength(0),
        numStructTypes(0) {
  }

  Tier tier() const { return compilerEnv->tier(); }
  OptimizedBackend optimizedBackend() const {
    return compilerEnv->optimizedBackend();
  }
  CompileMode mode() const { return compilerEnv->mode(); }
  DebugEnabled debug() const { return compilerEnv->debug(); }
  size_t numTables() const { return tables.length(); }
  size_t numTypes() const { return types.length(); }
  size_t numFuncs() const { return funcTypes.length(); }
  size_t numFuncImports() const { return funcImportGlobalDataOffsets.length(); }
  size_t numFuncDefs() const {
    return funcTypes.length() - funcImportGlobalDataOffsets.length();
  }
  HasGcTypes gcTypesEnabled() const { return compilerEnv->gcTypes(); }
  bool usesMemory() const { return memoryUsage != MemoryUsage::None; }
  bool usesSharedMemory() const { return memoryUsage == MemoryUsage::Shared; }
  bool isAsmJS() const { return kind == ModuleKind::AsmJS; }
  bool debugEnabled() const {
    return compilerEnv->debug() == DebugEnabled::True;
  }
  bool funcIsImport(uint32_t funcIndex) const {
    return funcIndex < funcImportGlobalDataOffsets.length();
  }
  bool isRefSubtypeOf(ValType one, ValType two) const {
    MOZ_ASSERT(one.isReference());
    MOZ_ASSERT(two.isReference());
    MOZ_ASSERT(gcTypesEnabled() == HasGcTypes::True);
    return one == two || two == ValType::AnyRef || one == ValType::NullRef ||
           (one.isRef() && two.isRef() && isStructPrefixOf(two, one));
  }

 private:
  bool isStructPrefixOf(ValType a, ValType b) const {
    const StructType& other = types[a.refTypeIndex()].structType();
    return types[b.refTypeIndex()].structType().hasPrefix(other);
  }
};

// The Encoder class appends bytes to the Bytes object it is given during
// construction. The client is responsible for the Bytes's lifetime and must
// keep the Bytes alive as long as the Encoder is used.

class Encoder {
  Bytes& bytes_;

  template <class T>
  MOZ_MUST_USE bool write(const T& v) {
    return bytes_.append(reinterpret_cast<const uint8_t*>(&v), sizeof(T));
  }

  template <typename UInt>
  MOZ_MUST_USE bool writeVarU(UInt i) {
    do {
      uint8_t byte = i & 0x7f;
      i >>= 7;
      if (i != 0) {
        byte |= 0x80;
      }
      if (!bytes_.append(byte)) {
        return false;
      }
    } while (i != 0);
    return true;
  }

  template <typename SInt>
  MOZ_MUST_USE bool writeVarS(SInt i) {
    bool done;
    do {
      uint8_t byte = i & 0x7f;
      i >>= 7;
      done = ((i == 0) && !(byte & 0x40)) || ((i == -1) && (byte & 0x40));
      if (!done) {
        byte |= 0x80;
      }
      if (!bytes_.append(byte)) {
        return false;
      }
    } while (!done);
    return true;
  }

  void patchVarU32(size_t offset, uint32_t patchBits, uint32_t assertBits) {
    do {
      uint8_t assertByte = assertBits & 0x7f;
      uint8_t patchByte = patchBits & 0x7f;
      assertBits >>= 7;
      patchBits >>= 7;
      if (assertBits != 0) {
        assertByte |= 0x80;
        patchByte |= 0x80;
      }
      MOZ_ASSERT(assertByte == bytes_[offset]);
      bytes_[offset] = patchByte;
      offset++;
    } while (assertBits != 0);
  }

  void patchFixedU7(size_t offset, uint8_t patchBits, uint8_t assertBits) {
    MOZ_ASSERT(patchBits <= uint8_t(INT8_MAX));
    patchFixedU8(offset, patchBits, assertBits);
  }

  void patchFixedU8(size_t offset, uint8_t patchBits, uint8_t assertBits) {
    MOZ_ASSERT(bytes_[offset] == assertBits);
    bytes_[offset] = patchBits;
  }

  uint32_t varU32ByteLength(size_t offset) const {
    size_t start = offset;
    while (bytes_[offset] & 0x80) {
      offset++;
    }
    return offset - start + 1;
  }

 public:
  explicit Encoder(Bytes& bytes) : bytes_(bytes) { MOZ_ASSERT(empty()); }

  size_t currentOffset() const { return bytes_.length(); }
  bool empty() const { return currentOffset() == 0; }

  // Fixed-size encoding operations simply copy the literal bytes (without
  // attempting to align).

  MOZ_MUST_USE bool writeFixedU7(uint8_t i) {
    MOZ_ASSERT(i <= uint8_t(INT8_MAX));
    return writeFixedU8(i);
  }
  MOZ_MUST_USE bool writeFixedU8(uint8_t i) { return write<uint8_t>(i); }
  MOZ_MUST_USE bool writeFixedU32(uint32_t i) { return write<uint32_t>(i); }
  MOZ_MUST_USE bool writeFixedF32(float f) { return write<float>(f); }
  MOZ_MUST_USE bool writeFixedF64(double d) { return write<double>(d); }

  // Variable-length encodings that all use LEB128.

  MOZ_MUST_USE bool writeVarU32(uint32_t i) { return writeVarU<uint32_t>(i); }
  MOZ_MUST_USE bool writeVarS32(int32_t i) { return writeVarS<int32_t>(i); }
  MOZ_MUST_USE bool writeVarU64(uint64_t i) { return writeVarU<uint64_t>(i); }
  MOZ_MUST_USE bool writeVarS64(int64_t i) { return writeVarS<int64_t>(i); }
  MOZ_MUST_USE bool writeValType(ValType type) {
    static_assert(size_t(TypeCode::Limit) <= UINT8_MAX, "fits");
    MOZ_ASSERT(size_t(type.code()) < size_t(TypeCode::Limit));
    if (type.isRef()) {
      return writeFixedU8(uint8_t(TypeCode::Ref)) &&
             writeVarU32(type.refTypeIndex());
    }
    return writeFixedU8(uint8_t(type.code()));
  }
  MOZ_MUST_USE bool writeBlockType(ExprType type) {
    static_assert(size_t(TypeCode::Limit) <= UINT8_MAX, "fits");
    MOZ_ASSERT(size_t(type.code()) < size_t(TypeCode::Limit));
    if (type.isRef()) {
      return writeFixedU8(uint8_t(ExprType::Ref)) &&
             writeVarU32(type.refTypeIndex());
    }
    return writeFixedU8(uint8_t(type.code()));
  }
  MOZ_MUST_USE bool writeOp(Op op) {
    static_assert(size_t(Op::Limit) == 256, "fits");
    MOZ_ASSERT(size_t(op) < size_t(Op::Limit));
    return writeFixedU8(uint8_t(op));
  }
  MOZ_MUST_USE bool writeOp(MiscOp op) {
    static_assert(size_t(MiscOp::Limit) <= 256, "fits");
    MOZ_ASSERT(size_t(op) < size_t(MiscOp::Limit));
    return writeFixedU8(uint8_t(Op::MiscPrefix)) && writeFixedU8(uint8_t(op));
  }
  MOZ_MUST_USE bool writeOp(ThreadOp op) {
    static_assert(size_t(ThreadOp::Limit) <= 256, "fits");
    MOZ_ASSERT(size_t(op) < size_t(ThreadOp::Limit));
    return writeFixedU8(uint8_t(Op::ThreadPrefix)) && writeFixedU8(uint8_t(op));
  }
  MOZ_MUST_USE bool writeOp(MozOp op) {
    static_assert(size_t(MozOp::Limit) <= 256, "fits");
    MOZ_ASSERT(size_t(op) < size_t(MozOp::Limit));
    return writeFixedU8(uint8_t(Op::MozPrefix)) && writeFixedU8(uint8_t(op));
  }

  // Fixed-length encodings that allow back-patching.

  MOZ_MUST_USE bool writePatchableFixedU7(size_t* offset) {
    *offset = bytes_.length();
    return writeFixedU8(UINT8_MAX);
  }
  void patchFixedU7(size_t offset, uint8_t patchBits) {
    return patchFixedU7(offset, patchBits, UINT8_MAX);
  }

  // Variable-length encodings that allow back-patching.

  MOZ_MUST_USE bool writePatchableVarU32(size_t* offset) {
    *offset = bytes_.length();
    return writeVarU32(UINT32_MAX);
  }
  void patchVarU32(size_t offset, uint32_t patchBits) {
    return patchVarU32(offset, patchBits, UINT32_MAX);
  }

  // Byte ranges start with an LEB128 length followed by an arbitrary sequence
  // of bytes. When used for strings, bytes are to be interpreted as utf8.

  MOZ_MUST_USE bool writeBytes(const void* bytes, uint32_t numBytes) {
    return writeVarU32(numBytes) &&
           bytes_.append(reinterpret_cast<const uint8_t*>(bytes), numBytes);
  }

  // A "section" is a contiguous range of bytes that stores its own size so
  // that it may be trivially skipped without examining the payload. Sections
  // require backpatching since the size of the section is only known at the
  // end while the size's varU32 must be stored at the beginning. Immediately
  // after the section length is the string id of the section.

  MOZ_MUST_USE bool startSection(SectionId id, size_t* offset) {
    MOZ_ASSERT(uint32_t(id) < 128);
    return writeVarU32(uint32_t(id)) && writePatchableVarU32(offset);
  }
  void finishSection(size_t offset) {
    return patchVarU32(offset,
                       bytes_.length() - offset - varU32ByteLength(offset));
  }
};

// DeferredValidationState holds mutable state shared between threads that
// compile a module.  The state accumulates information needed to complete
// validation at the end of compilation of a module.

struct DeferredValidationState {
  // These three fields keep track of the highest data segment index
  // mentioned in the code section, if any, and the associated section
  // offset, so as to facilitate error message creation.  The use of
  // |haveHighestDataSegIndex| avoids the difficulty of having to
  // special-case one of the |highestDataSegIndex| values to mean "we
  // haven't seen any data segments (yet)."

  bool haveHighestDataSegIndex;
  uint32_t highestDataSegIndex;
  size_t highestDataSegIndexOffset;

  DeferredValidationState() { init(); }

  void init() {
    haveHighestDataSegIndex = false;
    highestDataSegIndex = 0;
    highestDataSegIndexOffset = 0;
  }

  // Call here to notify the use of the data segment index with value
  // |segIndex| at module offset |offsetInModule| whilst iterating through
  // the code segment.
  void notifyDataSegmentIndex(uint32_t segIndex, size_t offsetInModule);

  // Call here to perform all final validation actions once the module tail
  // has been processed.  Returns |true| if there are no errors.
  bool performDeferredValidation(const ModuleEnvironment& env,
                                 UniqueChars* error);
};

typedef ExclusiveData<DeferredValidationState> ExclusiveDeferredValidationState;

// The Decoder class decodes the bytes in the range it is given during
// construction. The client is responsible for keeping the byte range alive as
// long as the Decoder is used.

class Decoder {
  const uint8_t* const beg_;
  const uint8_t* const end_;
  const uint8_t* cur_;
  const size_t offsetInModule_;
  UniqueChars* error_;
  UniqueCharsVector* warnings_;
  bool resilientMode_;

  template <class T>
  MOZ_MUST_USE bool read(T* out) {
    if (bytesRemain() < sizeof(T)) {
      return false;
    }
    memcpy((void*)out, cur_, sizeof(T));
    cur_ += sizeof(T);
    return true;
  }

  template <class T>
  T uncheckedRead() {
    MOZ_ASSERT(bytesRemain() >= sizeof(T));
    T ret;
    memcpy(&ret, cur_, sizeof(T));
    cur_ += sizeof(T);
    return ret;
  }

  template <class T>
  void uncheckedRead(T* ret) {
    MOZ_ASSERT(bytesRemain() >= sizeof(T));
    memcpy(ret, cur_, sizeof(T));
    cur_ += sizeof(T);
  }

  template <typename UInt>
  MOZ_MUST_USE bool readVarU(UInt* out) {
    DebugOnly<const uint8_t*> before = cur_;
    const unsigned numBits = sizeof(UInt) * CHAR_BIT;
    const unsigned remainderBits = numBits % 7;
    const unsigned numBitsInSevens = numBits - remainderBits;
    UInt u = 0;
    uint8_t byte;
    UInt shift = 0;
    do {
      if (!readFixedU8(&byte)) {
        return false;
      }
      if (!(byte & 0x80)) {
        *out = u | UInt(byte) << shift;
        return true;
      }
      u |= UInt(byte & 0x7F) << shift;
      shift += 7;
    } while (shift != numBitsInSevens);
    if (!readFixedU8(&byte) || (byte & (unsigned(-1) << remainderBits))) {
      return false;
    }
    *out = u | (UInt(byte) << numBitsInSevens);
    MOZ_ASSERT_IF(sizeof(UInt) == 4,
                  unsigned(cur_ - before) <= MaxVarU32DecodedBytes);
    return true;
  }

  template <typename SInt>
  MOZ_MUST_USE bool readVarS(SInt* out) {
    using UInt = typename mozilla::MakeUnsigned<SInt>::Type;
    const unsigned numBits = sizeof(SInt) * CHAR_BIT;
    const unsigned remainderBits = numBits % 7;
    const unsigned numBitsInSevens = numBits - remainderBits;
    SInt s = 0;
    uint8_t byte;
    unsigned shift = 0;
    do {
      if (!readFixedU8(&byte)) {
        return false;
      }
      s |= SInt(byte & 0x7f) << shift;
      shift += 7;
      if (!(byte & 0x80)) {
        if (byte & 0x40) {
          s |= UInt(-1) << shift;
        }
        *out = s;
        return true;
      }
    } while (shift < numBitsInSevens);
    if (!remainderBits || !readFixedU8(&byte) || (byte & 0x80)) {
      return false;
    }
    uint8_t mask = 0x7f & (uint8_t(-1) << remainderBits);
    if ((byte & mask) != ((byte & (1 << (remainderBits - 1))) ? mask : 0)) {
      return false;
    }
    *out = s | UInt(byte) << shift;
    return true;
  }

 public:
  Decoder(const uint8_t* begin, const uint8_t* end, size_t offsetInModule,
          UniqueChars* error, UniqueCharsVector* warnings = nullptr,
          bool resilientMode = false)
      : beg_(begin),
        end_(end),
        cur_(begin),
        offsetInModule_(offsetInModule),
        error_(error),
        warnings_(warnings),
        resilientMode_(resilientMode) {
    MOZ_ASSERT(begin <= end);
  }
  explicit Decoder(const Bytes& bytes, size_t offsetInModule = 0,
                   UniqueChars* error = nullptr,
                   UniqueCharsVector* warnings = nullptr)
      : beg_(bytes.begin()),
        end_(bytes.end()),
        cur_(bytes.begin()),
        offsetInModule_(offsetInModule),
        error_(error),
        warnings_(warnings),
        resilientMode_(false) {}

  // These convenience functions use currentOffset() as the errorOffset.
  bool fail(const char* msg) { return fail(currentOffset(), msg); }
  bool failf(const char* msg, ...) MOZ_FORMAT_PRINTF(2, 3);
  void warnf(const char* msg, ...) MOZ_FORMAT_PRINTF(2, 3);

  // Report an error at the given offset (relative to the whole module).
  bool fail(size_t errorOffset, const char* msg);

  UniqueChars* error() { return error_; }

  void clearError() {
    if (error_) {
      error_->reset();
    }
  }

  bool done() const {
    MOZ_ASSERT(cur_ <= end_);
    return cur_ == end_;
  }
  bool resilientMode() const { return resilientMode_; }

  size_t bytesRemain() const {
    MOZ_ASSERT(end_ >= cur_);
    return size_t(end_ - cur_);
  }
  // pos must be a value previously returned from currentPosition.
  void rollbackPosition(const uint8_t* pos) { cur_ = pos; }
  const uint8_t* currentPosition() const { return cur_; }
  size_t currentOffset() const { return offsetInModule_ + (cur_ - beg_); }
  const uint8_t* begin() const { return beg_; }
  const uint8_t* end() const { return end_; }

  // Fixed-size encoding operations simply copy the literal bytes (without
  // attempting to align).

  MOZ_MUST_USE bool readFixedU8(uint8_t* i) { return read<uint8_t>(i); }
  MOZ_MUST_USE bool readFixedU32(uint32_t* u) { return read<uint32_t>(u); }
  MOZ_MUST_USE bool readFixedF32(float* f) { return read<float>(f); }
  MOZ_MUST_USE bool readFixedF64(double* d) { return read<double>(d); }

  // Variable-length encodings that all use LEB128.

  MOZ_MUST_USE bool readVarU32(uint32_t* out) {
    return readVarU<uint32_t>(out);
  }
  MOZ_MUST_USE bool readVarS32(int32_t* out) { return readVarS<int32_t>(out); }
  MOZ_MUST_USE bool readVarU64(uint64_t* out) {
    return readVarU<uint64_t>(out);
  }
  MOZ_MUST_USE bool readVarS64(int64_t* out) { return readVarS<int64_t>(out); }
  MOZ_MUST_USE bool readValType(uint8_t* code, uint32_t* refTypeIndex) {
    static_assert(uint8_t(TypeCode::Limit) <= UINT8_MAX, "fits");
    if (!readFixedU8(code)) {
      return false;
    }
    if (*code == uint8_t(TypeCode::Ref)) {
      if (!readVarU32(refTypeIndex)) {
        return false;
      }
      if (*refTypeIndex > MaxTypes) {
        return false;
      }
    } else {
      *refTypeIndex = NoRefTypeIndex;
    }
    return true;
  }
  MOZ_MUST_USE bool readBlockType(uint8_t* code, uint32_t* refTypeIndex) {
    static_assert(size_t(TypeCode::Limit) <= UINT8_MAX, "fits");
    if (!readFixedU8(code)) {
      return false;
    }
    if (*code == uint8_t(TypeCode::Ref)) {
      if (!readVarU32(refTypeIndex)) {
        return false;
      }
    } else {
      *refTypeIndex = NoRefTypeIndex;
    }
    return true;
  }
  MOZ_MUST_USE bool readOp(OpBytes* op) {
    static_assert(size_t(Op::Limit) == 256, "fits");
    uint8_t u8;
    if (!readFixedU8(&u8)) {
      return false;
    }
    op->b0 = u8;
    if (MOZ_LIKELY(!IsPrefixByte(u8))) {
      return true;
    }
    if (!readFixedU8(&u8)) {
      op->b1 = 0;  // Make it sane
      return false;
    }
    op->b1 = u8;
    return true;
  }

  // See writeBytes comment.

  MOZ_MUST_USE bool readBytes(uint32_t numBytes,
                              const uint8_t** bytes = nullptr) {
    if (bytes) {
      *bytes = cur_;
    }
    if (bytesRemain() < numBytes) {
      return false;
    }
    cur_ += numBytes;
    return true;
  }

  // See "section" description in Encoder.

  MOZ_MUST_USE bool readSectionHeader(uint8_t* id, SectionRange* range);

  MOZ_MUST_USE bool startSection(SectionId id, ModuleEnvironment* env,
                                 MaybeSectionRange* range,
                                 const char* sectionName);
  MOZ_MUST_USE bool finishSection(const SectionRange& range,
                                  const char* sectionName);

  // Custom sections do not cause validation errors unless the error is in
  // the section header itself.

  MOZ_MUST_USE bool startCustomSection(const char* expected,
                                       size_t expectedLength,
                                       ModuleEnvironment* env,
                                       MaybeSectionRange* range);

  template <size_t NameSizeWith0>
  MOZ_MUST_USE bool startCustomSection(const char (&name)[NameSizeWith0],
                                       ModuleEnvironment* env,
                                       MaybeSectionRange* range) {
    MOZ_ASSERT(name[NameSizeWith0 - 1] == '\0');
    return startCustomSection(name, NameSizeWith0 - 1, env, range);
  }

  void finishCustomSection(const char* name, const SectionRange& range);
  void skipAndFinishCustomSection(const SectionRange& range);

  MOZ_MUST_USE bool skipCustomSection(ModuleEnvironment* env);

  // The Name section has its own optional subsections.

  MOZ_MUST_USE bool startNameSubsection(NameType nameType,
                                        Maybe<uint32_t>* endOffset);
  MOZ_MUST_USE bool finishNameSubsection(uint32_t endOffset);
  MOZ_MUST_USE bool skipNameSubsection();

  // The infallible "unchecked" decoding functions can be used when we are
  // sure that the bytes are well-formed (by construction or due to previous
  // validation).

  uint8_t uncheckedReadFixedU8() { return uncheckedRead<uint8_t>(); }
  uint32_t uncheckedReadFixedU32() { return uncheckedRead<uint32_t>(); }
  void uncheckedReadFixedF32(float* out) { uncheckedRead<float>(out); }
  void uncheckedReadFixedF64(double* out) { uncheckedRead<double>(out); }
  template <typename UInt>
  UInt uncheckedReadVarU() {
    static const unsigned numBits = sizeof(UInt) * CHAR_BIT;
    static const unsigned remainderBits = numBits % 7;
    static const unsigned numBitsInSevens = numBits - remainderBits;
    UInt decoded = 0;
    uint32_t shift = 0;
    do {
      uint8_t byte = *cur_++;
      if (!(byte & 0x80)) {
        return decoded | (UInt(byte) << shift);
      }
      decoded |= UInt(byte & 0x7f) << shift;
      shift += 7;
    } while (shift != numBitsInSevens);
    uint8_t byte = *cur_++;
    MOZ_ASSERT(!(byte & 0xf0));
    return decoded | (UInt(byte) << numBitsInSevens);
  }
  uint32_t uncheckedReadVarU32() { return uncheckedReadVarU<uint32_t>(); }
  int32_t uncheckedReadVarS32() {
    int32_t i32 = 0;
    MOZ_ALWAYS_TRUE(readVarS32(&i32));
    return i32;
  }
  uint64_t uncheckedReadVarU64() { return uncheckedReadVarU<uint64_t>(); }
  int64_t uncheckedReadVarS64() {
    int64_t i64 = 0;
    MOZ_ALWAYS_TRUE(readVarS64(&i64));
    return i64;
  }
  Op uncheckedReadOp() {
    static_assert(size_t(Op::Limit) == 256, "fits");
    uint8_t u8 = uncheckedReadFixedU8();
    return u8 != UINT8_MAX ? Op(u8) : Op(uncheckedReadFixedU8() + UINT8_MAX);
  }
};

// The local entries are part of function bodies and thus serialized by both
// wasm and asm.js and decoded as part of both validation and compilation.

MOZ_MUST_USE bool EncodeLocalEntries(Encoder& d, const ValTypeVector& locals);

// This performs no validation; the local entries must already have been
// validated by an earlier pass.

MOZ_MUST_USE bool DecodeValidatedLocalEntries(Decoder& d,
                                              ValTypeVector* locals);

// This validates the entries.

MOZ_MUST_USE bool DecodeLocalEntries(Decoder& d, ModuleKind kind,
                                     const TypeDefVector& types,
                                     HasGcTypes gcTypesEnabled,
                                     ValTypeVector* locals);

// Returns whether the given [begin, end) prefix of a module's bytecode starts a
// code section and, if so, returns the SectionRange of that code section.
// Note that, even if this function returns 'false', [begin, end) may actually
// be a valid module in the special case when there are no function defs and the
// code section is not present. Such modules can be valid so the caller must
// handle this special case.

MOZ_MUST_USE bool StartsCodeSection(const uint8_t* begin, const uint8_t* end,
                                    SectionRange* range);

// Calling DecodeModuleEnvironment decodes all sections up to the code section
// and performs full validation of all those sections. The client must then
// decode the code section itself, reusing ValidateFunctionBody if necessary,
// and finally call DecodeModuleTail to decode all remaining sections after the
// code section (again, performing full validation).

MOZ_MUST_USE bool DecodeModuleEnvironment(Decoder& d, ModuleEnvironment* env);

MOZ_MUST_USE bool ValidateFunctionBody(const ModuleEnvironment& env,
                                       uint32_t funcIndex, uint32_t bodySize,
                                       Decoder& d,
                                       ExclusiveDeferredValidationState& dvs);

MOZ_MUST_USE bool DecodeModuleTail(Decoder& d, ModuleEnvironment* env,
                                   ExclusiveDeferredValidationState& dvs);

void ConvertMemoryPagesToBytes(Limits* memory);

// Validate an entire module, returning true if the module was validated
// successfully. If Validate returns false:
//  - if *error is null, the caller should report out-of-memory
//  - otherwise, there was a legitimate error described by *error

MOZ_MUST_USE bool Validate(JSContext* cx, const ShareableBytes& bytecode,
                           UniqueChars* error);

}  // namespace wasm
}  // namespace js

#endif  // namespace wasm_validate_h
