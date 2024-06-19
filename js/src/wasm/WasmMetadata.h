/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef wasm_WasmMetadata_h
#define wasm_WasmMetadata_h

#include "wasm/WasmBinaryTypes.h"
#include "wasm/WasmInstanceData.h"
#include "wasm/WasmModuleTypes.h"
#include "wasm/WasmProcess.h"

namespace js::wasm {

// ModuleMetadata contains all the state necessary to process or render
// functions, and all of the state necessary to validate all aspects of the
// functions.
//
// A ModuleMetadata is created by decoding all the sections before the wasm
// code section and then used immutably during. When compiling a module using a
// ModuleGenerator, the ModuleMetadata holds state shared between the
// ModuleGenerator thread and background compile threads. All the threads
// are given a read-only view of the ModuleMetadata, thus preventing race
// conditions.

struct ModuleMetadata;

struct CodeMetadata {
  // Constant parameters for the entire compilation:
  const ModuleKind kind;
  const FeatureArgs features;

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

  // OpIter needs to know types of functions for calls. This will increase size
  // of Code/Metadata compared to before. We can probably shrink this class by
  // removing typeIndex/typePtr redundancy and move the flag to ModuleEnv.
  //
  // We could also manually clear this vector when we're in a mode that is
  // not doing partial tiering.
  FuncDescVector funcs;

  // FIXME extra stuff to do here?  Ryan writes:
  // OpIter needs to know just the type and count of elem segments, but not
  // their payload. So we should split this up to avoid a major size regression
  // in Code/Metadata.
  ModuleElemSegmentVector elemSegments;

  // asm.js needs this for compilation, technically a size regression for
  // Code/Metadata but likely not major and we don't care about asm.js enough
  // to optimize for this.
  Uint32Vector asmJSSigToTableIndex;

  // Bytecode ranges for the code section?  FIXME check
  MaybeSectionRange
      codeSection;  // !!!! moved, but the doc doesn't specify that

  // Bytecode ranges for custom sections?  FIXME check
  CustomSectionRangeVector customSectionRanges;  // !!!! also moved

  // Indicates whether the branch hint section was successfully parsed.
  bool parsedBranchHints;

  explicit CodeMetadata(FeatureArgs features,
                        ModuleKind kind = ModuleKind::Wasm)
      : kind(kind),
        features(features),
        numFuncImports(0),
        numGlobalImports(0),
        funcImportsOffsetStart(UINT32_MAX),
        typeDefsOffsetStart(UINT32_MAX),
        memoriesOffsetStart(UINT32_MAX),
        tablesOffsetStart(UINT32_MAX),
        tagsOffsetStart(UINT32_MAX),
        parsedBranchHints(false) {}

  [[nodiscard]] bool init() {
    types = js_new<TypeContext>(features);
    return types;
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

  // Lay out the instance.
  // If successful, return the total instance data length.
  [[nodiscard]] Maybe<uint32_t> doInstanceLayout();

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
      /*MOD*/ ModuleMetadata* meta, ValTypeVector&& params,
      ValTypeVector&& results, bool declareForRef = false,
      Maybe<CacheableName>&& optionalExportedName = mozilla::Nothing());

  bool addImportedFunc(/*MOD*/ ModuleMetadata* meta, ValTypeVector&& params,
                       ValTypeVector&& results, CacheableName&& importModName,
                       CacheableName&& importFieldName);
};

struct ModuleMetadata {
  // Module fields decoded from the module environment (or initialized while
  // validating an asm.js module) and immutable during compilation:
  ImportVector imports;
  ExportVector exports;
  Maybe<uint32_t> startFuncIndex;
  // MaybeSectionRange codeSection; !!!! moved, but the doc doesn't specify that

  // Fields decoded as part of the wasm module tail:
  DataSegmentRangeVector dataSegmentRanges;
  // CustomSectionRangeVector customSectionRanges; !!!! also moved
  //                          ^ was moved because Decoder needs it
  Maybe<uint32_t> nameCustomSectionIndex;
  Maybe<Name> moduleName;
  NameVector funcNames;

  explicit ModuleMetadata() {}
};

}  // namespace js::wasm

#endif /* wasm_WasmMetadata_h */
