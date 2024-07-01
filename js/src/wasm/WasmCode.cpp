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
using mozilla::BinarySearch;
using mozilla::BinarySearchIf;
using mozilla::MakeEnumeratedRange;
using mozilla::PodAssign;

size_t LinkData::SymbolicLinkArray::sizeOfExcludingThis(
    MallocSizeOf mallocSizeOf) const {
  size_t size = 0;
  for (const Uint32Vector& offsets : *this) {
    size += offsets.sizeOfExcludingThis(mallocSizeOf);
  }
  return size;
}

static uint32_t RoundupCodeLength(uint32_t codeLength) {
  // AllocateExecutableMemory() requires a multiple of ExecutableCodePageSize.
  return RoundUp(codeLength, ExecutableCodePageSize);
}

UniqueCodeBytes wasm::AllocateCodeBytes(
    Maybe<AutoMarkJitCodeWritableForThread>& writable, uint32_t codeLength) {
  if (codeLength > MaxCodeBytesPerProcess) {
    return nullptr;
  }

  static_assert(MaxCodeBytesPerProcess <= INT32_MAX, "rounding won't overflow");
  uint32_t roundedCodeLength = RoundupCodeLength(codeLength);

  void* p =
      AllocateExecutableMemory(roundedCodeLength, ProtectionSetting::Writable,
                               MemCheckKind::MakeUndefined);

  // If the allocation failed and the embedding gives us a last-ditch attempt
  // to purge all memory (which, in gecko, does a purging GC/CC/GC), do that
  // then retry the allocation.
  if (!p) {
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
  MOZ_ASSERT(codeLength == RoundupCodeLength(codeLength));

#ifdef MOZ_VTUNE
  vtune::UnmarkBytes(bytes, codeLength);
#endif
  DeallocateExecutableMemory(bytes, codeLength);
}

bool wasm::StaticallyLink(jit::AutoMarkJitCodeWritableForThread& writable,
                          uint8_t* base, const LinkData& linkData) {
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
      const char* file = codeMeta.filename.get();
      if (codeRange.isFunction()) {
        if (!name.append('\0')) {
          return;
        }
        unsigned line = codeRange.funcLineOrBytecode();
        CollectPerfSpewerWasmFunctionMap(start, size, file, line, name.begin());
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

bool CodeSegment::linkAndMakeExecutable(
    jit::AutoMarkJitCodeWritableForThread& writable, const LinkData& linkData) {
  if (!StaticallyLink(writable, bytes_.get(), linkData)) {
    return false;
  }

  // Optimized compilation finishes on a background thread, so we must make sure
  // to flush the icaches of all the executing threads.
  // Reprotect the whole region to avoid having separate RW and RX mappings.
  return ExecutableAllocator::makeExecutableAndFlushICache(
      base(), RoundupCodeLength(lengthBytes()));
}

SharedCodeSegment CodeSegment::createEmpty(size_t capacityBytes) {
  uint32_t codeLength = 0;
  uint32_t codeCapacity = RoundupCodeLength(capacityBytes);
  Maybe<AutoMarkJitCodeWritableForThread> writable;
  UniqueCodeBytes codeBytes = AllocateCodeBytes(writable, codeCapacity);
  if (!codeBytes) {
    return nullptr;
  }

  return js_new<CodeSegment>(std::move(codeBytes), codeLength, codeCapacity);
}

/* static */
SharedCodeSegment CodeSegment::createFromMasm(MacroAssembler& masm,
                                              const LinkData& linkData) {
  uint32_t codeLength = masm.bytesNeeded();
  uint32_t codeCapacity = RoundupCodeLength(codeLength);
  Maybe<AutoMarkJitCodeWritableForThread> writable;
  UniqueCodeBytes codeBytes = AllocateCodeBytes(writable, codeCapacity);
  if (!codeBytes) {
    return nullptr;
  }

  masm.executableCopy(codeBytes.get());

  SharedCodeSegment segment =
      js_new<CodeSegment>(std::move(codeBytes), codeLength, codeCapacity);
  if (!segment || !segment->linkAndMakeExecutable(*writable, linkData)) {
    return nullptr;
  }

  return segment;
}

/* static */
SharedCodeSegment CodeSegment::createFromBytes(const uint8_t* unlinkedBytes,
                                               size_t unlinkedBytesLength,
                                               const LinkData& linkData) {
  uint32_t codeLength = unlinkedBytesLength;
  uint32_t codeCapacity = RoundupCodeLength(codeLength);
  Maybe<AutoMarkJitCodeWritableForThread> writable;
  UniqueCodeBytes codeBytes = AllocateCodeBytes(writable, codeLength);
  if (!codeBytes) {
    return nullptr;
  }

  memcpy(codeBytes.get(), unlinkedBytes, unlinkedBytesLength);

  SharedCodeSegment segment =
      js_new<CodeSegment>(std::move(codeBytes), codeLength, codeCapacity);
  if (!segment || !segment->linkAndMakeExecutable(*writable, linkData)) {
    return nullptr;
  }
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

// When allocating a single stub to a page, we should not always place the stub
// at the beginning of the page as the stubs will tend to thrash the icache by
// creating conflicts (everything ends up in the same cache set).  Instead,
// locate stubs at different line offsets up to 3/4 the system page size (the
// code allocation quantum).
//
// This may be called on background threads, hence the atomic.

static void PadCodeForSingleStub(MacroAssembler& masm) {
  // Assume 64B icache line size
  static uint8_t zeroes[64];

  // The counter serves only to spread the code out, it has no other meaning and
  // can wrap around.
  static mozilla::Atomic<uint32_t, mozilla::MemoryOrdering::ReleaseAcquire>
      counter(0);

  uint32_t maxPadLines = ((gc::SystemPageSize() * 3) / 4) / sizeof(zeroes);
  uint32_t padLines = counter++ % maxPadLines;
  for (uint32_t i = 0; i < padLines; i++) {
    masm.appendRawCode(zeroes, sizeof(zeroes));
  }
}

static constexpr unsigned LAZY_STUB_LIFO_DEFAULT_CHUNK_SIZE = 8 * 1024;

bool Code::createManyLazyEntryStubs(const WriteGuard& guard,
                                    const Uint32Vector& funcExportIndices,
                                    const CodeBlock& tierCodeBlock,
                                    size_t* stubBlockIndex) const {
  MOZ_ASSERT(funcExportIndices.length());

  LifoAlloc lifo(LAZY_STUB_LIFO_DEFAULT_CHUNK_SIZE);
  TempAllocator alloc(&lifo);
  JitContext jitContext;
  WasmMacroAssembler masm(alloc);

  if (funcExportIndices.length() == 1) {
    PadCodeForSingleStub(masm);
  }

  const FuncExportVector& funcExports = tierCodeBlock.funcExports;
  uint8_t* segmentBase = tierCodeBlock.segment->base();

  CodeRangeVector codeRanges;
  DebugOnly<uint32_t> numExpectedRanges = 0;
  for (uint32_t funcExportIndex : funcExportIndices) {
    const FuncExport& fe = funcExports[funcExportIndex];
    const FuncType& funcType = codeMeta_->getFuncExportType(fe);
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

  size_t codeLength = CodeSegment::AlignBytesNeeded(masm.bytesNeeded());

  if (guard->segments.length() == 0 ||
      !guard->segments[guard->segments.length() - 1]->hasSpace(codeLength)) {
    SharedCodeSegment newSegment = CodeSegment::createEmpty(codeLength);
    if (!newSegment) {
      return false;
    }
    if (!guard->segments.emplaceBack(std::move(newSegment))) {
      return false;
    }
  }

  MOZ_ASSERT(guard->segments.length() > 0);
  CodeSegment* segment = guard->segments[guard->segments.length() - 1].get();

  uint8_t* codePtr = nullptr;
  segment->claimSpace(codeLength, &codePtr);
  size_t offsetInSegment = codePtr - segment->base();

  UniqueCodeBlock stubCodeBlock =
      MakeUnique<CodeBlock>(CodeBlockKind::LazyStubs);
  if (!stubCodeBlock) {
    return false;
  }
  stubCodeBlock->segment = segment;
  stubCodeBlock->codeBase = codePtr;
  stubCodeBlock->codeLength = codeLength;
  stubCodeBlock->codeRanges = std::move(codeRanges);

  {
    AutoMarkJitCodeWritableForThread writable;
    masm.executableCopy(codePtr);
    PatchDebugSymbolicAccesses(codePtr, masm);
    memset(codePtr + masm.bytesNeeded(), 0, codeLength - masm.bytesNeeded());

    for (const CodeLabel& label : masm.codeLabels()) {
      Assembler::Bind(codePtr, label);
    }
  }

  if (!ExecutableAllocator::makeExecutableAndFlushICache(codePtr, codeLength)) {
    return false;
  }

  *stubBlockIndex = guard->blocks.length();

  uint32_t codeRangeIndex = 0;
  for (uint32_t funcExportIndex : funcExportIndices) {
    const FuncExport& fe = funcExports[funcExportIndex];
    const FuncType& funcType = codeMeta_->getFuncExportType(fe);

    LazyFuncExport lazyExport(fe.funcIndex(), *stubBlockIndex, codeRangeIndex,
                              tierCodeBlock.tier());

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
      MOZ_ASSERT(guard->lazyExports[exportIndex].tier == Tier::Baseline);
      guard->lazyExports[exportIndex] = std::move(lazyExport);
    } else if (!guard->lazyExports.insert(
                   guard->lazyExports.begin() + exportIndex,
                   std::move(lazyExport))) {
      return false;
    }
  }

  // Initialization makes the code block visible to the whole process through
  // the process code map. We must wait until we're no longer initializing the
  // code block to do it.
  if (!stubCodeBlock->initialize(*tierCodeBlock.code)) {
    return false;
  }

  return guard->blocks.append(std::move(stubCodeBlock));
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
  const FuncType& funcType = codeMeta_->getFuncExportType(fe);

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
  Tier tier = bestTier();

  size_t funcExportIndex;
  *funcExport = &codeBlock(tier).lookupFuncExport(funcIndex, &funcExportIndex);

  const FuncExport& fe = **funcExport;
  if (fe.hasEagerStubs()) {
    *interpEntry = segment(tier).base() + fe.eagerInterpEntryOffset();
    return true;
  }

  MOZ_ASSERT(!codeMetaForAsmJS_, "only wasm can lazily export functions");

  auto guard = data_.writeLock();
  *interpEntry = lookupLazyInterpEntry(guard, funcIndex);
  if (*interpEntry) {
    return true;
  }

  const CodeBlock& tierCodeBlock = codeBlock(tier);
  return createOneLazyEntryStub(guard, funcExportIndex, tierCodeBlock,
                                interpEntry);
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

  for (size_t i = 0; i < guard->lazyExports.length(); i++) {
    const LazyFuncExport& lfe = guard->lazyExports[i];
    MOZ_ASSERT(lfe.tier == Tier::Baseline);
    size_t funcExportIndex;
    tier2Code.lookupFuncExport(lfe.funcIndex, &funcExportIndex);
    funcExportIndices.infallibleAppend(funcExportIndex);
  }

  size_t stubBlockIndex;
  if (!createManyLazyEntryStubs(guard, funcExportIndices, tier2Code,
                                &stubBlockIndex)) {
    return false;
  }

  outStubBlockIndex->emplace(stubBlockIndex);
  return true;
}

bool Code::finishCompleteTier2(const LinkData& linkData,
                               UniqueCodeBlock tier2Code) const {
  MOZ_RELEASE_ASSERT(bestTier() == Tier::Baseline &&
                     tier2Code->tier() == Tier::Optimized);
  // Publish this code to the process wide map.
  if (!tier2Code->initialize(*this)) {
    return false;
  }

  // Acquire the write guard before we start mutating anything. We hold this
  // for the minimum amount of time necessary.
  {
    auto guard = data_.writeLock();

    // Before we can make tier-2 live, we need to compile tier2 versions of any
    // extant tier1 lazy stubs (otherwise, tiering would break the assumption
    // that any extant exported wasm function has had a lazy entry stub already
    // compiled for it).
    //
    // Also see doc block for stubs in WasmJS.cpp.
    Maybe<size_t> stub2Index;
    if (!createTier2LazyEntryStubs(guard, *tier2Code.get(), &stub2Index)) {
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
    tier2_ = std::move(tier2Code);
    hasTier2_ = true;
    MOZ_ASSERT(hasTier2());

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
  uint8_t* base = segment(Tier::Optimized).base();
  for (const CodeRange& cr : codeBlock(Tier::Optimized).codeRanges) {
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

void* Code::lookupLazyInterpEntry(const WriteGuard& guard,
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
  const LazyFuncExport& fe = guard->lazyExports[match];
  const CodeBlock& block = *guard->blocks[fe.lazyStubBlockIndex];
  const CodeSegment& segment = *block.segment;
  return segment.base() + block.codeRanges[fe.funcCodeRangeIndex].begin();
}

CodeBlock::~CodeBlock() {
  if (unregisterOnDestroy_) {
    UnregisterCodeBlock(this);
  }
}

bool CodeBlock::initialize(const Code& code) {
  MOZ_ASSERT(!initialized());
  this->code = &code;
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
           funcImports.sizeOfExcludingThis(mallocSizeOf) +
           funcExports.sizeOfExcludingThis(mallocSizeOf);
  ;
}

const CodeRange* CodeBlock::lookupRange(const void* pc) const {
  CodeRange::OffsetInCode target((uint8_t*)pc - segment->base());
  return LookupInSorted(codeRanges, target);
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

bool JumpTables::initialize(CompileMode mode, const CodeBlock& tier1) {
  static_assert(JSScript::offsetOfJitCodeRaw() == 0,
                "wasm fast jit entry is at (void*) jit[funcIndex]");

  mode_ = mode;

  size_t numFuncs = 0;
  for (const CodeRange& cr : tier1.codeRanges) {
    if (cr.isFunction()) {
      numFuncs++;
    }
  }

  numFuncs_ = numFuncs;

  if (mode_ == CompileMode::Tier1) {
    tiering_ = TablePointer(js_pod_calloc<void*>(numFuncs));
    if (!tiering_) {
      return false;
    }
  }

  // The number of jit entries is overestimated, but it is simpler when
  // filling/looking up the jit entries and safe (worst case we'll crash
  // because of a null deref when trying to call the jit entry of an
  // unexported function).
  jit_ = TablePointer(js_pod_calloc<void*>(numFuncs));
  if (!jit_) {
    return false;
  }

  uint8_t* codeBase = tier1.segment->base();
  for (const CodeRange& cr : tier1.codeRanges) {
    if (cr.isFunction()) {
      setTieringEntry(cr.funcIndex(), codeBase + cr.funcTierEntry());
    } else if (cr.isJitEntry()) {
      setJitEntry(cr.funcIndex(), codeBase + cr.begin());
    }
  }
  return true;
}

Code::Code(const CodeMetadata& codeMeta,
           const CodeMetadataForAsmJS* codeMetaForAsmJS, UniqueCodeBlock tier1,
           JumpTables&& maybeJumpTables)
    : data_(mutexid::WasmCodeProtected),
      codeMeta_(&codeMeta),
      codeMetaForAsmJS_(codeMetaForAsmJS),
      tier1_(std::move(tier1)),
      profilingLabels_(mutexid::WasmCodeProfilingLabels,
                       CacheableCharsVector()),
      jumpTables_(std::move(maybeJumpTables)),
      trapCode_(nullptr) {}

bool Code::initialize(const LinkData& linkData) {
  MOZ_ASSERT(!initialized());

  if (!tier1_->initialize(*this)) {
    return false;
  }
  trapCode_ = tier1_->segment->base() + linkData.trapOffset;

  MOZ_ASSERT(initialized());
  return true;
}

uint32_t Code::getFuncIndex(JSFunction* fun) const {
  MOZ_ASSERT(fun->isWasm() || fun->isAsmJSNative());
  if (!fun->isWasmWithJitEntry()) {
    return fun->wasmFuncIndex();
  }
  return jumpTables_.funcIndexFromJitEntry(fun->wasmJitEntry());
}

Tiers Code::tiers() const {
  if (hasTier2()) {
    return Tiers(tier1_->tier(), tier2_->tier());
  }
  return Tiers(tier1_->tier());
}

bool Code::hasTier(Tier t) const {
  if (hasTier2() && tier2_->tier() == t) {
    return true;
  }
  return tier1_->tier() == t;
}

Tier Code::stableTier() const { return tier1_->tier(); }

Tier Code::bestTier() const {
  if (hasTier2()) {
    return tier2_->tier();
  }
  return tier1_->tier();
}

const CodeBlock& Code::codeBlock(Tier tier) const {
  switch (tier) {
    case Tier::Baseline:
      if (tier1_->tier() == Tier::Baseline) {
        MOZ_ASSERT(tier1_->initialized());
        return *tier1_;
      }
      MOZ_CRASH("No code segment at this tier");
    case Tier::Optimized:
      if (tier1_->tier() == Tier::Optimized) {
        MOZ_ASSERT(tier1_->initialized());
        return *tier1_;
      }
      // It is incorrect to ask for the optimized tier without there being such
      // a tier and the tier having been committed.  The guard here could
      // instead be `if (hasTier2()) ... ` but codeBlock(t) should not be called
      // in contexts where that test is necessary.
      MOZ_RELEASE_ASSERT(hasTier2());
      MOZ_ASSERT(tier2_->initialized());
      return *tier2_;
  }
  MOZ_CRASH();
}

struct CallSiteRetAddrOffset {
  const CallSiteVector& callSites;
  explicit CallSiteRetAddrOffset(const CallSiteVector& callSites)
      : callSites(callSites) {}
  uint32_t operator[](size_t index) const {
    return callSites[index].returnAddressOffset();
  }
};

const CallSite* Code::lookupCallSite(void* returnAddress) const {
  for (Tier t : tiers()) {
    uint32_t target = ((uint8_t*)returnAddress) - segment(t).base();
    size_t lowerBound = 0;
    size_t upperBound = codeBlock(t).callSites.length();

    size_t match;
    if (BinarySearch(CallSiteRetAddrOffset(codeBlock(t).callSites), lowerBound,
                     upperBound, target, &match)) {
      return &codeBlock(t).callSites[match];
    }
  }

  return nullptr;
}

const CodeRange* Code::lookupFuncRange(void* pc) const {
  for (Tier t : tiers()) {
    const CodeRange* result = codeBlock(t).lookupRange(pc);
    if (result && result->isFunction()) {
      return result;
    }
  }
  return nullptr;
}

const StackMap* Code::lookupStackMap(uint8_t* nextPC) const {
  for (Tier t : tiers()) {
    const StackMap* result = codeBlock(t).stackMaps.findMap(nextPC);
    if (result) {
      return result;
    }
  }
  return nullptr;
}

const wasm::TryNote* Code::lookupTryNote(void* pc, Tier* tier) const {
  for (Tier t : tiers()) {
    const TryNote* result = codeBlock(t).lookupTryNote(pc);
    if (result) {
      *tier = t;
      return result;
    }
  }
  return nullptr;
}

struct TrapSitePCOffset {
  const TrapSiteVector& trapSites;
  explicit TrapSitePCOffset(const TrapSiteVector& trapSites)
      : trapSites(trapSites) {}
  uint32_t operator[](size_t index) const { return trapSites[index].pcOffset; }
};

bool Code::lookupTrap(void* pc, Trap* trapOut, BytecodeOffset* bytecode) const {
  for (Tier t : tiers()) {
    uint32_t target = ((uint8_t*)pc) - segment(t).base();
    const TrapSiteVectorArray& trapSitesArray = codeBlock(t).trapSites;
    for (Trap trap : MakeEnumeratedRange(Trap::Limit)) {
      const TrapSiteVector& trapSites = trapSitesArray[trap];

      size_t upperBound = trapSites.length();
      size_t match;
      if (BinarySearch(TrapSitePCOffset(trapSites), 0, upperBound, target,
                       &match)) {
        MOZ_ASSERT(codeBlock(t).containsCodePC(pc));
        *trapOut = trap;
        *bytecode = trapSites[match].bytecode;
        return true;
      }
    }
  }

  return false;
}

bool Code::lookupFunctionTier(const CodeRange* codeRange, Tier* tier) const {
  // This logic only works if the codeRange is a function, and therefore only
  // exists in metadata and not a lazy stub tier. Generalizing to access lazy
  // stubs would require taking a lock, which is undesirable for the profiler.
  MOZ_ASSERT(codeRange->isFunction());
  for (Tier t : tiers()) {
    const CodeBlock& code = codeBlock(t);
    if (codeRange >= code.codeRanges.begin() &&
        codeRange < code.codeRanges.end()) {
      *tier = t;
      return true;
    }
  }
  return false;
}

struct UnwindInfoPCOffset {
  const CodeRangeUnwindInfoVector& info;
  explicit UnwindInfoPCOffset(const CodeRangeUnwindInfoVector& info)
      : info(info) {}
  uint32_t operator[](size_t index) const { return info[index].offset(); }
};

const CodeRangeUnwindInfo* Code::lookupUnwindInfo(void* pc) const {
  for (Tier t : tiers()) {
    uint32_t target = ((uint8_t*)pc) - segment(t).base();
    const CodeRangeUnwindInfoVector& unwindInfoArray =
        codeBlock(t).codeRangeUnwindInfos;
    size_t match;
    const CodeRangeUnwindInfo* info = nullptr;
    if (BinarySearch(UnwindInfoPCOffset(unwindInfoArray), 0,
                     unwindInfoArray.length(), target, &match)) {
      info = &unwindInfoArray[match];
    } else {
      // Exact match is not found, using insertion point to get the previous
      // info entry; skip if info is outside of codeRangeUnwindInfos.
      if (match == 0) continue;
      if (match == unwindInfoArray.length()) {
        MOZ_ASSERT(unwindInfoArray[unwindInfoArray.length() - 1].unwindHow() ==
                   CodeRangeUnwindInfo::Normal);
        continue;
      }
      info = &unwindInfoArray[match - 1];
    }
    return info->unwindHow() == CodeRangeUnwindInfo::Normal ? nullptr : info;
  }
  return nullptr;
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

  for (const CodeRange& codeRange : codeBlock(stableTier()).codeRanges) {
    if (!codeRange.isFunction()) {
      continue;
    }

    Int32ToCStringBuf cbuf;
    size_t bytecodeStrLen;
    const char* bytecodeStr =
        Uint32ToCString(&cbuf, codeRange.funcLineOrBytecode(), &bytecodeStrLen);
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
      return;
    }

    if (const char* filename = codeMeta().filename.get()) {
      if (!name.append(filename, strlen(filename))) {
        return;
      }
    } else {
      if (!name.append('?')) {
        return;
      }
    }

    if (!name.append(':') || !name.append(bytecodeStr, bytecodeStrLen) ||
        !name.append(")\0", 2)) {
      return;
    }

    UniqueChars label(name.extractOrCopyRawBuffer());
    if (!label) {
      return;
    }

    if (codeRange.funcIndex() >= labels->length()) {
      if (!labels->resize(codeRange.funcIndex() + 1)) {
        return;
      }
    }

    ((CacheableCharsVector&)labels)[codeRange.funcIndex()] = std::move(label);
  }
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
      mallocSizeOf(this) +
      guard->lazyExports.sizeOfExcludingThis(mallocSizeOf) +
      (codeMetaForAsmJS() ? codeMetaForAsmJS()->sizeOfIncludingThisIfNotSeen(
                                mallocSizeOf, seenCodeMetaForAsmJS)
                          : 0) +
      profilingLabels_.lock()->sizeOfExcludingThis(mallocSizeOf) +
      jumpTables_.sizeOfMiscExcludingThis();
  for (const SharedCodeSegment& stub : guard->segments) {
    stub->addSizeOfMisc(mallocSizeOf, code, data);
  }

  for (auto t : tiers()) {
    codeBlock(t).addSizeOfMisc(mallocSizeOf, code, data);
  }
}

void Code::disassemble(JSContext* cx, Tier tier, int kindSelection,
                       PrintCallback printString) const {
  const CodeBlock& codeBlock = this->codeBlock(tier);
  const CodeSegment& segment = this->segment(tier);

  for (const CodeRange& range : codeBlock.codeRanges) {
    if (kindSelection & (1 << range.kind())) {
      MOZ_ASSERT(range.begin() < segment.lengthBytes());
      MOZ_ASSERT(range.end() < segment.lengthBytes());

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
        if (codeMetaForAsmJS()) {
          ok = codeMetaForAsmJS()->getFuncNameForAsmJS(range.funcIndex(),
                                                       &namebuf);
        } else {
          ok = codeMeta().getFuncNameForWasm(NameContext::Standalone,
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

      uint8_t* theCode = segment.base() + range.begin();
      jit::Disassemble(theCode, range.end() - range.begin(), printString);
    }
  }
}

// Return a map with names and associated statistics
MetadataAnalysisHashMap Code::metadataAnalysis(JSContext* cx) const {
  MetadataAnalysisHashMap hashmap;
  if (!hashmap.reserve(15)) {
    return hashmap;
  }

  for (auto t : tiers()) {
    size_t length = codeBlock(t).funcToCodeRange.length();
    length += codeBlock(t).codeRanges.length();
    length += codeBlock(t).callSites.length();
    length += codeBlock(t).trapSites.sumOfLengths();
    length += codeBlock(t).funcImports.length();
    length += codeBlock(t).funcExports.length();
    length += codeBlock(t).stackMaps.length();
    length += codeBlock(t).tryNotes.length();

    hashmap.putNewInfallible("metadata length", length);

    // Iterate over the Code Ranges and accumulate all pieces of code.
    size_t code_size = 0;
    for (const CodeRange& codeRange : codeBlock(stableTier()).codeRanges) {
      if (!codeRange.isFunction()) {
        continue;
      }
      code_size += codeRange.end() - codeRange.begin();
    }

    hashmap.putNewInfallible("stackmaps number",
                             this->codeBlock(t).stackMaps.length());
    hashmap.putNewInfallible("trapSites number",
                             this->codeBlock(t).trapSites.sumOfLengths());
    hashmap.putNewInfallible("codeRange size in bytes", code_size);
    hashmap.putNewInfallible("code segment capacity",
                             this->codeBlock(t).segment->capacityBytes());

    auto mallocSizeOf = cx->runtime()->debuggerMallocSizeOf;

    hashmap.putNewInfallible(
        "funcToCodeRange size",
        codeBlock(t).funcToCodeRange.sizeOfExcludingThis(mallocSizeOf));
    hashmap.putNewInfallible(
        "codeRanges size",
        codeBlock(t).codeRanges.sizeOfExcludingThis(mallocSizeOf));
    hashmap.putNewInfallible(
        "callSites size",
        codeBlock(t).callSites.sizeOfExcludingThis(mallocSizeOf));
    hashmap.putNewInfallible(
        "tryNotes size",
        codeBlock(t).tryNotes.sizeOfExcludingThis(mallocSizeOf));
    hashmap.putNewInfallible(
        "trapSites size",
        codeBlock(t).trapSites.sizeOfExcludingThis(mallocSizeOf));
    hashmap.putNewInfallible(
        "stackMaps size",
        codeBlock(t).stackMaps.sizeOfExcludingThis(mallocSizeOf));
    hashmap.putNewInfallible(
        "funcImports size",
        codeBlock(t).funcImports.sizeOfExcludingThis(mallocSizeOf));
    hashmap.putNewInfallible(
        "funcExports size",
        codeBlock(t).funcExports.sizeOfExcludingThis(mallocSizeOf));
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
