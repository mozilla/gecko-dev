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

#include "wasm/WasmCode.h"

#include "mozilla/Atomics.h"
#include "mozilla/BinarySearch.h"
#include "mozilla/EnumeratedRange.h"
#include "mozilla/Sprintf.h"

#include <algorithm>

#include "jsnum.h"

#include "jit/Disassemble.h"
#include "jit/ExecutableAllocator.h"
#include "jit/FlushICache.h"  // for FlushExecutionContextForAllThreads
#include "jit/MacroAssembler.h"
#include "jit/PerfSpewer.h"
#include "util/Poison.h"
#include "vm/HelperThreadState.h"  // PartialTier2CompileTask
#ifdef MOZ_VTUNE
#  include "vtune/VTuneWrapper.h"
#endif
#include "wasm/WasmModule.h"
#include "wasm/WasmProcess.h"
#include "wasm/WasmSerialize.h"
#include "wasm/WasmStubs.h"
#include "wasm/WasmUtility.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;
using mozilla::Atomic;
using mozilla::BinarySearch;
using mozilla::BinarySearchIf;
using mozilla::DebugOnly;
using mozilla::MakeEnumeratedRange;
using mozilla::MallocSizeOf;
using mozilla::Maybe;

size_t LinkData::SymbolicLinkArray::sizeOfExcludingThis(
    MallocSizeOf mallocSizeOf) const {
  size_t size = 0;
  for (const Uint32Vector& offsets : *this) {
    size += offsets.sizeOfExcludingThis(mallocSizeOf);
  }
  return size;
}

static uint32_t RoundupExecutableCodePageSize(uint32_t codeLength) {
  // AllocateExecutableMemory() requires a multiple of ExecutableCodePageSize.
  return RoundUp(codeLength, ExecutableCodePageSize);
}

UniqueCodeBytes wasm::AllocateCodeBytes(
    Maybe<AutoMarkJitCodeWritableForThread>& writable, uint32_t codeLength,
    bool allowLastDitchGC) {
  if (codeLength > MaxCodeBytesPerProcess) {
    return nullptr;
  }

  static_assert(MaxCodeBytesPerProcess <= INT32_MAX, "rounding won't overflow");
  uint32_t roundedCodeLength = RoundupExecutableCodePageSize(codeLength);

  void* p =
      AllocateExecutableMemory(roundedCodeLength, ProtectionSetting::Writable,
                               MemCheckKind::MakeUndefined);

  // If the allocation failed and the embedding gives us a last-ditch attempt
  // to purge all memory (which, in gecko, does a purging GC/CC/GC), do that
  // then retry the allocation.
  if (!p && allowLastDitchGC) {
    if (OnLargeAllocationFailure) {
      OnLargeAllocationFailure();
      p = AllocateExecutableMemory(roundedCodeLength,
                                   ProtectionSetting::Writable,
                                   MemCheckKind::MakeUndefined);
    }
  }

  if (!p) {
    return nullptr;
  }

  // Construct AutoMarkJitCodeWritableForThread after allocating memory, to
  // ensure it's not nested (OnLargeAllocationFailure can trigger GC).
  writable.emplace();

  // Zero the padding.
  memset(((uint8_t*)p) + codeLength, 0, roundedCodeLength - codeLength);

  // We account for the bytes allocated in WasmModuleObject::create, where we
  // have the necessary JSContext.

  return UniqueCodeBytes((uint8_t*)p, FreeCode(roundedCodeLength));
}

void FreeCode::operator()(uint8_t* bytes) {
  MOZ_ASSERT(codeLength);
  MOZ_ASSERT(codeLength == RoundupExecutableCodePageSize(codeLength));

#ifdef MOZ_VTUNE
  vtune::UnmarkBytes(bytes, codeLength);
#endif
  DeallocateExecutableMemory(bytes, codeLength);
}

bool wasm::StaticallyLink(jit::AutoMarkJitCodeWritableForThread& writable,
                          uint8_t* base, const LinkData& linkData,
                          const Code* maybeCode) {
  if (!EnsureBuiltinThunksInitialized(writable)) {
    return false;
  }

  for (LinkData::InternalLink link : linkData.internalLinks) {
    CodeLabel label;
    label.patchAt()->bind(link.patchAtOffset);
    label.target()->bind(link.targetOffset);
#ifdef JS_CODELABEL_LINKMODE
    label.setLinkMode(static_cast<CodeLabel::LinkMode>(link.mode));
#endif
    Assembler::Bind(base, label);
  }

  for (CallFarJump far : linkData.callFarJumps) {
    MOZ_ASSERT(maybeCode && maybeCode->mode() == CompileMode::LazyTiering);
    const CodeBlock& bestBlock = maybeCode->funcCodeBlock(far.targetFuncIndex);
    uint32_t stubRangeIndex = bestBlock.funcToCodeRange[far.targetFuncIndex];
    const CodeRange& stubRange = bestBlock.codeRanges[stubRangeIndex];
    uint8_t* stubBase = bestBlock.segment->base();
    MacroAssembler::patchFarJump(base + far.jumpOffset,
                                 stubBase + stubRange.funcUncheckedCallEntry());
  }

  for (auto imm : MakeEnumeratedRange(SymbolicAddress::Limit)) {
    const Uint32Vector& offsets = linkData.symbolicLinks[imm];
    if (offsets.empty()) {
      continue;
    }

    void* target = SymbolicAddressTarget(imm);
    for (uint32_t offset : offsets) {
      uint8_t* patchAt = base + offset;
      Assembler::PatchDataWithValueCheck(CodeLocationLabel(patchAt),
                                         PatchedImmPtr(target),
                                         PatchedImmPtr((void*)-1));
    }
  }

  return true;
}

void wasm::StaticallyUnlink(uint8_t* base, const LinkData& linkData) {
  for (LinkData::InternalLink link : linkData.internalLinks) {
    CodeLabel label;
    label.patchAt()->bind(link.patchAtOffset);
    label.target()->bind(-size_t(base));  // to reset immediate to null
#ifdef JS_CODELABEL_LINKMODE
    label.setLinkMode(static_cast<CodeLabel::LinkMode>(link.mode));
#endif
    Assembler::Bind(base, label);
  }

  for (auto imm : MakeEnumeratedRange(SymbolicAddress::Limit)) {
    const Uint32Vector& offsets = linkData.symbolicLinks[imm];
    if (offsets.empty()) {
      continue;
    }

    void* target = SymbolicAddressTarget(imm);
    for (uint32_t offset : offsets) {
      uint8_t* patchAt = base + offset;
      Assembler::PatchDataWithValueCheck(CodeLocationLabel(patchAt),
                                         PatchedImmPtr((void*)-1),
                                         PatchedImmPtr(target));
    }
  }
}

static bool AppendToString(const char* str, UTF8Bytes* bytes) {
  return bytes->append(str, strlen(str)) && bytes->append('\0');
}

static void SendCodeRangesToProfiler(
    const uint8_t* segmentBase, const CodeMetadata& codeMeta,
    const CodeMetadataForAsmJS* codeMetaForAsmJS,
    const CodeRangeVector& codeRanges) {
  bool enabled = false;
  enabled |= PerfEnabled();
#ifdef MOZ_VTUNE
  enabled |= vtune::IsProfilingActive();
#endif
  if (!enabled) {
    return;
  }

  for (const CodeRange& codeRange : codeRanges) {
    if (!codeRange.hasFuncIndex()) {
      continue;
    }

    uintptr_t start = uintptr_t(segmentBase + codeRange.begin());
    uintptr_t size = codeRange.end() - codeRange.begin();

    UTF8Bytes name;
    bool ok;
    if (codeMetaForAsmJS) {
      ok = codeMetaForAsmJS->getFuncNameForAsmJS(codeRange.funcIndex(), &name);
    } else {
      ok = codeMeta.getFuncNameForWasm(NameContext::Standalone,
                                       codeRange.funcIndex(), &name);
    }
    if (!ok) {
      return;
    }

    // Avoid "unused" warnings
    (void)start;
    (void)size;

    if (PerfEnabled()) {
      const char* file = codeMeta.scriptedCaller().filename.get();
      if (codeRange.isFunction()) {
        if (!name.append('\0')) {
          return;
        }
        CollectPerfSpewerWasmFunctionMap(
            start, size, file,
            codeMeta.funcBytecodeOffset(codeRange.funcIndex()), name.begin());
      } else if (codeRange.isInterpEntry()) {
        if (!AppendToString(" slow entry", &name)) {
          return;
        }
        CollectPerfSpewerWasmMap(start, size, file, name.begin());
      } else if (codeRange.isJitEntry()) {
        if (!AppendToString(" fast entry", &name)) {
          return;
        }
        CollectPerfSpewerWasmMap(start, size, file, name.begin());
      } else if (codeRange.isImportInterpExit()) {
        if (!AppendToString(" slow exit", &name)) {
          return;
        }
        CollectPerfSpewerWasmMap(start, size, file, name.begin());
      } else if (codeRange.isImportJitExit()) {
        if (!AppendToString(" fast exit", &name)) {
          return;
        }
        CollectPerfSpewerWasmMap(start, size, file, name.begin());
      } else {
        MOZ_CRASH("unhandled perf hasFuncIndex type");
      }
    }
#ifdef MOZ_VTUNE
    if (!vtune::IsProfilingActive()) {
      continue;
    }
    if (!codeRange.isFunction()) {
      continue;
    }
    if (!name.append('\0')) {
      return;
    }
    vtune::MarkWasm(vtune::GenerateUniqueMethodID(), name.begin(), (void*)start,
                    size);
#endif
  }
}

size_t CodeSegment::AllocationAlignment() {
  // If we are write-protecting code, all new code allocations must be rounded
  // to the system page size.
  if (JitOptions.writeProtectCode) {
    return gc::SystemPageSize();
  }

  // Otherwise we can just use the standard JIT code alignment.
  return jit::CodeAlignment;
}

size_t CodeSegment::AlignAllocationBytes(uintptr_t bytes) {
  return AlignBytes(bytes, AllocationAlignment());
}

bool CodeSegment::IsAligned(uintptr_t bytes) {
  return bytes == AlignAllocationBytes(bytes);
}

bool CodeSegment::hasSpace(size_t bytes) const {
  MOZ_ASSERT(CodeSegment::IsAligned(bytes));
  return bytes <= capacityBytes() && lengthBytes_ <= capacityBytes() - bytes;
}

void CodeSegment::claimSpace(size_t bytes, uint8_t** claimedBase) {
  MOZ_RELEASE_ASSERT(hasSpace(bytes));
  *claimedBase = base() + lengthBytes_;
  lengthBytes_ += bytes;
}

bool CodeSegment::linkAndMakeExecutableSubRange(
    jit::AutoMarkJitCodeWritableForThread& writable, const LinkData& linkData,
    const Code* maybeCode, uint8_t* allocationStart, uint8_t* codeStart,
    uint32_t allocationLength) {
  MOZ_ASSERT(CodeSegment::IsAligned(uintptr_t(allocationStart)));
  MOZ_ASSERT(codeStart >= allocationStart);
  MOZ_ASSERT_IF(JitOptions.writeProtectCode,
                uintptr_t(allocationStart) % gc::SystemPageSize() == 0 &&
                    allocationLength % gc::SystemPageSize() == 0);

  if (!StaticallyLink(writable, codeStart, linkData, maybeCode)) {
    return false;
  }

  // Optimized compilation finishes on a background thread, so we must make sure
  // to flush the icaches of all the executing threads.
  // Reprotect the whole region to avoid having separate RW and RX mappings.
  return ExecutableAllocator::makeExecutableAndFlushICache(allocationStart,
                                                           allocationLength);
}

bool CodeSegment::linkAndMakeExecutableSubRange(
    jit::AutoMarkJitCodeWritableForThread& writable, jit::MacroAssembler& masm,
    uint8_t* allocationStart, uint8_t* codeStart, uint32_t allocationLength) {
  MOZ_ASSERT(CodeSegment::IsAligned(uintptr_t(allocationStart)));
  MOZ_ASSERT(codeStart >= allocationStart);
  MOZ_ASSERT_IF(JitOptions.writeProtectCode,
                uintptr_t(allocationStart) % gc::SystemPageSize() == 0 &&
                    allocationLength % gc::SystemPageSize() == 0);

  PatchDebugSymbolicAccesses(codeStart, masm);
  for (const CodeLabel& label : masm.codeLabels()) {
    Assembler::Bind(codeStart, label);
  }

  // Optimized compilation finishes on a background thread, so we must make sure
  // to flush the icaches of all the executing threads.
  // Reprotect the whole region to avoid having separate RW and RX mappings.
  return ExecutableAllocator::makeExecutableAndFlushICache(allocationStart,
                                                           allocationLength);
}

bool CodeSegment::linkAndMakeExecutable(
    jit::AutoMarkJitCodeWritableForThread& writable, const LinkData& linkData,
    const Code* maybeCode) {
  MOZ_ASSERT(base() == bytes_.get());
  return linkAndMakeExecutableSubRange(
      writable, linkData, maybeCode,
      /*allocationStart=*/base(), /*codeStart=*/base(),
      /*allocationLength=*/RoundupExecutableCodePageSize(lengthBytes()));
}

/* static */
SharedCodeSegment CodeSegment::createEmpty(size_t capacityBytes,
                                           bool allowLastDitchGC) {
  uint32_t codeLength = 0;
  uint32_t codeCapacity = RoundupExecutableCodePageSize(capacityBytes);
  Maybe<AutoMarkJitCodeWritableForThread> writable;
  UniqueCodeBytes codeBytes =
      AllocateCodeBytes(writable, codeCapacity, allowLastDitchGC);
  if (!codeBytes) {
    return nullptr;
  }

  return js_new<CodeSegment>(std::move(codeBytes), codeLength, codeCapacity);
}

/* static */
SharedCodeSegment CodeSegment::createFromMasm(MacroAssembler& masm,
                                              const LinkData& linkData,
                                              const Code* maybeCode,
                                              bool allowLastDitchGC) {
  uint32_t codeLength = masm.bytesNeeded();
  if (codeLength == 0) {
    return js_new<CodeSegment>(nullptr, 0, 0);
  }

  uint32_t codeCapacity = RoundupExecutableCodePageSize(codeLength);
  Maybe<AutoMarkJitCodeWritableForThread> writable;
  UniqueCodeBytes codeBytes =
      AllocateCodeBytes(writable, codeCapacity, allowLastDitchGC);
  if (!codeBytes) {
    return nullptr;
  }

  masm.executableCopy(codeBytes.get());

  SharedCodeSegment segment =
      js_new<CodeSegment>(std::move(codeBytes), codeLength, codeCapacity);
  if (!segment ||
      !segment->linkAndMakeExecutable(*writable, linkData, maybeCode)) {
    return nullptr;
  }

  return segment;
}

/* static */
SharedCodeSegment CodeSegment::createFromBytes(const uint8_t* unlinkedBytes,
                                               size_t unlinkedBytesLength,
                                               const LinkData& linkData,
                                               bool allowLastDitchGC) {
  uint32_t codeLength = unlinkedBytesLength;
  if (codeLength == 0) {
    return js_new<CodeSegment>(nullptr, 0, 0);
  }

  uint32_t codeCapacity = RoundupExecutableCodePageSize(codeLength);
  Maybe<AutoMarkJitCodeWritableForThread> writable;
  UniqueCodeBytes codeBytes =
      AllocateCodeBytes(writable, codeLength, allowLastDitchGC);
  if (!codeBytes) {
    return nullptr;
  }

  memcpy(codeBytes.get(), unlinkedBytes, unlinkedBytesLength);

  SharedCodeSegment segment =
      js_new<CodeSegment>(std::move(codeBytes), codeLength, codeCapacity);
  if (!segment ||
      !segment->linkAndMakeExecutable(*writable, linkData, nullptr)) {
    return nullptr;
  }
  return segment;
}

// When allocating a single stub to a page, we should not always place the stub
// at the beginning of the page as the stubs will tend to thrash the icache by
// creating conflicts (everything ends up in the same cache set).  Instead,
// locate stubs at different line offsets up to 3/4 the system page size (the
// code allocation quantum).
//
// This may be called on background threads, hence the atomic.
static uint32_t RandomPaddingForCodeLength(uint32_t codeLength) {
  // The counter serves only to spread the code out, it has no other meaning and
  // can wrap around.
  static mozilla::Atomic<uint32_t, mozilla::MemoryOrdering::ReleaseAcquire>
      counter(0);
  // We assume that the icache line size is 64 bytes, which is close to
  // universally true.
  const size_t cacheLineSize = 64;
  const size_t systemPageSize = gc::SystemPageSize();

  // If we're not write-protecting code, then we do not need to add any padding
  if (!JitOptions.writeProtectCode) {
    MOZ_ASSERT(CodeSegment::AllocationAlignment() != gc::SystemPageSize());
    return 0;
  }

  // Don't add more than 3/4 of a page of padding
  size_t maxPadBytes = ((systemPageSize * 3) / 4);
  size_t maxPadLines = maxPadBytes / cacheLineSize;

  // If code length is close to a page boundary, avoid pushing it to a new page
  size_t remainingBytesInPage =
      AlignBytes(codeLength, systemPageSize) - codeLength;
  size_t remainingLinesInPage = remainingBytesInPage / cacheLineSize;

  // Limit padding to the smallest of the above
  size_t padLinesAvailable = std::min(maxPadLines, remainingLinesInPage);

  // Don't add any padding if none is available
  if (padLinesAvailable == 0) {
    return 0;
  }

  uint32_t random = counter++;
  uint32_t padding = (random % padLinesAvailable) * cacheLineSize;
  // "adding on the padding area doesn't change the total number of pages
  //  required"
  MOZ_ASSERT(AlignBytes(codeLength + padding, systemPageSize) ==
             AlignBytes(codeLength, systemPageSize));
  return padding;
}

/* static */
SharedCodeSegment CodeSegment::claimSpaceFromPool(
    uint32_t codeLength, SharedCodeSegmentVector* segmentPool,
    bool allowLastDitchGC, uint8_t** allocationStartOut, uint8_t** codeStartOut,
    uint32_t* allocationLengthOut) {
  uint32_t paddingLength = RandomPaddingForCodeLength(codeLength);
  uint32_t allocationLength =
      CodeSegment::AlignAllocationBytes(paddingLength + codeLength);

  // Find a CodeSegment that has enough space. We just check the last code
  // segment in the pool for simplicity.
  if (segmentPool->length() == 0 ||
      !(*segmentPool)[segmentPool->length() - 1]->hasSpace(allocationLength)) {
    SharedCodeSegment newSegment =
        CodeSegment::createEmpty(allocationLength, allowLastDitchGC);
    if (!newSegment) {
      return nullptr;
    }
    if (!segmentPool->emplaceBack(std::move(newSegment))) {
      return nullptr;
    }
  }

  MOZ_ASSERT(segmentPool->length() > 0);
  SharedCodeSegment segment = (*segmentPool)[segmentPool->length() - 1].get();

  uint8_t* allocationStart = nullptr;
  segment->claimSpace(allocationLength, &allocationStart);
  uint8_t* codeStart = allocationStart + paddingLength;

  MOZ_ASSERT(CodeSegment::IsAligned(uintptr_t(segment->base())));
  MOZ_ASSERT(CodeSegment::IsAligned(allocationStart - segment->base()));

  *allocationStartOut = allocationStart;
  *codeStartOut = codeStart;
  *allocationLengthOut = allocationLength;
  return segment;
}

void CodeSegment::addSizeOfMisc(MallocSizeOf mallocSizeOf, size_t* code,
                                size_t* data) const {
  *code += capacityBytes();
  *data += mallocSizeOf(this);
}

size_t CacheableChars::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const {
  return mallocSizeOf(get());
}

static constexpr unsigned LAZY_STUB_LIFO_DEFAULT_CHUNK_SIZE = 8 * 1024;

bool Code::createManyLazyEntryStubs(const WriteGuard& guard,
                                    const Uint32Vector& funcExportIndices,
                                    const CodeBlock& tierCodeBlock,
                                    size_t* stubBlockIndex) const {
  MOZ_ASSERT(funcExportIndices.length());

  LifoAlloc lifo(LAZY_STUB_LIFO_DEFAULT_CHUNK_SIZE, js::MallocArena);
  TempAllocator alloc(&lifo);
  JitContext jitContext;
  WasmMacroAssembler masm(alloc);

  const FuncExportVector& funcExports = tierCodeBlock.funcExports;
  uint8_t* segmentBase = tierCodeBlock.segment->base();

  CodeRangeVector codeRanges;
  DebugOnly<uint32_t> numExpectedRanges = 0;
  for (uint32_t funcExportIndex : funcExportIndices) {
    const FuncExport& fe = funcExports[funcExportIndex];
    const FuncType& funcType = codeMeta_->getFuncType(fe.funcIndex());
    // Exports that don't support a jit entry get only the interp entry.
    numExpectedRanges += (funcType.canHaveJitEntry() ? 2 : 1);
    void* calleePtr =
        segmentBase + tierCodeBlock.codeRange(fe).funcUncheckedCallEntry();
    Maybe<ImmPtr> callee;
    callee.emplace(calleePtr, ImmPtr::NoCheckToken());
    if (!GenerateEntryStubs(masm, funcExportIndex, fe, funcType, callee,
                            /* asmjs */ false, &codeRanges)) {
      return false;
    }
  }
  MOZ_ASSERT(codeRanges.length() == numExpectedRanges,
             "incorrect number of entries per function");

  masm.finish();

  MOZ_ASSERT(masm.callSites().empty());
  MOZ_ASSERT(masm.callSiteTargets().empty());
  MOZ_ASSERT(masm.trapSites().empty());
  MOZ_ASSERT(masm.tryNotes().empty());
  MOZ_ASSERT(masm.codeRangeUnwindInfos().empty());

  if (masm.oom()) {
    return false;
  }

  UniqueCodeBlock stubCodeBlock =
      MakeUnique<CodeBlock>(CodeBlockKind::LazyStubs);
  if (!stubCodeBlock) {
    return false;
  }

  // Allocate space in a code segment we can use
  uint32_t codeLength = masm.bytesNeeded();
  uint8_t* allocationStart;
  uint8_t* codeStart;
  uint32_t allocationLength;
  stubCodeBlock->segment = CodeSegment::claimSpaceFromPool(
      codeLength, &guard->lazyStubSegments,
      /* allowLastDitchGC = */ true, &allocationStart, &codeStart,
      &allocationLength);
  if (!stubCodeBlock->segment) {
    return false;
  }

  // Copy, link, and make the code executable
  {
    AutoMarkJitCodeWritableForThread writable;

    masm.executableCopy(codeStart);

    // Clear the padding between the end of the code and the end of the
    // allocation.
    uint8_t* allocationEnd = allocationStart + allocationLength;
    uint8_t* codeEnd = codeStart + codeLength;
    MOZ_ASSERT(codeEnd <= allocationEnd);
    size_t paddingAfterCode = allocationEnd - codeEnd;
    memset(codeEnd, 0, paddingAfterCode);

    if (!stubCodeBlock->segment->linkAndMakeExecutableSubRange(
            writable, masm, allocationStart, codeStart, allocationLength)) {
      return false;
    }
  }

  stubCodeBlock->codeBase = codeStart;
  stubCodeBlock->codeLength = codeLength;
  stubCodeBlock->codeRanges = std::move(codeRanges);

  uint32_t offsetInSegment = codeStart - stubCodeBlock->segment->base();
  *stubBlockIndex = guard->blocks.length();

  uint32_t codeRangeIndex = 0;
  for (uint32_t funcExportIndex : funcExportIndices) {
    const FuncExport& fe = funcExports[funcExportIndex];
    const FuncType& funcType = codeMeta_->getFuncType(fe.funcIndex());

    LazyFuncExport lazyExport(fe.funcIndex(), *stubBlockIndex, codeRangeIndex,
                              tierCodeBlock.kind);

    // Offset the code range for the interp entry to where it landed in the
    // segment.
    CodeRange& interpRange = stubCodeBlock->codeRanges[codeRangeIndex];
    MOZ_ASSERT(interpRange.isInterpEntry());
    MOZ_ASSERT(interpRange.funcIndex() == fe.funcIndex());
    interpRange.offsetBy(offsetInSegment);
    codeRangeIndex += 1;

    // Offset the code range for the jit entry (if any) to where it landed in
    // the segment.
    if (funcType.canHaveJitEntry()) {
      CodeRange& jitRange = stubCodeBlock->codeRanges[codeRangeIndex];
      MOZ_ASSERT(jitRange.isJitEntry());
      MOZ_ASSERT(jitRange.funcIndex() == fe.funcIndex());
      codeRangeIndex += 1;
      jitRange.offsetBy(offsetInSegment);
    }

    size_t exportIndex;
    const uint32_t targetFunctionIndex = fe.funcIndex();

    if (BinarySearchIf(
            guard->lazyExports, 0, guard->lazyExports.length(),
            [targetFunctionIndex](const LazyFuncExport& funcExport) {
              return targetFunctionIndex - funcExport.funcIndex;
            },
            &exportIndex)) {
      DebugOnly<CodeBlockKind> oldKind =
          guard->lazyExports[exportIndex].funcKind;
      MOZ_ASSERT(oldKind == CodeBlockKind::SharedStubs ||
                 oldKind == CodeBlockKind::BaselineTier);
      guard->lazyExports[exportIndex] = std::move(lazyExport);
    } else if (!guard->lazyExports.insert(
                   guard->lazyExports.begin() + exportIndex,
                   std::move(lazyExport))) {
      return false;
    }
  }

  return addCodeBlock(guard, std::move(stubCodeBlock), nullptr);
}

bool Code::createOneLazyEntryStub(const WriteGuard& guard,
                                  uint32_t funcExportIndex,
                                  const CodeBlock& tierCodeBlock,
                                  void** interpEntry) const {
  Uint32Vector funcExportIndexes;
  if (!funcExportIndexes.append(funcExportIndex)) {
    return false;
  }

  size_t stubBlockIndex;
  if (!createManyLazyEntryStubs(guard, funcExportIndexes, tierCodeBlock,
                                &stubBlockIndex)) {
    return false;
  }

  const CodeBlock& block = *guard->blocks[stubBlockIndex];
  const CodeSegment& segment = *block.segment;
  const CodeRangeVector& codeRanges = block.codeRanges;

  const FuncExport& fe = tierCodeBlock.funcExports[funcExportIndex];
  const FuncType& funcType = codeMeta_->getFuncType(fe.funcIndex());

  // We created one or two stubs, depending on the function type.
  uint32_t funcEntryRanges = funcType.canHaveJitEntry() ? 2 : 1;
  MOZ_ASSERT(codeRanges.length() >= funcEntryRanges);

  // The first created range is the interp entry
  const CodeRange& interpRange =
      codeRanges[codeRanges.length() - funcEntryRanges];
  MOZ_ASSERT(interpRange.isInterpEntry());
  *interpEntry = segment.base() + interpRange.begin();

  // The second created range is the jit entry
  if (funcType.canHaveJitEntry()) {
    const CodeRange& jitRange =
        codeRanges[codeRanges.length() - funcEntryRanges + 1];
    MOZ_ASSERT(jitRange.isJitEntry());
    jumpTables_.setJitEntry(jitRange.funcIndex(),
                            segment.base() + jitRange.begin());
  }
  return true;
}

bool Code::getOrCreateInterpEntry(uint32_t funcIndex,
                                  const FuncExport** funcExport,
                                  void** interpEntry) const {
  size_t funcExportIndex;
  const CodeBlock& codeBlock = funcCodeBlock(funcIndex);
  *funcExport = &codeBlock.lookupFuncExport(funcIndex, &funcExportIndex);

  const FuncExport& fe = **funcExport;
  if (fe.hasEagerStubs()) {
    *interpEntry = codeBlock.segment->base() + fe.eagerInterpEntryOffset();
    return true;
  }

  MOZ_ASSERT(!codeMetaForAsmJS_, "only wasm can lazily export functions");

  auto guard = data_.writeLock();
  *interpEntry = lookupLazyInterpEntry(guard, funcIndex);
  if (*interpEntry) {
    return true;
  }

  return createOneLazyEntryStub(guard, funcExportIndex, codeBlock, interpEntry);
}

bool Code::createTier2LazyEntryStubs(const WriteGuard& guard,
                                     const CodeBlock& tier2Code,
                                     Maybe<size_t>* outStubBlockIndex) const {
  if (!guard->lazyExports.length()) {
    return true;
  }

  Uint32Vector funcExportIndices;
  if (!funcExportIndices.reserve(guard->lazyExports.length())) {
    return false;
  }

  for (size_t i = 0; i < tier2Code.funcExports.length(); i++) {
    const FuncExport& fe = tier2Code.funcExports[i];
    const LazyFuncExport* lfe = lookupLazyFuncExport(guard, fe.funcIndex());
    if (lfe) {
      MOZ_ASSERT(lfe->funcKind == CodeBlockKind::BaselineTier);
      funcExportIndices.infallibleAppend(i);
    }
  }

  if (funcExportIndices.length() == 0) {
    return true;
  }

  size_t stubBlockIndex;
  if (!createManyLazyEntryStubs(guard, funcExportIndices, tier2Code,
                                &stubBlockIndex)) {
    return false;
  }

  outStubBlockIndex->emplace(stubBlockIndex);
  return true;
}

class Module::PartialTier2CompileTaskImpl : public PartialTier2CompileTask {
  const SharedCode code_;
  uint32_t funcIndex_;
  Atomic<bool> cancelled_;

 public:
  PartialTier2CompileTaskImpl(const Code& code, uint32_t funcIndex)
      : code_(&code), funcIndex_(funcIndex), cancelled_(false) {}

  void cancel() override { cancelled_ = true; }

  void runHelperThreadTask(AutoLockHelperThreadState& locked) override {
    if (!cancelled_) {
      AutoUnlockHelperThreadState unlock(locked);

      // In the case `!success && !cancelled_`, compilation has failed
      // and this function will be stuck in state TierUpState::Requested
      // forever.
      UniqueChars error;
      UniqueCharsVector warnings;
      bool success = CompilePartialTier2(*code_, funcIndex_, &error, &warnings,
                                         &cancelled_);

      ReportTier2ResultsOffThread(success, mozilla::Some(funcIndex_),
                                  code_->codeMeta().scriptedCaller(), error,
                                  warnings);
    }

    // The task is finished, release it.
    js_delete(this);
  }

  ThreadType threadType() override {
    return ThreadType::THREAD_TYPE_WASM_COMPILE_PARTIAL_TIER2;
  }
};

bool Code::requestTierUp(uint32_t funcIndex) const {
  // Note: this runs on the requesting (wasm-running) thread, not on a
  // compilation-helper thread.
  MOZ_ASSERT(mode_ == CompileMode::LazyTiering);
  FuncState& state = funcStates_[funcIndex - codeMeta_->numFuncImports];
  if (!state.tierUpState.compareExchange(TierUpState::NotRequested,
                                         TierUpState::Requested)) {
    return true;
  }

  auto task =
      js::MakeUnique<Module::PartialTier2CompileTaskImpl>(*this, funcIndex);
  if (!task) {
    // Effect is (I think), if we OOM here, the request is ignored.
    // See bug 1911060.
    return false;
  }

  StartOffThreadWasmPartialTier2Compile(std::move(task));
  return true;
}

bool Code::finishTier2(UniqueCodeBlock tier2CodeBlock,
                       UniqueLinkData tier2LinkData) const {
  MOZ_RELEASE_ASSERT(mode_ == CompileMode::EagerTiering ||
                     mode_ == CompileMode::LazyTiering);
  MOZ_RELEASE_ASSERT(hasCompleteTier2_ == false &&
                     tier2CodeBlock->tier() == Tier::Optimized);
  // Acquire the write guard before we start mutating anything. We hold this
  // for the minimum amount of time necessary.
  CodeBlock* tier2CodePointer;
  {
    auto guard = data_.writeLock();

    // Borrow the tier2 pointer before moving it into the block vector. This
    // ensures we maintain the invariant that completeTier2_ is never read if
    // hasCompleteTier2_ is false.
    tier2CodePointer = tier2CodeBlock.get();

    // Publish this code to the process wide map.
    if (!addCodeBlock(guard, std::move(tier2CodeBlock),
                      std::move(tier2LinkData))) {
      return false;
    }

    // Before we can make tier-2 live, we need to compile tier2 versions of any
    // extant tier1 lazy stubs (otherwise, tiering would break the assumption
    // that any extant exported wasm function has had a lazy entry stub already
    // compiled for it).
    //
    // Also see doc block for stubs in WasmJS.cpp.
    Maybe<size_t> stub2Index;
    if (!createTier2LazyEntryStubs(guard, *tier2CodePointer, &stub2Index)) {
      return false;
    }

    // Initializing the code above will have flushed the icache for all cores.
    // However, there could still be stale data in the execution pipeline of
    // other cores on some platforms. Force an execution context flush on all
    // threads to fix this before we commit the code.
    //
    // This is safe due to the check in `PlatformCanTier` in WasmCompile.cpp
    jit::FlushExecutionContextForAllThreads();

    // Now that we can't fail or otherwise abort tier2, make it live.
    if (mode_ == CompileMode::EagerTiering) {
      completeTier2_ = tier2CodePointer;
      hasCompleteTier2_ = true;

      // We don't need to update funcStates, because we're doing eager tiering
      MOZ_ASSERT(!funcStates_.get());
    } else {
      for (const CodeRange& cr : tier2CodePointer->codeRanges) {
        if (!cr.isFunction()) {
          continue;
        }
        FuncState& state =
            funcStates_.get()[cr.funcIndex() - codeMeta_->numFuncImports];
        state.bestTier = tier2CodePointer;
        state.tierUpState = TierUpState::Finished;
      }
    }

    // Update jump vectors with pointers to tier-2 lazy entry stubs, if any.
    if (stub2Index) {
      const CodeBlock& block = *guard->blocks[*stub2Index];
      const CodeSegment& segment = *block.segment;
      for (const CodeRange& cr : block.codeRanges) {
        if (!cr.isJitEntry()) {
          continue;
        }
        jumpTables_.setJitEntry(cr.funcIndex(), segment.base() + cr.begin());
      }
    }
  }

  // And we update the jump vectors with pointers to tier-2 functions and eager
  // stubs.  Callers will continue to invoke tier-1 code until, suddenly, they
  // will invoke tier-2 code.  This is benign.
  uint8_t* base = tier2CodePointer->segment->base();
  for (const CodeRange& cr : tier2CodePointer->codeRanges) {
    // These are racy writes that we just want to be visible, atomically,
    // eventually.  All hardware we care about will do this right.  But
    // we depend on the compiler not splitting the stores hidden inside the
    // set*Entry functions.
    if (cr.isFunction()) {
      jumpTables_.setTieringEntry(cr.funcIndex(), base + cr.funcTierEntry());
    } else if (cr.isJitEntry()) {
      jumpTables_.setJitEntry(cr.funcIndex(), base + cr.begin());
    }
  }
  return true;
}

bool Code::addCodeBlock(const WriteGuard& guard, UniqueCodeBlock block,
                        UniqueLinkData maybeLinkData) const {
  // Don't bother saving the link data if the block won't be serialized
  if (maybeLinkData && !block->isSerializable()) {
    maybeLinkData = nullptr;
  }

  CodeBlock* blockPtr = block.get();
  size_t codeBlockIndex = guard->blocks.length();
  return guard->blocks.append(std::move(block)) &&
         guard->blocksLinkData.append(std::move(maybeLinkData)) &&
         blockMap_.insert(blockPtr) &&
         blockPtr->initialize(*this, codeBlockIndex);
}

SharedCodeSegment Code::createFuncCodeSegmentFromPool(
    jit::MacroAssembler& masm, const LinkData& linkData, bool allowLastDitchGC,
    uint8_t** codeStartOut, uint32_t* codeLengthOut) const {
  uint32_t codeLength = masm.bytesNeeded();

  // Allocate space in a code segment we can use
  uint8_t* allocationStart;
  uint8_t* codeStart;
  uint32_t allocationLength;
  SharedCodeSegment segment;
  {
    auto guard = data_.writeLock();
    segment = CodeSegment::claimSpaceFromPool(
        codeLength, &guard->lazyFuncSegments, allowLastDitchGC,
        &allocationStart, &codeStart, &allocationLength);
    if (!segment) {
      return nullptr;
    }
  }

  // Update allocation statistics
  {
    auto guard = codeMeta().stats.writeLock();
    guard->partialCodeBytesMapped += allocationLength;
    guard->partialCodeBytesUsed += codeLength;
  }

  // Copy and link the function code
  Maybe<AutoMarkJitCodeWritableForThread> writable;
  writable.emplace();

  masm.executableCopy(codeStart);
  if (!segment->linkAndMakeExecutableSubRange(*writable, linkData, this,
                                              allocationStart, codeStart,
                                              allocationLength)) {
    return nullptr;
  }

  *codeStartOut = codeStart;
  *codeLengthOut = codeLength;
  return segment;
}

const LazyFuncExport* Code::lookupLazyFuncExport(const WriteGuard& guard,
                                                 uint32_t funcIndex) const {
  size_t match;
  if (!BinarySearchIf(
          guard->lazyExports, 0, guard->lazyExports.length(),
          [funcIndex](const LazyFuncExport& funcExport) {
            return funcIndex - funcExport.funcIndex;
          },
          &match)) {
    return nullptr;
  }
  return &guard->lazyExports[match];
}

void* Code::lookupLazyInterpEntry(const WriteGuard& guard,
                                  uint32_t funcIndex) const {
  const LazyFuncExport* fe = lookupLazyFuncExport(guard, funcIndex);
  if (!fe) {
    return nullptr;
  }
  const CodeBlock& block = *guard->blocks[fe->lazyStubBlockIndex];
  const CodeSegment& segment = *block.segment;
  return segment.base() + block.codeRanges[fe->funcCodeRangeIndex].begin();
}

CodeBlock::~CodeBlock() {
  if (unregisterOnDestroy_) {
    UnregisterCodeBlock(this);
  }
}

bool CodeBlock::initialize(const Code& code, size_t codeBlockIndex) {
  MOZ_ASSERT(!initialized());
  this->code = &code;
  this->codeBlockIndex = codeBlockIndex;
  segment->setCode(code);

  SendCodeRangesToProfiler(segment->base(), code.codeMeta(),
                           code.codeMetaForAsmJS(), codeRanges);

  // In the case of tiering, RegisterCodeBlock() immediately makes this code
  // block live to access from other threads executing the containing
  // module. So only call once the CodeBlock is fully initialized.
  if (!RegisterCodeBlock(this)) {
    return false;
  }

  // This bool is only used by the destructor which cannot be called racily
  // and so it is not a problem to mutate it after RegisterCodeBlock().
  MOZ_ASSERT(!unregisterOnDestroy_);
  unregisterOnDestroy_ = true;

  MOZ_ASSERT(initialized());
  return true;
}

void CodeBlock::offsetMetadataBy(uint32_t delta) {
  if (delta == 0) {
    return;
  }
  for (CodeRange& cr : codeRanges) {
    cr.offsetBy(delta);
  }
  callSites.offsetBy(delta);
  trapSites.offsetBy(delta);
  for (FuncExport& fe : funcExports) {
    fe.offsetBy(delta);
  }
  stackMaps.offsetBy(delta);
  for (TryNote& tn : tryNotes) {
    tn.offsetBy(delta);
  }
  for (CodeRangeUnwindInfo& crui : codeRangeUnwindInfos) {
    crui.offsetBy(delta);
  }
}

void CodeBlock::addSizeOfMisc(MallocSizeOf mallocSizeOf, size_t* code,
                              size_t* data) const {
  segment->addSizeOfMisc(mallocSizeOf, code, data);
  *data += funcToCodeRange.sizeOfExcludingThis(mallocSizeOf) +
           codeRanges.sizeOfExcludingThis(mallocSizeOf) +
           callSites.sizeOfExcludingThis(mallocSizeOf) +
           tryNotes.sizeOfExcludingThis(mallocSizeOf) +
           codeRangeUnwindInfos.sizeOfExcludingThis(mallocSizeOf) +
           trapSites.sizeOfExcludingThis(mallocSizeOf) +
           stackMaps.sizeOfExcludingThis(mallocSizeOf) +
           funcExports.sizeOfExcludingThis(mallocSizeOf);
  ;
}

const CodeRange* CodeBlock::lookupRange(const void* pc) const {
  CodeRange::OffsetInCode target((uint8_t*)pc - segment->base());
  return LookupInSorted(codeRanges, target);
}

bool CodeBlock::lookupCallSite(void* pc, CallSite* callSite) const {
  uint32_t target = ((uint8_t*)pc) - segment->base();
  return callSites.lookup(target, callSite);
}

const StackMap* CodeBlock::lookupStackMap(uint8_t* pc) const {
  return stackMaps.findMap(pc);
}

const wasm::TryNote* CodeBlock::lookupTryNote(const void* pc) const {
  size_t target = (uint8_t*)pc - segment->base();

  // We find the first hit (there may be multiple) to obtain the innermost
  // handler, which is why we cannot binary search here.
  for (const auto& tryNote : tryNotes) {
    if (tryNote.offsetWithinTryBody(target)) {
      return &tryNote;
    }
  }

  return nullptr;
}

bool CodeBlock::lookupTrap(void* pc, Trap* trapOut,
                           BytecodeOffset* bytecodeOut) const {
  MOZ_ASSERT(containsCodePC(pc));
  uint32_t target = ((uint8_t*)pc) - segment->base();
  return trapSites.lookup(target, trapOut, bytecodeOut);
}

struct UnwindInfoPCOffset {
  const CodeRangeUnwindInfoVector& info;
  explicit UnwindInfoPCOffset(const CodeRangeUnwindInfoVector& info)
      : info(info) {}
  uint32_t operator[](size_t index) const { return info[index].offset(); }
};

const CodeRangeUnwindInfo* CodeBlock::lookupUnwindInfo(void* pc) const {
  uint32_t target = ((uint8_t*)pc) - segment->base();
  size_t match;
  const CodeRangeUnwindInfo* info = nullptr;
  if (BinarySearch(UnwindInfoPCOffset(codeRangeUnwindInfos), 0,
                   codeRangeUnwindInfos.length(), target, &match)) {
    info = &codeRangeUnwindInfos[match];
  } else {
    // Exact match is not found, using insertion point to get the previous
    // info entry; skip if info is outside of codeRangeUnwindInfos.
    if (match == 0) return nullptr;
    if (match == codeRangeUnwindInfos.length()) {
      MOZ_ASSERT(
          codeRangeUnwindInfos[codeRangeUnwindInfos.length() - 1].unwindHow() ==
          CodeRangeUnwindInfo::Normal);
      return nullptr;
    }
    info = &codeRangeUnwindInfos[match - 1];
  }
  return info->unwindHow() == CodeRangeUnwindInfo::Normal ? nullptr : info;
}

struct ProjectFuncIndex {
  const FuncExportVector& funcExports;
  explicit ProjectFuncIndex(const FuncExportVector& funcExports)
      : funcExports(funcExports) {}
  uint32_t operator[](size_t index) const {
    return funcExports[index].funcIndex();
  }
};

FuncExport& CodeBlock::lookupFuncExport(
    uint32_t funcIndex, size_t* funcExportIndex /* = nullptr */) {
  size_t match;
  if (!BinarySearch(ProjectFuncIndex(funcExports), 0, funcExports.length(),
                    funcIndex, &match)) {
    MOZ_CRASH("missing function export");
  }
  if (funcExportIndex) {
    *funcExportIndex = match;
  }
  return funcExports[match];
}

const FuncExport& CodeBlock::lookupFuncExport(uint32_t funcIndex,
                                              size_t* funcExportIndex) const {
  return const_cast<CodeBlock*>(this)->lookupFuncExport(funcIndex,
                                                        funcExportIndex);
}

bool JumpTables::initialize(CompileMode mode, const CodeMetadata& codeMeta,
                            const CodeBlock& sharedStubs,
                            const CodeBlock& tier1) {
  static_assert(JSScript::offsetOfJitCodeRaw() == 0,
                "wasm fast jit entry is at (void*) jit[funcIndex]");

  mode_ = mode;
  numFuncs_ = codeMeta.numFuncs();

  if (mode_ != CompileMode::Once) {
    tiering_ = TablePointer(js_pod_calloc<void*>(numFuncs_));
    if (!tiering_) {
      return false;
    }
  }

  // The number of jit entries is overestimated, but it is simpler when
  // filling/looking up the jit entries and safe (worst case we'll crash
  // because of a null deref when trying to call the jit entry of an
  // unexported function).
  jit_ = TablePointer(js_pod_calloc<void*>(numFuncs_));
  if (!jit_) {
    return false;
  }

  uint8_t* codeBase = sharedStubs.segment->base();
  for (const CodeRange& cr : sharedStubs.codeRanges) {
    if (cr.isFunction()) {
      setTieringEntry(cr.funcIndex(), codeBase + cr.funcTierEntry());
    } else if (cr.isJitEntry()) {
      setJitEntry(cr.funcIndex(), codeBase + cr.begin());
    }
  }

  codeBase = tier1.segment->base();
  for (const CodeRange& cr : tier1.codeRanges) {
    if (cr.isFunction()) {
      setTieringEntry(cr.funcIndex(), codeBase + cr.funcTierEntry());
    } else if (cr.isJitEntry()) {
      setJitEntry(cr.funcIndex(), codeBase + cr.begin());
    }
  }
  return true;
}

Code::Code(CompileMode mode, const CodeMetadata& codeMeta,
           const CodeMetadataForAsmJS* codeMetaForAsmJS)
    : mode_(mode),
      data_(mutexid::WasmCodeProtected),
      codeMeta_(&codeMeta),
      codeMetaForAsmJS_(codeMetaForAsmJS),
      completeTier1_(nullptr),
      completeTier2_(nullptr),
      profilingLabels_(mutexid::WasmCodeProfilingLabels,
                       CacheableCharsVector()),
      trapCode_(nullptr),
      debugStubOffset_(0),
      requestTierUpStubOffset_(0),
      updateCallRefMetricsStubOffset_(0) {}

bool Code::initialize(FuncImportVector&& funcImports,
                      UniqueCodeBlock sharedStubs,
                      UniqueLinkData sharedStubsLinkData,
                      UniqueCodeBlock tier1CodeBlock,
                      UniqueLinkData tier1LinkData) {
  funcImports_ = std::move(funcImports);

  auto guard = data_.writeLock();

  sharedStubs_ = sharedStubs.get();
  completeTier1_ = tier1CodeBlock.get();
  trapCode_ = sharedStubs_->segment->base() + sharedStubsLinkData->trapOffset;
  if (!jumpTables_.initialize(mode_, *codeMeta_, *sharedStubs_,
                              *completeTier1_) ||
      !addCodeBlock(guard, std::move(sharedStubs),
                    std::move(sharedStubsLinkData)) ||
      !addCodeBlock(guard, std::move(tier1CodeBlock),
                    std::move(tier1LinkData))) {
    return false;
  }

  if (mode_ == CompileMode::LazyTiering) {
    uint32_t numFuncDefs = codeMeta_->numFuncs() - codeMeta_->numFuncImports;
    funcStates_ = FuncStatesPointer(js_pod_calloc<FuncState>(numFuncDefs));
    if (!funcStates_) {
      return false;
    }
    for (uint32_t funcDefIndex = 0; funcDefIndex < numFuncDefs;
         funcDefIndex++) {
      funcStates_.get()[funcDefIndex].bestTier = completeTier1_;
      funcStates_.get()[funcDefIndex].tierUpState = TierUpState::NotRequested;
    }
  }

  return true;
}

uint32_t Code::getFuncIndex(JSFunction* fun) const {
  MOZ_ASSERT(fun->isWasm() || fun->isAsmJSNative());
  if (!fun->isWasmWithJitEntry()) {
    return fun->wasmFuncIndex();
  }
  return jumpTables_.funcIndexFromJitEntry(fun->wasmJitEntry());
}

Tiers Code::completeTiers() const {
  if (hasCompleteTier2_) {
    return Tiers(completeTier1_->tier(), completeTier2_->tier());
  }
  return Tiers(completeTier1_->tier());
}

bool Code::hasCompleteTier(Tier t) const {
  if (hasCompleteTier2_ && completeTier2_->tier() == t) {
    return true;
  }
  return completeTier1_->tier() == t;
}

Tier Code::stableCompleteTier() const { return completeTier1_->tier(); }

Tier Code::bestCompleteTier() const {
  if (hasCompleteTier2_) {
    return completeTier2_->tier();
  }
  return completeTier1_->tier();
}

const CodeBlock& Code::completeTierCodeBlock(Tier tier) const {
  switch (tier) {
    case Tier::Baseline:
      if (completeTier1_->tier() == Tier::Baseline) {
        MOZ_ASSERT(completeTier1_->initialized());
        return *completeTier1_;
      }
      MOZ_CRASH("No code segment at this tier");
    case Tier::Optimized:
      if (completeTier1_->tier() == Tier::Optimized) {
        MOZ_ASSERT(completeTier1_->initialized());
        return *completeTier1_;
      }
      // It is incorrect to ask for the optimized tier without there being such
      // a tier and the tier having been committed.  The guard here could
      // instead be `if (hasCompleteTier2_) ... ` but codeBlock(t) should not be
      // called in contexts where that test is necessary.
      MOZ_RELEASE_ASSERT(hasCompleteTier2_);
      MOZ_ASSERT(completeTier2_->initialized());
      return *completeTier2_;
  }
  MOZ_CRASH();
}

const LinkData* Code::codeBlockLinkData(const CodeBlock& block) const {
  auto guard = data_.readLock();
  MOZ_ASSERT(block.initialized() && block.code == this);
  return guard->blocksLinkData[block.codeBlockIndex].get();
}

void Code::clearLinkData() const {
  auto guard = data_.writeLock();
  for (UniqueLinkData& linkData : guard->blocksLinkData) {
    linkData = nullptr;
  }
}

bool Code::lookupFunctionTier(const CodeRange* codeRange, Tier* tier) const {
  // This logic only works if the codeRange is a function, and therefore only
  // exists in metadata and not a lazy stub tier. Generalizing to access lazy
  // stubs would require taking a lock, which is undesirable for the profiler.
  MOZ_ASSERT(codeRange->isFunction());
  for (Tier t : completeTiers()) {
    const CodeBlock& code = completeTierCodeBlock(t);
    if (codeRange >= code.codeRanges.begin() &&
        codeRange < code.codeRanges.end()) {
      *tier = t;
      return true;
    }
  }
  return false;
}

// When enabled, generate profiling labels for every name in funcNames_ that is
// the name of some Function CodeRange. This involves malloc() so do it now
// since, once we start sampling, we'll be in a signal-handing context where we
// cannot malloc.
void Code::ensureProfilingLabels(bool profilingEnabled) const {
  auto labels = profilingLabels_.lock();

  if (!profilingEnabled) {
    labels->clear();
    return;
  }

  if (!labels->empty()) {
    return;
  }

  // Any tier will do, we only need tier-invariant data that are incidentally
  // stored with the code ranges.
  const CodeBlock& sharedStubsCodeBlock = sharedStubs();
  const CodeBlock& tier1CodeBlock = completeTierCodeBlock(stableCompleteTier());

  // Ignore any OOM failures, nothing we can do about it
  (void)appendProfilingLabels(labels, sharedStubsCodeBlock);
  (void)appendProfilingLabels(labels, tier1CodeBlock);
}

bool Code::appendProfilingLabels(
    const ExclusiveData<CacheableCharsVector>::Guard& labels,
    const CodeBlock& codeBlock) const {
  for (const CodeRange& codeRange : codeBlock.codeRanges) {
    if (!codeRange.isFunction()) {
      continue;
    }

    Int32ToCStringBuf cbuf;
    size_t bytecodeStrLen;
    const char* bytecodeStr = Uint32ToCString(
        &cbuf, codeMeta().funcBytecodeOffset(codeRange.funcIndex()),
        &bytecodeStrLen);
    MOZ_ASSERT(bytecodeStr);

    UTF8Bytes name;
    bool ok;
    if (codeMetaForAsmJS()) {
      ok =
          codeMetaForAsmJS()->getFuncNameForAsmJS(codeRange.funcIndex(), &name);
    } else {
      ok = codeMeta().getFuncNameForWasm(NameContext::Standalone,
                                         codeRange.funcIndex(), &name);
    }
    if (!ok || !name.append(" (", 2)) {
      return false;
    }

    if (const char* filename = codeMeta().scriptedCaller().filename.get()) {
      if (!name.append(filename, strlen(filename))) {
        return false;
      }
    } else {
      if (!name.append('?')) {
        return false;
      }
    }

    if (!name.append(':') || !name.append(bytecodeStr, bytecodeStrLen) ||
        !name.append(")\0", 2)) {
      return false;
    }

    UniqueChars label(name.extractOrCopyRawBuffer());
    if (!label) {
      return false;
    }

    if (codeRange.funcIndex() >= labels->length()) {
      if (!labels->resize(codeRange.funcIndex() + 1)) {
        return false;
      }
    }

    ((CacheableCharsVector&)labels)[codeRange.funcIndex()] = std::move(label);
  }
  return true;
}

const char* Code::profilingLabel(uint32_t funcIndex) const {
  auto labels = profilingLabels_.lock();

  if (funcIndex >= labels->length() ||
      !((CacheableCharsVector&)labels)[funcIndex]) {
    return "?";
  }
  return ((CacheableCharsVector&)labels)[funcIndex].get();
}

void Code::addSizeOfMiscIfNotSeen(
    MallocSizeOf mallocSizeOf, CodeMetadata::SeenSet* seenCodeMeta,
    CodeMetadataForAsmJS::SeenSet* seenCodeMetaForAsmJS,
    Code::SeenSet* seenCode, size_t* code, size_t* data) const {
  auto p = seenCode->lookupForAdd(this);
  if (p) {
    return;
  }
  bool ok = seenCode->add(p, this);
  (void)ok;  // oh well

  auto guard = data_.readLock();
  *data +=
      mallocSizeOf(this) + guard->blocks.sizeOfExcludingThis(mallocSizeOf) +
      guard->blocksLinkData.sizeOfExcludingThis(mallocSizeOf) +
      guard->lazyExports.sizeOfExcludingThis(mallocSizeOf) +
      (codeMetaForAsmJS() ? codeMetaForAsmJS()->sizeOfIncludingThisIfNotSeen(
                                mallocSizeOf, seenCodeMetaForAsmJS)
                          : 0) +
      funcImports_.sizeOfExcludingThis(mallocSizeOf) +
      profilingLabels_.lock()->sizeOfExcludingThis(mallocSizeOf) +
      jumpTables_.sizeOfMiscExcludingThis();
  for (const SharedCodeSegment& stub : guard->lazyStubSegments) {
    stub->addSizeOfMisc(mallocSizeOf, code, data);
  }

  sharedStubs().addSizeOfMisc(mallocSizeOf, code, data);
  for (auto t : completeTiers()) {
    completeTierCodeBlock(t).addSizeOfMisc(mallocSizeOf, code, data);
  }
}

void CodeBlock::disassemble(JSContext* cx, int kindSelection,
                            PrintCallback printString) const {
  for (const CodeRange& range : codeRanges) {
    if (kindSelection & (1 << range.kind())) {
      MOZ_ASSERT(range.begin() < segment->lengthBytes());
      MOZ_ASSERT(range.end() < segment->lengthBytes());

      const char* kind;
      char kindbuf[128];
      switch (range.kind()) {
        case CodeRange::Function:
          kind = "Function";
          break;
        case CodeRange::InterpEntry:
          kind = "InterpEntry";
          break;
        case CodeRange::JitEntry:
          kind = "JitEntry";
          break;
        case CodeRange::ImportInterpExit:
          kind = "ImportInterpExit";
          break;
        case CodeRange::ImportJitExit:
          kind = "ImportJitExit";
          break;
        default:
          SprintfLiteral(kindbuf, "CodeRange::Kind(%d)", range.kind());
          kind = kindbuf;
          break;
      }
      const char* separator =
          "\n--------------------------------------------------\n";
      // The buffer is quite large in order to accomodate mangled C++ names;
      // lengths over 3500 have been observed in the wild.
      char buf[4096];
      if (range.hasFuncIndex()) {
        const char* funcName = "(unknown)";
        UTF8Bytes namebuf;
        bool ok;
        if (code->codeMetaForAsmJS()) {
          ok = code->codeMetaForAsmJS()->getFuncNameForAsmJS(range.funcIndex(),
                                                             &namebuf);
        } else {
          ok = code->codeMeta().getFuncNameForWasm(NameContext::Standalone,
                                                   range.funcIndex(), &namebuf);
        }
        if (ok && namebuf.append('\0')) {
          funcName = namebuf.begin();
        }
        SprintfLiteral(buf, "%sKind = %s, index = %d, name = %s:\n", separator,
                       kind, range.funcIndex(), funcName);
      } else {
        SprintfLiteral(buf, "%sKind = %s\n", separator, kind);
      }
      printString(buf);

      uint8_t* theCode = segment->base() + range.begin();
      jit::Disassemble(theCode, range.end() - range.begin(), printString);
    }
  }
}

void Code::disassemble(JSContext* cx, Tier tier, int kindSelection,
                       PrintCallback printString) const {
  this->sharedStubs().disassemble(cx, kindSelection, printString);
  this->completeTierCodeBlock(tier).disassemble(cx, kindSelection, printString);
}

// Return a map with names and associated statistics
MetadataAnalysisHashMap Code::metadataAnalysis(JSContext* cx) const {
  MetadataAnalysisHashMap hashmap;
  if (!hashmap.reserve(14)) {
    return hashmap;
  }

  for (auto t : completeTiers()) {
    const CodeBlock& codeBlock = completeTierCodeBlock(t);
    size_t length = codeBlock.funcToCodeRange.numEntries();
    length += codeBlock.codeRanges.length();
    length += codeBlock.callSites.length();
    length += codeBlock.trapSites.sumOfLengths();
    length += codeBlock.funcExports.length();
    length += codeBlock.stackMaps.length();
    length += codeBlock.tryNotes.length();

    hashmap.putNewInfallible("metadata length", length);

    // Iterate over the Code Ranges and accumulate all pieces of code.
    size_t code_size = 0;
    for (const CodeRange& codeRange : codeBlock.codeRanges) {
      if (!codeRange.isFunction()) {
        continue;
      }
      code_size += codeRange.end() - codeRange.begin();
    }

    hashmap.putNewInfallible("stackmaps number", codeBlock.stackMaps.length());
    hashmap.putNewInfallible("trapSites number",
                             codeBlock.trapSites.sumOfLengths());
    hashmap.putNewInfallible("codeRange size in bytes", code_size);
    hashmap.putNewInfallible("code segment capacity",
                             codeBlock.segment->capacityBytes());

    auto mallocSizeOf = cx->runtime()->debuggerMallocSizeOf;

    hashmap.putNewInfallible(
        "funcToCodeRange size",
        codeBlock.funcToCodeRange.sizeOfExcludingThis(mallocSizeOf));
    hashmap.putNewInfallible(
        "codeRanges size",
        codeBlock.codeRanges.sizeOfExcludingThis(mallocSizeOf));
    hashmap.putNewInfallible(
        "callSites size",
        codeBlock.callSites.sizeOfExcludingThis(mallocSizeOf));
    hashmap.putNewInfallible(
        "tryNotes size", codeBlock.tryNotes.sizeOfExcludingThis(mallocSizeOf));
    hashmap.putNewInfallible(
        "trapSites size",
        codeBlock.trapSites.sizeOfExcludingThis(mallocSizeOf));
    hashmap.putNewInfallible(
        "stackMaps size",
        codeBlock.stackMaps.sizeOfExcludingThis(mallocSizeOf));
    hashmap.putNewInfallible(
        "funcExports size",
        codeBlock.funcExports.sizeOfExcludingThis(mallocSizeOf));
  }

  return hashmap;
}

void wasm::PatchDebugSymbolicAccesses(uint8_t* codeBase, MacroAssembler& masm) {
#ifdef WASM_CODEGEN_DEBUG
  for (auto& access : masm.symbolicAccesses()) {
    switch (access.target) {
      case SymbolicAddress::PrintI32:
      case SymbolicAddress::PrintPtr:
      case SymbolicAddress::PrintF32:
      case SymbolicAddress::PrintF64:
      case SymbolicAddress::PrintText:
        break;
      default:
        MOZ_CRASH("unexpected symbol in PatchDebugSymbolicAccesses");
    }
    ABIFunctionType abiType;
    void* target = AddressOf(access.target, &abiType);
    uint8_t* patchAt = codeBase + access.patchAt.offset();
    Assembler::PatchDataWithValueCheck(CodeLocationLabel(patchAt),
                                       PatchedImmPtr(target),
                                       PatchedImmPtr((void*)-1));
  }
#else
  MOZ_ASSERT(masm.symbolicAccesses().empty());
#endif
}
