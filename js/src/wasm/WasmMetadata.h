/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef wasm_WasmMetadata_h
#define wasm_WasmMetadata_h

#include "wasm/WasmBinaryTypes.h"
#include "wasm/WasmInstanceData.h"  // various of *InstanceData
#include "wasm/WasmModuleTypes.h"
#include "wasm/WasmProcess.h"  // IsHugeMemoryEnabled

namespace js {
namespace wasm {

// A FuncExport represents a single function definition inside a wasm Module
// that has been exported one or more times. A FuncExport represents an
// internal entry point that can be called via function definition index by
// Instance::callExport(). To allow O(log(n)) lookup of a FuncExport by
// function definition index, the FuncExportVector is stored sorted by
// function definition index.

class FuncExport {
  uint32_t funcIndex_;
  uint32_t eagerInterpEntryOffset_;  // Machine code offset
  bool hasEagerStubs_;

  WASM_CHECK_CACHEABLE_POD(funcIndex_, eagerInterpEntryOffset_,
                           hasEagerStubs_);

 public:
  FuncExport() = default;
  explicit FuncExport(uint32_t funcIndex,
                      bool hasEagerStubs) {
    funcIndex_ = funcIndex;
    eagerInterpEntryOffset_ = UINT32_MAX;
    hasEagerStubs_ = hasEagerStubs;
  }
  void initEagerInterpEntryOffset(uint32_t entryOffset) {
    MOZ_ASSERT(eagerInterpEntryOffset_ == UINT32_MAX);
    MOZ_ASSERT(hasEagerStubs());
    eagerInterpEntryOffset_ = entryOffset;
  }

  bool hasEagerStubs() const { return hasEagerStubs_; }
  uint32_t funcIndex() const { return funcIndex_; }
  uint32_t eagerInterpEntryOffset() const {
    MOZ_ASSERT(eagerInterpEntryOffset_ != UINT32_MAX);
    MOZ_ASSERT(hasEagerStubs());
    return eagerInterpEntryOffset_;
  }
};

WASM_DECLARE_CACHEABLE_POD(FuncExport);

using FuncExportVector = Vector<FuncExport, 0, SystemAllocPolicy>;

// A FuncImport contains the runtime metadata needed to implement a call to an
// imported function. Each function import has two call stubs: an optimized path
// into JIT code and a slow path into the generic C++ js::Invoke and these
// offsets of these stubs are stored so that function-import callsites can be
// dynamically patched at runtime.

class FuncImport {
 private:
  uint32_t instanceOffset_;
  uint32_t interpExitCodeOffset_;  // Machine code offset
  uint32_t jitExitCodeOffset_;     // Machine code offset

  WASM_CHECK_CACHEABLE_POD(instanceOffset_, interpExitCodeOffset_,
                           jitExitCodeOffset_);

 public:
  FuncImport()
      : instanceOffset_(0),
        interpExitCodeOffset_(0),
        jitExitCodeOffset_(0) {}

  explicit FuncImport(uint32_t instanceOffset) {
    instanceOffset_ = instanceOffset;
    interpExitCodeOffset_ = 0;
    jitExitCodeOffset_ = 0;
  }

  void initInterpExitOffset(uint32_t off) {
    MOZ_ASSERT(!interpExitCodeOffset_);
    interpExitCodeOffset_ = off;
  }
  void initJitExitOffset(uint32_t off) {
    MOZ_ASSERT(!jitExitCodeOffset_);
    jitExitCodeOffset_ = off;
  }

  uint32_t instanceOffset() const { return instanceOffset_; }
  uint32_t interpExitCodeOffset() const { return interpExitCodeOffset_; }
  uint32_t jitExitCodeOffset() const { return jitExitCodeOffset_; }
};

WASM_DECLARE_CACHEABLE_POD(FuncImport)

using FuncImportVector = Vector<FuncImport, 0, SystemAllocPolicy>;

// ==== Printing of names
//
// The Developer-Facing Display Conventions section of the WebAssembly Web
// API spec defines two cases for displaying a wasm function name:
//  1. the function name stands alone
//  2. the function name precedes the location

enum class NameContext { Standalone, BeforeLocation };

// wasm::CodeMetadata contains metadata whose lifetime ends at the same time
// that the lifetime of wasm::Code ends.  This encompasses a wide variety of
// uses.  In practice that means metadata needed for any and all aspects of
// compilation or execution of wasm code.  Hence this metadata conceptually
// belongs to, and is kept alive by, wasm::Code.  Note also that wasm::Code is
// in turn kept alive by wasm::Instance(s), hence this metadata will be kept
// alive as long as any instance for it exists.

using ModuleHash = uint8_t[8];

struct CodeMetadata : public ShareableBase<CodeMetadata> {
  // NOTE: if you add, remove, rename or reorder fields here, be sure to
  // update CodeCodeMetadata() to keep it in sync.

  // Constant parameters for the entire compilation.  These are not marked
  // `const` only because it breaks constructor delegation in
  // CodeMetadata::CodeMetadata, which is a shame.
  ModuleKind kind;
  FeatureArgs features;

  // Module fields decoded from the module environment (or initialized while
  // validating an asm.js module) and immutable during compilation:
  Maybe<uint32_t> dataCount;
  MemoryDescVector memories;
  MutableTypeContext types;
  BranchHintCollection branchHints;

  uint32_t numFuncImports;
  uint32_t numGlobalImports;
  GlobalDescVector globals;
  TagDescVector tags;
  TableDescVector tables;

  // ==== Fields relating to Instance layout
  //
  // The start offset of the FuncDefInstanceData[] section of the instance
  // data. There is one entry for every function definition.
  uint32_t funcDefsOffsetStart;
  // The start offset of the FuncImportInstanceData[] section of the instance
  // data. There is one entry for every imported function.
  uint32_t funcImportsOffsetStart;
  // The start offset of the TypeDefInstanceData[] section of the instance
  // data. There is one entry for every type.
  uint32_t typeDefsOffsetStart;
  // The start offset of the MemoryInstanceData[] section of the instance data.
  // There is one entry for every memory.
  uint32_t memoriesOffsetStart;
  // The start offset of the TableInstanceData[] section of the instance data.
  // There is one entry for every table.
  uint32_t tablesOffsetStart;
  // The start offset of the tag section of the instance data. There is one
  // entry for every tag.
  uint32_t tagsOffsetStart;
  // The total size of the instance data.
  uint32_t instanceDataLength;

  // ==== Names of things
  //
  bool filenameIsURL;
  CacheableChars filename;
  CacheableChars sourceMapURL;
  // namePayload points at the name section's CustomSection::payload so that
  // the Names (which are use payload-relative offsets) can be used
  // independently of the Module without duplicating the name section.  This
  // is marked `mutable` only because CodeModule (WasmSerialize.cpp) has to
  // update the field during deserialization, after the containing
  // CodeMetadata has been deserialized and marked `const`.
  mutable SharedBytes namePayload;
  Maybe<Name> moduleName;
  NameVector funcNames;

  // ==== Misc Maybes
  //
  Maybe<uint32_t> startFuncIndex;
  Maybe<uint32_t> nameCustomSectionIndex;

  // Info about all functions (import and locally defined) in the module.
  FuncDescVector funcs;

  // Info about elem segments needed only for validation and compilation.
  // Should have the same length as ModuleMetadata::elemSegments, and each
  // entry here should be identical the corresponding .elemType field in
  // ModuleMetadata::elemSegments.
  RefTypeVector elemSegmentTypes;

  // asm.js needs this for compilation, technically a size regression for
  // Code/Metadata but likely not major and we don't care about asm.js enough
  // to optimize for this.
  Uint32Vector asmJSSigToTableIndex;

  // Bytecode ranges for the code section.
  MaybeSectionRange codeSection;

  // Bytecode ranges for custom sections.  FIXME: is it feasible to move this
  // to ModuleMetadata?
  CustomSectionRangeVector customSectionRanges;

  // Indicates whether the branch hint section was successfully parsed.
  bool parsedBranchHints;

  // Debug-enabled code is not serialized.
  bool debugEnabled;
  ModuleHash debugHash;

  explicit CodeMetadata()
      : kind(ModuleKind::Wasm),
        features(),
        numFuncImports(0),
        numGlobalImports(0),
        funcDefsOffsetStart(UINT32_MAX),
        funcImportsOffsetStart(UINT32_MAX),
        typeDefsOffsetStart(UINT32_MAX),
        memoriesOffsetStart(UINT32_MAX),
        tablesOffsetStart(UINT32_MAX),
        tagsOffsetStart(UINT32_MAX),
        instanceDataLength(0),
        filenameIsURL(false),
        parsedBranchHints(false),
        debugEnabled(false),
        debugHash() {}

  explicit CodeMetadata(FeatureArgs features_,
                        ModuleKind kind_ = ModuleKind::Wasm)
      : CodeMetadata() {
    features = features_;
    kind = kind_;
  }

  [[nodiscard]] bool init() {
    MOZ_ASSERT(!types);
    types = js_new<TypeContext>(features);
    return types;
  }

  const TypeDef& getFuncTypeDef(uint32_t funcIndex) const {
    return types->type(funcs[funcIndex].typeIndex);
  }
  const FuncType& getFuncType(uint32_t funcIndex) const {
    return getFuncTypeDef(funcIndex).funcType();
  }

  size_t numTables() const { return tables.length(); }
  size_t numTypes() const { return types->length(); }
  size_t numFuncs() const { return funcs.length(); }
  size_t numFuncDefs() const { return funcs.length() - numFuncImports; }

  bool funcIsImport(uint32_t funcIndex) const {
    return funcIndex < numFuncImports;
  }
  size_t numMemories() const { return memories.length(); }

#define WASM_FEATURE(NAME, SHORT_NAME, ...) \
  bool SHORT_NAME##Enabled() const { return features.SHORT_NAME; }
  JS_FOR_WASM_FEATURES(WASM_FEATURE)
#undef WASM_FEATURE
  Shareable sharedMemoryEnabled() const { return features.sharedMemory; }
  bool simdAvailable() const { return features.simd; }

  bool isAsmJS() const { return kind == ModuleKind::AsmJS; }
  // A builtin module is a host constructed wasm module that exports host
  // functionality, using special opcodes. Otherwise, it has the same rules
  // as wasm modules and so it does not get a new ModuleKind.
  bool isBuiltinModule() const { return features.isBuiltinModule; }

  bool hugeMemoryEnabled(uint32_t memoryIndex) const {
    return !isAsmJS() && memoryIndex < memories.length() &&
           IsHugeMemoryEnabled(memories[memoryIndex].indexType());
  }
  bool usesSharedMemory(uint32_t memoryIndex) const {
    return memoryIndex < memories.length() && memories[memoryIndex].isShared();
  }

  void declareFuncExported(uint32_t funcIndex, bool eager, bool canRefFunc) {
    FuncFlags flags = funcs[funcIndex].flags;

    // Set the `Exported` flag, if not set.
    flags = FuncFlags(uint8_t(flags) | uint8_t(FuncFlags::Exported));

    // Merge in the `Eager` and `CanRefFunc` flags, if they're set. Be sure
    // to not unset them if they've already been set.
    if (eager) {
      flags = FuncFlags(uint8_t(flags) | uint8_t(FuncFlags::Eager));
    }
    if (canRefFunc) {
      flags = FuncFlags(uint8_t(flags) | uint8_t(FuncFlags::CanRefFunc));
    }

    funcs[funcIndex].flags = flags;
  }

  // ==== Methods relating to Instance layout
  //
  // Allocate space for `bytes`@`align` in the instance, updating
  // `instanceDataLength` and returning the offset in in `assignedOffset`.
  [[nodiscard]] bool allocateInstanceDataBytes(uint32_t bytes, uint32_t align,
                                               uint32_t* assignedOffset);
  // The same for an array of allocations.
  [[nodiscard]] bool allocateInstanceDataBytesN(uint32_t bytes, uint32_t align,
                                                uint32_t count,
                                                uint32_t* assignedOffset);
  // Lay out the instance, writing results into `typeDefsOffsetStart`,
  // `funcImportsOffsetStart`, `memoriesOffsetStart`, `tablesOffsetStart`,
  // `tagsOffsetStart`, and, for each global, its `GlobalDesc::offset_`.  The
  // total data length is also recorded in `instanceDataLength`.
  [[nodiscard]] bool initInstanceLayout(CompileMode mode);

  uint32_t offsetOfFuncDefInstanceData(uint32_t funcIndex) const {
    MOZ_ASSERT(funcIndex >= numFuncImports && funcIndex < numFuncs());
    return funcDefsOffsetStart +
           (funcIndex - numFuncImports) * sizeof(FuncDefInstanceData);
  }

  uint32_t offsetOfFuncImportInstanceData(uint32_t funcIndex) const {
    MOZ_ASSERT(funcIndex < numFuncImports);
    return funcImportsOffsetStart + funcIndex * sizeof(FuncImportInstanceData);
  }

  uint32_t offsetOfTypeDefInstanceData(uint32_t typeIndex) const {
    MOZ_ASSERT(typeIndex < types->length());
    return typeDefsOffsetStart + typeIndex * sizeof(TypeDefInstanceData);
  }

  uint32_t offsetOfTypeDef(uint32_t typeIndex) const {
    return offsetOfTypeDefInstanceData(typeIndex) +
           offsetof(TypeDefInstanceData, typeDef);
  }
  uint32_t offsetOfSuperTypeVector(uint32_t typeIndex) const {
    return offsetOfTypeDefInstanceData(typeIndex) +
           offsetof(TypeDefInstanceData, superTypeVector);
  }

  uint32_t offsetOfMemoryInstanceData(uint32_t memoryIndex) const {
    MOZ_ASSERT(memoryIndex < memories.length());
    return memoriesOffsetStart + memoryIndex * sizeof(MemoryInstanceData);
  }
  uint32_t offsetOfTableInstanceData(uint32_t tableIndex) const {
    MOZ_ASSERT(tableIndex < tables.length());
    return tablesOffsetStart + tableIndex * sizeof(TableInstanceData);
  }

  uint32_t offsetOfTagInstanceData(uint32_t tagIndex) const {
    MOZ_ASSERT(tagIndex < tags.length());
    return tagsOffsetStart + tagIndex * sizeof(TagInstanceData);
  }

  bool addDefinedFunc(
      ModuleMetadata* meta, ValTypeVector&& params, ValTypeVector&& results,
      bool declareForRef = false,
      Maybe<CacheableName>&& optionalExportedName = mozilla::Nothing());

  bool addImportedFunc(ModuleMetadata* meta, ValTypeVector&& params,
                       ValTypeVector&& results, CacheableName&& importModName,
                       CacheableName&& importFieldName);

  // This gets names for wasm only.
  // For asm.js, see CodeMetadataForAsmJS::getFuncNameForAsmJS.
  bool getFuncNameForWasm(NameContext ctx, uint32_t funcIndex,
                          UTF8Bytes* name) const;

  size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
};

using MutableCodeMetadata = RefPtr<CodeMetadata>;
using SharedCodeMetadata = RefPtr<const CodeMetadata>;

// wasm::ModuleMetadata contains metadata whose lifetime ends at the same time
// that the lifetime of wasm::Module ends.  In practice that means metadata
// that is needed only for creating wasm::Instances.  Hence this metadata
// conceptually belongs to, and is held alive by, wasm::Module.

struct ModuleMetadata : public ShareableBase<ModuleMetadata> {
  // NOTE: if you add, remove, rename or reorder fields here, be sure to
  // update CodeModuleMetadata() to keep it in sync.

  // The subset of module metadata that is shared between a module and
  // instance.
  MutableCodeMetadata codeMeta;

  // Module fields decoded from the module environment (or initialized while
  // validating an asm.js module) and immutable during compilation:
  ImportVector imports;
  ExportVector exports;

  // Info about elem segments needed for instantiation.  Should have the same
  // length as CodeMetadata::elemSegmentTypes.
  ModuleElemSegmentVector elemSegments;

  // Info about data segments needed for instantiation.  These wind up having
  // the same length.  Initially both are empty.  `dataSegmentRanges` is
  // filled in during validation, and `dataSegments` remains empty.  Later, at
  // module-generation time, `dataSegments` is filled in, by copying the
  // underlying data blocks, and so the two vectors have the same length after
  // that.
  DataSegmentRangeVector dataSegmentRanges;
  DataSegmentVector dataSegments;

  CustomSectionVector customSections;

  // Which features were observed when compiling this module.
  FeatureUsage featureUsage;

  explicit ModuleMetadata() = default;

  [[nodiscard]] bool init() {
    codeMeta = js_new<CodeMetadata>();
    return !!codeMeta && codeMeta->init();
  }
  [[nodiscard]] bool init(FeatureArgs features,
                          ModuleKind kind = ModuleKind::Wasm) {
    codeMeta = js_new<CodeMetadata>(features, kind);
    return !!codeMeta && codeMeta->init();
  }

  size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
};

using MutableModuleMetadata = RefPtr<ModuleMetadata>;
using SharedModuleMetadata = RefPtr<const ModuleMetadata>;

}  // namespace wasm
}  // namespace js

#endif /* wasm_WasmMetadata_h */
