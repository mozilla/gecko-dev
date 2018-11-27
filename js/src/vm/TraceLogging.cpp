/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/TraceLogging.h"

#include "mozilla/EndianUtils.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ScopeExit.h"

#include <string.h>
#include <utility>

#include "jit/BaselineJIT.h"
#include "jit/CompileWrappers.h"
#include "js/Printf.h"
#include "js/TraceLoggerAPI.h"
#include "threading/LockGuard.h"
#include "util/Text.h"
#include "vm/JSScript.h"
#include "vm/Runtime.h"
#include "vm/Time.h"
#include "vm/TraceLoggingGraph.h"

#include "jit/JitFrames-inl.h"

using namespace js;

TraceLoggerThreadState* traceLoggerState = nullptr;

static bool
EnsureTraceLoggerState()
{
    if (MOZ_LIKELY(traceLoggerState)) {
        return true;
    }

    traceLoggerState = js_new<TraceLoggerThreadState>();
    if (!traceLoggerState) {
        return false;
    }

    if (!traceLoggerState->init()) {
        DestroyTraceLoggerThreadState();
        return false;
    }

    return true;
}

size_t
js::SizeOfTraceLogState(mozilla::MallocSizeOf mallocSizeOf)
{
    return traceLoggerState ? traceLoggerState->sizeOfIncludingThis(mallocSizeOf) : 0;
}

void
js::ResetTraceLogger()
{
    if (!traceLoggerState) {
        return;
    }

    traceLoggerState->clear();
}

void
js::DestroyTraceLoggerThreadState()
{
    if (traceLoggerState) {
        js_delete(traceLoggerState);
        traceLoggerState = nullptr;
    }
}

#ifdef DEBUG
bool
js::CurrentThreadOwnsTraceLoggerThreadStateLock()
{
    return traceLoggerState && traceLoggerState->lock.ownedByCurrentThread();
}
#endif

void
js::DestroyTraceLogger(TraceLoggerThread* logger)
{
    if (!EnsureTraceLoggerState()) {
        return;
    }
    traceLoggerState->destroyLogger(logger);
}

bool
TraceLoggerThread::init()
{
    if (!events.init()) {
        return false;
    }

    // Minimum amount of capacity needed for operation to allow flushing.
    // Flushing requires space for the actual event and two spaces to log the
    // start and stop of flushing.
    if (!events.ensureSpaceBeforeAdd(3)) {
        return false;
    }

    return true;
}

void
TraceLoggerThread::initGraph()
{
    // Create a graph. I don't like this is called reset, but it locks the
    // graph into the UniquePtr. So it gets deleted when TraceLoggerThread
    // is destructed.
    graph.reset(js_new<TraceLoggerGraph>());
    if (!graph.get()) {
        return;
    }

    MOZ_ASSERT(traceLoggerState);
    bool graphFile = traceLoggerState->isGraphFileEnabled();
    double delta = traceLoggerState->getTimeStampOffset(mozilla::TimeStamp::Now());
    uint64_t start = static_cast<uint64_t>(delta);
    if (!graph->init(start, graphFile)) {
        graph = nullptr;
        return;
    }

    if (graphFile) {
        // Report the textIds to the graph.
        for (uint32_t i = 0; i < TraceLogger_TreeItemEnd; i++) {
            TraceLoggerTextId id = TraceLoggerTextId(i);
            graph->addTextId(i, TLTextIdString(id));
        }
        graph->addTextId(TraceLogger_TreeItemEnd, "TraceLogger internal");
        for (uint32_t i = TraceLogger_TreeItemEnd + 1; i < TraceLogger_Last; i++) {
            TraceLoggerTextId id = TraceLoggerTextId(i);
            graph->addTextId(i, TLTextIdString(id));
        }
    }
}

void
TraceLoggerThreadState::disableAllTextIds() {
    for (uint32_t i = 1; i < TraceLogger_Last; i++) {
        enabledTextIds[i] = false;
    }
}

void
TraceLoggerThreadState::enableTextIdsForProfiler() {
    enableDefaultLogging();
}

void
TraceLoggerThreadState::disableTextIdsForProfiler() {
    disableAllTextIds();
    // We have to keep the Baseline and IonMonkey id's alive because they control whether
    // the jitted codegen has tracelogger start & stop events builtin.  Otherwise, we end up
    // in situations when some jitted code that was created before the profiler was even
    // started ends up not starting and stoping any events.  The TraceLogger_Engine stop events
    // can accidentally stop the wrong event in this case, and then it's no longer possible to
    // build a graph.
    enabledTextIds[TraceLogger_Engine] = true;
    enabledTextIds[TraceLogger_Interpreter] = true;
    enabledTextIds[TraceLogger_Baseline] = true;
    enabledTextIds[TraceLogger_IonMonkey] = true;
}

void
TraceLoggerThreadState::enableDefaultLogging()
{
    enabledTextIds[TraceLogger_AnnotateScripts] = true;
    enabledTextIds[TraceLogger_Bailout] = true;
    enabledTextIds[TraceLogger_Baseline] = true;
    enabledTextIds[TraceLogger_BaselineCompilation] = true;
    enabledTextIds[TraceLogger_GC] = true;
    enabledTextIds[TraceLogger_GCAllocation] = true;
    enabledTextIds[TraceLogger_GCSweeping] = true;
    enabledTextIds[TraceLogger_Interpreter] = true;
    enabledTextIds[TraceLogger_IonAnalysis] = true;
    enabledTextIds[TraceLogger_IonCompilation] = true;
    enabledTextIds[TraceLogger_IonLinking] = true;
    enabledTextIds[TraceLogger_IonMonkey] = true;
    enabledTextIds[TraceLogger_MinorGC] = true;
    enabledTextIds[TraceLogger_Frontend] = true;
    enabledTextIds[TraceLogger_ParsingFull] = true;
    enabledTextIds[TraceLogger_ParsingSyntax] = true;
    enabledTextIds[TraceLogger_BytecodeEmission] = true;
    enabledTextIds[TraceLogger_IrregexpCompile] = true;
    enabledTextIds[TraceLogger_IrregexpExecute] = true;
    enabledTextIds[TraceLogger_Scripts] = true;
    enabledTextIds[TraceLogger_Engine] = true;
    enabledTextIds[TraceLogger_WasmCompilation] = true;
    enabledTextIds[TraceLogger_Interpreter] = true;
    enabledTextIds[TraceLogger_Baseline] = true;
    enabledTextIds[TraceLogger_IonMonkey] = true;
}

void
TraceLoggerThreadState::enableIonLogging()
{
    enabledTextIds[TraceLogger_IonCompilation] = true;
    enabledTextIds[TraceLogger_IonLinking] = true;
    enabledTextIds[TraceLogger_PruneUnusedBranches] = true;
    enabledTextIds[TraceLogger_FoldTests] = true;
    enabledTextIds[TraceLogger_SplitCriticalEdges] = true;
    enabledTextIds[TraceLogger_RenumberBlocks] = true;
    enabledTextIds[TraceLogger_ScalarReplacement] = true;
    enabledTextIds[TraceLogger_DominatorTree] = true;
    enabledTextIds[TraceLogger_PhiAnalysis] = true;
    enabledTextIds[TraceLogger_MakeLoopsContiguous] = true;
    enabledTextIds[TraceLogger_ApplyTypes] = true;
    enabledTextIds[TraceLogger_EagerSimdUnbox] = true;
    enabledTextIds[TraceLogger_AliasAnalysis] = true;
    enabledTextIds[TraceLogger_GVN] = true;
    enabledTextIds[TraceLogger_LICM] = true;
    enabledTextIds[TraceLogger_Sincos] = true;
    enabledTextIds[TraceLogger_RangeAnalysis] = true;
    enabledTextIds[TraceLogger_LoopUnrolling] = true;
    enabledTextIds[TraceLogger_FoldLinearArithConstants] = true;
    enabledTextIds[TraceLogger_EffectiveAddressAnalysis] = true;
    enabledTextIds[TraceLogger_AlignmentMaskAnalysis] = true;
    enabledTextIds[TraceLogger_EliminateDeadCode] = true;
    enabledTextIds[TraceLogger_ReorderInstructions] = true;
    enabledTextIds[TraceLogger_EdgeCaseAnalysis] = true;
    enabledTextIds[TraceLogger_EliminateRedundantChecks] = true;
    enabledTextIds[TraceLogger_AddKeepAliveInstructions] = true;
    enabledTextIds[TraceLogger_GenerateLIR] = true;
    enabledTextIds[TraceLogger_RegisterAllocation] = true;
    enabledTextIds[TraceLogger_GenerateCode] = true;
    enabledTextIds[TraceLogger_Scripts] = true;
    enabledTextIds[TraceLogger_IonBuilderRestartLoop] = true;
}

void
TraceLoggerThreadState::enableFrontendLogging()
{
    enabledTextIds[TraceLogger_Frontend] = true;
    enabledTextIds[TraceLogger_ParsingFull] = true;
    enabledTextIds[TraceLogger_ParsingSyntax] = true;
    enabledTextIds[TraceLogger_BytecodeEmission] = true;
    enabledTextIds[TraceLogger_BytecodeFoldConstants] = true;
    enabledTextIds[TraceLogger_BytecodeNameFunctions] = true;
}

TraceLoggerThread::~TraceLoggerThread()
{
    if (graph.get()) {
        if (!failed) {
            graph->log(events, traceLoggerState->startTime);
        }
        graph = nullptr;
    }
}

bool
TraceLoggerThread::enable()
{
    if (enabled_ > 0) {
        enabled_++;
        return true;
    }

    if (failed) {
        return false;
    }

    enabled_ = 1;
    logTimestamp(TraceLogger_Enable);

    return true;
}

bool
TraceLoggerThread::fail(JSContext* cx, const char* error)
{
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_TRACELOGGER_ENABLE_FAIL, error);
    failed = true;
    enabled_ = 0;

    return false;
}

void
TraceLoggerThread::silentFail(const char* error)
{
    traceLoggerState->maybeSpewError(error);
    failed = true;
    enabled_ = 0;
}

size_t
TraceLoggerThread::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
    size_t size = 0;
#ifdef DEBUG
    size += graphStack.sizeOfExcludingThis(mallocSizeOf);
#endif
    size += events.sizeOfExcludingThis(mallocSizeOf);
    if (graph.get()) {
        size += graph->sizeOfIncludingThis(mallocSizeOf);
    }
    return size;
}

size_t
TraceLoggerThread::sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
    return mallocSizeOf(this) + sizeOfExcludingThis(mallocSizeOf);
}

bool
TraceLoggerThread::enable(JSContext* cx)
{
    using namespace js::jit;

    if (!enable()) {
        return fail(cx, "internal error");
    }

    if (enabled_ == 1) {
        // Get the top Activation to log the top script/pc (No inlined frames).
        ActivationIterator iter(cx);
        Activation* act = iter.activation();

        if (!act) {
            return fail(cx, "internal error");
        }

        JSScript* script = nullptr;
        int32_t engine = 0;

        if (act->isJit()) {
            JitFrameIter frame(iter->asJit());

            while (!frame.done()) {
                if (frame.isWasm()) {
                    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                              JSMSG_TRACELOGGER_ENABLE_FAIL,
                                              "not yet supported in wasm code");
                    return false;
                }
                if (frame.asJSJit().isScripted()) {
                    break;
                }
                ++frame;
            }

            MOZ_ASSERT(!frame.done());

            const JSJitFrameIter& jitFrame = frame.asJSJit();
            MOZ_ASSERT(jitFrame.isIonJS() || jitFrame.isBaselineJS());

            script = jitFrame.script();
            engine = jitFrame.isIonJS() ? TraceLogger_IonMonkey : TraceLogger_Baseline;
        } else {
            MOZ_ASSERT(act->isInterpreter());
            InterpreterFrame* fp = act->asInterpreter()->current();
            MOZ_ASSERT(!fp->runningInJit());

            script = fp->script();
            engine = TraceLogger_Interpreter;
        }
        if (script->compartment() != cx->compartment()) {
            return fail(cx, "compartment mismatch");
        }

        TraceLoggerEvent event(TraceLogger_Scripts, script);
        startEvent(event);
        startEvent(engine);
    }

    return true;
}

bool
TraceLoggerThread::disable(bool force, const char* error)
{
    if (failed) {
        MOZ_ASSERT(enabled_ == 0);
        return false;
    }

    if (enabled_ == 0) {
        return true;
    }

    if (enabled_ > 1 && !force) {
        enabled_--;
        return true;
    }

    if (force) {
        traceLoggerState->maybeSpewError(error);
    }

    logTimestamp(TraceLogger_Disable);
    enabled_ = 0;

    return true;
}

const char*
TraceLoggerThread::maybeEventText(uint32_t id)
{
    if (id < TraceLogger_Last) {
        return TLTextIdString(static_cast<TraceLoggerTextId>(id));
    }
    return traceLoggerState->maybeEventText(id);
}

TraceLoggerEventPayload*
TraceLoggerThreadState::getPayload(uint32_t id) {
    if (id < TraceLogger_Last) {
        return nullptr;
    }

    TextIdToPayloadMap::Ptr p = textIdPayloads.lookup(id);
    if (!p) {
        return nullptr;
    }

    p->value()->use();
    return p->value();
}

const char*
TraceLoggerThreadState::maybeEventText(uint32_t id)
{
    LockGuard<Mutex> guard(lock);

    TextIdToPayloadMap::Ptr p = textIdPayloads.lookup(id);
    if (!p) {
        return nullptr;
    }

    uint32_t dictId = p->value()->dictionaryId();
    MOZ_ASSERT(dictId < nextDictionaryId);
    return dictionaryData[dictId].get();
}

const char*
TraceLoggerThreadState::maybeEventText(TraceLoggerEventPayload *p)
{
    LockGuard<Mutex> guard(lock);
    if (!p) {
        return nullptr;
    }


    uint32_t dictId = p->dictionaryId();
    MOZ_ASSERT(dictId < nextDictionaryId);
    return dictionaryData[dictId].get();
}

size_t
TraceLoggerThreadState::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf)
{
    LockGuard<Mutex> guard(lock);

    // Do not count threadLoggers since they are counted by JSContext::traceLogger.

    size_t size = 0;
    size += dictionaryData.sizeOfExcludingThis(mallocSizeOf);
    size += payloadDictionary.shallowSizeOfExcludingThis(mallocSizeOf);
    size += textIdPayloads.shallowSizeOfExcludingThis(mallocSizeOf);
    for (TextIdToPayloadMap::Range r = textIdPayloads.all(); !r.empty(); r.popFront()) {
        r.front().value()->sizeOfIncludingThis(mallocSizeOf);
    }

    return size;
}

TraceLoggerEventPayload*
TraceLoggerThreadState::getOrCreateEventPayload(const char* text)
{
    LockGuard<Mutex> guard(lock);

    uint32_t dictId = nextDictionaryId;

    StringHashToDictionaryMap::AddPtr dictp = payloadDictionary.lookupForAdd(text);
    if (dictp) {
        dictId = dictp->value();
        MOZ_ASSERT(dictId < nextDictionaryId); // Sanity check.
    } else {
        UniqueChars str = DuplicateString(text);
        if (!str) {
            return nullptr;
        }
        if(!payloadDictionary.add(dictp, str.get(), nextDictionaryId)) {
            return nullptr;
        }
        if(!dictionaryData.append(std::move(str))) {
            return nullptr;
        }

        nextDictionaryId++;
    }

    uint32_t textId = nextTextId;

    auto* payload = js_new<TraceLoggerEventPayload>(textId, dictId);
    if (!payload) {
        return nullptr;
    }

    if (!textIdPayloads.putNew(textId, payload)) {
        js_delete(payload);
        return nullptr;
    }

    payload->use();

    nextTextId++;

    return payload;
}

TraceLoggerEventPayload*
TraceLoggerThreadState::getOrCreateEventPayload(const char* filename,
                                                uint32_t lineno, uint32_t colno)
{
    if (!filename) {
        filename = "<unknown>";
    }

    TraceLoggerEventPayload *payload = getOrCreateEventPayload(filename);
    if (!payload) {
        return nullptr;
    }

    payload->setLine(lineno);
    payload->setColumn(colno);

    return payload;
}

TraceLoggerEventPayload*
TraceLoggerThreadState::getOrCreateEventPayload(JSScript* script)
{
    return getOrCreateEventPayload(script->filename(), script->lineno(), script->column());
}

void
TraceLoggerThreadState::purgeUnusedPayloads()
{
    // Care needs to be taken to maintain a coherent state in this function,
    // as payloads can have their use count change at any time from non-zero to
    // zero (but not the other way around; see TraceLoggerEventPayload::use()).
    LockGuard<Mutex> guard(lock);

    // Free all other payloads that have no uses anymore.
    for (TextIdToPayloadMap::Enum e(textIdPayloads); !e.empty(); e.popFront()) {
        if (e.front().value()->uses() == 0) {
            uint32_t dictId = e.front().value()->dictionaryId();
            dictionaryData.erase(dictionaryData.begin() + dictId);
            js_delete(e.front().value());
            e.removeFront();
        }
    }
}

void
TraceLoggerThread::startEvent(TraceLoggerTextId id) {
    startEvent(uint32_t(id));
}

void
TraceLoggerThread::startEvent(const TraceLoggerEvent& event) {
    if (!event.hasTextId()) {
        if (!enabled()) {
            return;
        }
        startEvent(TraceLogger_Error);
        disable(/* force = */ true, "TraceLogger encountered an empty event. "
                                    "Potentially due to OOM during creation of "
                                    "this event. Disabling TraceLogger.");
        return;
    }
    startEvent(event.textId());
}

void
TraceLoggerThread::startEvent(uint32_t id)
{
    if (!jit::JitOptions.enableTraceLogger) {
        return;
    }

    MOZ_ASSERT(TLTextIdIsTreeEvent(id) || id == TraceLogger_Error);
    MOZ_ASSERT(traceLoggerState);
    if (!traceLoggerState->isTextIdEnabled(id)) {
       return;
    }

#ifdef DEBUG
    if (enabled_ > 0) {
        AutoEnterOOMUnsafeRegion oomUnsafe;
        if (!graphStack.append(id)) {
            oomUnsafe.crash("Could not add item to debug stack.");
        }
    }
#endif

    if (graph.get() && traceLoggerState->isGraphFileEnabled()) {
        // Flush each textId to disk.  textId values up to TraceLogger_Last are statically defined
        // and each one has an associated constant event string defined by TLTextIdString().  For
        // any events with textId >= TraceLogger_Last the payload associated with that textId must
        // first be found and then maybeEventText() will find the event string form the dictionary.
        for (uint32_t otherId = graph->nextTextId(); otherId <= id; otherId++) {
            if (id < TraceLogger_Last) {
                const char *text = TLTextIdString(static_cast<TraceLoggerTextId>(id));
                graph->addTextId(otherId, text);
            } else {
                TraceLoggerEventPayload *p = traceLoggerState->getPayload(id);
                if (p) {
                    const char *filename = traceLoggerState->maybeEventText(p);
                    mozilla::Maybe<uint32_t> line   = p->line();
                    mozilla::Maybe<uint32_t> column = p->column();
                    graph->addTextId(otherId, filename, line, column);
                    p->release();
                }
            }
        }
    }

    log(id);
}

void
TraceLoggerThread::stopEvent(TraceLoggerTextId id) {
    stopEvent(uint32_t(id));
}

void
TraceLoggerThread::stopEvent(const TraceLoggerEvent& event) {
    if (!event.hasTextId()) {
        stopEvent(TraceLogger_Error);
        return;
    }
    stopEvent(event.textId());
}

void
TraceLoggerThread::stopEvent(uint32_t id)
{
    if (!jit::JitOptions.enableTraceLogger) {
        return;
    }

    MOZ_ASSERT(TLTextIdIsTreeEvent(id) || id == TraceLogger_Error);
    MOZ_ASSERT(traceLoggerState);
    if (!traceLoggerState->isTextIdEnabled(id)) {
        return;
    }

#ifdef DEBUG
    if (!graphStack.empty()) {
        uint32_t prev = graphStack.popCopy();
        if (id == TraceLogger_Error || prev == TraceLogger_Error) {
            // When encountering an Error id the stack will most likely not be correct anymore.
            // Ignore this.
        } else if (id == TraceLogger_Engine) {
            MOZ_ASSERT(prev == TraceLogger_IonMonkey || prev == TraceLogger_Baseline ||
                       prev == TraceLogger_Interpreter);
        } else if (id == TraceLogger_Scripts) {
            MOZ_ASSERT(prev >= TraceLogger_Last);
        } else if (id >= TraceLogger_Last) {
            MOZ_ASSERT(prev >= TraceLogger_Last);
            if (prev != id) {
                // Ignore if the text has been flushed already.
                MOZ_ASSERT_IF(maybeEventText(prev), strcmp(maybeEventText(id), maybeEventText(prev)) == 0);
            }
        } else {
            MOZ_ASSERT(id == prev);
        }
    }
#endif

    log(TraceLogger_Stop);
}

void
TraceLoggerThread::logTimestamp(TraceLoggerTextId id)
{
    logTimestamp(uint32_t(id));
}

void
TraceLoggerThread::logTimestamp(uint32_t id)
{
    MOZ_ASSERT(id > TraceLogger_TreeItemEnd && id < TraceLogger_Last);
    log(id);
}

void
TraceLoggerThread::log(uint32_t id)
{
    if (enabled_ == 0) {
        return;
    }

#ifdef DEBUG
    if (id == TraceLogger_Disable) {
        graphStack.clear();
    }
#endif

    MOZ_ASSERT(traceLoggerState);

    // We request for 3 items to add, since if we don't have enough room
    // we record the time it took to make more space. To log this information
    // we need 2 extra free entries.
    if (!events.hasSpaceForAdd(3)) {
        mozilla::TimeStamp start = mozilla::TimeStamp::Now();

        if (!events.ensureSpaceBeforeAdd(3)) {
            if (graph.get()) {
                graph->log(events, traceLoggerState->startTime);
            }

            // The data structures are full, and the graph file is not enabled
            // so we cannot flush to disk.  Trace logging should stop here.
            if (!traceLoggerState->isGraphFileEnabled()) {
                enabled_ = 0;
                return;
            }

            iteration_++;
            events.clear();

            // Periodically remove unused payloads from the global logger state.
            traceLoggerState->purgeUnusedPayloads();
        }

        // Log the time it took to flush the events as being from the
        // Tracelogger.
        if (graph.get()) {
            MOZ_ASSERT(events.hasSpaceForAdd(2));
            EventEntry& entryStart = events.pushUninitialized();
            entryStart.time = start;
            entryStart.textId = TraceLogger_Internal;

            EventEntry& entryStop = events.pushUninitialized();
            entryStop.time = mozilla::TimeStamp::Now();
            entryStop.textId = TraceLogger_Stop;
        }

    }

    mozilla::TimeStamp time = mozilla::TimeStamp::Now();

    EventEntry& entry = events.pushUninitialized();
    entry.time = time;
    entry.textId = id;
}

void TraceLoggerThreadState::clear()
{
    LockGuard<Mutex> guard(lock);
    for (TraceLoggerThread* logger : threadLoggers) {
        logger->clear();
    }

    // Clear all payloads that are not currently used.  There may be some events that
    // still hold a pointer to a payload.  Restarting the profiler may add this event
    // to the new events array and so we need to maintain it's existence.
    for (TextIdToPayloadMap::Enum e(textIdPayloads); !e.empty(); e.popFront()) {
        if (e.front().value()->uses() == 0) {
            js_delete(e.front().value());
            e.removeFront();
        }
    }

    // Clear and free any data used for the string dictionary.
    for (auto range = dictionaryData.all(); !range.empty(); range.popFront()) {
        range.front().reset();
    }

    dictionaryData.clearAndFree();
    payloadDictionary.clearAndCompact();

    nextTextId = TraceLogger_Last;
    nextDictionaryId = 0;
}

void TraceLoggerThread::clear()
{
    if (graph.get()) {
        graph.reset();
    }

    graph = nullptr;

#ifdef DEBUG
    graphStack.clear();
#endif

    if (!events.reset()) {
        silentFail("Cannot reset event buffer.");
    }

}

TraceLoggerThreadState::~TraceLoggerThreadState()
{
    while (TraceLoggerThread* logger = threadLoggers.popFirst()) {
        js_delete(logger);
    }

    threadLoggers.clear();

    for (TextIdToPayloadMap::Range r = textIdPayloads.all(); !r.empty(); r.popFront()) {
        js_delete(r.front().value());
    }

#ifdef DEBUG
    initialized = false;
#endif
}

static bool
ContainsFlag(const char* str, const char* flag)
{
    size_t flaglen = strlen(flag);
    const char* index = strstr(str, flag);
    while (index) {
        if ((index == str || index[-1] == ',') && (index[flaglen] == 0 || index[flaglen] == ',')) {
            return true;
        }
        index = strstr(index + flaglen, flag);
    }
    return false;
}

bool
TraceLoggerThreadState::init()
{
    const char* env = getenv("TLLOG");
    if (env) {
        if (strstr(env, "help")) {
            fflush(nullptr);
            printf(
                "\n"
                "usage: TLLOG=option,option,option,... where options can be:\n"
                "\n"
                "Collections:\n"
                "  Default        Output all default. It includes:\n"
                "                 AnnotateScripts, Bailout, Baseline, BaselineCompilation, GC,\n"
                "                 GCAllocation, GCSweeping, Interpreter, IonAnalysis, IonCompilation,\n"
                "                 IonLinking, IonMonkey, MinorGC, Frontend, ParsingFull,\n"
                "                 ParsingSyntax, BytecodeEmission, IrregexpCompile, IrregexpExecute,\n"
                "                 Scripts, Engine, WasmCompilation\n"
                "\n"
                "  IonCompiler    Output all information about compilation. It includes:\n"
                "                 IonCompilation, IonLinking, PruneUnusedBranches, FoldTests,\n"
                "                 SplitCriticalEdges, RenumberBlocks, ScalarReplacement,\n"
                "                 DominatorTree, PhiAnalysis, MakeLoopsContiguous, ApplyTypes,\n"
                "                 EagerSimdUnbox, AliasAnalysis, GVN, LICM, Sincos, RangeAnalysis,\n"
                "                 LoopUnrolling, FoldLinearArithConstants, EffectiveAddressAnalysis,\n"
                "                 AlignmentMaskAnalysis, EliminateDeadCode, ReorderInstructions,\n"
                "                 EdgeCaseAnalysis, EliminateRedundantChecks,\n"
                "                 AddKeepAliveInstructions, GenerateLIR, RegisterAllocation,\n"
                "                 GenerateCode, Scripts, IonBuilderRestartLoop\n"
                "\n"
                "  VMSpecific     Output the specific name of the VM call\n"
                "\n"
                "  Frontend       Output all information about frontend compilation. It includes:\n"
                "                 Frontend, ParsingFull, ParsingSyntax, Tokenizing,\n"
                "                 BytecodeEmission, BytecodeFoldConstants, BytecodeNameFunctions\n"
                "Specific log items:\n"
            );
            for (uint32_t i = 1; i < TraceLogger_Last; i++) {
                TraceLoggerTextId id = TraceLoggerTextId(i);
                if (!TLTextIdIsTogglable(id)) {
                    continue;
                }
                printf("  %s\n", TLTextIdString(id));
            }
            printf("\n");
            exit(0);
            /*NOTREACHED*/
        }

        for (uint32_t i = 1; i < TraceLogger_Last; i++) {
            TraceLoggerTextId id = TraceLoggerTextId(i);
            if (TLTextIdIsTogglable(id)) {
                enabledTextIds[i] = ContainsFlag(env, TLTextIdString(id));
            } else {
                enabledTextIds[i] = true;
            }
        }

        if (ContainsFlag(env, "Default")) {
            enableDefaultLogging();
        }

        if (ContainsFlag(env, "IonCompiler")) {
            enableIonLogging();
        }

        if (ContainsFlag(env, "Frontend")) {
            enableFrontendLogging();
        }

#ifdef DEBUG
        enabledTextIds[TraceLogger_Error] = true;
#endif

    } else {
        // Most of the textId's will be enabled through JS::StartTraceLogger when
        // the gecko profiler is started.
        disableTextIdsForProfiler();
    }

    enabledTextIds[TraceLogger_Interpreter] = enabledTextIds[TraceLogger_Engine];
    enabledTextIds[TraceLogger_Baseline] = enabledTextIds[TraceLogger_Engine];
    enabledTextIds[TraceLogger_IonMonkey] = enabledTextIds[TraceLogger_Engine];

    enabledTextIds[TraceLogger_Error] = true;

    const char* options = getenv("TLOPTIONS");
    if (options) {
        if (strstr(options, "help")) {
            fflush(nullptr);
            printf(
                "\n"
                "usage: TLOPTIONS=option,option,option,... where options can be:\n"
                "\n"
                "  EnableMainThread        Start logging main threads immediately.\n"
                "  EnableOffThread         Start logging helper threads immediately.\n"
                "  EnableGraph             Enable the tracelogging graph.\n"
                "  EnableGraphFile         Enable flushing tracelogger data to a file.\n"
                "  Errors                  Report errors during tracing to stderr.\n"
            );
            printf("\n");
            exit(0);
            /*NOTREACHED*/
        }

        if (strstr(options, "EnableMainThread")) {
            mainThreadEnabled = true;
        }
        if (strstr(options, "EnableOffThread")) {
            helperThreadEnabled = true;
        }
        if (strstr(options, "EnableGraph")) {
            graphEnabled = true;
        }
        if (strstr(options, "EnableGraphFile")) {
            graphFileEnabled = true;
            jit::JitOptions.enableTraceLogger = true;
        }
        if (strstr(options, "Errors")) {
            spewErrors = true;
        }
    } else {
            mainThreadEnabled = true;
            helperThreadEnabled = true;
            graphEnabled = false;
            graphFileEnabled = false;
            spewErrors = false;
    }

    startTime = mozilla::TimeStamp::Now();

#ifdef DEBUG
    initialized = true;
#endif

    return true;
}

void
TraceLoggerThreadState::enableTextId(JSContext* cx, uint32_t textId)
{
    MOZ_ASSERT(TLTextIdIsTogglable(textId));

    if (enabledTextIds[textId]) {
        return;
    }

    ReleaseAllJITCode(cx->runtime()->defaultFreeOp());

    enabledTextIds[textId] = true;
    if (textId == TraceLogger_Engine) {
        enabledTextIds[TraceLogger_IonMonkey] = true;
        enabledTextIds[TraceLogger_Baseline] = true;
        enabledTextIds[TraceLogger_Interpreter] = true;
    }

    if (textId == TraceLogger_Scripts) {
        jit::ToggleBaselineTraceLoggerScripts(cx->runtime(), true);
    }
    if (textId == TraceLogger_Engine) {
        jit::ToggleBaselineTraceLoggerEngine(cx->runtime(), true);
    }

}
void
TraceLoggerThreadState::disableTextId(JSContext* cx, uint32_t textId)
{
    MOZ_ASSERT(TLTextIdIsTogglable(textId));

    if (!enabledTextIds[textId]) {
        return;
    }

    ReleaseAllJITCode(cx->runtime()->defaultFreeOp());

    enabledTextIds[textId] = false;
    if (textId == TraceLogger_Engine) {
        enabledTextIds[TraceLogger_IonMonkey] = false;
        enabledTextIds[TraceLogger_Baseline] = false;
        enabledTextIds[TraceLogger_Interpreter] = false;
    }

    if (textId == TraceLogger_Scripts) {
        jit::ToggleBaselineTraceLoggerScripts(cx->runtime(), false);
    }
    if (textId == TraceLogger_Engine) {
        jit::ToggleBaselineTraceLoggerEngine(cx->runtime(), false);
    }
}

TraceLoggerThread*
js::TraceLoggerForCurrentThread(JSContext* maybecx)
{
    if (!EnsureTraceLoggerState()) {
        return nullptr;
    }
    return traceLoggerState->forCurrentThread(maybecx);
}

TraceLoggerThread*
TraceLoggerThreadState::forCurrentThread(JSContext* maybecx)
{
    if (!jit::JitOptions.enableTraceLogger) {
        return nullptr;
    }

    MOZ_ASSERT(initialized);
    MOZ_ASSERT_IF(maybecx, maybecx == TlsContext.get());

    JSContext* cx = maybecx ? maybecx : TlsContext.get();
    if (!cx) {
        return nullptr;
    }

    if (!cx->traceLogger) {
        LockGuard<Mutex> guard(lock);

        TraceLoggerThread* logger = js_new<TraceLoggerThread>();
        if (!logger) {
            return nullptr;
        }

        if (!logger->init()) {
            return nullptr;
        }

        threadLoggers.insertFront(logger);
        cx->traceLogger = logger;

        if (graphEnabled) {
            logger->initGraph();
        }

        if (CurrentHelperThread() ? helperThreadEnabled : mainThreadEnabled) {
            logger->enable();
        }
    }

    return cx->traceLogger;
}

void
TraceLoggerThreadState::destroyLogger(TraceLoggerThread* logger)
{
    MOZ_ASSERT(initialized);
    MOZ_ASSERT(logger);
    LockGuard<Mutex> guard(lock);

    logger->remove();
    js_delete(logger);
}

bool
js::TraceLogTextIdEnabled(uint32_t textId)
{
    if (!EnsureTraceLoggerState()) {
        return false;
    }
    return traceLoggerState->isTextIdEnabled(textId);
}

void
js::TraceLogEnableTextId(JSContext* cx, uint32_t textId)
{
    if (!EnsureTraceLoggerState()) {
        return;
    }
    traceLoggerState->enableTextId(cx, textId);
}
void
js::TraceLogDisableTextId(JSContext* cx, uint32_t textId)
{
    if (!EnsureTraceLoggerState()) {
        return;
    }
    traceLoggerState->disableTextId(cx, textId);
}

TraceLoggerEvent::TraceLoggerEvent(TraceLoggerTextId type, JSScript* script)
  : TraceLoggerEvent(type, script->filename(), script->lineno(), script->column())
{ }

TraceLoggerEvent::TraceLoggerEvent(TraceLoggerTextId type, const char* filename, uint32_t line,
                                   uint32_t column)
  : payload_()
{
    MOZ_ASSERT(type == TraceLogger_Scripts || type == TraceLogger_AnnotateScripts ||
               type == TraceLogger_InlinedScripts || type == TraceLogger_Frontend);

    if (!traceLoggerState || !jit::JitOptions.enableTraceLogger) {
        return;
    }

    // Only log scripts when enabled, otherwise use the more generic type
    // (which will get filtered out).
    if (!traceLoggerState->isTextIdEnabled(type)) {
        payload_.setTextId(type);
        return;
    }

    payload_.setEventPayload(
        traceLoggerState->getOrCreateEventPayload(filename, line, column));
}

TraceLoggerEvent::TraceLoggerEvent(const char* text)
  : payload_()
{
    if (jit::JitOptions.enableTraceLogger && traceLoggerState) {
        payload_.setEventPayload(traceLoggerState->getOrCreateEventPayload(text));
    }
}

TraceLoggerEvent::~TraceLoggerEvent()
{
    if (hasExtPayload()) {
        extPayload()->release();
    }
}

uint32_t
TraceLoggerEvent::textId() const
{
    MOZ_ASSERT(hasTextId());
    if (hasExtPayload()) {
        return extPayload()->textId();
    }
    return payload_.textId();
}

TraceLoggerEvent&
TraceLoggerEvent::operator=(const TraceLoggerEvent& other)
{
    if (other.hasExtPayload()) {
        other.extPayload()->use();
    }
    if (hasExtPayload()) {
        extPayload()->release();
    }

    payload_ = other.payload_;

    return *this;
}

TraceLoggerEvent::TraceLoggerEvent(const TraceLoggerEvent& other)
  : payload_(other.payload_)
{
    if (hasExtPayload()) {
        extPayload()->use();
    }
}

JS_PUBLIC_API void
JS::ResetTraceLogger(void)
{
    js::ResetTraceLogger();
}

JS_PUBLIC_API void
JS::StartTraceLogger(JSContext *cx, mozilla::TimeStamp profilerStart)
{
    if (jit::JitOptions.enableTraceLogger || !traceLoggerState)  {
        return;
    }

    LockGuard<Mutex> guard(traceLoggerState->lock);
    traceLoggerState->enableTextIdsForProfiler();
    jit::JitOptions.enableTraceLogger = true;

    // Reset the start time to profile start so it aligns with sampling.
    traceLoggerState->startTime = profilerStart;

    if (cx->traceLogger) {
        cx->traceLogger->enable();
    }
}

JS_PUBLIC_API void
JS::StopTraceLogger(JSContext *cx)
{
    if (!jit::JitOptions.enableTraceLogger || !traceLoggerState) {
        return;
    }

    LockGuard<Mutex> guard(traceLoggerState->lock);
    traceLoggerState->disableTextIdsForProfiler();
    jit::JitOptions.enableTraceLogger = false;
    if (cx->traceLogger) {
        cx->traceLogger->disable();
    }
}
