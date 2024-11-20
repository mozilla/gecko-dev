/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Copyright 2015 Mozilla Foundation
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

#include "wasm/WasmGenerator.h"

#include "mozilla/EnumeratedRange.h"
#include "mozilla/SHA1.h"

#include <algorithm>

#include "jit/Assembler.h"
#include "jit/JitOptions.h"
#include "js/Printf.h"
#include "threading/Thread.h"
#include "util/Memory.h"
#include "util/Text.h"
#include "vm/HelperThreads.h"
#include "vm/Time.h"
#include "wasm/WasmBaselineCompile.h"
#include "wasm/WasmCompile.h"
#include "wasm/WasmGC.h"
#include "wasm/WasmIonCompile.h"
#include "wasm/WasmStubs.h"
#include "wasm/WasmSummarizeInsn.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

using mozilla::EnumeratedArray;
using mozilla::MakeEnumeratedRange;

bool CompiledCode::swap(MacroAssembler& masm) {
  MOZ_ASSERT(bytes.empty());
  if (!masm.swapBuffer(bytes)) {
    return false;
  }

  callSites.swap(masm.callSites());
  callSiteTargets.swap(masm.callSiteTargets());
  trapSites.swap(masm.trapSites());
  symbolicAccesses.swap(masm.symbolicAccesses());
  tryNotes.swap(masm.tryNotes());
  codeRangeUnwindInfos.swap(masm.codeRangeUnwindInfos());
  callRefMetricsPatches.swap(masm.callRefMetricsPatches());
  codeLabels.swap(masm.codeLabels());
  return true;
}

// ****************************************************************************
// ModuleGenerator

static const unsigned GENERATOR_LIFO_DEFAULT_CHUNK_SIZE = 4 * 1024;
static const unsigned COMPILATION_LIFO_DEFAULT_CHUNK_SIZE = 64 * 1024;

ModuleGenerator::MacroAssemblerScope::MacroAssemblerScope(LifoAlloc& lifo)
    : masmAlloc(&lifo), masm(masmAlloc, /* limitedSize= */ false) {}

ModuleGenerator::ModuleGenerator(const CodeMetadata& codeMeta,
                                 const CompilerEnvironment& compilerEnv,
                                 CompileState compileState,
                                 const mozilla::Atomic<bool>* cancelled,
                                 UniqueChars* error,
                                 UniqueCharsVector* warnings)
    : compileArgs_(codeMeta.compileArgs.get()),
      compileState_(compileState),
      error_(error),
      warnings_(warnings),
      cancelled_(cancelled),
      codeMeta_(&codeMeta),
      compilerEnv_(&compilerEnv),
      featureUsage_(FeatureUsage::None),
      codeBlock_(nullptr),
      linkData_(nullptr),
      lifo_(GENERATOR_LIFO_DEFAULT_CHUNK_SIZE, js::MallocArena),
      masm_(nullptr),
      debugStubCodeOffset_(0),
      requestTierUpStubCodeOffset_(0),
      updateCallRefMetricsStubCodeOffset_(0),
      lastPatchedCallSite_(0),
      startOfUnpatchedCallsites_(0),
      numCallRefMetrics_(0),
      parallel_(false),
      outstanding_(0),
      currentTask_(nullptr),
      batchedBytecode_(0),
      finishedFuncDefs_(false) {
  MOZ_ASSERT(codeMeta_->isPreparedForCompile());
}

ModuleGenerator::~ModuleGenerator() {
  MOZ_ASSERT_IF(finishedFuncDefs_, !batchedBytecode_);
  MOZ_ASSERT_IF(finishedFuncDefs_, !currentTask_);

  if (parallel_) {
    if (outstanding_) {
      AutoLockHelperThreadState lock;

      // Remove any pending compilation tasks from the worklist.
      size_t removed =
          RemovePendingWasmCompileTasks(taskState_, compileState_, lock);
      MOZ_ASSERT(outstanding_ >= removed);
      outstanding_ -= removed;

      // Wait until all active compilation tasks have finished.
      while (true) {
        MOZ_ASSERT(outstanding_ >= taskState_.finished().length());
        outstanding_ -= taskState_.finished().length();
        taskState_.finished().clear();

        MOZ_ASSERT(outstanding_ >= taskState_.numFailed());
        outstanding_ -= taskState_.numFailed();
        taskState_.numFailed() = 0;

        if (!outstanding_) {
          break;
        }

        taskState_.condVar().wait(lock); /* failed or finished */
      }
    }
  } else {
    MOZ_ASSERT(!outstanding_);
  }

  // Propagate error state.
  if (error_ && !*error_) {
    AutoLockHelperThreadState lock;
    *error_ = std::move(taskState_.errorMessage());
  }
}

bool ModuleGenerator::initializeCompleteTier(
    CodeMetadataForAsmJS* codeMetaForAsmJS) {
  MOZ_ASSERT(compileState_ != CompileState::LazyTier2);

  // Initialize our task system
  if (!initTasks()) {
    return false;
  }

  // If codeMetaForAsmJS is null, we're compiling wasm; else we're compiling
  // asm.js, in whih case it contains wasm::Code-lifetime asm.js-specific
  // information.
  MOZ_ASSERT(isAsmJS() == !!codeMetaForAsmJS);
  codeMetaForAsmJS_ = codeMetaForAsmJS;

  // Generate the shared stubs block, if we're compiling tier-1
  if (compilingTier1() && !prepareTier1()) {
    return false;
  }

  return startCompleteTier();
}

bool ModuleGenerator::initializePartialTier(const Code& code,
                                            uint32_t funcIndex) {
  MOZ_ASSERT(compileState_ == CompileState::LazyTier2);
  MOZ_ASSERT(!isAsmJS());

  // Initialize our task system
  if (!initTasks()) {
    return false;
  }

  // The implied codeMeta must be consistent with the one we already have.
  MOZ_ASSERT(&code.codeMeta() == codeMeta_);

  MOZ_ASSERT(!partialTieringCode_);
  partialTieringCode_ = &code;
  return startPartialTier(funcIndex);
}

bool ModuleGenerator::funcIsCompiledInBlock(uint32_t funcIndex) const {
  return codeBlock_->funcToCodeRange[funcIndex] != BAD_CODE_RANGE;
}

const CodeRange& ModuleGenerator::funcCodeRangeInBlock(
    uint32_t funcIndex) const {
  MOZ_ASSERT(funcIsCompiledInBlock(funcIndex));
  const CodeRange& cr =
      codeBlock_->codeRanges[codeBlock_->funcToCodeRange[funcIndex]];
  MOZ_ASSERT(cr.isFunction());
  return cr;
}

static bool InRange(uint32_t caller, uint32_t callee) {
  // We assume JumpImmediateRange is defined conservatively enough that the
  // slight difference between 'caller' (which is really the return address
  // offset) and the actual base of the relative displacement computation
  // isn't significant.
  uint32_t range = std::min(JitOptions.jumpThreshold, JumpImmediateRange);
  if (caller < callee) {
    return callee - caller < range;
  }
  return caller - callee < range;
}

using OffsetMap =
    HashMap<uint32_t, uint32_t, DefaultHasher<uint32_t>, SystemAllocPolicy>;
using TrapMaybeOffsetArray =
    EnumeratedArray<Trap, mozilla::Maybe<uint32_t>, size_t(Trap::Limit)>;

bool ModuleGenerator::linkCallSites() {
  AutoCreatedBy acb(*masm_, "linkCallSites");

  masm_->haltingAlign(CodeAlignment);

  // Create far jumps for calls that have relative offsets that may otherwise
  // go out of range. This method is called both between function bodies (at a
  // frequency determined by the ISA's jump range) and once at the very end of
  // a module's codegen after all possible calls/traps have been emitted.

  OffsetMap existingCallFarJumps;
  for (; lastPatchedCallSite_ < codeBlock_->callSites.length();
       lastPatchedCallSite_++) {
    const CallSite& callSite = codeBlock_->callSites[lastPatchedCallSite_];
    const CallSiteTarget& target = callSiteTargets_[lastPatchedCallSite_];
    uint32_t callerOffset = callSite.returnAddressOffset();
    switch (callSite.kind()) {
      case CallSiteDesc::Import:
      case CallSiteDesc::Indirect:
      case CallSiteDesc::IndirectFast:
      case CallSiteDesc::Symbolic:
      case CallSiteDesc::Breakpoint:
      case CallSiteDesc::EnterFrame:
      case CallSiteDesc::LeaveFrame:
      case CallSiteDesc::CollapseFrame:
      case CallSiteDesc::FuncRef:
      case CallSiteDesc::FuncRefFast:
      case CallSiteDesc::ReturnStub:
      case CallSiteDesc::StackSwitch:
      case CallSiteDesc::RequestTierUp:
        break;
      case CallSiteDesc::ReturnFunc:
      case CallSiteDesc::Func: {
        auto patch = [this, callSite](uint32_t callerOffset,
                                      uint32_t calleeOffset) {
          if (callSite.kind() == CallSiteDesc::ReturnFunc) {
            masm_->patchFarJump(CodeOffset(callerOffset), calleeOffset);
          } else {
            MOZ_ASSERT(callSite.kind() == CallSiteDesc::Func);
            masm_->patchCall(callerOffset, calleeOffset);
          }
        };
        if (funcIsCompiledInBlock(target.funcIndex())) {
          uint32_t calleeOffset =
              funcCodeRangeInBlock(target.funcIndex()).funcUncheckedCallEntry();
          if (InRange(callerOffset, calleeOffset)) {
            patch(callerOffset, calleeOffset);
            break;
          }
        }

        OffsetMap::AddPtr p =
            existingCallFarJumps.lookupForAdd(target.funcIndex());
        if (!p) {
          Offsets offsets;
          offsets.begin = masm_->currentOffset();
          if (!callFarJumps_.emplaceBack(target.funcIndex(),
                                         masm_->farJumpWithPatch().offset())) {
            return false;
          }
          offsets.end = masm_->currentOffset();
          if (masm_->oom()) {
            return false;
          }
          if (!codeBlock_->codeRanges.emplaceBack(CodeRange::FarJumpIsland,
                                                  offsets)) {
            return false;
          }
          if (!existingCallFarJumps.add(p, target.funcIndex(), offsets.begin)) {
            return false;
          }
        }

        patch(callerOffset, p->value());
        break;
      }
    }
  }

  masm_->flushBuffer();
  return !masm_->oom();
}

void ModuleGenerator::noteCodeRange(uint32_t codeRangeIndex,
                                    const CodeRange& codeRange) {
  switch (codeRange.kind()) {
    case CodeRange::Function:
      MOZ_ASSERT(codeBlock_->funcToCodeRange[codeRange.funcIndex()] ==
                 BAD_CODE_RANGE);
      codeBlock_->funcToCodeRange.insertInfallible(codeRange.funcIndex(),
                                                   codeRangeIndex);
      break;
    case CodeRange::InterpEntry:
      codeBlock_->lookupFuncExport(codeRange.funcIndex())
          .initEagerInterpEntryOffset(codeRange.begin());
      break;
    case CodeRange::JitEntry:
      // Nothing to do: jit entries are linked in the jump tables.
      break;
    case CodeRange::ImportJitExit:
      funcImports_[codeRange.funcIndex()].initJitExitOffset(codeRange.begin());
      break;
    case CodeRange::ImportInterpExit:
      funcImports_[codeRange.funcIndex()].initInterpExitOffset(
          codeRange.begin());
      break;
    case CodeRange::DebugStub:
      MOZ_ASSERT(!debugStubCodeOffset_);
      debugStubCodeOffset_ = codeRange.begin();
      break;
    case CodeRange::RequestTierUpStub:
      MOZ_ASSERT(!requestTierUpStubCodeOffset_);
      requestTierUpStubCodeOffset_ = codeRange.begin();
      break;
    case CodeRange::UpdateCallRefMetricsStub:
      MOZ_ASSERT(!updateCallRefMetricsStubCodeOffset_);
      updateCallRefMetricsStubCodeOffset_ = codeRange.begin();
      break;
    case CodeRange::TrapExit:
      MOZ_ASSERT(!linkData_->trapOffset);
      linkData_->trapOffset = codeRange.begin();
      break;
    case CodeRange::Throw:
      // Jumped to by other stubs, so nothing to do.
      break;
    case CodeRange::FarJumpIsland:
    case CodeRange::BuiltinThunk:
      MOZ_CRASH("Unexpected CodeRange kind");
  }
}

// Append every element from `srcVec` where `filterOp(srcElem) == true`.
// Applies `mutateOp(dstElem)` to every element that is appended.
template <class Vec, class FilterOp, class MutateOp>
static bool AppendForEach(Vec* dstVec, const Vec& srcVec, FilterOp filterOp,
                          MutateOp mutateOp) {
  // Eagerly grow the vector to the whole src vector. Any filtered elements
  // will be trimmed later.
  if (!dstVec->growByUninitialized(srcVec.length())) {
    return false;
  }

  using T = typename Vec::ElementType;

  T* dstBegin = dstVec->begin();
  T* dstEnd = dstVec->end();

  // We appended srcVec.length() elements at the beginning, so we append
  // elements starting at the first uninitialized element.
  T* dst = dstEnd - srcVec.length();

  for (const T* src = srcVec.begin(); src != srcVec.end(); src++) {
    if (!filterOp(src)) {
      continue;
    }
    new (dst) T(*src);
    mutateOp(dst - dstBegin, dst);
    dst++;
  }

  // Trim off the filtered out elements that were eagerly added at the
  // beginning
  size_t newSize = dst - dstBegin;
  if (newSize != dstVec->length()) {
    dstVec->shrinkTo(newSize);
  }

  return true;
}

template <typename T>
bool FilterNothing(const T* element) {
  return true;
}

// The same as the above `AppendForEach`, without performing any filtering.
template <class Vec, class MutateOp>
static bool AppendForEach(Vec* dstVec, const Vec& srcVec, MutateOp mutateOp) {
  using T = typename Vec::ElementType;
  return AppendForEach(dstVec, srcVec, &FilterNothing<T>, mutateOp);
}

bool ModuleGenerator::linkCompiledCode(CompiledCode& code) {
  AutoCreatedBy acb(*masm_, "ModuleGenerator::linkCompiledCode");
  JitContext jcx;

  // Combine observed features from the compiled code into the metadata
  featureUsage_ |= code.featureUsage;

  if (compilingTier1() && mode() == CompileMode::LazyTiering) {
    // All the CallRefMetrics from this batch of functions will start indexing
    // at our current length of metrics.
    uint32_t startOfCallRefMetrics = numCallRefMetrics_;

    for (const FuncCompileOutput& func : code.funcs) {
      // We only compile defined functions, not imported functions
      MOZ_ASSERT(func.index >= codeMeta_->numFuncImports);
      uint32_t funcDefIndex = func.index - codeMeta_->numFuncImports;

      // This function should only be compiled once
      MOZ_ASSERT(funcDefFeatureUsages_[funcDefIndex] == FeatureUsage::None);

      // Track the feature usage for this function
      funcDefFeatureUsages_[funcDefIndex] = func.featureUsage;

      // Record the range of CallRefMetrics this function owns. The metrics
      // will be processed below when we patch the offsets into code.
      MOZ_ASSERT(func.callRefMetricsRange.begin +
                     func.callRefMetricsRange.length <=
                 code.callRefMetricsPatches.length());
      funcDefCallRefMetrics_[funcDefIndex] = func.callRefMetricsRange;
      funcDefCallRefMetrics_[funcDefIndex].offsetBy(startOfCallRefMetrics);
    }
  } else {
    MOZ_ASSERT(funcDefFeatureUsages_.empty());
    MOZ_ASSERT(funcDefCallRefMetrics_.empty());
    MOZ_ASSERT(code.callRefMetricsPatches.empty());
#ifdef DEBUG
    for (const FuncCompileOutput& func : code.funcs) {
      MOZ_ASSERT(func.callRefMetricsRange.length == 0);
    }
#endif
  }

  // Before merging in new code, if calls in a prior code range might go out of
  // range, insert far jumps to extend the range.

  if (!InRange(startOfUnpatchedCallsites_,
               masm_->size() + code.bytes.length())) {
    startOfUnpatchedCallsites_ = masm_->size();
    if (!linkCallSites()) {
      return false;
    }
  }

  // All code offsets in 'code' must be incremented by their position in the
  // overall module when the code was appended.

  masm_->haltingAlign(CodeAlignment);
  const size_t offsetInModule = masm_->size();
  if (code.bytes.length() != 0 &&
      !masm_->appendRawCode(code.bytes.begin(), code.bytes.length())) {
    return false;
  }

  auto codeRangeOp = [offsetInModule, this](uint32_t codeRangeIndex,
                                            CodeRange* codeRange) {
    codeRange->offsetBy(offsetInModule);
    noteCodeRange(codeRangeIndex, *codeRange);
  };
  if (!AppendForEach(&codeBlock_->codeRanges, code.codeRanges, codeRangeOp)) {
    return false;
  }

  auto callSiteOp = [=](uint32_t, CallSite* cs) {
    cs->offsetBy(offsetInModule);
  };
  if (!AppendForEach(&codeBlock_->callSites, code.callSites, callSiteOp)) {
    return false;
  }

  if (!callSiteTargets_.appendAll(code.callSiteTargets)) {
    return false;
  }

  for (Trap trap : MakeEnumeratedRange(Trap::Limit)) {
    auto trapSiteOp = [=](uint32_t, TrapSite* ts) {
      ts->offsetBy(offsetInModule);
    };
    if (!AppendForEach(&codeBlock_->trapSites[trap], code.trapSites[trap],
                       trapSiteOp)) {
      return false;
    }
  }

  for (const SymbolicAccess& access : code.symbolicAccesses) {
    uint32_t patchAt = offsetInModule + access.patchAt.offset();
    if (!linkData_->symbolicLinks[access.target].append(patchAt)) {
      return false;
    }
  }

  for (const CallRefMetricsPatch& patch : code.callRefMetricsPatches) {
    if (!patch.hasOffsetOfOffsetPatch()) {
      numCallRefMetrics_ += 1;
      continue;
    }

    CodeOffset offset = CodeOffset(patch.offsetOfOffsetPatch());
    offset.offsetBy(offsetInModule);

    size_t callRefIndex = numCallRefMetrics_;
    numCallRefMetrics_ += 1;
    size_t callRefMetricOffset = callRefIndex * sizeof(CallRefMetrics);

    // Compute the offset of the metrics, and patch it. This may overflow,
    // in which case we report an OOM. We might need to do something smarter
    // here.
    if (callRefMetricOffset > (INT32_MAX / sizeof(CallRefMetrics))) {
      return false;
    }

    masm_->patchMove32(offset, Imm32(int32_t(callRefMetricOffset)));
  }

  for (const CodeLabel& codeLabel : code.codeLabels) {
    LinkData::InternalLink link;
    link.patchAtOffset = offsetInModule + codeLabel.patchAt().offset();
    link.targetOffset = offsetInModule + codeLabel.target().offset();
#ifdef JS_CODELABEL_LINKMODE
    link.mode = codeLabel.linkMode();
#endif
    if (!linkData_->internalLinks.append(link)) {
      return false;
    }
  }

  for (size_t i = 0; i < code.stackMaps.length(); i++) {
    StackMaps::Maplet maplet = code.stackMaps.move(i);
    maplet.offsetBy(offsetInModule);
    if (!codeBlock_->stackMaps.add(maplet)) {
      // This function is now the only owner of maplet.map, so we'd better
      // free it right now.
      maplet.map->destroy();
      return false;
    }
  }

  auto unwindInfoOp = [=](uint32_t, CodeRangeUnwindInfo* i) {
    i->offsetBy(offsetInModule);
  };
  if (!AppendForEach(&codeBlock_->codeRangeUnwindInfos,
                     code.codeRangeUnwindInfos, unwindInfoOp)) {
    return false;
  }

  auto tryNoteFilter = [](const TryNote* tn) {
    // Filter out all try notes that were never given a try body. This may
    // happen due to dead code elimination.
    return tn->hasTryBody();
  };
  auto tryNoteOp = [=](uint32_t, TryNote* tn) { tn->offsetBy(offsetInModule); };
  return AppendForEach(&codeBlock_->tryNotes, code.tryNotes, tryNoteFilter,
                       tryNoteOp);
}

static bool ExecuteCompileTask(CompileTask* task, UniqueChars* error) {
  MOZ_ASSERT(task->lifo.isEmpty());
  MOZ_ASSERT(task->output.empty());

  switch (task->compilerEnv.tier()) {
    case Tier::Optimized:
      if (!IonCompileFunctions(task->codeMeta, task->compilerEnv, task->lifo,
                               task->inputs, &task->output, error)) {
        return false;
      }
      break;
    case Tier::Baseline:
      if (!BaselineCompileFunctions(task->codeMeta, task->compilerEnv,
                                    task->lifo, task->inputs, &task->output,
                                    error)) {
        return false;
      }
      break;
  }

  MOZ_ASSERT(task->lifo.isEmpty());
  MOZ_ASSERT(task->inputs.length() == task->output.codeRanges.length());
  task->inputs.clear();
  return true;
}

void CompileTask::runHelperThreadTask(AutoLockHelperThreadState& lock) {
  UniqueChars error;
  bool ok;

  {
    AutoUnlockHelperThreadState unlock(lock);
    ok = ExecuteCompileTask(this, &error);
  }

  // Don't release the lock between updating our state and returning from this
  // method.

  if (!ok || !state.finished().append(this)) {
    state.numFailed()++;
    if (!state.errorMessage()) {
      state.errorMessage() = std::move(error);
    }
  }

  state.condVar().notify_one(); /* failed or finished */
}

ThreadType CompileTask::threadType() {
  switch (compileState) {
    case CompileState::Once:
    case CompileState::EagerTier1:
    case CompileState::LazyTier1:
      return ThreadType::THREAD_TYPE_WASM_COMPILE_TIER1;
    case CompileState::EagerTier2:
    case CompileState::LazyTier2:
      return ThreadType::THREAD_TYPE_WASM_COMPILE_TIER2;
    default:
      MOZ_CRASH();
  }
}

bool ModuleGenerator::initTasks() {
  // Determine whether parallel or sequential compilation is to be used and
  // initialize the CompileTasks that will be used in either mode.

  MOZ_ASSERT(GetHelperThreadCount() > 1);

  MOZ_ASSERT(!parallel_);
  uint32_t numTasks = 1;
  if (  // "obvious" prerequisites for doing off-thread compilation
      CanUseExtraThreads() && GetHelperThreadCPUCount() > 1 &&
      // For lazy tier 2 compilations, the current thread -- running a
      // WasmPartialTier2CompileTask -- is already dedicated to compiling the
      // to-be-tiered-up function.  So don't create a new task for it.
      compileState_ != CompileState::LazyTier2) {
    parallel_ = true;
    numTasks = 2 * GetMaxWasmCompilationThreads();
  }

  if (!tasks_.initCapacity(numTasks)) {
    return false;
  }
  for (size_t i = 0; i < numTasks; i++) {
    tasks_.infallibleEmplaceBack(*codeMeta_, *compilerEnv_, compileState_,
                                 taskState_,
                                 COMPILATION_LIFO_DEFAULT_CHUNK_SIZE);
  }

  if (!freeTasks_.reserve(numTasks)) {
    return false;
  }
  for (size_t i = 0; i < numTasks; i++) {
    freeTasks_.infallibleAppend(&tasks_[i]);
  }
  return true;
}

bool ModuleGenerator::locallyCompileCurrentTask() {
  if (!ExecuteCompileTask(currentTask_, error_)) {
    return false;
  }
  if (!finishTask(currentTask_)) {
    return false;
  }
  currentTask_ = nullptr;
  batchedBytecode_ = 0;
  return true;
}

bool ModuleGenerator::finishTask(CompileTask* task) {
  AutoCreatedBy acb(*masm_, "ModuleGenerator::finishTask");

  masm_->haltingAlign(CodeAlignment);

  if (!linkCompiledCode(task->output)) {
    return false;
  }

  task->output.clear();

  MOZ_ASSERT(task->inputs.empty());
  MOZ_ASSERT(task->output.empty());
  MOZ_ASSERT(task->lifo.isEmpty());
  freeTasks_.infallibleAppend(task);
  return true;
}

bool ModuleGenerator::launchBatchCompile() {
  MOZ_ASSERT(currentTask_);

  if (cancelled_ && *cancelled_) {
    return false;
  }

  if (!parallel_) {
    return locallyCompileCurrentTask();
  }

  if (!StartOffThreadWasmCompile(currentTask_, compileState_)) {
    return false;
  }
  outstanding_++;
  currentTask_ = nullptr;
  batchedBytecode_ = 0;
  return true;
}

bool ModuleGenerator::finishOutstandingTask() {
  MOZ_ASSERT(parallel_);

  CompileTask* task = nullptr;
  {
    AutoLockHelperThreadState lock;
    while (true) {
      MOZ_ASSERT(outstanding_ > 0);

      if (taskState_.numFailed() > 0) {
        return false;
      }

      if (!taskState_.finished().empty()) {
        outstanding_--;
        task = taskState_.finished().popCopy();
        break;
      }

      taskState_.condVar().wait(lock); /* failed or finished */
    }
  }

  // Call outside of the compilation lock.
  return finishTask(task);
}

bool ModuleGenerator::compileFuncDef(uint32_t funcIndex,
                                     uint32_t lineOrBytecode,
                                     const uint8_t* begin, const uint8_t* end,
                                     Uint32Vector&& lineNums) {
  MOZ_ASSERT(!finishedFuncDefs_);
  MOZ_ASSERT(funcIndex < codeMeta_->numFuncs());

  if (compilingTier1()) {
    static_assert(MaxFunctionBytes < UINT32_MAX);
    uint32_t bodyLength = (uint32_t)(end - begin);
    funcDefRanges_.infallibleAppend(BytecodeRange(lineOrBytecode, bodyLength));
  }

  uint32_t threshold;
  switch (tier()) {
    case Tier::Baseline:
      threshold = JitOptions.wasmBatchBaselineThreshold;
      break;
    case Tier::Optimized:
      threshold = JitOptions.wasmBatchIonThreshold;
      break;
    default:
      MOZ_CRASH("Invalid tier value");
      break;
  }

  uint32_t funcBytecodeLength = end - begin;

  // Do not go over the threshold if we can avoid it: spin off the compilation
  // before appending the function if we would go over.  (Very large single
  // functions may still exceed the threshold but this is fine; it'll be very
  // uncommon and is in any case safely handled by the MacroAssembler's buffer
  // limit logic.)

  if (currentTask_ && currentTask_->inputs.length() &&
      batchedBytecode_ + funcBytecodeLength > threshold) {
    if (!launchBatchCompile()) {
      return false;
    }
  }

  if (!currentTask_) {
    if (freeTasks_.empty() && !finishOutstandingTask()) {
      return false;
    }
    currentTask_ = freeTasks_.popCopy();
  }

  if (!currentTask_->inputs.emplaceBack(funcIndex, lineOrBytecode, begin, end,
                                        std::move(lineNums))) {
    return false;
  }

  batchedBytecode_ += funcBytecodeLength;
  MOZ_ASSERT(batchedBytecode_ <= MaxCodeSectionBytes);
  return true;
}

bool ModuleGenerator::finishFuncDefs() {
  MOZ_ASSERT(!finishedFuncDefs_);

  if (currentTask_ && !locallyCompileCurrentTask()) {
    return false;
  }

  finishedFuncDefs_ = true;
  return true;
}

static void CheckCodeBlock(const CodeBlock& codeBlock) {
#if defined(DEBUG)
  // Assert all sorted metadata is sorted.
  uint32_t last = 0;
  for (const CodeRange& codeRange : codeBlock.codeRanges) {
    MOZ_ASSERT(codeRange.begin() >= last);
    last = codeRange.end();
  }

  last = 0;
  for (const CallSite& callSite : codeBlock.callSites) {
    MOZ_ASSERT(callSite.returnAddressOffset() >= last);
    last = callSite.returnAddressOffset();
  }

  for (Trap trap : MakeEnumeratedRange(Trap::Limit)) {
    last = 0;
    for (const TrapSite& trapSite : codeBlock.trapSites[trap]) {
      MOZ_ASSERT(trapSite.pcOffset >= last);
      last = trapSite.pcOffset;
    }
  }

  last = 0;
  for (const CodeRangeUnwindInfo& info : codeBlock.codeRangeUnwindInfos) {
    MOZ_ASSERT(info.offset() >= last);
    last = info.offset();
  }

  // Try notes should be sorted so that the end of ranges are in rising order
  // so that the innermost catch handler is chosen.
  last = 0;
  for (const wasm::TryNote& tryNote : codeBlock.tryNotes) {
    MOZ_ASSERT(tryNote.tryBodyEnd() >= last);
    MOZ_ASSERT(tryNote.tryBodyEnd() > tryNote.tryBodyBegin());
    last = tryNote.tryBodyBegin();
  }

  // Check that the stackmap vector is sorted with no duplicates, and each
  // entry points to a plausible instruction.
  const uint8_t* previousNextInsnAddr = nullptr;
  for (size_t i = 0; i < codeBlock.stackMaps.length(); i++) {
    const StackMaps::Maplet& maplet = codeBlock.stackMaps.get(i);
    MOZ_ASSERT_IF(i > 0, uintptr_t(maplet.nextInsnAddr) >
                             uintptr_t(previousNextInsnAddr));
    previousNextInsnAddr = maplet.nextInsnAddr;

    MOZ_ASSERT(IsPlausibleStackMapKey(maplet.nextInsnAddr),
               "wasm stackmap does not reference a valid insn");
  }

#  if (defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86) ||   \
       defined(JS_CODEGEN_ARM64) || defined(JS_CODEGEN_ARM) || \
       defined(JS_CODEGEN_LOONG64) || defined(JS_CODEGEN_MIPS64))
  // Check that each trapsite is associated with a plausible instruction.  The
  // required instruction kind depends on the trapsite kind.
  //
  // NOTE: currently enabled on x86_{32,64}, arm{32,64}, loongson64 and mips64.
  // Ideally it should be extended to riscv64 too.
  //
  for (Trap trap : MakeEnumeratedRange(Trap::Limit)) {
    const TrapSiteVector& trapSites = codeBlock.trapSites[trap];
    for (const TrapSite& trapSite : trapSites) {
      const uint8_t* insnAddr = ((const uint8_t*)(codeBlock.segment->base())) +
                                uintptr_t(trapSite.pcOffset);
      // `expected` describes the kind of instruction we expect to see at
      // `insnAddr`.  Find out what is actually there and check it matches.
      const TrapMachineInsn expected = trapSite.insn;
      mozilla::Maybe<TrapMachineInsn> actual =
          SummarizeTrapInstruction(insnAddr);
      bool valid = actual.isSome() && actual.value() == expected;
      // This is useful for diagnosing validation failures.
      // if (!valid) {
      //   fprintf(stderr,
      //           "FAIL: reason=%-22s  expected=%-12s  "
      //           "pcOffset=%-5u  addr= %p\n",
      //           NameOfTrap(trap), NameOfTrapMachineInsn(expected),
      //           trapSite.pcOffset, insnAddr);
      //   if (actual.isSome()) {
      //     fprintf(stderr, "FAIL: identified as %s\n",
      //             actual.isSome() ? NameOfTrapMachineInsn(actual.value())
      //                             : "(insn not identified)");
      //   }
      // }
      MOZ_ASSERT(valid, "wasm trapsite does not reference a valid insn");
    }
  }
#  endif
#endif
}

bool ModuleGenerator::startCodeBlock(CodeBlockKind kind) {
  MOZ_ASSERT(!masmScope_ && !linkData_ && !codeBlock_);
  masmScope_.emplace(lifo_);
  masm_ = &masmScope_->masm;
  linkData_ = js::MakeUnique<LinkData>();
  codeBlock_ = js::MakeUnique<CodeBlock>(kind);
  return !!linkData_ && !!codeBlock_;
}

UniqueCodeBlock ModuleGenerator::finishCodeBlock(UniqueLinkData* linkData) {
  // Now that all functions and stubs are generated and their CodeRanges
  // known, patch all calls (which can emit far jumps) and far jumps. Linking
  // can emit tiny far-jump stubs, so there is an ordering dependency here.

  if (!linkCallSites()) {
    return nullptr;
  }

  for (CallFarJump far : callFarJumps_) {
    if (funcIsCompiledInBlock(far.targetFuncIndex)) {
      masm_->patchFarJump(
          jit::CodeOffset(far.jumpOffset),
          funcCodeRangeInBlock(far.targetFuncIndex).funcUncheckedCallEntry());
    } else if (!linkData_->callFarJumps.append(far)) {
      return nullptr;
    }
  }

  lastPatchedCallSite_ = 0;
  startOfUnpatchedCallsites_ = 0;
  callSiteTargets_.clear();
  callFarJumps_.clear();

  // None of the linking or far-jump operations should emit masm metadata.

  MOZ_ASSERT(masm_->callSites().empty());
  MOZ_ASSERT(masm_->callSiteTargets().empty());
  MOZ_ASSERT(masm_->trapSites().empty());
  MOZ_ASSERT(masm_->symbolicAccesses().empty());
  MOZ_ASSERT(masm_->tryNotes().empty());
  MOZ_ASSERT(masm_->codeLabels().empty());

  masm_->finish();
  if (masm_->oom()) {
    return nullptr;
  }

  // The stackmaps aren't yet sorted.  Do so now, since we'll need to
  // binary-search them at GC time.
  codeBlock_->stackMaps.finishAndSort();

  // The try notes also need to be sorted to simplify lookup.
  std::sort(codeBlock_->tryNotes.begin(), codeBlock_->tryNotes.end());

  // These Vectors can get large and the excess capacity can be significant,
  // so realloc them down to size.

  codeBlock_->funcToCodeRange.shrinkStorageToFit();
  codeBlock_->codeRanges.shrinkStorageToFit();
  codeBlock_->callSites.shrinkStorageToFit();
  codeBlock_->trapSites.shrinkStorageToFit();
  codeBlock_->tryNotes.shrinkStorageToFit();
  for (Trap trap : MakeEnumeratedRange(Trap::Limit)) {
    codeBlock_->trapSites[trap].shrinkStorageToFit();
  }

  // Allocate the code storage, copy/link the code from `masm_` into it, set up
  // `codeBlock_->segment / codeBase / codeLength`, and adjust the metadata
  // offsets on `codeBlock_` accordingly.
  if (partialTieringCode_) {
    // We're compiling a single function during tiering.  Place it in its own
    // hardware page, inside an existing CodeSegment if possible, or allocate a
    // new one and use that.  Either way, the chosen CodeSegment will be owned
    // by Code::lazyFuncSegments.
    MOZ_ASSERT(mode() == CompileMode::LazyTiering);

    // Try to allocate from Code::lazyFuncSegments. We do not allow a last-ditch
    // GC here as we may be running in OOL-code that is not ready for a GC.
    uint8_t* codeStart = nullptr;
    uint32_t codeLength = 0;
    codeBlock_->segment = partialTieringCode_->createFuncCodeSegmentFromPool(
        *masm_, *linkData_, /* allowLastDitchGC = */ false, &codeStart,
        &codeLength);
    if (!codeBlock_->segment) {
      warnf("failed to allocate executable memory for module");
      return nullptr;
    }
    codeBlock_->codeBase = codeStart;
    codeBlock_->codeLength = codeLength;

    // All metadata in code block is relative to the start of the code segment
    // we were placed in, so we must adjust offsets for where we were
    // allocated.
    uint32_t codeBlockOffset = codeStart - codeBlock_->segment->base();
    codeBlock_->offsetMetadataBy(codeBlockOffset);
  } else {
    // Create a new CodeSegment for the code and use that.
    codeBlock_->segment = CodeSegment::createFromMasm(
        *masm_, *linkData_, partialTieringCode_.get());
    if (!codeBlock_->segment) {
      warnf("failed to allocate executable memory for module");
      return nullptr;
    }
    codeBlock_->codeBase = codeBlock_->segment->base();
    codeBlock_->codeLength = codeBlock_->segment->lengthBytes();
  }

  // Add the segment base address to the stack map addresses.  See comments
  // at the declaration of CodeBlock::funcToCodeRange for explanation.
  codeBlock_->stackMaps.offsetBy(uintptr_t(codeBlock_->segment->base()));

  // Check that metadata is consistent with the actual code we generated,
  // linked, and loaded.
  CheckCodeBlock(*codeBlock_);

  // Free the macro assembler scope, and reset our masm pointer
  masm_ = nullptr;
  masmScope_ = mozilla::Nothing();

  *linkData = std::move(linkData_);
  return std::move(codeBlock_);
}

bool ModuleGenerator::prepareTier1() {
  if (!startCodeBlock(CodeBlockKind::SharedStubs)) {
    return false;
  }

  // Initialize function definition ranges
  if (!funcDefRanges_.reserve(codeMeta_->numFuncDefs())) {
    return false;
  }

  // Initialize function definition feature usages (only used for lazy tiering
  // and inlining right now).
  if (mode() == CompileMode::LazyTiering &&
      (!funcDefFeatureUsages_.resize(codeMeta_->numFuncDefs()) ||
       !funcDefCallRefMetrics_.resize(codeMeta_->numFuncDefs()))) {
    return false;
  }

  // Initialize function import metadata
  if (!funcImports_.resize(codeMeta_->numFuncImports)) {
    return false;
  }

  // The shared stubs code will contains function definitions for each imported
  // function.
  if (!FuncToCodeRangeMap::createDense(0, codeMeta_->numFuncImports,
                                       &codeBlock_->funcToCodeRange)) {
    return false;
  }

  uint32_t exportedFuncCount = 0;
  for (uint32_t funcIndex = 0; funcIndex < codeMeta_->numFuncImports;
       funcIndex++) {
    const FuncDesc& func = codeMeta_->funcs[funcIndex];
    if (func.isExported()) {
      exportedFuncCount++;
    }
  }
  if (!codeBlock_->funcExports.reserve(exportedFuncCount)) {
    return false;
  }

  for (uint32_t funcIndex = 0; funcIndex < codeMeta_->numFuncImports;
       funcIndex++) {
    const FuncDesc& func = codeMeta_->funcs[funcIndex];
    if (!func.isExported()) {
      continue;
    }

    codeBlock_->funcExports.infallibleEmplaceBack(
        FuncExport(funcIndex, func.isEager()));
  }

  // Generate the stubs for the module first
  CompiledCode& stubCode = tasks_[0].output;
  MOZ_ASSERT(stubCode.empty());

  if (!GenerateStubs(*codeMeta_, funcImports_, codeBlock_->funcExports,
                     &stubCode) ||
      !linkCompiledCode(stubCode)) {
    return false;
  }
  stubCode.clear();

  sharedStubsCodeBlock_ = finishCodeBlock(&sharedStubsLinkData_);
  return !!sharedStubsCodeBlock_;
}

bool ModuleGenerator::startCompleteTier() {
#ifdef JS_JITSPEW
  JS_LOG(wasmPerf, mozilla::LogLevel::Info,
         "CM=..%06lx  MG::startCompleteTier (%s, %u imports, %u functions)",
         (unsigned long)(uintptr_t(codeMeta_) & 0xFFFFFFL),
         tier() == Tier::Baseline ? "BL" : "OPT",
         (uint32_t)codeMeta_->numFuncImports,
         (uint32_t)codeMeta_->numFuncs() - (uint32_t)codeMeta_->numFuncImports);
#endif

  if (!startCodeBlock(CodeBlock::kindFromTier(tier()))) {
    return false;
  }

  // funcToCodeRange maps function indices to code-range indices and all
  // elements will be initialized by the time module generation is finished.

  if (!FuncToCodeRangeMap::createDense(
          codeMeta_->numFuncImports,
          codeMeta_->funcs.length() - codeMeta_->numFuncImports,
          &codeBlock_->funcToCodeRange)) {
    return false;
  }

  // Pre-reserve space for large Vectors to avoid the significant cost of the
  // final reallocs. In particular, the MacroAssembler can be enormous, so be
  // extra conservative. Since large over-reservations may fail when the
  // actual allocations will succeed, ignore OOM failures. Note,
  // shrinkStorageToFit calls at the end will trim off unneeded capacity.

  size_t codeSectionSize =
      codeMeta_->codeSectionRange ? codeMeta_->codeSectionRange->size : 0;

  size_t estimatedCodeSize =
      size_t(1.2 * EstimateCompiledCodeSize(tier(), codeSectionSize));
  (void)masm_->reserve(std::min(estimatedCodeSize, MaxCodeBytesPerProcess));

  (void)codeBlock_->codeRanges.reserve(2 * codeMeta_->numFuncDefs());

  const size_t ByteCodesPerCallSite = 50;
  (void)codeBlock_->callSites.reserve(codeSectionSize / ByteCodesPerCallSite);

  const size_t ByteCodesPerOOBTrap = 10;
  (void)codeBlock_->trapSites[Trap::OutOfBounds].reserve(codeSectionSize /
                                                         ByteCodesPerOOBTrap);

  // Accumulate all exported functions:
  // - explicitly marked as such;
  // - implicitly exported by being an element of function tables;
  // - implicitly exported by being the start function;
  // - implicitly exported by being used in global ref.func initializer
  // ModuleEnvironment accumulates this information for us during decoding,
  // transfer it to the FuncExportVector stored in Metadata.

  uint32_t exportedFuncCount = 0;
  for (uint32_t funcIndex = codeMeta_->numFuncImports;
       funcIndex < codeMeta_->funcs.length(); funcIndex++) {
    const FuncDesc& func = codeMeta_->funcs[funcIndex];
    if (func.isExported()) {
      exportedFuncCount++;
    }
  }
  if (!codeBlock_->funcExports.reserve(exportedFuncCount)) {
    return false;
  }

  for (uint32_t funcIndex = codeMeta_->numFuncImports;
       funcIndex < codeMeta_->funcs.length(); funcIndex++) {
    const FuncDesc& func = codeMeta_->funcs[funcIndex];

    if (!func.isExported()) {
      continue;
    }

    codeBlock_->funcExports.infallibleEmplaceBack(
        FuncExport(funcIndex, func.isEager()));
  }

  return true;
}

bool ModuleGenerator::startPartialTier(uint32_t funcIndex) {
#ifdef JS_JITSPEW
  UTF8Bytes name;
  if (codeMeta_->namePayload.get()) {
    if (!codeMeta_->getFuncNameForWasm(NameContext::Standalone, funcIndex,
                                       &name) ||
        !name.append("\0", 1)) {
      return false;
    }
  }
  uint32_t bytecodeLength = codeMeta_->funcDefRange(funcIndex).size;
  JS_LOG(wasmPerf, mozilla::LogLevel::Info,
         "CM=..%06lx  MG::startPartialTier  fI=%-5u  sz=%-5u  %s",
         (unsigned long)(uintptr_t(codeMeta_) & 0xFFFFFFL), funcIndex,
         bytecodeLength, name.length() > 0 ? name.begin() : "(unknown-name)");
#endif

  if (!startCodeBlock(CodeBlock::kindFromTier(tier()))) {
    return false;
  }

  if (!FuncToCodeRangeMap::createDense(funcIndex, 1,
                                       &codeBlock_->funcToCodeRange)) {
    return false;
  }

  const FuncDesc& func = codeMeta_->funcs[funcIndex];
  if (func.isExported() && !codeBlock_->funcExports.emplaceBack(
                               FuncExport(funcIndex, func.isEager()))) {
    return false;
  }

  return true;
}

UniqueCodeBlock ModuleGenerator::finishTier(UniqueLinkData* linkData) {
  MOZ_ASSERT(finishedFuncDefs_);

  while (outstanding_ > 0) {
    if (!finishOutstandingTask()) {
      return nullptr;
    }
  }

#ifdef DEBUG
  if (mode() != CompileMode::LazyTiering) {
    codeBlock_->funcToCodeRange.assertAllInitialized();
  }
#endif

  // Now that all funcs have been compiled, we can generate entry stubs for
  // the ones that have been exported.

  CompiledCode& stubCode = tasks_[0].output;
  MOZ_ASSERT(stubCode.empty());

  if (!GenerateEntryStubs(*codeMeta_, codeBlock_->funcExports, &stubCode)) {
    return nullptr;
  }

  if (!linkCompiledCode(stubCode)) {
    return nullptr;
  }

  return finishCodeBlock(linkData);
}

// Complete all tier-1 construction and return the resulting Module.  For this
// we will need both codeMeta_ (and maybe codeMetaForAsmJS_) and moduleMeta_.
SharedModule ModuleGenerator::finishModule(
    const ShareableBytes& bytecode, MutableModuleMetadata moduleMeta,
    JS::OptimizedEncodingListener* maybeCompleteTier2Listener) {
  MOZ_ASSERT(compilingTier1());

  UniqueLinkData tier1LinkData;
  UniqueCodeBlock tier1Code = finishTier(&tier1LinkData);
  if (!tier1Code) {
    return nullptr;
  }

  // Record what features we encountered in this module
  moduleMeta->featureUsage = featureUsage_;

  // Copy over data from the Bytecode, which is going away at the end of
  // compilation.
  //
  // In particular, convert the data- and custom-section ranges in the
  // ModuleMetadata into their full-fat versions by copying the underlying
  // data blocks.

  MOZ_ASSERT(moduleMeta->dataSegments.empty());
  if (!moduleMeta->dataSegments.reserve(
          moduleMeta->dataSegmentRanges.length())) {
    return nullptr;
  }
  for (const DataSegmentRange& srcRange : moduleMeta->dataSegmentRanges) {
    MutableDataSegment dstSeg = js_new<DataSegment>();
    if (!dstSeg) {
      return nullptr;
    }
    if (!dstSeg->init(bytecode, srcRange)) {
      return nullptr;
    }
    moduleMeta->dataSegments.infallibleAppend(std::move(dstSeg));
  }

  MOZ_ASSERT(moduleMeta->customSections.empty());
  if (!moduleMeta->customSections.reserve(
          codeMeta_->customSectionRanges.length())) {
    return nullptr;
  }
  for (const CustomSectionRange& srcRange : codeMeta_->customSectionRanges) {
    CustomSection sec;
    if (!sec.name.append(bytecode.begin() + srcRange.nameOffset,
                         srcRange.nameLength)) {
      return nullptr;
    }
    MutableBytes payload = js_new<ShareableBytes>();
    if (!payload) {
      return nullptr;
    }
    if (!payload->append(bytecode.begin() + srcRange.payloadOffset,
                         srcRange.payloadLength)) {
      return nullptr;
    }
    sec.payload = std::move(payload);
    moduleMeta->customSections.infallibleAppend(std::move(sec));
  }

  MutableCodeMetadata codeMeta = moduleMeta->codeMeta;

  // Transfer the function definition ranges
  MOZ_ASSERT(funcDefRanges_.length() == codeMeta->numFuncDefs());
  codeMeta->funcDefRanges = std::move(funcDefRanges_);

  // Transfer the function definition feature usages
  codeMeta->funcDefFeatureUsages = std::move(funcDefFeatureUsages_);
  codeMeta->funcDefCallRefs = std::move(funcDefCallRefMetrics_);
  MOZ_ASSERT_IF(mode() != CompileMode::LazyTiering, numCallRefMetrics_ == 0);
  codeMeta->numCallRefMetrics = numCallRefMetrics_;

  if (mode() == CompileMode::LazyTiering) {
    codeMeta->callRefHints = MutableCallRefHints(
        js_pod_calloc<MutableCallRefHint>(numCallRefMetrics_));
    if (!codeMeta->callRefHints) {
      return nullptr;
    }
  }

  // We keep the bytecode alive for debuggable modules, or if we're doing
  // partial tiering.
  if (debugEnabled()) {
    MOZ_ASSERT(mode() != CompileMode::LazyTiering);
    codeMeta->debugBytecode = &bytecode;
  } else if (mode() == CompileMode::LazyTiering) {
    MutableBytes codeSectionBytecode = js_new<ShareableBytes>();
    if (!codeSectionBytecode) {
      return nullptr;
    }

    if (codeMeta->codeSectionRange) {
      const uint8_t* codeSectionStart =
          bytecode.begin() + codeMeta->codeSectionRange->start;
      if (!codeSectionBytecode->append(codeSectionStart,
                                       codeMeta->codeSectionRange->size)) {
        return nullptr;
      }
    }

    codeMeta->codeSectionBytecode = codeSectionBytecode;
  }

  // Store a reference to the name section on the code metadata
  if (codeMeta_->nameCustomSectionIndex) {
    codeMeta->namePayload =
        moduleMeta->customSections[*codeMeta_->nameCustomSectionIndex].payload;
  }

  // Initialize the debug hash for display urls, if we need to
  if (debugEnabled()) {
    codeMeta->debugEnabled = true;

    static_assert(sizeof(ModuleHash) <= sizeof(mozilla::SHA1Sum::Hash),
                  "The ModuleHash size shall not exceed the SHA1 hash size.");
    mozilla::SHA1Sum::Hash hash;
    mozilla::SHA1Sum sha1Sum;
    sha1Sum.update(bytecode.begin(), bytecode.length());
    sha1Sum.finish(hash);
    memcpy(codeMeta->debugHash, hash, sizeof(ModuleHash));
  }

  // Update statistics in the CodeMeta.
  {
    auto guard = codeMeta->stats.writeLock();
    guard->completeNumFuncs = codeMeta->numFuncDefs();
    guard->completeBCSize = 0;
    for (const BytecodeRange& range : codeMeta->funcDefRanges) {
      guard->completeBCSize += range.size;
    }
    // Now that we know the complete bytecode size for the module, we can set
    // the inlining budget for tiered-up compilation, if appropriate.  See
    // "[SMDOC] Per-function and per-module inlining limits" (WasmHeuristics.h)
    guard->inliningBudget =
        mode() == CompileMode::LazyTiering
            ? int64_t(guard->completeBCSize) * PerModuleMaxInliningRatio
            : 0;
    // But don't be overly stingy for tiny modules.  Function-level inlining
    // limits will still protect us from excessive inlining.
    guard->inliningBudget = std::max<int64_t>(guard->inliningBudget, 1000);
  }

  MutableCode code = js_new<Code>(mode(), *codeMeta_, codeMetaForAsmJS_);
  if (!code || !code->initialize(
                   std::move(funcImports_), std::move(sharedStubsCodeBlock_),
                   std::move(sharedStubsLinkData_), std::move(tier1Code),
                   std::move(tier1LinkData))) {
    return nullptr;
  }

  // Copy in a couple of offsets.
  code->setDebugStubOffset(debugStubCodeOffset_);
  code->setRequestTierUpStubOffset(requestTierUpStubCodeOffset_);
  code->setUpdateCallRefMetricsStubOffset(updateCallRefMetricsStubCodeOffset_);

  // All the components are finished, so create the complete Module and start
  // tier-2 compilation if requested.

  MutableModule module = js_new<Module>(*moduleMeta, *code);
  if (!module) {
    return nullptr;
  }

  // If we can serialize (not asm.js), are not planning on serializing already
  // and are testing serialization, then do a roundtrip through serialization
  // to test it out.
  if (!isAsmJS() && compileArgs_->features.testSerialization &&
      module->canSerialize()) {
    MOZ_RELEASE_ASSERT(mode() == CompileMode::Once &&
                       tier() == Tier::Serialized);

    Bytes serializedBytes;
    if (!module->serialize(&serializedBytes)) {
      return nullptr;
    }

    MutableModule deserializedModule =
        Module::deserialize(serializedBytes.begin(), serializedBytes.length());
    if (!deserializedModule) {
      return nullptr;
    }
    module = deserializedModule;

    // Perform storeOptimizedEncoding here instead of below so we don't have to
    // re-serialize the module.
    if (maybeCompleteTier2Listener && module->canSerialize()) {
      maybeCompleteTier2Listener->storeOptimizedEncoding(
          serializedBytes.begin(), serializedBytes.length());
      maybeCompleteTier2Listener = nullptr;
    }
  }

  if (compileState_ == CompileState::EagerTier1) {
    module->startTier2(bytecode, maybeCompleteTier2Listener);
  } else if (tier() == Tier::Serialized && maybeCompleteTier2Listener &&
             module->canSerialize()) {
    Bytes bytes;
    if (module->serialize(&bytes)) {
      maybeCompleteTier2Listener->storeOptimizedEncoding(bytes.begin(),
                                                         bytes.length());
    }
  }

#ifdef JS_JITSPEW
  JS_LOG(wasmPerf, mozilla::LogLevel::Info,
         "CM=..%06lx  MG::finishModule      (%s, complete tier)",
         (unsigned long)(uintptr_t(codeMeta_) & 0xFFFFFFL),
         tier() == Tier::Baseline ? "BL" : "OPT");
#endif

  return module;
}

// Complete all tier-2 construction.  This merely augments the existing Code
// and does not require moduleMeta_.
bool ModuleGenerator::finishTier2(const Module& module) {
  MOZ_ASSERT(!compilingTier1());
  MOZ_ASSERT(compileState_ == CompileState::EagerTier2);
  MOZ_ASSERT(tier() == Tier::Optimized);
  MOZ_ASSERT(!compilerEnv_->debugEnabled());

  if (cancelled_ && *cancelled_) {
    return false;
  }

  UniqueLinkData tier2LinkData;
  UniqueCodeBlock tier2Code = finishTier(&tier2LinkData);
  if (!tier2Code) {
    return false;
  }

  if (MOZ_UNLIKELY(JitOptions.wasmDelayTier2)) {
    // Introduce an artificial delay when testing wasmDelayTier2, since we
    // want to exercise both tier1 and tier2 code in this case.
    ThisThread::SleepMilliseconds(500);
  }

  return module.finishTier2(std::move(tier2Code), std::move(tier2LinkData));
}

bool ModuleGenerator::finishPartialTier2() {
  MOZ_ASSERT(!compilingTier1());
  MOZ_ASSERT(compileState_ == CompileState::LazyTier2);
  MOZ_ASSERT(tier() == Tier::Optimized);
  MOZ_ASSERT(!compilerEnv_->debugEnabled());

  if (cancelled_ && *cancelled_) {
    return false;
  }

  UniqueLinkData tier2LinkData;
  UniqueCodeBlock tier2Code = finishTier(&tier2LinkData);
  if (!tier2Code) {
    return false;
  }

  return partialTieringCode_->finishTier2(std::move(tier2Code),
                                          std::move(tier2LinkData));
}

void ModuleGenerator::warnf(const char* msg, ...) {
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

  (void)warnings_->append(std::move(str));
}

size_t CompiledCode::sizeOfExcludingThis(
    mozilla::MallocSizeOf mallocSizeOf) const {
  size_t trapSitesSize = 0;
  for (const TrapSiteVector& vec : trapSites) {
    trapSitesSize += vec.sizeOfExcludingThis(mallocSizeOf);
  }

  return funcs.sizeOfExcludingThis(mallocSizeOf) +
         bytes.sizeOfExcludingThis(mallocSizeOf) +
         codeRanges.sizeOfExcludingThis(mallocSizeOf) +
         callSites.sizeOfExcludingThis(mallocSizeOf) +
         callSiteTargets.sizeOfExcludingThis(mallocSizeOf) + trapSitesSize +
         symbolicAccesses.sizeOfExcludingThis(mallocSizeOf) +
         tryNotes.sizeOfExcludingThis(mallocSizeOf) +
         codeRangeUnwindInfos.sizeOfExcludingThis(mallocSizeOf) +
         callRefMetricsPatches.sizeOfExcludingThis(mallocSizeOf) +
         codeLabels.sizeOfExcludingThis(mallocSizeOf);
}

size_t CompileTask::sizeOfExcludingThis(
    mozilla::MallocSizeOf mallocSizeOf) const {
  return lifo.sizeOfExcludingThis(mallocSizeOf) +
         inputs.sizeOfExcludingThis(mallocSizeOf) +
         output.sizeOfExcludingThis(mallocSizeOf);
}
