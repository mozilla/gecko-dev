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

#ifndef wasm_code_h
#define wasm_code_h

#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/PodOperations.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/UniquePtr.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <utility>

#include "jstypes.h"

#include "gc/Memory.h"
#include "jit/ProcessExecutableMemory.h"
#include "js/AllocPolicy.h"
#include "js/UniquePtr.h"
#include "js/Utility.h"
#include "js/Vector.h"
#include "threading/ExclusiveData.h"
#include "util/Memory.h"
#include "vm/MutexIDs.h"
#include "wasm/AsmJS.h"  // CodeMetadataForAsmJS::SeenSet
#include "wasm/WasmBuiltinModule.h"
#include "wasm/WasmBuiltins.h"
#include "wasm/WasmCodegenConstants.h"
#include "wasm/WasmCodegenTypes.h"
#include "wasm/WasmCompileArgs.h"
#include "wasm/WasmConstants.h"
#include "wasm/WasmExprType.h"
#include "wasm/WasmGC.h"
#include "wasm/WasmLog.h"
#include "wasm/WasmMetadata.h"
#include "wasm/WasmModuleTypes.h"
#include "wasm/WasmSerialize.h"
#include "wasm/WasmShareable.h"
#include "wasm/WasmTypeDecls.h"
#include "wasm/WasmTypeDef.h"
#include "wasm/WasmValType.h"

struct JS_PUBLIC_API JSContext;
class JSFunction;

namespace js {

namespace jit {
class MacroAssembler;
};

namespace wasm {

// LinkData contains all the metadata necessary to patch all the locations
// that depend on the absolute address of a CodeSegment. This happens in a
// "linking" step after compilation and after the module's code is serialized.
// The LinkData is serialized along with the Module but does not (normally, see
// Module::debugLinkData_ comment) persist after (de)serialization, which
// distinguishes it from Metadata, which is stored in the Code object.

struct LinkDataCacheablePod {
  uint32_t trapOffset = 0;

  WASM_CHECK_CACHEABLE_POD(trapOffset);

  LinkDataCacheablePod() = default;
};

WASM_DECLARE_CACHEABLE_POD(LinkDataCacheablePod);

WASM_CHECK_CACHEABLE_POD_PADDING(LinkDataCacheablePod)

struct LinkData : LinkDataCacheablePod {
  LinkData() = default;

  LinkDataCacheablePod& pod() { return *this; }
  const LinkDataCacheablePod& pod() const { return *this; }

  struct InternalLink {
    uint32_t patchAtOffset;
    uint32_t targetOffset;
#ifdef JS_CODELABEL_LINKMODE
    uint32_t mode;
#endif

    WASM_CHECK_CACHEABLE_POD(patchAtOffset, targetOffset);
#ifdef JS_CODELABEL_LINKMODE
    WASM_CHECK_CACHEABLE_POD(mode)
#endif
  };
  using InternalLinkVector = Vector<InternalLink, 0, SystemAllocPolicy>;

  struct SymbolicLinkArray
      : mozilla::EnumeratedArray<SymbolicAddress, Uint32Vector,
                                 size_t(SymbolicAddress::Limit)> {
    bool isEmpty() const {
      for (const Uint32Vector& symbolicLinks : *this) {
        if (symbolicLinks.length() != 0) {
          return false;
        }
      }
      return true;
    }
    void clear() {
      for (SymbolicAddress symbolicAddress :
           mozilla::MakeEnumeratedRange(SymbolicAddress::Limit)) {
        (*this)[symbolicAddress].clear();
      }
    }

    size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
  };

  InternalLinkVector internalLinks;
  CallFarJumpVector callFarJumps;
  SymbolicLinkArray symbolicLinks;

  bool isEmpty() const {
    return internalLinks.length() == 0 && callFarJumps.length() == 0 &&
           symbolicLinks.isEmpty();
  }
  void clear() {
    internalLinks.clear();
    callFarJumps.clear();
    symbolicLinks.clear();
  }

  size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
};

WASM_DECLARE_CACHEABLE_POD(LinkData::InternalLink);

using UniqueLinkData = UniquePtr<LinkData>;
using UniqueLinkDataVector = Vector<UniqueLinkData, 0, SystemAllocPolicy>;

// Executable code must be deallocated specially.

struct FreeCode {
  uint32_t codeLength;
  FreeCode() : codeLength(0) {}
  explicit FreeCode(uint32_t codeLength) : codeLength(codeLength) {}
  void operator()(uint8_t* codeBytes);
};

using UniqueCodeBytes = UniquePtr<uint8_t, FreeCode>;

class Code;
class CodeBlock;

using UniqueCodeBlock = UniquePtr<CodeBlock>;
using UniqueConstCodeBlock = UniquePtr<const CodeBlock>;
using UniqueConstCodeBlockVector =
    Vector<UniqueConstCodeBlock, 0, SystemAllocPolicy>;
using RawCodeBlockVector = Vector<const CodeBlock*, 0, SystemAllocPolicy>;

enum class CodeBlockKind {
  SharedStubs,
  BaselineTier,
  OptimizedTier,
  LazyStubs
};

// CodeSegment contains common helpers for determining the base and length of a
// code segment and if a pc belongs to this segment. It is inherited by:
// - ModuleSegment, i.e. the code segment of a Module, generated
// eagerly when a Module is instanciated.
// - LazyStubSegment, i.e. the code segment of entry stubs that are lazily
// generated.

// LazyStubSegment is a code segment lazily generated for function entry stubs
// (both interpreter and jit ones).
//
// Because a stub is usually small (a few KiB) and an executable code segment
// isn't (64KiB), a given stub segment can contain entry stubs of many
// functions.

// A wasm ModuleSegment owns the allocated executable code for a wasm module.

class CodeSegment : public ShareableBase<CodeSegment> {
 private:
  const UniqueCodeBytes bytes_;
  uint32_t lengthBytes_;
  const uint32_t capacityBytes_;
  const Code* code_;

 public:
  CodeSegment(UniqueCodeBytes bytes, uint32_t lengthBytes,
              uint32_t capacityBytes)
      : bytes_(std::move(bytes)),
        lengthBytes_(lengthBytes),
        capacityBytes_(capacityBytes),
        code_(nullptr) {}

  // Create a new, empty code segment.  Allocation granularity is
  // ExecutableCodePageSize (64KB).
  static RefPtr<CodeSegment> createEmpty(size_t capacityBytes);

  // Create a new code segment and copy/link code from `masm` into it.
  // Allocation granularity is ExecutableCodePageSize (64KB).
  static RefPtr<CodeSegment> createFromMasm(jit::MacroAssembler& masm,
                                            const LinkData& linkData,
                                            const Code* maybeCode);

  // Create a new code segment and copy/link code from `unlinkedBytes` into
  // it.  Allocation granularity is ExecutableCodePageSize (64KB).
  static RefPtr<CodeSegment> createFromBytes(const uint8_t* unlinkedBytes,
                                             size_t unlinkedBytesLength,
                                             const LinkData& linkData);

  // Allocate code space at a hardware page granularity, taking space from
  // `code->lazyFuncSegments`, and copy/link code from `masm` into it.  If an
  // existing segment can satisy the allocation, space is reserved there, and
  // that segment is returned; else a new segment is created for the
  // allocation, is added to `code->lazyFuncSegments`, and returned.
  //
  // The location/length of the final code is returned in
  // `*codeStart`/`*codeLength`.  Note that placement is somewhat randomised
  // inside the page, so `*codeStart` will not be page-aligned.  Also, the
  // metadata associated with the code block will have to be offset by the
  // value returned in `*metadataBias`.
  static RefPtr<CodeSegment> createFromMasmWithBumpAlloc(
      jit::MacroAssembler& masm, const LinkData& linkData, const Code* code,
      uint8_t** codeStartOut, uint32_t* codeLengthOut,
      uint32_t* metadataBiasOut);

  // For this CodeSegment, perform linking on the area
  // [codeStart, +codeLength), then make all pages that intersect
  // [pageStart, +codeLength+(codeStart-pageStart)) executable.  See ASCII
  // art at CodeSegment::createFromMasmWithBumpAlloc (implementation) for the
  // meaning of pageStart/codeStart/codeLength.
  bool linkAndMakeExecutableSubRange(
      jit::AutoMarkJitCodeWritableForThread& writable, const LinkData& linkData,
      const Code* maybeCode, uint8_t* pageStart, uint8_t* codeStart,
      uint32_t codeLength);

  // For this CodeSegment, perform linking on the entire code area, then make
  // it executable.
  bool linkAndMakeExecutable(jit::AutoMarkJitCodeWritableForThread& writable,
                             const LinkData& linkData, const Code* maybeCode);

  void setCode(const Code& code) { code_ = &code; }

  uint8_t* base() const { return bytes_.get(); }
  uint32_t lengthBytes() const {
    MOZ_ASSERT(lengthBytes_ != UINT32_MAX);
    return lengthBytes_;
  }
  uint32_t capacityBytes() const {
    MOZ_ASSERT(capacityBytes_ != UINT32_MAX);
    return capacityBytes_;
  }

  static size_t PageSize() { return gc::SystemPageSize(); }
  static size_t PageRoundup(uintptr_t bytes) {
    // All new code allocations must be rounded to the system page size
    return AlignBytes(bytes, gc::SystemPageSize());
  }
  static bool IsPageAligned(uintptr_t bytes) {
    return bytes == PageRoundup(bytes);
  }
  bool hasSpace(size_t bytes) const {
    MOZ_ASSERT(IsPageAligned(bytes));
    return bytes <= capacityBytes() && lengthBytes_ <= capacityBytes() - bytes;
  }
  void claimSpace(size_t bytes, uint8_t** claimedBase) {
    MOZ_RELEASE_ASSERT(hasSpace(bytes));
    *claimedBase = base() + lengthBytes_;
    lengthBytes_ += bytes;
  }

  const Code& code() const { return *code_; }

  void addSizeOfMisc(mozilla::MallocSizeOf mallocSizeOf, size_t* code,
                     size_t* data) const;
  WASM_DECLARE_FRIEND_SERIALIZE(CodeSegment);
};

using SharedCodeSegment = RefPtr<CodeSegment>;
using SharedCodeSegmentVector = Vector<SharedCodeSegment, 0, SystemAllocPolicy>;

extern UniqueCodeBytes AllocateCodeBytes(
    mozilla::Maybe<jit::AutoMarkJitCodeWritableForThread>& writable,
    uint32_t codeLength);
extern bool StaticallyLink(jit::AutoMarkJitCodeWritableForThread& writable,
                           uint8_t* base, const LinkData& linkData,
                           const Code* maybeCode);
extern void StaticallyUnlink(uint8_t* base, const LinkData& linkData);

enum class TierUpState : uint32_t {
  NotRequested,
  Requested,
  Finished,
};

struct FuncState {
  mozilla::Atomic<const CodeBlock*> bestTier;
  mozilla::Atomic<TierUpState> tierUpState;
};
using FuncStatesPointer = mozilla::UniquePtr<FuncState[], JS::FreePolicy>;

// LazyFuncExport helps to efficiently lookup a CodeRange from a given function
// index. It is inserted in a vector sorted by function index, to perform
// binary search on it later.

struct LazyFuncExport {
  size_t funcIndex;
  size_t lazyStubBlockIndex;
  size_t funcCodeRangeIndex;
  // Used to make sure we only upgrade a lazy stub from baseline to ion.
  mozilla::DebugOnly<CodeBlockKind> funcKind;

  LazyFuncExport(size_t funcIndex, size_t lazyStubBlockIndex,
                 size_t funcCodeRangeIndex, CodeBlockKind funcKind)
      : funcIndex(funcIndex),
        lazyStubBlockIndex(lazyStubBlockIndex),
        funcCodeRangeIndex(funcCodeRangeIndex),
        funcKind(funcKind) {}
};

using LazyFuncExportVector = Vector<LazyFuncExport, 0, SystemAllocPolicy>;

static const uint32_t BAD_CODE_RANGE = UINT32_MAX;

class FuncToCodeRangeMap {
  uint32_t startFuncIndex_ = 0;
  Uint32Vector funcToCodeRange_;

  bool denseHasFuncIndex(uint32_t funcIndex) const {
    return funcIndex >= startFuncIndex_ &&
           funcIndex - startFuncIndex_ < funcToCodeRange_.length();
  }

  FuncToCodeRangeMap(uint32_t startFuncIndex, Uint32Vector&& funcToCodeRange)
      : startFuncIndex_(startFuncIndex),
        funcToCodeRange_(std::move(funcToCodeRange)) {}

 public:
  [[nodiscard]] static bool createDense(uint32_t startFuncIndex,
                                        uint32_t numFuncs,
                                        FuncToCodeRangeMap* result) {
    Uint32Vector funcToCodeRange;
    if (!funcToCodeRange.appendN(BAD_CODE_RANGE, numFuncs)) {
      return false;
    }
    *result = FuncToCodeRangeMap(startFuncIndex, std::move(funcToCodeRange));
    return true;
  }

  FuncToCodeRangeMap() = default;
  FuncToCodeRangeMap(FuncToCodeRangeMap&& rhs) = default;
  FuncToCodeRangeMap& operator=(FuncToCodeRangeMap&& rhs) = default;
  FuncToCodeRangeMap(const FuncToCodeRangeMap& rhs) = delete;
  FuncToCodeRangeMap& operator=(const FuncToCodeRangeMap& rhs) = delete;

  uint32_t lookup(uint32_t funcIndex) const {
    if (!denseHasFuncIndex(funcIndex)) {
      return BAD_CODE_RANGE;
    }
    return funcToCodeRange_[funcIndex - startFuncIndex_];
  }

  uint32_t operator[](uint32_t funcIndex) const { return lookup(funcIndex); }

  [[nodiscard]] bool insert(uint32_t funcIndex, uint32_t codeRangeIndex) {
    if (!denseHasFuncIndex(funcIndex)) {
      return false;
    }
    funcToCodeRange_[funcIndex - startFuncIndex_] = codeRangeIndex;
    return true;
  }
  void insertInfallible(uint32_t funcIndex, uint32_t codeRangeIndex) {
    bool result = insert(funcIndex, codeRangeIndex);
    MOZ_RELEASE_ASSERT(result);
  }

  void shrinkStorageToFit() { funcToCodeRange_.shrinkStorageToFit(); }

  void assertAllInitialized() {
#ifdef DEBUG
    for (uint32_t codeRangeIndex : funcToCodeRange_) {
      MOZ_ASSERT(codeRangeIndex != BAD_CODE_RANGE);
    }
#endif
  }

  size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
    return funcToCodeRange_.sizeOfExcludingThis(mallocSizeOf);
  }

  size_t numEntries() const { return funcToCodeRange_.length(); }

  WASM_DECLARE_FRIEND_SERIALIZE(FuncToCodeRangeMap);
};

// CodeBlock contains all the data related to a given compilation tier. It is
// built during module generation and then immutably stored in a Code.
//
// Code contains a map from PC to containing code block. The map is thread-safe
// to support lookups from multiple threads (see ThreadSafeCodeBlockMap). This
// is safe because code blocks are immutable after creation, so there won't
// be any concurrent modification during a metadata lookup.

class CodeBlock {
 public:
  // Weak reference to the code that owns us, not serialized.
  const Code* code;
  // The index we are held inside our containing Code::data::blocks_ vector.
  size_t codeBlockIndex;

  // The following information is all serialized
  // Which kind of code is being stored in this block. Most consumers don't
  // care about this.
  const CodeBlockKind kind;

  // The code segment our JIT code is within.
  SharedCodeSegment segment;
  // The sub-range of the code segment our JIT code is within.
  const uint8_t* codeBase;
  size_t codeLength;

  // Metadata about the code we have contributed to the segment.
  //
  // * `funcToCodeRange` does not involve code locations.
  //
  // * `stackMaps` specifies code locations directly, in
  //   StackMaps::Maplet::nextInsnAddr.
  //
  // * All 6 other fields specify a code locations in by carrying an offset
  //   which is interpreted to be relative to the start of the containing
  //   segment, *not* relative to `CodeBlock::codeBase`.  That is, the denoted
  //   address is `segment->base() + ..offset...`.
  //
  FuncToCodeRangeMap funcToCodeRange;
  CodeRangeVector codeRanges;
  CallSiteVector callSites;
  TrapSiteVectorArray trapSites;
  FuncExportVector funcExports;
  StackMaps stackMaps;
  TryNoteVector tryNotes;
  CodeRangeUnwindInfoVector codeRangeUnwindInfos;

  // Track whether we are registered in the process map of code blocks.
  bool unregisterOnDestroy_;

  static constexpr CodeBlockKind kindFromTier(Tier tier) {
    if (tier == Tier::Optimized) {
      return CodeBlockKind::OptimizedTier;
    }
    MOZ_ASSERT(tier == Tier::Baseline);
    return CodeBlockKind::BaselineTier;
  }

  explicit CodeBlock(CodeBlockKind kind)
      : code(nullptr),
        codeBlockIndex((size_t)-1),
        kind(kind),
        unregisterOnDestroy_(false) {}
  ~CodeBlock();

  bool initialized() const {
    if (code) {
      // Initialize should have given us an index too.
      MOZ_ASSERT(codeBlockIndex != (size_t)-1);
      return true;
    }
    return false;
  }

  bool initialize(const Code& code, size_t codeBlockIndex);

  // Gets the tier for this code block. Only valid for non-lazy stub code.
  Tier tier() const {
    switch (kind) {
      case CodeBlockKind::BaselineTier:
        return Tier::Baseline;
      case CodeBlockKind::OptimizedTier:
        return Tier::Optimized;
      default:
        MOZ_CRASH();
    }
  }

  // Returns whether this code block should be considered for serialization.
  bool isSerializable() const {
    return kind == CodeBlockKind::SharedStubs ||
           kind == CodeBlockKind::OptimizedTier;
  }

  // Add an offset to the metadata.  See comment above.
  void offsetMetadataBy(uint32_t delta);

  const uint8_t* base() const { return codeBase; }
  uint32_t length() const { return codeLength; }
  bool containsCodePC(const void* pc) const {
    return pc >= base() && pc < (base() + length());
  }

  const CodeRange& codeRange(const FuncExport& funcExport) const {
    return codeRanges[funcToCodeRange[funcExport.funcIndex()]];
  }

  const CodeRange* lookupRange(const void* pc) const;
  const CallSite* lookupCallSite(void* pc) const;
  const StackMap* lookupStackMap(uint8_t* pc) const;
  const TryNote* lookupTryNote(const void* pc) const;
  bool lookupTrap(void* pc, Trap* trapOut, BytecodeOffset* bytecode) const;
  const CodeRangeUnwindInfo* lookupUnwindInfo(void* pc) const;
  FuncExport& lookupFuncExport(uint32_t funcIndex,
                               size_t* funcExportIndex = nullptr);
  const FuncExport& lookupFuncExport(uint32_t funcIndex,
                                     size_t* funcExportIndex = nullptr) const;

  void disassemble(JSContext* cx, int kindSelection,
                   PrintCallback printString) const;

  void addSizeOfMisc(mozilla::MallocSizeOf mallocSizeOf, size_t* code,
                     size_t* data) const;

  WASM_DECLARE_FRIEND_SERIALIZE_ARGS(CodeBlock, const wasm::LinkData& data);
};

// Because of profiling, the thread running wasm might need to know to which
// CodeBlock the current PC belongs, during a call to lookup(). A lookup
// is a read-only operation, and we don't want to take a lock then
// (otherwise, we could have a deadlock situation if an async lookup
// happened on a given thread that was holding mutatorsMutex_ while getting
// sampled). Since the writer could be modifying the data that is getting
// looked up, the writer functions use spin-locks to know if there are any
// observers (i.e. calls to lookup()) of the atomic data.

class ThreadSafeCodeBlockMap {
  // Since writes (insertions or removals) can happen on any background
  // thread at the same time, we need a lock here.

  Mutex mutatorsMutex_ MOZ_UNANNOTATED;

  RawCodeBlockVector segments1_;
  RawCodeBlockVector segments2_;

  // Except during swapAndWait(), there are no lookup() observers of the
  // vector pointed to by mutableCodeBlocks_

  RawCodeBlockVector* mutableCodeBlocks_;
  mozilla::Atomic<const RawCodeBlockVector*> readonlyCodeBlocks_;
  mozilla::Atomic<size_t> numActiveLookups_;

  struct CodeBlockPC {
    const void* pc;
    explicit CodeBlockPC(const void* pc) : pc(pc) {}
    int operator()(const CodeBlock* cb) const {
      if (cb->containsCodePC(pc)) {
        return 0;
      }
      if (pc < cb->base()) {
        return -1;
      }
      return 1;
    }
  };

  void swapAndWait() {
    // Both vectors are consistent for lookup at this point although their
    // contents are different: there is no way for the looked up PC to be
    // in the code segment that is getting registered, because the code
    // segment is not even fully created yet.

    // If a lookup happens before this instruction, then the
    // soon-to-become-former read-only pointer is used during the lookup,
    // which is valid.

    mutableCodeBlocks_ = const_cast<RawCodeBlockVector*>(
        readonlyCodeBlocks_.exchange(mutableCodeBlocks_));

    // If a lookup happens after this instruction, then the updated vector
    // is used, which is valid:
    // - in case of insertion, it means the new vector contains more data,
    // but it's fine since the code segment is getting registered and thus
    // isn't even fully created yet, so the code can't be running.
    // - in case of removal, it means the new vector contains one less
    // entry, but it's fine since unregistering means the code segment
    // isn't used by any live instance anymore, thus PC can't be in the
    // to-be-removed code segment's range.

    // A lookup could have happened on any of the two vectors. Wait for
    // observers to be done using any vector before mutating.

    while (numActiveLookups_ > 0) {
    }
  }

 public:
  ThreadSafeCodeBlockMap()
      : mutatorsMutex_(mutexid::WasmCodeBlockMap),
        mutableCodeBlocks_(&segments1_),
        readonlyCodeBlocks_(&segments2_),
        numActiveLookups_(0) {}

  ~ThreadSafeCodeBlockMap() {
    MOZ_RELEASE_ASSERT(numActiveLookups_ == 0);
    segments1_.clearAndFree();
    segments2_.clearAndFree();
  }

  size_t numActiveLookups() const { return numActiveLookups_; }

  bool insert(const CodeBlock* cs) {
    LockGuard<Mutex> lock(mutatorsMutex_);

    size_t index;
    MOZ_ALWAYS_FALSE(BinarySearchIf(*mutableCodeBlocks_, 0,
                                    mutableCodeBlocks_->length(),
                                    CodeBlockPC(cs->base()), &index));

    if (!mutableCodeBlocks_->insert(mutableCodeBlocks_->begin() + index, cs)) {
      return false;
    }

    swapAndWait();

#ifdef DEBUG
    size_t otherIndex;
    MOZ_ALWAYS_FALSE(BinarySearchIf(*mutableCodeBlocks_, 0,
                                    mutableCodeBlocks_->length(),
                                    CodeBlockPC(cs->base()), &otherIndex));
    MOZ_ASSERT(index == otherIndex);
#endif

    // Although we could simply revert the insertion in the read-only
    // vector, it is simpler to just crash and given that each CodeBlock
    // consumes multiple pages, it is unlikely this insert() would OOM in
    // practice
    AutoEnterOOMUnsafeRegion oom;
    if (!mutableCodeBlocks_->insert(mutableCodeBlocks_->begin() + index, cs)) {
      oom.crash("when inserting a CodeBlock in the process-wide map");
    }

    return true;
  }

  size_t remove(const CodeBlock* cs) {
    LockGuard<Mutex> lock(mutatorsMutex_);

    size_t index;
    MOZ_ALWAYS_TRUE(BinarySearchIf(*mutableCodeBlocks_, 0,
                                   mutableCodeBlocks_->length(),
                                   CodeBlockPC(cs->base()), &index));

    mutableCodeBlocks_->erase(mutableCodeBlocks_->begin() + index);
    size_t newCodeBlockCount = mutableCodeBlocks_->length();

    swapAndWait();

#ifdef DEBUG
    size_t otherIndex;
    MOZ_ALWAYS_TRUE(BinarySearchIf(*mutableCodeBlocks_, 0,
                                   mutableCodeBlocks_->length(),
                                   CodeBlockPC(cs->base()), &otherIndex));
    MOZ_ASSERT(index == otherIndex);
#endif

    mutableCodeBlocks_->erase(mutableCodeBlocks_->begin() + index);
    return newCodeBlockCount;
  }

  const CodeBlock* lookup(const void* pc,
                          const CodeRange** codeRange = nullptr) {
    auto decObserver = mozilla::MakeScopeExit([&] {
      MOZ_ASSERT(numActiveLookups_ > 0);
      numActiveLookups_--;
    });
    numActiveLookups_++;

    const RawCodeBlockVector* readonly = readonlyCodeBlocks_;

    size_t index;
    if (!BinarySearchIf(*readonly, 0, readonly->length(), CodeBlockPC(pc),
                        &index)) {
      if (codeRange) {
        *codeRange = nullptr;
      }
      return nullptr;
    }

    // It is fine returning a raw CodeBlock*, because we assume we are
    // looking up a live PC in code which is on the stack, keeping the
    // CodeBlock alive.

    const CodeBlock* result = (*readonly)[index];
    if (codeRange) {
      *codeRange = result->lookupRange(pc);
    }
    return result;
  }
};

// Jump tables that implement function tiering and fast js-to-wasm calls.
//
// There is one JumpTable object per Code object, holding two jump tables: the
// tiering jump table and the jit-entry jump table.  The JumpTable is not
// serialized with its Code, but is a run-time entity only.  At run-time it is
// shared across threads with its owning Code (and the Module that owns the
// Code).  Values in the JumpTable /must/ /always/ be JSContext-agnostic and
// Instance-agnostic, because of this sharing.
//
// Both jump tables have a number of entries equal to the number of functions in
// their Module, including imports.  In the tiering table, the elements
// corresponding to the Module's imported functions are unused; in the jit-entry
// table, the elements corresponding to the Module's non-exported functions are
// unused.  (Functions can be exported explicitly via the exports section or
// implicitly via a mention of their indices outside function bodies.)  See
// comments at JumpTables::init() and WasmInstanceObject::getExportedFunction().
// The entries are void*.  Unused entries are null.
//
// The tiering jump table.
//
// This table holds code pointers that are used by baseline functions to enter
// optimized code.  See the large comment block in WasmCompile.cpp for
// information about how tiering works.
//
// The jit-entry jump table.
//
// The jit-entry jump table entry for a function holds a stub that allows Jitted
// JS code to call wasm using the JS JIT ABI.  See large comment block at
// WasmInstanceObject::getExportedFunction() for more about exported functions
// and stubs and the lifecycle of the entries in the jit-entry table - there are
// complex invariants.

class JumpTables {
  using TablePointer = mozilla::UniquePtr<void*[], JS::FreePolicy>;

  CompileMode mode_;
  TablePointer tiering_;
  TablePointer jit_;
  size_t numFuncs_;

  static_assert(
      JumpTableJitEntryOffset == 0,
      "Each jit entry in table must have compatible layout with BaseScript and"
      "SelfHostedLazyScript");

 public:
  bool initialize(CompileMode mode, const CodeMetadata& codeMeta,
                  const CodeBlock& sharedStubs, const CodeBlock& tier1);

  void setJitEntry(size_t i, void* target) const {
    // Make sure that write is atomic; see comment in wasm::Module::finishTier2
    // to that effect.
    MOZ_ASSERT(i < numFuncs_);
    jit_.get()[i] = target;
  }
  void setJitEntryIfNull(size_t i, void* target) const {
    // Make sure that compare-and-write is atomic; see comment in
    // wasm::Module::finishTier2 to that effect.
    MOZ_ASSERT(i < numFuncs_);
    void* expected = nullptr;
    (void)__atomic_compare_exchange_n(&jit_.get()[i], &expected, target,
                                      /*weak=*/false, __ATOMIC_RELAXED,
                                      __ATOMIC_RELAXED);
  }
  void** getAddressOfJitEntry(size_t i) const {
    MOZ_ASSERT(i < numFuncs_);
    MOZ_ASSERT(jit_.get()[i]);
    return &jit_.get()[i];
  }
  size_t funcIndexFromJitEntry(void** target) const {
    MOZ_ASSERT(target >= &jit_.get()[0]);
    MOZ_ASSERT(target <= &(jit_.get()[numFuncs_ - 1]));
    return (intptr_t*)target - (intptr_t*)&jit_.get()[0];
  }

  void setTieringEntry(size_t i, void* target) const {
    MOZ_ASSERT(i < numFuncs_);
    // See comment in wasm::Module::finishTier2.
    if (mode_ != CompileMode::Once) {
      tiering_.get()[i] = target;
    }
  }
  void** tiering() const { return tiering_.get(); }

  size_t sizeOfMiscExcludingThis() const {
    // 2 words per function for the jit entry table, plus maybe 1 per
    // function if we're tiering.
    return sizeof(void*) * (2 + (tiering_ ? 1 : 0)) * numFuncs_;
  }
};

// Code objects own executable code and the metadata that describe it. A single
// Code object is normally shared between a module and all its instances.
//
// profilingLabels_ is lazily initialized, but behind a lock.

using SharedCode = RefPtr<const Code>;
using MutableCode = RefPtr<Code>;
using MetadataAnalysisHashMap =
    HashMap<const char*, uint32_t, mozilla::CStringHasher, SystemAllocPolicy>;

class Code : public ShareableBase<Code> {
  // A primitive PRNG, as used in early C library implementations.
  // See https://en.wikipedia.org/wiki/
  //             Linear_congruential_generator#Parameters_in_common_use.
  // It is used for randomising code layout so as to avoid icache misses, not
  // for any security-related reason, which is why we don't care about its
  // quality too much.  It also gives us repeatability when debugging or
  // profiling.
  class SimplePRNG {
    uint32_t state_;

   public:
    SimplePRNG() : state_(999) {}
    // Returns an 11-bit pseudo-random number.
    uint32_t get11RandomBits() {
      state_ = state_ * 1103515245 + 12345;
      // Both the high and low order bits are reputed to be not very random.
      // Throw them away.
      return (state_ >> 4) & 0x7FF;
    }
  };
  struct ProtectedData {
    // A vector of all of the code blocks owned by this code. Each code block
    // is immutable once added to the vector, but this vector may grow.
    UniqueConstCodeBlockVector blocks;
    // A vector of link data paired 1:1 with `blocks`. Entries may be null if
    // the code block is not serializable. This is separate from CodeBlock so
    // that we may clear it out after serialization has happened.
    UniqueLinkDataVector blocksLinkData;

    // A vector of code segments that we can allocate lazy segments into
    SharedCodeSegmentVector lazyStubSegments;
    // A sorted vector of LazyFuncExport
    LazyFuncExportVector lazyExports;

    // A vector of code segments that we can lazily allocate functions into
    SharedCodeSegmentVector lazyFuncSegments;

    // For randomizing code layout.
    SimplePRNG simplePRNG;
  };
  using ReadGuard = RWExclusiveData<ProtectedData>::ReadGuard;
  using WriteGuard = RWExclusiveData<ProtectedData>::WriteGuard;

  // The compile mode this code is used with.
  const CompileMode mode_;

  // Core data that is not thread-safe and must acquire a lock in order to
  // access.
  RWExclusiveData<ProtectedData> data_;

  // Thread-safe mutable map from code pointer to code block that contains it.
  mutable ThreadSafeCodeBlockMap blockMap_;

  // These have the same lifetime end as Code itself -- they can be dropped
  // when Code itself is dropped.  FIXME: should these be MutableCodeXX?
  //
  // This must always be non-null.
  SharedCodeMetadata codeMeta_;
  // This is null for a wasm module, non-null for asm.js
  SharedCodeMetadataForAsmJS codeMetaForAsmJS_;

  const CodeBlock* sharedStubs_;
  const CodeBlock* completeTier1_;

  // [SMDOC] Tier-2 data
  //
  // hasCompleteTier2_ and completeTier2_ implement a three-state protocol for
  // broadcasting tier-2 data; this also amounts to a single-writer/
  // multiple-reader setup.
  //
  // Initially hasCompleteTier2_ is false and completeTier2_ is null.
  //
  // While hasCompleteTier2_ is false, *no* thread may read completeTier2_, but
  // one thread may make completeTier2_ non-null (this will be the tier-2
  // compiler thread).  That same thread must then later set hasCompleteTier2_
  // to true to broadcast the completeTier2_ value and its availability.  Note
  // that the writing thread may not itself read completeTier2_ before setting
  // hasCompleteTier2_, in order to simplify reasoning about global invariants.
  //
  // Once hasCompleteTier2_ is true, *no* thread may write completeTier2_ and
  // *no* thread may read completeTier2_ without having observed
  // hasCompleteTier2_ as true first.  Once hasCompleteTier2_ is true, it stays
  // true.
  mutable const CodeBlock* completeTier2_;
  mutable mozilla::Atomic<bool> hasCompleteTier2_;

  // State for every defined function (not imported) in this module. This is
  // only needed if we're doing partial tiering.
  mutable FuncStatesPointer funcStates_;

  FuncImportVector funcImports_;
  ExclusiveData<CacheableCharsVector> profilingLabels_;
  JumpTables jumpTables_;

  // Where to redirect PC to for handling traps from the signal handler.
  uint8_t* trapCode_;

  // Offset of the debug stub in the `sharedStubs_` CodeBlock.  Not serialized.
  uint32_t debugStubOffset_;

  // Offset of the request-tier-up stub in the `sharedStubs_` CodeBlock.
  uint32_t requestTierUpStubOffset_;

  // Methods for getting complete tiers, private while we're moving to partial
  // tiering.
  Tiers completeTiers() const;

  [[nodiscard]] bool addCodeBlock(const WriteGuard& guard,
                                  UniqueCodeBlock block,
                                  UniqueLinkData maybeLinkData) const;

  [[nodiscard]] const LazyFuncExport* lookupLazyFuncExport(
      const WriteGuard& guard, uint32_t funcIndex) const;

  // Returns a pointer to the raw interpreter entry of a given function for
  // which stubs have been lazily generated.
  [[nodiscard]] void* lookupLazyInterpEntry(const WriteGuard& guard,
                                            uint32_t funcIndex) const;

  [[nodiscard]] bool createOneLazyEntryStub(const WriteGuard& guard,
                                            uint32_t funcExportIndex,
                                            const CodeBlock& tierCodeBlock,
                                            void** interpEntry) const;
  [[nodiscard]] bool createManyLazyEntryStubs(
      const WriteGuard& guard, const Uint32Vector& funcExportIndices,
      const CodeBlock& tierCodeBlock, size_t* stubBlockIndex) const;
  // Create one lazy stub for all the functions in funcExportIndices, putting
  // them in a single stub. Jit entries won't be used until
  // setJitEntries() is actually called, after the Code owner has committed
  // tier2.
  [[nodiscard]] bool createTier2LazyEntryStubs(
      const WriteGuard& guard, const CodeBlock& tier2Code,
      mozilla::Maybe<size_t>* outStubBlockIndex) const;
  [[nodiscard]] bool appendProfilingLabels(
      const ExclusiveData<CacheableCharsVector>::Guard& labels,
      const CodeBlock& codeBlock) const;

 public:
  Code(CompileMode mode, const CodeMetadata& codeMeta,
       const CodeMetadataForAsmJS* codeMetaForAsmJS);

  [[nodiscard]] bool initialize(FuncImportVector&& funcImports,
                                UniqueCodeBlock sharedStubs,
                                UniqueLinkData sharedStubsLinkData,
                                UniqueCodeBlock tier1CodeBlock,
                                UniqueLinkData tier1LinkData);
  [[nodiscard]] bool finishTier2(UniqueCodeBlock tier2CodeBlock,
                                 UniqueLinkData tier2LinkData) const;

  [[nodiscard]] bool getOrCreateInterpEntry(uint32_t funcIndex,
                                            const FuncExport** funcExport,
                                            void** interpEntry) const;

  const RWExclusiveData<ProtectedData>& data() const { return data_; }

  bool requestTierUp(uint32_t funcIndex) const;

  CompileMode mode() const { return mode_; }

  void** tieringJumpTable() const { return jumpTables_.tiering(); }

  void setJitEntryIfNull(size_t i, void* target) const {
    jumpTables_.setJitEntryIfNull(i, target);
  }
  void** getAddressOfJitEntry(size_t i) const {
    return jumpTables_.getAddressOfJitEntry(i);
  }
  uint32_t getFuncIndex(JSFunction* fun) const;

  uint8_t* trapCode() const { return trapCode_; }

  uint32_t debugStubOffset() const { return debugStubOffset_; }
  void setDebugStubOffset(uint32_t offs) { debugStubOffset_ = offs; }

  uint32_t requestTierUpStubOffset() const { return requestTierUpStubOffset_; }
  void setRequestTierUpStubOffset(uint32_t offs) {
    requestTierUpStubOffset_ = offs;
  }

  const Bytes& bytecode() const {
    MOZ_ASSERT(codeMeta().debugEnabled || mode_ == CompileMode::LazyTiering);
    return codeMeta_->bytecode->bytes;
  }

  const FuncImport& funcImport(uint32_t funcIndex) const {
    return funcImports_[funcIndex];
  }
  const FuncImportVector& funcImports() const { return funcImports_; }

  bool hasCompleteTier(Tier tier) const;
  // The 'stable' complete tier of code. This is stable during a run/
  Tier stableCompleteTier() const;
  // The 'best' complete tier of code. This may transition from baseline to ion
  // at any time.
  Tier bestCompleteTier() const;
  bool hasSerializableCode() const { return hasCompleteTier(Tier::Serialized); }

  const CodeMetadata& codeMeta() const { return *codeMeta_; }
  const CodeMetadataForAsmJS* codeMetaForAsmJS() const {
    return codeMetaForAsmJS_;
  }

  const CodeBlock& sharedStubs() const { return *sharedStubs_; }
  const CodeBlock& debugCodeBlock() const {
    MOZ_ASSERT(codeMeta_->debugEnabled);
    MOZ_ASSERT(completeTier1_->tier() == Tier::Debug);
    return *completeTier1_;
  }
  const CodeBlock& completeTierCodeBlock(Tier tier) const;
  const CodeBlock& funcCodeBlock(uint32_t funcIndex) const {
    if (funcIndex < funcImports_.length()) {
      return *sharedStubs_;
    }
    if (mode_ == CompileMode::LazyTiering) {
      return *funcStates_.get()[funcIndex - codeMeta_->numFuncImports].bestTier;
    }
    return completeTierCodeBlock(bestCompleteTier());
  }
  bool funcHasTier(uint32_t funcIndex, Tier tier) const {
    return funcCodeBlock(funcIndex).tier() == tier;
  }

  const LinkData* codeBlockLinkData(const CodeBlock& block) const;
  void clearLinkData() const;

  // Code metadata lookup:
  const CallSite* lookupCallSite(void* pc) const {
    const CodeBlock* block = blockMap_.lookup(pc);
    if (!block) {
      return nullptr;
    }
    return block->lookupCallSite(pc);
  }
  const CodeRange* lookupFuncRange(void* pc) const {
    const CodeBlock* block = blockMap_.lookup(pc);
    if (!block) {
      return nullptr;
    }
    const CodeRange* result = block->lookupRange(pc);
    if (result && result->isFunction()) {
      return result;
    }
    return nullptr;
  }
  const StackMap* lookupStackMap(uint8_t* pc) const {
    const CodeBlock* block = blockMap_.lookup(pc);
    if (!block) {
      return nullptr;
    }
    return block->lookupStackMap(pc);
  }
  const wasm::TryNote* lookupTryNote(void* pc, const CodeBlock** block) const {
    *block = blockMap_.lookup(pc);
    if (!*block) {
      return nullptr;
    }
    return (*block)->lookupTryNote(pc);
  }
  bool lookupTrap(void* pc, Trap* trapOut, BytecodeOffset* bytecode) const {
    const CodeBlock* block = blockMap_.lookup(pc);
    if (!block) {
      return false;
    }
    return block->lookupTrap(pc, trapOut, bytecode);
  }
  const CodeRangeUnwindInfo* lookupUnwindInfo(void* pc) const {
    const CodeBlock* block = blockMap_.lookup(pc);
    if (!block) {
      return nullptr;
    }
    return block->lookupUnwindInfo(pc);
  }
  bool lookupFunctionTier(const CodeRange* codeRange, Tier* tier) const;

  // To save memory, profilingLabels_ are generated lazily when profiling mode
  // is enabled.

  void ensureProfilingLabels(bool profilingEnabled) const;
  const char* profilingLabel(uint32_t funcIndex) const;

  // Wasm disassembly support

  void disassemble(JSContext* cx, Tier tier, int kindSelection,
                   PrintCallback printString) const;

  // Wasm metadata size analysis
  MetadataAnalysisHashMap metadataAnalysis(JSContext* cx) const;

  // about:memory reporting:

  void addSizeOfMiscIfNotSeen(
      mozilla::MallocSizeOf mallocSizeOf, CodeMetadata::SeenSet* seenCodeMeta,
      CodeMetadataForAsmJS::SeenSet* seenCodeMetaForAsmJS,
      Code::SeenSet* seenCode, size_t* code, size_t* data) const;

  size_t tier1CodeMemoryUsed() const {
    return completeTier1_->segment->capacityBytes();
  }

  WASM_DECLARE_FRIEND_SERIALIZE_ARGS(SharedCode,
                                     const wasm::LinkData& sharedStubsLinkData,
                                     const wasm::LinkData& optimizedLinkData);
};

void PatchDebugSymbolicAccesses(uint8_t* codeBase, jit::MacroAssembler& masm);

// Allocate executable memory from the pool in `lazySegments`, or if none of
// those have space, create a new Segment, add it to the vector, and allocate
// from that.  `*roundedUpAllocationSize` returns the actual allocation size;
// it is guaranteed to be a multiple of the machine's page size.
SharedCodeSegment AllocateCodePagesFrom(SharedCodeSegmentVector& lazySegments,
                                        uint32_t bytesNeeded,
                                        size_t* offsetInSegment,
                                        size_t* roundedUpAllocationSize);

}  // namespace wasm
}  // namespace js

#endif  // wasm_code_h
