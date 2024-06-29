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

using js::CodeMetadataForAsmJS;

struct JS_PUBLIC_API JSContext;
class JSFunction;

namespace js {

namespace jit {
class MacroAssembler;
};

namespace wasm {

// LinkData contains all the metadata necessary to patch all the locations
// that depend on the absolute address of a ModuleSegment. This happens in a
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
  explicit LinkData(Tier tier) : tier(tier) {}

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

  struct SymbolicLinkArray : EnumeratedArray<SymbolicAddress, Uint32Vector,
                                             size_t(SymbolicAddress::Limit)> {
    size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
  };

  const Tier tier;
  InternalLinkVector internalLinks;
  SymbolicLinkArray symbolicLinks;

  size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
};

WASM_DECLARE_CACHEABLE_POD(LinkData::InternalLink);

using UniqueLinkData = UniquePtr<LinkData>;

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
class ModuleSegment;
class LazyStubSegment;

using UniqueCodeBlock = UniquePtr<CodeBlock>;
using UniqueConstCodeBlock = UniquePtr<const CodeBlock>;
using UniqueCodeBlockVector = Vector<UniqueCodeBlock, 0, SystemAllocPolicy>;

// CodeSegment contains common helpers for determining the base and length of a
// code segment and if a pc belongs to this segment. It is inherited by:
// - ModuleSegment, i.e. the code segment of a Module, generated
// eagerly when a Module is instanciated.
// - LazyStubSegment, i.e. the code segment of entry stubs that are lazily
// generated.

class CodeSegment : public ShareableBase<CodeSegment> {
 protected:
  enum class Kind { LazyStubs, Module };

  CodeSegment(UniqueCodeBytes bytes, uint32_t length, Kind kind)
      : bytes_(std::move(bytes)),
        length_(length),
        kind_(kind),
        code_(nullptr) {}

  bool initialize(const Code& code);

 private:
  const UniqueCodeBytes bytes_;
  const uint32_t length_;
  const Kind kind_;
  const Code* code_;

 public:
  bool initialized() const { return !!code_; }

  bool isLazyStubs() const { return kind_ == Kind::LazyStubs; }
  bool isModule() const { return kind_ == Kind::Module; }
  const ModuleSegment* asModule() const {
    MOZ_ASSERT(isModule());
    return (ModuleSegment*)this;
  }
  ModuleSegment* asModule() {
    MOZ_ASSERT(isModule());
    return (ModuleSegment*)this;
  }
  const LazyStubSegment* asLazyStub() const {
    MOZ_ASSERT(isLazyStubs());
    return (LazyStubSegment*)this;
  }
  LazyStubSegment* asLazyStub() {
    MOZ_ASSERT(isLazyStubs());
    return (LazyStubSegment*)this;
  }

  uint8_t* base() const { return bytes_.get(); }
  uint32_t length() const {
    MOZ_ASSERT(length_ != UINT32_MAX);
    return length_;
  }

  const Code& code() const { return *code_; }

  void addSizeOfMisc(MallocSizeOf mallocSizeOf, size_t* code) const;
};

using SharedCodeSegment = RefPtr<CodeSegment>;

// A wasm ModuleSegment owns the allocated executable code for a wasm module.

using SharedModuleSegment = RefPtr<ModuleSegment>;

class ModuleSegment : public CodeSegment {
  const Tier tier_;
  uint8_t* const trapCode_;

 public:
  ModuleSegment(Tier tier, UniqueCodeBytes codeBytes, uint32_t codeLength,
                const LinkData& linkData);

  static SharedModuleSegment create(Tier tier, jit::MacroAssembler& masm,
                                    const LinkData& linkData);
  static SharedModuleSegment create(Tier tier, const Bytes& unlinkedBytes,
                                    const LinkData& linkData);

  bool initialize(const CodeBlock& codeBlock, const LinkData& linkData,
                  const CodeMetadata& codeMeta,
                  const CodeMetadataForAsmJS* codeMetaForAsmJS);

  Tier tier() const { return tier_; }

  const CodeBlock& codeBlock() const;

  // Pointers to stubs to which PC is redirected from the signal-handler.

  uint8_t* trapCode() const { return trapCode_; }

  const CodeRange* lookupRange(const void* pc) const;

  void addSizeOfMisc(mozilla::MallocSizeOf mallocSizeOf, size_t* code,
                     size_t* data) const;

  WASM_DECLARE_FRIEND_SERIALIZE(ModuleSegment);
};

extern UniqueCodeBytes AllocateCodeBytes(
    mozilla::Maybe<jit::AutoMarkJitCodeWritableForThread>& writable,
    uint32_t codeLength);
extern bool StaticallyLink(const ModuleSegment& ms, const LinkData& linkData);
extern void StaticallyUnlink(uint8_t* base, const LinkData& linkData);

// MetadataTier holds all the data that is needed to describe compiled wasm
// code at runtime (as opposed to data that is only used to statically link or
// instantiate a module), for one specific tier (baseline or Ion) of code.

// LazyStubSegment is a code segment lazily generated for function entry stubs
// (both interpreter and jit ones).
//
// Because a stub is usually small (a few KiB) and an executable code segment
// isn't (64KiB), a given stub segment can contain entry stubs of many
// functions.

using SharedLazyStubSegment = RefPtr<LazyStubSegment>;
using LazyStubSegmentVector =
    Vector<SharedLazyStubSegment, 0, SystemAllocPolicy>;

class LazyStubSegment : public CodeSegment {
  CodeRangeVector codeRanges_;
  size_t usedBytes_;

 public:
  LazyStubSegment(UniqueCodeBytes bytes, size_t length)
      : CodeSegment(std::move(bytes), length, CodeSegment::Kind::LazyStubs),
        usedBytes_(0) {}

  static SharedLazyStubSegment create(const Code& code, size_t codeLength);

  static size_t AlignBytesNeeded(size_t bytes) {
    return AlignBytes(bytes, gc::SystemPageSize());
  }

  bool hasSpace(size_t bytes) const;
  [[nodiscard]] bool addStubs(const CodeMetadata& codeMeta, size_t codeLength,
                              const Uint32Vector& funcExportIndices,
                              const FuncExportVector& funcExports,
                              const CodeRangeVector& codeRanges,
                              uint8_t** codePtr, size_t* offsetInSegment);

  const CodeRangeVector& codeRanges() const { return codeRanges_; }
  [[nodiscard]] const CodeRange* lookupRange(const void* pc) const;

  void addSizeOfMisc(MallocSizeOf mallocSizeOf, size_t* code,
                     size_t* data) const;
};

// LazyFuncExport helps to efficiently lookup a CodeRange from a given function
// index. It is inserted in a vector sorted by function index, to perform
// binary search on it later.

struct LazyFuncExport {
  size_t funcIndex;
  size_t lazyStubBlockIndex;
  size_t funcCodeRangeIndex;
  // Used to make sure we only upgrade a lazy stub from baseline to ion.
  mozilla::DebugOnly<Tier> tier;

  LazyFuncExport(size_t funcIndex, size_t lazyStubBlockIndex,
                 size_t funcCodeRangeIndex, Tier tier)
      : funcIndex(funcIndex),
        lazyStubBlockIndex(lazyStubBlockIndex),
        funcCodeRangeIndex(funcCodeRangeIndex),
        tier(tier) {}
};

using LazyFuncExportVector = Vector<LazyFuncExport, 0, SystemAllocPolicy>;

// LazyStubTier contains all the necessary information for lazy function entry
// stubs that are generated at runtime. None of its data are ever serialized.
//
// It must be protected by a lock, because the main thread can both read and
// write lazy stubs at any time while a background thread can regenerate lazy
// stubs for tier2 at any time.

class LazyStubTier {
  LazyStubSegmentVector stubSegments_;
  UniqueCodeBlockVector codeBlocks_;
  LazyFuncExportVector exports_;

  [[nodiscard]] bool createManyEntryStubs(const Uint32Vector& funcExportIndices,
                                          const CodeMetadata& codeMeta,
                                          const CodeBlock& tierCodeBlock,
                                          size_t* stubBlockIndex);

 public:
  LazyStubTier() = default;

  // Creates one lazy stub for the exported function, for which the jit entry
  // will be set to the lazily-generated one.
  [[nodiscard]] bool createOneEntryStub(uint32_t funcExportIndex,
                                        const CodeMetadata& codeMeta,
                                        const CodeBlock& tierCodeBlock);

  bool entryStubsEmpty() const { return codeBlocks_.empty(); }
  bool hasEntryStub(uint32_t funcIndex) const;

  // Returns a pointer to the raw interpreter entry of a given function for
  // which stubs have been lazily generated.
  [[nodiscard]] void* lookupInterpEntry(uint32_t funcIndex) const;

  // Create one lazy stub for all the functions in funcExportIndices, putting
  // them in a single stub. Jit entries won't be used until
  // setJitEntries() is actually called, after the Code owner has committed
  // tier2.
  [[nodiscard]] bool createTier2(const CodeMetadata& codeMeta,
                                 const CodeBlock& tierCodeBlock,
                                 Maybe<size_t>* outStubBlockIndex);
  void setJitEntries(const Maybe<size_t>& stubBlockIndex, const Code& code);

  void addSizeOfMisc(MallocSizeOf mallocSizeOf, size_t* code,
                     size_t* data) const;
};

// CodeBlock contains all the data related to a given compilation tier. It is
// built during module generation and then immutably stored in a Code.

enum class CodeBlockKind { BaselineTier, OptimizedTier, LazyStubs };

class CodeBlock {
 public:
  // Weak reference to the code that owns us, not serialized.
  const Code* code;

  // The following information is all serialized
  // Which kind of code is being stored in this block. Most consumers don't
  // care about this.
  const CodeBlockKind kind;

  // The code segment our code is inside, along with the range we're occupying.
  SharedCodeSegment segment;
  const uint8_t* codeBase;
  size_t codeLength;

  // Metadata about the code we've put in the segment. All offsets are
  // temporarily relative to the segment base, not our block base.
  Uint32Vector funcToCodeRange;
  CodeRangeVector codeRanges;
  CallSiteVector callSites;
  TrapSiteVectorArray trapSites;
  FuncImportVector funcImports;
  FuncExportVector funcExports;
  StackMaps stackMaps;
  TryNoteVector tryNotes;
  CodeRangeUnwindInfoVector codeRangeUnwindInfos;

  // Debug information, not serialized.
  uint32_t debugTrapOffset;

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
        kind(kind),
        debugTrapOffset(0),
        unregisterOnDestroy_(false) {}
  explicit CodeBlock(Tier tier) : CodeBlock(kindFromTier(tier)) {}
  ~CodeBlock();

  bool initialized() const { return !!code && segment->initialized(); }
  bool initialize(const Code& code, const LinkData* linkData,
                  const CodeMetadata& codeMeta,
                  const CodeMetadataForAsmJS* codeMetaForAsmJS);

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

  const uint8_t* base() const { return codeBase; }
  uint32_t length() const { return codeLength; }
  bool containsCodePC(const void* pc) const {
    return pc >= base() && pc < (base() + length());
  }

  const ModuleSegment& moduleSegment() const { return *segment->asModule(); }
  const LazyStubSegment& lazyStubSegment() const {
    return *segment->asLazyStub();
  }

  const CodeRange& codeRange(const FuncExport& funcExport) const {
    return codeRanges[funcToCodeRange[funcExport.funcIndex()]];
  }

  const CodeRange* lookupRange(const void* pc) const;
  const TryNote* lookupTryNote(const void* pc) const;
  FuncExport& lookupFuncExport(uint32_t funcIndex,
                               size_t* funcExportIndex = nullptr);
  const FuncExport& lookupFuncExport(uint32_t funcIndex,
                                     size_t* funcExportIndex = nullptr) const;

  void addSizeOfMisc(MallocSizeOf mallocSizeOf, size_t* code,
                     size_t* data) const;

  WASM_DECLARE_FRIEND_SERIALIZE_ARGS(CodeBlock, const wasm::LinkData& data);
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
  bool init(CompileMode mode, const ModuleSegment& ms,
            const CodeRangeVector& codeRanges);

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
    if (mode_ == CompileMode::Tier1) {
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
  // These have the same lifetime end as Code itself -- they can be dropped
  // when Code itself is dropped.  FIXME: should these be MutableCodeXX?
  //
  // This must always be non-null.
  SharedCodeMetadata codeMeta_;
  // This is null for a wasm module, non-null for asm.js
  SharedCodeMetadataForAsmJS codeMetaForAsmJS_;

  // [SMDOC] Tier-2 data
  //
  // hasTier2_ and tier2_ implement a three-state protocol for broadcasting
  // tier-2 data; this also amounts to a single-writer/multiple-reader setup.
  //
  // Initially hasTier2_ is false and tier2_ is null.
  //
  // While hasTier2_ is false, *no* thread may read tier2_, but one thread may
  // make tier2_ non-null (this will be the tier-2 compiler thread).  That same
  // thread must then later set hasTier2_ to true to broadcast the tier2_ value
  // and its availability.  Note that the writing thread may not itself read
  // tier2_ before setting hasTier2_, in order to simplify reasoning about
  // global invariants.
  //
  // Once hasTier2_ is true, *no* thread may write tier2_ and *no* thread may
  // read tier2_ without having observed hasTier2_ as true first.  Once
  // hasTier2_ is true, it stays true.
  mutable UniqueConstCodeBlock tier2_;
  mutable Atomic<bool> hasTier2_;

  UniqueCodeBlock tier1_;

  ExclusiveData<CacheableCharsVector> profilingLabels_;
  JumpTables jumpTables_;

  // Lazy stubs, not serialized.
  RWExclusiveData<LazyStubTier> lazyStubs_;

 public:
  Code(const CodeMetadata& codeMeta,
       const CodeMetadataForAsmJS* codeMetaForAsmJS, UniqueCodeBlock tier1,
       JumpTables&& maybeJumpTables);
  bool initialized() const { return tier1_->initialized(); }

  bool initialize(const LinkData& linkData);

  void setTieringEntry(size_t i, void* target) const {
    jumpTables_.setTieringEntry(i, target);
  }
  void** tieringJumpTable() const { return jumpTables_.tiering(); }

  void setJitEntry(size_t i, void* target) const {
    jumpTables_.setJitEntry(i, target);
  }
  void setJitEntryIfNull(size_t i, void* target) const {
    jumpTables_.setJitEntryIfNull(i, target);
  }
  void** getAddressOfJitEntry(size_t i) const {
    return jumpTables_.getAddressOfJitEntry(i);
  }
  uint32_t getFuncIndex(JSFunction* fun) const;

  // Install the tier2 code without committing it.  To maintain the invariant
  // that tier2_ is never accessed without the tier having been committed, this
  // returns a pointer to the installed tier that the caller can use for
  // subsequent operations.
  bool setAndBorrowTier2(UniqueCodeBlock tier2, const LinkData& linkData,
                         const CodeBlock** borrowedTier) const;
  void commitTier2() const;

  bool hasTier2() const { return hasTier2_; }
  Tiers tiers() const;
  bool hasTier(Tier t) const;

  Tier stableTier() const;  // This is stable during a run
  Tier bestTier()
      const;  // This may transition from Baseline -> Ion at any time

  const CodeBlock& codeBlock(Tier tier) const;
  const CodeMetadata& codeMeta() const { return *codeMeta_; }
  const CodeMetadataForAsmJS* codeMetaForAsmJS() const {
    return codeMetaForAsmJS_;
  }

  const ModuleSegment& segment(Tier iter) const {
    return *codeBlock(iter).segment->asModule();
  }

  const RWExclusiveData<LazyStubTier>& lazyStubs() const { return lazyStubs_; }

  // Metadata lookup functions:

  const CallSite* lookupCallSite(void* returnAddress) const;
  const CodeRange* lookupFuncRange(void* pc) const;
  const StackMap* lookupStackMap(uint8_t* nextPC) const;
  const TryNote* lookupTryNote(void* pc, Tier* tier) const;
  bool lookupTrap(void* pc, Trap* trap, BytecodeOffset* bytecode) const;
  const CodeRangeUnwindInfo* lookupUnwindInfo(void* pc) const;
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
      MallocSizeOf mallocSizeOf, CodeMetadata::SeenSet* seenCodeMeta,
      CodeMetadataForAsmJS::SeenSet* seenCodeMetaForAsmJS,
      Code::SeenSet* seenCode, size_t* code, size_t* data) const;

  WASM_DECLARE_FRIEND_SERIALIZE_ARGS(SharedCode, const wasm::LinkData& data);
};

void PatchDebugSymbolicAccesses(uint8_t* codeBase, jit::MacroAssembler& masm);

}  // namespace wasm
}  // namespace js

#endif  // wasm_code_h
