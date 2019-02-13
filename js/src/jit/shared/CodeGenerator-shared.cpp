/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/shared/CodeGenerator-shared-inl.h"

#include "mozilla/DebugOnly.h"

#include "jit/CompactBuffer.h"
#include "jit/IonCaches.h"
#include "jit/JitcodeMap.h"
#include "jit/JitSpewer.h"
#include "jit/MacroAssembler.h"
#include "jit/MIR.h"
#include "jit/MIRGenerator.h"
#include "jit/OptimizationTracking.h"
#include "js/Conversions.h"
#include "vm/TraceLogging.h"

#include "jit/JitFrames-inl.h"
#include "jit/MacroAssembler-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::BitwiseCast;
using mozilla::DebugOnly;

namespace js {
namespace jit {

MacroAssembler&
CodeGeneratorShared::ensureMasm(MacroAssembler* masmArg)
{
    if (masmArg)
        return *masmArg;
    maybeMasm_.emplace();
    return *maybeMasm_;
}

CodeGeneratorShared::CodeGeneratorShared(MIRGenerator* gen, LIRGraph* graph, MacroAssembler* masmArg)
  : maybeMasm_(),
    masm(ensureMasm(masmArg)),
    gen(gen),
    graph(*graph),
    current(nullptr),
    snapshots_(),
    recovers_(),
    deoptTable_(nullptr),
#ifdef DEBUG
    pushedArgs_(0),
#endif
    lastOsiPointOffset_(0),
    safepoints_(graph->totalSlotCount(), (gen->info().nargs() + 1) * sizeof(Value)),
    nativeToBytecodeMap_(nullptr),
    nativeToBytecodeMapSize_(0),
    nativeToBytecodeTableOffset_(0),
    nativeToBytecodeNumRegions_(0),
    nativeToBytecodeScriptList_(nullptr),
    nativeToBytecodeScriptListLength_(0),
    trackedOptimizationsMap_(nullptr),
    trackedOptimizationsMapSize_(0),
    trackedOptimizationsRegionTableOffset_(0),
    trackedOptimizationsTypesTableOffset_(0),
    trackedOptimizationsAttemptsTableOffset_(0),
    osrEntryOffset_(0),
    skipArgCheckEntryOffset_(0),
#ifdef CHECK_OSIPOINT_REGISTERS
    checkOsiPointRegisters(js_JitOptions.checkOsiPointRegisters),
#endif
    frameDepth_(graph->paddedLocalSlotsSize() + graph->argumentsSize()),
    frameInitialAdjustment_(0)
{
    if (gen->isProfilerInstrumentationEnabled())
        masm.enableProfilingInstrumentation();

    if (gen->compilingAsmJS()) {
        // Since asm.js uses the system ABI which does not necessarily use a
        // regular array where all slots are sizeof(Value), it maintains the max
        // argument stack depth separately.
        MOZ_ASSERT(graph->argumentSlotCount() == 0);
        frameDepth_ += gen->maxAsmJSStackArgBytes();

        if (gen->usesSimd()) {
            // If the function uses any SIMD then we may need to insert padding
            // so that local slots are aligned for SIMD.
            frameInitialAdjustment_ = ComputeByteAlignment(sizeof(AsmJSFrame),
                                                           AsmJSStackAlignment);
            frameDepth_ += frameInitialAdjustment_;
            // Keep the stack aligned. Some SIMD sequences build values on the
            // stack and need the stack aligned.
            frameDepth_ += ComputeByteAlignment(sizeof(AsmJSFrame) + frameDepth_,
                                                AsmJSStackAlignment);
        } else if (gen->performsCall()) {
            // An MAsmJSCall does not align the stack pointer at calls sites but
            // instead relies on the a priori stack adjustment. This must be the
            // last adjustment of frameDepth_.
            frameDepth_ += ComputeByteAlignment(sizeof(AsmJSFrame) + frameDepth_,
                                                AsmJSStackAlignment);
        }

        // FrameSizeClass is only used for bailing, which cannot happen in
        // asm.js code.
        frameClass_ = FrameSizeClass::None();
    } else {
        frameClass_ = FrameSizeClass::FromDepth(frameDepth_);
    }
}

bool
CodeGeneratorShared::generateOutOfLineCode()
{
    for (size_t i = 0; i < outOfLineCode_.length(); i++) {
        // Add native => bytecode mapping entries for OOL sites.
        // Not enabled on asm.js yet since asm doesn't contain bytecode mappings.
        if (!gen->compilingAsmJS()) {
            if (!addNativeToBytecodeEntry(outOfLineCode_[i]->bytecodeSite()))
                return false;
        }

        if (!gen->alloc().ensureBallast())
            return false;

        JitSpew(JitSpew_Codegen, "# Emitting out of line code");

        masm.setFramePushed(outOfLineCode_[i]->framePushed());
        lastPC_ = outOfLineCode_[i]->pc();
        outOfLineCode_[i]->bind(&masm);

        outOfLineCode_[i]->generate(this);
    }

    return true;
}

void
CodeGeneratorShared::addOutOfLineCode(OutOfLineCode* code, const MInstruction* mir)
{
    MOZ_ASSERT(mir);
    addOutOfLineCode(code, mir->trackedSite());
}

void
CodeGeneratorShared::addOutOfLineCode(OutOfLineCode* code, const BytecodeSite* site)
{
    code->setFramePushed(masm.framePushed());
    code->setBytecodeSite(site);
    MOZ_ASSERT_IF(!gen->compilingAsmJS(), code->script()->containsPC(code->pc()));
    masm.propagateOOM(outOfLineCode_.append(code));
}

bool
CodeGeneratorShared::addNativeToBytecodeEntry(const BytecodeSite* site)
{
    // Skip the table entirely if profiling is not enabled.
    if (!isProfilerInstrumentationEnabled())
        return true;

    MOZ_ASSERT(site);
    MOZ_ASSERT(site->tree());
    MOZ_ASSERT(site->pc());

    InlineScriptTree* tree = site->tree();
    jsbytecode* pc = site->pc();
    uint32_t nativeOffset = masm.currentOffset();

    MOZ_ASSERT_IF(nativeToBytecodeList_.empty(), nativeOffset == 0);

    if (!nativeToBytecodeList_.empty()) {
        size_t lastIdx = nativeToBytecodeList_.length() - 1;
        NativeToBytecode& lastEntry = nativeToBytecodeList_[lastIdx];

        MOZ_ASSERT(nativeOffset >= lastEntry.nativeOffset.offset());

        // If the new entry is for the same inlineScriptTree and same
        // bytecodeOffset, but the nativeOffset has changed, do nothing.
        // The same site just generated some more code.
        if (lastEntry.tree == tree && lastEntry.pc == pc) {
            JitSpew(JitSpew_Profiling, " => In-place update [%u-%u]",
                    lastEntry.nativeOffset.offset(), nativeOffset);
            return true;
        }

        // If the new entry is for the same native offset, then update the
        // previous entry with the new bytecode site, since the previous
        // bytecode site did not generate any native code.
        if (lastEntry.nativeOffset.offset() == nativeOffset) {
            lastEntry.tree = tree;
            lastEntry.pc = pc;
            JitSpew(JitSpew_Profiling, " => Overwriting zero-length native region.");

            // This overwrite might have made the entry merge-able with a
            // previous one.  If so, merge it.
            if (lastIdx > 0) {
                NativeToBytecode& nextToLastEntry = nativeToBytecodeList_[lastIdx - 1];
                if (nextToLastEntry.tree == lastEntry.tree && nextToLastEntry.pc == lastEntry.pc) {
                    JitSpew(JitSpew_Profiling, " => Merging with previous region");
                    nativeToBytecodeList_.erase(&lastEntry);
                }
            }

            dumpNativeToBytecodeEntry(nativeToBytecodeList_.length() - 1);
            return true;
        }
    }

    // Otherwise, some native code was generated for the previous bytecode site.
    // Add a new entry for code that is about to be generated.
    NativeToBytecode entry;
    entry.nativeOffset = CodeOffsetLabel(nativeOffset);
    entry.tree = tree;
    entry.pc = pc;
    if (!nativeToBytecodeList_.append(entry))
        return false;

    JitSpew(JitSpew_Profiling, " => Push new entry.");
    dumpNativeToBytecodeEntry(nativeToBytecodeList_.length() - 1);
    return true;
}

void
CodeGeneratorShared::dumpNativeToBytecodeEntries()
{
#ifdef DEBUG
    InlineScriptTree* topTree = gen->info().inlineScriptTree();
    JitSpewStart(JitSpew_Profiling, "Native To Bytecode Entries for %s:%d\n",
                 topTree->script()->filename(), topTree->script()->lineno());
    for (unsigned i = 0; i < nativeToBytecodeList_.length(); i++)
        dumpNativeToBytecodeEntry(i);
#endif
}

void
CodeGeneratorShared::dumpNativeToBytecodeEntry(uint32_t idx)
{
#ifdef DEBUG
    NativeToBytecode& ref = nativeToBytecodeList_[idx];
    InlineScriptTree* tree = ref.tree;
    JSScript* script = tree->script();
    uint32_t nativeOffset = ref.nativeOffset.offset();
    unsigned nativeDelta = 0;
    unsigned pcDelta = 0;
    if (idx + 1 < nativeToBytecodeList_.length()) {
        NativeToBytecode* nextRef = &ref + 1;
        nativeDelta = nextRef->nativeOffset.offset() - nativeOffset;
        if (nextRef->tree == ref.tree)
            pcDelta = nextRef->pc - ref.pc;
    }
    JitSpewStart(JitSpew_Profiling, "    %08x [+%-6d] => %-6d [%-4d] {%-10s} (%s:%d",
                 ref.nativeOffset.offset(),
                 nativeDelta,
                 ref.pc - script->code(),
                 pcDelta,
                 js_CodeName[JSOp(*ref.pc)],
                 script->filename(), script->lineno());

    for (tree = tree->caller(); tree; tree = tree->caller()) {
        JitSpewCont(JitSpew_Profiling, " <= %s:%d", tree->script()->filename(),
                                                    tree->script()->lineno());
    }
    JitSpewCont(JitSpew_Profiling, ")");
    JitSpewFin(JitSpew_Profiling);
#endif
}

bool
CodeGeneratorShared::addTrackedOptimizationsEntry(const TrackedOptimizations* optimizations)
{
    if (!isOptimizationTrackingEnabled())
        return true;

    MOZ_ASSERT(optimizations);

    uint32_t nativeOffset = masm.currentOffset();

    if (!trackedOptimizations_.empty()) {
        NativeToTrackedOptimizations& lastEntry = trackedOptimizations_.back();
        MOZ_ASSERT(nativeOffset >= lastEntry.endOffset.offset());

        // If we're still generating code for the same set of optimizations,
        // we are done.
        if (lastEntry.optimizations == optimizations)
            return true;
    }

    // If we're generating code for a new set of optimizations, add a new
    // entry.
    NativeToTrackedOptimizations entry;
    entry.startOffset = CodeOffsetLabel(nativeOffset);
    entry.endOffset = CodeOffsetLabel(nativeOffset);
    entry.optimizations = optimizations;
    return trackedOptimizations_.append(entry);
}

void
CodeGeneratorShared::extendTrackedOptimizationsEntry(const TrackedOptimizations* optimizations)
{
    if (!isOptimizationTrackingEnabled())
        return;

    uint32_t nativeOffset = masm.currentOffset();
    NativeToTrackedOptimizations& entry = trackedOptimizations_.back();
    MOZ_ASSERT(entry.optimizations == optimizations);
    MOZ_ASSERT(nativeOffset >= entry.endOffset.offset());

    entry.endOffset = CodeOffsetLabel(nativeOffset);

    // If we generated no code, remove the last entry.
    if (nativeOffset == entry.startOffset.offset())
        trackedOptimizations_.popBack();
}

// see OffsetOfFrameSlot
static inline int32_t
ToStackIndex(LAllocation* a)
{
    if (a->isStackSlot()) {
        MOZ_ASSERT(a->toStackSlot()->slot() >= 1);
        return a->toStackSlot()->slot();
    }
    return -int32_t(sizeof(JitFrameLayout) + a->toArgument()->index());
}

void
CodeGeneratorShared::encodeAllocation(LSnapshot* snapshot, MDefinition* mir,
                                      uint32_t* allocIndex)
{
    if (mir->isBox())
        mir = mir->toBox()->getOperand(0);

    MIRType type =
        mir->isRecoveredOnBailout() ? MIRType_None :
        mir->isUnused() ? MIRType_MagicOptimizedOut :
        mir->type();

    RValueAllocation alloc;

    switch (type) {
      case MIRType_None:
      {
        MOZ_ASSERT(mir->isRecoveredOnBailout());
        uint32_t index = 0;
        LRecoverInfo* recoverInfo = snapshot->recoverInfo();
        MNode** it = recoverInfo->begin();
        MNode** end = recoverInfo->end();
        while (it != end && mir != *it) {
            ++it;
            ++index;
        }

        // This MDefinition is recovered, thus it should be listed in the
        // LRecoverInfo.
        MOZ_ASSERT(it != end && mir == *it);

        // Lambda should have a default value readable for iterating over the
        // inner frames.
        if (mir->isLambda()) {
            MConstant* constant = mir->toLambda()->functionOperand();
            uint32_t cstIndex;
            masm.propagateOOM(graph.addConstantToPool(constant->value(), &cstIndex));
            alloc = RValueAllocation::RecoverInstruction(index, cstIndex);
            break;
        }

        alloc = RValueAllocation::RecoverInstruction(index);
        break;
      }
      case MIRType_Undefined:
        alloc = RValueAllocation::Undefined();
        break;
      case MIRType_Null:
        alloc = RValueAllocation::Null();
        break;
      case MIRType_Int32:
      case MIRType_String:
      case MIRType_Symbol:
      case MIRType_Object:
      case MIRType_ObjectOrNull:
      case MIRType_Boolean:
      case MIRType_Double:
      {
        LAllocation* payload = snapshot->payloadOfSlot(*allocIndex);
        if (payload->isConstant()) {
            MConstant* constant = mir->toConstant();
            uint32_t index;
            masm.propagateOOM(graph.addConstantToPool(constant->value(), &index));
            alloc = RValueAllocation::ConstantPool(index);
            break;
        }

        JSValueType valueType =
            (type == MIRType_ObjectOrNull) ? JSVAL_TYPE_OBJECT : ValueTypeFromMIRType(type);

        MOZ_ASSERT(payload->isMemory() || payload->isRegister());
        if (payload->isMemory())
            alloc = RValueAllocation::Typed(valueType, ToStackIndex(payload));
        else if (payload->isGeneralReg())
            alloc = RValueAllocation::Typed(valueType, ToRegister(payload));
        else if (payload->isFloatReg())
            alloc = RValueAllocation::Double(ToFloatRegister(payload));
        break;
      }
      case MIRType_Float32:
      case MIRType_Int32x4:
      case MIRType_Float32x4:
      {
        LAllocation* payload = snapshot->payloadOfSlot(*allocIndex);
        if (payload->isConstant()) {
            MConstant* constant = mir->toConstant();
            uint32_t index;
            masm.propagateOOM(graph.addConstantToPool(constant->value(), &index));
            alloc = RValueAllocation::ConstantPool(index);
            break;
        }

        MOZ_ASSERT(payload->isMemory() || payload->isFloatReg());
        if (payload->isFloatReg())
            alloc = RValueAllocation::AnyFloat(ToFloatRegister(payload));
        else
            alloc = RValueAllocation::AnyFloat(ToStackIndex(payload));
        break;
      }
      case MIRType_MagicOptimizedArguments:
      case MIRType_MagicOptimizedOut:
      case MIRType_MagicUninitializedLexical:
      {
        uint32_t index;
        Value v = MagicValue(type == MIRType_MagicOptimizedArguments
                             ? JS_OPTIMIZED_ARGUMENTS
                             : (type == MIRType_MagicOptimizedOut
                                ? JS_OPTIMIZED_OUT
                                : JS_UNINITIALIZED_LEXICAL));
        masm.propagateOOM(graph.addConstantToPool(v, &index));
        alloc = RValueAllocation::ConstantPool(index);
        break;
      }
      default:
      {
        MOZ_ASSERT(mir->type() == MIRType_Value);
        LAllocation* payload = snapshot->payloadOfSlot(*allocIndex);
#ifdef JS_NUNBOX32
        LAllocation* type = snapshot->typeOfSlot(*allocIndex);
        if (type->isRegister()) {
            if (payload->isRegister())
                alloc = RValueAllocation::Untyped(ToRegister(type), ToRegister(payload));
            else
                alloc = RValueAllocation::Untyped(ToRegister(type), ToStackIndex(payload));
        } else {
            if (payload->isRegister())
                alloc = RValueAllocation::Untyped(ToStackIndex(type), ToRegister(payload));
            else
                alloc = RValueAllocation::Untyped(ToStackIndex(type), ToStackIndex(payload));
        }
#elif JS_PUNBOX64
        if (payload->isRegister())
            alloc = RValueAllocation::Untyped(ToRegister(payload));
        else
            alloc = RValueAllocation::Untyped(ToStackIndex(payload));
#endif
        break;
      }
    }

    // This set an extra bit as part of the RValueAllocation, such that we know
    // that recover instruction have to be executed without wrapping the
    // instruction in a no-op recover instruction.
    if (mir->isIncompleteObject())
        alloc.setNeedSideEffect();

    snapshots_.add(alloc);
    *allocIndex += mir->isRecoveredOnBailout() ? 0 : 1;
}

void
CodeGeneratorShared::encode(LRecoverInfo* recover)
{
    if (recover->recoverOffset() != INVALID_RECOVER_OFFSET)
        return;

    uint32_t numInstructions = recover->numInstructions();
    JitSpew(JitSpew_IonSnapshots, "Encoding LRecoverInfo %p (frameCount %u, instructions %u)",
            (void*)recover, recover->mir()->frameCount(), numInstructions);

    MResumePoint::Mode mode = recover->mir()->mode();
    MOZ_ASSERT(mode != MResumePoint::Outer);
    bool resumeAfter = (mode == MResumePoint::ResumeAfter);

    RecoverOffset offset = recovers_.startRecover(numInstructions, resumeAfter);

    for (MNode* insn : *recover)
        recovers_.writeInstruction(insn);

    recovers_.endRecover();
    recover->setRecoverOffset(offset);
    masm.propagateOOM(!recovers_.oom());
}

void
CodeGeneratorShared::encode(LSnapshot* snapshot)
{
    if (snapshot->snapshotOffset() != INVALID_SNAPSHOT_OFFSET)
        return;

    LRecoverInfo* recoverInfo = snapshot->recoverInfo();
    encode(recoverInfo);

    RecoverOffset recoverOffset = recoverInfo->recoverOffset();
    MOZ_ASSERT(recoverOffset != INVALID_RECOVER_OFFSET);

    JitSpew(JitSpew_IonSnapshots, "Encoding LSnapshot %p (LRecover %p)",
            (void*)snapshot, (void*) recoverInfo);

    SnapshotOffset offset = snapshots_.startSnapshot(recoverOffset, snapshot->bailoutKind());

#ifdef TRACK_SNAPSHOTS
    uint32_t pcOpcode = 0;
    uint32_t lirOpcode = 0;
    uint32_t lirId = 0;
    uint32_t mirOpcode = 0;
    uint32_t mirId = 0;

    if (LNode* ins = instruction()) {
        lirOpcode = ins->op();
        lirId = ins->id();
        if (ins->mirRaw()) {
            mirOpcode = ins->mirRaw()->op();
            mirId = ins->mirRaw()->id();
            if (ins->mirRaw()->trackedPc())
                pcOpcode = *ins->mirRaw()->trackedPc();
        }
    }
    snapshots_.trackSnapshot(pcOpcode, mirOpcode, mirId, lirOpcode, lirId);
#endif

    uint32_t allocIndex = 0;
    for (LRecoverInfo::OperandIter it(recoverInfo); !it; ++it) {
        DebugOnly<uint32_t> allocWritten = snapshots_.allocWritten();
        encodeAllocation(snapshot, *it, &allocIndex);
        MOZ_ASSERT(allocWritten + 1 == snapshots_.allocWritten());
    }

    MOZ_ASSERT(allocIndex == snapshot->numSlots());
    snapshots_.endSnapshot();
    snapshot->setSnapshotOffset(offset);
    masm.propagateOOM(!snapshots_.oom());
}

bool
CodeGeneratorShared::assignBailoutId(LSnapshot* snapshot)
{
    MOZ_ASSERT(snapshot->snapshotOffset() != INVALID_SNAPSHOT_OFFSET);

    // Can we not use bailout tables at all?
    if (!deoptTable_)
        return false;

    MOZ_ASSERT(frameClass_ != FrameSizeClass::None());

    if (snapshot->bailoutId() != INVALID_BAILOUT_ID)
        return true;

    // Is the bailout table full?
    if (bailouts_.length() >= BAILOUT_TABLE_SIZE)
        return false;

    unsigned bailoutId = bailouts_.length();
    snapshot->setBailoutId(bailoutId);
    JitSpew(JitSpew_IonSnapshots, "Assigned snapshot bailout id %u", bailoutId);
    return bailouts_.append(snapshot->snapshotOffset());
}

void
CodeGeneratorShared::encodeSafepoints()
{
    for (SafepointIndex& index : safepointIndices_) {
        LSafepoint* safepoint = index.safepoint();

        if (!safepoint->encoded()) {
            safepoint->fixupOffset(&masm);
            safepoints_.encode(safepoint);
        }

        index.resolve();
    }
}

bool
CodeGeneratorShared::createNativeToBytecodeScriptList(JSContext* cx)
{
    js::Vector<JSScript*, 0, SystemAllocPolicy> scriptList;
    InlineScriptTree* tree = gen->info().inlineScriptTree();
    for (;;) {
        // Add script from current tree.
        bool found = false;
        for (uint32_t i = 0; i < scriptList.length(); i++) {
            if (scriptList[i] == tree->script()) {
                found = true;
                break;
            }
        }
        if (!found) {
            if (!scriptList.append(tree->script()))
                return false;
        }

        // Process rest of tree

        // If children exist, emit children.
        if (tree->hasChildren()) {
            tree = tree->firstChild();
            continue;
        }

        // Otherwise, find the first tree up the chain (including this one)
        // that contains a next sibling.
        while (!tree->hasNextCallee() && tree->hasCaller())
            tree = tree->caller();

        // If we found a sibling, use it.
        if (tree->hasNextCallee()) {
            tree = tree->nextCallee();
            continue;
        }

        // Otherwise, we must have reached the top without finding any siblings.
        MOZ_ASSERT(tree->isOutermostCaller());
        break;
    }

    // Allocate array for list.
    JSScript** data = cx->runtime()->pod_malloc<JSScript*>(scriptList.length());
    if (!data)
        return false;

    for (uint32_t i = 0; i < scriptList.length(); i++)
        data[i] = scriptList[i];

    // Success.
    nativeToBytecodeScriptListLength_ = scriptList.length();
    nativeToBytecodeScriptList_ = data;
    return true;
}

bool
CodeGeneratorShared::generateCompactNativeToBytecodeMap(JSContext* cx, JitCode* code)
{
    MOZ_ASSERT(nativeToBytecodeScriptListLength_ == 0);
    MOZ_ASSERT(nativeToBytecodeScriptList_ == nullptr);
    MOZ_ASSERT(nativeToBytecodeMap_ == nullptr);
    MOZ_ASSERT(nativeToBytecodeMapSize_ == 0);
    MOZ_ASSERT(nativeToBytecodeTableOffset_ == 0);
    MOZ_ASSERT(nativeToBytecodeNumRegions_ == 0);

    // Iterate through all nativeToBytecode entries, fix up their masm offsets.
    for (unsigned i = 0; i < nativeToBytecodeList_.length(); i++) {
        NativeToBytecode& entry = nativeToBytecodeList_[i];

        // Fixup code offsets.
        entry.nativeOffset = CodeOffsetLabel(masm.actualOffset(entry.nativeOffset.offset()));
    }

    if (!createNativeToBytecodeScriptList(cx))
        return false;

    MOZ_ASSERT(nativeToBytecodeScriptListLength_ > 0);
    MOZ_ASSERT(nativeToBytecodeScriptList_ != nullptr);

    CompactBufferWriter writer;
    uint32_t tableOffset = 0;
    uint32_t numRegions = 0;

    if (!JitcodeIonTable::WriteIonTable(
            writer, nativeToBytecodeScriptList_, nativeToBytecodeScriptListLength_,
            &nativeToBytecodeList_[0],
            &nativeToBytecodeList_[0] + nativeToBytecodeList_.length(),
            &tableOffset, &numRegions))
    {
        js_free(nativeToBytecodeScriptList_);
        return false;
    }

    MOZ_ASSERT(tableOffset > 0);
    MOZ_ASSERT(numRegions > 0);

    // Writer is done, copy it to sized buffer.
    uint8_t* data = cx->runtime()->pod_malloc<uint8_t>(writer.length());
    if (!data) {
        js_free(nativeToBytecodeScriptList_);
        return false;
    }

    memcpy(data, writer.buffer(), writer.length());
    nativeToBytecodeMap_ = data;
    nativeToBytecodeMapSize_ = writer.length();
    nativeToBytecodeTableOffset_ = tableOffset;
    nativeToBytecodeNumRegions_ = numRegions;

    verifyCompactNativeToBytecodeMap(code);

    JitSpew(JitSpew_Profiling, "Compact Native To Bytecode Map [%p-%p]",
            data, data + nativeToBytecodeMapSize_);

    return true;
}

void
CodeGeneratorShared::verifyCompactNativeToBytecodeMap(JitCode* code)
{
#ifdef DEBUG
    MOZ_ASSERT(nativeToBytecodeScriptListLength_ > 0);
    MOZ_ASSERT(nativeToBytecodeScriptList_ != nullptr);
    MOZ_ASSERT(nativeToBytecodeMap_ != nullptr);
    MOZ_ASSERT(nativeToBytecodeMapSize_ > 0);
    MOZ_ASSERT(nativeToBytecodeTableOffset_ > 0);
    MOZ_ASSERT(nativeToBytecodeNumRegions_ > 0);

    // The pointer to the table must be 4-byte aligned
    const uint8_t* tablePtr = nativeToBytecodeMap_ + nativeToBytecodeTableOffset_;
    MOZ_ASSERT(uintptr_t(tablePtr) % sizeof(uint32_t) == 0);

    // Verify that numRegions was encoded correctly.
    const JitcodeIonTable* ionTable = reinterpret_cast<const JitcodeIonTable*>(tablePtr);
    MOZ_ASSERT(ionTable->numRegions() == nativeToBytecodeNumRegions_);

    // Region offset for first region should be at the start of the payload region.
    // Since the offsets are backward from the start of the table, the first entry
    // backoffset should be equal to the forward table offset from the start of the
    // allocated data.
    MOZ_ASSERT(ionTable->regionOffset(0) == nativeToBytecodeTableOffset_);

    // Verify each region.
    for (uint32_t i = 0; i < ionTable->numRegions(); i++) {
        // Back-offset must point into the payload region preceding the table, not before it.
        MOZ_ASSERT(ionTable->regionOffset(i) <= nativeToBytecodeTableOffset_);

        // Back-offset must point to a later area in the payload region than previous
        // back-offset.  This means that back-offsets decrease monotonically.
        MOZ_ASSERT_IF(i > 0, ionTable->regionOffset(i) < ionTable->regionOffset(i - 1));

        JitcodeRegionEntry entry = ionTable->regionEntry(i);

        // Ensure native code offset for region falls within jitcode.
        MOZ_ASSERT(entry.nativeOffset() <= code->instructionsSize());

        // Read out script/pc stack and verify.
        JitcodeRegionEntry::ScriptPcIterator scriptPcIter = entry.scriptPcIterator();
        while (scriptPcIter.hasMore()) {
            uint32_t scriptIdx = 0, pcOffset = 0;
            scriptPcIter.readNext(&scriptIdx, &pcOffset);

            // Ensure scriptIdx refers to a valid script in the list.
            MOZ_ASSERT(scriptIdx < nativeToBytecodeScriptListLength_);
            JSScript* script = nativeToBytecodeScriptList_[scriptIdx];

            // Ensure pcOffset falls within the script.
            MOZ_ASSERT(pcOffset < script->length());
        }

        // Obtain the original nativeOffset and pcOffset and script.
        uint32_t curNativeOffset = entry.nativeOffset();
        JSScript* script = nullptr;
        uint32_t curPcOffset = 0;
        {
            uint32_t scriptIdx = 0;
            scriptPcIter.reset();
            scriptPcIter.readNext(&scriptIdx, &curPcOffset);
            script = nativeToBytecodeScriptList_[scriptIdx];
        }

        // Read out nativeDeltas and pcDeltas and verify.
        JitcodeRegionEntry::DeltaIterator deltaIter = entry.deltaIterator();
        while (deltaIter.hasMore()) {
            uint32_t nativeDelta = 0;
            int32_t pcDelta = 0;
            deltaIter.readNext(&nativeDelta, &pcDelta);

            curNativeOffset += nativeDelta;
            curPcOffset = uint32_t(int32_t(curPcOffset) + pcDelta);

            // Ensure that nativeOffset still falls within jitcode after delta.
            MOZ_ASSERT(curNativeOffset <= code->instructionsSize());

            // Ensure that pcOffset still falls within bytecode after delta.
            MOZ_ASSERT(curPcOffset < script->length());
        }
    }
#endif // DEBUG
}

bool
CodeGeneratorShared::generateCompactTrackedOptimizationsMap(JSContext* cx, JitCode* code,
                                                            IonTrackedTypeVector* allTypes)
{
    MOZ_ASSERT(trackedOptimizationsMap_ == nullptr);
    MOZ_ASSERT(trackedOptimizationsMapSize_ == 0);
    MOZ_ASSERT(trackedOptimizationsRegionTableOffset_ == 0);
    MOZ_ASSERT(trackedOptimizationsTypesTableOffset_ == 0);
    MOZ_ASSERT(trackedOptimizationsAttemptsTableOffset_ == 0);

    if (trackedOptimizations_.empty())
        return true;

    UniqueTrackedOptimizations unique(cx);
    if (!unique.init())
        return false;

    // Iterate through all entries, fix up their masm offsets and deduplicate
    // their optimization attempts.
    for (size_t i = 0; i < trackedOptimizations_.length(); i++) {
        NativeToTrackedOptimizations& entry = trackedOptimizations_[i];
        entry.startOffset = CodeOffsetLabel(masm.actualOffset(entry.startOffset.offset()));
        entry.endOffset = CodeOffsetLabel(masm.actualOffset(entry.endOffset.offset()));
        if (!unique.add(entry.optimizations))
            return false;
    }

    // Sort the unique optimization attempts by frequency to stabilize the
    // attempts' indices in the compact table we will write later.
    if (!unique.sortByFrequency(cx))
        return false;

    // Write out the ranges and the table.
    CompactBufferWriter writer;
    uint32_t numRegions;
    uint32_t regionTableOffset;
    uint32_t typesTableOffset;
    uint32_t attemptsTableOffset;
    if (!WriteIonTrackedOptimizationsTable(cx, writer,
                                           trackedOptimizations_.begin(),
                                           trackedOptimizations_.end(),
                                           unique, &numRegions,
                                           &regionTableOffset, &typesTableOffset,
                                           &attemptsTableOffset, allTypes))
    {
        return false;
    }

    MOZ_ASSERT(regionTableOffset > 0);
    MOZ_ASSERT(typesTableOffset > 0);
    MOZ_ASSERT(attemptsTableOffset > 0);
    MOZ_ASSERT(typesTableOffset > regionTableOffset);
    MOZ_ASSERT(attemptsTableOffset > typesTableOffset);

    // Copy over the table out of the writer's buffer.
    uint8_t* data = cx->runtime()->pod_malloc<uint8_t>(writer.length());
    if (!data)
        return false;

    memcpy(data, writer.buffer(), writer.length());
    trackedOptimizationsMap_ = data;
    trackedOptimizationsMapSize_ = writer.length();
    trackedOptimizationsRegionTableOffset_ = regionTableOffset;
    trackedOptimizationsTypesTableOffset_ = typesTableOffset;
    trackedOptimizationsAttemptsTableOffset_ = attemptsTableOffset;

    verifyCompactTrackedOptimizationsMap(code, numRegions, unique, allTypes);

    JitSpew(JitSpew_OptimizationTracking,
            "== Compact Native To Optimizations Map [%p-%p] size %u",
            data, data + trackedOptimizationsMapSize_, trackedOptimizationsMapSize_);
    JitSpew(JitSpew_OptimizationTracking,
            "     with type list of length %u, size %u",
            allTypes->length(), allTypes->length() * sizeof(IonTrackedTypeWithAddendum));

    return true;
}

#ifdef DEBUG
// Since this is a DEBUG-only verification, crash on OOM in the forEach ops
// below.

class ReadTempAttemptsVectorOp : public JS::ForEachTrackedOptimizationAttemptOp
{
    TempOptimizationAttemptsVector* attempts_;

  public:
    explicit ReadTempAttemptsVectorOp(TempOptimizationAttemptsVector* attempts)
      : attempts_(attempts)
    { }

    void operator()(JS::TrackedStrategy strategy, JS::TrackedOutcome outcome) override {
        MOZ_ALWAYS_TRUE(attempts_->append(OptimizationAttempt(strategy, outcome)));
    }
};

struct ReadTempTypeInfoVectorOp : public IonTrackedOptimizationsTypeInfo::ForEachOp
{
    TempAllocator& alloc_;
    TempOptimizationTypeInfoVector* types_;
    TempTypeList accTypes_;

  public:
    ReadTempTypeInfoVectorOp(TempAllocator& alloc, TempOptimizationTypeInfoVector* types)
      : alloc_(alloc),
        types_(types),
        accTypes_(alloc)
    { }

    void readType(const IonTrackedTypeWithAddendum& tracked) override {
        MOZ_ALWAYS_TRUE(accTypes_.append(tracked.type));
    }

    void operator()(JS::TrackedTypeSite site, MIRType mirType) override {
        OptimizationTypeInfo ty(alloc_, site, mirType);
        for (uint32_t i = 0; i < accTypes_.length(); i++)
            MOZ_ALWAYS_TRUE(ty.trackType(accTypes_[i]));
        MOZ_ALWAYS_TRUE(types_->append(mozilla::Move(ty)));
        accTypes_.clear();
    }
};
#endif // DEBUG

void
CodeGeneratorShared::verifyCompactTrackedOptimizationsMap(JitCode* code, uint32_t numRegions,
                                                          const UniqueTrackedOptimizations& unique,
                                                          const IonTrackedTypeVector* allTypes)
{
#ifdef DEBUG
    MOZ_ASSERT(trackedOptimizationsMap_ != nullptr);
    MOZ_ASSERT(trackedOptimizationsMapSize_ > 0);
    MOZ_ASSERT(trackedOptimizationsRegionTableOffset_ > 0);
    MOZ_ASSERT(trackedOptimizationsTypesTableOffset_ > 0);
    MOZ_ASSERT(trackedOptimizationsAttemptsTableOffset_ > 0);

    // Table pointers must all be 4-byte aligned.
    const uint8_t* regionTableAddr = trackedOptimizationsMap_ +
                                     trackedOptimizationsRegionTableOffset_;
    const uint8_t* typesTableAddr = trackedOptimizationsMap_ +
                                    trackedOptimizationsTypesTableOffset_;
    const uint8_t* attemptsTableAddr = trackedOptimizationsMap_ +
                                       trackedOptimizationsAttemptsTableOffset_;
    MOZ_ASSERT(uintptr_t(regionTableAddr) % sizeof(uint32_t) == 0);
    MOZ_ASSERT(uintptr_t(typesTableAddr) % sizeof(uint32_t) == 0);
    MOZ_ASSERT(uintptr_t(attemptsTableAddr) % sizeof(uint32_t) == 0);

    // Assert that the number of entries matches up for the tables.
    const IonTrackedOptimizationsRegionTable* regionTable =
        (const IonTrackedOptimizationsRegionTable*) regionTableAddr;
    MOZ_ASSERT(regionTable->numEntries() == numRegions);
    const IonTrackedOptimizationsTypesTable* typesTable =
        (const IonTrackedOptimizationsTypesTable*) typesTableAddr;
    MOZ_ASSERT(typesTable->numEntries() == unique.count());
    const IonTrackedOptimizationsAttemptsTable* attemptsTable =
        (const IonTrackedOptimizationsAttemptsTable*) attemptsTableAddr;
    MOZ_ASSERT(attemptsTable->numEntries() == unique.count());

    // Verify each region.
    uint32_t trackedIdx = 0;
    for (uint32_t regionIdx = 0; regionIdx < regionTable->numEntries(); regionIdx++) {
        // Check reverse offsets are within bounds.
        MOZ_ASSERT(regionTable->entryOffset(regionIdx) <= trackedOptimizationsRegionTableOffset_);
        MOZ_ASSERT_IF(regionIdx > 0, regionTable->entryOffset(regionIdx) <
                                     regionTable->entryOffset(regionIdx - 1));

        IonTrackedOptimizationsRegion region = regionTable->entry(regionIdx);

        // Check the region range is covered by jitcode.
        MOZ_ASSERT(region.startOffset() <= code->instructionsSize());
        MOZ_ASSERT(region.endOffset() <= code->instructionsSize());

        IonTrackedOptimizationsRegion::RangeIterator iter = region.ranges();
        while (iter.more()) {
            // Assert that the offsets are correctly decoded from the delta.
            uint32_t startOffset, endOffset;
            uint8_t index;
            iter.readNext(&startOffset, &endOffset, &index);
            NativeToTrackedOptimizations& entry = trackedOptimizations_[trackedIdx++];
            MOZ_ASSERT(startOffset == entry.startOffset.offset());
            MOZ_ASSERT(endOffset == entry.endOffset.offset());
            MOZ_ASSERT(index == unique.indexOf(entry.optimizations));

            // Assert that the type info and attempts vectors are correctly
            // decoded. This is disabled for now if the types table might
            // contain nursery pointers, in which case the types might not
            // match, see bug 1175761.
            if (!code->runtimeFromMainThread()->gc.storeBuffer.cancelIonCompilations()) {
                IonTrackedOptimizationsTypeInfo typeInfo = typesTable->entry(index);
                TempOptimizationTypeInfoVector tvec(alloc());
                ReadTempTypeInfoVectorOp top(alloc(), &tvec);
                typeInfo.forEach(top, allTypes);
                MOZ_ASSERT(entry.optimizations->matchTypes(tvec));
            }

            IonTrackedOptimizationsAttempts attempts = attemptsTable->entry(index);
            TempOptimizationAttemptsVector avec(alloc());
            ReadTempAttemptsVectorOp aop(&avec);
            attempts.forEach(aop);
            MOZ_ASSERT(entry.optimizations->matchAttempts(avec));
        }
    }
#endif
}

void
CodeGeneratorShared::markSafepoint(LInstruction* ins)
{
    markSafepointAt(masm.currentOffset(), ins);
}

void
CodeGeneratorShared::markSafepointAt(uint32_t offset, LInstruction* ins)
{
    MOZ_ASSERT_IF(!safepointIndices_.empty(),
                  offset - safepointIndices_.back().displacement() >= sizeof(uint32_t));
    masm.propagateOOM(safepointIndices_.append(SafepointIndex(offset, ins->safepoint())));
}

void
CodeGeneratorShared::ensureOsiSpace()
{
    // For a refresher, an invalidation point is of the form:
    // 1: call <target>
    // 2: ...
    // 3: <osipoint>
    //
    // The four bytes *before* instruction 2 are overwritten with an offset.
    // Callers must ensure that the instruction itself has enough bytes to
    // support this.
    //
    // The bytes *at* instruction 3 are overwritten with an invalidation jump.
    // jump. These bytes may be in a completely different IR sequence, but
    // represent the join point of the call out of the function.
    //
    // At points where we want to ensure that invalidation won't corrupt an
    // important instruction, we make sure to pad with nops.
    if (masm.currentOffset() - lastOsiPointOffset_ < Assembler::PatchWrite_NearCallSize()) {
        int32_t paddingSize = Assembler::PatchWrite_NearCallSize();
        paddingSize -= masm.currentOffset() - lastOsiPointOffset_;
        for (int32_t i = 0; i < paddingSize; ++i)
            masm.nop();
    }
    MOZ_ASSERT(masm.currentOffset() - lastOsiPointOffset_ >= Assembler::PatchWrite_NearCallSize());
    lastOsiPointOffset_ = masm.currentOffset();
}

uint32_t
CodeGeneratorShared::markOsiPoint(LOsiPoint* ins)
{
    encode(ins->snapshot());
    ensureOsiSpace();

    uint32_t offset = masm.currentOffset();
    SnapshotOffset so = ins->snapshot()->snapshotOffset();
    masm.propagateOOM(osiIndices_.append(OsiIndex(offset, so)));

    return offset;
}

#ifdef CHECK_OSIPOINT_REGISTERS
template <class Op>
static void
HandleRegisterDump(Op op, MacroAssembler& masm, LiveRegisterSet liveRegs, Register activation,
                   Register scratch)
{
    const size_t baseOffset = JitActivation::offsetOfRegs();

    // Handle live GPRs.
    for (GeneralRegisterIterator iter(liveRegs.gprs()); iter.more(); iter++) {
        Register reg = *iter;
        Address dump(activation, baseOffset + RegisterDump::offsetOfRegister(reg));

        if (reg == activation) {
            // To use the original value of the activation register (that's
            // now on top of the stack), we need the scratch register.
            masm.push(scratch);
            masm.loadPtr(Address(masm.getStackPointer(), sizeof(uintptr_t)), scratch);
            op(scratch, dump);
            masm.pop(scratch);
        } else {
            op(reg, dump);
        }
    }

    // Handle live FPRs.
    for (FloatRegisterIterator iter(liveRegs.fpus()); iter.more(); iter++) {
        FloatRegister reg = *iter;
        Address dump(activation, baseOffset + RegisterDump::offsetOfRegister(reg));
        op(reg, dump);
    }
}

class StoreOp
{
    MacroAssembler& masm;

  public:
    explicit StoreOp(MacroAssembler& masm)
      : masm(masm)
    {}

    void operator()(Register reg, Address dump) {
        masm.storePtr(reg, dump);
    }
    void operator()(FloatRegister reg, Address dump) {
        if (reg.isDouble())
            masm.storeDouble(reg, dump);
        else if (reg.isSingle())
            masm.storeFloat32(reg, dump);
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
        else if (reg.isInt32x4())
            masm.storeUnalignedInt32x4(reg, dump);
        else if (reg.isFloat32x4())
            masm.storeUnalignedFloat32x4(reg, dump);
#endif
        else
            MOZ_CRASH("Unexpected register type.");
    }
};

static void
StoreAllLiveRegs(MacroAssembler& masm, LiveRegisterSet liveRegs)
{
    // Store a copy of all live registers before performing the call.
    // When we reach the OsiPoint, we can use this to check nothing
    // modified them in the meantime.

    // Load pointer to the JitActivation in a scratch register.
    AllocatableGeneralRegisterSet allRegs(GeneralRegisterSet::All());
    Register scratch = allRegs.takeAny();
    masm.push(scratch);
    masm.loadJitActivation(scratch);

    Address checkRegs(scratch, JitActivation::offsetOfCheckRegs());
    masm.add32(Imm32(1), checkRegs);

    StoreOp op(masm);
    HandleRegisterDump<StoreOp>(op, masm, liveRegs, scratch, allRegs.getAny());

    masm.pop(scratch);
}

class VerifyOp
{
    MacroAssembler& masm;
    Label* failure_;

  public:
    VerifyOp(MacroAssembler& masm, Label* failure)
      : masm(masm), failure_(failure)
    {}

    void operator()(Register reg, Address dump) {
        masm.branchPtr(Assembler::NotEqual, dump, reg, failure_);
    }
    void operator()(FloatRegister reg, Address dump) {
        FloatRegister scratch;
        if (reg.isDouble()) {
            scratch = ScratchDoubleReg;
            masm.loadDouble(dump, scratch);
            masm.branchDouble(Assembler::DoubleNotEqual, scratch, reg, failure_);
        } else if (reg.isSingle()) {
            scratch = ScratchFloat32Reg;
            masm.loadFloat32(dump, scratch);
            masm.branchFloat(Assembler::DoubleNotEqual, scratch, reg, failure_);
        }

        // :TODO: (Bug 1133745) Add support to verify SIMD registers.
    }
};

void
CodeGeneratorShared::verifyOsiPointRegs(LSafepoint* safepoint)
{
    // Ensure the live registers stored by callVM did not change between
    // the call and this OsiPoint. Try-catch relies on this invariant.

    // Load pointer to the JitActivation in a scratch register.
    AllocatableGeneralRegisterSet allRegs(GeneralRegisterSet::All());
    Register scratch = allRegs.takeAny();
    masm.push(scratch);
    masm.loadJitActivation(scratch);

    // If we should not check registers (because the instruction did not call
    // into the VM, or a GC happened), we're done.
    Label failure, done;
    Address checkRegs(scratch, JitActivation::offsetOfCheckRegs());
    masm.branch32(Assembler::Equal, checkRegs, Imm32(0), &done);

    // Having more than one VM function call made in one visit function at
    // runtime is a sec-ciritcal error, because if we conservatively assume that
    // one of the function call can re-enter Ion, then the invalidation process
    // will potentially add a call at a random location, by patching the code
    // before the return address.
    masm.branch32(Assembler::NotEqual, checkRegs, Imm32(1), &failure);

    // Set checkRegs to 0, so that we don't try to verify registers after we
    // return from this script to the caller.
    masm.store32(Imm32(0), checkRegs);

    // Ignore clobbered registers. Some instructions (like LValueToInt32) modify
    // temps after calling into the VM. This is fine because no other
    // instructions (including this OsiPoint) will depend on them. Also
    // backtracking can also use the same register for an input and an output.
    // These are marked as clobbered and shouldn't get checked.
    LiveRegisterSet liveRegs;
    liveRegs.set() = RegisterSet::Intersect(safepoint->liveRegs().set(),
                                            RegisterSet::Not(safepoint->clobberedRegs().set()));

    VerifyOp op(masm, &failure);
    HandleRegisterDump<VerifyOp>(op, masm, liveRegs, scratch, allRegs.getAny());

    masm.jump(&done);

    // Do not profile the callWithABI that occurs below.  This is to avoid a
    // rare corner case that occurs when profiling interacts with itself:
    //
    // When slow profiling assertions are turned on, FunctionBoundary ops
    // (which update the profiler pseudo-stack) may emit a callVM, which
    // forces them to have an osi point associated with them.  The
    // FunctionBoundary for inline function entry is added to the caller's
    // graph with a PC from the caller's code, but during codegen it modifies
    // SPS instrumentation to add the callee as the current top-most script.
    // When codegen gets to the OSIPoint, and the callWithABI below is
    // emitted, the codegen thinks that the current frame is the callee, but
    // the PC it's using from the OSIPoint refers to the caller.  This causes
    // the profiler instrumentation of the callWithABI below to ASSERT, since
    // the script and pc are mismatched.  To avoid this, we simply omit
    // instrumentation for these callWithABIs.

    // Any live register captured by a safepoint (other than temp registers)
    // must remain unchanged between the call and the OsiPoint instruction.
    masm.bind(&failure);
    masm.assumeUnreachable("Modified registers between VM call and OsiPoint");

    masm.bind(&done);
    masm.pop(scratch);
}

bool
CodeGeneratorShared::shouldVerifyOsiPointRegs(LSafepoint* safepoint)
{
    if (!checkOsiPointRegisters)
        return false;

    if (safepoint->liveRegs().emptyGeneral() && safepoint->liveRegs().emptyFloat())
        return false; // No registers to check.

    return true;
}

void
CodeGeneratorShared::resetOsiPointRegs(LSafepoint* safepoint)
{
    if (!shouldVerifyOsiPointRegs(safepoint))
        return;

    // Set checkRegs to 0. If we perform a VM call, the instruction
    // will set it to 1.
    AllocatableGeneralRegisterSet allRegs(GeneralRegisterSet::All());
    Register scratch = allRegs.takeAny();
    masm.push(scratch);
    masm.loadJitActivation(scratch);
    Address checkRegs(scratch, JitActivation::offsetOfCheckRegs());
    masm.store32(Imm32(0), checkRegs);
    masm.pop(scratch);
}
#endif

// Before doing any call to Cpp, you should ensure that volatile
// registers are evicted by the register allocator.
void
CodeGeneratorShared::callVM(const VMFunction& fun, LInstruction* ins, const Register* dynStack)
{
    // If we're calling a function with an out parameter type of double, make
    // sure we have an FPU.
    MOZ_ASSERT_IF(fun.outParam == Type_Double, GetJitContext()->runtime->jitSupportsFloatingPoint());

#ifdef DEBUG
    if (ins->mirRaw()) {
        MOZ_ASSERT(ins->mirRaw()->isInstruction());
        MInstruction* mir = ins->mirRaw()->toInstruction();
        MOZ_ASSERT_IF(mir->needsResumePoint(), mir->resumePoint());
    }
#endif

#ifdef JS_TRACE_LOGGING
    emitTracelogStartEvent(TraceLogger_VM);
#endif

    // Stack is:
    //    ... frame ...
    //    [args]
#ifdef DEBUG
    MOZ_ASSERT(pushedArgs_ == fun.explicitArgs);
    pushedArgs_ = 0;
#endif

    // Get the wrapper of the VM function.
    JitCode* wrapper = gen->jitRuntime()->getVMWrapper(fun);
    if (!wrapper) {
        masm.setOOM();
        return;
    }

#ifdef CHECK_OSIPOINT_REGISTERS
    if (shouldVerifyOsiPointRegs(ins->safepoint()))
        StoreAllLiveRegs(masm, ins->safepoint()->liveRegs());
#endif

    // Call the wrapper function.  The wrapper is in charge to unwind the stack
    // when returning from the call.  Failures are handled with exceptions based
    // on the return value of the C functions.  To guard the outcome of the
    // returned value, use another LIR instruction.
    uint32_t callOffset;
    if (dynStack)
        callOffset = masm.callWithExitFrame(wrapper, *dynStack);
    else
        callOffset = masm.callWithExitFrame(wrapper);

    markSafepointAt(callOffset, ins);

    // Remove rest of the frame left on the stack. We remove the return address
    // which is implicitly poped when returning.
    int framePop = sizeof(ExitFrameLayout) - sizeof(void*);

    // Pop arguments from framePushed.
    masm.implicitPop(fun.explicitStackSlots() * sizeof(void*) + framePop);
    // Stack is:
    //    ... frame ...

#ifdef JS_TRACE_LOGGING
    emitTracelogStopEvent(TraceLogger_VM);
#endif
}

class OutOfLineTruncateSlow : public OutOfLineCodeBase<CodeGeneratorShared>
{
    FloatRegister src_;
    Register dest_;
    bool needFloat32Conversion_;

  public:
    OutOfLineTruncateSlow(FloatRegister src, Register dest, bool needFloat32Conversion = false)
      : src_(src), dest_(dest), needFloat32Conversion_(needFloat32Conversion)
    { }

    void accept(CodeGeneratorShared* codegen) {
        codegen->visitOutOfLineTruncateSlow(this);
    }
    FloatRegister src() const {
        return src_;
    }
    Register dest() const {
        return dest_;
    }
    bool needFloat32Conversion() const {
        return needFloat32Conversion_;
    }

};

OutOfLineCode*
CodeGeneratorShared::oolTruncateDouble(FloatRegister src, Register dest, MInstruction* mir)
{
    OutOfLineTruncateSlow* ool = new(alloc()) OutOfLineTruncateSlow(src, dest);
    addOutOfLineCode(ool, mir);
    return ool;
}

void
CodeGeneratorShared::emitTruncateDouble(FloatRegister src, Register dest, MInstruction* mir)
{
    OutOfLineCode* ool = oolTruncateDouble(src, dest, mir);

    masm.branchTruncateDouble(src, dest, ool->entry());
    masm.bind(ool->rejoin());
}

void
CodeGeneratorShared::emitTruncateFloat32(FloatRegister src, Register dest, MInstruction* mir)
{
    OutOfLineTruncateSlow* ool = new(alloc()) OutOfLineTruncateSlow(src, dest, true);
    addOutOfLineCode(ool, mir);

    masm.branchTruncateFloat32(src, dest, ool->entry());
    masm.bind(ool->rejoin());
}

void
CodeGeneratorShared::visitOutOfLineTruncateSlow(OutOfLineTruncateSlow* ool)
{
    FloatRegister src = ool->src();
    Register dest = ool->dest();

    saveVolatile(dest);
#if defined(JS_CODEGEN_ARM)
    if (ool->needFloat32Conversion()) {
        masm.convertFloat32ToDouble(src, ScratchDoubleReg);
        src = ScratchDoubleReg;
    }

#else
    FloatRegister srcSingle = src.asSingle();
    if (ool->needFloat32Conversion()) {
        MOZ_ASSERT(src.isSingle());
        masm.push(src);
        masm.convertFloat32ToDouble(src, src);
        src = src.asDouble();
    }
#endif
    masm.setupUnalignedABICall(1, dest);
    masm.passABIArg(src, MoveOp::DOUBLE);
    if (gen->compilingAsmJS())
        masm.callWithABI(AsmJSImm_ToInt32);
    else
        masm.callWithABI(BitwiseCast<void*, int32_t(*)(double)>(JS::ToInt32));
    masm.storeCallResult(dest);

#if !defined(JS_CODEGEN_ARM)
    if (ool->needFloat32Conversion())
        masm.pop(srcSingle);
#endif
    restoreVolatile(dest);

    masm.jump(ool->rejoin());
}

bool
CodeGeneratorShared::omitOverRecursedCheck() const
{
    // If the current function makes no calls (which means it isn't recursive)
    // and it uses only a small amount of stack space, it doesn't need a
    // stack overflow check. Note that the actual number here is somewhat
    // arbitrary, and codegen actually uses small bounded amounts of
    // additional stack space in some cases too.
    return frameSize() < 64 && !gen->performsCall();
}

void
CodeGeneratorShared::emitAsmJSCall(LAsmJSCall* ins)
{
    MAsmJSCall* mir = ins->mir();

    if (mir->spIncrement())
        masm.freeStack(mir->spIncrement());

    MOZ_ASSERT((sizeof(AsmJSFrame) + masm.framePushed()) % AsmJSStackAlignment == 0);

#ifdef DEBUG
    static_assert(AsmJSStackAlignment >= ABIStackAlignment &&
                  AsmJSStackAlignment % ABIStackAlignment == 0,
                  "The asm.js stack alignment should subsume the ABI-required alignment");
    Label ok;
    masm.branchTestStackPtr(Assembler::Zero, Imm32(AsmJSStackAlignment - 1), &ok);
    masm.breakpoint();
    masm.bind(&ok);
#endif

    MAsmJSCall::Callee callee = mir->callee();
    switch (callee.which()) {
      case MAsmJSCall::Callee::Internal:
        masm.call(mir->desc(), callee.internal());
        break;
      case MAsmJSCall::Callee::Dynamic:
        masm.call(mir->desc(), ToRegister(ins->getOperand(mir->dynamicCalleeOperandIndex())));
        break;
      case MAsmJSCall::Callee::Builtin:
        masm.call(AsmJSImmPtr(callee.builtin()));
        break;
    }

    if (mir->spIncrement())
        masm.reserveStack(mir->spIncrement());
}

void
CodeGeneratorShared::emitPreBarrier(Register base, const LAllocation* index)
{
    if (index->isConstant()) {
        Address address(base, ToInt32(index) * sizeof(Value));
        masm.patchableCallPreBarrier(address, MIRType_Value);
    } else {
        BaseIndex address(base, ToRegister(index), TimesEight);
        masm.patchableCallPreBarrier(address, MIRType_Value);
    }
}

void
CodeGeneratorShared::emitPreBarrier(Address address)
{
    masm.patchableCallPreBarrier(address, MIRType_Value);
}

Label*
CodeGeneratorShared::labelForBackedgeWithImplicitCheck(MBasicBlock* mir)
{
    // If this is a loop backedge to a loop header with an implicit interrupt
    // check, use a patchable jump. Skip this search if compiling without a
    // script for asm.js, as there will be no interrupt check instruction.
    // Due to critical edge unsplitting there may no longer be unique loop
    // backedges, so just look for any edge going to an earlier block in RPO.
    if (!gen->compilingAsmJS() && mir->isLoopHeader() && mir->id() <= current->mir()->id()) {
        for (LInstructionIterator iter = mir->lir()->begin(); iter != mir->lir()->end(); iter++) {
            if (iter->isMoveGroup()) {
                // Continue searching for an interrupt check.
            } else if (iter->isInterruptCheckImplicit()) {
                return iter->toInterruptCheckImplicit()->oolEntry();
            } else {
                // The interrupt check should be the first instruction in the
                // loop header other than the initial label and move groups.
                MOZ_ASSERT(iter->isInterruptCheck());
                return nullptr;
            }
        }
    }

    return nullptr;
}

void
CodeGeneratorShared::jumpToBlock(MBasicBlock* mir)
{
    // Skip past trivial blocks.
    mir = skipTrivialBlocks(mir);

    // No jump necessary if we can fall through to the next block.
    if (isNextBlock(mir->lir()))
        return;

    if (Label* oolEntry = labelForBackedgeWithImplicitCheck(mir)) {
        // Note: the backedge is initially a jump to the next instruction.
        // It will be patched to the target block's label during link().
        RepatchLabel rejoin;
        CodeOffsetJump backedge = masm.backedgeJump(&rejoin);
        masm.bind(&rejoin);

        masm.propagateOOM(patchableBackedges_.append(PatchableBackedgeInfo(backedge, mir->lir()->label(), oolEntry)));
    } else {
        masm.jump(mir->lir()->label());
    }
}

// This function is not used for MIPS. MIPS has branchToBlock.
#ifndef JS_CODEGEN_MIPS
void
CodeGeneratorShared::jumpToBlock(MBasicBlock* mir, Assembler::Condition cond)
{
    // Skip past trivial blocks.
    mir = skipTrivialBlocks(mir);

    if (Label* oolEntry = labelForBackedgeWithImplicitCheck(mir)) {
        // Note: the backedge is initially a jump to the next instruction.
        // It will be patched to the target block's label during link().
        RepatchLabel rejoin;
        CodeOffsetJump backedge = masm.jumpWithPatch(&rejoin, cond);
        masm.bind(&rejoin);

        masm.propagateOOM(patchableBackedges_.append(PatchableBackedgeInfo(backedge, mir->lir()->label(), oolEntry)));
    } else {
        masm.j(cond, mir->lir()->label());
    }
}
#endif

size_t
CodeGeneratorShared::addCacheLocations(const CacheLocationList& locs, size_t* numLocs)
{
    size_t firstIndex = runtimeData_.length();
    size_t numLocations = 0;
    for (CacheLocationList::iterator iter = locs.begin(); iter != locs.end(); iter++) {
        // allocateData() ensures that sizeof(CacheLocation) is word-aligned.
        // If this changes, we will need to pad to ensure alignment.
        size_t curIndex = allocateData(sizeof(CacheLocation));
        new (&runtimeData_[curIndex]) CacheLocation(iter->pc, iter->script);
        numLocations++;
    }
    MOZ_ASSERT(numLocations != 0);
    *numLocs = numLocations;
    return firstIndex;
}

ReciprocalMulConstants
CodeGeneratorShared::computeDivisionConstants(int d) {
    // In what follows, d is positive and is not a power of 2.
    MOZ_ASSERT(d > 0 && (d & (d - 1)) != 0);

    // Speeding up division by non power-of-2 constants is possible by
    // calculating, during compilation, a value M such that high-order
    // bits of M*n correspond to the result of the division. Formally,
    // we compute values 0 <= M < 2^32 and 0 <= s < 31 such that
    //         (M * n) >> (32 + s) = floor(n/d)    if n >= 0
    //         (M * n) >> (32 + s) = ceil(n/d) - 1 if n < 0.
    // The original presentation of this technique appears in Hacker's
    // Delight, a book by Henry S. Warren, Jr.. A proof of correctness
    // for our version follows.

    // Define p = 32 + s, M = ceil(2^p/d), and assume that s satisfies
    //                     M - 2^p/d <= 2^(s+1)/d.                 (1)
    // (Observe that s = FloorLog32(d) satisfies this, because in this
    // case d <= 2^(s+1) and so the RHS of (1) is at least one). Then,
    //
    // a) If s <= FloorLog32(d), then M <= 2^32 - 1.
    // Proof: Indeed, M is monotone in s and, for s = FloorLog32(d),
    // the inequalities 2^31 > d >= 2^s + 1 readily imply
    //    2^p / d  = 2^p/(d - 1) * (d - 1)/d
    //            <= 2^32 * (1 - 1/d) < 2 * (2^31 - 1) = 2^32 - 2.
    // The claim follows by applying the ceiling function.
    //
    // b) For any 0 <= n < 2^31, floor(Mn/2^p) = floor(n/d).
    // Proof: Put x = floor(Mn/2^p); it's the unique integer for which
    //                    Mn/2^p - 1 < x <= Mn/2^p.                (2)
    // Using M >= 2^p/d on the LHS and (1) on the RHS, we get
    //           n/d - 1 < x <= n/d + n/(2^31 d) < n/d + 1/d.
    // Since x is an integer, it's not in the interval (n/d, (n+1)/d),
    // and so n/d - 1 < x <= n/d, which implies x = floor(n/d).
    //
    // c) For any -2^31 <= n < 0, floor(Mn/2^p) + 1 = ceil(n/d).
    // Proof: The proof is similar. Equation (2) holds as above. Using
    // M > 2^p/d (d isn't a power of 2) on the RHS and (1) on the LHS,
    //                 n/d + n/(2^31 d) - 1 < x < n/d.
    // Using n >= -2^31 and summing 1,
    //                  n/d - 1/d < x + 1 < n/d + 1.
    // Since x + 1 is an integer, this implies n/d <= x + 1 < n/d + 1.
    // In other words, x + 1 = ceil(n/d).
    //
    // Condition (1) isn't necessary for the existence of M and s with
    // the properties above. Hacker's Delight provides a slightly less
    // restrictive condition when d >= 196611, at the cost of a 3-page
    // proof of correctness.

    // Note that, since d*M - 2^p = d - (2^p)%d, (1) can be written as
    //                   2^(s+1) >= d - (2^p)%d.
    // We now compute the least s with this property...

    int32_t shift = 0;
    while ((int64_t(1) << (shift+1)) + (int64_t(1) << (shift+32)) % d < d)
        shift++;

    // ...and the corresponding M. This may not fit in a signed 32-bit
    // integer; we will compute (M - 2^32) * n + (2^32 * n) instead of
    // M * n if this is the case (cf. item (a) above).
    ReciprocalMulConstants rmc;
    rmc.multiplier = int32_t((int64_t(1) << (shift+32))/d + 1);
    rmc.shiftAmount = shift;

    return rmc;
}


#ifdef JS_TRACE_LOGGING

void
CodeGeneratorShared::emitTracelogScript(bool isStart)
{
    if (!TraceLogTextIdEnabled(TraceLogger_Scripts))
        return;

    Label done;

    AllocatableRegisterSet regs(RegisterSet::Volatile());
    Register logger = regs.takeAnyGeneral();
    Register script = regs.takeAnyGeneral();

    masm.Push(logger);

    CodeOffsetLabel patchLogger = masm.movWithPatch(ImmPtr(nullptr), logger);
    masm.propagateOOM(patchableTraceLoggers_.append(patchLogger));

    Address enabledAddress(logger, TraceLoggerThread::offsetOfEnabled());
    masm.branch32(Assembler::Equal, enabledAddress, Imm32(0), &done);

    masm.Push(script);

    CodeOffsetLabel patchScript = masm.movWithPatch(ImmWord(0), script);
    masm.propagateOOM(patchableTLScripts_.append(patchScript));

    if (isStart)
        masm.tracelogStartId(logger, script);
    else
        masm.tracelogStopId(logger, script);

    masm.Pop(script);

    masm.bind(&done);

    masm.Pop(logger);
}

void
CodeGeneratorShared::emitTracelogTree(bool isStart, uint32_t textId)
{
    if (!TraceLogTextIdEnabled(textId))
        return;

    Label done;
    AllocatableRegisterSet regs(RegisterSet::Volatile());
    Register logger = regs.takeAnyGeneral();

    masm.Push(logger);

    CodeOffsetLabel patchLocation = masm.movWithPatch(ImmPtr(nullptr), logger);
    masm.propagateOOM(patchableTraceLoggers_.append(patchLocation));

    Address enabledAddress(logger, TraceLoggerThread::offsetOfEnabled());
    masm.branch32(Assembler::Equal, enabledAddress, Imm32(0), &done);

    if (isStart)
        masm.tracelogStartId(logger, textId);
    else
        masm.tracelogStopId(logger, textId);

    masm.bind(&done);

    masm.Pop(logger);
}
#endif

} // namespace jit
} // namespace js
