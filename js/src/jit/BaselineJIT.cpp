/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/BaselineJIT.h"

#include "mozilla/BinarySearch.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/MemoryReporting.h"

#include "gc/FreeOp.h"
#include "jit/BaselineCompiler.h"
#include "jit/BaselineIC.h"
#include "jit/CompileInfo.h"
#include "jit/IonControlFlow.h"
#include "jit/JitCommon.h"
#include "jit/JitSpewer.h"
#include "util/StructuredSpewer.h"
#include "vm/Debugger.h"
#include "vm/Interpreter.h"
#include "vm/TraceLogging.h"
#include "wasm/WasmInstance.h"

#include "gc/PrivateIterators-inl.h"
#include "jit/JitFrames-inl.h"
#include "jit/MacroAssembler-inl.h"
#include "vm/BytecodeUtil-inl.h"
#include "vm/GeckoProfiler-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/JSScript-inl.h"
#include "vm/Stack-inl.h"

using mozilla::BinarySearchIf;
using mozilla::DebugOnly;

using namespace js;
using namespace js::jit;

/* static */ PCMappingSlotInfo::SlotLocation PCMappingSlotInfo::ToSlotLocation(
    const StackValue* stackVal) {
  if (stackVal->kind() == StackValue::Register) {
    if (stackVal->reg() == R0) {
      return SlotInR0;
    }
    MOZ_ASSERT(stackVal->reg() == R1);
    return SlotInR1;
  }
  MOZ_ASSERT(stackVal->kind() != StackValue::Stack);
  return SlotIgnore;
}

void ICStubSpace::freeAllAfterMinorGC(Zone* zone) {
  if (zone->isAtomsZone()) {
    MOZ_ASSERT(allocator_.isEmpty());
  } else {
    zone->runtimeFromMainThread()->gc.freeAllLifoBlocksAfterMinorGC(
        &allocator_);
  }
}

static bool CheckFrame(InterpreterFrame* fp) {
  if (fp->isDebuggerEvalFrame()) {
    // Debugger eval-in-frame. These are likely short-running scripts so
    // don't bother compiling them for now.
    JitSpew(JitSpew_BaselineAbort, "debugger frame");
    return false;
  }

  if (fp->isFunctionFrame() && fp->numActualArgs() > BASELINE_MAX_ARGS_LENGTH) {
    // Fall back to the interpreter to avoid running out of stack space.
    JitSpew(JitSpew_BaselineAbort, "Too many arguments (%u)",
            fp->numActualArgs());
    return false;
  }

  return true;
}

static JitExecStatus EnterBaseline(JSContext* cx, EnterJitData& data) {
  MOZ_ASSERT(data.osrFrame);

  // Check for potential stack overflow before OSR-ing.
  uint8_t spDummy;
  uint32_t extra =
      BaselineFrame::Size() + (data.osrNumStackValues * sizeof(Value));
  uint8_t* checkSp = (&spDummy) - extra;
  if (!CheckRecursionLimitWithStackPointer(cx, checkSp)) {
    return JitExec_Aborted;
  }

#ifdef DEBUG
  // Assert we don't GC before entering JIT code. A GC could discard JIT code
  // or move the function stored in the CalleeToken (it won't be traced at
  // this point). We use Maybe<> here so we can call reset() to call the
  // AutoAssertNoGC destructor before we enter JIT code.
  mozilla::Maybe<JS::AutoAssertNoGC> nogc;
  nogc.emplace(cx);
#endif

  MOZ_ASSERT(jit::IsBaselineEnabled(cx));
  MOZ_ASSERT(CheckFrame(data.osrFrame));

  EnterJitCode enter = cx->runtime()->jitRuntime()->enterJit();

  // Caller must construct |this| before invoking the function.
  MOZ_ASSERT_IF(data.constructing,
                data.maxArgv[0].isObject() ||
                    data.maxArgv[0].isMagic(JS_UNINITIALIZED_LEXICAL));

  data.result.setInt32(data.numActualArgs);
  {
    AssertRealmUnchanged aru(cx);
    ActivationEntryMonitor entryMonitor(cx, data.calleeToken);
    JitActivation activation(cx);

    data.osrFrame->setRunningInJit();

#ifdef DEBUG
    nogc.reset();
#endif
    // Single transition point from Interpreter to Baseline.
    CALL_GENERATED_CODE(enter, data.jitcode, data.maxArgc, data.maxArgv,
                        data.osrFrame, data.calleeToken, data.envChain.get(),
                        data.osrNumStackValues, data.result.address());

    data.osrFrame->clearRunningInJit();
  }

  MOZ_ASSERT(!cx->hasIonReturnOverride());

  // Jit callers wrap primitive constructor return, except for derived
  // class constructors, which are forced to do it themselves.
  if (!data.result.isMagic() && data.constructing &&
      data.result.isPrimitive()) {
    MOZ_ASSERT(data.maxArgv[0].isObject());
    data.result = data.maxArgv[0];
  }

  // Release temporary buffer used for OSR into Ion.
  cx->freeOsrTempData();

  MOZ_ASSERT_IF(data.result.isMagic(), data.result.isMagic(JS_ION_ERROR));
  return data.result.isMagic() ? JitExec_Error : JitExec_Ok;
}

JitExecStatus jit::EnterBaselineAtBranch(JSContext* cx, InterpreterFrame* fp,
                                         jsbytecode* pc) {
  MOZ_ASSERT(JSOp(*pc) == JSOP_LOOPENTRY);

  BaselineScript* baseline = fp->script()->baselineScript();

  EnterJitData data(cx);
  PCMappingSlotInfo slotInfo;
  data.jitcode = baseline->nativeCodeForPC(fp->script(), pc, &slotInfo);
  MOZ_ASSERT(slotInfo.isStackSynced());

  // Skip debug breakpoint/trap handler, the interpreter already handled it
  // for the current op.
  if (fp->isDebuggee()) {
    MOZ_RELEASE_ASSERT(baseline->hasDebugInstrumentation());
    data.jitcode += MacroAssembler::ToggledCallSize(data.jitcode);
  }

  // Note: keep this in sync with SetEnterJitData.

  data.osrFrame = fp;
  data.osrNumStackValues =
      fp->script()->nfixed() + cx->interpreterRegs().stackDepth();

  RootedValue newTarget(cx);

  if (fp->isFunctionFrame()) {
    data.constructing = fp->isConstructing();
    data.numActualArgs = fp->numActualArgs();
    data.maxArgc = Max(fp->numActualArgs(), fp->numFormalArgs()) +
                   1;               // +1 = include |this|
    data.maxArgv = fp->argv() - 1;  // -1 = include |this|
    data.envChain = nullptr;
    data.calleeToken = CalleeToToken(&fp->callee(), data.constructing);
  } else {
    data.constructing = false;
    data.numActualArgs = 0;
    data.maxArgc = 0;
    data.maxArgv = nullptr;
    data.envChain = fp->environmentChain();

    data.calleeToken = CalleeToToken(fp->script());

    if (fp->isEvalFrame()) {
      newTarget = fp->newTarget();
      data.maxArgc = 1;
      data.maxArgv = newTarget.address();
    }
  }

  TraceLoggerThread* logger = TraceLoggerForCurrentThread(cx);
  TraceLogStopEvent(logger, TraceLogger_Interpreter);
  TraceLogStartEvent(logger, TraceLogger_Baseline);

  JitExecStatus status = EnterBaseline(cx, data);
  if (status != JitExec_Ok) {
    return status;
  }

  fp->setReturnValue(data.result);
  return JitExec_Ok;
}

MethodStatus jit::BaselineCompile(JSContext* cx, JSScript* script,
                                  bool forceDebugInstrumentation) {
  cx->check(script);
  MOZ_ASSERT(!script->hasBaselineScript());
  MOZ_ASSERT(script->canBaselineCompile());
  MOZ_ASSERT(IsBaselineEnabled(cx));
  AutoGeckoProfilerEntry pseudoFrame(cx, "Baseline script compilation");

  script->ensureNonLazyCanonicalFunction();

  TempAllocator temp(&cx->tempLifoAlloc());
  JitContext jctx(cx, nullptr);

  BaselineCompiler compiler(cx, temp, script);
  if (!compiler.init()) {
    ReportOutOfMemory(cx);
    return Method_Error;
  }

  if (forceDebugInstrumentation) {
    compiler.setCompileDebugInstrumentation();
  }

  MethodStatus status = compiler.compile();

  MOZ_ASSERT_IF(status == Method_Compiled, script->hasBaselineScript());
  MOZ_ASSERT_IF(status != Method_Compiled, !script->hasBaselineScript());

  if (status == Method_CantCompile) {
    script->setBaselineScript(cx->runtime(), BASELINE_DISABLED_SCRIPT);
  }

  return status;
}

static MethodStatus CanEnterBaselineJIT(JSContext* cx, HandleScript script,
                                        InterpreterFrame* osrFrame) {
  MOZ_ASSERT(jit::IsBaselineEnabled(cx));

  // Skip if the script has been disabled.
  if (!script->canBaselineCompile()) {
    return Method_Skipped;
  }

  if (script->length() > BaselineMaxScriptLength) {
    return Method_CantCompile;
  }

  if (script->nslots() > BaselineMaxScriptSlots) {
    return Method_CantCompile;
  }

  if (script->hasBaselineScript()) {
    return Method_Compiled;
  }

  // Check this before calling ensureJitRealmExists, so we're less
  // likely to report OOM in JSRuntime::createJitRuntime.
  if (!CanLikelyAllocateMoreExecutableMemory()) {
    return Method_Skipped;
  }

  if (!cx->realm()->ensureJitRealmExists(cx)) {
    return Method_Error;
  }

  // Check script warm-up counter.
  if (script->incWarmUpCounter() <= JitOptions.baselineWarmUpThreshold) {
    return Method_Skipped;
  }

  // Frames can be marked as debuggee frames independently of its underlying
  // script being a debuggee script, e.g., when performing
  // Debugger.Frame.prototype.eval.
  return BaselineCompile(cx, script, osrFrame && osrFrame->isDebuggee());
}

MethodStatus jit::CanEnterBaselineAtBranch(JSContext* cx,
                                           InterpreterFrame* fp) {
  if (!CheckFrame(fp)) {
    return Method_CantCompile;
  }

  // This check is needed in the following corner case. Consider a function h,
  //
  //   function h(x) {
  //      h(false);
  //      if (!x)
  //        return;
  //      for (var i = 0; i < N; i++)
  //         /* do stuff */
  //   }
  //
  // Suppose h is not yet compiled in baseline and is executing in the
  // interpreter. Let this interpreter frame be f_older. The debugger marks
  // f_older as isDebuggee. At the point of the recursive call h(false), h is
  // compiled in baseline without debug instrumentation, pushing a baseline
  // frame f_newer. The debugger never flags f_newer as isDebuggee, and never
  // recompiles h. When the recursive call returns and execution proceeds to
  // the loop, the interpreter attempts to OSR into baseline. Since h is
  // already compiled in baseline, execution jumps directly into baseline
  // code. This is incorrect as h's baseline script does not have debug
  // instrumentation.
  if (fp->isDebuggee() &&
      !Debugger::ensureExecutionObservabilityOfOsrFrame(cx, fp)) {
    return Method_Error;
  }

  RootedScript script(cx, fp->script());
  return CanEnterBaselineJIT(cx, script, fp);
}

MethodStatus jit::CanEnterBaselineMethod(JSContext* cx, RunState& state) {
  if (state.isInvoke()) {
    InvokeState& invoke = *state.asInvoke();
    if (invoke.args().length() > BASELINE_MAX_ARGS_LENGTH) {
      JitSpew(JitSpew_BaselineAbort, "Too many arguments (%u)",
              invoke.args().length());
      return Method_CantCompile;
    }
  } else {
    if (state.asExecute()->isDebuggerEval()) {
      JitSpew(JitSpew_BaselineAbort, "debugger frame");
      return Method_CantCompile;
    }
  }

  RootedScript script(cx, state.script());
  return CanEnterBaselineJIT(cx, script, /* osrFrame = */ nullptr);
};

BaselineScript* BaselineScript::New(
    JSScript* jsscript, uint32_t bailoutPrologueOffset,
    uint32_t debugOsrPrologueOffset, uint32_t debugOsrEpilogueOffset,
    uint32_t profilerEnterToggleOffset, uint32_t profilerExitToggleOffset,
    size_t retAddrEntries, size_t pcMappingIndexEntries, size_t pcMappingSize,
    size_t bytecodeTypeMapEntries, size_t resumeEntries,
    size_t traceLoggerToggleOffsetEntries) {
  static const unsigned DataAlignment = sizeof(uintptr_t);

  size_t retAddrEntriesSize = retAddrEntries * sizeof(RetAddrEntry);
  size_t pcMappingIndexEntriesSize =
      pcMappingIndexEntries * sizeof(PCMappingIndexEntry);
  size_t bytecodeTypeMapSize = bytecodeTypeMapEntries * sizeof(uint32_t);
  size_t resumeEntriesSize = resumeEntries * sizeof(uintptr_t);
  size_t tlEntriesSize = traceLoggerToggleOffsetEntries * sizeof(uint32_t);

  size_t paddedRetAddrEntriesSize =
      AlignBytes(retAddrEntriesSize, DataAlignment);
  size_t paddedPCMappingIndexEntriesSize =
      AlignBytes(pcMappingIndexEntriesSize, DataAlignment);
  size_t paddedPCMappingSize = AlignBytes(pcMappingSize, DataAlignment);
  size_t paddedBytecodeTypesMapSize =
      AlignBytes(bytecodeTypeMapSize, DataAlignment);
  size_t paddedResumeEntriesSize = AlignBytes(resumeEntriesSize, DataAlignment);
  size_t paddedTLEntriesSize = AlignBytes(tlEntriesSize, DataAlignment);

  size_t allocBytes = paddedRetAddrEntriesSize +
                      paddedPCMappingIndexEntriesSize + paddedPCMappingSize +
                      paddedBytecodeTypesMapSize + paddedResumeEntriesSize +
                      paddedTLEntriesSize;

  BaselineScript* script =
      jsscript->zone()->pod_malloc_with_extra<BaselineScript, uint8_t>(
          allocBytes);
  if (!script) {
    return nullptr;
  }
  new (script) BaselineScript(bailoutPrologueOffset, debugOsrPrologueOffset,
                              debugOsrEpilogueOffset, profilerEnterToggleOffset,
                              profilerExitToggleOffset);

  size_t offsetCursor = sizeof(BaselineScript);
  MOZ_ASSERT(offsetCursor == AlignBytes(sizeof(BaselineScript), DataAlignment));

  script->retAddrEntriesOffset_ = offsetCursor;
  script->retAddrEntries_ = retAddrEntries;
  offsetCursor += paddedRetAddrEntriesSize;

  script->pcMappingIndexOffset_ = offsetCursor;
  script->pcMappingIndexEntries_ = pcMappingIndexEntries;
  offsetCursor += paddedPCMappingIndexEntriesSize;

  script->pcMappingOffset_ = offsetCursor;
  script->pcMappingSize_ = pcMappingSize;
  offsetCursor += paddedPCMappingSize;

  script->bytecodeTypeMapOffset_ = bytecodeTypeMapEntries ? offsetCursor : 0;
  offsetCursor += paddedBytecodeTypesMapSize;

  script->resumeEntriesOffset_ = resumeEntries ? offsetCursor : 0;
  offsetCursor += paddedResumeEntriesSize;

  script->traceLoggerToggleOffsetsOffset_ = tlEntriesSize ? offsetCursor : 0;
  script->numTraceLoggerToggleOffsets_ = traceLoggerToggleOffsetEntries;
  offsetCursor += paddedTLEntriesSize;

  MOZ_ASSERT(offsetCursor == sizeof(BaselineScript) + allocBytes);
  return script;
}

void BaselineScript::trace(JSTracer* trc) {
  TraceEdge(trc, &method_, "baseline-method");
  TraceNullableEdge(trc, &templateEnv_, "baseline-template-environment");
}

void ICScript::trace(JSTracer* trc) {
  // Mark all IC stub codes hanging off the IC stub entries.
  for (size_t i = 0; i < numICEntries(); i++) {
    ICEntry& ent = icEntry(i);
    ent.trace(trc);
  }
}

/* static */
void BaselineScript::writeBarrierPre(Zone* zone, BaselineScript* script) {
  if (zone->needsIncrementalBarrier()) {
    script->trace(zone->barrierTracer());
  }
}

void BaselineScript::Trace(JSTracer* trc, BaselineScript* script) {
  script->trace(trc);
}

void BaselineScript::Destroy(FreeOp* fop, BaselineScript* script) {
  MOZ_ASSERT(!script->hasPendingIonBuilder());

  script->unlinkDependentWasmImports(fop);

  fop->delete_(script);
}

void JS::DeletePolicy<js::jit::BaselineScript>::operator()(
    const js::jit::BaselineScript* script) {
  BaselineScript::Destroy(rt_->defaultFreeOp(),
                          const_cast<BaselineScript*>(script));
}

void BaselineScript::clearDependentWasmImports() {
  // Remove any links from wasm::Instances that contain optimized import calls
  // into this BaselineScript.
  if (dependentWasmImports_) {
    for (DependentWasmImport& dep : *dependentWasmImports_) {
      dep.instance->deoptimizeImportExit(dep.importIndex);
    }
    dependentWasmImports_->clear();
  }
}

void BaselineScript::unlinkDependentWasmImports(FreeOp* fop) {
  // Remove any links from wasm::Instances that contain optimized FFI calls into
  // this BaselineScript.
  clearDependentWasmImports();
  if (dependentWasmImports_) {
    fop->delete_(dependentWasmImports_);
    dependentWasmImports_ = nullptr;
  }
}

bool BaselineScript::addDependentWasmImport(JSContext* cx,
                                            wasm::Instance& instance,
                                            uint32_t idx) {
  if (!dependentWasmImports_) {
    dependentWasmImports_ = cx->new_<Vector<DependentWasmImport>>(cx);
    if (!dependentWasmImports_) {
      return false;
    }
  }
  return dependentWasmImports_->emplaceBack(instance, idx);
}

void BaselineScript::removeDependentWasmImport(wasm::Instance& instance,
                                               uint32_t idx) {
  if (!dependentWasmImports_) {
    return;
  }

  for (DependentWasmImport& dep : *dependentWasmImports_) {
    if (dep.instance == &instance && dep.importIndex == idx) {
      dependentWasmImports_->erase(&dep);
      break;
    }
  }
}

RetAddrEntry& BaselineScript::retAddrEntry(size_t index) {
  MOZ_ASSERT(index < numRetAddrEntries());
  return retAddrEntryList()[index];
}

PCMappingIndexEntry& BaselineScript::pcMappingIndexEntry(size_t index) {
  MOZ_ASSERT(index < numPCMappingIndexEntries());
  return pcMappingIndexEntryList()[index];
}

CompactBufferReader BaselineScript::pcMappingReader(size_t indexEntry) {
  PCMappingIndexEntry& entry = pcMappingIndexEntry(indexEntry);

  uint8_t* dataStart = pcMappingData() + entry.bufferOffset;
  uint8_t* dataEnd =
      (indexEntry == numPCMappingIndexEntries() - 1)
          ? pcMappingData() + pcMappingSize_
          : pcMappingData() + pcMappingIndexEntry(indexEntry + 1).bufferOffset;

  return CompactBufferReader(dataStart, dataEnd);
}

struct ICEntries {
  using EntryT = ICEntry;

  ICScript* const icScript_;

  explicit ICEntries(ICScript* icScript) : icScript_(icScript) {}

  size_t numEntries() const { return icScript_->numICEntries(); }
  ICEntry& operator[](size_t index) const { return icScript_->icEntry(index); }
};

struct RetAddrEntries {
  using EntryT = RetAddrEntry;

  BaselineScript* const baseline_;

  explicit RetAddrEntries(BaselineScript* baseline) : baseline_(baseline) {}

  size_t numEntries() const { return baseline_->numRetAddrEntries(); }
  RetAddrEntry& operator[](size_t index) const {
    return baseline_->retAddrEntry(index);
  }
};

RetAddrEntry& BaselineScript::retAddrEntryFromReturnOffset(
    CodeOffset returnOffset) {
  size_t loc;
#ifdef DEBUG
  bool found =
#endif
      BinarySearchIf(RetAddrEntries(this), 0, numRetAddrEntries(),
                     [&returnOffset](const RetAddrEntry& entry) {
                       size_t roffset = returnOffset.offset();
                       size_t entryRoffset = entry.returnOffset().offset();
                       if (roffset < entryRoffset) {
                         return -1;
                       }
                       if (entryRoffset < roffset) {
                         return 1;
                       }
                       return 0;
                     },
                     &loc);

  MOZ_ASSERT(found);
  MOZ_ASSERT(loc < numRetAddrEntries());
  MOZ_ASSERT(retAddrEntry(loc).returnOffset().offset() ==
             returnOffset.offset());
  return retAddrEntry(loc);
}

template <typename Entries, typename ScriptT>
static inline bool ComputeBinarySearchMid(ScriptT* script, uint32_t pcOffset,
                                          size_t* loc) {
  Entries entries(script);
  return BinarySearchIf(entries, 0, entries.numEntries(),
                        [pcOffset](typename Entries::EntryT& entry) {
                          uint32_t entryOffset = entry.pcOffset();
                          if (pcOffset < entryOffset) {
                            return -1;
                          }
                          if (entryOffset < pcOffset) {
                            return 1;
                          }
                          return 0;
                        },
                        loc);
}

uint8_t* BaselineScript::returnAddressForEntry(const RetAddrEntry& ent) {
  return method()->raw() + ent.returnOffset().offset();
}

ICEntry* ICScript::maybeICEntryFromPCOffset(uint32_t pcOffset) {
  // Multiple IC entries can have the same PC offset, but this method only looks
  // for those which have isForOp() set.
  size_t mid;
  if (!ComputeBinarySearchMid<ICEntries>(this, pcOffset, &mid)) {
    return nullptr;
  }

  MOZ_ASSERT(mid < numICEntries());

  // Found an IC entry with a matching PC offset.  Search backward, and then
  // forward from this IC entry, looking for one with the same PC offset which
  // has isForOp() set.
  for (size_t i = mid; icEntry(i).pcOffset() == pcOffset; i--) {
    if (icEntry(i).isForOp()) {
      return &icEntry(i);
    }
    if (i == 0) {
      break;
    }
  }
  for (size_t i = mid + 1; i < numICEntries(); i++) {
    if (icEntry(i).pcOffset() != pcOffset) {
      break;
    }
    if (icEntry(i).isForOp()) {
      return &icEntry(i);
    }
  }
  return nullptr;
}

ICEntry& ICScript::icEntryFromPCOffset(uint32_t pcOffset) {
  ICEntry* entry = maybeICEntryFromPCOffset(pcOffset);
  MOZ_RELEASE_ASSERT(entry);
  return *entry;
}

ICEntry* ICScript::maybeICEntryFromPCOffset(uint32_t pcOffset,
                                            ICEntry* prevLookedUpEntry) {
  // Do a linear forward search from the last queried PC offset, or fallback to
  // a binary search if the last offset is too far away.
  if (prevLookedUpEntry && pcOffset >= prevLookedUpEntry->pcOffset() &&
      (pcOffset - prevLookedUpEntry->pcOffset()) <= 10) {
    ICEntry* firstEntry = &icEntry(0);
    ICEntry* lastEntry = &icEntry(numICEntries() - 1);
    ICEntry* curEntry = prevLookedUpEntry;
    while (curEntry >= firstEntry && curEntry <= lastEntry) {
      if (curEntry->pcOffset() == pcOffset && curEntry->isForOp()) {
        return curEntry;
      }
      curEntry++;
    }
    return nullptr;
  }

  return maybeICEntryFromPCOffset(pcOffset);
}

ICEntry& ICScript::icEntryFromPCOffset(uint32_t pcOffset,
                                       ICEntry* prevLookedUpEntry) {
  ICEntry* entry = maybeICEntryFromPCOffset(pcOffset, prevLookedUpEntry);
  MOZ_RELEASE_ASSERT(entry);
  return *entry;
}

RetAddrEntry& BaselineScript::retAddrEntryFromPCOffset(
    uint32_t pcOffset, RetAddrEntry::Kind kind) {
  size_t mid;
  MOZ_ALWAYS_TRUE(ComputeBinarySearchMid<RetAddrEntries>(this, pcOffset, &mid));
  MOZ_ASSERT(mid < numRetAddrEntries());

  for (size_t i = mid; retAddrEntry(i).pcOffset() == pcOffset; i--) {
    if (retAddrEntry(i).kind() == kind) {
      return retAddrEntry(i);
    }
    if (i == 0) {
      break;
    }
  }
  for (size_t i = mid + 1; i < numRetAddrEntries(); i++) {
    if (retAddrEntry(i).pcOffset() != pcOffset) {
      break;
    }
    if (retAddrEntry(i).kind() == kind) {
      return retAddrEntry(i);
    }
  }
  MOZ_CRASH("Didn't find RetAddrEntry.");
}

RetAddrEntry& BaselineScript::prologueRetAddrEntry(RetAddrEntry::Kind kind) {
  MOZ_ASSERT(kind == RetAddrEntry::Kind::StackCheck ||
             kind == RetAddrEntry::Kind::WarmupCounter);

  // The prologue entries will always be at a very low offset, so just do a
  // linear search from the beginning.
  for (size_t i = 0; i < numRetAddrEntries(); i++) {
    if (retAddrEntry(i).pcOffset() != 0) {
      break;
    }
    if (retAddrEntry(i).kind() == kind) {
      return retAddrEntry(i);
    }
  }
  MOZ_CRASH("Didn't find prologue RetAddrEntry.");
}

RetAddrEntry& BaselineScript::retAddrEntryFromReturnAddress(
    uint8_t* returnAddr) {
  MOZ_ASSERT(returnAddr > method_->raw());
  MOZ_ASSERT(returnAddr < method_->raw() + method_->instructionsSize());
  CodeOffset offset(returnAddr - method_->raw());
  return retAddrEntryFromReturnOffset(offset);
}

void BaselineScript::computeResumeNativeOffsets(JSScript* script) {
  if (!script->hasResumeOffsets()) {
    return;
  }

  // Translate pcOffset to BaselineScript native address. This may return
  // nullptr if compiler decided code was unreachable.
  auto computeNative = [this, script](uint32_t pcOffset) {
    PCMappingSlotInfo slotInfo;
    uint8_t* nativeCode =
        maybeNativeCodeForPC(script, script->offsetToPC(pcOffset), &slotInfo);
    MOZ_ASSERT(slotInfo.isStackSynced());

    return nativeCode;
  };

  mozilla::Span<const uint32_t> pcOffsets = script->resumeOffsets();
  uint8_t** nativeOffsets = resumeEntryList();
  std::transform(pcOffsets.begin(), pcOffsets.end(), nativeOffsets,
                 computeNative);
}

void ICScript::initICEntries(JSScript* script, const ICEntry* entries) {
  // Fix up the return offset in the IC entries and copy them in.
  // Also write out the IC entry ptrs in any fallback stubs that were added.
  for (uint32_t i = 0; i < numICEntries(); i++) {
    ICEntry& realEntry = icEntry(i);
    new (&realEntry) ICEntry(entries[i]);

    // If the attached stub is a fallback stub, then fix it up with
    // a pointer to the (now available) realEntry.
    if (realEntry.firstStub()->isFallback()) {
      realEntry.firstStub()->toFallbackStub()->fixupICEntry(&realEntry);
    }

    if (realEntry.firstStub()->isTypeMonitor_Fallback()) {
      ICTypeMonitor_Fallback* stub =
          realEntry.firstStub()->toTypeMonitor_Fallback();
      stub->fixupICEntry(&realEntry);
    }
  }
}

void BaselineScript::copyRetAddrEntries(JSScript* script,
                                        const RetAddrEntry* entries) {
  for (uint32_t i = 0; i < numRetAddrEntries(); i++) {
    retAddrEntry(i) = entries[i];
  }
}

void BaselineScript::copyPCMappingEntries(const CompactBufferWriter& entries) {
  MOZ_ASSERT(entries.length() > 0);
  MOZ_ASSERT(entries.length() == pcMappingSize_);

  memcpy(pcMappingData(), entries.buffer(), entries.length());
}

void BaselineScript::copyPCMappingIndexEntries(
    const PCMappingIndexEntry* entries) {
  for (uint32_t i = 0; i < numPCMappingIndexEntries(); i++) {
    pcMappingIndexEntry(i) = entries[i];
  }
}

uint8_t* BaselineScript::maybeNativeCodeForPC(JSScript* script, jsbytecode* pc,
                                              PCMappingSlotInfo* slotInfo) {
  MOZ_ASSERT_IF(script->hasBaselineScript(), script->baselineScript() == this);

  uint32_t pcOffset = script->pcToOffset(pc);

  // Find PCMappingIndexEntry containing pc. They are in ascedending order
  // with the start of one entry being the end of the previous entry. Find
  // first entry where pcOffset < endOffset.
  uint32_t i = 0;
  for (; (i + 1) < numPCMappingIndexEntries(); i++) {
    uint32_t endOffset = pcMappingIndexEntry(i + 1).pcOffset;
    if (pcOffset < endOffset) {
      break;
    }
  }

  PCMappingIndexEntry& entry = pcMappingIndexEntry(i);
  MOZ_ASSERT(pcOffset >= entry.pcOffset);

  CompactBufferReader reader(pcMappingReader(i));
  MOZ_ASSERT(reader.more());

  jsbytecode* curPC = script->offsetToPC(entry.pcOffset);
  uint32_t curNativeOffset = entry.nativeOffset;
  MOZ_ASSERT(script->containsPC(curPC));

  while (reader.more()) {
    // If the high bit is set, the native offset relative to the
    // previous pc != 0 and comes next.
    uint8_t b = reader.readByte();
    if (b & 0x80) {
      curNativeOffset += reader.readUnsigned();
    }

    if (curPC == pc) {
      *slotInfo = PCMappingSlotInfo(b & 0x7F);
      return method_->raw() + curNativeOffset;
    }

    curPC += GetBytecodeLength(curPC);
  }

  // Code was not generated for this PC because BaselineCompiler believes it
  // is unreachable.
  return nullptr;
}

jsbytecode* BaselineScript::approximatePcForNativeAddress(
    JSScript* script, uint8_t* nativeAddress) {
  MOZ_ASSERT(script->baselineScript() == this);
  MOZ_ASSERT(containsCodeAddress(nativeAddress));

  uint32_t nativeOffset = nativeAddress - method_->raw();

  // The native code address can occur before the start of ops. Associate
  // those with start of bytecode.
  if (nativeOffset < pcMappingIndexEntry(0).nativeOffset) {
    return script->code();
  }

  // Find corresponding PCMappingIndexEntry for native offset. They are in
  // ascedending order with the start of one entry being the end of the
  // previous entry. Find first entry where nativeOffset < endOffset.
  uint32_t i = 0;
  for (; (i + 1) < numPCMappingIndexEntries(); i++) {
    uint32_t endOffset = pcMappingIndexEntry(i + 1).nativeOffset;
    if (nativeOffset < endOffset) {
      break;
    }
  }

  PCMappingIndexEntry& entry = pcMappingIndexEntry(i);
  MOZ_ASSERT(nativeOffset >= entry.nativeOffset);

  CompactBufferReader reader(pcMappingReader(i));
  MOZ_ASSERT(reader.more());

  jsbytecode* curPC = script->offsetToPC(entry.pcOffset);
  uint32_t curNativeOffset = entry.nativeOffset;
  MOZ_ASSERT(script->containsPC(curPC));

  jsbytecode* lastPC = curPC;
  while (reader.more()) {
    // If the high bit is set, the native offset relative to the
    // previous pc != 0 and comes next.
    uint8_t b = reader.readByte();
    if (b & 0x80) {
      curNativeOffset += reader.readUnsigned();
    }

    // Return the last PC that matched nativeOffset. Some bytecode
    // generate no native code (e.g., constant-pushing bytecode like
    // JSOP_INT8), and so their entries share the same nativeOffset as the
    // next op that does generate code.
    if (curNativeOffset > nativeOffset) {
      return lastPC;
    }

    lastPC = curPC;
    curPC += GetBytecodeLength(curPC);
  }

  // Associate all addresses at end of PCMappingIndexEntry with lastPC.
  return lastPC;
}

void BaselineScript::toggleDebugTraps(JSScript* script, jsbytecode* pc) {
  MOZ_ASSERT(script->baselineScript() == this);

  // Only scripts compiled for debug mode have toggled calls.
  if (!hasDebugInstrumentation()) {
    return;
  }

  AutoWritableJitCode awjc(method());

  for (uint32_t i = 0; i < numPCMappingIndexEntries(); i++) {
    PCMappingIndexEntry& entry = pcMappingIndexEntry(i);

    CompactBufferReader reader(pcMappingReader(i));
    jsbytecode* curPC = script->offsetToPC(entry.pcOffset);
    uint32_t nativeOffset = entry.nativeOffset;

    MOZ_ASSERT(script->containsPC(curPC));

    while (reader.more()) {
      uint8_t b = reader.readByte();
      if (b & 0x80) {
        nativeOffset += reader.readUnsigned();
      }

      if (!pc || pc == curPC) {
        bool enabled =
            script->stepModeEnabled() || script->hasBreakpointsAt(curPC);

        // Patch the trap.
        CodeLocationLabel label(method(), CodeOffset(nativeOffset));
        Assembler::ToggleCall(label, enabled);
      }

      curPC += GetBytecodeLength(curPC);
    }
  }
}

#ifdef JS_TRACE_LOGGING
void BaselineScript::initTraceLogger(JSScript* script,
                                     const Vector<CodeOffset>& offsets) {
#ifdef DEBUG
  traceLoggerScriptsEnabled_ = TraceLogTextIdEnabled(TraceLogger_Scripts);
  traceLoggerEngineEnabled_ = TraceLogTextIdEnabled(TraceLogger_Engine);
#endif

  MOZ_ASSERT(offsets.length() == numTraceLoggerToggleOffsets_);
  for (size_t i = 0; i < offsets.length(); i++) {
    traceLoggerToggleOffsets()[i] = offsets[i].offset();
  }

  if (TraceLogTextIdEnabled(TraceLogger_Engine) ||
      TraceLogTextIdEnabled(TraceLogger_Scripts)) {
    traceLoggerScriptEvent_ = TraceLoggerEvent(TraceLogger_Scripts, script);
    for (size_t i = 0; i < numTraceLoggerToggleOffsets_; i++) {
      CodeLocationLabel label(method_,
                              CodeOffset(traceLoggerToggleOffsets()[i]));
      Assembler::ToggleToCmp(label);
    }
  }
}

void BaselineScript::toggleTraceLoggerScripts(JSScript* script, bool enable) {
  DebugOnly<bool> engineEnabled = TraceLogTextIdEnabled(TraceLogger_Engine);
  MOZ_ASSERT(enable == !traceLoggerScriptsEnabled_);
  MOZ_ASSERT(engineEnabled == traceLoggerEngineEnabled_);

  // Patch the logging script textId to be correct.
  // When logging log the specific textId else the global Scripts textId.
  if (enable && !traceLoggerScriptEvent_.hasTextId()) {
    traceLoggerScriptEvent_ = TraceLoggerEvent(TraceLogger_Scripts, script);
  }

  AutoWritableJitCode awjc(method());

  // Enable/Disable the traceLogger.
  for (size_t i = 0; i < numTraceLoggerToggleOffsets_; i++) {
    CodeLocationLabel label(method_, CodeOffset(traceLoggerToggleOffsets()[i]));
    if (enable) {
      Assembler::ToggleToCmp(label);
    } else {
      Assembler::ToggleToJmp(label);
    }
  }

#if DEBUG
  traceLoggerScriptsEnabled_ = enable;
#endif
}

void BaselineScript::toggleTraceLoggerEngine(bool enable) {
  DebugOnly<bool> scriptsEnabled = TraceLogTextIdEnabled(TraceLogger_Scripts);
  MOZ_ASSERT(enable == !traceLoggerEngineEnabled_);
  MOZ_ASSERT(scriptsEnabled == traceLoggerScriptsEnabled_);

  AutoWritableJitCode awjc(method());

  // Enable/Disable the traceLogger prologue and epilogue.
  for (size_t i = 0; i < numTraceLoggerToggleOffsets_; i++) {
    CodeLocationLabel label(method_, CodeOffset(traceLoggerToggleOffsets()[i]));
    if (enable) {
      Assembler::ToggleToCmp(label);
    } else {
      Assembler::ToggleToJmp(label);
    }
  }

#if DEBUG
  traceLoggerEngineEnabled_ = enable;
#endif
}
#endif

void BaselineScript::toggleProfilerInstrumentation(bool enable) {
  if (enable == isProfilerInstrumentationOn()) {
    return;
  }

  JitSpew(JitSpew_BaselineIC, "  toggling profiling %s for BaselineScript %p",
          enable ? "on" : "off", this);

  // Toggle the jump
  CodeLocationLabel enterToggleLocation(method_,
                                        CodeOffset(profilerEnterToggleOffset_));
  CodeLocationLabel exitToggleLocation(method_,
                                       CodeOffset(profilerExitToggleOffset_));
  if (enable) {
    Assembler::ToggleToCmp(enterToggleLocation);
    Assembler::ToggleToCmp(exitToggleLocation);
    flags_ |= uint32_t(PROFILER_INSTRUMENTATION_ON);
  } else {
    Assembler::ToggleToJmp(enterToggleLocation);
    Assembler::ToggleToJmp(exitToggleLocation);
    flags_ &= ~uint32_t(PROFILER_INSTRUMENTATION_ON);
  }
}

void ICScript::purgeOptimizedStubs(JSScript* script) {
  MOZ_ASSERT(script->icScript() == this);

  Zone* zone = script->zone();
  if (zone->isGCSweeping() && IsAboutToBeFinalizedDuringSweep(*script)) {
    // We're sweeping and the script is dead. Don't purge optimized stubs
    // because (1) accessing CacheIRStubInfo pointers in ICStubs is invalid
    // because we may have swept them already when we started (incremental)
    // sweeping and (2) it's unnecessary because this script will be finalized
    // soon anyway.
    return;
  }

  JitSpew(JitSpew_BaselineIC, "Purging optimized stubs");

  for (size_t i = 0; i < numICEntries(); i++) {
    ICEntry& entry = icEntry(i);
    ICStub* lastStub = entry.firstStub();
    while (lastStub->next()) {
      lastStub = lastStub->next();
    }

    if (lastStub->isFallback()) {
      // Unlink all stubs allocated in the optimized space.
      ICStub* stub = entry.firstStub();
      ICStub* prev = nullptr;

      while (stub->next()) {
        if (!stub->allocatedInFallbackSpace()) {
          lastStub->toFallbackStub()->unlinkStub(zone, prev, stub);
          stub = stub->next();
          continue;
        }

        prev = stub;
        stub = stub->next();
      }

      if (lastStub->isMonitoredFallback()) {
        // Monitor stubs can't make calls, so are always in the
        // optimized stub space.
        ICTypeMonitor_Fallback* lastMonStub =
            lastStub->toMonitoredFallbackStub()->maybeFallbackMonitorStub();
        if (lastMonStub) {
          lastMonStub->resetMonitorStubChain(zone);
        }
      }
    } else if (lastStub->isTypeMonitor_Fallback()) {
      lastStub->toTypeMonitor_Fallback()->resetMonitorStubChain(zone);
    } else {
      MOZ_CRASH("Unknown fallback stub");
    }
  }

#ifdef DEBUG
  // All remaining stubs must be allocated in the fallback space.
  for (size_t i = 0; i < numICEntries(); i++) {
    ICEntry& entry = icEntry(i);
    ICStub* stub = entry.firstStub();
    while (stub->next()) {
      MOZ_ASSERT(stub->allocatedInFallbackSpace());
      stub = stub->next();
    }
  }
#endif
}

#ifdef JS_STRUCTURED_SPEW
static bool GetStubEnteredCount(ICStub* stub, uint32_t* count) {
  switch (stub->kind()) {
    case ICStub::CacheIR_Regular:
      *count = stub->toCacheIR_Regular()->enteredCount();
      return true;
    case ICStub::CacheIR_Updated:
      *count = stub->toCacheIR_Updated()->enteredCount();
      return true;
    case ICStub::CacheIR_Monitored:
      *count = stub->toCacheIR_Monitored()->enteredCount();
      return true;
    default:
      return false;
  }
}

bool HasEnteredCounters(ICEntry& entry) {
  ICStub* stub = entry.firstStub();
  while (stub && !stub->isFallback()) {
    uint32_t count;
    if (GetStubEnteredCount(stub, &count)) {
      return true;
    }
    stub = stub->next();
  }
  return false;
}

void jit::JitSpewBaselineICStats(JSScript* script, const char* dumpReason) {
  MOZ_ASSERT(script->hasICScript());
  JSContext* cx = TlsContext.get();
  AutoStructuredSpewer spew(cx, SpewChannel::BaselineICStats, script);
  if (!spew) {
    return;
  }

  ICScript* icScript = script->icScript();
  spew->property("reason", dumpReason);
  spew->beginListProperty("entries");
  for (size_t i = 0; i < icScript->numICEntries(); i++) {
    ICEntry& entry = icScript->icEntry(i);
    if (!HasEnteredCounters(entry)) {
      continue;
    }

    uint32_t pcOffset = entry.pcOffset();
    jsbytecode* pc = entry.pc(script);

    unsigned column;
    unsigned int line = PCToLineNumber(script, pc, &column);

    spew->beginObject();
    spew->property("op", CodeName[*pc]);
    spew->property("pc", pcOffset);
    spew->property("line", line);
    spew->property("column", column);

    spew->beginListProperty("counts");
    ICStub* stub = entry.firstStub();
    while (stub && !stub->isFallback()) {
      uint32_t count;
      if (GetStubEnteredCount(stub, &count)) {
        spew->value(count);
      } else {
        spew->value("?");
      }
      stub = stub->next();
    }
    spew->endList();
    spew->property("fallback_count", entry.fallbackStub()->enteredCount());
    spew->endObject();
  }
  spew->endList();
}
#endif

void jit::FinishDiscardBaselineScript(FreeOp* fop, JSScript* script) {
  if (!script->hasBaselineScript()) {
    return;
  }

  if (script->baselineScript()->active()) {
    // Reset |active| flag so that we don't need a separate script
    // iteration to unmark them.
    script->baselineScript()->resetActive();

    // The baseline caches have been wiped out, so the script will need to
    // warm back up before it can be inlined during Ion compilation.
    script->baselineScript()->clearIonCompiledOrInlined();
    return;
  }

  BaselineScript* baseline = script->baselineScript();
  script->setBaselineScript(fop->runtime(), nullptr);
  BaselineScript::Destroy(fop, baseline);
}

void jit::AddSizeOfBaselineData(JSScript* script,
                                mozilla::MallocSizeOf mallocSizeOf,
                                size_t* data, size_t* fallbackStubs) {
  if (script->hasICScript()) {
    // ICScript is stored in TypeScript but we report its size here and not
    // in TypeScript::sizeOfIncludingThis.
    script->icScript()->addSizeOfIncludingThis(mallocSizeOf, data,
                                               fallbackStubs);
  }
  if (script->hasBaselineScript()) {
    script->baselineScript()->addSizeOfIncludingThis(mallocSizeOf, data);
  }
}

void jit::ToggleBaselineProfiling(JSRuntime* runtime, bool enable) {
  JitRuntime* jrt = runtime->jitRuntime();
  if (!jrt) {
    return;
  }

  for (ZonesIter zone(runtime, SkipAtoms); !zone.done(); zone.next()) {
    for (auto script = zone->cellIter<JSScript>(); !script.done();
         script.next()) {
      if (!script->hasBaselineScript()) {
        continue;
      }
      AutoWritableJitCode awjc(script->baselineScript()->method());
      script->baselineScript()->toggleProfilerInstrumentation(enable);
    }
  }
}

#ifdef JS_TRACE_LOGGING
void jit::ToggleBaselineTraceLoggerScripts(JSRuntime* runtime, bool enable) {
  for (ZonesIter zone(runtime, SkipAtoms); !zone.done(); zone.next()) {
    for (auto script = zone->cellIter<JSScript>(); !script.done();
         script.next()) {
      if (!script->hasBaselineScript()) {
        continue;
      }
      script->baselineScript()->toggleTraceLoggerScripts(script, enable);
    }
  }
}

void jit::ToggleBaselineTraceLoggerEngine(JSRuntime* runtime, bool enable) {
  for (ZonesIter zone(runtime, SkipAtoms); !zone.done(); zone.next()) {
    for (auto script = zone->cellIter<JSScript>(); !script.done();
         script.next()) {
      if (!script->hasBaselineScript()) {
        continue;
      }
      script->baselineScript()->toggleTraceLoggerEngine(enable);
    }
  }
}
#endif

static void MarkActiveBaselineScripts(JSContext* cx,
                                      const JitActivationIterator& activation) {
  for (OnlyJSJitFrameIter iter(activation); !iter.done(); ++iter) {
    const JSJitFrameIter& frame = iter.frame();
    switch (frame.type()) {
      case FrameType::BaselineJS:
        frame.script()->baselineScript()->setActive();
        break;
      case FrameType::Exit:
        if (frame.exitFrame()->is<LazyLinkExitFrameLayout>()) {
          LazyLinkExitFrameLayout* ll =
              frame.exitFrame()->as<LazyLinkExitFrameLayout>();
          ScriptFromCalleeToken(ll->jsFrame()->calleeToken())
              ->baselineScript()
              ->setActive();
        }
        break;
      case FrameType::Bailout:
      case FrameType::IonJS: {
        // Keep the baseline script around, since bailouts from the ion
        // jitcode might need to re-enter into the baseline jitcode.
        frame.script()->baselineScript()->setActive();
        for (InlineFrameIterator inlineIter(cx, &frame); inlineIter.more();
             ++inlineIter) {
          inlineIter.script()->baselineScript()->setActive();
        }
        break;
      }
      default:;
    }
  }
}

void jit::MarkActiveBaselineScripts(Zone* zone) {
  if (zone->isAtomsZone()) {
    return;
  }
  JSContext* cx = TlsContext.get();
  for (JitActivationIterator iter(cx); !iter.done(); ++iter) {
    if (iter->compartment()->zone() == zone) {
      MarkActiveBaselineScripts(cx, iter);
    }
  }
}
